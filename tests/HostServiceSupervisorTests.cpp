#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "tests/TestHarness.h"
#include "source/client/HostServiceSupervisor.h"
#include "source/network/HostServiceControlProtocol.h"

namespace {
using namespace Duel6;
using namespace std::chrono_literals;

class ManualClock {
public:
    Client::HostServiceTimePoint now() const {
        return Client::HostServiceTimePoint{} + std::chrono::nanoseconds(ticks.load());
    }
    void set(std::chrono::nanoseconds value) { ticks.store(value.count()); }
    void advance(std::chrono::nanoseconds value) { ticks.fetch_add(value.count()); }
private:
    std::atomic<std::int64_t> ticks{0};
};

struct ChildState {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Client::HostServiceStatusEvent> events;
    std::atomic<bool> exited{false};
    std::atomic<bool> treeGone{true};
    std::atomic<unsigned> stopRequests{0};
    std::atomic<unsigned> waits{0};
    std::atomic<unsigned> forces{0};
    bool cooperative = true;
    bool forceWorks = true;
    std::function<void()> onWait;
    std::function<void()> onForce;

    void status(Network::HostServiceStatusCode code, Client::HostServiceTimePoint receivedAt) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            events.push_back({code, receivedAt});
        }
        changed.notify_all();
    }
};

class FakeChild final : public Client::HostServiceChild {
public:
    explicit FakeChild(std::shared_ptr<ChildState> state) : state(std::move(state)) {}
    bool readStatus(Client::HostServiceStatusEvent &event, std::chrono::milliseconds timeout) override {
        std::unique_lock<std::mutex> lock(state->mutex);
        state->changed.wait_for(lock, timeout, [&] { return !state->events.empty(); });
        if (state->events.empty()) return false;
        event = state->events.front();
        state->events.pop_front();
        return true;
    }
    bool hasExited() override { return state->exited.load(); }
    void requestStop() override {
        state->stopRequests.fetch_add(1);
        if (state->cooperative) state->exited.store(true);
    }
    bool waitForExit(std::chrono::milliseconds) override {
        state->waits.fetch_add(1);
        if (state->onWait) state->onWait();
        return state->exited.load();
    }
    void forceTerminate() override {
        state->forces.fetch_add(1);
        if (state->onForce) state->onForce();
        if (state->forceWorks) {
            state->exited.store(true);
            state->treeGone.store(true);
        }
    }
    bool cleanupConfirmed() override { return state->exited.load() && state->treeGone.load(); }
    bool waitForCleanup(std::chrono::milliseconds timeout) override {
        waitForExit(timeout);
        return cleanupConfirmed();
    }
private:
    std::shared_ptr<ChildState> state;
};

struct Fixture {
    ManualClock clock;
    std::mutex mutex;
    std::vector<Client::HostServiceStartConfig> launches;
    std::vector<std::shared_ptr<ChildState>> children;
    bool failLaunch = false;
    bool failMonitorLaunch = false;
    bool throwHandoff = false;
    std::vector<std::string> handoffs;
    std::function<void()> onHandoff;

    Client::HostServiceDependencies dependencies() {
        Client::HostServiceDependencies result;
        result.now = [this] { return clock.now(); };
        result.launcher = [this](const Client::HostServiceStartConfig &config) {
            std::lock_guard<std::mutex> lock(mutex);
            launches.push_back(config);
            if (failLaunch) return std::unique_ptr<Client::HostServiceChild>{};
            auto state = std::make_shared<ChildState>();
            children.push_back(state);
            return std::unique_ptr<Client::HostServiceChild>(new FakeChild(state));
        };
        result.intentionalEndHandoff = [this](const char *reason) {
            handoffs.emplace_back(reason ? reason : "");
            if (onHandoff) onHandoff();
            if (throwHandoff) throw std::runtime_error("injected handoff failure");
        };
        if (failMonitorLaunch) {
            result.monitorLauncher = [](std::function<void()>) -> std::thread {
                throw std::runtime_error("injected monitor launch failure");
            };
        }
        return result;
    }
    Client::HostServiceStartConfig config() const {
        Client::HostServiceStartConfig value;
#ifdef _WIN32
        value.serverExecutable = "C:\\trusted\\duel6r-server.exe";
        value.resourcePath = "C:\\trusted\\resources";
#else
        value.serverExecutable = "/trusted/duel6r-server";
        value.resourcePath = "/trusted/resources";
#endif
        value.endpoint = {"127.0.0.1", 34127};
        value.localPlayers = 2;
        value.enabledGameplayScripts = {"profiles/host/gameplay.lua"};
        return value;
    }
    std::shared_ptr<ChildState> child(std::size_t index = 0) {
        std::lock_guard<std::mutex> lock(mutex);
        return children.at(index);
    }
    std::size_t launchCount() {
        std::lock_guard<std::mutex> lock(mutex);
        return launches.size();
    }
};

