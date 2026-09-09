#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "tests/TestHarness.h"
#include "source/network/SessionLifecycle.h"

namespace {
    using namespace Duel6::Network;
    using namespace Duel6::Network::Lifecycle;
    using namespace std::chrono_literals;

    struct ManualClock {
        TimePoint value{};
        Clock clock() { return [this] { return value; }; }
        void advance(std::chrono::milliseconds amount) { value += amount; }
    };

    struct CredentialSource {
        std::uint8_t next = 1;
        Trust::RandomFill random() {
            return [this](std::uint8_t *target, std::size_t size) {
                for (std::size_t index = 0; index < size; ++index)
                    target[index] = static_cast<std::uint8_t>(next + index);
                ++next;
                return true;
            };
        }
    };

    ReconnectRequest requestFor(const ReconnectGrant &grant) {
        return {grant.sessionId, grant.participantId, grant.reservationId, grant.credential};
    }

}

D6R_TEST_CASE("lifecycle protocol rejects malformed zero and cross-kind credential messages") {
    CredentialSource source;
    ManualClock time;
    Trust::ReconnectReservation reservation(91, 7, 3, time.clock(), source.random());
    ReconnectGrant grant{91, 7, 3, reservation.credential()};
    ReconnectRequest request = requestFor(grant);

    const auto grantBytes = serializeReconnectGrant(grant);
    const auto requestBytes = serializeReconnectRequest(request);
    D6R_REQUIRE_EQ(48u, grantBytes.size());
    D6R_REQUIRE_EQ(48u, requestBytes.size());
    D6R_REQUIRE(deserializeReconnectGrant(grantBytes).has_value());
    D6R_REQUIRE(deserializeReconnectRequest(requestBytes).has_value());
    D6R_REQUIRE(!deserializeReconnectGrant(requestBytes).has_value());
    D6R_REQUIRE(!deserializeReconnectRequest(grantBytes).has_value());

    auto malformed = requestBytes;
    malformed.push_back(0);
    D6R_REQUIRE(!deserializeReconnectRequest(malformed).has_value());
    malformed = requestBytes;
    malformed[0] ^= 0xff;
    D6R_REQUIRE(!deserializeReconnectRequest(malformed).has_value());
    request.credential.clear();
    D6R_REQUIRE(serializeReconnectRequest(request).empty());

    const IntentionalHostEndNotice notice{91};
    const auto noticeBytes = serializeIntentionalHostEnd(notice);
    D6R_REQUIRE_EQ(16u, noticeBytes.size());
    D6R_REQUIRE_EQ(91u, deserializeIntentionalHostEnd(noticeBytes)->sessionId);
    D6R_REQUIRE(serializeIntentionalHostEnd({0}).empty());
}

D6R_TEST_CASE("participant actions round trip exactly and reject malformed or unknown framing") {
    for (const auto kind: {ParticipantActionKind::Ready,
                           ParticipantActionKind::NotReady,
                           ParticipantActionKind::Leave,
                           ParticipantActionKind::ConfigurationChanged}) {
        const ParticipantAction action{91, 7, kind};
        const auto payload = serializeParticipantAction(action);
        D6R_REQUIRE_EQ(26u, payload.size());
        const auto parsed = deserializeParticipantAction(payload);
        D6R_REQUIRE(parsed.has_value());
        D6R_REQUIRE_EQ(action.sessionId, parsed->sessionId);
        D6R_REQUIRE_EQ(action.participantId, parsed->participantId);
        D6R_REQUIRE(action.kind == parsed->kind);
    }

    D6R_REQUIRE(serializeParticipantAction({0, 7, ParticipantActionKind::Ready}).empty());
    D6R_REQUIRE(serializeParticipantAction({91, 0, ParticipantActionKind::Ready}).empty());
    D6R_REQUIRE(serializeParticipantAction(
            {91, 7, static_cast<ParticipantActionKind>(5)}).empty());
    auto malformed = serializeParticipantAction({91, 7, ParticipantActionKind::Ready});
    malformed.push_back(0);
    D6R_REQUIRE(!deserializeParticipantAction(malformed).has_value());
    malformed = serializeParticipantAction({91, 7, ParticipantActionKind::Ready});
    malformed[8] = 4;
    malformed[9] = 0;
    D6R_REQUIRE(!deserializeParticipantAction(malformed).has_value());
}

