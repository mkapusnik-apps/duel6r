#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "tests/TestHarness.h"
#include "source/client/ConnectionPlan.h"
#include "source/network/NetworkTrustPolicy.h"
#include "source/network/SessionTransport.h"

namespace {
    using namespace Duel6;
    using namespace Duel6::Network::Trust;
    using namespace std::chrono_literals;

    struct ManualClock {
        TimePoint value{};
        Clock clock() { return [this] { return value; }; }
        void advance(std::chrono::milliseconds amount) { value += amount; }
        void set(TimePoint next) { value = next; }
    };

    ReconnectCredential credential(std::uint8_t seed) {
        ReconnectCredential result;
        for (std::size_t index = 0; index < result.bytes.size(); ++index)
            result.bytes[index] = static_cast<std::uint8_t>(seed + index);
        return result;
    }

    bool allZero(const ReconnectCredential &value) {
        return std::all_of(value.bytes.begin(), value.bytes.end(), [](std::uint8_t byte) { return byte == 0; });
    }
}

D6R_TEST_CASE("network trust constants retain exact approved limits") {
    D6R_REQUIRE_EQ(262144u, MaxAdmissionPayloadBytes);
    D6R_REQUIRE_EQ(4096u, MaxProperties);
    D6R_REQUIRE_EQ(128u, MaxKeyBytes);
    D6R_REQUIRE_EQ(4096u, MaxStringBytes);
    D6R_REQUIRE_EQ(253u, MaxHostnameBytes);
    D6R_REQUIRE_EQ(64u, MaxParticipantNameBytes);
    D6R_REQUIRE_EQ(256u, MaxReasonBytes);
    D6R_REQUIRE_EQ(256u, MaxCollectionEntries);
    D6R_REQUIRE_EQ(256u, MaxManifestEntries);
    D6R_REQUIRE_EQ(15u, MaxParticipants);
    D6R_REQUIRE_EQ(64u, MaxResolverIpv4Addresses);
    D6R_REQUIRE_EQ(8u, MaxPendingAdmissions);
    D6R_REQUIRE_EQ(4u, MaxPendingAdmissionsPerSource);
    D6R_REQUIRE_EQ(20u, MaxAdmissionAttemptsPerSourcePerMinute);
    D6R_REQUIRE_EQ(4u, AdmissionAttemptBurst);
    D6R_REQUIRE_EQ(32u * 1024u * 1024u, MaxAggregateQueuedBytes);
    D6R_REQUIRE_EQ(512u * 1024u, GuestToHostBytesPerSecond);
    D6R_REQUIRE_EQ(1024u * 1024u, GuestToHostBurstBytes);
    D6R_REQUIRE_EQ(4u * 1024u * 1024u, HostToGuestBytesPerSecond);
    D6R_REQUIRE_EQ(4u * 1024u * 1024u, HostToGuestBurstBytes);
    D6R_REQUIRE_EQ(30u, NonInputActionsPerSecond);
    D6R_REQUIRE_EQ(60u, NonInputActionBurst);
    D6R_REQUIRE_EQ(120u, InputsPerOwnedSlotPerSecond);
    D6R_REQUIRE_EQ(1800u, GlobalAcceptedInputsPerSecond);
    D6R_REQUIRE(FirstAdmissionRequestDeadline == 3s);
    D6R_REQUIRE(ReconnectCredentialLifetime == 30s);
    D6R_REQUIRE_EQ(16u, ReconnectCredentialBytes);
    D6R_REQUIRE_EQ(4u, MaxReconnectCredentialGenerationAttempts);
}

D6R_TEST_CASE("listener and guest endpoints fail closed outside loopback and RFC1918") {
    using Scope = EndpointScope;
    const std::vector<std::pair<std::string, Scope>> corpus{
        {"127.0.0.1", Scope::Loopback}, {"127.0.0.255", Scope::Loopback}, {"127.255.255.254", Scope::Loopback},
        {"10.0.0.1", Scope::PrivateLan}, {"10.0.0.255", Scope::PrivateLan}, {"10.255.255.254", Scope::PrivateLan},
        {"172.16.0.1", Scope::PrivateLan}, {"172.16.0.255", Scope::PrivateLan}, {"172.31.255.254", Scope::PrivateLan},
        {"192.168.0.1", Scope::PrivateLan}, {"192.168.0.255", Scope::PrivateLan}, {"192.168.255.254", Scope::PrivateLan},
        {"0.0.0.0", Scope::Unsupported}, {"8.8.8.8", Scope::Unsupported},
        {"169.254.1.1", Scope::Unsupported}, {"224.0.0.1", Scope::Unsupported},
        {"239.255.255.255", Scope::Unsupported}, {"255.255.255.255", Scope::Unsupported},
        {"172.15.0.1", Scope::Unsupported}, {"172.32.0.1", Scope::Unsupported},
        {"", Scope::Invalid}, {"1.2.3", Scope::Invalid}, {"1.2.3.4.5", Scope::Invalid},
        {"01.2.3.4", Scope::Invalid}, {"256.2.3.4", Scope::Invalid}, {"1..3.4", Scope::Invalid},
        {"1.2.3.4x", Scope::Invalid}, {"-1.2.3.4", Scope::Invalid}};
    for (const auto &entry: corpus) D6R_REQUIRE(classifyIpv4Literal(entry.first) == entry.second);

    D6R_REQUIRE_EQ("Network session is limited to this machine. No authentication or encryption is used.",
                   std::string(LoopbackExposureCopy));
    D6R_REQUIRE_EQ("Network session is limited to a private LAN. No authentication or encryption is used. Do not expose this port to the Internet.",
                   std::string(PrivateLanExposureCopy));
    D6R_REQUIRE_EQ("Network session cannot use a public or wildcard address. Use loopback or a private LAN address.",
                   std::string(UnsupportedAddressCopy));
}