void requireState(Client::HostServiceSupervisor &supervisor, Client::HostServiceState state) {
    D6R_REQUIRE(supervisor.waitForState(state, 2s));
    D6R_REQUIRE_EQ(state, supervisor.snapshot().state);
}

void requireCleanup(Client::HostServiceSupervisor &supervisor) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!supervisor.snapshot().cleanupComplete && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
}

void makeReady(Fixture &fixture, Client::HostServiceSupervisor &supervisor,
               std::chrono::nanoseconds receivedAt = 1ns) {
    fixture.child()->status(Network::HostServiceStatusCode::Ready,
                            Client::HostServiceTimePoint{} + receivedAt);
    requireState(supervisor, Client::HostServiceState::Active);
}

void finishByExit(Fixture &fixture, Client::HostServiceSupervisor &supervisor,
                  Client::HostServiceState expected) {
    fixture.child()->exited.store(true);
    fixture.child()->changed.notify_all();
    requireState(supervisor, expected);
}
}

D6R_TEST_CASE("HSL-AC-008 fixed outcome identifiers and visible copy are byte exact") {
    using Client::HostServiceOutcome;
    struct Row { HostServiceOutcome outcome; const char *identifier; const char *copy; };
    const std::array<Row, 5> rows{{
        {HostServiceOutcome::StartFailed, "host-service-start-failed", "Hosted session could not start."},
        {HostServiceOutcome::PortUnavailable, "host-service-port-unavailable",
         "The selected port is unavailable. Choose another port and try again."},
        {HostServiceOutcome::ExitedBeforeReady, "host-service-exited-before-ready",
         "Hosted session stopped before it was ready."},
        {HostServiceOutcome::StartupTimedOut, "host-service-startup-timed-out",
         "Hosted session startup timed out."},
        {HostServiceOutcome::StoppedUnexpectedly, "host-service-stopped-unexpectedly",
         "Hosted session stopped unexpectedly."}
    }};
    for (const auto &row: rows) {
        D6R_REQUIRE_EQ(std::string(row.identifier), std::string(Client::hostServiceOutcomeIdentifier(row.outcome)));
        D6R_REQUIRE_EQ(std::string(row.copy), std::string(Client::hostServiceOutcomeCopy(row.outcome)));
    }
}

D6R_TEST_CASE("HSL-AC-001 AC-002 one supervisor owns one service and only explicit start launches") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE_EQ(std::size_t(0), fixture.launchCount());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    D6R_REQUIRE_EQ(std::size_t(1), fixture.launchCount());
    D6R_REQUIRE(!supervisor.start(fixture.config()));
    D6R_REQUIRE(!supervisor.retry());
    D6R_REQUIRE_EQ(std::size_t(1), fixture.launchCount());
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
}

D6R_TEST_CASE("HSL-AC-003 state machine rejects actions outside their exact source states") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(!supervisor.cancelStartup());
    D6R_REQUIRE(!supervisor.endSession());
    D6R_REQUIRE(!supervisor.retry());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    D6R_REQUIRE(!supervisor.endSession());
    makeReady(fixture, supervisor);
    D6R_REQUIRE(!supervisor.cancelStartup());
    D6R_REQUIRE(!supervisor.retry());
    D6R_REQUIRE(supervisor.endSession());
    D6R_REQUIRE(!supervisor.endSession());
    D6R_REQUIRE(!supervisor.start(fixture.config()));
    requireState(supervisor, Client::HostServiceState::NoService);
}