D6R_TEST_CASE("authenticated participant actions drive readiness and connected Leave only") {
    ManualClock time;
    CredentialSource source;
    std::vector<ParticipantId> removed;
    HostHooks hooks;
    hooks.removeBatch = [&](const std::vector<ParticipantId> &ids, Phase) {
        removed = ids;
        return true;
    };
    HostSessionLifecycle host(92, 1, 10, {1}, time.clock(), source.random(), hooks);
    D6R_REQUIRE(host.admitGuest(2, 20, {2}, false).has_value());
    D6R_REQUIRE(host.applyParticipantAction({92, 1, ParticipantActionKind::Ready}, 10));
    D6R_REQUIRE(host.applyParticipantAction({92, 2, ParticipantActionKind::Ready}, 20));
    D6R_REQUIRE(host.allConnectedAndReady());
    D6R_REQUIRE(!host.applyParticipantAction({93, 2, ParticipantActionKind::NotReady}, 20));
    D6R_REQUIRE(!host.applyParticipantAction({92, 2, ParticipantActionKind::NotReady}, 99));
    D6R_REQUIRE(host.ready(2));
    D6R_REQUIRE(host.applyParticipantAction({92, 2, ParticipantActionKind::NotReady}, 20));
    D6R_REQUIRE(!host.ready(2));
    D6R_REQUIRE(host.applyParticipantAction({92, 2, ParticipantActionKind::Leave}, 20));
    D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::LobbyUpdated);
    D6R_REQUIRE_EQ((std::vector<ParticipantId>{2}), removed);
}

D6R_TEST_CASE("guest reconnect retains non-current context and one positive 30 second deadline") {
    CredentialSource source;
    ManualClock time;
    Trust::ReconnectReservation reservation(100, 2, 1, time.clock(), source.random());
    ReconnectGrant grant{100, 2, 1, reservation.credential()};
    GuestSessionRecovery guest(100, 2, 20, true);
    D6R_REQUIRE(guest.acceptGrant(grant));
    D6R_REQUIRE(guest.retainedContextIsCurrent());
    D6R_REQUIRE(guest.transportClosed(time.value, true));
    D6R_REQUIRE(guest.journey() == GuestJourney::Reconnecting);
    D6R_REQUIRE(guest.hasRetainedContext());
    D6R_REQUIRE(!guest.retainedContextIsCurrent());
    D6R_REQUIRE_EQ(30u, *guest.positiveSecondsRemaining(time.value));

    const auto firstAttempt = guest.retry();
    time.advance(29001ms);
    D6R_REQUIRE(guest.applyReconnectResult(ReconnectOutcome::RetryableFailure, time.value, false, 0));
    const auto secondAttempt = guest.retry();
    D6R_REQUIRE(firstAttempt.has_value() && secondAttempt.has_value());
    D6R_REQUIRE_EQ(firstAttempt->reservationId, secondAttempt->reservationId);
    D6R_REQUIRE_EQ(firstAttempt->credential.bytes, secondAttempt->credential.bytes);
    D6R_REQUIRE_EQ(1u, *guest.positiveSecondsRemaining(time.value));

    time.advance(999ms);
    guest.update(time.value);
    D6R_REQUIRE(guest.journey() == GuestJourney::ConnectionFailure);
    D6R_REQUIRE(guest.destination() == Destination::ConnectionFailure);
    D6R_REQUIRE(!guest.retry().has_value());
    D6R_REQUIRE(!guest.positiveSecondsRemaining(time.value).has_value());
    D6R_REQUIRE_EQ(std::string(ReconnectExpiredCopy), std::string(guest.failureCopy()));
}