D6R_TEST_CASE("local bind helper accepts assigned trusted interfaces and rejects unassigned values") {
    D6R_REQUIRE(isLocalIpv4AddressAssigned({127, 0, 0, 1}));
    D6R_REQUIRE(!isLocalIpv4AddressAssigned({0, 0, 0, 0}));
    D6R_REQUIRE(!isLocalIpv4AddressAssigned({255, 255, 255, 255}));
    D6R_REQUIRE(!isLocalIpv4AddressAssigned({10, 0, 0, 255}));
    D6R_REQUIRE(!isLocalIpv4AddressAssigned({172, 16, 0, 255}));
    D6R_REQUIRE(!isLocalIpv4AddressAssigned({192, 168, 0, 255}));
}

D6R_TEST_CASE("hostname syntax and resolver policy enforce exact boundaries before connect or bind") {
    const std::string label63(63, 'a');
    const std::string hostname253 = label63 + '.' + label63 + '.' + label63 + '.' + std::string(61, 'a');
    D6R_REQUIRE_EQ(253u, hostname253.size());
    D6R_REQUIRE(validHostname(hostname253));
    D6R_REQUIRE(!validHostname(hostname253 + "a"));
    for (const std::string invalid: {"", ".host", "host.", "a..b", "-host", "host-", "bad_name", "bad host", "h\xC3\xA9st"})
        D6R_REQUIRE(!validHostname(invalid));

    std::atomic<int> resolverCalls{0};
    Network::SessionTransportDependencies rejectedDependencies;
    rejectedDependencies.enforceNetworkSessionPolicy = true;
    rejectedDependencies.resolve = [&](const auto &, auto, auto, const auto &) {
        ++resolverCalls;
        return Network::ResolveOutcome{};
    };
    for (const std::string host: {"0.0.0.0", "8.8.8.8", "224.0.0.1", "169.254.1.1", "bad host"}) {
        Network::TcpListener listener(1, rejectedDependencies);
        D6R_REQUIRE(listener.start({host, 26660}));
        D6R_REQUIRE(!listener.waitForReady(1s));
        D6R_REQUIRE(listener.failure() == Network::TransportFailure::InvalidEndpoint);
        listener.shutdown();
    }
    D6R_REQUIRE_EQ(0, resolverCalls.load());

    std::atomic<int> connectorCalls{0};
    Network::SessionTransportDependencies clientDependencies;
    clientDependencies.enforceNetworkSessionPolicy = true;
    clientDependencies.resolve = [](const auto &, std::uint16_t port, auto, const auto &) {
        std::vector<Network::ResolvedIpv4Endpoint> results(64, {{10, 1, 2, 3}, port});
        results[0] = {{8, 8, 8, 8}, port};
        results[1] = {{127, 0, 0, 1}, port};
        return Network::ResolveOutcome{Network::ResolveStatus::Resolved, results};
    };
    clientDependencies.connect = [&](const auto &results, auto, const auto &) {
        ++connectorCalls;
        D6R_REQUIRE_EQ(63u, results.size());
        D6R_REQUIRE(std::all_of(results.begin(), results.end(), [](const auto &endpoint) {
            const auto scope = classifyIpv4(endpoint.address);
            return scope == EndpointScope::Loopback || scope == EndpointScope::PrivateLan;
        }));
        return Network::ConnectOutcome{Network::ConnectStatus::ConnectionRefused, -1};
    };
    Network::TcpClient boundedClient(clientDependencies);
    D6R_REQUIRE(boundedClient.start({"private.example", 26660}));
    D6R_REQUIRE(!boundedClient.waitForConnected(1s));
    D6R_REQUIRE_EQ(1, connectorCalls.load());

    clientDependencies.resolve = [](const auto &, std::uint16_t port, auto, const auto &) {
        return Network::ResolveOutcome{Network::ResolveStatus::Resolved,
                std::vector<Network::ResolvedIpv4Endpoint>(65, {{10, 1, 2, 3}, port})};
    };
    Network::TcpClient oversizedResolution(clientDependencies);
    D6R_REQUIRE(oversizedResolution.start({"private.example", 26660}));
    D6R_REQUIRE(!oversizedResolution.waitForConnected(1s));
    D6R_REQUIRE(oversizedResolution.failure() == Network::TransportFailure::ResolveFailed);
    D6R_REQUIRE_EQ(1, connectorCalls.load());
}

D6R_TEST_CASE("local play planning stays loopback only while remote endpoint is copied exactly") {
    Network::ClientConnectionConfig local;
    local.mode = Network::ConnectionMode::LocalGame;
    local.localEndpoint = {"0.0.0.0", 27660};
    const auto localPlan = Client::createConnectionPlan(local);
    D6R_REQUIRE_EQ("127.0.0.1", localPlan.endpoint.host);
    D6R_REQUIRE_EQ("--host=127.0.0.1", localPlan.localServerArguments.at(2));

    Network::ClientConnectionConfig remote;
    remote.mode = Network::ConnectionMode::RemoteServer;
    remote.remoteEndpoint = {"Private-Host.example", 27661};
    const auto remotePlan = Client::createConnectionPlan(remote);
    D6R_REQUIRE_EQ(remote.remoteEndpoint.host, remotePlan.endpoint.host);
    D6R_REQUIRE_EQ(remote.remoteEndpoint.port, remotePlan.endpoint.port);
    D6R_REQUIRE(!remotePlan.launchesLocalServer);
}

