#include <algorithm>
#include <chrono>
#include <cstdint>
#include <set>
#include <vector>

#include "source/network/NetworkResponsiveness.h"
#include "tests/TestHarness.h"

namespace {
    namespace N = Duel6::Network::Responsiveness;
    namespace R = Duel6::Network::Replication;
    using namespace std::chrono_literals;

    N::TimePoint at(std::chrono::milliseconds elapsed) {
        return N::TimePoint{} + elapsed;
    }

    R::CanonicalState activeState(std::size_t playerCount = 2) {
        R::CanonicalState state;
        state.sessionId = 10;
        state.matchId = 20;
        state.hostParticipantId = 30;
        state.phase = R::Phase::ActiveRound;
        state.currentRoundNumber = 1;
        state.phaseTime = 100;
        state.settings.mode = "Deathmatch";
        state.settings.levelPlan = "Fixed";
        state.settings.fixedLevel = "levels/a.json";
        state.settings.levels = {"levels/a.json"};
        state.settings.roundLimit = 1;
        state.round = R::RoundState{40, 1, "levels/a.json", false, {}, {}};
        R::ParticipantState participant;
        participant.participantId = 30;
        participant.host = true;
        participant.connection = R::ConnectionState::Connected;
        participant.ready = true;
        state.participants.push_back(participant);
        for (std::size_t index = 0; index < playerCount; ++index) {
            R::PlayerState player;
            player.playerId = 100 + index;
            player.ownerParticipantId = 30;
            player.rosterPosition = static_cast<std::uint8_t>(index);
            player.displayName = "Player " + std::to_string(index + 1);
            player.lifeState = R::LifeState::Alive;
            player.life = 100;
            state.players.push_back(player);
            state.participants[0].ownedPlayerIds.push_back(player.playerId);
            state.round->rosterOrder.push_back(player.playerId);
            state.score.players.push_back({player.playerId});
            state.score.ranking.push_back(player.playerId);
        }
        return state;
    }

    const N::PresentedPlayerPose &presented(
            const std::vector<N::PresentedPlayerPose> &poses, R::Identity playerId) {
        const auto found = std::find_if(poses.begin(), poses.end(), [playerId](const auto &pose) {
            return pose.playerId == playerId;
        });
        D6R_REQUIRE(found != poses.end());
        return *found;
    }

    void observeFreshSupported(N::ConnectionQualityMonitor &monitor, R::StateVersion version,
                               N::TimePoint now, std::chrono::milliseconds latency = 10ms) {
        D6R_REQUIRE(monitor.observeNetworkSample({latency, 100, 0}, now));
        D6R_REQUIRE(monitor.observeCanonicalState(version, now));
    }
}

D6R_TEST_CASE("responsiveness budgets expose the approved same-machine and private-LAN limits") {
    const auto sameMachine = N::budget(N::Environment::SameMachine);
    D6R_REQUIRE_EQ(20ms, sameMachine.roundTripLatency);
    D6R_REQUIRE_EQ(5ms, sameMachine.jitter);
    D6R_REQUIRE_EQ(0.0, sameMachine.packetLossPercent);
    D6R_REQUIRE_EQ(100ms, sameMachine.localResponse);

    const auto privateLan = N::budget(N::Environment::PrivateLan);
    D6R_REQUIRE_EQ(100ms, privateLan.roundTripLatency);
    D6R_REQUIRE_EQ(30ms, privateLan.jitter);
    D6R_REQUIRE_EQ(1.0, privateLan.packetLossPercent);
    D6R_REQUIRE_EQ(150ms, privateLan.localResponse);
}

D6R_TEST_CASE("canonical pacing publishes at 20 Hz and lifecycle transitions immediately") {
    N::CanonicalUpdatePacer pacer;
    std::vector<std::uint64_t> ordinaryPublications;
    for (std::uint64_t tick = 0; tick <= 60; ++tick)
        if (pacer.shouldPublish(tick, false)) ordinaryPublications.push_back(tick);
    D6R_REQUIRE_EQ(std::size_t{21}, ordinaryPublications.size());
    for (std::size_t index = 1; index < ordinaryPublications.size(); ++index)
        D6R_REQUIRE_EQ(std::uint64_t{3}, ordinaryPublications[index] - ordinaryPublications[index - 1]);

    pacer.reset(60);
    D6R_REQUIRE(!pacer.shouldPublish(61, false));
    D6R_REQUIRE(pacer.shouldPublish(61, true));
    D6R_REQUIRE(!pacer.shouldPublish(62, false));
    D6R_REQUIRE(!pacer.shouldPublish(63, false));
    D6R_REQUIRE(pacer.shouldPublish(64, false));
}

