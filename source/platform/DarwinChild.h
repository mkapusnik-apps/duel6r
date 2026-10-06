#ifndef DUEL6_DARWIN_CHILD_H
#define DUEL6_DARWIN_CHILD_H

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Duel6::Platform::Darwin {
    std::string executablePath();
    std::string siblingExecutable(const char *name);
    bool socketPair(int (&descriptors)[2]);

    // Owns the guardian, not a bare numeric service PID. A bounded process-wide
    // custodian retains cancelled resolver ownership until positive cleanup.
    class GuardedChild {
    public:
        struct State;
        static std::unique_ptr<GuardedChild> launch(const std::vector<std::string> &arguments);
        ~GuardedChild();
        GuardedChild(const GuardedChild &) = delete;
        GuardedChild &operator=(const GuardedChild &) = delete;
        int output() const;
        int input() const;
        void terminate();
        bool exited();
        std::chrono::steady_clock::time_point exitObservedAt();
        bool failed();
        bool startupFailed();
        bool cleanupConfirmed();
        bool waitForCleanup(std::chrono::milliseconds timeout);
    private:
        explicit GuardedChild(std::shared_ptr<State> state);
        std::shared_ptr<State> state;
    };
}
#endif