D6R_TEST_CASE("validation APIs accept exact limits and reject one above") {
    D6R_REQUIRE(validPropertyCount(MaxProperties));
    D6R_REQUIRE(!validPropertyCount(MaxProperties + 1));
    D6R_REQUIRE(validPropertyKey("k" + std::string(MaxKeyBytes - 1, 'x')));
    D6R_REQUIRE(!validPropertyKey("k" + std::string(MaxKeyBytes, 'x')));
    for (const std::string &key: std::vector<std::string>{"", ".leading", "-leading", "under_score", "dot.name",
                                  "dash-name", "white space", std::string("nul\0key", 7), "nonascii-\xC3\xA9"}) {
        const bool expected = key == "under_score" || key == "dot.name" || key == "dash-name";
        D6R_REQUIRE(validPropertyKey(key) == expected);
    }

    D6R_REQUIRE(validGeneralString(std::string(MaxStringBytes, 's')));
    D6R_REQUIRE(!validGeneralString(std::string(MaxStringBytes + 1, 's')));
    D6R_REQUIRE(validGeneralString(std::string("embedded\0value", 14)));

    D6R_REQUIRE(validAsciiReason(std::string(MaxReasonBytes, 'R')));
    D6R_REQUIRE(!validAsciiReason(std::string(MaxReasonBytes + 1, 'R')));
    for (const std::string &reason: {std::string("line\nbreak"), std::string("nul\0byte", 8), std::string("tab\tvalue")})
        D6R_REQUIRE(!validAsciiReason(reason));

    D6R_REQUIRE(validParticipantName(std::string(MaxParticipantNameBytes, 'n')));
    D6R_REQUIRE(!validParticipantName(std::string(MaxParticipantNameBytes + 1, 'n')));
    D6R_REQUIRE(validParticipantName("Ren\xC3\xA9"));
    const std::vector<std::string> hostileNames{
        "", std::string("nul\0name", 8), "line\nbreak", "carriage\rreturn", "tab\tname",
        std::string("\xC0\xAF", 2), std::string("\xED\xA0\x80", 3),
        std::string("name\xE2\x80\xAE", 7), std::string("name\xE2\x81\xA6", 7),
        std::string("name\xE2\x80\xA8", 7)};
    for (const auto &name: hostileNames) D6R_REQUIRE(!validParticipantName(name));

    D6R_REQUIRE(validCollectionSize(MaxCollectionEntries));
    D6R_REQUIRE(!validCollectionSize(MaxCollectionEntries + 1));
    D6R_REQUIRE(validManifestEntryCount(MaxManifestEntries));
    D6R_REQUIRE(!validManifestEntryCount(MaxManifestEntries + 1));

    const std::string label63(63, 'h');
    const std::string hostname253 = label63 + '.' + label63 + '.' + label63 + '.' + std::string(61, 'h');
    D6R_REQUIRE(validHostname(hostname253));
    D6R_REQUIRE(!validHostname(hostname253 + 'h'));

    const std::string path240 = std::string(60, 'a') + '/' + std::string(59, 'b') + '/'
                                + std::string(59, 'c') + '/' + std::string(59, 'd');
    D6R_REQUIRE_EQ(MaxLogicalPathBytes, path240.size());
    D6R_REQUIRE(validLogicalPath(path240));
    D6R_REQUIRE(!validLogicalPath(std::string(60, 'a') + '/' + std::string(60, 'b') + '/'
                                  + std::string(59, 'c') + '/' + std::string(59, 'd')));
    std::string sixteenSegments = "a";
    for (std::size_t index = 1; index < MaxLogicalPathSegments; ++index) sixteenSegments += "/a";
    D6R_REQUIRE(validLogicalPath(sixteenSegments));
    D6R_REQUIRE(!validLogicalPath(sixteenSegments + "/a"));
    for (const std::string &path: std::vector<std::string>{"", "/absolute", "trailing/", "double//slash", "../escape",
                                   ".hidden", "segment/../escape", "white space", "back\\slash",
                                   std::string("nul\0path", 8), "nonascii-\xC3\xA9"})
        D6R_REQUIRE(!validLogicalPath(path));
}

D6R_TEST_CASE("bounded validators reject deterministic malformed and mutation corpus without crashing") {
    const std::vector<std::string> malformedUtf8{
        std::string("\x80", 1), std::string("\xBF", 1), std::string("\xC2", 1),
        std::string("\xE2\x82", 2), std::string("\xF0\x9F\x92", 3),
        std::string("\xC0\x80", 2), std::string("\xC1\xBF", 2),
        std::string("\xE0\x80\x80", 3), std::string("\xF0\x80\x80\x80", 4),
        std::string("\xED\xA0\x80", 3), std::string("\xED\xBF\xBF", 3),
        std::string("\xF4\x90\x80\x80", 4), std::string("\xF5\x80\x80\x80", 4),
        std::string("\xFF", 1), std::string("ok\xC2\x80", 4),
        std::string("ok\xD8\x9C", 4), std::string("ok\xE2\x80\x8E", 5),
        std::string("ok\xE2\x80\x8F", 5), std::string("ok\xE2\x80\xAA", 5),
        std::string("ok\xE2\x80\xAE", 5), std::string("ok\xE2\x81\xA6", 5),
        std::string("ok\xE2\x81\xA9", 5)};
    for (const auto &value: malformedUtf8) D6R_REQUIRE(!validParticipantName(value));

    for (unsigned byte = 0; byte <= 255; ++byte) {
        const std::string mutation(1, static_cast<char>(byte));
        if (byte < 0x20 || byte >= 0x7f) {
            D6R_REQUIRE(!validAsciiReason(mutation));
            D6R_REQUIRE(!validPropertyKey("k" + mutation));
            D6R_REQUIRE(!validLogicalPath("p" + mutation));
        }
    }
    for (std::size_t index = 0; index < 64; ++index) {
        std::string name(64, 'n');
        name[index] = index % 2 == 0 ? '\0' : '\n';
        D6R_REQUIRE(!validParticipantName(name));
    }

    const std::vector<std::string> malformedHosts{
        std::string(MaxHostnameBytes + 1, 'h'), std::string(64, 'a') + ".test", "a..b", ".a", "a.",
        "-a", "a-", "a_b", "a b", std::string("a\0b", 3), std::string("a\xFF", 2)};
    for (const auto &value: malformedHosts) {
        D6R_REQUIRE(!validHostname(value));
        D6R_REQUIRE(!validGuestEndpointName(value));
    }

    const std::vector<std::string> malformedPaths{
        std::string(MaxLogicalPathBytes + 1, 'p'), std::string(65, 'p'), std::string("p\0q", 3),
        "/p", "p/", "p//q", "../p", "p/../q", "p\\q", "p q", std::string("p\xFF", 2)};
    for (const auto &value: malformedPaths) D6R_REQUIRE(!validLogicalPath(value));

    D6R_REQUIRE(!validPropertyCount((std::numeric_limits<std::size_t>::max)()));
    D6R_REQUIRE(!validCollectionSize((std::numeric_limits<std::size_t>::max)()));
    D6R_REQUIRE(!validManifestEntryCount((std::numeric_limits<std::size_t>::max)()));
    D6R_REQUIRE(!validGeneralString(std::string(MaxStringBytes + 257, 's')));
    D6R_REQUIRE(!validAsciiReason(std::string(MaxReasonBytes + 257, 'r')));
    D6R_REQUIRE(!validPropertyKey(std::string(MaxKeyBytes + 257, 'k')));
    D6R_REQUIRE(!validParticipantName(std::string(MaxParticipantNameBytes + 257, 'n')));
}