D6R_TEST_CASE("supported boundary samples remain connected and do not become degraded") {
    for (const auto environment : {N::Environment::SameMachine, N::Environment::PrivateLan}) {
        N::ConnectionQualityMonitor monitor(environment);
        const auto limit = N::budget(environment);
        D6R_REQUIRE(monitor.observeNetworkSample({limit.roundTripLatency, 10000,
                environment == N::Environment::PrivateLan ? 100u : 0u}, at(0ms)));
        D6R_REQUIRE(monitor.observeNetworkSample({limit.roundTripLatency, 10000,
                environment == N::Environment::PrivateLan ? 100u : 0u}, at(1ms)));
        D6R_REQUIRE(monitor.observeCanonicalState(1, at(1ms)));
        for (std::uint64_t version = 2; version <= 82; ++version) {
            const auto now = at(std::chrono::milliseconds(version * 50));
            D6R_REQUIRE(monitor.observeCanonicalState(version, now));
            const auto presentation = monitor.update(now);
            D6R_REQUIRE(!presentation.degraded);
            D6R_REQUIRE(!presentation.reconnecting);
        }
    }
}

D6R_TEST_CASE("state age must breach continuously for one second before exact degraded text appears") {
    N::ConnectionQualityMonitor monitor(N::Environment::SameMachine);
    observeFreshSupported(monitor, 1, at(0ms));
    D6R_REQUIRE(!monitor.update(at(1249ms)).degraded);
    const auto degraded = monitor.update(at(1250ms));
    D6R_REQUIRE(degraded.degraded);
    D6R_REQUIRE_EQ(std::string("Network connection degraded."), degraded.degradedText);
    D6R_REQUIRE(degraded.degradedText.find("disconnected") == std::string::npos);
    D6R_REQUIRE(degraded.degradedText.find("host") == std::string::npos);
}

D6R_TEST_CASE("continuously stale canonical updates still trigger degradation after one second") {
    N::ConnectionQualityMonitor monitor(N::Environment::SameMachine);
    D6R_REQUIRE(monitor.observeNetworkSample({10ms, 100, 0}, at(0ms)));
    for (R::StateVersion version = 1; version <= 20; ++version) {
        const auto now = at(std::chrono::milliseconds((version - 1) * 50));
        D6R_REQUIRE(monitor.observeCanonicalState(version, 300ms, now));
        D6R_REQUIRE(!monitor.update(now).degraded);
    }
    const auto now = at(1000ms);
    D6R_REQUIRE(monitor.observeCanonicalState(21, 300ms, now));
    const auto degraded = monitor.update(now);
    D6R_REQUIRE(degraded.degraded);
    D6R_REQUIRE_EQ(std::string("Network connection degraded."), degraded.degradedText);
}

D6R_TEST_CASE("network budget breach must be sustained and supported recovery must last three seconds") {
    N::ConnectionQualityMonitor monitor(N::Environment::PrivateLan);
    observeFreshSupported(monitor, 1, at(0ms), 90ms);
    D6R_REQUIRE(monitor.observeNetworkSample({101ms, 100, 0}, at(10ms)));
    D6R_REQUIRE(monitor.observeCanonicalState(2, at(3009ms)));
    D6R_REQUIRE(!monitor.update(at(3009ms)).degraded);
    D6R_REQUIRE(monitor.observeCanonicalState(3, at(3010ms)));
    D6R_REQUIRE(monitor.update(at(3010ms)).degraded);

    D6R_REQUIRE(monitor.observeNetworkSample({90ms, 100, 0}, at(3020ms)));
    D6R_REQUIRE(monitor.observeNetworkSample({90ms, 100, 0}, at(3020ms)));
    D6R_REQUIRE(monitor.observeCanonicalState(4, at(3020ms)));
    D6R_REQUIRE(monitor.update(at(3020ms)).degraded);
    R::StateVersion version = 5;
    for (auto elapsed = 50ms; elapsed <= 2950ms; elapsed += 50ms) {
        D6R_REQUIRE(monitor.observeCanonicalState(version++, at(3020ms + elapsed)));
        D6R_REQUIRE(monitor.update(at(3020ms + elapsed)).degraded);
    }
    D6R_REQUIRE(monitor.observeCanonicalState(version++, at(6019ms)));
    D6R_REQUIRE(monitor.update(at(6019ms)).degraded);
    D6R_REQUIRE(monitor.observeCanonicalState(version, at(6020ms)));
    const auto recovered = monitor.update(at(6020ms));
    D6R_REQUIRE(!recovered.degraded);
    D6R_REQUIRE(recovered.degradedText.empty());
}

