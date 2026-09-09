#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "tests/TestHarness.h"
#include "source/client/HostServiceSupervisor.h"
#include "source/input/PlayerControls.h"
#include "source/network/HostCompositionProtocol.h"
#include "source/network/NetworkResponsiveness.h"
#include "source/network/PlayerInputProtocol.h"
#include "source/network/Protocol.h"
#include "source/network/SessionLifecycle.h"
#include "source/network/StateReplication.h"
#include "source/network/StateReplicationProtocol.h"

// This test-only access exposes queued application commands and snapshots without
// adding a production diagnostic API solely for QA.
#define private public
#include "source/client/NetworkSessionRuntime.h"
#undef private

namespace {
    using namespace Duel6;

    Network::Replication::CanonicalState canonical(std::uint64_t sessionId,
                                                     Network::Replication::Phase phase) {
        Network::Replication::CanonicalState state;
        state.sessionId = sessionId;
        state.phase = phase;
        return state;
    }

    Client::NetworkLocalPlayer player(std::string name) {
        Client::NetworkLocalPlayer value;
        value.name = std::move(name);
        return value;
    }
}

D6R_TEST_CASE("issue-38 guest participant commands preserve order and reconnect Leave preempts queued work") {
    Client::NetworkSessionRuntime runtime;
    runtime.current.localParticipantId = 17;
    runtime.current.canonical = canonical(91, Network::Replication::Phase::Lobby);

    runtime.setReady(true);
    runtime.localConfigurationChanged();
    runtime.setReady(false);
    D6R_REQUIRE_EQ(3u, runtime.pendingGuestCommands.size());

    const std::vector<Network::Lifecycle::ParticipantActionKind> expected{
            Network::Lifecycle::ParticipantActionKind::Ready,
            Network::Lifecycle::ParticipantActionKind::ConfigurationChanged,
            Network::Lifecycle::ParticipantActionKind::NotReady};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto action = Network::Lifecycle::deserializeParticipantAction(
                runtime.pendingGuestCommands[index]);
        D6R_REQUIRE(action.has_value());
        D6R_REQUIRE_EQ(91u, action->sessionId);
        D6R_REQUIRE_EQ(17u, action->participantId);
        D6R_REQUIRE(action->kind == expected[index]);
    }

    runtime.current.journey = Client::NetworkJourney::Reconnecting;
    runtime.leave();
    D6R_REQUIRE_EQ(1u, runtime.pendingGuestCommands.size());
    const auto leave = Network::Lifecycle::deserializeParticipantAction(
            runtime.pendingGuestCommands.front());
    D6R_REQUIRE(leave.has_value());
    D6R_REQUIRE(leave->kind == Network::Lifecycle::ParticipantActionKind::Leave);
    D6R_REQUIRE(runtime.current.journey == Client::NetworkJourney::Cancelling);
    D6R_REQUIRE_EQ(std::string("Leaving session…"), runtime.current.status);
}

D6R_TEST_CASE("issue-38 admitted local slot count is immutable while valid same-count rebinding remains editable") {
    Client::NetworkSessionRuntime runtime;
    runtime.players = {player("Alpha"), player("Beta")};
    runtime.sampledActions = {1, 2};

    runtime.rebindLocalPlayers({player("Only one")});
    D6R_REQUIRE_EQ(2u, runtime.players.size());
    D6R_REQUIRE_EQ(std::string("Alpha"), runtime.players[0].name);

    runtime.rebindLocalPlayers({player("Gamma"), player(std::string("bad\nname"))});
    D6R_REQUIRE_EQ(std::string("Alpha"), runtime.players[0].name);
    D6R_REQUIRE_EQ(std::string("Beta"), runtime.players[1].name);

    runtime.rebindLocalPlayers({player("Gamma"), player("Delta")});
    D6R_REQUIRE_EQ(2u, runtime.players.size());
    D6R_REQUIRE_EQ(std::string("Gamma"), runtime.players[0].name);
    D6R_REQUIRE_EQ(std::string("Delta"), runtime.players[1].name);
    D6R_REQUIRE_EQ(2u, runtime.sampledActions.size());
    D6R_REQUIRE_EQ(0u, runtime.sampledActions[0]);
    D6R_REQUIRE_EQ(0u, runtime.sampledActions[1]);
}