D6R_TEST_CASE("HSL-AC-004 AC-006 readiness is strict before ten seconds under delayed polling") {
    for (const auto received: {9999999999ns, 10000000000ns, 10000000001ns}) {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.clock.set(11s);
        fixture.child()->status(Network::HostServiceStatusCode::Ready,
                                Client::HostServiceTimePoint{} + received);
        if (received < 10s) {
            requireState(supervisor, Client::HostServiceState::Active);
            D6R_REQUIRE(supervisor.endSession());
            requireState(supervisor, Client::HostServiceState::NoService);
        } else {
            requireState(supervisor, Client::HostServiceState::StartupFailed);
            D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartupTimedOut, supervisor.snapshot().outcome);
        }
    }
}

D6R_TEST_CASE("HSL-AC-006 timeout is selected at exactly ten seconds") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    fixture.clock.set(10s);
    fixture.child()->changed.notify_all();
    requireState(supervisor, Client::HostServiceState::StartupFailed);
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartupTimedOut, supervisor.snapshot().outcome);
}

D6R_TEST_CASE("HSL-AC-007 AC-009 accepted Cancel wins over queued readiness and retains editable restart setup") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    const auto config = fixture.config();
    D6R_REQUIRE(supervisor.start(config));
    D6R_REQUIRE(supervisor.cancelStartup());
    fixture.child()->status(Network::HostServiceStatusCode::Ready, Client::HostServiceTimePoint{} + 1ns);
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::None, supervisor.snapshot().outcome);
    D6R_REQUIRE(supervisor.start(config));
    D6R_REQUIRE_EQ(std::size_t(2), fixture.launchCount());
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
}

D6R_TEST_CASE("HSL-AC-001 AC-009 stale duplicate and out-of-order status cannot replace the current result") {
    {
        Fixture fixture;
        fixture.clock.set(5s);
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.child()->status(Network::HostServiceStatusCode::Ready, Client::HostServiceTimePoint{} + 4s);
        fixture.child()->status(Network::HostServiceStatusCode::Ready, Client::HostServiceTimePoint{} + 6s);
        fixture.child()->status(Network::HostServiceStatusCode::Ready, Client::HostServiceTimePoint{} + 7s);
        requireState(supervisor, Client::HostServiceState::Active);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::None, supervisor.snapshot().outcome);
        D6R_REQUIRE(supervisor.endSession());
        requireState(supervisor, Client::HostServiceState::NoService);
    }
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.child()->status(Network::HostServiceStatusCode::PortUnavailable,
                                Client::HostServiceTimePoint{} + 1ns);
        fixture.child()->status(Network::HostServiceStatusCode::Ready,
                                Client::HostServiceTimePoint{} + 2ns);
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::PortUnavailable, supervisor.snapshot().outcome);
    }
}

D6R_TEST_CASE("HSL-AC-009 status and child-exit observation use one deterministic total order") {
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.child()->status(Network::HostServiceStatusCode::StartFailed,
                                Client::HostServiceTimePoint{} + 1ns);
        fixture.child()->exited.store(true);
        fixture.child()->changed.notify_all();
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartFailed, supervisor.snapshot().outcome);
    }
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.child()->status(Network::HostServiceStatusCode::Ready,
                                Client::HostServiceTimePoint{} + 1ns);
        fixture.child()->exited.store(true);
        fixture.child()->changed.notify_all();
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::ExitedBeforeReady, supervisor.snapshot().outcome);
    }
}

D6R_TEST_CASE("HSL-AC-006 AC-008 child exit is early only strictly before the startup deadline") {
    for (const auto observed: {9999999999ns, 10000000000ns, 10000000001ns}) {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.clock.set(observed);
        fixture.child()->exited.store(true);
        fixture.child()->changed.notify_all();
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        D6R_REQUIRE_EQ(observed < 10s ? Client::HostServiceOutcome::ExitedBeforeReady
                                     : Client::HostServiceOutcome::StartupTimedOut,
                       supervisor.snapshot().outcome);
    }
}