D6R_TEST_CASE("admission gate enforces payload one-request and exact deadline without granting policy") {
    ManualClock time;
    int hookCalls = 0;
    AdmissionGate gate([&](const auto &) { ++hookCalls; return AdmissionOutcome::Accepted; }, time.clock());
    D6R_REQUIRE(gate.submit(std::vector<std::uint8_t>(MaxAdmissionPayloadBytes, 1)) == AdmissionOutcome::Accepted);
    D6R_REQUIRE_EQ(1, hookCalls);
    D6R_REQUIRE(gate.submit({1}) == AdmissionOutcome::SessionPolicyViolation);
    D6R_REQUIRE_EQ(1, hookCalls);

    AdmissionGate oversized([&](const auto &) { ++hookCalls; return AdmissionOutcome::Accepted; }, time.clock());
    D6R_REQUIRE(oversized.submit(std::vector<std::uint8_t>(MaxAdmissionPayloadBytes + 1, 1)) == AdmissionOutcome::MalformedRequest);
    D6R_REQUIRE_EQ(1, hookCalls);
    AdmissionGate empty([&](const auto &) { ++hookCalls; return AdmissionOutcome::Accepted; }, time.clock());
    D6R_REQUIRE(empty.submit({}) == AdmissionOutcome::MalformedRequest);

    AdmissionGate before([](const auto &) { return AdmissionOutcome::Accepted; }, time.clock());
    time.advance(2999ms);
    D6R_REQUIRE(before.submit({1}) == AdmissionOutcome::Accepted);
    AdmissionGate boundary([](const auto &) { return AdmissionOutcome::Accepted; }, time.clock());
    time.advance(3000ms);
    D6R_REQUIRE(boundary.expireIfDue());
    D6R_REQUIRE(boundary.state() == AdmissionGate::State::Expired);
}

D6R_TEST_CASE("pending admission quotas release safely and permit paced same-source participants") {
    ManualClock time;
    PendingAdmissionLimiter limiter(time.clock());
    std::vector<std::shared_ptr<PendingAdmissionLimiter::Reservation>> held;
    for (std::uint32_t source = 1; source <= 2; ++source)
        for (std::size_t index = 0; index < 4; ++index) D6R_REQUIRE(held.emplace_back(limiter.reserve(source)) != nullptr);
    D6R_REQUIRE(!limiter.reserve(3));
    held.front()->release();
    held.front()->release();
    D6R_REQUIRE(limiter.reserve(3));
    held.clear();

    PendingAdmissionLimiter sequential(time.clock());
    for (std::size_t index = 0; index < MaxParticipants; ++index) {
        auto reservation = sequential.reserve(0x7f000001u);
        D6R_REQUIRE(reservation != nullptr);
        reservation.reset();
        time.advance(3s);
    }

    PendingAdmissionLimiter burst(time.clock());
    for (std::size_t index = 0; index < AdmissionAttemptBurst; ++index) {
        auto reservation = burst.reserve(99);
        D6R_REQUIRE(reservation != nullptr);
        reservation.reset();
    }
    D6R_REQUIRE(!burst.reserve(99));
    time.advance(3s);
    D6R_REQUIRE(burst.reserve(99));
}

D6R_TEST_CASE("aggregate queue budget is atomic concurrent and never evicts existing reservations") {
    AggregateQueueBudget budget;
    D6R_REQUIRE(budget.reserve(MaxAggregateQueuedBytes));
    D6R_REQUIRE_EQ(MaxAggregateQueuedBytes, budget.used());
    D6R_REQUIRE(!budget.reserve(1));
    D6R_REQUIRE_EQ(MaxAggregateQueuedBytes, budget.used());
    budget.release(MaxAggregateQueuedBytes);
    D6R_REQUIRE_EQ(0u, budget.used());
    D6R_REQUIRE(!budget.reserve(MaxAggregateQueuedBytes + 1));

    constexpr std::size_t threads = 16;
    constexpr std::size_t share = MaxAggregateQueuedBytes / threads;
    std::atomic<std::size_t> accepted{0};
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < threads + 1; ++index)
        workers.emplace_back([&] { if (budget.reserve(share)) ++accepted; });
    for (auto &worker: workers) worker.join();
    D6R_REQUIRE_EQ(threads, accepted.load());
    D6R_REQUIRE_EQ(MaxAggregateQueuedBytes, budget.used());
    workers.clear();
    for (std::size_t index = 0; index < threads; ++index) workers.emplace_back([&] { budget.release(share); });
    for (auto &worker: workers) worker.join();
    D6R_REQUIRE_EQ(0u, budget.used());
}

