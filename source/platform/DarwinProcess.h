#ifndef DUEL6_DARWIN_PROCESS_H
#define DUEL6_DARWIN_PROCESS_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <sys/types.h>
#endif

namespace Duel6::Platform::Darwin {
    enum class GroupInspection { Unknown, Descendants, LeaderOnly };

    // The observer cannot manufacture ownership. Only the direct-child wait
    // boundary can release the anchor. Unknown inspection is never tree-zero.
    class CleanupState {
    public:
        void observeExit() { exited = true; }
        void loseAnchor() { anchored = false; zero = 0; }
        bool maySignal() const { return anchored; }
        bool observeGroup(GroupInspection result) {
            if (!anchored || !exited || result != GroupInspection::LeaderOnly) zero = 0;
            else if (zero < 2) ++zero;
            return anchored && exited && zero == 2;
        }
    private:
        bool anchored = true, exited = false;
        unsigned zero = 0;
    };

    // Fixed eight-byte local status: big-endian event followed by uint32 PID.
    // Started is spawn notification, NEVER application/service readiness.
    enum class GuardianEvent : unsigned { Started = 1, LeaderExited = 2, Cleaned = 3, Failed = 4 };

#ifdef __APPLE__
    GroupInspection inspectGroup(pid_t leader);
    // parentLife has the sole writer in the owning application. status is a
    // private socket, not stdout. Inputs must not contain credentials. This
    // entry owns neither a shell nor arbitrary inherited environment/FDs.
    int runGuardian(pid_t parent, int parentLife, int status,
                    const std::vector<std::string> &workerArguments,
                    const std::function<GroupInspection(pid_t)> &inspect = inspectGroup);

    class ParentMonitor {
    public:
        // Call before any worker initialization/listener. Requires this worker
        // to lead its own group, directly parented by the guardian.
        static std::unique_ptr<ParentMonitor> start(pid_t guardian, int life);
        ~ParentMonitor();
        ParentMonitor(const ParentMonitor &) = delete;
        ParentMonitor &operator=(const ParentMonitor &) = delete;
    private:
        ParentMonitor();
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
#endif
}
#endif