D6R_TEST_CASE("HSL-AC-008 AC-010 specific startup failures preserve outcome and retry only after cleanup") {
    const std::array<std::pair<Network::HostServiceStatusCode, Client::HostServiceOutcome>, 3> cases{{
        {Network::HostServiceStatusCode::HostManifestInvalid, Client::HostServiceOutcome::HostManifestInvalid},
        {Network::HostServiceStatusCode::PortUnavailable, Client::HostServiceOutcome::PortUnavailable},
        {Network::HostServiceStatusCode::StartFailed, Client::HostServiceOutcome::StartFailed}
    }};
    for (const auto &test: cases) {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.child()->status(test.first, Client::HostServiceTimePoint{} + 1ns);
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        const auto result = supervisor.snapshot();
        D6R_REQUIRE_EQ(test.second, result.outcome);
        D6R_REQUIRE_EQ(test.second != Client::HostServiceOutcome::HostManifestInvalid, result.retryAllowed);
        if (result.retryAllowed) {
            D6R_REQUIRE(supervisor.retry());
            D6R_REQUIRE_EQ(std::size_t(2), fixture.launchCount());
            D6R_REQUIRE(supervisor.cancelStartup());
            requireState(supervisor, Client::HostServiceState::NoService);
        }
    }
}

D6R_TEST_CASE("HSL-AC-008 early child exit before readiness is distinct and retryable") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    finishByExit(fixture, supervisor, Client::HostServiceState::StartupFailed);
    const auto result = supervisor.snapshot();
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::ExitedBeforeReady, result.outcome);
    D6R_REQUIRE(result.retryAllowed);
}

D6R_TEST_CASE("HSL-AC-009 application exit wins queued readiness and disables Retry") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    supervisor.applicationExit();
    fixture.child()->status(Network::HostServiceStatusCode::Ready, Client::HostServiceTimePoint{} + 1ns);
    requireState(supervisor, Client::HostServiceState::ApplicationExit);
    requireCleanup(supervisor);
    D6R_REQUIRE(!supervisor.snapshot().retryAllowed);
}

D6R_TEST_CASE("HSL-AC-011 unexpected post-ready exit is terminal with no restart or Retry") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    makeReady(fixture, supervisor);
    finishByExit(fixture, supervisor, Client::HostServiceState::SessionFailed);
    const auto result = supervisor.snapshot();
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::StoppedUnexpectedly, result.outcome);
    D6R_REQUIRE(!result.retryAllowed);
    D6R_REQUIRE(!supervisor.retry());
    D6R_REQUIRE_EQ(std::size_t(1), fixture.launchCount());
}

D6R_TEST_CASE("HSL-AC-012 AC-013 confirmed End and application shutdown are separate non-failure paths") {
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        makeReady(fixture, supervisor);
        D6R_REQUIRE(supervisor.endSession());
        requireState(supervisor, Client::HostServiceState::NoService);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::None, supervisor.snapshot().outcome);
        D6R_REQUIRE_EQ(std::size_t(1), fixture.handoffs.size());
        D6R_REQUIRE_EQ(std::string("intentional-host-end"), fixture.handoffs.front());
    }
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        makeReady(fixture, supervisor);
        supervisor.applicationExit();
        requireState(supervisor, Client::HostServiceState::ApplicationExit);
        requireCleanup(supervisor);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::None, supervisor.snapshot().outcome);
        D6R_REQUIRE(fixture.handoffs.empty());
    }
}

D6R_TEST_CASE("HSL-AC-012 intentional end handoff occurs exactly once and cannot extend cleanup") {
    for (const bool throws: {false, true}) {
        Fixture fixture;
        fixture.throwHandoff = throws;
        fixture.clock.set(20s);
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        makeReady(fixture, supervisor, 20s + 1ns);
        auto child = fixture.child();
        child->cooperative = false;
        child->onWait = [&] { fixture.clock.advance(10ms); };
        unsigned stopsAtHandoff = 99;
        fixture.onHandoff = [&] { stopsAtHandoff = child->stopRequests.load(); };
        Client::HostServiceTimePoint firstForce{};
        child->onForce = [&] { if (firstForce == Client::HostServiceTimePoint{}) firstForce = fixture.clock.now(); };
        D6R_REQUIRE(supervisor.endSession());
        D6R_REQUIRE(!supervisor.endSession());
        requireState(supervisor, Client::HostServiceState::NoService);
        D6R_REQUIRE_EQ(std::size_t(1), fixture.handoffs.size());
        D6R_REQUIRE_EQ(Client::HostServiceTimePoint{} + 22900ms, firstForce);
        D6R_REQUIRE_EQ(0u, stopsAtHandoff);
    }
}

