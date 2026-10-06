#include "TestHarness.h"
#include "source/platform/DarwinChild.h"
#include "source/platform/DarwinProcess.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
    std::atomic<pid_t> trackedPid{-1};
    std::atomic<bool> modelReusedPid{false};
    std::atomic<unsigned> waits{0}, reaps{0}, signals{0};
    std::atomic<int> observedWaitError{0};
    int delayedObservation(idtype_t type, id_t id, siginfo_t *info, int flags) {
        const bool tracked = static_cast<pid_t>(id) == trackedPid.load();
        if (tracked) {
            ++waits;
            if (modelReusedPid) {
                *info = {}; info->si_pid = static_cast<pid_t>(id);
                info->si_code = CLD_EXITED; info->si_status = 0;
                return 0; // Model a different child reusing this number, not ownership.
            }
        }
        const int result = waitid(type, id, info, flags); // Real WNOWAIT; never reap here.
        if (tracked) observedWaitError = result < 0 ? errno : 0;
        if (!permitObservation && result == 0) *info = {};
        return result;
    }
    pid_t observedReap(pid_t pid, int *status, int flags) {
        if (pid == trackedPid.load()) {
            ++reaps;
            if (modelReusedPid) return pid; // Unsafe follow-up would falsely confirm cleanup.
        }
        return waitpid(pid, status, flags);
    }
    int observedSignal(pid_t pid, int signal) {
        if (pid == trackedPid.load()) {
            ++signals;
            if (modelReusedPid) return 0;
        }
        return kill(pid, signal);
    }
}
#define waitid(...) delayedObservation(__VA_ARGS__)
#define waitpid(...) observedReap(__VA_ARGS__)
#define kill(...) observedSignal(__VA_ARGS__)
#include "source/platform/DarwinChild.cpp"
#undef waitid
#undef waitpid
#undef kill

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

D6R_TEST_CASE("Darwin lost resolver ownership permanently quarantines PID operations and slot") {
    constexpr const char *caseName = "Darwin lost resolver ownership permanently quarantines PID operations and slot";
    if (std::getenv("D6R_LOSS_FIXTURE_CHILD")) {
        // A quarantined production record must prevent normal completed exit.
        // Isolate this negative fixture; only the test forcibly exits, after
        // disposing every real child. There is no production release shortcut.
        try {
            auto victim = GuardedChild::launchResolver({D6R_RESOLVER_FIXTURE, "block"});
            D6R_REQUIRE(victim);
            const auto old = entered(*victim);
            D6R_REQUIRE(::kill(old[0], SIGKILL) == 0);
            pid_t stolen;
            do { stolen = ::waitpid(old[0], nullptr, 0); } while (stolen < 0 && errno == EINTR);
            D6R_REQUIRE_EQ(old[0], stolen); // Actual unexpected exact-child reap.
            trackedPid = old[0];
            D6R_REQUIRE(!victim->cleanupConfirmed());
            D6R_REQUIRE_EQ(ECHILD, observedWaitError.load());
            const auto baselineWaits = waits.load();
            const auto baselineReaps = reaps.load();
            const auto baselineSignals = signals.load();
            modelReusedPid = true;
            auto unrelated = GuardedChild::launchResolver({D6R_RESOLVER_FIXTURE, "block"});
            D6R_REQUIRE(unrelated);
            const auto other = entered(*unrelated);
            D6R_REQUIRE(other[0] != old[0]);
            for (unsigned attempt = 0; attempt < 32; ++attempt) {
                D6R_REQUIRE(victim->failed());
                D6R_REQUIRE(victim->exited()); // Terminal failure, not confirmed cleanup.
                D6R_REQUIRE(!victim->cleanupConfirmed());
                D6R_REQUIRE(!victim->waitForCleanup(0ms));
                victim->terminate();
            }
            victim.reset();
            {
                auto &owner = custodian();
                std::lock_guard<std::mutex> lock(owner.mutex);
                bool found = false;
                for (const auto &state : owner.owned) if (state && state->guardian == old[0]) {
                    std::lock_guard<std::mutex> childLock(state->mutex);
                    for (unsigned i = 0; i < 32; ++i) state->refreshLocked();
                    D6R_REQUIRE(state->anchorLost && state->broken && !state->cleaned && !state->reaped);
                    found = true;
                }
                D6R_REQUIRE(found);
            }
            D6R_REQUIRE_EQ(baselineWaits, waits.load());
            D6R_REQUIRE_EQ(baselineReaps, reaps.load());
            D6R_REQUIRE_EQ(baselineSignals, signals.load());
            D6R_REQUIRE(::kill(other[0], 0) == 0);
            unrelated->terminate();
            D6R_REQUIRE(unrelated->waitForCleanup(3s));
            unrelated.reset();
            std::cout << "lost-anchor: actual ECHILD; zero subsequent PID operations; slot quarantined; unrelated child survived\n" << std::flush;
            _exit(0);
        } catch (const std::exception &error) {
            std::cerr << "lost-anchor fixture failed: " << error.what() << '\n' << std::flush;
            _exit(1);
        }
    }
    auto executable = executablePath();
    std::string filter = std::string("D6R_TEST_FILTER=") + caseName;
    char exact[] = "D6R_TEST_EXACT=1", childFlag[] = "D6R_LOSS_FIXTURE_CHILD=1";
    char *environment[] = {filter.data(), exact, childFlag, nullptr};
    char *arguments[] = {executable.data(), nullptr};
    pid_t process = -1;
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    D6R_REQUIRE_EQ(0, posix_spawn_file_actions_init(&actions));
    D6R_REQUIRE_EQ(0, posix_spawnattr_init(&attributes));
    int setup = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    if (!setup) setup = posix_spawn_file_actions_addinherit_np(&actions, 1);
    if (!setup) setup = posix_spawn_file_actions_addinherit_np(&actions, 2);
    if (!setup) setup = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);
    const int spawned = setup ? setup : posix_spawn(&process, executable.c_str(), &actions, &attributes, arguments, environment);
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
    D6R_REQUIRE_EQ(0, spawned);
    int status = 0;
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    pid_t observed = 0;
    do {
        observed = waitpid(process, &status, WNOHANG);
        if (observed == process) break;
        if (observed < 0 && errno != EINTR)
            Duel6::Test::fail("test subprocess ownership retained", __FILE__, __LINE__);
        std::this_thread::sleep_for(1ms);
    } while (std::chrono::steady_clock::now() < deadline);
    if (observed != process) {
        kill(process, SIGKILL); waitpid(process, &status, 0);
        Duel6::Test::fail("isolated ownership fixture completed", __FILE__, __LINE__);
    }
    D6R_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
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