D6R_TEST_CASE("resynchronization retains only confirmed context and transport close routes to reconnect") {
    N::ConnectionQualityMonitor monitor(N::Environment::SameMachine);
    observeFreshSupported(monitor, 7, at(0ms));
    monitor.beginResynchronization();
    auto presentation = monitor.update(at(1ms));
    D6R_REQUIRE(presentation.resynchronizing);
    D6R_REQUIRE(presentation.retainingLastConfirmedState);
    D6R_REQUIRE_EQ(std::string("Last confirmed state"), presentation.retainedStateText);
    D6R_REQUIRE(!monitor.observeCanonicalState(6, at(2ms)));
    D6R_REQUIRE(monitor.update(at(2ms)).resynchronizing);
    D6R_REQUIRE(monitor.observeCanonicalState(8, at(3ms)));
    D6R_REQUIRE(!monitor.update(at(3ms)).resynchronizing);

    monitor.transportClosed();
    presentation = monitor.update(at(4ms));
    D6R_REQUIRE(presentation.reconnecting);
    D6R_REQUIRE(presentation.resynchronizing);
    D6R_REQUIRE(!presentation.degraded);
    D6R_REQUIRE(presentation.retainingLastConfirmedState);
}

D6R_TEST_CASE("only living local players can be predicted and authoritative outcomes are untouched") {
    N::CanonicalMovementPresentation movement;
    movement.setLocallyControlledPlayers({100, 101});
    auto canonical = activeState();
    canonical.players[1].lifeState = R::LifeState::Dead;
    canonical.players[1].life = 0;
    canonical.score.players[0].kills = 1;
    canonical.score.players[1].deaths = 1;
    D6R_REQUIRE(movement.accept(1, canonical, at(0ms)));

    D6R_REQUIRE(movement.predictLocalMovement({100, 500, 20, true, true}, at(1ms)));
    D6R_REQUIRE(!movement.predictLocalMovement({101, 500, 20, true, true}, at(1ms)));
    D6R_REQUIRE(!movement.predictLocalMovement({999, 500, 20, true, true}, at(1ms)));
    const auto poses = movement.sample(at(1ms));
    D6R_REQUIRE_EQ(std::int64_t{500}, presented(poses, 100).positionX);
    D6R_REQUIRE_EQ(std::int64_t{0}, presented(poses, 101).positionX);
    D6R_REQUIRE_EQ(std::uint64_t{1}, canonical.score.players[0].kills);
    D6R_REQUIRE_EQ(std::uint64_t{1}, canonical.score.players[1].deaths);
}

D6R_TEST_CASE("remote interpolation is monotonic and local correction converges by 150 ms") {
    N::CanonicalMovementPresentation movement;
    movement.setLocallyControlledPlayers({100});
    auto initial = activeState();
    D6R_REQUIRE(movement.accept(1, initial, at(0ms)));
    D6R_REQUIRE(movement.predictLocalMovement({100, 400, 0, false, false}, at(1ms)));

    auto next = initial;
    next.phaseTime++;
    next.players[0].positionX = 100;
    next.players[1].positionX = 100;
    D6R_REQUIRE(movement.accept(2, next, at(10ms)));
    D6R_REQUIRE(!movement.predictLocalMovement({101, 900, 0, false, false}, at(11ms)));

    const auto first = movement.sample(at(35ms));
    const auto second = movement.sample(at(60ms));
    D6R_REQUIRE_EQ(std::int64_t{50}, presented(first, 101).positionX);
    D6R_REQUIRE_EQ(std::int64_t{100}, presented(second, 101).positionX);
    D6R_REQUIRE(presented(first, 101).positionX <= presented(second, 101).positionX);

    const auto correctionOne = movement.sample(at(60ms));
    const auto correctionTwo = movement.sample(at(110ms));
    const auto correctionDone = movement.sample(at(160ms));
    D6R_REQUIRE_EQ(std::int64_t{300}, presented(correctionOne, 100).positionX);
    D6R_REQUIRE_EQ(std::int64_t{200}, presented(correctionTwo, 100).positionX);
    D6R_REQUIRE_EQ(std::int64_t{100}, presented(correctionDone, 100).positionX);
}

D6R_TEST_CASE("full resynchronization rejects rewind and replaces removed player presentation") {
    N::CanonicalMovementPresentation movement;
    movement.setLocallyControlledPlayers({100});
    auto state = activeState(15);
    D6R_REQUIRE(movement.accept(10, state, at(0ms)));
    D6R_REQUIRE_EQ(std::size_t{15}, movement.sample(at(0ms)).size());
    movement.beginResynchronization();
    D6R_REQUIRE(!movement.predictLocalMovement({100, 10, 0, false, false}, at(1ms)));

    auto rewind = state;
    rewind.phaseTime--;
    D6R_REQUIRE(!movement.accept(11, rewind, at(2ms)));
    D6R_REQUIRE(movement.resynchronizing());
    state.phaseTime++;
    state.players.pop_back();
    state.round->rosterOrder.pop_back();
    state.participants[0].ownedPlayerIds.pop_back();
    state.score.players.pop_back();
    state.score.ranking.pop_back();
    D6R_REQUIRE(movement.accept(12, state, at(3ms)));
    D6R_REQUIRE(!movement.resynchronizing());
    D6R_REQUIRE_EQ(std::size_t{14}, movement.sample(at(3ms)).size());
}
