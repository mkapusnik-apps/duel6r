#include "DarwinProcess.h"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <libproc.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace Duel6::Platform::Darwin {
    namespace {
        std::atomic<bool> guardedWorker{false};
        class Descriptor {
        public:
            explicit Descriptor(int value = -1) : value(value) {}
            ~Descriptor() { if (value >= 0) close(value); }
            Descriptor(const Descriptor &) = delete;
            Descriptor &operator=(const Descriptor &) = delete;
            int get() const { return value; }
            int value;
        };

        bool nonblocking(int descriptor) {
            const int flags = fcntl(descriptor, F_GETFL);
            return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
        }

        int watchParent(pid_t parent, int life) {
            if (parent <= 1 || getppid() != parent || fcntl(life, F_GETFD) < 0) return -1;
            Descriptor queue(kqueue());
            if (queue.get() < 0 || fcntl(queue.get(), F_SETFD, FD_CLOEXEC) < 0) return -1;
            struct kevent changes[2];
            EV_SET(&changes[0], parent, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT, 0, nullptr);
            EV_SET(&changes[1], life, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
            if (kevent(queue.get(), changes, 2, nullptr, 0, nullptr) != 0 || getppid() != parent) return -1;
            const int result = queue.value;
            queue.value = -1;
            return result;
        }

        bool parentGone(int queue, int life, int milliseconds) {
            struct kevent events[2];
            timespec timeout{milliseconds / 1000, (milliseconds % 1000) * 1000000L};
            const int count = kevent(queue, nullptr, 0, events, 2, &timeout);
            if (count < 0) return errno != EINTR; // Broken observation fails closed.
            for (int index = 0; index < count; ++index) {
                const auto &event = events[index];
                if (event.flags & EV_ERROR) return true;
                if (event.filter == EVFILT_PROC && (event.fflags & NOTE_EXIT)) return true;
                // EOF or any explicit cancellation byte is terminal. Never
                // consume and then treat an old attempt as live again.
                if (event.filter == EVFILT_READ && event.ident == static_cast<uintptr_t>(life)) return true;
            }
            return false;
        }

        bool report(int socket, GuardianEvent event, pid_t worker, std::uint64_t origin) {
            const auto bytes = guardianFrame(event, static_cast<std::uint32_t>(worker), origin);
            // Bounded nonblocking status. A lost/partial status fails the
            // reporting channel, not termination. No diagnostics on stdout.
            ssize_t sent;
            do { sent = send(socket, bytes.data(), bytes.size(), 0); } while (sent < 0 && errno == EINTR);
            return sent == static_cast<ssize_t>(bytes.size());
        }

        int moveAboveReserved(int descriptor) {
            const int moved = fcntl(descriptor, F_DUPFD_CLOEXEC, 7);
            close(descriptor);
            return moved;
        }

        pid_t spawnWorker(const std::vector<std::string> &arguments, int life, WorkerChannels channels) {
            if (arguments.empty() || arguments.front().empty() || arguments.front().front() != '/') return -1;
            std::vector<std::string> owned = arguments;
            owned.push_back("--guardian-parent=" + std::to_string(getpid()));
            std::vector<char *> argv;
            for (auto &argument : owned) argv.push_back(argument.data());
            argv.push_back(nullptr);
            Descriptor output(channels.output < 0 ? -1 : fcntl(channels.output, F_DUPFD_CLOEXEC, 7));
            Descriptor input(channels.input < 0 ? -1 : fcntl(channels.input, F_DUPFD_CLOEXEC, 7));
            if ((channels.output >= 0 && output.get() < 0) || (channels.input >= 0 && input.get() < 0)) return -1;
            posix_spawn_file_actions_t actions;
            posix_spawnattr_t attributes;
            if (posix_spawn_file_actions_init(&actions) != 0) return -1;
            if (posix_spawnattr_init(&attributes) != 0) {
                posix_spawn_file_actions_destroy(&actions); return -1;
            }
            int error = 0;
            for (int fd = 0; fd < 3 && !error; ++fd)
                error = posix_spawn_file_actions_addopen(&actions, fd, "/dev/null", fd ? O_WRONLY : O_RDONLY, 0);
            if (!error) error = posix_spawn_file_actions_adddup2(&actions, life, 3);
            if (!error) error = posix_spawn_file_actions_addclose(&actions, life);
            if (!error && output.get() >= 0) error = posix_spawn_file_actions_adddup2(&actions, output.get(), 4);
            if (!error && input.get() >= 0) error = posix_spawn_file_actions_adddup2(&actions, input.get(), 5);
            if (!error && output.get() >= 0) error = posix_spawn_file_actions_addclose(&actions, output.get());
            if (!error && input.get() >= 0) error = posix_spawn_file_actions_addclose(&actions, input.get());
            if (!error) error = posix_spawnattr_setflags(&attributes,
                    POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETPGROUP);
            if (!error) error = posix_spawnattr_setpgroup(&attributes, 0);
            pid_t child = -1;
            char *environment[] = {nullptr};
            if (!error) error = posix_spawn(&child, argv.front(), &actions, &attributes, argv.data(), environment);
            posix_spawnattr_destroy(&attributes);
            posix_spawn_file_actions_destroy(&actions);
            return error ? -1 : child;
        }
    }

    GroupInspection inspectGroup(pid_t leader) {
        // The pinned leader must be present even when it is a zombie. A full
        // buffer, inaccessible entry, unexpected group or missing anchor is
        // Unknown, never an empty tree. No adoption/grandchild wait is used.
        std::array<pid_t, 4097> members{};
        const int bytes = proc_listpids(PROC_PGRP_ONLY, static_cast<uint32_t>(leader), members.data(), sizeof(members));
        if (bytes <= 0 || bytes % sizeof(pid_t) || bytes >= static_cast<int>(sizeof(members)))
            return GroupInspection::Unknown;
        bool anchor = false, descendants = false;
        for (int index = 0; index < bytes / static_cast<int>(sizeof(pid_t)); ++index) {
            const pid_t pid = members[index];
            if (pid <= 0) return GroupInspection::Unknown;
            if (pid == leader) { anchor = true; continue; }
            proc_bsdinfo information{};
            if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &information, sizeof(information)) != sizeof(information)
                || information.pbi_pid != static_cast<uint32_t>(pid)
                || information.pbi_pgid != static_cast<uint32_t>(leader)) return GroupInspection::Unknown;
            descendants = true; // Zombies count until the actual parent/system reaps them.
        }
        return !anchor ? GroupInspection::Unknown : descendants ? GroupInspection::Descendants : GroupInspection::LeaderOnly;
    }

    int runGuardian(pid_t parent, int parentLife, int status,
                    const std::vector<std::string> &arguments,
                    const std::function<GroupInspection(pid_t)> &inspect, WorkerChannels channels) {
        Descriptor queue(watchParent(parent, parentLife));
        const int noSignal = 1;
        if (queue.get() < 0 || !nonblocking(status)
            || setsockopt(status, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal)) != 0) return 2;
        bool statusOpen = true;
        const auto publish = [&](GuardianEvent event, pid_t worker, std::uint64_t origin = monotonicNanoseconds()) {
            if (!statusOpen) return false;
            if (report(status, event, worker, origin)) return true;
            // A partial fixed frame cannot be followed by another frame: that
            // would allow a reader to accidentally resynchronize on mixed bytes.
            statusOpen = false;
            shutdown(status, SHUT_WR);
            return false;
        };
        if (parentGone(queue.get(), parentLife, 0)) return publish(GuardianEvent::Cleaned, 0) ? 0 : 2;
        int pipeEnds[2];
        if (pipe(pipeEnds) != 0) return 2;
        Descriptor reader(moveAboveReserved(pipeEnds[0]));
        Descriptor writer(moveAboveReserved(pipeEnds[1]));
        if (reader.get() < 0 || writer.get() < 0) return 2;
        // Registration and the pre-spawn cancel check precede all worker code.
        if (parentGone(queue.get(), parentLife, 0)) return publish(GuardianEvent::Cleaned, 0) ? 0 : 2;
        const pid_t worker = spawnWorker(arguments, reader.get(), channels);
        if (channels.output >= 0) close(channels.output);
        if (channels.input >= 0) close(channels.input);
        if (worker < 0) {
            const bool failure = publish(GuardianEvent::Failed, 0);
            const bool noService = publish(GuardianEvent::Cleaned, 0);
            return failure && noService ? 0 : 2;
        }
        CleanupState cleanup;
        bool reporting = publish(GuardianEvent::Started, worker);
        bool stopping = !reporting, observedExit = false;
        for (;;) {
            if (parentGone(queue.get(), parentLife, 10)) stopping = true;
            siginfo_t information{};
            int result;
            do { result = waitid(P_PID, static_cast<id_t>(worker), &information, WEXITED | WNOHANG | WNOWAIT); }
            while (result < 0 && errno == EINTR);
            if (result < 0) {
                cleanup.loseAnchor(); // Do NOT signal a numeric group on ECHILD/unknown ownership.
                publish(GuardianEvent::Failed, worker);
                return 2; // Writer closes; runnable worker independently handles supervision loss.
            }
            if (information.si_pid == worker && (information.si_code == CLD_EXITED
                || information.si_code == CLD_KILLED || information.si_code == CLD_DUMPED)) {
                const auto origin = monotonicNanoseconds(); // First confirmed waitid observation, not GUI polling.
                cleanup.observeExit(); stopping = true;
                if (!observedExit) {
                    observedExit = true;
                    reporting = publish(GuardianEvent::LeaderExited, worker, origin) && reporting;
                }
            }
            if (stopping && cleanup.maySignal()) kill(-worker, SIGKILL);
            GroupInspection group = GroupInspection::Unknown;
            try { group = inspect(worker); } catch (...) { /* retain ownership */ }
            if (!cleanup.observeGroup(group)) {
                if (stopping) std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            pid_t reaped;
            do { reaped = waitpid(worker, nullptr, 0); } while (reaped < 0 && errno == EINTR);
            cleanup.loseAnchor();
            if (reaped != worker) { publish(GuardianEvent::Failed, worker); return 2; }
            const bool delivered = publish(GuardianEvent::Cleaned, worker);
            // No PGID use after the exact leader reap, even if reporting fails.
            return reporting && delivered ? 0 : 2;
        }
    }

    struct ParentMonitor::Impl {
        Descriptor queue;
        int life;
        std::atomic<bool> finished{false};
        std::thread watcher;
        bool active = false;
        Impl(int queue, int life) : queue(queue), life(life) {}
        ~Impl() {
            finished = true;
            if (watcher.joinable()) watcher.join();
            if (active) guardedWorker = false;
        }
    };
    ParentMonitor::ParentMonitor() = default;
    ParentMonitor::~ParentMonitor() = default;
    bool inGuardedWorker() { return guardedWorker.load(); }
    std::unique_ptr<ParentMonitor> ParentMonitor::start(pid_t guardian, int life, bool ownsGroup) {
        if (inGuardedWorker() || (ownsGroup ? getpgrp() != getpid() : getpgrp() != getpgid(guardian))) return nullptr;
        const int queue = watchParent(guardian, life);
        if (queue < 0) return nullptr;
        auto monitor = std::unique_ptr<ParentMonitor>(new ParentMonitor());
        monitor->impl = std::make_unique<Impl>(queue, life);
        if (parentGone(queue, life, 0)) return nullptr;
        auto *state = monitor->impl.get();
        try {
            state->watcher = std::thread([state, ownsGroup] {
                while (!state->finished) {
                    if (parentGone(state->queue.get(), state->life, 10)) {
                        // A live group owner pins its own PGID. This terminal
                        // supervision loss is never intentional End.
                        // A same-group resolver is a leaf, not the group owner.
                        // Killing the group here would also kill its service.
                        kill(ownsGroup ? 0 : getpid(), SIGKILL);
                        _exit(2);
                    }
                }
            });
        } catch (...) { return nullptr; }
        state->active = true;
        guardedWorker = true;
        return monitor;
    }
}
