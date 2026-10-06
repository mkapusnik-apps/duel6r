#include "TestHarness.h"
#include "source/platform/DarwinChild.h"
#include "source/platform/DarwinProcess.h"
#include <algorithm>
#include <array>
#include <atomic>
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

namespace {
    std::atomic<bool> permitObservation{true};
    int delayedObservation(idtype_t type, id_t id, siginfo_t *info, int flags) {
        const int result = waitid(type, id, info, flags); // Real WNOWAIT; never reap here.
        if (!permitObservation && result == 0) *info = {};
        return result;
    }
}
#define waitid(...) delayedObservation(__VA_ARGS__)
#include "source/platform/DarwinChild.cpp"
#undef waitid

namespace {
    using namespace Duel6::Platform::Darwin;
    using namespace std::chrono_literals;
    std::array<int, 2> entered(GuardedChild &child) {
        std::array<int, 2> identity{};
        auto *bytes = reinterpret_cast<unsigned char *>(identity.data());
        std::size_t used = 0;
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (used < sizeof(identity) && std::chrono::steady_clock::now() < deadline) {
            const auto count = recv(child.output(), bytes + used, sizeof(identity) - used, 0);
            if (count > 0) used += static_cast<std::size_t>(count);
            else std::this_thread::sleep_for(1ms);
        }
        D6R_REQUIRE_EQ(sizeof(identity), used);
        return identity;
    }
    std::size_t retained() {
        auto &owner = custodian(); // Same-TU test observation; no shipped test API.
        std::lock_guard<std::mutex> lock(owner.mutex);
        return static_cast<std::size_t>(std::count_if(owner.owned.begin(), owner.owned.end(), [](const auto &p) { return bool(p); }));
    }
    bool released() {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        while (retained() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
        return retained() == 0;
    }
}

D6R_TEST_CASE("Live same-group resolver cancellation retains exact children slots until delayed cleanup is observed") {
    D6R_REQUIRE(released());
    struct Restore { ~Restore() { permitObservation = true; } } restore;
    permitObservation = false;
    for (unsigned slot = 0; slot < 32; ++slot) {
        auto child = GuardedChild::launchResolver({D6R_RESOLVER_FIXTURE, "block"});
        D6R_REQUIRE(child);
        const auto identity = entered(*child);
        D6R_REQUIRE_EQ(getpgrp(), identity[1]); // No escape from the owning service group.
        D6R_REQUIRE(kill(identity[0], 0) == 0);
        child->terminate();
        D6R_REQUIRE(!child->waitForCleanup(10ms));
        child.reset();
        D6R_REQUIRE_EQ(static_cast<std::size_t>(slot + 1), retained());
    }
    D6R_REQUIRE(!GuardedChild::launchResolver({D6R_RESOLVER_FIXTURE, "block"}));
    permitObservation = true;
    D6R_REQUIRE(released());
    auto next = GuardedChild::launchResolver({D6R_RESOLVER_FIXTURE, "block"});
    D6R_REQUIRE(next);
    entered(*next);
    next->terminate();
    D6R_REQUIRE(next->waitForCleanup(3s));
    next.reset();
    D6R_REQUIRE(released());
}

D6R_TEST_CASE("Guardian origin exit stays before startup deadline when GUI first polls after that deadline") {
    const auto began = std::chrono::steady_clock::now();
    const auto deadline = began + 10s;
    auto child = GuardedChild::launch({D6R_RESOLVER_FIXTURE, "exit"});
    D6R_REQUIRE(child);
    const auto identity = entered(*child);
    const auto observeUntil = began + 3s;
    bool gone = false;
    while (std::chrono::steady_clock::now() < observeUntil) {
        if (kill(identity[0], 0) != 0 && errno == ESRCH) { gone = true; break; }
        std::this_thread::sleep_for(1ms);
    }
    D6R_REQUIRE(gone); // Guardian already observed/reaped; do not poll its IPC yet.
    std::this_thread::sleep_until(deadline + 5ms);
    D6R_REQUIRE(child->exited());
    D6R_REQUIRE(child->exitObservedAt() >= began);
    D6R_REQUIRE(child->exitObservedAt() < deadline);
    const auto origin = child->exitObservedAt();
    D6R_REQUIRE(child->waitForCleanup(3s));
    D6R_REQUIRE_EQ(origin, child->exitObservedAt());
}