D6R_TEST_CASE("rate primitives enforce burst refill windows isolation and input tick uniqueness") {
    ManualClock time;
    TokenBucket guest(GuestToHostBytesPerSecond, GuestToHostBurstBytes, time.clock());
    D6R_REQUIRE(guest.consume(GuestToHostBurstBytes));
    D6R_REQUIRE(!guest.consume(1));
    time.advance(1s);
    D6R_REQUIRE(guest.consume(GuestToHostBytesPerSecond));
    D6R_REQUIRE(!guest.consume(1));
    TokenBucket isolated(GuestToHostBytesPerSecond, GuestToHostBurstBytes, time.clock());
    D6R_REQUIRE(isolated.consume(GuestToHostBurstBytes));

    TokenBucket host(HostToGuestBytesPerSecond, HostToGuestBurstBytes, time.clock());
    D6R_REQUIRE(host.consume(HostToGuestBurstBytes));
    D6R_REQUIRE(!host.consume(1));
    TokenBucket actions(NonInputActionsPerSecond, NonInputActionBurst, time.clock());
    D6R_REQUIRE(actions.consume(NonInputActionBurst));
    D6R_REQUIRE(!actions.consume(1));

    PerSecondRateCounter slot(InputsPerOwnedSlotPerSecond, time.clock());
    D6R_REQUIRE(slot.consume(InputsPerOwnedSlotPerSecond));
    D6R_REQUIRE(!slot.consume(1));
    PerSecondRateCounter global(GlobalAcceptedInputsPerSecond, time.clock());
    D6R_REQUIRE(global.consume(GlobalAcceptedInputsPerSecond));
    D6R_REQUIRE(!global.consume(1));
    time.advance(1s);
    D6R_REQUIRE(slot.consume(InputsPerOwnedSlotPerSecond));
    D6R_REQUIRE(global.consume(GlobalAcceptedInputsPerSecond));

    ConsecutiveWindowLimit offender(time.clock()), other(time.clock());
    D6R_REQUIRE(!offender.recordOverLimit());
    time.advance(1s);
    D6R_REQUIRE(offender.recordOverLimit());
    D6R_REQUIRE(!other.recordOverLimit());
    time.advance(1s);
    offender.recordWithinLimit();
    D6R_REQUIRE(!offender.recordOverLimit());

    AppliedInputGate applied;
    D6R_REQUIRE(applied.reserve(4, 100));
    D6R_REQUIRE(!applied.reserve(4, 100));
    D6R_REQUIRE(applied.reserve(5, 100));
    D6R_REQUIRE(applied.reserve(4, 101));
    applied.clearBefore(101);
    D6R_REQUIRE(applied.reserve(4, 100));
}

D6R_TEST_CASE("token buckets reject nonfinite and negative values without poisoning valid state") {
    const double nan = (std::numeric_limits<double>::quiet_NaN)();
    const double infinity = (std::numeric_limits<double>::infinity)();
    D6R_REQUIRE_THROW(TokenBucket(-1, 1), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(1, -1), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(nan, 1), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(1, nan), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(infinity, 1), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(1, infinity), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(-infinity, 1), std::invalid_argument);
    D6R_REQUIRE_THROW(TokenBucket(1, -infinity), std::invalid_argument);

    ManualClock time;
    TokenBucket bucket(10, 20, time.clock());
    D6R_REQUIRE(!bucket.consume(-1));
    D6R_REQUIRE(!bucket.consume(nan));
    D6R_REQUIRE(!bucket.consume(infinity));
    D6R_REQUIRE(!bucket.consume(-infinity));
    D6R_REQUIRE(bucket.consume(20));
    D6R_REQUIRE(!bucket.consume(1));
    time.advance(1s);
    D6R_REQUIRE(bucket.consume(10));

    TokenBucket zero(0, 0, time.clock());
    D6R_REQUIRE(zero.consume(0));
    D6R_REQUIRE(!zero.consume(1));
    ManualClock hugeTime;
    TokenBucket huge(1, 5, hugeTime.clock());
    D6R_REQUIRE(huge.consume(5));
    hugeTime.set(TimePoint::max());
    D6R_REQUIRE(huge.consume(5));
}

D6R_TEST_CASE("consecutive rate windows handle adjacency gaps rollback and extreme time") {
    ManualClock adjacentTime;
    adjacentTime.set(TimePoint{} + 999ms);
    ConsecutiveWindowLimit adjacent(adjacentTime.clock());
    D6R_REQUIRE(!adjacent.recordOverLimit());
    adjacentTime.advance(1ms);
    D6R_REQUIRE(adjacent.recordOverLimit());

    ManualClock gapTime;
    ConsecutiveWindowLimit gaps(gapTime.clock());
    D6R_REQUIRE(!gaps.recordOverLimit());
    gapTime.advance(2s);
    D6R_REQUIRE(!gaps.recordOverLimit());
    gapTime.advance(1s);
    D6R_REQUIRE(gaps.recordOverLimit());
    gapTime.advance(3s);
    D6R_REQUIRE(!gaps.recordOverLimit());
    gapTime.advance(1s);
    gaps.recordWithinLimit();
    gapTime.advance(1s);
    D6R_REQUIRE(!gaps.recordOverLimit());

    ManualClock rollbackTime;
    rollbackTime.set(TimePoint{} + 10s);
    ConsecutiveWindowLimit rollback(rollbackTime.clock());
    D6R_REQUIRE(!rollback.recordOverLimit());
    rollbackTime.set(TimePoint{} + 9s);
    D6R_REQUIRE(!rollback.recordOverLimit());
    rollbackTime.set(TimePoint{} + 10s);
    D6R_REQUIRE(rollback.recordOverLimit());

    ManualClock extremeTime;
    extremeTime.set(TimePoint::min());
    ConsecutiveWindowLimit extreme(extremeTime.clock());
    D6R_REQUIRE(!extreme.recordOverLimit());
    extremeTime.set(TimePoint::max());
    D6R_REQUIRE(!extreme.recordOverLimit());
    extremeTime.set(TimePoint::min());
    D6R_REQUIRE(!extreme.recordOverLimit());
}