D6R_TEST_CASE("HSL-AC-012 missing downstream sink still records exactly one observable intentional end handoff") {
    Fixture fixture;
    auto dependencies = fixture.dependencies();
    dependencies.intentionalEndHandoff = {};
    Client::HostServiceSupervisor supervisor(std::move(dependencies));
    D6R_REQUIRE(supervisor.start(fixture.config()));
    makeReady(fixture, supervisor);
    D6R_REQUIRE(supervisor.endSession());
    D6R_REQUIRE(!supervisor.endSession());
    requireState(supervisor, Client::HostServiceState::NoService);
    const auto result = supervisor.snapshot();
    D6R_REQUIRE(result.intentionalEndHandoffEmitted);
    D6R_REQUIRE_EQ(Client::HostServiceStopReason::IntentionalHostEnd, result.stopReason);
    D6R_REQUIRE(fixture.handoffs.empty());
}

D6R_TEST_CASE("HSL-AC-012 blocking reentrant handoff cannot block cleanup duplicate or extend its deadline") {
    Fixture fixture;
    fixture.clock.set(40s);
    auto dependencies = fixture.dependencies();
    std::mutex handoffMutex;
    std::condition_variable handoffChanged;
    bool entered = false;
    bool release = false;
    bool reentrantEndAccepted = true;
    Client::HostServiceState reentrantState = Client::HostServiceState::NoService;
    Client::HostServiceSupervisor *supervisorAddress = nullptr;
    dependencies.intentionalEndHandoff = [&](const char *reason) {
        fixture.handoffs.emplace_back(reason ? reason : "");
        reentrantEndAccepted = supervisorAddress->endSession();
        reentrantState = supervisorAddress->snapshot().state;
        std::unique_lock<std::mutex> lock(handoffMutex);
        entered = true;
        handoffChanged.notify_all();
        handoffChanged.wait(lock, [&] { return release; });
    };
    Client::HostServiceSupervisor supervisor(std::move(dependencies));
    supervisorAddress = &supervisor;
    D6R_REQUIRE(supervisor.start(fixture.config()));
    makeReady(fixture, supervisor, 40s + 1ns);
    auto child = fixture.child();
    child->cooperative = false;
    child->onWait = [&] { fixture.clock.advance(10ms); };
    Client::HostServiceTimePoint firstForce{};
    child->onForce = [&] { if (firstForce == Client::HostServiceTimePoint{}) firstForce = fixture.clock.now(); };
    bool accepted = false;
    std::thread caller([&] { accepted = supervisor.endSession(); });
    {
        std::unique_lock<std::mutex> lock(handoffMutex);
        const bool callbackEntered = handoffChanged.wait_for(lock, 2s, [&] { return entered; });
        if (!callbackEntered) {
            release = true;
            handoffChanged.notify_all();
            caller.join();
            D6R_REQUIRE(callbackEntered);
        }
    }
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
    D6R_REQUIRE_EQ(Client::HostServiceTimePoint{} + 42900ms, firstForce);
    D6R_REQUIRE_EQ(std::size_t(1), fixture.handoffs.size());
    D6R_REQUIRE(!reentrantEndAccepted);
    D6R_REQUIRE(reentrantState == Client::HostServiceState::Stopping
                || reentrantState == Client::HostServiceState::NoService);
    {
        std::lock_guard<std::mutex> lock(handoffMutex);
        release = true;
    }
    handoffChanged.notify_all();
    caller.join();
    D6R_REQUIRE(accepted);
}

D6R_TEST_CASE("HSL-AC-012 delayed failing handoff retains the already accepted cleanup deadline") {
    Fixture fixture;
    fixture.clock.set(20s);
    fixture.throwHandoff = true;
    fixture.onHandoff = [&] { fixture.clock.advance(4s); };
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    makeReady(fixture, supervisor, 20s + 1ns);
    auto child = fixture.child();
    child->cooperative = false;
    Client::HostServiceTimePoint firstForce{};
    child->onForce = [&] { if (firstForce == Client::HostServiceTimePoint{}) firstForce = fixture.clock.now(); };
    D6R_REQUIRE(supervisor.endSession());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE_EQ(Client::HostServiceTimePoint{} + 24s, firstForce);
    D6R_REQUIRE_EQ(std::size_t(1), fixture.handoffs.size());
}

