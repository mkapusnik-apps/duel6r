#ifdef __linux__
#include <algorithm>
#include <csignal>
#include <fstream>
#include <sys/wait.h>
#include "tests/HostDiagnosticFixture.h"
#include "source/client/HostServiceSupervisor.h"

namespace {
using namespace Duel6;
using namespace std::chrono_literals;
struct DiagnosticEnvironment {
    std::string previous;
    bool present;
    explicit DiagnosticEnvironment(const char *value) {
        const char *old = std::getenv(Network::HostServiceDiagnostics::Environment);
        present = old != nullptr;
        if (old) previous = old;
        if (value) setenv(Network::HostServiceDiagnostics::Environment, value, 1);
        else unsetenv(Network::HostServiceDiagnostics::Environment);
    }
    ~DiagnosticEnvironment() {
        if (present) setenv(Network::HostServiceDiagnostics::Environment, previous.c_str(), 1);
        else unsetenv(Network::HostServiceDiagnostics::Environment);
    }
};
Client::HostServiceStartConfig childConfig(const Test::HostDiagnosticFixture &fixture, const std::string &mode) {
    Client::HostServiceStartConfig config;
    config.serverExecutable = std::filesystem::absolute(D6R_HOST_SERVICE_TEST_CHILD).string();
    config.endpoint = {"127.0.0.1", 34127};
    config.resourcePath = (fixture.directory / mode).string();
    config.enabledGameplayScripts = {(fixture.directory / "release").string()};
    return config;
}
}

D6R_TEST_CASE("Host diagnostics retain native cause and selected reason before child reset") {
    using Event = Network::HostDiagnosticEvent;
    for (const std::string mode : {"diagnostic-end", "diagnostic-failure", "diagnostic-signal"}) {
        Test::HostDiagnosticFixture fixture;
        DiagnosticEnvironment env(fixture.fifo.c_str());
        Client::HostServiceSupervisor supervisor;
        D6R_REQUIRE(supervisor.start(childConfig(fixture, mode)));
        D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::Active, 3s));
        if (mode == "diagnostic-end") {
            D6R_REQUIRE(supervisor.endSession());
            D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::NoService, 3s));
        } else {
            { std::ofstream release(fixture.directory / "release"); release << "go"; }
            D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::SessionFailed, 3s));
        }
        D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
        const auto records = fixture.drain();
        const auto find = [&](Event event, bool parent = true) {
            return std::find_if(records.begin(), records.end(), [&](const auto &r) {
                return r.event == static_cast<int>(event) && (r.pid == getpid()) == parent;
            });
        };
        const auto started = find(Event::ChildStarted), native = find(Event::NativeExit);
        const auto selected = find(Event::StopSelected), reset = find(Event::BeforeReset);
        D6R_REQUIRE(started != records.end() && native != records.end()
                    && selected != records.end() && reset != records.end());
        D6R_REQUIRE_EQ(started->a, native->a);
        D6R_REQUIRE_EQ(mode == "diagnostic-signal" ? CLD_KILLED : CLD_EXITED, native->b);
        D6R_REQUIRE_EQ(mode == "diagnostic-signal" ? SIGUSR1 : mode == "diagnostic-failure" ? 76 : 0, native->c);
        D6R_REQUIRE(native->seq < reset->seq && selected->seq < reset->seq);
        D6R_REQUIRE_EQ(static_cast<int>(mode == "diagnostic-end"
                ? Client::HostServiceStopReason::IntentionalHostEnd : Client::HostServiceStopReason::SessionFailure), selected->a);
        D6R_REQUIRE_EQ(static_cast<int>(mode == "diagnostic-end"
                ? Client::HostServiceOutcome::None : Client::HostServiceOutcome::StoppedUnexpectedly), selected->b);
        const auto request = find(mode == "diagnostic-end" ? Event::EndRequested : Event::StopRequested);
        D6R_REQUIRE(request != records.end() && selected->seq < request->seq);
        if (mode == "diagnostic-end") D6R_REQUIRE(request->seq < native->seq);
        else D6R_REQUIRE(native->seq < selected->seq);
        D6R_REQUIRE(find(Event::ForceRequested) == records.end());
        D6R_REQUIRE(find(Event::BackendBegin, false) != records.end());
        if (mode == "diagnostic-failure") {
            const auto failure = find(Event::BackendFailure, false);
            D6R_REQUIRE(failure != records.end());
            D6R_REQUIRE_EQ(static_cast<int>(Network::HostDiagnosticFailure::CanonicalValidationRejected), failure->a);
            D6R_REQUIRE(failure->ns <= native->ns);
        }
        if (mode == "diagnostic-end") D6R_REQUIRE(find(Event::BackendOutcome, false) != records.end());
        for (const auto &record : records) D6R_REQUIRE_EQ(0u, record.dropped);
        D6R_REQUIRE(supervisor.snapshot().cleanupComplete);
        D6R_REQUIRE(fixture.drain().empty());
    }
}