D6R_TEST_CASE("authorization is immutable owner scoped revoked on disconnect and fail closed") {
    AuthorizationPolicy policy;
    policy.createLocalHost(1, 10);
    D6R_REQUIRE(policy.bindGuest(2, 20));
    D6R_REQUIRE(!policy.bindGuest(2, 30));
    D6R_REQUIRE(!policy.bindGuest(3, 20));
    D6R_REQUIRE(policy.setOwnedSlots(10, {1, 2}));
    D6R_REQUIRE(policy.setOwnedSlots(20, {3, 4}));
    D6R_REQUIRE(!policy.setOwnedSlots(20, {2, 3}));
    D6R_REQUIRE(!policy.setOwnedSlots(20, std::vector<PlayerSlotId>(MaxParticipants + 1, 9)));
    D6R_REQUIRE(policy.authorize(1, AuthorityAction::HostOnly));
    D6R_REQUIRE(!policy.authorize(2, AuthorityAction::HostOnly));
    D6R_REQUIRE(policy.authorize(2, AuthorityAction::OwnReadiness));
    D6R_REQUIRE(policy.authorize(2, AuthorityAction::PlayerInput, 3));
    D6R_REQUIRE(!policy.authorize(2, AuthorityAction::PlayerInput, 1));
    D6R_REQUIRE(!policy.authorize(2, AuthorityAction::PlayerInput));
    D6R_REQUIRE(!policy.authorize(999, AuthorityAction::OwnReadiness));
    const auto denied = policy.decide(2, AuthorityAction::HostOnly);
    D6R_REQUIRE(!denied.allowed && denied.closeConnection);
    D6R_REQUIRE(denied.outcome == AdmissionOutcome::SessionPolicyViolation);
    D6R_REQUIRE_EQ("Connection ended.", std::string(outcomeUserCopy(denied.outcome)));
    policy.disconnect(2);
    D6R_REQUIRE(!policy.authorize(2, AuthorityAction::PlayerInput, 3));
    D6R_REQUIRE(!policy.authorize(22, AuthorityAction::PlayerInput, 3));
}

D6R_TEST_CASE("fixed outcomes and diagnostics are byte exact and cannot disclose hostile peer values") {
    const std::vector<std::tuple<AdmissionOutcome, std::string, std::string>> outcomes{
        {AdmissionOutcome::MalformedRequest, "malformed-request", "Connection request rejected."},
        {AdmissionOutcome::NotAuthorized, "not-authorized", "Connection not authorized."},
        {AdmissionOutcome::HostPolicyRejected, "host-policy-rejected", "Host rejected the connection."},
        {AdmissionOutcome::SessionPolicyViolation, "session-policy-violation", "Connection ended."},
        {AdmissionOutcome::Accepted, "accepted", ""}};
    const std::vector<std::string> hostile{"peer-secret", "203.0.113.77", "../../etc/passwd", "sha256:hostile", "credential-value", "\xE2\x80\xAEname"};
    for (const auto &entry: outcomes) {
        D6R_REQUIRE_EQ(std::get<1>(entry), std::string(outcomeCode(std::get<0>(entry))));
        D6R_REQUIRE_EQ(std::get<2>(entry), std::string(outcomeUserCopy(std::get<0>(entry))));
        for (const auto &value: hostile) D6R_REQUIRE(std::get<2>(entry).find(value) == std::string::npos);
    }
    const std::string diagnostic = formatDiagnostic({123, 7, DiagnosticStage::Authorization,
            DiagnosticCategory::Rejected, DiagnosticLimit::Inputs, 16, 15});
    D6R_REQUIRE_EQ("timestamp=123 connection=7 stage=authorization category=rejected limit=inputs current=16 maximum=15", diagnostic);
    for (const auto &value: hostile) D6R_REQUIRE(diagnostic.find(value) == std::string::npos);
}

D6R_TEST_CASE("wrong reconnect attempts preserve reservation deadline and legitimate credential") {
    ManualClock time;
    auto fill = [](std::uint8_t *target, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) target[index] = static_cast<std::uint8_t>(0x80u + index);
        return true;
    };
    ReconnectReservation wrongAttempt(11, 22, 33, time.clock(), fill);
    auto expected = wrongAttempt.credential();
    auto wrong = expected;
    wrong.bytes.back() ^= 1;
    const std::vector<ReconnectAuthorizationResult> failures{
        wrongAttempt.authorizeAndConsume(wrong, 11, 22, 33),
        wrongAttempt.authorizeAndConsume(expected, 12, 22, 33),
        wrongAttempt.authorizeAndConsume(expected, 11, 23, 33),
        wrongAttempt.authorizeAndConsume(expected, 11, 22, 34),
        wrongAttempt.authorizeAndConsume(ReconnectCredential{}, 11, 22, 33)};
    for (const auto &failure: failures) {
        D6R_REQUIRE(!failure.accepted);
        D6R_REQUIRE(failure.ratePolicyFailure);
        D6R_REQUIRE_EQ(std::string(ReconnectAuthorizationFailureCopy), std::string(failure.userCopy));
    }
    D6R_REQUIRE(wrongAttempt.valid());
    D6R_REQUIRE(wrongAttempt.credential().bytes == expected.bytes);
    time.advance(29999ms);
    const auto accepted = wrongAttempt.authorizeAndConsume(expected, 11, 22, 33);
    D6R_REQUIRE(accepted.accepted);
    D6R_REQUIRE(!accepted.ratePolicyFailure);
    D6R_REQUIRE(accepted.userCopy.empty());
    D6R_REQUIRE(!wrongAttempt.valid());
    D6R_REQUIRE(!wrongAttempt.consume(expected, 11, 22, 33));
    D6R_REQUIRE_THROW(wrongAttempt.credential(), std::logic_error);
}

