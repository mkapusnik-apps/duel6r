#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "source/network/StateReplication.h"
#include "source/server/AuthoritativeMatch.h"
#include "source/server/AuthoritativeReplication.h"
#include "tests/TestHarness.h"

namespace {
    namespace R = Duel6::Network::Replication;
    namespace A = Duel6::Server::Authoritative;

    R::CanonicalState lobbyState() {
        R::CanonicalState state;
        state.sessionId = 10;
        state.hostParticipantId = 20;
        state.phase = R::Phase::Lobby;
        state.participants = {
                {20, true, R::ConnectionState::Connected, true, {101}},
                {21, false, R::ConnectionState::Connected, false, {102}}};
        state.settings.mode = "Deathmatch";
        state.settings.levelPlan = "Fixed";
        state.settings.levels = {"levels/a.json"};
        state.settings.roundLimit = 2;
        R::PlayerState first;
        first.playerId = 101;
        first.ownerParticipantId = 20;
        first.rosterPosition = 0;
        first.displayName = "Host";
        first.life = 100;
        R::PlayerState second = first;
        second.playerId = 102;
        second.ownerParticipantId = 21;
        second.rosterPosition = 1;
        second.displayName = "Guest";
        state.players = {first, second};
        state.score.ranking = {101, 102};
        return state;
    }

    R::CanonicalState activeState() {
        R::CanonicalState state = lobbyState();
        state.matchId = 30;
        state.phase = R::Phase::ActiveRound;
        state.currentRoundNumber = 1;
        state.phaseTime = 12;
        state.round = R::RoundState{40, 1, "levels/a.json", false, {101, 102}, {}};
        state.players[0].positionX = 1000;
        state.players[0].heldWeapon = "pistol";
        state.players[0].ammunition = 6;
        state.players[1].positionX = 2000;
        state.messages.status = "ActiveRound";
        state.messages.currentPlayerIndicators = {101};
        state.messages.roundProgress = 12;
        R::WorldEntityState projectile;
        projectile.entityId = 50;
        projectile.kind = R::EntityKind::Projectile;
        projectile.ownerPlayerId = 101;
        projectile.type = "rocket";
        projectile.lifecycle = "active";
        state.entities = {projectile};
        R::ScoreRowState hostScore;
        hostScore.playerId = 101;
        R::ScoreRowState guestScore;
        guestScore.playerId = 102;
        state.score.players = {hostScore, guestScore};
        return state;
    }

    const R::PlayerState *player(const R::CanonicalState &state, R::Identity identity) {
        const auto found = std::find_if(state.players.begin(), state.players.end(), [identity](const auto &value) {
            return value.playerId == identity;
        });
        return found == state.players.end() ? nullptr : &*found;
    }

    const R::WorldEntityState *entity(const R::CanonicalState &state, R::Identity identity) {
        const auto found = std::find_if(state.entities.begin(), state.entities.end(), [identity](const auto &value) {
            return value.entityId == identity;
        });
        return found == state.entities.end() ? nullptr : &*found;
    }

    void requireCoreStateEqual(const R::CanonicalState &expected, const R::CanonicalState &actual) {
        D6R_REQUIRE_EQ(expected.sessionId, actual.sessionId);
        D6R_REQUIRE_EQ(expected.matchId, actual.matchId);
        D6R_REQUIRE(expected.phase == actual.phase);
        D6R_REQUIRE_EQ(expected.currentRoundNumber, actual.currentRoundNumber);
        D6R_REQUIRE_EQ(expected.completedRounds, actual.completedRounds);
        D6R_REQUIRE_EQ(expected.participants.size(), actual.participants.size());
        D6R_REQUIRE_EQ(expected.players.size(), actual.players.size());
        D6R_REQUIRE_EQ(expected.entities.size(), actual.entities.size());
        D6R_REQUIRE_EQ(expected.score.ranking, actual.score.ranking);
        D6R_REQUIRE_EQ(expected.messages.status, actual.messages.status);
        D6R_REQUIRE_EQ(expected.result.serialized, actual.result.serialized);
        D6R_REQUIRE_EQ(expected.round.has_value(), actual.round.has_value());
        if (expected.round) D6R_REQUIRE_EQ(expected.round->roundId, actual.round->roundId);
        for (const auto &expectedPlayer: expected.players) {
            const auto *actualPlayer = player(actual, expectedPlayer.playerId);
            D6R_REQUIRE(actualPlayer != nullptr);
            D6R_REQUIRE_EQ(expectedPlayer.positionX, actualPlayer->positionX);
            D6R_REQUIRE_EQ(expectedPlayer.life, actualPlayer->life);
            D6R_REQUIRE_EQ(expectedPlayer.heldWeapon, actualPlayer->heldWeapon);
        }
        for (const auto &expectedEntity: expected.entities) {
            const auto *actualEntity = entity(actual, expectedEntity.entityId);
            D6R_REQUIRE(actualEntity != nullptr);
            D6R_REQUIRE_EQ(expectedEntity.positionX, actualEntity->positionX);
            D6R_REQUIRE_EQ(expectedEntity.lifecycle, actualEntity->lifecycle);
        }
    }