D6R_TEST_CASE("admission clears readiness and accepted reconnect requires current full state and rotates authority") {
    ManualClock time;
    CredentialSource source;
    std::vector<ParticipantId> disconnected;
    std::vector<std::pair<ParticipantId, ConnectionId>> restored;
    std::vector<ConnectionId> closed;
    HostHooks hooks;
    hooks.disconnect = [&](ParticipantId id) { disconnected.push_back(id); return true; };
    hooks.restoreCurrent = [&](ParticipantId id, ConnectionId connection) {
        D6R_REQUIRE_EQ(1u, disconnected.size());
        restored.emplace_back(id, connection);
        return true;
    };
    hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
    HostSessionLifecycle host(500, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto grant = host.admitGuest(2, 20, {2}, true);
    D6R_REQUIRE(grant.has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));
    D6R_REQUIRE(host.reserved(2));
    D6R_REQUIRE(!host.ready(2));
    D6R_REQUIRE(!host.connected(2));

    const auto oldRequest = requestFor(*grant);
    auto result = host.reconnect(oldRequest, 21);
    D6R_REQUIRE(result.outcome == ReconnectOutcome::Accepted);
    D6R_REQUIRE(!result.closeOffendingConnection);
    D6R_REQUIRE(result.nextGrant.has_value());
    D6R_REQUIRE(result.nextGrant->reservationId != grant->reservationId);
    D6R_REQUIRE(result.nextGrant->credential.bytes != grant->credential.bytes);
    D6R_REQUIRE_EQ(1u, restored.size());
    D6R_REQUIRE(host.connected(2));
    D6R_REQUIRE(!host.ready(2));

    const auto replay = host.reconnect(oldRequest, 99);
    D6R_REQUIRE(replay.outcome == ReconnectOutcome::AuthorizationFailed);
    D6R_REQUIRE_EQ(1u, restored.size());
    D6R_REQUIRE_EQ(99u, closed.back());

    GuestSessionRecovery guest(500, 2, 20, true);
    D6R_REQUIRE(guest.acceptGrant(*grant));
    D6R_REQUIRE(guest.transportClosed(TimePoint{}, true));
    D6R_REQUIRE(!guest.applyReconnectResult(ReconnectOutcome::Accepted, TimePoint{} + 1s,
                                            false, 21, result.nextGrant));
    D6R_REQUIRE(guest.journey() == GuestJourney::Reconnecting);
    D6R_REQUIRE(guest.applyReconnectResult(ReconnectOutcome::Accepted, TimePoint{} + 2s,
                                           true, 21, result.nextGrant));
    D6R_REQUIRE(guest.journey() == GuestJourney::Established);
    D6R_REQUIRE(guest.retainedContextIsCurrent());
}

D6R_TEST_CASE("wrong zero and mis-scoped reconnect credentials reveal no protected state and preserve authority") {
    ManualClock time;
    CredentialSource source;
    std::size_t restores = 0;
    std::vector<ConnectionId> closed;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.restoreCurrent = [&](ParticipantId, ConnectionId) { ++restores; return true; };
    hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
    HostSessionLifecycle host(700, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto grant = host.admitGuest(2, 20, {2}, false);
    D6R_REQUIRE(grant.has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));

    std::vector<ReconnectRequest> attacks;
    auto wrong = requestFor(*grant); wrong.credential.bytes[0] ^= 0xff; attacks.push_back(wrong);
    auto zero = requestFor(*grant); zero.credential.clear(); attacks.push_back(zero);
    auto session = requestFor(*grant); ++session.sessionId; attacks.push_back(session);
    auto participant = requestFor(*grant); ++participant.participantId; attacks.push_back(participant);
    auto reservation = requestFor(*grant); ++reservation.reservationId; attacks.push_back(reservation);
    for (std::size_t index = 0; index < attacks.size(); ++index) {
        const auto result = host.reconnect(attacks[index], 30 + index);
        D6R_REQUIRE(result.outcome == ReconnectOutcome::AuthorizationFailed);
        D6R_REQUIRE_EQ(std::string(Trust::ReconnectAuthorizationFailureCopy),
                       std::string(reconnectCopy(result.outcome)));
        D6R_REQUIRE_EQ(0u, restores);
        D6R_REQUIRE(host.reserved(2));
    }
    D6R_REQUIRE_EQ(attacks.size(), closed.size());

    const auto accepted = host.reconnect(requestFor(*grant), 40);
    D6R_REQUIRE(accepted.outcome == ReconnectOutcome::Accepted);
    D6R_REQUIRE_EQ(1u, restores);
}