D6R_TEST_CASE("successful reconnect consumes once and expiry invalidates at exact boundary") {
    ManualClock time;
    auto fill = [](std::uint8_t *target, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) target[index] = static_cast<std::uint8_t>(0x80u + index);
        return true;
    };
    ReconnectReservation beforeExpiry(11, 22, 33, time.clock(), fill);
    auto expected = beforeExpiry.credential();
    time.advance(29999ms);
    D6R_REQUIRE(beforeExpiry.consume(expected, 11, 22, 33));
    D6R_REQUIRE(!beforeExpiry.consume(expected, 11, 22, 33));
    D6R_REQUIRE(!beforeExpiry.valid());
    D6R_REQUIRE_THROW(beforeExpiry.credential(), std::logic_error);

    ReconnectReservation atExpiry(11, 22, 33, time.clock(), fill);
    expected = atExpiry.credential();
    time.advance(30s);
    D6R_REQUIRE(!atExpiry.consume(expected, 11, 22, 33));
    D6R_REQUIRE(!atExpiry.valid());
    D6R_REQUIRE_THROW(atExpiry.credential(), std::logic_error);
}

D6R_TEST_CASE("reconnect lifecycle invalidates on cancel removal session end and explicit expiry") {
    ManualClock time;
    auto fill = [](std::uint8_t *target, std::size_t size) {
        const auto value = credential(9);
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    };
    ReconnectReservation cancelled(11, 22, 33, time.clock(), fill);
    D6R_REQUIRE(!cancelled.cancel(11, 22, 34));
    D6R_REQUIRE(cancelled.valid());
    D6R_REQUIRE(cancelled.cancel(11, 22, 33));
    D6R_REQUIRE(!cancelled.valid());

    ReconnectReservation removed(11, 22, 33, time.clock(), fill);
    D6R_REQUIRE(!removed.participantRemoved(11, 23));
    D6R_REQUIRE(removed.valid());
    D6R_REQUIRE(removed.participantRemoved(11, 22));
    D6R_REQUIRE(!removed.valid());

    ReconnectReservation ended(11, 22, 33, time.clock(), fill);
    D6R_REQUIRE(!ended.sessionEnded(12));
    D6R_REQUIRE(ended.valid());
    D6R_REQUIRE(ended.sessionEnded(11));
    D6R_REQUIRE(!ended.valid());

    ReconnectReservation expired(11, 22, 33, time.clock(), fill);
    time.advance(29999ms);
    D6R_REQUIRE(!expired.expireIfDue());
    D6R_REQUIRE(expired.valid());
    time.advance(1ms);
    D6R_REQUIRE(expired.expireIfDue());
    D6R_REQUIRE(!expired.valid());
}

D6R_TEST_CASE("replacement invalidates old before generating distinct new credential") {
    ManualClock time;
    std::size_t calls = 0;
    ReconnectReservation rotating(11, 22, 33, time.clock(), [&](auto *target, std::size_t size) {
        const auto value = credential(calls++ == 0 ? 1 : 2);
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    });
    const auto old = rotating.credential();
    D6R_REQUIRE(rotating.replace(11, 22, 33, 34));
    const auto replacement = rotating.credential();
    D6R_REQUIRE(old.bytes != replacement.bytes);
    D6R_REQUIRE(!rotating.consume(old, 11, 22, 33));
    D6R_REQUIRE(rotating.valid());
    D6R_REQUIRE(rotating.credential().bytes == replacement.bytes);
    D6R_REQUIRE(rotating.consume(replacement, 11, 22, 34));
    D6R_REQUIRE(!rotating.valid());

    std::size_t failedCalls = 0;
    ReconnectReservation failedReplacement(11, 22, 40, time.clock(), [&](auto *target, std::size_t size) {
        ++failedCalls;
        const auto value = failedCalls == 1 ? credential(3) : ReconnectCredential{};
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    });
    const auto invalidatedOld = failedReplacement.credential();
    D6R_REQUIRE(!failedReplacement.replace(11, 22, 40, 41));
    D6R_REQUIRE_EQ(1u + MaxReconnectCredentialGenerationAttempts, failedCalls);
    D6R_REQUIRE(!failedReplacement.valid());
    D6R_REQUIRE(!failedReplacement.consume(invalidatedOld, 11, 22, 40));
}

D6R_TEST_CASE("all-zero RNG regenerates then fails closed after bounded repeated zeros") {
    ManualClock time;
    std::size_t calls = 0;
    ReconnectReservation regenerated(11, 22, 33, time.clock(), [&](auto *target, std::size_t size) {
        ++calls;
        const auto value = calls == 1 ? ReconnectCredential{} : credential(7);
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    });
    D6R_REQUIRE_EQ(2u, calls);
    D6R_REQUIRE(regenerated.valid());
    D6R_REQUIRE(regenerated.credential().bytes == credential(7).bytes);

    calls = 0;
    ReconnectReservation repeatedZeros(11, 22, 33, time.clock(), [&](auto *target, std::size_t size) {
        ++calls;
        std::fill(target, target + size, 0);
        return true;
    });
    D6R_REQUIRE_EQ(MaxReconnectCredentialGenerationAttempts, calls);
    D6R_REQUIRE(!repeatedZeros.valid());
    D6R_REQUIRE_THROW(repeatedZeros.credential(), std::logic_error);
}

