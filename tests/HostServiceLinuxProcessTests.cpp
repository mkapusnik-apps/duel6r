#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <optional>
#include <pthread.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "tests/TestHarness.h"
#include "source/client/HostServiceSupervisor.h"

#ifndef D6R_HOST_SERVICE_TEST_CHILD
#error D6R_HOST_SERVICE_TEST_CHILD must name the native lifecycle test child
#endif

namespace {
using namespace Duel6;
using namespace std::chrono_literals;

void ignoreSignal(int) {}

std::filesystem::path markerPath(const char *label) {
    static std::atomic<unsigned> sequence{0};
    return std::filesystem::temp_directory_path() /
           (std::string("duel6r-host-") + label + '-' + std::to_string(getpid()) + '-'
            + std::to_string(sequence.fetch_add(1)) + ".pid");
}

Client::HostServiceStartConfig treeConfig(const std::filesystem::path &marker) {
    Client::HostServiceStartConfig config;
    config.serverExecutable = std::filesystem::absolute(D6R_HOST_SERVICE_TEST_CHILD).string();
    config.endpoint = {"127.0.0.1", 34127};
    config.resourcePath = (std::filesystem::temp_directory_path() / "tree").string();
    config.enabledGameplayScripts = {marker.string()};
    config.localPlayers = 1;
    return config;
}

pid_t waitForPublishedPid(const std::filesystem::path &marker) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    do {
        std::ifstream stream(marker);
        long long value = 0;
        char trailing = 0;
        if (stream >> value && value > 0 && !(stream >> trailing)) return static_cast<pid_t>(value);
        std::this_thread::sleep_for(2ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return 0;
}

struct NumericGroupOwnershipSeam {
    int ownedGroup = 0;
    bool ownershipAnchor = true;
    bool treeComplete = false;
    std::vector<int> signalledGroups;

    void force() {
        if (ownershipAnchor && !treeComplete) signalledGroups.push_back(ownedGroup);
    }
    void exactLeaderReap() {
        ownershipAnchor = false;
        treeComplete = true;
    }
};
}

D6R_TEST_CASE("HSL-AC-014 AC-015 Linux keeps WNOWAIT leader ownership until descendants and two tree-zero observations") {
    const auto marker = markerPath("wnowait");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    auto child = Client::launchHostServiceProcess(treeConfig(marker));
    D6R_REQUIRE(child != nullptr);
    const pid_t descendant = waitForPublishedPid(marker);
    D6R_REQUIRE(descendant > 0);

    child->requestStop();
    Client::HostServiceExitEvent exit;
    const auto exitDeadline = std::chrono::steady_clock::now() + 3s;
    while (!child->observeExit(exit, [] { return std::chrono::steady_clock::now(); })
           && std::chrono::steady_clock::now() < exitDeadline)
        std::this_thread::sleep_for(1ms);
    const auto observed = child->processSnapshot();
    D6R_REQUIRE(observed.leaderExitObserved);
    D6R_REQUIRE(observed.ownershipAnchorRetained);
    D6R_REQUIRE(!observed.treeComplete);
    D6R_REQUIRE(kill(descendant, 0) == 0);

    child->forceTerminate();
    D6R_REQUIRE(!child->cleanupConfirmed());
    const auto cleanupDeadline = std::chrono::steady_clock::now() + 3s;
    while (!child->cleanupConfirmed() && std::chrono::steady_clock::now() < cleanupDeadline)
        std::this_thread::sleep_for(1ms);
    D6R_REQUIRE(child->cleanupConfirmed());
    const auto complete = child->processSnapshot();
    D6R_REQUIRE(complete.treeComplete);
    D6R_REQUIRE(!complete.ownershipAnchorRetained);
    const auto signals = complete.forceSignalAttempts;
    child->forceTerminate();
    D6R_REQUIRE_EQ(signals, child->processSnapshot().forceSignalAttempts);
    std::filesystem::remove(marker, ignored);
}

D6R_TEST_CASE("HSL-AC-014 Linux leader reap remains EINTR-safe") {
    const auto marker = markerPath("eintr");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    auto child = Client::launchHostServiceProcess(treeConfig(marker));
    D6R_REQUIRE(child != nullptr);
    D6R_REQUIRE(waitForPublishedPid(marker) > 0);
    child->requestStop();
    child->forceTerminate();

    struct sigaction action{};
    struct sigaction previous{};
    action.sa_handler = ignoreSignal;
    sigemptyset(&action.sa_mask);
    D6R_REQUIRE(sigaction(SIGUSR1, &action, &previous) == 0);
    const pthread_t waitingThread = pthread_self();
    std::thread interrupter([&] {
        for (unsigned attempt = 0; attempt < 256; ++attempt) {
            pthread_kill(waitingThread, SIGUSR1);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });
    const bool cleaned = child->waitForCleanup(3s);
    interrupter.join();
    sigaction(SIGUSR1, &previous, nullptr);
    D6R_REQUIRE(cleaned);
    D6R_REQUIRE(child->processSnapshot().treeComplete);
    std::filesystem::remove(marker, ignored);
}

D6R_TEST_CASE("HSL-AC-015 numeric PGID reuse seam never signals an unrelated group after exact leader reap") {
    NumericGroupOwnershipSeam seam;
    seam.ownedGroup = 24001;
    seam.force();
    D6R_REQUIRE_EQ(std::size_t(1), seam.signalledGroups.size());
    D6R_REQUIRE_EQ(24001, seam.signalledGroups.front());
    seam.exactLeaderReap();
    seam.ownedGroup = 24001; // The same numeric PGID now denotes an unrelated process group.
    seam.force();
    D6R_REQUIRE_EQ(std::size_t(1), seam.signalledGroups.size());
}