D6R_TEST_CASE("host reconnect acceptance is strictly before the unchanged host deadline") {
    ManualClock time;
    CredentialSource source;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.restoreCurrent = [](ParticipantId, ConnectionId) { return true; };
    HostSessionLifecycle host(800, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto first = host.admitGuest(2, 20, {2}, false);
    D6R_REQUIRE(first.has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));
    time.advance(29999ms);
    const auto restored = host.reconnect(requestFor(*first), 21);
    D6R_REQUIRE(restored.outcome == ReconnectOutcome::Accepted);
    D6R_REQUIRE(restored.nextGrant.has_value());

    D6R_REQUIRE(host.transportClosed(2, 21));
    time.advance(30s);
    D6R_REQUIRE(host.reconnect(requestFor(*restored.nextGrant), 22).outcome
                == ReconnectOutcome::AuthorizationFailed);
    D6R_REQUIRE(host.reserved(2) == false);
}

D6R_TEST_CASE("failed reconnect delivery restores the original credential and fixed deadline") {
    ManualClock time;
    CredentialSource source;
    std::vector<ConnectionId> closed;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.restoreCurrent = [](ParticipantId, ConnectionId) { return true; };
    hooks.closeConnection = [&](ConnectionId connection) { closed.push_back(connection); };
    HostSessionLifecycle host(825, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto original = host.admitGuest(2, 20, {2}, false);
    D6R_REQUIRE(original.has_value());
    D6R_REQUIRE(host.setReady(1, 10, true));
    D6R_REQUIRE(host.setReady(2, 20, true));
    D6R_REQUIRE(host.transportClosed(2, 20));

    const auto provisional = host.reconnect(requestFor(*original), 21);
    D6R_REQUIRE(provisional.outcome == ReconnectOutcome::Accepted);
    D6R_REQUIRE(provisional.nextGrant.has_value());
    D6R_REQUIRE(host.reconnectDeliveryFailed(2, 21));
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{21}), closed);
    D6R_REQUIRE(host.reserved(2));
    D6R_REQUIRE(!host.connected(2));
    D6R_REQUIRE(host.ready(2));

    time.advance(29999ms);
    const auto retried = host.reconnect(requestFor(*original), 22);
    D6R_REQUIRE(retried.outcome == ReconnectOutcome::Accepted);
    D6R_REQUIRE(host.ready(2));
    D6R_REQUIRE(host.allConnectedAndReady());
    D6R_REQUIRE(host.reconnectDeliverySucceeded(2, 22));
    D6R_REQUIRE(!host.reconnectDeliverySucceeded(2, 22));
    D6R_REQUIRE(host.admitGuest(3, 30, {3}, true).has_value());
    D6R_REQUIRE(!host.ready(1) && !host.ready(2) && !host.ready(3));
}

D6R_TEST_CASE("compatibility rejection is terminal to the attempt but preserves the reservation") {
    const std::vector<std::pair<ReconnectCompatibility, ReconnectOutcome>> cases{
        {ReconnectCompatibility::ReleaseMismatch, ReconnectOutcome::ReleaseMismatch},
        {ReconnectCompatibility::ContentMismatch, ReconnectOutcome::ContentMismatch},
        {ReconnectCompatibility::TrustRejected, ReconnectOutcome::AuthorizationFailed}};
    for (const auto &[compatibility, expected]: cases) {
        ManualClock time;
        CredentialSource source;
        std::size_t restores = 0;
        std::vector<ConnectionId> closed;
        HostHooks hooks;
        hooks.disconnect = [](ParticipantId) { return true; };
        hooks.restoreCurrent = [&](ParticipantId, ConnectionId) { ++restores; return true; };
        hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
        HostSessionLifecycle host(850, 1, 10, {1}, time.clock(), source.random(), hooks);
        const auto grant = host.admitGuest(2, 20, {2}, false);
        D6R_REQUIRE(grant.has_value());
        D6R_REQUIRE(host.transportClosed(2, 20));
        D6R_REQUIRE(host.reconnect(requestFor(*grant), 21, compatibility).outcome == expected);
        D6R_REQUIRE_EQ(0u, restores);
        D6R_REQUIRE_EQ((std::vector<ConnectionId>{21}), closed);
        D6R_REQUIRE(host.reserved(2));
        D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::NothingChanged);
        D6R_REQUIRE_EQ(2u, host.retainedPlayerCount());
        D6R_REQUIRE(host.reconnect(requestFor(*grant), 22).outcome == ReconnectOutcome::Accepted);
        D6R_REQUIRE_EQ(1u, restores);
    }
}

