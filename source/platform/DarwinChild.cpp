#include "DarwinChild.h"
#include "DarwinProcess.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <mach-o/dyld.h>
#include <mutex>
#include <spawn.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <utility>
#include <unistd.h>

namespace Duel6::Platform::Darwin {
    std::string executablePath() {
        std::uint32_t length = 0;
        _NSGetExecutablePath(nullptr, &length);
        if (!length || length > 1024 * 1024) return {};
        std::vector<char> path(length);
        if (_NSGetExecutablePath(path.data(), &length) != 0) return {};
        std::error_code error;
        const auto resolved = std::filesystem::canonical(path.data(), error);
        return error ? std::string{} : resolved.string();
    }
    std::string siblingExecutable(const char *name) {
        const auto self = executablePath();
        return self.empty() ? std::string{} : (std::filesystem::path(self).parent_path() / name).string();
    }
    bool socketPair(int (&descriptors)[2]) {
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0) return false;
        for (int &descriptor : descriptors) {
            const int moved = fcntl(descriptor, F_DUPFD_CLOEXEC, 7);
            close(descriptor); descriptor = moved;
        }
        const int one = 1;
        if (descriptors[0] >= 0 && descriptors[1] >= 0
            && setsockopt(descriptors[0], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) == 0
            && setsockopt(descriptors[1], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) == 0) return true;
        for (int &descriptor : descriptors) { if (descriptor >= 0) close(descriptor); descriptor = -1; }
        return false;
    }

    struct GuardedChild::State {
        std::mutex mutex;
        pid_t guardian = -1;
        std::uint32_t worker = 0;
        int life = -1, events = -1, output = -1, input = -1;
        bool workerExited = false, guardianExited = false, broken = false, cleaned = false, reaped = false;
        bool cleanedEvent = false, eof = false;
        bool spawnFailed = false;
        std::chrono::steady_clock::time_point exitObservation{};
        std::array<unsigned char, 8> frame{};
        std::size_t used = 0, total = 0;
        ~State() { for (int fd : {life, events, output, input}) if (fd >= 0) close(fd); }
        void terminateLocked() { if (life >= 0) { close(life); life = -1; } }
        void recordExit() {
            if (exitObservation == std::chrono::steady_clock::time_point{}) exitObservation = std::chrono::steady_clock::now();
        }
        void refreshLocked() {
            if (cleaned || guardian < 0) return;
            while (!eof && !broken) {
                const auto count = recv(events, frame.data() + used, frame.size() - used, 0);
                if (count < 0 && errno == EINTR) continue;
                if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (count <= 0) { eof = true; if (count < 0 || used || !cleanedEvent) broken = true; break; }
                used += static_cast<std::size_t>(count); total += static_cast<std::size_t>(count);
                if (total > 32) { broken = true; break; }
                if (used != frame.size()) continue;
                const auto word = [&](unsigned start) {
                    std::uint32_t value = 0;
                    for (unsigned i = start; i < start + 4; ++i) value = (value << 8) | frame[i];
                    return value;
                };
                const auto event = static_cast<GuardianEvent>(word(0));
                const auto pid = word(4);
                used = 0;
                if (cleanedEvent) broken = true;
                else if (event == GuardianEvent::Started && !worker && !spawnFailed && pid > 1) worker = pid;
                else if (event == GuardianEvent::LeaderExited && worker && pid == worker && !workerExited) { workerExited = true; recordExit(); }
                else if (event == GuardianEvent::Cleaned && pid == worker && (!worker || workerExited)) cleanedEvent = true;
                else if (event == GuardianEvent::Failed && !worker && !pid && !spawnFailed) { spawnFailed = true; recordExit(); }
                else broken = true; // Includes Failed; never interpret a failed stream as tree-zero.
            }
            if (broken) { recordExit(); terminateLocked(); }
            siginfo_t info{};
            int result;
            do { result = waitid(P_PID, static_cast<id_t>(guardian), &info, WEXITED | WNOHANG | WNOWAIT); }
            while (result < 0 && errno == EINTR);
            if (result < 0) { broken = true; guardianExited = true; recordExit(); terminateLocked(); return; }
            if (info.si_pid != guardian) return;
            guardianExited = true;
            recordExit();
            // Drain may precede exit in this cycle; wait for actual channel EOF
            // before accepting it. No numeric PID or PGID signalling is used.
            if (!eof) return;
            if (broken || !cleanedEvent || info.si_code != CLD_EXITED || info.si_status != 0) {
                broken = true; terminateLocked(); return; // Retain unresolved ownership.
            }
            pid_t resultPid;
            do { resultPid = waitpid(guardian, nullptr, 0); } while (resultPid < 0 && errno == EINTR);
            if (resultPid != guardian) { broken = true; return; }
            reaped = cleaned = true;
            terminateLocked();
        }
    };

    namespace {
        class Custodian {
        public:
            std::mutex mutex;
            std::array<std::shared_ptr<GuardedChild::State>, 32> owned{};
            Custodian() {
                std::thread([this] {
                    for (;;) {
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            // Active callers own first observation/timestamp.
                            // Only abandoned/cancelled handles move to this reaper.
                            for (auto &state : owned) if (state && state.use_count() == 1) {
                                bool cleaned;
                                { std::lock_guard<std::mutex> childLock(state->mutex);
                                  state->refreshLocked(); cleaned = state->cleaned; }
                                if (cleaned) state.reset();
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                }).detach();
            }
        };
        Custodian &custodian() { static auto *value = new Custodian(); return *value; }
    }

    std::unique_ptr<GuardedChild> GuardedChild::launch(const std::vector<std::string> &arguments) {
        // A service must never escape its owning group by launching another
        // independently grouped worker. Its existing guardian owns all work.
        if (inGuardedWorker()) return {};
        if (arguments.empty() || arguments.size() > 32 || arguments.front().empty() || arguments.front()[0] != '/') return {};
        for (const auto &argument : arguments) if (argument.size() > 4096 || argument.find('\0') != std::string::npos) return {};
        const auto executable = siblingExecutable("duel6r-darwin-guardian");
        if (executable.empty()) return {};
        auto state = std::make_shared<State>();
        auto child = std::unique_ptr<GuardedChild>(new GuardedChild(state));
        auto &owner = custodian();
        std::lock_guard<std::mutex> reservation(owner.mutex);
        auto slot = owner.owned.end();
        for (auto it = owner.owned.begin(); it != owner.owned.end(); ++it) if (!*it) { slot = it; break; }
        if (slot == owner.owned.end()) return {};
        int pairs[4][2]{{-1,-1},{-1,-1},{-1,-1},{-1,-1}};
        struct ClosePairs { int (*pairs)[2]; ~ClosePairs() { for (unsigned i=0;i<4;++i) for(int fd:pairs[i]) if(fd>=0) close(fd); } } closePairs{pairs};
        for (auto &pair : pairs) if (!socketPair(pair)) return {};
        for (int fd : {pairs[1][0], pairs[2][0], pairs[3][0]}) {
            const int flags = fcntl(fd, F_GETFL);
            if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return {};
        }
        std::vector<std::string> strings{executable, std::to_string(getpid()), "3", "4", "--worker-io", "5", "6"};
        strings.insert(strings.end(), arguments.begin(), arguments.end());
        std::vector<char *> argv;
        for (auto &argument : strings) argv.push_back(argument.data());
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t attributes;
        if (posix_spawn_file_actions_init(&actions) != 0) return {};
        if (posix_spawnattr_init(&attributes) != 0) { posix_spawn_file_actions_destroy(&actions); return {}; }
        int error = 0;
        for (int fd = 0; fd < 3 && !error; ++fd)
            error = posix_spawn_file_actions_addopen(&actions, fd, "/dev/null", fd ? O_WRONLY : O_RDONLY, 0);
        for (unsigned i = 0; i < 4 && !error; ++i) {
            error = posix_spawn_file_actions_adddup2(&actions, pairs[i][1], static_cast<int>(3 + i));
            if (!error) error = posix_spawn_file_actions_addclose(&actions, pairs[i][1]);
        }
        if (!error) error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
        char *environment[] = {nullptr};
        if (!error) error = posix_spawn(&state->guardian, executable.c_str(), &actions, &attributes, argv.data(), environment);
        posix_spawnattr_destroy(&attributes); posix_spawn_file_actions_destroy(&actions);
        if (error) return {};
        state->life = pairs[0][0]; state->events = pairs[1][0];
        state->output = pairs[2][0]; state->input = pairs[3][0];
        for (auto &pair : pairs) pair[0] = -1;
        *slot = state;
        return child;
    }
    GuardedChild::GuardedChild(std::shared_ptr<State> state) : state(std::move(state)) {}
    GuardedChild::~GuardedChild() { terminate(); }
    int GuardedChild::output() const { return state->output; }
    int GuardedChild::input() const { return state->input; }
    void GuardedChild::terminate() { std::lock_guard<std::mutex> lock(state->mutex); state->terminateLocked(); }
    bool GuardedChild::exited() { std::lock_guard<std::mutex> lock(state->mutex); state->refreshLocked(); return state->workerExited || state->guardianExited || state->spawnFailed || state->broken; }
    std::chrono::steady_clock::time_point GuardedChild::exitObservedAt() { std::lock_guard<std::mutex> lock(state->mutex); return state->exitObservation; }
    bool GuardedChild::startupFailed() { std::lock_guard<std::mutex> lock(state->mutex); state->refreshLocked(); return state->spawnFailed; }
    bool GuardedChild::failed() { std::lock_guard<std::mutex> lock(state->mutex); state->refreshLocked(); return state->broken; }
    bool GuardedChild::cleanupConfirmed() { std::lock_guard<std::mutex> lock(state->mutex); state->refreshLocked(); return state->cleaned; }
    bool GuardedChild::waitForCleanup(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            if (cleanupConfirmed()) return true;
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (true);
    }
}