D6R_TEST_CASE("HSL-AC-012 handoff is never emitted for cancel dismissal failure or application exit") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    D6R_REQUIRE(!supervisor.endSession());
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE(fixture.handoffs.empty());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    fixture.child(1)->status(Network::HostServiceStatusCode::StartFailed, fixture.clock.now() + 1ns);
    requireState(supervisor, Client::HostServiceState::StartupFailed);
    D6R_REQUIRE(supervisor.dismissFailure());
    D6R_REQUIRE(fixture.handoffs.empty());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    supervisor.applicationExit();
    requireCleanup(supervisor);
    D6R_REQUIRE(fixture.handoffs.empty());
}

D6R_TEST_CASE("HSL-AC-012 timeout and unexpected service failure never emit intentional handoff") {
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        fixture.clock.set(10s);
        fixture.child()->changed.notify_all();
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartupTimedOut, supervisor.snapshot().outcome);
        D6R_REQUIRE(!supervisor.snapshot().intentionalEndHandoffEmitted);
        D6R_REQUIRE(fixture.handoffs.empty());
    }
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        makeReady(fixture, supervisor);
        finishByExit(fixture, supervisor, Client::HostServiceState::SessionFailed);
        D6R_REQUIRE(!supervisor.snapshot().intentionalEndHandoffEmitted);
        D6R_REQUIRE(fixture.handoffs.empty());
    }
}

D6R_TEST_CASE("HSL-AC-014 cooperative cleanup is idempotent and keeps first stop selection") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    auto child = fixture.child();
    D6R_REQUIRE(supervisor.cancelStartup());
    D6R_REQUIRE(!supervisor.cancelStartup());
    supervisor.applicationExit();
    supervisor.applicationExit();
    requireState(supervisor, Client::HostServiceState::ApplicationExit);
    requireCleanup(supervisor);
    D6R_REQUIRE_EQ(1u, child->stopRequests.load());
    D6R_REQUIRE_EQ(0u, child->forces.load());
}

D6R_TEST_CASE("HSL-AC-014 hung cleanup force terminates before the three-second boundary") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    auto child = fixture.child();
    child->cooperative = false;
    child->onWait = [&] { fixture.clock.advance(10ms); };
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE_EQ(1u, child->stopRequests.load());
    D6R_REQUIRE_EQ(1u, child->forces.load());
    D6R_REQUIRE(fixture.clock.now() <= Client::HostServiceTimePoint{} + 3s);
}

D6R_TEST_CASE("HSL-AC-014 cleanup deadline starts when the stop action is accepted") {
    Fixture fixture;
    fixture.clock.set(100s);
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    fixture.clock.set(150s);
    auto child = fixture.child();
    child->cooperative = false;
    child->onWait = [&] { fixture.clock.advance(10ms); };
    Client::HostServiceTimePoint firstForce{};
    child->onForce = [&] { if (firstForce == Client::HostServiceTimePoint{}) firstForce = fixture.clock.now(); };
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE_EQ(Client::HostServiceTimePoint{} + 152900ms, firstForce);
}

D6R_TEST_CASE("HSL-AC-014 cooperative direct-child exit with a live descendant still forces and confirms tree zero") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    auto child = fixture.child();
    child->treeGone.store(false);
    child->onWait = [&] { fixture.clock.advance(10ms); };
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE(child->exited.load());
    D6R_REQUIRE(child->treeGone.load());
    D6R_REQUIRE_EQ(1u, child->forces.load());
}

D6R_TEST_CASE("HSL-AC-014 cleanup can become truthful exactly at three seconds") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    auto child = fixture.child();
    child->cooperative = false;
    child->forceWorks = false;
    child->onWait = [&] { fixture.clock.advance(10ms); };
    child->onForce = [&] {
        if (fixture.clock.now() >= Client::HostServiceTimePoint{} + 3s) {
            child->exited.store(true);
            child->treeGone.store(true);
        }
    };
    D6R_REQUIRE(supervisor.cancelStartup());
    requireState(supervisor, Client::HostServiceState::NoService);
    D6R_REQUIRE_EQ(Client::HostServiceTimePoint{} + 3s, fixture.clock.now());
}