D6R_TEST_CASE("restore crossing the fixed deadline cannot publish authority and queues removal") {
    ManualClock time;
    CredentialSource source;
    std::vector<ParticipantId> disconnected;
    std::vector<ParticipantId> removed;
    std::vector<ConnectionId> closed;
    HostHooks hooks;
    hooks.disconnect = [&](ParticipantId id) { disconnected.push_back(id); return true; };
    hooks.restoreCurrent = [&](ParticipantId, ConnectionId) { time.advance(2ms); return true; };
    hooks.removeBatch = [&](const std::vector<ParticipantId> &ids, Phase) { removed = ids; return true; };
    hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
    HostSessionLifecycle host(860, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto grant = host.admitGuest(2, 20, {2}, false);
    D6R_REQUIRE(grant.has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));
    time.advance(29999ms);
    const auto result = host.reconnect(requestFor(*grant), 21);
    D6R_REQUIRE(result.outcome == ReconnectOutcome::Expired);
    D6R_REQUIRE(!result.nextGrant.has_value());
    D6R_REQUIRE(!host.connected(2));
    D6R_REQUIRE_EQ((std::vector<ParticipantId>{2, 2}), disconnected);
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{21}), closed);
    D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::LobbyUpdated);
    D6R_REQUIRE_EQ((std::vector<ParticipantId>{2}), removed);
}

D6R_TEST_CASE("reentrant failing and throwing lifecycle callbacks fail closed without duplicate mutation") {
    ManualClock time;
    CredentialSource source;
    HostSessionLifecycle *active = nullptr;
    int removeCalls = 0;
    int discards = 0;
    HostHooks hooks;
    hooks.disconnect = [&](ParticipantId) {
        D6R_REQUIRE(active != nullptr);
        D6R_REQUIRE(!active->queueIntentionalLeave(2, 20));
        D6R_REQUIRE(!active->endSession(1, 10).accepted);
        return true;
    };
    hooks.removeBatch = [&](const std::vector<ParticipantId> &, Phase) {
        ++removeCalls;
        D6R_REQUIRE(!active->clearReadiness());
        if (removeCalls == 1) throw std::runtime_error("injected removal failure");
        return true;
    };
    hooks.discardSession = [&] { ++discards; throw std::runtime_error("injected discard failure"); };
    HostSessionLifecycle host(865, 1, 10, {1}, time.clock(), source.random(), hooks);
    active = &host;
    D6R_REQUIRE(host.admitGuest(2, 20, {2}, false).has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));
    time.advance(30s);
    D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::Failed);
    D6R_REQUIRE_EQ(2u, host.retainedPlayerCount());
    D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::LobbyUpdated);
    D6R_REQUIRE_EQ(1u, host.retainedPlayerCount());
    D6R_REQUIRE_EQ(2, removeCalls);
    D6R_REQUIRE_EQ(std::string(HostedServiceStoppedCopy), std::string(host.supervisedHostFailure()));
    D6R_REQUIRE_EQ(1, discards);

    HostHooks throwingDisconnect;
    throwingDisconnect.disconnect = [](ParticipantId) -> bool { throw std::runtime_error("disconnect"); };
    throwingDisconnect.discardSession = [] { throw std::runtime_error("discard"); };
    HostSessionLifecycle failed(866, 1, 10, {1}, time.clock(), source.random(), throwingDisconnect);
    D6R_REQUIRE(failed.admitGuest(2, 20, {2}, false).has_value());
    D6R_REQUIRE(!failed.transportClosed(2, 20));
    D6R_REQUIRE(failed.ended());
}