D6R_TEST_CASE("reconnect failures use identical copy and isolate rate-policy attacker") {
    ManualClock time;
    auto fill = [](std::uint8_t *target, std::size_t size) {
        const auto value = credential(5);
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    };
    ReconnectReservation attacker(1, 2, 3, time.clock(), fill);
    ReconnectReservation other(1, 4, 5, time.clock(), fill);
    const auto legitimate = attacker.credential();
    const auto otherCredential = other.credential();
    std::vector<ReconnectAuthorizationResult> failures;
    for (std::size_t attempt = 0; attempt < 8; ++attempt)
        failures.push_back(attacker.authorizeAndConsume(ReconnectCredential{}, 1, 2, 3));
    ManualClock expiryTime;
    ReconnectReservation expired(1, 6, 7, expiryTime.clock(), fill);
    const auto expiredCredential = expired.credential();
    expiryTime.advance(30s);
    failures.push_back(expired.authorizeAndConsume(expiredCredential, 1, 6, 7));
    ReconnectReservation unavailable(0, 0, 0, time.clock(), fill);
    failures.push_back(unavailable.authorizeAndConsume(legitimate, 1, 2, 3));
    for (const auto &failure: failures) {
        D6R_REQUIRE(!failure.accepted && failure.ratePolicyFailure);
        D6R_REQUIRE_EQ(std::string(ReconnectAuthorizationFailureCopy), std::string(failure.userCopy));
    }
    D6R_REQUIRE(attacker.valid());
    D6R_REQUIRE(attacker.credential().bytes == legitimate.bytes);
    D6R_REQUIRE(other.valid());
    D6R_REQUIRE(other.consume(otherCredential, 1, 4, 5));
}

D6R_TEST_CASE("independent reconnect reservations rotate credentials") {
    ManualClock time;

    ReconnectReservation first(11, 22, 1, time.clock(), [&](auto *target, std::size_t size) {
        const auto value = credential(1);
        std::copy(value.bytes.begin(), value.bytes.end(), target); return size == ReconnectCredentialBytes;
    });
    ReconnectReservation rotated(11, 22, 2, time.clock(), [&](auto *target, std::size_t size) {
        const auto value = credential(2);
        std::copy(value.bytes.begin(), value.bytes.end(), target); return size == ReconnectCredentialBytes;
    });
    D6R_REQUIRE(first.credential().bytes != rotated.credential().bytes);
}

D6R_TEST_CASE("reconnect credential RAII copies moves and assignments preserve scoped single use") {
    ManualClock time;
    auto fill = [](std::uint8_t *target, std::size_t size) {
        const auto value = credential(21);
        std::copy(value.bytes.begin(), value.bytes.end(), target);
        return size == value.bytes.size();
    };
    ReconnectReservation reservation(7, 8, 9, time.clock(), fill);
    ReconnectCredential original = reservation.credential();
    ReconnectCredential copied(original);
    ReconnectCredential copyAssigned;
    copyAssigned = original;
    D6R_REQUIRE(copied.bytes == original.bytes);
    D6R_REQUIRE(copyAssigned.bytes == original.bytes);

    ReconnectCredential moved(std::move(copied));
    D6R_REQUIRE(moved.bytes == original.bytes);
    D6R_REQUIRE(allZero(copied));
    ReconnectCredential moveAssigned;
    moveAssigned = std::move(copyAssigned);
    D6R_REQUIRE(moveAssigned.bytes == original.bytes);
    D6R_REQUIRE(allZero(copyAssigned));

    ReconnectCredential survivesInnerDestruction;
    {
        ReconnectCredential temporary(original);
        survivesInnerDestruction = temporary;
    }
    D6R_REQUIRE(survivesInnerDestruction.bytes == original.bytes);
    D6R_REQUIRE(!reservation.consume(moved, 7, 8, 10));
    D6R_REQUIRE(reservation.valid());
    D6R_REQUIRE(reservation.consume(survivesInnerDestruction, 7, 8, 9));
    D6R_REQUIRE(!reservation.consume(original, 7, 8, 9));
}

D6R_TEST_CASE("operating system reconnect randomness is nonzero and sampling does not repeat") {
    std::set<std::array<std::uint8_t, ReconnectCredentialBytes>> observed;
    for (std::uint64_t index = 1; index <= 32; ++index) {
        ReconnectReservation reservation(1, index, index);
        D6R_REQUIRE(reservation.valid());
        D6R_REQUIRE(!allZero(reservation.credential()));
        D6R_REQUIRE(observed.insert(reservation.credential().bytes).second);
    }
}

D6R_TEST_CASE("guest content and script execution stays prohibited while host work remains bounded") {
    D6R_REQUIRE(!guestContentMayLoadOrExecute());
    ConcurrentWorkLimiter validations;
    D6R_REQUIRE(validations.reserve());
    D6R_REQUIRE(validations.reserve());
    D6R_REQUIRE(!validations.reserve());
    D6R_REQUIRE_EQ(2u, validations.active());
    validations.release();
    D6R_REQUIRE(validations.reserve());
    validations.release(); validations.release(); validations.release();
    D6R_REQUIRE_EQ(0u, validations.active());
    D6R_REQUIRE(validParticipantName("bounded cosmetic name"));
    D6R_REQUIRE(!validLogicalPath("../profile.lua"));
    D6R_REQUIRE(validLogicalPath("profiles/host_skin.json"));
}