D6R_TEST_CASE("HSL-AC-014 cleanup is never reported complete when force termination fails") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    auto child = fixture.child();
    child->cooperative = false;
    child->forceWorks = false;
    child->onWait = [&] { fixture.clock.advance(10ms); };
    D6R_REQUIRE(supervisor.cancelStartup());
    const auto wallDeadline = std::chrono::steady_clock::now() + 2s;
    while (fixture.clock.now() < Client::HostServiceTimePoint{} + 3s
           && std::chrono::steady_clock::now() < wallDeadline)
        std::this_thread::sleep_for(1ms);
    std::this_thread::sleep_for(10ms);
    const bool truthful = child->exited.load() || !supervisor.snapshot().cleanupComplete;
    const auto pending = supervisor.snapshot();
    D6R_REQUIRE_EQ(Client::HostServiceState::Stopping, pending.state);
    D6R_REQUIRE(!pending.retryAllowed);
    D6R_REQUIRE(!pending.failureDismissalAllowed);
    D6R_REQUIRE(!supervisor.start(fixture.config()));
    child->exited.store(true);
    child->treeGone.store(true);
    child->changed.notify_all();
    D6R_REQUIRE(truthful);
    requireState(supervisor, Client::HostServiceState::NoService);
}

D6R_TEST_CASE("HSL-AC-014 monitor-thread creation failure cleans its owned child before enabling Retry") {
    Fixture fixture;
    fixture.failMonitorLaunch = true;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    requireState(supervisor, Client::HostServiceState::StartupFailed);
    const auto result = supervisor.snapshot();
    D6R_REQUIRE(result.cleanupComplete);
    D6R_REQUIRE(result.retryAllowed);
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartFailed, result.outcome);
    D6R_REQUIRE_EQ(1u, fixture.child()->stopRequests.load());
}

D6R_TEST_CASE("HSL-AC-010 startup and session failures expose retained setup and exact dismissal") {
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        const auto original = fixture.config();
        D6R_REQUIRE(supervisor.start(original));
        fixture.child()->status(Network::HostServiceStatusCode::PortUnavailable,
                                Client::HostServiceTimePoint{} + 1ns);
        requireState(supervisor, Client::HostServiceState::StartupFailed);
        Client::HostServiceStartConfig retained;
        D6R_REQUIRE(supervisor.retainedSetup(retained));
        D6R_REQUIRE_EQ(original.endpoint.host, retained.endpoint.host);
        D6R_REQUIRE_EQ(original.endpoint.port, retained.endpoint.port);
        D6R_REQUIRE(supervisor.snapshot().failureDismissalAllowed);
        D6R_REQUIRE(supervisor.dismissFailure());
        D6R_REQUIRE_EQ(Client::HostServiceState::NoService, supervisor.snapshot().state);
        D6R_REQUIRE(!supervisor.retry());
    }
    {
        Fixture fixture;
        Client::HostServiceSupervisor supervisor(fixture.dependencies());
        D6R_REQUIRE(supervisor.start(fixture.config()));
        makeReady(fixture, supervisor);
        finishByExit(fixture, supervisor, Client::HostServiceState::SessionFailed);
        D6R_REQUIRE(supervisor.snapshot().failureDismissalAllowed);
        D6R_REQUIRE(!supervisor.snapshot().retryAllowed);
        D6R_REQUIRE(supervisor.dismissFailure());
        D6R_REQUIRE_EQ(Client::HostServiceState::NoService, supervisor.snapshot().state);
    }
}