D6R_TEST_CASE("credential-bearing wire buffers are observably erased after controlled use") {
    ManualClock time;
    CredentialSource source;
    Trust::ReconnectReservation reservation(870, 2, 1, time.clock(), source.random());
    ReconnectGrant grant{870, 2, 1, reservation.credential()};
    auto requestPayload = serializeReconnectRequest(requestFor(grant));
    auto responsePayload = serializeReconnectResponse({870, 2, ReconnectOutcome::Accepted, grant});
    ContentIdentity identity{};
    identity[0] = 1;
    const auto compatibility = makeLocalAdmissionRequest(
            1, GameplayManifest{{"levels/a.json", identity}});
    auto attemptPayload = serializeReconnectAttempt({requestFor(grant), compatibility});
    D6R_REQUIRE(!requestPayload.empty() && !responsePayload.empty() && !attemptPayload.empty());
    eraseLifecycleCredentialPayload(requestPayload);
    eraseLifecycleCredentialPayload(responsePayload);
    eraseLifecycleCredentialPayload(attemptPayload);
    D6R_REQUIRE(std::all_of(requestPayload.begin() + 32, requestPayload.begin() + 48,
                            [](std::uint8_t byte) { return byte == 0; }));
    D6R_REQUIRE(std::all_of(responsePayload.begin() + 36, responsePayload.begin() + 52,
                            [](std::uint8_t byte) { return byte == 0; }));
    D6R_REQUIRE(std::all_of(attemptPayload.begin() + 36, attemptPayload.begin() + 52,
                            [](std::uint8_t byte) { return byte == 0; }));
}

D6R_TEST_CASE("missing grant still retains complete context but cannot manufacture a retry") {
    GuestSessionRecovery guest(872, 2, 20, true);
    ReconnectGrant zero{872, 2, 1, {}};
    D6R_REQUIRE(!guest.acceptGrant(zero));
    D6R_REQUIRE(guest.transportClosed(TimePoint{}, true));
    D6R_REQUIRE(guest.journey() == GuestJourney::Reconnecting);
    D6R_REQUIRE(guest.hasRetainedContext());
    D6R_REQUIRE(!guest.retainedContextIsCurrent());
    D6R_REQUIRE(!guest.retry().has_value());
    guest.update(TimePoint{} + 30s);
    D6R_REQUIRE(guest.journey() == GuestJourney::ConnectionFailure);
    D6R_REQUIRE_EQ(std::string(ReconnectExpiredCopy), std::string(guest.failureCopy()));
}

D6R_TEST_CASE("admission and explicit clearing mutations clear every lifecycle readiness value") {
    ManualClock time;
    CredentialSource source;
    HostSessionLifecycle host(874, 1, 10, {1}, time.clock(), source.random(), {});
    D6R_REQUIRE(host.admitGuest(2, 20, {2}, true).has_value());
    D6R_REQUIRE(!host.ready(2));
    D6R_REQUIRE(host.admitGuest(3, 30, {3}, true).has_value());
    D6R_REQUIRE(!host.ready(2));
    D6R_REQUIRE(!host.ready(3));
    D6R_REQUIRE(host.clearReadiness());
    D6R_REQUIRE(!host.ready(2) && !host.ready(3));
}

D6R_TEST_CASE("successful reserved Leave closes its attempt and removes the reservation once") {
    ManualClock time;
    CredentialSource source;
    std::vector<ConnectionId> closed;
    int removals = 0;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
    hooks.removeBatch = [&](const std::vector<ParticipantId> &ids, Phase) {
        ++removals; D6R_REQUIRE_EQ((std::vector<ParticipantId>{2}), ids); return true;
    };
    HostSessionLifecycle host(876, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto grant = host.admitGuest(2, 20, {2}, false);
    D6R_REQUIRE(grant.has_value());
    D6R_REQUIRE(host.transportClosed(2, 20));
    D6R_REQUIRE(host.queueReservedLeave(requestFor(*grant), 77));
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{77}), closed);
    D6R_REQUIRE(host.processLifecycleBatch(Phase::Lobby) == RemovalOutcome::LobbyUpdated);
    D6R_REQUIRE_EQ(1, removals);
    D6R_REQUIRE_EQ(1u, host.retainedPlayerCount());
}

