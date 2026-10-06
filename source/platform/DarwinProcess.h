#ifndef DUEL6_DARWIN_PROCESS_H
#define DUEL6_DARWIN_PROCESS_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
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

    // Fixed local status: event, PID and origin monotonic nanoseconds. Each
    // attempt has a fresh private socket; origin time is validated against it.
    // Started is spawn notification, NEVER application/service readiness.
    enum class GuardianEvent : unsigned { Started = 1, LeaderExited = 2, Cleaned = 3, Failed = 4 };
    constexpr std::size_t GuardianFrameBytes = 16;
    using GuardianFrame = std::array<unsigned char, GuardianFrameBytes>;
    inline std::uint64_t monotonicNanoseconds() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    inline GuardianFrame guardianFrame(GuardianEvent event, std::uint32_t pid, std::uint64_t origin) {
        GuardianFrame bytes{};
        const std::uint32_t words[] = {static_cast<std::uint32_t>(event), pid};
        for (unsigned w = 0; w < 2; ++w) for (unsigned b = 0; b < 4; ++b)
            bytes[w * 4 + b] = static_cast<unsigned char>(words[w] >> (24 - 8 * b));
        for (unsigned b = 0; b < 8; ++b) bytes[8 + b] = static_cast<unsigned char>(origin >> (56 - 8 * b));
        return bytes;
    }
    struct GuardianStatus { GuardianEvent event; std::uint32_t pid; std::uint64_t origin; };
    inline std::optional<GuardianStatus> decodeGuardianFrame(const GuardianFrame &bytes,
            std::uint64_t attemptBegan, std::uint64_t lastOrigin, std::uint64_t observedNow) {
        std::uint32_t kind = 0, pid = 0;
        std::uint64_t origin = 0;
        for (unsigned i = 0; i < 4; ++i) { kind = (kind << 8) | bytes[i]; pid = (pid << 8) | bytes[i + 4]; }
        for (unsigned i = 8; i < 16; ++i) origin = (origin << 8) | bytes[i];
        if (kind < 1 || kind > 4 || pid > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            || ((kind == 1 || kind == 2) && pid <= 1) || origin == 0 || origin < attemptBegan
            || origin < lastOrigin || origin > observedNow
            || origin > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return {};
        return GuardianStatus{static_cast<GuardianEvent>(kind), pid, origin};
    }

#ifdef __APPLE__
    bool inGuardedWorker();
    struct WorkerChannels {
        int output = -1; // Mapped to worker FD 4; never interpreted by guardian.
        int input = -1;  // Mapped to worker FD 5.
    };
    GroupInspection inspectGroup(pid_t leader);
    // parentLife has the sole writer in the owning application. status is a
    // private socket, not stdout. Inputs must not contain credentials. This
    // entry owns neither a shell nor arbitrary inherited environment/FDs.
    int runGuardian(pid_t parent, int parentLife, int status,
                    const std::vector<std::string> &workerArguments,
                    const std::function<GroupInspection(pid_t)> &inspect = inspectGroup,
                    WorkerChannels channels = {});

    class ParentMonitor {
    public:
        // Call before initialization/listening. Group owners lead their group;
        // resolver leaves instead share the parent's group and kill only self.
        static std::unique_ptr<ParentMonitor> start(pid_t guardian, int life, bool ownsGroup = true);
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