D6R_TEST_CASE("HSL-AC-016 IPC messages are fixed endian and reject partial trailing unknown corrupt and reserved bytes") {
    const auto statusMessage = Network::encodeHostServiceStatus(Network::HostServiceStatusCode::Ready,
                                                                 0x0102030405060708ULL);
    const std::array<std::uint8_t, 16> expected{{
        0x44, 0x36, 0x48, 0x53, 0x01, 0x04, 0x00, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    }};
    D6R_REQUIRE_EQ(expected, statusMessage);
    D6R_REQUIRE_EQ(std::string("4"), Duel6::Test::toString(Network::HostServiceStatusCode::Ready));
    D6R_REQUIRE_EQ(std::string("[0x44, 0x36, 0x48, 0x53, 0x01, 0x04, 0x00, 0x00, "
                               "0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]"),
                   Duel6::Test::toString(expected));
    Network::HostServiceStatusCode status{};
    std::uint64_t timestamp = 0;
    D6R_REQUIRE(Network::decodeHostServiceStatus(statusMessage.data(), statusMessage.size(), status, timestamp));
    D6R_REQUIRE_EQ(Network::HostServiceStatusCode::Ready, status);
    D6R_REQUIRE_EQ(0x0102030405060708ULL, timestamp);
    D6R_REQUIRE(!Network::decodeHostServiceStatus(statusMessage.data(), statusMessage.size() - 1, status, timestamp));
    std::array<std::uint8_t, 17> trailing{};
    std::copy(statusMessage.begin(), statusMessage.end(), trailing.begin());
    D6R_REQUIRE(!Network::decodeHostServiceStatus(trailing.data(), trailing.size(), status, timestamp));
    for (const std::size_t index: {0u, 4u, 5u, 6u, 7u}) {
        auto invalid = statusMessage;
        invalid[index] ^= 0x7f;
        D6R_REQUIRE(!Network::decodeHostServiceStatus(invalid.data(), invalid.size(), status, timestamp));
    }
    D6R_REQUIRE(!Network::decodeHostServiceStatus(nullptr, statusMessage.size(), status, timestamp));

    const auto command = Network::encodeHostServiceCommand(Network::HostServiceCommandCode::Stop);
    const std::array<std::uint8_t, 8> expectedCommand{{0x44, 0x36, 0x48, 0x53, 0x01, 0x01, 0x00, 0x00}};
    D6R_REQUIRE_EQ(expectedCommand, command);
    Network::HostServiceCommandCode decoded{};
    D6R_REQUIRE(Network::decodeHostServiceCommand(command.data(), command.size(), decoded));
    D6R_REQUIRE(!Network::decodeHostServiceCommand(command.data(), command.size() - 1, decoded));
    auto unknown = command;
    unknown[5] = 0xff;
    D6R_REQUIRE(!Network::decodeHostServiceCommand(unknown.data(), unknown.size(), decoded));
}

D6R_TEST_CASE("HSL-AC-016 startup validation requires absolute bounded non-secret configuration") {
    Fixture fixture;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    auto config = fixture.config();
    config.serverExecutable = "duel6r-server";
    D6R_REQUIRE(!supervisor.start(config));
    config = fixture.config(); config.endpoint.port = 0;
    D6R_REQUIRE(!supervisor.start(config));
    config = fixture.config(); config.localPlayers = 0;
    D6R_REQUIRE(!supervisor.start(config));
    config = fixture.config(); config.localPlayers = 16;
    D6R_REQUIRE(!supervisor.start(config));
    config = fixture.config(); config.enabledGameplayScripts.assign(17, "safe.lua");
    D6R_REQUIRE(!supervisor.start(config));
    config = fixture.config(); config.endpoint.host.assign(Network::MaxProtocolStringBytes + 1, 'a');
    D6R_REQUIRE(!supervisor.start(config));
    D6R_REQUIRE_EQ(std::size_t(0), fixture.launchCount());
}

D6R_TEST_CASE("HSL-AC-008 immediate process creation failure has generic exact retryable outcome") {
    Fixture fixture;
    fixture.failLaunch = true;
    Client::HostServiceSupervisor supervisor(fixture.dependencies());
    D6R_REQUIRE(supervisor.start(fixture.config()));
    const auto result = supervisor.snapshot();
    D6R_REQUIRE_EQ(Client::HostServiceState::StartupFailed, result.state);
    D6R_REQUIRE_EQ(Client::HostServiceOutcome::StartFailed, result.outcome);
    D6R_REQUIRE(result.cleanupComplete);
    D6R_REQUIRE(result.retryAllowed);
}