D6R_TEST_CASE("positive reconnect countdown ceilings a sub-millisecond remainder to one") {
    ManualClock time;
    CredentialSource source;
    Trust::ReconnectReservation reservation(878, 2, 1, time.clock(), source.random());
    GuestSessionRecovery guest(878, 2, 20, true);
    D6R_REQUIRE(guest.acceptGrant({878, 2, 1, reservation.credential()}));
    D6R_REQUIRE(guest.transportClosed(TimePoint{}, true));
    const auto justBefore = TimePoint{} + 30s - 1ns;
    D6R_REQUIRE_EQ(1u, *guest.positiveSecondsRemaining(justBefore));
    D6R_REQUIRE(!guest.positiveSecondsRemaining(TimePoint{} + 30s).has_value());
}

D6R_TEST_CASE("lifecycle admits the host plus fourteen guests but never a sixteenth player") {
    ManualClock time;
    CredentialSource source;
    HostSessionLifecycle host(875, 1, 10, {1}, time.clock(), source.random(), {});
    for (ParticipantId participant = 2; participant <= 15; ++participant)
        D6R_REQUIRE(host.admitGuest(participant, participant * 10, {participant}, false).has_value());
    D6R_REQUIRE_EQ(15u, host.retainedPlayerCount());
    D6R_REQUIRE(!host.admitGuest(16, 160, {16}, false).has_value());
}

D6R_TEST_CASE("same-clock connected leave and reservation expiry form one lifecycle removal batch") {
    ManualClock time;
    CredentialSource source;
    std::vector<std::vector<ParticipantId>> batches;
    std::vector<Phase> phases;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.removeBatch = [&](const std::vector<ParticipantId> &batch, Phase phase) {
        batches.push_back(batch); phases.push_back(phase); return true;
    };
    HostSessionLifecycle host(900, 1, 10, {1}, time.clock(), source.random(), hooks);
    D6R_REQUIRE(host.admitGuest(2, 20, {2}, true).has_value());
    D6R_REQUIRE(host.admitGuest(3, 30, {3}, true).has_value());
    D6R_REQUIRE(host.queueIntentionalLeave(2, 20));
    D6R_REQUIRE(host.transportClosed(3, 30));
    time.advance(30s);
    D6R_REQUIRE(host.processLifecycleBatch(Phase::ActiveRound) == RemovalOutcome::InterruptedToLobby);
    D6R_REQUIRE_EQ(1u, batches.size());
    D6R_REQUIRE_EQ((std::vector<ParticipantId>{2, 3}), batches.front());
    D6R_REQUIRE(phases.front() == Phase::ActiveRound);
    D6R_REQUIRE_EQ(1u, host.retainedPlayerCount());
    D6R_REQUIRE(!host.ready(2));
    D6R_REQUIRE(!host.ready(3));
}

D6R_TEST_CASE("lifecycle removal outcomes preserve each phase contract") {
    const std::vector<std::pair<Phase, RemovalOutcome>> cases{
        {Phase::Lobby, RemovalOutcome::LobbyUpdated},
        {Phase::ActiveRound, RemovalOutcome::ActiveRoundContinues},
        {Phase::NonFinalRoundSummary, RemovalOutcome::NextRoundContinues},
        {Phase::FinalSummary, RemovalOutcome::FinalSummaryRetained}};
    for (const auto &[phase, expected]: cases) {
        ManualClock time;
        CredentialSource source;
        int removeCalls = 0;
        HostHooks hooks;
        hooks.removeBatch = [&](const std::vector<ParticipantId> &ids, Phase observed) {
            ++removeCalls;
            D6R_REQUIRE_EQ((std::vector<ParticipantId>{2}), ids);
            D6R_REQUIRE(observed == phase);
            return true;
        };
        HostSessionLifecycle host(1000, 1, 10, {1, 4}, time.clock(), source.random(), hooks);
        D6R_REQUIRE(host.admitGuest(2, 20, {2}, true).has_value());
        D6R_REQUIRE(host.queueIntentionalLeave(2, 20));
        D6R_REQUIRE(host.processLifecycleBatch(phase) == expected);
        D6R_REQUIRE_EQ(1, removeCalls);
        D6R_REQUIRE_EQ(2u, host.retainedPlayerCount());
    }
}