    R::IncrementalUpdate validUpdate(const R::CanonicalState &before, R::CanonicalState after,
                                     std::vector<R::PresentationEvent> events = {}) {
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(before));
        const auto update = publisher.publish(std::move(after), std::move(events));
        D6R_REQUIRE(update.has_value());
        return *update;
    }

    void requireRejectedWithoutVersionMutation(const R::IncrementalUpdate &invalid) {
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply(R::FullSnapshot{1, activeState()}) == R::ApplyResult::Applied);
        D6R_REQUIRE(client.apply(invalid) == R::ApplyResult::ResynchronizationRequired);
        D6R_REQUIRE_EQ(1u, client.version());
        D6R_REQUIRE(!client.current());
        D6R_REQUIRE(client.state() == nullptr);
        D6R_REQUIRE(client.takePresentationEvents().empty());
    }

    std::vector<A::PlayerDefinition> roster() {
        return {{20, 101, "Host", 0}, {21, 102, "Guest", 1}};
    }

    A::MatchConfig matchConfig() {
        A::MatchConfig config;
        config.seed = 1234;
        config.hostParticipantId = 20;
        config.levelPlan = A::LevelPlan::Fixed;
        config.fixedLevel = "levels/a.json";
        config.playableLevels = {"levels/a.json"};
        config.enabledWeapons = {"pistol"};
        config.roundLimit = 1;
        return config;
    }

    Duel6::Network::GameplayManifest manifest() {
        return {{"data/blocks.json", {}}, {"data/config.script", {}}, {"levels/a.json", {}}};
    }
}

D6R_TEST_CASE("REP stable identity source issues nonzero unique non-reused category identities") {
    R::StableIdentitySource source;
    const auto first = source.issue(R::IdentityCategory::WorldEntity);
    const auto second = source.issue(R::IdentityCategory::WorldEntity);
    D6R_REQUIRE(first != 0);
    D6R_REQUIRE(second != 0);
    D6R_REQUIRE(first != second);
    D6R_REQUIRE(source.wasIssued(R::IdentityCategory::WorldEntity, first));
    D6R_REQUIRE(!source.wasIssued(R::IdentityCategory::Player, first));
}