D6R_TEST_CASE("issue-38 canonical phase mapping and presenter event retention are deterministic and bounded") {
    Client::NetworkSessionRuntime runtime;
    runtime.current.host = true;
    runtime.current.journey = Client::NetworkJourney::Starting;

    const std::vector<std::pair<Network::Replication::Phase, Client::NetworkJourney>> phases{
            {Network::Replication::Phase::Lobby, Client::NetworkJourney::Lobby},
            {Network::Replication::Phase::ActiveRound, Client::NetworkJourney::Match},
            {Network::Replication::Phase::RoundSummary, Client::NetworkJourney::Match},
            {Network::Replication::Phase::FinalSummary, Client::NetworkJourney::Summary},
            {Network::Replication::Phase::Ended, Client::NetworkJourney::HostEnded}};
    for (const auto &[phase, journey]: phases) {
        auto state = canonical(91, phase);
        state.hostParticipantId = 7;
        runtime.applyCanonical(state);
        const auto snapshot = runtime.snapshot();
        D6R_REQUIRE(snapshot.journey == journey);
        D6R_REQUIRE_EQ(7u, snapshot.localParticipantId);
        D6R_REQUIRE_EQ(std::string("Connected"), snapshot.status);
    }

    std::vector<Network::Replication::PresentationEvent> events;
    for (std::size_t index = 0; index < Network::Replication::MaxReplicatedEvents + 3; ++index) {
        Network::Replication::PresentationEvent event;
        event.eventId = index + 1;
        events.push_back(event);
    }
    runtime.applyCanonical(canonical(91, Network::Replication::Phase::Lobby), {}, {}, events);
    const auto snapshot = runtime.snapshot();
    D6R_REQUIRE_EQ(Network::Replication::MaxReplicatedEvents, snapshot.presentationEvents.size());
    D6R_REQUIRE_EQ(4u, snapshot.presentationEvents.front().eventId);
}

D6R_TEST_CASE("issue-38 host retry categories distinguish cleanup restart invalid setup and ended session") {
    Client::NetworkSessionRuntime runtime;

    Client::HostServiceSnapshot observed;
    observed.state = Client::HostServiceState::Stopping;
    observed.stopReason = Client::HostServiceStopReason::StartupFailure;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.journey == Client::NetworkJourney::Failure);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::CleanupInProgress);
    D6R_REQUIRE(!runtime.current.retryAllowed);

    observed = {};
    observed.state = Client::HostServiceState::StartupFailed;
    observed.outcome = Client::HostServiceOutcome::HostManifestInvalid;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::RestartRequired);

    observed.outcome = Client::HostServiceOutcome::PortUnavailable;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::InvalidSetup);

    observed = {};
    observed.state = Client::HostServiceState::SessionFailed;
    observed.outcome = Client::HostServiceOutcome::StoppedUnexpectedly;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::EndedSession);
    D6R_REQUIRE_EQ(std::string("Hosted session stopped unexpectedly."), runtime.current.failure);
}

D6R_TEST_CASE("issue-38 host composition framing is deterministic for every UI action and setup boundary") {
    Network::HostComposition::Setup setup;
    for (unsigned index = 0; index < Network::MaxNetworkPlayers; ++index)
        setup.localPlayerNames.push_back(std::string(63, 'A' + index) + char('a' + index));
    setup.fixedLevel = std::string(60, 'a') + '/' + std::string(59, 'b') + '/'
                       + std::string(59, 'c') + '/' + std::string(59, 'd');
    D6R_REQUIRE_EQ(240u, setup.fixedLevel.size());

    const auto encodedSetup = Network::HostComposition::serializeSetup(setup);
    const auto decodedSetup = Network::HostComposition::deserialize(encodedSetup);
    D6R_REQUIRE(decodedSetup.has_value());
    D6R_REQUIRE(decodedSetup->kind == Network::HostComposition::Kind::Setup);
    D6R_REQUIRE(decodedSetup->setup.has_value());
    D6R_REQUIRE_EQ(Network::MaxNetworkPlayers, decodedSetup->setup->localPlayerNames.size());
    D6R_REQUIRE_EQ(setup.fixedLevel, decodedSetup->setup->fixedLevel);
    D6R_REQUIRE_EQ(encodedSetup, Network::HostComposition::serializeSetup(*decodedSetup->setup));

    const std::vector<Network::HostComposition::Kind> actions{
            Network::HostComposition::Kind::StartMatch,
            Network::HostComposition::Kind::ReturnToLobby,
            Network::HostComposition::Kind::AdvanceRound,
            Network::HostComposition::Kind::ConfigurationChanged,
            Network::HostComposition::Kind::Ready,
            Network::HostComposition::Kind::NotReady};
    for (const auto kind: actions) {
        const auto payload = Network::HostComposition::serializeAction(kind);
        D6R_REQUIRE_EQ(8u, payload.size());
        const auto decoded = Network::HostComposition::deserialize(payload);
        D6R_REQUIRE(decoded.has_value());
        D6R_REQUIRE(decoded->kind == kind);
        auto trailing = payload;
        trailing.push_back(0);
        D6R_REQUIRE(!Network::HostComposition::deserialize(trailing).has_value());
    }

    auto unknown = Network::HostComposition::serializeAction(Network::HostComposition::Kind::NotReady);
    unknown[6] = 0;
    unknown[7] = 14;
    D6R_REQUIRE(!Network::HostComposition::deserialize(unknown).has_value());
}