D6R_TEST_CASE("Host diagnostics default off and invalid destinations leave lifecycle unchanged") {
    Test::HostDiagnosticFixture fixture;
    for (const char *value : {static_cast<const char *>(nullptr), "/does-not-exist/trace.fifo"}) {
        DiagnosticEnvironment env(value);
        D6R_REQUIRE(!Network::HostServiceDiagnostics::fromEnvironment());
        Client::HostServiceSupervisor supervisor;
        D6R_REQUIRE(supervisor.start(childConfig(fixture, "diagnostic-end")));
        D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::Active, 3s));
        D6R_REQUIRE(supervisor.endSession());
        D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::NoService, 3s));
        D6R_REQUIRE(fixture.drain().empty());
    }
    const auto regular = fixture.directory / "regular";
    { std::ofstream file(regular); file << "unchanged"; }
    D6R_REQUIRE(!Network::HostServiceDiagnostics::open(regular.string()));
    std::ifstream input(regular); std::string text; input >> text;
    D6R_REQUIRE_EQ(std::string("unchanged"), text);
    const auto link = fixture.directory / "link";
    std::filesystem::create_symlink(fixture.fifo, link);
    D6R_REQUIRE(!Network::HostServiceDiagnostics::open(link.string()));
    chmod(fixture.fifo.c_str(), 0666);
    D6R_REQUIRE(!Network::HostServiceDiagnostics::open(fixture.fifo.string()));
}

D6R_TEST_CASE("Host diagnostics distinguish owner selected forced cleanup from unexpected signal") {
    Test::HostDiagnosticFixture fixture;
    DiagnosticEnvironment env(fixture.fifo.c_str());
    Client::HostServiceSupervisor supervisor;
    D6R_REQUIRE(supervisor.start(childConfig(fixture, "diagnostic-force")));
    D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::Active, 3s));
    D6R_REQUIRE(supervisor.endSession());
    D6R_REQUIRE(supervisor.waitForState(Client::HostServiceState::NoService, 5s));
    unsigned selected = 0, forced = 0, exited = 0, reset = 0;
    for (const auto &r : fixture.drain()) {
        if (r.pid != getpid()) continue;
        switch (static_cast<Network::HostDiagnosticEvent>(r.event)) {
            case Network::HostDiagnosticEvent::StopSelected:
                selected = r.seq;
                D6R_REQUIRE_EQ(static_cast<int>(Client::HostServiceStopReason::IntentionalHostEnd), r.a);
                break;
            case Network::HostDiagnosticEvent::ForceRequested: forced = r.seq; break;
            case Network::HostDiagnosticEvent::NativeExit:
                exited = r.seq;
                D6R_REQUIRE_EQ(CLD_KILLED, r.b); D6R_REQUIRE_EQ(SIGKILL, r.c);
                break;
            case Network::HostDiagnosticEvent::BeforeReset: reset = r.seq; break;
            default: break;
        }
    }
    D6R_REQUIRE(selected > 0 && selected < forced && forced < exited && exited < reset);
}

D6R_TEST_CASE("Host diagnostics are bounded and survive absent or backpressured collector") {
    Test::HostDiagnosticFixture fixture;
    auto diagnostics = Network::HostServiceDiagnostics::open(fixture.fifo.string());
    D6R_REQUIRE(diagnostics != nullptr);
    D6R_REQUIRE(fcntl(fixture.reader, F_SETPIPE_SZ, 4096) >= 0);
    close(fixture.reader); fixture.reader = -1;
    for (unsigned i = 0; i < 100; ++i) {
        errno = EDOM;
        diagnostics->record(Network::HostDiagnosticEvent::Begin);
        D6R_REQUIRE_EQ(EDOM, errno);
    }
    fixture.reader = ::open(fixture.fifo.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    D6R_REQUIRE(fixture.reader >= 0);
    const auto initial = fixture.drain();
    D6R_REQUIRE(!initial.empty() && initial.size() < 100);
    diagnostics->record(Network::HostDiagnosticEvent::BeforeReset);
    const auto resumed = fixture.drain();
    D6R_REQUIRE_EQ(std::size_t(1), resumed.size());
    D6R_REQUIRE_EQ(101u, resumed.front().seq);
    D6R_REQUIRE(resumed.front().dropped > 0);
    for (unsigned i = 0; i < 1000; ++i) diagnostics->record(Network::HostDiagnosticEvent::Begin);
    for (const auto &record : fixture.drain()) D6R_REQUIRE(record.seq <= Network::HostServiceDiagnostics::MaximumRecords);
    diagnostics->record(Network::HostDiagnosticEvent::Begin);
    D6R_REQUIRE(fixture.drain().empty());
}
#endif
