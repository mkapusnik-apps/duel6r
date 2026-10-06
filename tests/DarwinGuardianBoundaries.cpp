#include "source/platform/DarwinProcess.h"
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <libproc.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
    std::string fault, directory;
    bool registrationObserved = false, anchorReaped = false;
    unsigned sends = 0;
    int inheritedHighFd = -1;

    void mark(const char *name, const std::string &value) {
        const auto temporary = directory + '/' + name + ".part";
        { std::ofstream file(temporary); file << value; if (!file) _exit(70); }
        if (rename(temporary.c_str(), (directory + '/' + name).c_str()) != 0) _exit(70);
    }
    void barrier(const char *name) {
        mark("barrier", name);
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (access((directory + "/release").c_str(), F_OK) != 0) {
            if (std::chrono::steady_clock::now() >= until) _exit(71);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    int observedKevent(int queue, const struct kevent *changes, int nchanges,
                       struct kevent *events, int nevents, const struct timespec *timeout) {
        const bool registration = !registrationObserved && nchanges == 2;
        if (registration) {
            registrationObserved = true;
            if (fault == "registration-failure") {
                mark("registration-failed", "EACCES"); errno = EACCES; return -1;
            }
            if (fault == "before-registration") barrier("before-registration");
        }
        const int result = kevent(queue, changes, nchanges, events, nevents, timeout);
        if (registration && result == 0 && fault == "after-registration") barrier("after-registration");
        return result;
    }
    int observedSpawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *actions,
                      const posix_spawnattr_t *attributes, char *const arguments[], char *const environment[]) {
        if (fault == "before-spawn") barrier("before-spawn");
        if (fault == "leaked-high-fd"
            && posix_spawn_file_actions_addinherit_np(const_cast<posix_spawn_file_actions_t *>(actions),
                                                      inheritedHighFd) != 0) _exit(74);
        const int result = posix_spawn(pid, path, actions, attributes, arguments, environment);
        if (result == 0) {
            if (fault == "leaked-high-fd") {
                // This negative worker can exit before libproc can describe
                // it. Exact-child wait/reap below is the ownership evidence.
                mark("spawned", std::to_string(*pid));
                return result;
            }
            proc_bsdinfo info{};
            if (proc_pidinfo(*pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) _exit(72);
            mark("spawned", std::to_string(info.pbi_pid) + " " + std::to_string(info.pbi_start_tvsec)
                 + " " + std::to_string(info.pbi_start_tvusec));
            if (fault == "after-spawn") barrier("after-spawn");
        }
        return result;
    }
    int observedWait(idtype_t type, id_t pid, siginfo_t *info, int options) {
        const int result = waitid(type, pid, info, options);
        if (result == 0 && info->si_pid == static_cast<pid_t>(pid))
            mark("worker-exit-status", std::to_string(info->si_status));
        if (fault == "lost-anchor" && !anchorReaped && result == 0 && info->si_pid == static_cast<pid_t>(pid)) {
            // Actually reap the exact direct child, then let the real syscall
            // report ECHILD. This is not a fabricated ownership-state flag.
            if (waitpid(static_cast<pid_t>(pid), nullptr, WNOHANG) != static_cast<pid_t>(pid)) _exit(73);
            anchorReaped = true;
            mark("anchor-reaped", std::to_string(pid));
            *info = {};
            return waitid(type, pid, info, options);
        }
        return result;
    }
    int observedKill(pid_t pid, int signal) {
        if (anchorReaped && pid < 0) mark("signal-after-reap", std::to_string(pid));
        return kill(pid, signal);
    }
    pid_t observedReap(pid_t pid, int *status, int options) {
        const pid_t result = waitpid(pid, status, options);
        if (result == pid) mark("reaped-by-cleanup", std::to_string(pid));
        return result;
    }
    ssize_t observedSend(int socket, const void *bytes, size_t count, int flags) {
        mark("sends", std::to_string(++sends));
        // Real delivery of exactly half the first fixed frame. Subsequent
        // publication attempts are recorded even if the socket is shut down.
        if (fault == "partial-status" && sends == 1) return send(socket, bytes, count / 2, flags);
        return send(socket, bytes, count, flags);
    }
}

// These narrow observers compile only into this regression executable. Headers
// are loaded above before substitution; production targets have no fault flags.
#define kevent(...) observedKevent(__VA_ARGS__)
#define posix_spawn(...) observedSpawn(__VA_ARGS__)
#define waitid(...) observedWait(__VA_ARGS__)
#define waitpid(...) observedReap(__VA_ARGS__)
#define kill(...) observedKill(__VA_ARGS__)
#define send(...) observedSend(__VA_ARGS__)
#include "source/platform/DarwinProcess.cpp"
#undef kevent
#undef posix_spawn
#undef waitid
#undef waitpid
#undef kill
#undef send

#define main guardianEntry
#include "source/platform/DarwinGuardianMain.cpp"
#undef main

int main(int argc, char **argv) {
    const char *selected = std::getenv("D6R_TEST_GUARDIAN_FAULT");
    const char *root = std::getenv("D6R_TEST_GUARDIAN_DIRECTORY");
    const char *highFd = std::getenv("D6R_TEST_GUARDIAN_HIGH_FD");
    if (!selected || !root || !highFd) return 2;
    fault = selected; directory = root;
    inheritedHighFd = std::atoi(highFd);
    if (inheritedHighFd <= 1023 || fcntl(inheritedHighFd, F_GETFD) < 0) return 2;
    mark("high-fd-observed", std::to_string(inheritedHighFd));
    const int result = guardianEntry(argc, argv);
    mark("guardian-result", std::to_string(result));
    return result;
}