D6R_TEST_CASE("REP complete snapshot and lifecycle delta converge atomically to the same state") {
    const auto initial = activeState();
    auto expected = initial;
    expected.phaseTime = 13;
    expected.players[0].positionX = 1100;
    expected.entities[0].positionX = 500;
    R::WorldEntityState pickup;
    pickup.entityId = 51;
    pickup.kind = R::EntityKind::WeaponPickup;
    pickup.type = "bazooka";
    pickup.lifecycle = "available";
    expected.entities.push_back(pickup);

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    const auto update = publisher.publish(expected, {{60, "shot", 101, 102, 50, 1}});
    D6R_REQUIRE(update.has_value());
    D6R_REQUIRE_EQ(1u, update->baseline);
    D6R_REQUIRE_EQ(2u, update->version);

    R::ReplicatedState deltaClient;
    D6R_REQUIRE(deltaClient.apply(R::FullSnapshot{1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(deltaClient.apply(*update) == R::ApplyResult::Applied);
    const auto deliveredEvents = deltaClient.takePresentationEvents();
    D6R_REQUIRE_EQ(1u, deliveredEvents.size());
    D6R_REQUIRE_EQ(60u, deliveredEvents.front().eventId);

    R::ReplicatedState snapshotClient;
    D6R_REQUIRE(snapshotClient.apply(*publisher.fullSnapshot()) == R::ApplyResult::Applied);
    requireCoreStateEqual(*snapshotClient.state(), *deltaClient.state());
    requireCoreStateEqual(expected, *deltaClient.state());
}

D6R_TEST_CASE("REP create update remove tombstones and snapshot replacement are terminal") {
    const auto initial = activeState();
    auto removed = initial;
    removed.entities.clear();
    const auto removal = validUpdate(initial, removed);
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply(R::FullSnapshot{1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(removal) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state()->entities.empty());

    auto reuse = removed;
    reuse.entities = initial.entities;
    auto creation = validUpdate(removed, reuse);
    creation.baseline = 2;
    creation.version = 3;
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(2u, client.version());

    R::ReplicatedState replacement;
    D6R_REQUIRE(replacement.apply(R::FullSnapshot{1, initial}) == R::ApplyResult::Applied);
    auto noEntity = initial;
    noEntity.entities.clear();
    D6R_REQUIRE(replacement.apply(R::FullSnapshot{2, noEntity}) == R::ApplyResult::Applied);
    D6R_REQUIRE(replacement.state()->entities.empty());
    D6R_REQUIRE(replacement.apply(R::FullSnapshot{3, initial}) == R::ApplyResult::Invalid);
    D6R_REQUIRE_EQ(2u, replacement.version());
}

D6R_TEST_CASE("REP round transition rejects retained old round-bound identities") {
    const auto initial = activeState();
    auto nextRound = initial;
    nextRound.currentRoundNumber = 2;
    nextRound.completedRounds = 1;
    nextRound.round->roundId = 41;
    nextRound.round->roundNumber = 2;
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    D6R_REQUIRE(!publisher.publish(nextRound).has_value());
}

D6R_TEST_CASE("REP invalid stale duplicate out-of-order malformed and inconsistent deltas do not mutate") {
    auto next = activeState();
    next.players[0].positionX++;
    const auto valid = validUpdate(activeState(), next);

    auto stale = valid;
    stale.baseline = 0;
    requireRejectedWithoutVersionMutation(stale);
    auto duplicate = valid;
    duplicate.version = 1;
    requireRejectedWithoutVersionMutation(duplicate);
    auto outOfOrder = valid;
    outOfOrder.baseline = 2;
    outOfOrder.version = 3;
    requireRejectedWithoutVersionMutation(outOfOrder);
    auto malformed = valid;
    malformed.players.front().value.reset();
    requireRejectedWithoutVersionMutation(malformed);
    auto inconsistent = valid;
    inconsistent.players.push_back(inconsistent.players.front());
    requireRejectedWithoutVersionMutation(inconsistent);
}

D6R_TEST_CASE("REP one active resynchronization blocks deltas and restores every supported phase") {
    for (const auto phase: {R::Phase::Lobby, R::Phase::ActiveRound, R::Phase::RoundSummary,
                            R::Phase::FinalSummary, R::Phase::Ended}) {
        auto state = phase == R::Phase::Lobby ? lobbyState() : activeState();
        state.phase = phase;
        if (phase == R::Phase::FinalSummary || phase == R::Phase::Ended) state.round.reset();
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply(R::FullSnapshot{1, state}) == R::ApplyResult::Applied);
        auto invalid = validUpdate(state, state);
        invalid.baseline = 99;
        invalid.version = 100;
        D6R_REQUIRE(client.apply(invalid) == R::ApplyResult::ResynchronizationRequired);
        D6R_REQUIRE(client.apply(invalid) == R::ApplyResult::WaitingForSnapshot);
        D6R_REQUIRE(client.resynchronizationRequired());
        auto restored = state;
        restored.phaseTime++;
        D6R_REQUIRE(client.apply(R::FullSnapshot{2, restored}) == R::ApplyResult::Applied);
        D6R_REQUIRE(client.current());
        D6R_REQUIRE_EQ(2u, client.version());
        D6R_REQUIRE_EQ(restored.phaseTime, client.state()->phaseTime);
    }
}

D6R_TEST_CASE("REP presentation events are delivered once and invalid references trigger recovery") {
    const auto initial = activeState();
    auto next = initial;
    next.phaseTime++;
    const auto update = validUpdate(initial, next, {{60, "hit", 101, 102, 50, 5}});
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply(R::FullSnapshot{1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(update) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(1u, client.takePresentationEvents().size());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    auto replay = update;
    replay.baseline = 2;
    replay.version = 3;
    D6R_REQUIRE(client.apply(replay) == R::ApplyResult::ResynchronizationRequired);

    auto badReference = validUpdate(initial, next, {{61, "explosion", 101, 0, 9999, 1}});
    requireRejectedWithoutVersionMutation(badReference);
}

D6R_TEST_CASE("REP canonical validation enforces collection string payload and reference bounds") {
    D6R_REQUIRE(R::validateCanonicalState(activeState()));
    auto tooManyParticipants = activeState();
    tooManyParticipants.participants.resize(R::MaxReplicatedParticipants + 1,
                                            tooManyParticipants.participants.front());
    D6R_REQUIRE(!R::validateCanonicalState(tooManyParticipants));
    auto longName = activeState();
    longName.players[0].displayName.assign(65, 'x');
    D6R_REQUIRE(!R::validateCanonicalState(longName));
    auto tooManyMessages = activeState();
    tooManyMessages.messages.events.assign(R::MaxReplicatedMessages + 1, "event");
    D6R_REQUIRE(!R::validateCanonicalState(tooManyMessages));
    auto oversized = activeState();
    oversized.result.available = true;
    oversized.result.serialized.assign(R::MaxReplicatedResultBytes, 'x');
    D6R_REQUIRE(!R::validateCanonicalState(oversized));
    auto badOwner = activeState();
    badOwner.entities[0].ownerPlayerId = 9999;
    D6R_REQUIRE(!R::validateCanonicalState(badOwner));
    auto badEffect = activeState();
    badEffect.effects = {{70, "shield", 101, 9999, 12}};
    D6R_REQUIRE(!R::validateCanonicalState(badEffect));
}

D6R_TEST_CASE("REP authoritative headless match constructs lobby and convergent active state") {
    A::AuthoritativeReplication replication(900);
    const std::vector<R::ParticipantState> participants = {
            {20, true, R::ConnectionState::Connected, true, {101}},
            {21, false, R::ConnectionState::Connected, true, {102}}};
    D6R_REQUIRE(replication.setLobby(20, participants, roster(), matchConfig()));
    const auto lobby = replication.fullSnapshot();
    D6R_REQUIRE(lobby.has_value());
    D6R_REQUIRE(lobby->state.phase == R::Phase::Lobby);
    D6R_REQUIRE_EQ(2u, lobby->state.players.size());

    A::AuthoritativeMatch match;
    D6R_REQUIRE(match.start(matchConfig(), roster(), manifest()).code == A::OutcomeCode::None);
    const auto begin = replication.beginMatch(match);
    D6R_REQUIRE(begin.has_value());
    D6R_REQUIRE(begin->phase == R::Phase::ActiveRound);
    D6R_REQUIRE(begin->round.has_value());
    D6R_REQUIRE(begin->round->roundId != 0);

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply(*lobby) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(*begin) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state()->phase == R::Phase::ActiveRound);
    requireCoreStateEqual(replication.fullSnapshot()->state, *client.state());

    D6R_REQUIRE(match.advanceOneTick());
    const auto tick = replication.capture(match);
    D6R_REQUIRE(tick.has_value());
    D6R_REQUIRE(client.apply(*tick) == R::ApplyResult::Applied);
    requireCoreStateEqual(replication.fullSnapshot()->state, *client.state());

    D6R_REQUIRE(match.submit({match.currentTick(), 1, 20, 101, A::ActionKind::ShotDamage,
                              102, 0, A::MaximumLife}) == A::ActionResult::Accepted);
    D6R_REQUIRE(match.currentRoundResult().winnerPlayerIds == std::vector<A::Identity>{101});
    const auto roundSummary = replication.capture(match);
    D6R_REQUIRE(roundSummary.has_value());
    D6R_REQUIRE(roundSummary->phase == R::Phase::RoundSummary);
    D6R_REQUIRE(roundSummary->round->outcome.winnerPlayerIds == std::vector<R::Identity>{101});
    D6R_REQUIRE(client.apply(*roundSummary) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state()->round->outcome.winnerPlayerIds == std::vector<R::Identity>{101});
    for (std::uint32_t remaining = 0; remaining < A::RoundEndTotalTicks; ++remaining)
        D6R_REQUIRE(match.advanceOneTick());
    const auto finalSummary = replication.capture(match);
    D6R_REQUIRE(finalSummary.has_value());
    D6R_REQUIRE(finalSummary->phase == R::Phase::FinalSummary);
    D6R_REQUIRE(finalSummary->result.available);
    D6R_REQUIRE(finalSummary->result.sessionOnly);
    D6R_REQUIRE(!finalSummary->result.serialized.empty());
    D6R_REQUIRE(client.apply(*finalSummary) == R::ApplyResult::Applied);
    requireCoreStateEqual(replication.fullSnapshot()->state, *client.state());
}