D6R_TEST_CASE("only established-session host end reaches host-ended and unexpected loss stays reconnecting") {
    ManualClock time;
    CredentialSource source;
    std::vector<ConnectionId> noticeConnections;
    int removeCalls = 0;
    int discards = 0;
    HostHooks hooks;
    hooks.disconnect = [](ParticipantId) { return true; };
    hooks.removeBatch = [&](const std::vector<ParticipantId> &, Phase) { ++removeCalls; return true; };
    hooks.sendIntentionalHostEnd = [&](ConnectionId id, const std::vector<std::uint8_t> &payload) {
        D6R_REQUIRE(deserializeIntentionalHostEnd(payload).has_value());
        noticeConnections.push_back(id); return true;
    };
    hooks.discardSession = [&] { ++discards; };
    HostSessionLifecycle host(1100, 1, 10, {1}, time.clock(), source.random(), hooks);
    const auto connectedGrant = host.admitGuest(2, 20, {2}, false);
    const auto isolatedGrant = host.admitGuest(3, 30, {3}, false);
    D6R_REQUIRE(connectedGrant.has_value() && isolatedGrant.has_value());
    D6R_REQUIRE(host.transportClosed(3, 30));
    D6R_REQUIRE(host.queueIntentionalLeave(2, 20));
    D6R_REQUIRE(!host.endSession(2, 20).accepted);
    const auto ended = host.endSession(1, 10);
    D6R_REQUIRE(ended.accepted);
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{20}), ended.establishedGuestConnections);
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{20}), noticeConnections);
    D6R_REQUIRE_EQ(0, removeCalls);
    D6R_REQUIRE_EQ(1, discards);
    D6R_REQUIRE(host.ended());

    GuestSessionRecovery guest(1100, 2, 20, true);
    D6R_REQUIRE(guest.acceptGrant(*connectedGrant));
    D6R_REQUIRE(guest.transportClosed(TimePoint{} + 5s, true));
    D6R_REQUIRE(guest.journey() == GuestJourney::Reconnecting);
    D6R_REQUIRE(!guest.acceptIntentionalHostEnd({1100}, 99, TimePoint{} + 4s));
    D6R_REQUIRE(!guest.acceptIntentionalHostEnd({1100}, 20, TimePoint{} + 6s));
    D6R_REQUIRE(guest.journey() == GuestJourney::Reconnecting);
    D6R_REQUIRE(guest.acceptIntentionalHostEnd({1100}, 20, TimePoint{} + 5s));
    D6R_REQUIRE(guest.journey() == GuestJourney::HostEnded);
    D6R_REQUIRE(guest.destination() == Destination::HostEnded);
    D6R_REQUIRE(guest.hasRetainedContext());
    D6R_REQUIRE(!guest.retainedContextIsCurrent());
}

D6R_TEST_CASE("host-local supervised failure uses exact private outcome and bounded cleanup") {
    ManualClock time;
    CredentialSource source;
    std::vector<ConnectionId> closed;
    int notices = 0;
    int discards = 0;
    HostHooks hooks;
    hooks.closeConnection = [&](ConnectionId id) { closed.push_back(id); };
    hooks.sendIntentionalHostEnd = [&](ConnectionId, const std::vector<std::uint8_t> &) { ++notices; return true; };
    hooks.discardSession = [&] { ++discards; };
    HostSessionLifecycle host(1200, 1, 10, {1}, time.clock(), source.random(), hooks);
    D6R_REQUIRE(host.admitGuest(2, 20, {2}, false).has_value());
    D6R_REQUIRE(host.admitGuest(3, 30, {3}, false).has_value());
    D6R_REQUIRE_EQ(std::string("Hosted session stopped unexpectedly."),
                   std::string(host.supervisedHostFailure()));
    D6R_REQUIRE(host.ended());
    D6R_REQUIRE_EQ(1u, host.retainedPlayerCount());
    D6R_REQUIRE_EQ((std::vector<ConnectionId>{20, 30}), closed);
    D6R_REQUIRE_EQ(0, notices);
    D6R_REQUIRE_EQ(1, discards);
    D6R_REQUIRE(host.supervisedHostFailure().empty());
}
