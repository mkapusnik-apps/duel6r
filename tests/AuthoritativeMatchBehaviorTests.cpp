#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "source/server/AuthoritativeMatch.h"
#include "source/server/AuthoritativeHostedMatchController.h"
#include "source/server/AuthoritativeReplication.h"
#include "source/server/AuthoritativeMatchSerialization.h"
#include "source/server/AuthoritativeMatchValidation.h"
#include "source/server/CanonicalMatchRuntime.h"
#include "source/server/FrozenGameplayConfig.h"
#include "source/server/NetworkMatchResultRetention.h"
#include "source/network/CompatibilityManifest.h"
#include "source/network/StateReplicationProtocol.h"
#include "source/network/SessionLifecycle.h"
#include "tests/TestHarness.h"

namespace {
using namespace Duel6::Server::Authoritative;
namespace R = Duel6::Network::Replication;

std::vector<PlayerDefinition> roster(std::size_t count = 4) {
    std::vector<PlayerDefinition> result;
    for (std::size_t i = 0; i < count; ++i)
        result.push_back({i + 1, i + 101, "Player " + std::to_string(i + 1), static_cast<std::uint8_t>(i)});
    return result;
}

Duel6::Network::GameplayManifest manifest(std::vector<std::string> levels = {"levels/a.json", "levels/b.json", "levels/c.json"}) {
    Duel6::Network::GameplayManifest result = {{"data/blocks.json", {}}, {"data/config.script", {}}};
    for (auto &level: levels) result.push_back({level, {}});
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.logicalPath < b.logicalPath; });
    return result;
}

MatchConfig config() {
    MatchConfig value;
    value.seed = UINT64_C(0x123456789abcdef0);
    value.hostParticipantId = 1;
    value.levelPlan = LevelPlan::Fixed;
    value.fixedLevel = "levels/a.json";
    value.playableLevels = {"levels/a.json", "levels/b.json", "levels/c.json"};
    value.enabledWeapons = {"pistol", "bazooka"};
    value.roundLimit = 1;
    return value;
}

struct ProductionCanonicalFixture {
    MatchConfig requested;
    std::vector<PlayerDefinition> players = roster(2);
    Duel6::Network::ManifestBuildResult content;
    AuthoritativeHostedMatchController controller;
    std::vector<std::vector<std::uint8_t>> payloads;
    std::uint64_t sequence = 1;
    std::vector<std::uint32_t> previousInputs;
    std::uint8_t observedRound = 0;

    explicit ProductionCanonicalFixture(std::uint8_t rounds, std::size_t playerCount = 2)
            : requested(canonicalConfig(rounds)),
              players(roster(playerCount)),
              content(Duel6::Network::CompatibilityManifestBuilder(".", {}).build()),
              controller(1, CanonicalMatchRuntime::createDependencies(requested, players, content)),
              previousInputs(players.size(), std::numeric_limits<std::uint32_t>::max()) {
        D6R_REQUIRE(content.valid());
        std::vector<R::ParticipantState> participants;
        for (const auto &player: players)
            participants.push_back({player.participantId, player.participantId == 1,
                    R::ConnectionState::Connected, true, {player.playerId}});
        D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
        D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
            payloads.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE(controller.setParticipantReady(1, true));
        D6R_REQUIRE(controller.setParticipantReady(2, true));
        D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, content.manifest).code);
    }

    static MatchConfig canonicalConfig(std::uint8_t rounds) {
        const auto built = Duel6::Network::CompatibilityManifestBuilder(".", {}).build();
        D6R_REQUIRE(built.valid() && built.content);
        const auto source = built.content->find("data/config.script");
        D6R_REQUIRE(source != built.content->end());
        FrozenGameplayConfig gameplay;
        D6R_REQUIRE(parseFrozenGameplayConfig(std::string_view(
                reinterpret_cast<const char *>(source->second.data()), source->second.size()), gameplay));
        MatchConfig value;
        value.seed = 424242;
        value.hostParticipantId = 1;
        value.levelPlan = LevelPlan::Fixed;
        value.fixedLevel = "levels/duel_01.json";
        for (const auto &entry: built.manifest)
            if (entry.logicalPath.compare(0, 7, "levels/") == 0
                && entry.logicalPath.size() > 5
                && entry.logicalPath.compare(entry.logicalPath.size() - 5, 5, ".json") == 0)
                value.playableLevels.push_back(entry.logicalPath);
        value.enabledWeapons = std::move(gameplay.enabledWeapons);
        value.startingAmmoMinimum = 30;
        value.startingAmmoMaximum = 30;
        value.fixedStartingWeapon = "pistol";
        value.compactSpawnLayout = true;
        value.roundLimit = rounds;
        return value;
    }

    bool driveOneTick() {
        AuthoritativeMatch *match = controller.match();
        D6R_REQUIRE(match != nullptr);
        if (match->phase() == MatchPhase::ActiveRound && match->currentTick() % 5u == 0) {
            if (observedRound != match->roundDecision().roundNumber) {
                observedRound = match->roundDecision().roundNumber;
                std::fill(previousInputs.begin(), previousInputs.end(), std::numeric_limits<std::uint32_t>::max());
            }
            const CanonicalWorldSnapshot *world = match->canonicalWorldSnapshot();
            D6R_REQUIRE(world != nullptr);
            const auto shooter = std::find_if(world->players.begin(), world->players.end(),
                    [](const auto &player) { return player.playerId == 102; });
            const auto target = std::find_if(world->players.begin(), world->players.end(),
                    [](const auto &player) { return player.playerId != 102 && player.alive; });
            if (shooter != world->players.end() && target != world->players.end() && shooter->alive && target->alive) {
                const std::int64_t horizontal = std::llabs(target->positionX - shooter->positionX);
                std::uint32_t input = 0;
                if (match->currentTick() < 5u || horizontal > 3 * 65536)
                    input = target->positionX < shooter->positionX ? MoveLeft : MoveRight;
                if (horizontal < 4 * 65536 && (match->currentTick() / 30u + 1u) % 2u == 0) input |= Shoot;
                if (input != previousInputs[1]) {
                    previousInputs[1] = input;
                    D6R_REQUIRE_EQ(ActionResult::Accepted, match->submit({match->currentTick(), sequence++, 2, 102,
                            ActionKind::PlayerInput, 0, input, 0}));
                }
            }
        }
        D6R_REQUIRE(controller.advanceOneTick());
        return controller.observeMatchOutcome();
    }

    bool driveToRound(std::uint8_t roundNumber) {
        for (std::size_t tick = 0; tick < 20000; ++tick) {
            if (controller.match() && controller.match()->phase() == MatchPhase::ActiveRound
                && controller.match()->roundDecision().roundNumber == roundNumber) return true;
            if (!driveOneTick()) return false;
        }
        return false;
    }

    bool driveToRoundScore(std::uint8_t roundNumber) {
        for (std::size_t tick = 0; tick < 20000; ++tick) {
            const AuthoritativeMatch *match = controller.match();
            if (match && match->phase() == MatchPhase::ActiveRound
                && match->roundDecision().roundNumber == roundNumber) {
                const CanonicalWorldSnapshot *world = match->canonicalWorldSnapshot();
                D6R_REQUIRE(world != nullptr);
                if (std::any_of(world->players.begin(), world->players.end(), [](const auto &player) {
                        return player.statistics.totalPoints() != 0;
                    })) return true;
            }
            if (!driveOneTick()) return false;
        }
        return false;
    }

    bool driveToTerminal() {
        for (std::size_t tick = 0; tick < 60000 && controller.match(); ++tick)
            if (!driveOneTick()) return false;
        return controller.match() == nullptr && controller.stage() == HostedMatchStage::Lobby;
    }
};

const PlayerResultRow &resultPlayer(const SessionResult &result, Identity playerId) {
    const auto found = std::find_if(result.players.begin(), result.players.end(),
            [playerId](const auto &row) { return row.playerId == playerId; });
    D6R_REQUIRE(found != result.players.end());
    return *found;
}

const R::ScoreRowState &scorePlayer(const R::CanonicalState &state, Identity playerId) {
    const auto found = std::find_if(state.score.players.begin(), state.score.players.end(),
            [playerId](const auto &row) { return row.playerId == playerId; });
    D6R_REQUIRE(found != state.score.players.end());
    return *found;
}

void requireCumulativeScoreMatchesResult(const R::CanonicalState &state, const SessionResult &result) {
    D6R_REQUIRE_EQ(*serializeSessionResult(result), state.result.serialized);
    for (const auto &row: result.players) {
        const auto &score = scorePlayer(state, row.playerId);
        D6R_REQUIRE_EQ(row.statistics.totalPoints(), score.cumulativePoints);
        D6R_REQUIRE_EQ(row.statistics.shots, score.shots);
        D6R_REQUIRE_EQ(row.statistics.hits, score.hits);
        D6R_REQUIRE_EQ(row.statistics.kills, score.kills);
        D6R_REQUIRE_EQ(row.statistics.deaths, score.deaths);
        D6R_REQUIRE_EQ(row.statistics.assists, score.assists);
        D6R_REQUIRE_EQ(row.statistics.wins, score.wins);
        D6R_REQUIRE_EQ(row.statistics.penalties, score.penalties);
        D6R_REQUIRE_EQ(row.statistics.survivalTicks, score.survivalTicks);
        D6R_REQUIRE_EQ(row.statistics.damage, score.damage);
        D6R_REQUIRE_EQ(row.statistics.assistedDamage, score.assistedDamage);
    }
}

AuthoritativeAction action(const AuthoritativeMatch &match, std::uint64_t sequence, Identity participant,
                           Identity player, ActionKind kind, Identity target = 0, std::int32_t amount = 0,
                           std::uint32_t input = 0) {
    return {match.currentTick(), sequence, participant, player, kind, target, input, amount};
}

void eliminate(AuthoritativeMatch &match, std::uint64_t &sequence, const PlayerDefinition &source,
               const PlayerDefinition &target, int hits = 1) {
    for (int hit = 0; hit < hits; ++hit)
        D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit(action(match, sequence++, source.participantId,
                source.playerId, ActionKind::ShotDamage, target.playerId, MaximumLife)));
}

void finishDelay(AuthoritativeMatch &match) {
    for (std::uint32_t i = 0; i < RoundEndTotalTicks; ++i) match.advanceOneTick();
}

std::vector<R::CanonicalState> deliveredStates(const std::vector<std::vector<std::uint8_t>> &payloads) {
    R::ReplicatedState client;
    std::vector<R::CanonicalState> result;
    for (const auto &payload: payloads) {
        const auto frame = R::deserializeReplicationFrame(payload);
        D6R_REQUIRE(frame.has_value());
        R::ApplyResult applied = R::ApplyResult::Invalid;
        if (frame->snapshot) applied = client.apply(*frame->snapshot);
        else if (frame->update) applied = client.apply(*frame->update);
        else continue;
        D6R_REQUIRE(applied == R::ApplyResult::Applied);
        D6R_REQUIRE(client.state() != nullptr);
        result.push_back(*client.state());
    }
    return result;
}

const R::CanonicalState *lastPhase(const std::vector<R::CanonicalState> &states, R::Phase phase) {
    const auto found = std::find_if(states.rbegin(), states.rend(), [phase](const auto &state) {
        return state.phase == phase;
    });
    return found == states.rend() ? nullptr : &*found;
}

void requireStatisticsEqual(const PlayerStatistics &expected, const PlayerStatistics &actual) {
    D6R_REQUIRE_EQ(expected.roundsPlayed, actual.roundsPlayed);
    D6R_REQUIRE_EQ(expected.shots, actual.shots);
    D6R_REQUIRE_EQ(expected.hits, actual.hits);
    D6R_REQUIRE_EQ(expected.kills, actual.kills);
    D6R_REQUIRE_EQ(expected.deaths, actual.deaths);
    D6R_REQUIRE_EQ(expected.assists, actual.assists);
    D6R_REQUIRE_EQ(expected.wins, actual.wins);
    D6R_REQUIRE_EQ(expected.penalties, actual.penalties);
    D6R_REQUIRE_EQ(expected.survivalTicks, actual.survivalTicks);
    D6R_REQUIRE_EQ(expected.damage, actual.damage);
    D6R_REQUIRE_EQ(expected.assistedDamage, actual.assistedDamage);
}

void verifyProductionCanonicalInterruptedRoundDiscardsScore(std::uint8_t completedRounds) {
    ProductionCanonicalFixture fixture(3, 3);
    const std::uint8_t activeRound = static_cast<std::uint8_t>(completedRounds + 1);
    D6R_REQUIRE(fixture.driveToRound(activeRound));
    AuthoritativeMatch *match = fixture.controller.match();
    D6R_REQUIRE(match != nullptr);
    auto completedStatistics = match->playerStatistics();
    for (auto &entry: completedStatistics) entry.second.roundsPlayed = completedRounds;

    D6R_REQUIRE(fixture.driveToRoundScore(activeRound));
    match = fixture.controller.match();
    D6R_REQUIRE(match != nullptr && match->phase() == MatchPhase::ActiveRound);
    const CanonicalWorldSnapshot *scoredWorld = match->canonicalWorldSnapshot();
    D6R_REQUIRE(scoredWorld != nullptr);
    D6R_REQUIRE(std::any_of(scoredWorld->players.begin(), scoredWorld->players.end(), [](const auto &player) {
        return player.statistics.totalPoints() != 0;
    }));

    D6R_REQUIRE_EQ(ActionResult::Accepted, match->submitHostControl(1, ActionKind::RemovePlayer, 101));
    D6R_REQUIRE_EQ(ActionResult::Accepted, match->submitHostControl(1, ActionKind::RemovePlayer, 103));
    D6R_REQUIRE(fixture.controller.observeMatchOutcome());
    D6R_REQUIRE(fixture.controller.currentSessionResult().has_value());
    const SessionResult &result = *fixture.controller.currentSessionResult();
    D6R_REQUIRE(result.state == ResultState::Interrupted);
    D6R_REQUIRE_EQ(completedRounds, result.completedRounds);
    D6R_REQUIRE_EQ(static_cast<std::size_t>(completedRounds), result.rounds.size());
    for (const auto &row: result.players) {
        D6R_REQUIRE_EQ(static_cast<std::size_t>(completedRounds), row.rounds.size());
        requireStatisticsEqual(completedStatistics.at(row.playerId), row.statistics);
    }

    const auto states = deliveredStates(fixture.payloads);
    const auto *following = lastPhase(states, R::Phase::Lobby);
    D6R_REQUIRE(following != nullptr && following->result.available);
    D6R_REQUIRE_EQ(std::string("Interrupted"), following->result.state);
    requireCumulativeScoreMatchesResult(*following, result);
    if (completedRounds == 0) {
        D6R_REQUIRE(!following->round.has_value());
        for (const auto &score: following->score.players) D6R_REQUIRE_EQ(0, score.roundPoints);
    } else {
        D6R_REQUIRE(following->round.has_value());
        D6R_REQUIRE_EQ(completedRounds, following->round->roundNumber);
        D6R_REQUIRE(following->round->rosterOrder == result.rounds.back().rosterOrder);
        for (const auto &row: result.players)
            D6R_REQUIRE_EQ(row.rounds.back().totalPoints(), scorePlayer(*following, row.playerId).roundPoints);
    }
}

std::string followingLobbyMembershipEvidence(bool interrupted) {
    auto requested = config();
    if (interrupted) requested.roundLimit = 3;
    auto players = roster(2);
    auto participants = std::vector<R::ParticipantState>{
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.restoreReplication(2, [](auto) { return Duel6::Network::SendResult::Accepted; }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    if (interrupted) {
        D6R_REQUIRE_EQ(ActionResult::Accepted, controller.match()->submit(action(
                *controller.match(), sequence++, 1, 0, ActionKind::RemovePlayer, 102)));
    }
    D6R_REQUIRE(controller.observeMatchOutcome());

    controller.disconnectReplication(2);
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Reconnecting));
    const bool reconnectKeptFalse = !controller.participantReady(2);
    D6R_REQUIRE(controller.restoreReplication(2, [](auto) { return Duel6::Network::SendResult::Accepted; }));
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Connected));
    const bool connectionKeptFalse = !controller.participantReady(2);

    participants[0].ready = false;
    participants[1].ready = false;
    participants.push_back({3, false, R::ConnectionState::Connected, false, {103}});
    players.push_back({3, 103, "Player 3", 2});
    D6R_REQUIRE(controller.updateReplicationLobby(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> admittedPayloads;
    D6R_REQUIRE(controller.restoreReplication(3, [&](auto payload) {
        admittedPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    const auto admittedStates = deliveredStates(admittedPayloads);
    D6R_REQUIRE(!admittedStates.empty());
    const auto &retained = admittedStates.back();
    const bool readinessFalse = std::all_of(retained.participants.begin(), retained.participants.end(),
            [](const auto &participant) { return !participant.ready; })
            && !controller.participantReady(1) && !controller.participantReady(2)
            && !controller.participantReady(3);
    const bool noAutoStart = controller.stage() == HostedMatchStage::Lobby && controller.match() == nullptr;
    const bool resultRetained = retained.result.available
            && retained.result.state == (interrupted ? "Interrupted" : "Completed");
    std::vector<Identity> scoreRows;
    for (const auto &row: retained.score.players) scoreRows.push_back(row.playerId);
    const bool newcomerExcluded = scoreRows == std::vector<Identity>({101, 102})
            && retained.score.ranking == std::vector<R::Identity>({101, 102});

    for (const Identity participant: {Identity{1}, Identity{2}, Identity{3}})
        D6R_REQUIRE(controller.setParticipantReady(participant, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    const auto freshStates = deliveredStates(payloads);
    const auto *fresh = lastPhase(freshStates, R::Phase::ActiveRound);
    const bool explicitStartReset = fresh && !fresh->result.available && fresh->result.serialized.empty()
            && fresh->matchId != retained.matchId;

    return "reconnect-false=" + std::string(reconnectKeptFalse ? "true" : "false")
            + ";connection-false=" + (connectionKeptFalse ? "true" : "false")
            + ";readiness-false=" + (readinessFalse ? "true" : "false")
            + ";no-autostart=" + (noAutoStart ? "true" : "false")
            + ";result-retained=" + (resultRetained ? "true" : "false")
            + ";newcomer-excluded=" + (newcomerExcluded ? "true" : "false")
            + ";explicit-reset=" + (explicitStartReset ? "true" : "false");
}

std::string followingLobbySettingsEvidence(bool interrupted) {
    auto requested = config();
    if (interrupted) requested.roundLimit = 3;
    const auto players = roster(2);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    if (interrupted) {
        D6R_REQUIRE_EQ(ActionResult::Accepted, controller.match()->submit(action(
                *controller.match(), sequence++, 1, 0, ActionKind::RemovePlayer, 102)));
    }
    D6R_REQUIRE(controller.observeMatchOutcome());

    const auto outcomeStates = deliveredStates(payloads);
    const auto *prior = lastPhase(outcomeStates, R::Phase::Lobby);
    D6R_REQUIRE(prior != nullptr && prior->result.available);
    const std::string priorSerialized = prior->result.serialized;
    const std::string priorState = prior->result.state;
    const auto priorRanking = prior->score.ranking;
    const auto priorWinner = prior->score.winner;
    const auto priorRound = prior->round;

    bool everyUpdatePublished = true;
    bool everySettingReplicated = true;
    bool resultAlwaysRetained = true;
    const std::vector<std::pair<Mode, std::uint8_t>> changes = {
            {Mode::TeamDeathmatch, 2},
            {Mode::TeamDeathmatch, 3},
            {Mode::Deathmatch, 0}};
    for (const auto &[mode, teamCount]: changes) {
        requested.mode = mode;
        requested.teamCount = teamCount;
        requested.friendlyFire = false;
        const std::size_t payloadCount = payloads.size();
        const bool published = controller.updateReplicationLobby(participants, players, requested);
        everyUpdatePublished = everyUpdatePublished && published && payloads.size() > payloadCount;
        if (!published || payloads.size() == payloadCount) {
            everySettingReplicated = false;
            resultAlwaysRetained = false;
            continue;
        }
        const auto states = deliveredStates(payloads);
        const auto &updated = states.back();
        everySettingReplicated = everySettingReplicated
                && updated.phase == R::Phase::Lobby
                && updated.settings.mode == modeName(mode)
                && updated.settings.teamCount == teamCount;
        const bool sameRound = updated.round.has_value() == priorRound.has_value()
                && (!priorRound || (updated.round->roundId == priorRound->roundId
                        && updated.round->rosterOrder == priorRound->rosterOrder
                        && updated.round->outcome.winnerPlayerIds == priorRound->outcome.winnerPlayerIds
                        && updated.round->outcome.winningTeam == priorRound->outcome.winningTeam
                        && updated.round->outcome.noWinner == priorRound->outcome.noWinner));
        resultAlwaysRetained = resultAlwaysRetained
                && updated.result.available
                && updated.result.state == priorState
                && updated.result.serialized == priorSerialized
                && updated.score.ranking == priorRanking
                && updated.score.winner.winnerPlayerIds == priorWinner.winnerPlayerIds
                && updated.score.winner.winningTeam == priorWinner.winningTeam
                && updated.score.winner.noWinner == priorWinner.noWinner
                && sameRound;
    }

    const auto beforeStartStates = deliveredStates(payloads);
    const bool retainedUntilStart = !beforeStartStates.empty() && beforeStartStates.back().result.available
            && beforeStartStates.back().result.serialized == priorSerialized;
    D6R_REQUIRE(controller.setParticipantReady(1, true));
    D6R_REQUIRE(controller.setParticipantReady(2, true));
    const bool started = controller.start(requested, players, manifest()).code == OutcomeCode::None;
    const auto afterStartStates = deliveredStates(payloads);
    const auto *nextMatch = lastPhase(afterStartStates, R::Phase::ActiveRound);
    const bool onlyStartClears = retainedUntilStart && started && nextMatch
            && !nextMatch->result.available && nextMatch->result.serialized.empty()
            && nextMatch->matchId != prior->matchId;

    return "published=" + std::string(everyUpdatePublished ? "true" : "false")
            + ";settings=" + (everySettingReplicated ? "true" : "false")
            + ";retained=" + (resultAlwaysRetained ? "true" : "false")
            + ";start-only-clear=" + (onlyStartClears ? "true" : "false");
}

D6R_TEST_CASE("AHM mode matrix completes with documented winner and team assignment") {
    struct Variant { Mode mode; std::uint8_t teams; bool friendlyFire; };
    const std::vector<Variant> variants = {
        {Mode::Deathmatch, 0, false}, {Mode::Predator, 0, false},
        {Mode::TeamDeathmatch, 2, false}, {Mode::TeamDeathmatch, 2, true},
        {Mode::TeamDeathmatch, 3, false}, {Mode::TeamDeathmatch, 3, true},
        {Mode::TeamDeathmatch, 4, false}, {Mode::TeamDeathmatch, 4, true}};
    for (const auto &variant: variants) {
        MatchConfig requested = config();
        requested.mode = variant.mode; requested.teamCount = variant.teams; requested.friendlyFire = variant.friendlyFire;
        auto players = roster();
        AuthoritativeMatch match;
        D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
        std::uint64_t sequence = 1;
        if (variant.mode == Mode::Predator) {
            const Identity predator = match.roundDecision().predatorPlayerId;
            auto target = std::find_if(players.begin(), players.end(), [&](const auto &p) { return p.playerId == predator; });
            auto source = std::find_if(players.begin(), players.end(), [&](const auto &p) { return p.playerId != predator; });
            eliminate(match, sequence, *source, *target, 4);
        } else {
            const auto &source = players.front();
            for (const auto &target: players) {
                if (target.playerId == source.playerId) continue;
                if (variant.mode == Mode::TeamDeathmatch && target.rosterOrder % variant.teams == 0) continue;
                eliminate(match, sequence, source, target);
            }
        }
        D6R_REQUIRE_EQ(MatchPhase::RoundEndActive, match.phase());
        finishDelay(match);
        D6R_REQUIRE_EQ(OutcomeCode::Completed, match.outcome().code);
        D6R_REQUIRE(match.publishedResult().has_value());
        const auto &result = *match.publishedResult();
        D6R_REQUIRE_EQ(1u, result.completedRounds);
        D6R_REQUIRE(!result.finalNoWinner);
        if (variant.mode == Mode::TeamDeathmatch) {
            D6R_REQUIRE_EQ(Team::Alpha, result.finalWinningTeam);
            D6R_REQUIRE_EQ(static_cast<std::size_t>(variant.teams), result.teams.size());
            for (const auto &row: result.players)
                D6R_REQUIRE_EQ(static_cast<Team>(row.rosterOrder % variant.teams + 1), row.team);
        }
        D6R_REQUIRE_EQ(OutcomeCode::Completed, match.shutdown().code);
        D6R_REQUIRE(match.resourcesReleased());
    }
}

D6R_TEST_CASE("AHM no-winner penalties assists friendly-fire and points are authoritative") {
    MatchConfig requested = config();
    requested.mode = Mode::TeamDeathmatch; requested.teamCount = 2; requested.friendlyFire = true;
    requested.assistance = true;
    auto players = roster();
    AuthoritativeMatch match;
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit(action(match, sequence++, 3, 103,
            ActionKind::ShotDamage, 102, 41)));
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit(action(match, sequence++, 1, 101,
            ActionKind::ShotDamage, 102, 59)));
    eliminate(match, sequence, players[0], players[2], 1); // Same-team kill: penalty.
    eliminate(match, sequence, players[0], players[3], 1);
    finishDelay(match);
    const auto &result = *match.publishedResult();
    const auto assistant = std::find_if(result.players.begin(), result.players.end(), [](const auto &row) {
        return row.playerId == 103;
    });
    D6R_REQUIRE(assistant != result.players.end());
    D6R_REQUIRE_EQ(1u, assistant->statistics.assists);
    D6R_REQUIRE_EQ(41u, assistant->statistics.assistedDamage);
    const auto teamKiller = std::find_if(result.players.begin(), result.players.end(), [](const auto &row) {
        return row.playerId == 101;
    });
    D6R_REQUIRE(teamKiller != result.players.end());
    D6R_REQUIRE_EQ(1u, teamKiller->statistics.penalties);
    D6R_REQUIRE_EQ(2u, teamKiller->statistics.kills);
    D6R_REQUIRE_EQ(1u, assistant->statistics.deaths);
    D6R_REQUIRE_EQ(0u, assistant->statistics.penalties);
    for (const auto &row: result.players)
        D6R_REQUIRE_EQ(static_cast<std::int64_t>(row.statistics.kills + row.statistics.wins
                + row.statistics.assists) - static_cast<std::int64_t>(row.statistics.penalties),
                row.statistics.totalPoints());
    D6R_REQUIRE_EQ(Team::Alpha, result.teams.front().team);

    MatchConfig noWinnerConfig = config();
    auto two = roster(2);
    CanonicalWorldSnapshot snapshot;
    snapshot.valid = true; snapshot.stateDigest = 1; snapshot.players = {{101, true, 100}, {102, true, 100}};
    MatchRuntimeDependencies dependencies;
    dependencies.worldSnapshot = [&] { return snapshot; };
    dependencies.worldTick = [&](Tick, bool) {
        snapshot.players[0].alive = false; snapshot.players[0].life = 0;
        snapshot.players[1].alive = false; snapshot.players[1].life = 0;
        snapshot.roundOver = true; ++snapshot.stateDigest;
        return true;
    };
    AuthoritativeMatch noWinner(dependencies);
    D6R_REQUIRE_EQ(OutcomeCode::None, noWinner.start(noWinnerConfig, two, manifest()).code);
    noWinner.advanceOneTick();
    finishDelay(noWinner);
    D6R_REQUIRE(noWinner.publishedResult()->finalNoWinner);
    D6R_REQUIRE(noWinner.publishedResult()->finalWinnerPlayerIds.empty());
}

D6R_TEST_CASE("AHM opponent environmental self and team deaths apply only their qualifying penalties") {
    const auto players = roster(2);
    auto complete = [&](ActionKind kind, Identity participant, Identity source, Identity target) {
        AuthoritativeMatch match;
        D6R_REQUIRE_EQ(OutcomeCode::None, match.start(config(), players, manifest()).code);
        D6R_REQUIRE_EQ(ActionResult::Accepted,
                match.submit({0, 1, participant, source, kind, target, 0, MaximumLife}));
        finishDelay(match);
        D6R_REQUIRE(match.publishedResult());
        return *match.publishedResult();
    };

    const auto opponent = complete(ActionKind::ShotDamage, 1, 101, 102);
    const auto opponentKiller = std::find_if(opponent.players.begin(), opponent.players.end(),
            [](const auto &row) { return row.playerId == 101; });
    const auto opponentVictim = std::find_if(opponent.players.begin(), opponent.players.end(),
            [](const auto &row) { return row.playerId == 102; });
    D6R_REQUIRE(opponentKiller != opponent.players.end());
    D6R_REQUIRE(opponentVictim != opponent.players.end());
    D6R_REQUIRE_EQ(1u, opponentKiller->statistics.kills);
    D6R_REQUIRE_EQ(0u, opponentKiller->statistics.penalties);
    D6R_REQUIRE_EQ(1u, opponentVictim->statistics.deaths);
    D6R_REQUIRE_EQ(0u, opponentVictim->statistics.penalties);

    const auto environmental = complete(ActionKind::EnvironmentalDamage, 2, 102, 102);
    const auto environmentalVictim = std::find_if(environmental.players.begin(), environmental.players.end(),
            [](const auto &row) { return row.playerId == 102; });
    D6R_REQUIRE(environmentalVictim != environmental.players.end());
    D6R_REQUIRE_EQ(1u, environmentalVictim->statistics.deaths);
    D6R_REQUIRE_EQ(1u, environmentalVictim->statistics.penalties);

    const auto suicide = complete(ActionKind::ShotDamage, 2, 102, 102);
    const auto selfVictim = std::find_if(suicide.players.begin(), suicide.players.end(),
            [](const auto &row) { return row.playerId == 102; });
    D6R_REQUIRE(selfVictim != suicide.players.end());
    D6R_REQUIRE_EQ(1u, selfVictim->statistics.deaths);
    D6R_REQUIRE_EQ(1u, selfVictim->statistics.penalties);
    D6R_REQUIRE_EQ(0u, selfVictim->statistics.kills);
}

D6R_TEST_CASE("AHM validates settings roster levels weapons scripts and round bounds before content") {
    const auto players = roster(2);
    MatchConfig requested = config();
    for (const std::uint8_t accepted: {std::uint8_t{1}, std::uint8_t{99}}) {
        requested.roundLimit = accepted;
        D6R_REQUIRE(validateMatchConfig(requested, players).valid);
    }
    for (const std::uint8_t rejected: {std::uint8_t{0}, std::uint8_t{100}}) {
        requested.roundLimit = rejected;
        D6R_REQUIRE(!validateMatchConfig(requested, players).valid);
    }
    requested = config(); requested.optionalScriptsEnabled = true;
    D6R_REQUIRE_EQ(std::string("optional-scripts"), validateMatchConfig(requested, players).diagnostic);
    requested = config(); requested.mode = Mode::TeamDeathmatch; requested.teamCount = 4;
    D6R_REQUIRE(validateMatchConfig(requested, players).valid); // Empty configured teams are allowed.
    auto duplicate = players; duplicate[1].playerId = duplicate[0].playerId;
    D6R_REQUIRE(!validateMatchConfig(config(), duplicate).valid);
    auto missingLevel = config(); missingLevel.fixedLevel = "levels/missing.json";
    D6R_REQUIRE_EQ(std::string("fixed-level-unavailable"), validateFrozenContent(missingLevel, manifest()).diagnostic);
    auto noWeapons = config(); noWeapons.enabledWeapons.clear();
    D6R_REQUIRE_EQ(std::string("weapons-unavailable"), validateFrozenContent(noWeapons, manifest()).diagnostic);
    auto scripted = manifest(); scripted.push_back({"profiles/p/script.lua", {}});
    std::sort(scripted.begin(), scripted.end(), [](const auto &a, const auto &b) { return a.logicalPath < b.logicalPath; });
    D6R_REQUIRE_EQ(std::string("script-content"), validateFrozenContent(config(), scripted).diagnostic);

    AuthoritativeMatch precedence;
    auto invalid = config(); invalid.roundLimit = 0; invalid.enabledWeapons.clear();
    D6R_REQUIRE_EQ(OutcomeCode::SettingsInvalid, precedence.start(invalid, players, {}).code);
    D6R_REQUIRE(!precedence.publishedResult());
}

D6R_TEST_CASE("AHM fixed shuffle random plans and every round decision replay deterministically") {
    auto run = [](LevelPlan plan) {
        MatchConfig requested = config(); requested.levelPlan = plan; requested.roundLimit = 6;
        if (plan != LevelPlan::Fixed) requested.fixedLevel.clear();
        AuthoritativeMatch match;
        auto players = roster(2);
        D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
        std::uint64_t sequence = 1;
        while (match.outcome().code == OutcomeCode::None) {
            if (match.phase() == MatchPhase::ActiveRound) eliminate(match, sequence, players[0], players[1]);
            else match.advanceOneTick();
        }
        D6R_REQUIRE(match.publishedResult());
        return *match.publishedResult();
    };
    for (LevelPlan plan: {LevelPlan::Fixed, LevelPlan::ShuffleAll, LevelPlan::Random}) {
        const auto first = run(plan), second = run(plan);
        D6R_REQUIRE_EQ(*serializeSessionResult(first), *serializeSessionResult(second));
        for (const auto &round: first.rounds)
            D6R_REQUIRE(std::find(first.config.playableLevels.begin(), first.config.playableLevels.end(), round.level)
                        != first.config.playableLevels.end());
    }
    const auto fixed = run(LevelPlan::Fixed);
    for (const auto &round: fixed.rounds) D6R_REQUIRE_EQ(std::string("levels/a.json"), round.level);
    const auto shuffled = run(LevelPlan::ShuffleAll);
    D6R_REQUIRE_EQ(shuffled.rounds[0].level, shuffled.rounds[3].level);
    D6R_REQUIRE_EQ(shuffled.rounds[1].level, shuffled.rounds[4].level);
    D6R_REQUIRE_EQ(shuffled.rounds[2].level, shuffled.rounds[5].level);
    D6R_REQUIRE(shuffled.rounds[0].level != shuffled.rounds[1].level);
    D6R_REQUIRE(shuffled.rounds[0].level != shuffled.rounds[2].level);
    D6R_REQUIRE(shuffled.rounds[1].level != shuffled.rounds[2].level);

    for (const LevelPlan plan: {LevelPlan::Fixed, LevelPlan::ShuffleAll, LevelPlan::Random}) {
        MatchConfig subset = config();
        subset.levelPlan = plan;
        subset.playableLevels = {"levels/a.json", "levels/b.json"};
        if (plan != LevelPlan::Fixed) subset.fixedLevel.clear();
        D6R_REQUIRE_EQ(std::string("playable-level-set"), validateFrozenContent(subset, manifest()).diagnostic);
    }
}

D6R_TEST_CASE("AHM departure preserves roster-derived team identity across rounds") {
    MatchConfig requested = config();
    requested.mode = Mode::TeamDeathmatch;
    requested.teamCount = 2;
    requested.roundLimit = 2;
    auto players = roster();
    AuthoritativeMatch match;
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;

    eliminate(match, sequence, players[0], players[1]);
    eliminate(match, sequence, players[0], players[3]);
    finishDelay(match);
    D6R_REQUIRE_EQ(MatchPhase::ActiveRound, match.phase());
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit(action(match, sequence++, 1, 0,
            ActionKind::RemovePlayer, players[1].playerId)));
    eliminate(match, sequence, players[0], players[3]);
    finishDelay(match);

    D6R_REQUIRE(match.publishedResult());
    const auto &result = *match.publishedResult();
    D6R_REQUIRE_EQ(2u, result.completedRounds);
    D6R_REQUIRE_EQ(2u, result.rounds.size());
    for (const auto &round: result.rounds)
        D6R_REQUIRE_EQ(std::vector<Identity>({101, 102, 103, 104}), round.rosterOrder);
    const auto departed = std::find_if(result.players.begin(), result.players.end(), [](const auto &row) {
        return row.playerId == 102;
    });
    D6R_REQUIRE(departed != result.players.end());
    D6R_REQUIRE(departed->departed);
    D6R_REQUIRE_EQ(Team::Bravo, departed->team);
    D6R_REQUIRE_EQ(1u, departed->rosterOrder);
    D6R_REQUIRE_EQ(2u, departed->rounds.size());
    D6R_REQUIRE_EQ(2u, result.teams.size());
}

D6R_TEST_CASE("AHM strict tick ordering ownership values bounds and advancement authority") {
    AuthoritativeMatch match;
    auto players = roster(2);
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(ActionResult::RejectedOrder, match.submit({1, 1, 1, 101, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedOrder, match.submit({0, 0, 1, 101, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedAuthority, match.submit({0, 1, 2, 101, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedValue, match.submit({0, 1, 1, 101, ActionKind::PlayerInput, 0, 1u << 20u, 0}));
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit({0, 1, 1, 101, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedOrder, match.submit({0, 1, 1, 101, ActionKind::PlayerInput, 0, MoveRight, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedValue, match.submit({0, 2, 1, 101, ActionKind::PlayerInput, 0, MoveRight, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedAuthority, match.submit({0, 2, 2, 0, ActionKind::AdvanceRound, 0, 0, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedPhase, match.submit({0, 2, 1, 0, ActionKind::AdvanceRound, 0, 0, 0}));
}

D6R_TEST_CASE("AHM unauthorized and malformed floods cannot progress fail or count as accepted") {
    AuthoritativeMatch match;
    const auto players = roster(2);
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(config(), players, manifest()).code);
    const Tick initialTick = match.currentTick();
    const MatchPhase initialPhase = match.phase();

    for (std::uint64_t sequence = 1; sequence <= 65; ++sequence)
        D6R_REQUIRE_EQ(ActionResult::RejectedAuthority,
                match.submit({initialTick, sequence, 2, 0, ActionKind::AdvanceRound, 0, 0, 0}));
    for (std::uint64_t sequence = 1; sequence <= 65; ++sequence)
        D6R_REQUIRE_EQ(ActionResult::RejectedValue,
                match.submit({initialTick, sequence, 2, 102, ActionKind::PlayerInput, 0, 1u << 20u, 0}));

    D6R_REQUIRE_EQ(initialTick, match.currentTick());
    D6R_REQUIRE_EQ(initialPhase, match.phase());
    D6R_REQUIRE_EQ(OutcomeCode::None, match.outcome().code);
    D6R_REQUIRE(!match.publishedResult());
    D6R_REQUIRE_EQ(UINT64_C(0), match.acceptedActionCount());
    D6R_REQUIRE_EQ(UINT64_C(130), match.rejectedActionCount());
    D6R_REQUIRE_EQ(ActionResult::Accepted,
            match.submit({initialTick, 1, 2, 102, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(UINT64_C(1), match.acceptedActionCount());
}

D6R_TEST_CASE("AHM sequence maxima remain per owner and cannot block host End") {
    AuthoritativeMatch match;
    const auto players = roster(2);
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(config(), players, manifest()).code);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();

    D6R_REQUIRE_EQ(ActionResult::Accepted,
            match.submit({0, maximum, 2, 102, ActionKind::PlayerInput, 0, MoveLeft, 0}));
    D6R_REQUIRE_EQ(ActionResult::Accepted,
            match.submit({0, 1, 1, 101, ActionKind::PlayerInput, 0, MoveRight, 0}));
    D6R_REQUIRE_EQ(ActionResult::RejectedOrder,
            match.submit({0, 1, 2, 102, ActionKind::PlayerInput, 0, MoveRight, 0}));
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submitHostControl(1, ActionKind::EndSession));

    D6R_REQUIRE_EQ(OutcomeCode::EndedIntentionally, match.outcome().code);
    D6R_REQUIRE_EQ(UINT64_C(3), match.acceptedActionCount());
    D6R_REQUIRE_EQ(UINT64_C(1), match.rejectedActionCount());
    D6R_REQUIRE(!match.publishedResult());
}

D6R_TEST_CASE("AHM round-end boundaries update exactly one second then freeze five seconds") {
    std::vector<bool> simulated;
    MatchRuntimeDependencies dependencies;
    dependencies.worldTick = [&](Tick, bool simulate) { simulated.push_back(simulate); return true; };
    AuthoritativeMatch match(dependencies);
    auto players = roster(2);
    MatchConfig requested = config(); requested.roundLimit = 3;
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(match, sequence, players[0], players[1]);
    for (std::uint32_t i = 0; i < RoundEndActiveTicks - 1; ++i) {
        match.advanceOneTick(); D6R_REQUIRE_EQ(MatchPhase::RoundEndActive, match.phase());
    }
    match.advanceOneTick();
    D6R_REQUIRE_EQ(MatchPhase::RoundEndFrozen, match.phase());
    D6R_REQUIRE_EQ(static_cast<std::size_t>(RoundEndActiveTicks), simulated.size());
    for (std::uint32_t i = RoundEndActiveTicks; i < RoundEndTotalTicks - 1; ++i) match.advanceOneTick();
    D6R_REQUIRE_EQ(MatchPhase::RoundEndFrozen, match.phase());
    match.advanceOneTick();
    D6R_REQUIRE_EQ(MatchPhase::ActiveRound, match.phase());

    eliminate(match, sequence, players[0], players[1]);
    D6R_REQUIRE_EQ(ActionResult::RejectedAuthority, match.submit(action(match, sequence++, 2, 0, ActionKind::AdvanceRound)));
    D6R_REQUIRE_EQ(ActionResult::Accepted, match.submit(action(match, sequence++, 1, 0, ActionKind::AdvanceRound)));
    D6R_REQUIRE_EQ(MatchPhase::ActiveRound, match.phase());
}

D6R_TEST_CASE("AHM terminal results are atomic and cleanup controls exit meaning") {
    auto players = roster(2);
    AuthoritativeMatch interrupted;
    D6R_REQUIRE_EQ(OutcomeCode::None, interrupted.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(ActionResult::Accepted, interrupted.submit({0, 1, 1, 0, ActionKind::RemovePlayer, 102, 0, 0}));
    D6R_REQUIRE_EQ(OutcomeCode::InterruptedNoWinner, interrupted.outcome().code);
    D6R_REQUIRE_EQ(0, interrupted.outcome().exitStatus);
    D6R_REQUIRE(interrupted.publishedResult()->finalNoWinner);
    D6R_REQUIRE_EQ(0u, interrupted.publishedResult()->completedRounds);

    AuthoritativeMatch ended;
    D6R_REQUIRE_EQ(OutcomeCode::None, ended.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(ActionResult::RejectedAuthority, ended.submit({0, 1, 2, 0, ActionKind::EndSession, 0, 0, 0}));
    D6R_REQUIRE_EQ(ActionResult::Accepted, ended.submit({0, 1, 1, 0, ActionKind::EndSession, 0, 0, 0}));
    D6R_REQUIRE(!ended.publishedResult());
    D6R_REQUIRE_EQ(0, ended.shutdown().exitStatus);

    MatchRuntimeDependencies badCleanup; badCleanup.cleanup = [] { return false; };
    AuthoritativeMatch cleanup(badCleanup);
    D6R_REQUIRE_EQ(OutcomeCode::None, cleanup.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(ActionResult::Accepted, cleanup.submit({0, 1, 1, 0, ActionKind::EndSession, 0, 0, 0}));
    D6R_REQUIRE_EQ(OutcomeCode::ShutdownFailed, cleanup.shutdown().code);
    D6R_REQUIRE_EQ(4, cleanup.outcome().exitStatus);
    D6R_REQUIRE(!cleanup.resourcesReleased());
    D6R_REQUIRE(!cleanup.publishedResult());

    AuthoritativeMatch failed;
    D6R_REQUIRE_EQ(OutcomeCode::None, failed.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(ActionResult::RuntimeFailed, failed.submit({0, 1, 1, 0, ActionKind::RuntimeFailure, 0, 0, 0}));
    D6R_REQUIRE_EQ(3, failed.outcome().exitStatus);
    D6R_REQUIRE(!failed.publishedResult());
}

D6R_TEST_CASE("REP-017 NET-AC-018 completed hosted match publishes final summary then cleared-readiness lobby and clears result on new match") {
    auto requested = config();
    const auto players = roster(2);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE(controller.currentSessionResult().has_value());
    D6R_REQUIRE(controller.currentSessionResult()->state == ResultState::Completed);

    const auto resultStates = deliveredStates(payloads);
    const auto *finalSummary = lastPhase(resultStates, R::Phase::FinalSummary);
    const auto *followingLobby = lastPhase(resultStates, R::Phase::Lobby);
    const bool finalRetained = finalSummary && finalSummary->result.available
                               && finalSummary->result.state == "Completed"
                               && finalSummary->score.winner.winnerPlayerIds == std::vector<R::Identity>{101};
    const bool lobbyRetained = followingLobby && followingLobby->result.available
                               && followingLobby->result.state == "Completed"
                               && followingLobby->score.winner.winnerPlayerIds == std::vector<R::Identity>{101};
    const std::string lifecycle = "final=" + std::string(finalRetained ? "true" : "false")
            + ";lobby=" + (lobbyRetained ? "true" : "false")
            + ";stage-lobby=" + (controller.stage() == HostedMatchStage::Lobby ? "true" : "false")
            + ";ready-cleared=" + (!controller.participantReady(1) && !controller.participantReady(2)
                                      ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("final=true;lobby=true;stage-lobby=true;ready-cleared=true"), lifecycle);

    D6R_REQUIRE(controller.setParticipantReady(1, true));
    D6R_REQUIRE_EQ(OutcomeCode::SettingsInvalid, controller.start(requested, players, manifest()).code);
    D6R_REQUIRE(controller.currentSessionResult().has_value());
    D6R_REQUIRE(controller.retainsCompletedResult());
    std::vector<std::vector<std::uint8_t>> rejectedStartPayloads;
    D6R_REQUIRE(controller.restoreReplication(2, [&](auto payload) {
        rejectedStartPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    const auto rejectedStartStates = deliveredStates(rejectedStartPayloads);
    D6R_REQUIRE(!rejectedStartStates.empty());
    D6R_REQUIRE(rejectedStartStates.back().result.available);
    D6R_REQUIRE_EQ(std::string("Completed"), rejectedStartStates.back().result.state);

    D6R_REQUIRE(controller.setParticipantReady(1, true));
    D6R_REQUIRE(controller.setParticipantReady(2, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    const auto newMatchStates = deliveredStates(payloads);
    const auto *newMatch = lastPhase(newMatchStates, R::Phase::ActiveRound);
    D6R_REQUIRE(newMatch != nullptr);
    D6R_REQUIRE(!newMatch->result.available);
    D6R_REQUIRE(newMatch->result.serialized.empty());
    D6R_REQUIRE(newMatch->matchId != finalSummary->matchId);
    D6R_REQUIRE(!controller.currentSessionResult().has_value());
}

D6R_TEST_CASE("REP-017 NET-AC-018 interrupted hosted match goes directly to cleared-readiness lobby with completed result retained") {
    auto requested = config();
    requested.roundLimit = 3;
    const auto players = roster(2);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE_EQ(ActionResult::Accepted,
            controller.match()->submit(action(*controller.match(), sequence++, 1, 0,
                                               ActionKind::RemovePlayer, 102)));
    D6R_REQUIRE_EQ(OutcomeCode::InterruptedNoWinner, controller.match()->outcome().code);
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE(controller.currentSessionResult().has_value());
    D6R_REQUIRE(controller.currentSessionResult()->state == ResultState::Interrupted);
    D6R_REQUIRE(controller.currentSessionResult()->finalNoWinner);

    const auto states = deliveredStates(payloads);
    const auto *followingLobby = lastPhase(states, R::Phase::Lobby);
    D6R_REQUIRE(followingLobby != nullptr);
    const bool retained = followingLobby->result.available
                          && followingLobby->result.state == "Interrupted"
                          && followingLobby->score.winner.noWinner
                          && followingLobby->completedRounds == 1
                          && followingLobby->round
                          && followingLobby->round->outcome.winnerPlayerIds == std::vector<R::Identity>{101};
    const bool replicatedReadinessCleared = std::all_of(
            followingLobby->participants.begin(), followingLobby->participants.end(),
            [](const auto &participant) { return !participant.ready; });
    const std::string lifecycle = "retained=" + std::string(retained ? "true" : "false")
            + ";stage-lobby=" + (controller.stage() == HostedMatchStage::Lobby ? "true" : "false")
            + ";controller-ready-cleared="
            + (!controller.participantReady(1) && !controller.participantReady(2) ? "true" : "false")
            + ";replicated-ready-cleared=" + (replicatedReadinessCleared ? "true" : "false");
    D6R_REQUIRE_EQ(std::string(
            "retained=true;stage-lobby=true;controller-ready-cleared=true;replicated-ready-cleared=true"), lifecycle);
}

D6R_TEST_CASE("REP-017 NET-AC-018 completed following lobby permits reconnect and admission while retaining result then resets a fresh match") {
    auto requested = config();
    auto players = roster(2);
    auto participants = std::vector<R::ParticipantState>{
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> hostPayloads;
    std::vector<std::vector<std::uint8_t>> guestPayloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        hostPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.restoreReplication(2, [&](auto payload) {
        guestPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE_EQ(HostedMatchStage::Lobby, controller.stage());

    controller.disconnectReplication(2);
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Reconnecting));
    D6R_REQUIRE(controller.restoreReplication(2, [&](auto payload) {
        guestPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Connected));

    participants[0].ready = false;
    participants[1].ready = false;
    participants.push_back({3, false, R::ConnectionState::Connected, false, {103}});
    players.push_back({3, 103, "Player 3", 2});
    D6R_REQUIRE(controller.updateReplicationLobby(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> admittedPayloads;
    D6R_REQUIRE(controller.restoreReplication(3, [&](auto payload) {
        admittedPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));

    const auto retainedStates = deliveredStates(admittedPayloads);
    D6R_REQUIRE(!retainedStates.empty());
    const auto &retained = retainedStates.back();
    D6R_REQUIRE(retained.phase == R::Phase::Lobby);
    D6R_REQUIRE(retained.result.available && retained.result.state == "Completed");
    D6R_REQUIRE_EQ(3u, retained.participants.size());
    D6R_REQUIRE(std::all_of(retained.participants.begin(), retained.participants.end(),
                            [](const auto &participant) { return !participant.ready; }));

    for (const Identity participant: {Identity{1}, Identity{2}, Identity{3}})
        D6R_REQUIRE(controller.setParticipantReady(participant, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    const auto nextStates = deliveredStates(hostPayloads);
    const auto *fresh = lastPhase(nextStates, R::Phase::ActiveRound);
    D6R_REQUIRE(fresh != nullptr);
    D6R_REQUIRE(!fresh->result.available && fresh->result.serialized.empty());
    D6R_REQUIRE(fresh->matchId != retained.matchId);
}

D6R_TEST_CASE("REP-017 NET-AC-018 interrupted following lobby permits reconnect and admission while retaining result then resets a fresh match") {
    auto requested = config();
    requested.roundLimit = 3;
    auto players = roster(2);
    auto participants = std::vector<R::ParticipantState>{
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.restoreReplication(2, [](auto) { return Duel6::Network::SendResult::Accepted; }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE_EQ(ActionResult::Accepted, controller.match()->submit(action(
            *controller.match(), sequence++, 1, 0, ActionKind::RemovePlayer, 102)));
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE_EQ(HostedMatchStage::Lobby, controller.stage());

    controller.disconnectReplication(2);
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Reconnecting));
    D6R_REQUIRE(controller.restoreReplication(2, [](auto) { return Duel6::Network::SendResult::Accepted; }));
    D6R_REQUIRE(controller.updateReplicationConnection(2, R::ConnectionState::Connected));

    participants[0].ready = false;
    participants[1].ready = false;
    participants.push_back({3, false, R::ConnectionState::Connected, false, {103}});
    players.push_back({3, 103, "Player 3", 2});
    D6R_REQUIRE(controller.updateReplicationLobby(participants, players, requested));
    const auto retainedStates = deliveredStates(payloads);
    D6R_REQUIRE(!retainedStates.empty());
    const auto &retained = retainedStates.back();
    D6R_REQUIRE(retained.result.available && retained.result.state == "Interrupted");
    D6R_REQUIRE(retained.completedRounds == 1 && retained.score.winner.noWinner);
    D6R_REQUIRE(std::all_of(retained.participants.begin(), retained.participants.end(),
                            [](const auto &participant) { return !participant.ready; }));

    for (const Identity participant: {Identity{1}, Identity{2}, Identity{3}})
        D6R_REQUIRE(controller.setParticipantReady(participant, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    const auto freshStates = deliveredStates(payloads);
    const auto *fresh = lastPhase(freshStates, R::Phase::ActiveRound);
    D6R_REQUIRE(fresh != nullptr);
    D6R_REQUIRE(!fresh->result.available && fresh->result.serialized.empty());
    D6R_REQUIRE(fresh->matchId != retained.matchId);
}

D6R_TEST_CASE("AHM-AC-029 REP-013 REP-017 completed following lobby keeps connection separate from readiness and prior ranking") {
    D6R_REQUIRE_EQ(std::string(
            "reconnect-false=true;connection-false=true;readiness-false=true;no-autostart=true;"
            "result-retained=true;newcomer-excluded=true;explicit-reset=true"),
            followingLobbyMembershipEvidence(false));
}

D6R_TEST_CASE("AHM-AC-029 REP-013 REP-017 interrupted following lobby keeps connection separate from readiness and prior ranking") {
    D6R_REQUIRE_EQ(std::string(
            "reconnect-false=true;connection-false=true;readiness-false=true;no-autostart=true;"
            "result-retained=true;newcomer-excluded=true;explicit-reset=true"),
            followingLobbyMembershipEvidence(true));
}

D6R_TEST_CASE("REP-017 completed following lobby publishes mode and team-count changes without clearing prior result") {
    D6R_REQUIRE_EQ(std::string("published=true;settings=true;retained=true;start-only-clear=true"),
            followingLobbySettingsEvidence(false));
}

D6R_TEST_CASE("REP-017 interrupted following lobby publishes mode and team-count changes without clearing prior result") {
    D6R_REQUIRE_EQ(std::string("published=true;settings=true;retained=true;start-only-clear=true"),
            followingLobbySettingsEvidence(true));
}

D6R_TEST_CASE("AHM-AC-020 REP-017 REP-025 cumulative tie-break order survives final lobby incrementals and reconnect") {
    const auto players = roster(4);
    auto requested = config();
    CanonicalWorldSnapshot snapshot;
    snapshot.valid = true;
    snapshot.stateDigest = 1;
    for (const auto &definition: players) {
        CanonicalPlayerSnapshot player;
        player.playerId = definition.playerId;
        player.rosterSlot = definition.rosterOrder;
        player.alive = true;
        player.life = MaximumLife;
        snapshot.players.push_back(player);
    }
    snapshot.players[0].statistics.kills = 1;
    snapshot.players[0].statistics.damage = 100;
    snapshot.players[1].statistics.wins = 1;
    snapshot.players[2].statistics.kills = 1;
    snapshot.players[2].statistics.damage = 200;
    snapshot.players[3].statistics.kills = 1;
    snapshot.players[3].statistics.damage = 200;
    for (const auto &player: snapshot.players) D6R_REQUIRE_EQ(1, player.statistics.totalPoints());

    MatchRuntimeDependencies dependencies;
    dependencies.worldSnapshot = [&] { return snapshot; };
    dependencies.worldTick = [&](Tick, bool) {
        for (auto &player: snapshot.players) { player.alive = false; player.life = 0; }
        snapshot.roundOver = true;
        ++snapshot.worldTick;
        ++snapshot.stateDigest;
        return true;
    };
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}},
            {3, false, R::ConnectionState::Connected, true, {103}},
            {4, false, R::ConnectionState::Connected, true, {104}}};
    AuthoritativeHostedMatchController controller(1, dependencies);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> incrementalPayloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        incrementalPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    D6R_REQUIRE(controller.match()->advanceOneTick());
    finishDelay(*controller.match());
    D6R_REQUIRE(controller.observeMatchOutcome());

    const auto incrementalStates = deliveredStates(incrementalPayloads);
    const auto *finalSummary = lastPhase(incrementalStates, R::Phase::FinalSummary);
    const auto *followingLobby = lastPhase(incrementalStates, R::Phase::Lobby);
    std::vector<std::vector<std::uint8_t>> reconnectPayloads;
    D6R_REQUIRE(controller.restoreReplication(2, [&](auto payload) {
        reconnectPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    const auto reconnectStates = deliveredStates(reconnectPayloads);
    D6R_REQUIRE(!reconnectStates.empty());
    const std::vector<R::Identity> expected{102, 103, 104, 101};
    const auto rows = [](const R::CanonicalState *state) {
        std::vector<R::Identity> result;
        if (state) for (const auto &row: state->score.players) result.push_back(row.playerId);
        return result;
    };
    const bool finalCorrect = finalSummary && finalSummary->score.ranking == expected
            && rows(finalSummary) == expected;
    const bool lobbyCorrect = followingLobby && followingLobby->score.ranking == expected
            && rows(followingLobby) == expected;
    const bool reconnectCorrect = reconnectStates.back().score.ranking == expected
            && rows(&reconnectStates.back()) == expected;
    const std::string evidence = "final=" + std::string(finalCorrect ? "true" : "false")
            + ";lobby=" + (lobbyCorrect ? "true" : "false")
            + ";incremental=" + (finalSummary && followingLobby ? "true" : "false")
            + ";reconnect=" + (reconnectCorrect ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("final=true;lobby=true;incremental=true;reconnect=true"), evidence);
}

D6R_TEST_CASE("AHM REP-017 completed hosted cleanup failure is status 4 and publishes no retained result") {
    auto players = roster(2);
    auto requested = config();
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    MatchRuntimeDependencies dependencies;
    dependencies.cleanup = [] { return false; };
    AuthoritativeHostedMatchController controller(1, dependencies);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    const bool observed = controller.observeMatchOutcome();
    const auto states = deliveredStates(payloads);
    const bool resultReplicated = std::any_of(states.begin(), states.end(),
            [](const auto &state) { return state.result.available; });
    D6R_REQUIRE(controller.match() != nullptr);
    const std::string evidence = "observed=" + std::string(observed ? "true" : "false")
            + ";unexpected=" + (controller.stage() == HostedMatchStage::UnexpectedStop ? "true" : "false")
            + ";status=" + std::to_string(controller.match()->outcome().exitStatus)
            + ";released=" + (controller.match()->resourcesReleased() ? "true" : "false")
            + ";retained=" + (controller.match()->publishedResult() ? "true" : "false")
            + ";replicated=" + (resultReplicated ? "true" : "false")
            + ";intentional=" + (controller.match()->outcome().code == OutcomeCode::EndedIntentionally
                                   ? "true" : "false");
    D6R_REQUIRE_EQ(std::string(
            "observed=false;unexpected=true;status=4;released=false;retained=false;replicated=false;intentional=false"),
            evidence);
    D6R_REQUIRE(!controller.currentSessionResult().has_value());
}

D6R_TEST_CASE("AHM REP-017 interrupted hosted cleanup failure is status 4 and publishes no retained result") {
    auto players = roster(2);
    auto requested = config();
    requested.roundLimit = 3;
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    MatchRuntimeDependencies dependencies;
    dependencies.cleanup = [] { return false; };
    AuthoritativeHostedMatchController controller(1, dependencies);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE_EQ(ActionResult::Accepted, controller.match()->submit(action(
            *controller.match(), sequence++, 1, 0, ActionKind::RemovePlayer, 102)));
    const bool observed = controller.observeMatchOutcome();
    const auto states = deliveredStates(payloads);
    const bool resultReplicated = std::any_of(states.begin(), states.end(),
            [](const auto &state) { return state.result.available; });
    D6R_REQUIRE(controller.match() != nullptr);
    const std::string evidence = "observed=" + std::string(observed ? "true" : "false")
            + ";unexpected=" + (controller.stage() == HostedMatchStage::UnexpectedStop ? "true" : "false")
            + ";status=" + std::to_string(controller.match()->outcome().exitStatus)
            + ";released=" + (controller.match()->resourcesReleased() ? "true" : "false")
            + ";retained=" + (controller.match()->publishedResult() ? "true" : "false")
            + ";replicated=" + (resultReplicated ? "true" : "false")
            + ";intentional=" + (controller.match()->outcome().code == OutcomeCode::EndedIntentionally
                                   ? "true" : "false");
    D6R_REQUIRE_EQ(std::string(
            "observed=false;unexpected=true;status=4;released=false;retained=false;replicated=false;intentional=false"),
            evidence);
    D6R_REQUIRE(!controller.currentSessionResult().has_value());
}

D6R_TEST_CASE("AHM hosted End authorization is bound only to the frozen host identity") {
    const auto players = roster(2);
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE(controller.setParticipantReady(1, true));
    D6R_REQUIRE(controller.setParticipantReady(2, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(config(), players, manifest()).code);
    D6R_REQUIRE_EQ(HostedMatchStage::MatchActive, controller.stage());
    D6R_REQUIRE_EQ(OutcomeCode::SettingsInvalid, controller.end(2).code);
    D6R_REQUIRE_EQ(HostedMatchStage::MatchActive, controller.stage());
    D6R_REQUIRE_EQ(OutcomeCode::EndedIntentionally, controller.end(1).code);
    D6R_REQUIRE_EQ(HostedMatchStage::Ended, controller.stage());
    D6R_REQUIRE(!controller.currentSessionResult().has_value());
}

D6R_TEST_CASE("REP-017 final-summary and following-lobby departures preserve completion and mark result rows") {
    const auto departedResultRow = [](const std::string &serialized, Identity participantId) {
        const auto start = serialized.find("\"participantId\":" + std::to_string(participantId));
        if (start == std::string::npos) return false;
        const auto end = serialized.find('}', start);
        const auto departed = serialized.find("\"departed\":true", start);
        return departed != std::string::npos && (end == std::string::npos || departed < end);
    };
    auto requested = config();
    const auto players = roster(3);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}},
            {3, false, R::ConnectionState::Connected, true, {103}}};
    AuthoritativeMatch match;
    D6R_REQUIRE_EQ(OutcomeCode::None, match.start(requested, players, manifest()).code);
    AuthoritativeReplication replication(9001);
    D6R_REQUIRE(replication.setLobby(1, participants, players, requested));
    D6R_REQUIRE(replication.beginMatch(match).has_value());
    std::uint64_t sequence = 1;
    eliminate(match, sequence, players[0], players[1]);
    eliminate(match, sequence, players[0], players[2]);
    finishDelay(match);
    D6R_REQUIRE(match.publishedResult().has_value());
    D6R_REQUIRE(match.publishedResult()->state == ResultState::Completed);
    const auto originalGuest = std::find_if(match.publishedResult()->players.begin(),
            match.publishedResult()->players.end(), [](const auto &row) { return row.participantId == 2; });
    D6R_REQUIRE(originalGuest != match.publishedResult()->players.end());
    D6R_REQUIRE(!originalGuest->departed);
    D6R_REQUIRE(replication.capture(match).has_value());
    D6R_REQUIRE(replication.retainsCompletedResult());

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply(*replication.fullSnapshot()) == R::ApplyResult::Applied);
    const auto finalDeparture = replication.markResultParticipantsDeparted({2});
    D6R_REQUIRE(finalDeparture.has_value());
    D6R_REQUIRE(client.apply(*finalDeparture) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state()->phase == R::Phase::FinalSummary);
    D6R_REQUIRE(client.state()->result.state == "Completed");
    D6R_REQUIRE(departedResultRow(client.state()->result.serialized, 2));
    const auto departedFinal = std::find_if(client.state()->players.begin(), client.state()->players.end(),
            [](const auto &player) { return player.ownerParticipantId == 2; });
    D6R_REQUIRE(departedFinal != client.state()->players.end());
    D6R_REQUIRE(departedFinal->lifeState == R::LifeState::Departed);

    const auto followingLobby = replication.enterFollowingLobby();
    D6R_REQUIRE(followingLobby.has_value());
    D6R_REQUIRE(client.apply(*followingLobby) == R::ApplyResult::Applied);
    const auto lobbyDeparture = replication.markResultParticipantsDeparted({3});
    D6R_REQUIRE(lobbyDeparture.has_value());
    D6R_REQUIRE(client.apply(*lobbyDeparture) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state()->phase == R::Phase::Lobby);
    D6R_REQUIRE(client.state()->result.state == "Completed");
    D6R_REQUIRE(departedResultRow(client.state()->result.serialized, 2));
    D6R_REQUIRE(departedResultRow(client.state()->result.serialized, 3));
    const auto departedLobby = std::find_if(client.state()->players.begin(), client.state()->players.end(),
            [](const auto &player) { return player.ownerParticipantId == 3; });
    D6R_REQUIRE(departedLobby != client.state()->players.end());
    D6R_REQUIRE(departedLobby->lifeState == R::LifeState::Departed);
}

D6R_TEST_CASE("REP-013 following-lobby removal clears all lifecycle and replicated readiness") {
    auto requested = config();
    const auto players = roster(3);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}},
            {3, false, R::ConnectionState::Connected, true, {103}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    eliminate(*controller.match(), sequence, players[0], players[2]);
    finishDelay(*controller.match());
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE(controller.stage() == HostedMatchStage::Lobby);
    D6R_REQUIRE(controller.retainsCompletedResult());
    D6R_REQUIRE(controller.setParticipantReady(1, true));
    D6R_REQUIRE(controller.setParticipantReady(2, true));
    D6R_REQUIRE(controller.setParticipantReady(3, true));
    payloads.clear();

    D6R_REQUIRE(controller.removeLifecycleParticipants({2}));
    D6R_REQUIRE(!controller.participantReady(1));
    D6R_REQUIRE(!controller.participantReady(2));
    D6R_REQUIRE(!controller.participantReady(3));
    const auto states = deliveredStates(payloads);
    D6R_REQUIRE(!states.empty());
    D6R_REQUIRE(std::all_of(states.back().participants.begin(), states.back().participants.end(),
            [](const auto &participant) { return !participant.ready; }));
    D6R_REQUIRE_EQ(std::string("Completed"), states.back().result.state);
}

D6R_TEST_CASE("REP-013 REP-017 post-result newcomer Leave and expiry preserve result and continue session") {
    const auto verify = [](bool expire) {
        auto requested = config();
        auto players = roster(2);
        std::vector<R::ParticipantState> participants = {
                {1, true, R::ConnectionState::Connected, true, {101}},
                {2, false, R::ConnectionState::Connected, true, {102}}};
        AuthoritativeHostedMatchController controller(1);
        D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
        std::vector<std::vector<std::uint8_t>> payloads;
        D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
            payloads.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
        std::uint64_t sequence = 1;
        eliminate(*controller.match(), sequence, players[0], players[1]);
        finishDelay(*controller.match());
        D6R_REQUIRE(controller.observeMatchOutcome());
        D6R_REQUIRE(controller.stage() == HostedMatchStage::Lobby);
        D6R_REQUIRE(controller.retainsCompletedResult());

        participants.push_back({4, false, R::ConnectionState::Connected, false, {104}});
        players.push_back({4, 104, "Newcomer", 2});
        D6R_REQUIRE(controller.updateReplicationLobby(participants, players, requested));
        D6R_REQUIRE(controller.setParticipantReady(1, true));
        D6R_REQUIRE(controller.setParticipantReady(2, true));
        D6R_REQUIRE(controller.setParticipantReady(4, true));
        auto states = deliveredStates(payloads);
        D6R_REQUIRE(!states.empty());
        const std::string retainedResult = states.back().result.serialized;
        D6R_REQUIRE(retainedResult.find("\"participantId\":4") == std::string::npos);

        Duel6::Network::Lifecycle::TimePoint now{};
        std::uint8_t credentialSeed = 1;
        Duel6::Network::Lifecycle::HostHooks hooks;
        hooks.disconnect = [](auto) { return true; };
        hooks.removeBatch = [&](const std::vector<std::uint64_t> &removed,
                                Duel6::Network::Lifecycle::Phase phase) {
            D6R_REQUIRE_EQ((std::vector<std::uint64_t>{4}), removed);
            D6R_REQUIRE(phase == Duel6::Network::Lifecycle::Phase::FinalSummary);
            return controller.removeLifecycleParticipants(removed);
        };
        Duel6::Network::Lifecycle::HostSessionLifecycle lifecycle(
                9001, 1, 10, {101}, [&] { return now; },
                [&](std::uint8_t *target, std::size_t size) {
                    for (std::size_t index = 0; index < size; ++index)
                        target[index] = static_cast<std::uint8_t>(credentialSeed + index);
                    ++credentialSeed;
                    return true;
                }, hooks);
        D6R_REQUIRE(lifecycle.admitGuest(2, 20, {102}, false).has_value());
        D6R_REQUIRE(lifecycle.admitGuest(4, 40, {104}, false).has_value());
        D6R_REQUIRE(lifecycle.setReady(1, 10, true));
        D6R_REQUIRE(lifecycle.setReady(2, 20, true));
        D6R_REQUIRE(lifecycle.setReady(4, 40, true));
        if (expire) {
            D6R_REQUIRE(lifecycle.transportClosed(4, 40));
            now += std::chrono::seconds(30);
        } else {
            D6R_REQUIRE(lifecycle.queueIntentionalLeave(4, 40));
        }
        D6R_REQUIRE(lifecycle.processLifecycleBatch(Duel6::Network::Lifecycle::Phase::FinalSummary)
                    == Duel6::Network::Lifecycle::RemovalOutcome::FinalSummaryRetained);
        D6R_REQUIRE(!lifecycle.ended());
        D6R_REQUIRE_EQ(2u, lifecycle.retainedPlayerCount());
        D6R_REQUIRE(!lifecycle.ready(1) && !lifecycle.ready(2) && !lifecycle.ready(4));
        D6R_REQUIRE(controller.stage() == HostedMatchStage::Lobby);
        D6R_REQUIRE(controller.retainsCompletedResult());
        D6R_REQUIRE(!controller.participantReady(1));
        D6R_REQUIRE(!controller.participantReady(2));
        D6R_REQUIRE(!controller.participantReady(4));
        states = deliveredStates(payloads);
        D6R_REQUIRE_EQ(retainedResult, states.back().result.serialized);
        D6R_REQUIRE(std::all_of(states.back().participants.begin(), states.back().participants.end(),
                [](const auto &participant) { return !participant.ready; }));
    };

    verify(false);
    verify(true);
}

D6R_TEST_CASE("AHM malformed first and later frozen levels block before rounds clear readiness and permit End") {
    struct Case { LevelPlan plan; std::string malformedLevel; };
    for (const Case &test: {Case{LevelPlan::Fixed, "levels/a.json"},
                            Case{LevelPlan::ShuffleAll, "levels/b.json"}}) {
        int preflightCalls = 0;
        int worldStarts = 0;
        MatchRuntimeDependencies dependencies;
        dependencies.contentPreflight = [&](const Duel6::Network::GameplayManifest &frozen) {
            ++preflightCalls;
            return std::none_of(frozen.begin(), frozen.end(), [&](const auto &entry) {
                return entry.logicalPath == test.malformedLevel;
            });
        };
        dependencies.worldStart = [&](RoundStartDecision &) { ++worldStarts; return true; };
        AuthoritativeHostedMatchController controller(1, dependencies);
        const auto players = roster(2);
        MatchConfig requested = config(); requested.levelPlan = test.plan; requested.roundLimit = 3;
        if (test.plan != LevelPlan::Fixed) requested.fixedLevel.clear();

        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE(controller.setParticipantReady(1, true));
        D6R_REQUIRE(controller.setParticipantReady(2, true));
        D6R_REQUIRE_EQ(OutcomeCode::ContentUnavailable, controller.start(requested, players, manifest()).code);
        D6R_REQUIRE_EQ(HostedMatchStage::ContentBlocked, controller.stage());
        D6R_REQUIRE(controller.contentStartBlocked());
        D6R_REQUIRE(!controller.participantReady(1));
        D6R_REQUIRE(!controller.participantReady(2));
        D6R_REQUIRE(controller.match() == nullptr);
        D6R_REQUIRE_EQ(1, preflightCalls);
        D6R_REQUIRE_EQ(0, worldStarts);

        D6R_REQUIRE(!controller.setParticipantReady(1, true));
        D6R_REQUIRE_EQ(OutcomeCode::ContentUnavailable, controller.start(requested, players, manifest()).code);
        D6R_REQUIRE_EQ(1, preflightCalls);
        D6R_REQUIRE_EQ(0, worldStarts);
        D6R_REQUIRE_EQ(OutcomeCode::EndedIntentionally, controller.end(1).code);
        D6R_REQUIRE_EQ(HostedMatchStage::Ended, controller.stage());
    }
}

D6R_TEST_CASE("AHM canonical JSON is stable escaped bounded and excludes persistence and secrets") {
    SessionResult result;
    result.config = config(); result.completedRounds = 1;
    result.rounds.push_back({1, "levels/a.json", true, {101}, Team::None, false, {101, 102}});
    PlayerResultRow player; player.playerId = 101; player.participantId = 1;
    player.displayName = "quote\" slash\\ line\n"; player.rounds.push_back({});
    result.players.push_back(player); result.finalWinnerPlayerIds = {101};
    const auto first = serializeSessionResult(result);
    const auto second = serializeSessionResult(result);
    D6R_REQUIRE(first && second); D6R_REQUIRE_EQ(*first, *second);
    D6R_REQUIRE(first->find("quote\\\" slash\\\\ line\\n") != std::string::npos);
    for (const std::string forbidden: {"credential", "endpoint", "password", "elo", "history", "personRecord"})
        D6R_REQUIRE(first->find(forbidden) == std::string::npos);
    result.rounds.resize(100);
    result.completedRounds = 100;
    D6R_REQUIRE(!serializeSessionResult(result));
}

D6R_TEST_CASE("NET-AC-018 result retention accepts one exact terminal result per match and rejects replay") {
    NetworkMatchResultRetention retention;
    D6R_REQUIRE(!NetworkMatchResultRetention::persistenceEligible());
    D6R_REQUIRE(!retention.current().has_value());

    const auto firstGeneration = retention.beginMatch();
    D6R_REQUIRE(firstGeneration.has_value());
    SessionResult completed;
    completed.config = config();
    completed.completedRounds = 1;
    completed.rounds.push_back({1, "levels/a.json", false, {101}, Team::None, false, {101, 102}});
    PlayerResultRow winner;
    winner.playerId = 101;
    winner.participantId = 1;
    winner.displayName = "Winner";
    winner.rosterOrder = 0;
    winner.statistics.wins = 1;
    winner.rounds.push_back(winner.statistics);
    completed.players.push_back(winner);
    completed.finalWinnerPlayerIds = {101};

    D6R_REQUIRE(retention.retain(*firstGeneration, completed) == ResultRetentionStatus::Retained);
    D6R_REQUIRE(retention.current().has_value());
    D6R_REQUIRE_EQ(std::string("Winner"), retention.current()->players.front().displayName);
    D6R_REQUIRE(retention.retain(*firstGeneration, completed) == ResultRetentionStatus::Duplicate);

    SessionResult conflicting = completed;
    conflicting.players.front().displayName = "Forged replacement";
    D6R_REQUIRE(retention.retain(*firstGeneration, conflicting) == ResultRetentionStatus::Rejected);
    D6R_REQUIRE_EQ(std::string("Winner"), retention.current()->players.front().displayName);
    D6R_REQUIRE(retention.retain(0, completed) == ResultRetentionStatus::Rejected);

    const auto secondGeneration = retention.beginMatch();
    D6R_REQUIRE(secondGeneration.has_value());
    D6R_REQUIRE(*secondGeneration != *firstGeneration);
    D6R_REQUIRE(!retention.current().has_value());
    D6R_REQUIRE(retention.retain(*firstGeneration, completed) == ResultRetentionStatus::Rejected);
    D6R_REQUIRE(retention.retain(*secondGeneration, completed) == ResultRetentionStatus::Retained);

    retention.discard();
    retention.discard();
    D6R_REQUIRE(!retention.current().has_value());
    D6R_REQUIRE(retention.retain(*secondGeneration, completed) == ResultRetentionStatus::Rejected);
}

D6R_TEST_CASE("AHM-AC-015 AHM-AC-023 completed hosted result is session-only until host End discards it") {
    const auto players = roster(2);
    const std::vector<R::ParticipantState> participants = {
            {1, true, R::ConnectionState::Connected, true, {101}},
            {2, false, R::ConnectionState::Connected, true, {102}}};
    AuthoritativeHostedMatchController controller(1);
    D6R_REQUIRE(!controller.resultsPersistenceEligible());
    D6R_REQUIRE(controller.initializeReplication(participants, players, config()));
    D6R_REQUIRE(controller.markServiceReady());
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(config(), players, manifest()).code);

    std::uint64_t sequence = 1;
    eliminate(*controller.match(), sequence, players[0], players[1]);
    finishDelay(*controller.match());
    D6R_REQUIRE(controller.observeMatchOutcome());
    D6R_REQUIRE(controller.currentSessionResult().has_value());
    D6R_REQUIRE(controller.currentSessionResult()->state == ResultState::Completed);
    D6R_REQUIRE_EQ(std::string("Session only"), controller.currentSessionResult()->label);
    D6R_REQUIRE_EQ(1u, controller.currentSessionResult()->completedRounds);

    D6R_REQUIRE_EQ(OutcomeCode::EndedIntentionally, controller.end(1).code);
    D6R_REQUIRE(controller.stage() == HostedMatchStage::Ended);
    D6R_REQUIRE(!controller.currentSessionResult().has_value());
    D6R_REQUIRE(!controller.retainsCompletedResult());
    std::vector<std::vector<std::uint8_t>> payloads;
    D6R_REQUIRE(!controller.restoreReplication(3, [&](auto payload) {
        payloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    D6R_REQUIRE(payloads.empty());
}

D6R_TEST_CASE("NET-AC-013 NET-AC-018 completed and interrupted following-lobby departures synchronize retained and replicated results") {
    const auto verify = [](bool interrupted) {
        auto requested = config();
        if (interrupted) requested.roundLimit = 3;
        auto players = roster(interrupted ? 2 : 3);
        std::vector<R::ParticipantState> participants = {
                {1, true, R::ConnectionState::Connected, true, {101}},
                {2, false, R::ConnectionState::Connected, true, {102}}};
        if (!interrupted)
            participants.push_back({3, false, R::ConnectionState::Connected, true, {103}});
        AuthoritativeHostedMatchController controller(1);
        D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
        std::vector<std::vector<std::uint8_t>> payloads;
        D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
            payloads.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
        std::uint64_t sequence = 1;
        eliminate(*controller.match(), sequence, players[0], players[1]);
        if (!interrupted) eliminate(*controller.match(), sequence, players[0], players[2]);
        finishDelay(*controller.match());
        if (interrupted) {
            D6R_REQUIRE_EQ(ActionResult::Accepted, controller.match()->submit(action(
                    *controller.match(), sequence++, 1, 0, ActionKind::RemovePlayer, 102)));
            D6R_REQUIRE_EQ(OutcomeCode::InterruptedNoWinner, controller.match()->outcome().code);
        }
        D6R_REQUIRE(controller.observeMatchOutcome());
        D6R_REQUIRE(controller.currentSessionResult().has_value());
        D6R_REQUIRE(controller.currentSessionResult()->state
                    == (interrupted ? ResultState::Interrupted : ResultState::Completed));

        const Identity departure = interrupted ? 3 : 2;
        const auto beforeDeparture = serializeSessionResult(*controller.currentSessionResult());
        D6R_REQUIRE(beforeDeparture.has_value());
        if (interrupted) {
            participants[0].ready = false;
            participants[1].ready = false;
            participants.push_back({3, false, R::ConnectionState::Connected, false, {103}});
            players.push_back({3, 103, "Newcomer", 2});
            D6R_REQUIRE(controller.updateReplicationLobby(participants, players, requested));
            for (const Identity participant: {Identity{1}, Identity{2}, Identity{3}})
                D6R_REQUIRE(controller.setParticipantReady(participant, true));
        }
        payloads.clear();
        D6R_REQUIRE(controller.removeLifecycleParticipants({departure}));
        D6R_REQUIRE(controller.currentSessionResult().has_value());
        const auto retainedRow = std::find_if(controller.currentSessionResult()->players.begin(),
                controller.currentSessionResult()->players.end(), [departure](const auto &row) {
                    return row.participantId == departure;
                });
        if (interrupted) {
            D6R_REQUIRE(retainedRow == controller.currentSessionResult()->players.end());
            D6R_REQUIRE_EQ(*beforeDeparture, *serializeSessionResult(*controller.currentSessionResult()));
        } else {
            D6R_REQUIRE(retainedRow != controller.currentSessionResult()->players.end());
            D6R_REQUIRE(retainedRow->departed);
        }

        const auto states = deliveredStates(payloads);
        D6R_REQUIRE(!states.empty());
        const auto &replicated = states.back();
        D6R_REQUIRE(replicated.phase == R::Phase::Lobby);
        D6R_REQUIRE(replicated.result.available);
        D6R_REQUIRE_EQ(std::string(interrupted ? "Interrupted" : "Completed"), replicated.result.state);
        if (interrupted) {
            D6R_REQUIRE_EQ(*beforeDeparture, replicated.result.serialized);
            D6R_REQUIRE(std::all_of(replicated.participants.begin(), replicated.participants.end(),
                    [](const auto &participant) { return !participant.ready; }));
            return;
        }
        const auto serializedDeparture = replicated.result.serialized.find(
                "\"participantId\":" + std::to_string(departure));
        D6R_REQUIRE(serializedDeparture != std::string::npos);
        const auto serializedRowEnd = replicated.result.serialized.find('}', serializedDeparture);
        const auto departed = replicated.result.serialized.find("\"departed\":true", serializedDeparture);
        D6R_REQUIRE(departed != std::string::npos
                    && (serializedRowEnd == std::string::npos || departed < serializedRowEnd));
    };

    verify(false);
    verify(true);
}

void verifyInterruptedRetainedResultLifecycleDeparture(bool authoritativeExpiry) {
        auto requested = config();
        requested.roundLimit = 3;
        const auto players = roster(2);
        const std::vector<R::ParticipantState> participants = {
                {1, true, R::ConnectionState::Connected, true, {101}},
                {2, false, R::ConnectionState::Connected, true, {102}}};
        AuthoritativeHostedMatchController controller(1);
        D6R_REQUIRE(controller.initializeReplication(participants, players, requested));
        std::vector<std::vector<std::uint8_t>> incrementals;
        D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
            incrementals.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
        std::uint64_t sequence = 1;
        eliminate(*controller.match(), sequence, players[0], players[1]);
        finishDelay(*controller.match());
        D6R_REQUIRE(controller.stage() == HostedMatchStage::MatchActive);

        Duel6::Network::Lifecycle::TimePoint now{};
        std::uint8_t credentialSeed = 1;
        unsigned removalCalls = 0;
        Duel6::Network::Lifecycle::HostHooks hooks;
        hooks.disconnect = [&](Identity participantId) {
            return controller.updateReplicationConnection(participantId, R::ConnectionState::Reconnecting);
        };
        hooks.removeBatch = [&](const std::vector<std::uint64_t> &removed,
                                Duel6::Network::Lifecycle::Phase phase) {
            ++removalCalls;
            D6R_REQUIRE_EQ((std::vector<std::uint64_t>{2}), removed);
            D6R_REQUIRE(phase == Duel6::Network::Lifecycle::Phase::ActiveRound);
            return controller.removeLifecycleParticipants(removed);
        };
        Duel6::Network::Lifecycle::HostSessionLifecycle lifecycle(
                9001, 1, 10, {101}, [&] { return now; },
                [&](std::uint8_t *target, std::size_t size) {
                    for (std::size_t index = 0; index < size; ++index)
                        target[index] = static_cast<std::uint8_t>(credentialSeed + index);
                    ++credentialSeed;
                    return true;
                }, hooks);
        D6R_REQUIRE(lifecycle.admitGuest(2, 20, {102}, true).has_value());
        if (!authoritativeExpiry) {
            D6R_REQUIRE(lifecycle.applyParticipantAction(
                    {9001, 2, Duel6::Network::Lifecycle::ParticipantActionKind::Leave}, 20));
        } else {
            D6R_REQUIRE(lifecycle.transportClosed(2, 20));
            now += Duel6::Network::Lifecycle::ReconnectWindow;
        }
        const auto lifecycleOutcome = lifecycle.processLifecycleBatch(
                Duel6::Network::Lifecycle::Phase::ActiveRound);
        const std::string transitionEvidence = "outcome="
                + std::to_string(static_cast<unsigned>(lifecycleOutcome))
                + ";removals=" + std::to_string(removalCalls)
                + ";lifecycle-ended=" + (lifecycle.ended() ? "true" : "false")
                + ";stage-lobby=" + (controller.stage() == HostedMatchStage::Lobby ? "true" : "false")
                + ";retained=" + (controller.currentSessionResult() ? "true" : "false")
                + ";state=" + (controller.currentSessionResult()
                                ? (controller.currentSessionResult()->state == ResultState::Interrupted
                                   ? "Interrupted" : "Completed") : "None");
        D6R_REQUIRE_EQ(std::string(
                "outcome=3;removals=1;lifecycle-ended=false;stage-lobby=true;retained=true;state=Interrupted"),
                transitionEvidence);

        D6R_REQUIRE_EQ(1u, removalCalls);
        D6R_REQUIRE(!lifecycle.ended());
        D6R_REQUIRE_EQ(1u, lifecycle.retainedPlayerCount());
        D6R_REQUIRE(controller.stage() == HostedMatchStage::Lobby);
        D6R_REQUIRE(controller.currentSessionResult().has_value());
        D6R_REQUIRE(controller.currentSessionResult()->state == ResultState::Interrupted);
        D6R_REQUIRE(controller.currentSessionResult()->finalNoWinner);
        const auto hostResult = std::find_if(controller.currentSessionResult()->players.begin(),
                controller.currentSessionResult()->players.end(), [](const auto &row) {
                    return row.participantId == 1;
                });
        const auto guestResult = std::find_if(controller.currentSessionResult()->players.begin(),
                controller.currentSessionResult()->players.end(), [](const auto &row) {
                    return row.participantId == 2;
                });
        D6R_REQUIRE(hostResult != controller.currentSessionResult()->players.end());
        D6R_REQUIRE(guestResult != controller.currentSessionResult()->players.end());
        D6R_REQUIRE(!hostResult->departed);
        D6R_REQUIRE(guestResult->departed);
        const auto retained = serializeSessionResult(*controller.currentSessionResult());
        D6R_REQUIRE(retained.has_value());

        const auto incrementalStates = deliveredStates(incrementals);
        D6R_REQUIRE(!incrementalStates.empty());
        const auto &incremental = incrementalStates.back();
        D6R_REQUIRE(incremental.phase == R::Phase::Lobby);
        D6R_REQUIRE(incremental.result.available);
        D6R_REQUIRE_EQ(std::string("Interrupted"), incremental.result.state);
        D6R_REQUIRE_EQ(*retained, incremental.result.serialized);
        const auto hostCanonical = std::find_if(incremental.players.begin(), incremental.players.end(),
                [](const auto &player) { return player.ownerParticipantId == 1; });
        const auto guestCanonical = std::find_if(incremental.players.begin(), incremental.players.end(),
                [](const auto &player) { return player.ownerParticipantId == 2; });
        D6R_REQUIRE(hostCanonical != incremental.players.end());
        D6R_REQUIRE(guestCanonical != incremental.players.end());
        D6R_REQUIRE(hostCanonical->lifeState != R::LifeState::Departed);
        D6R_REQUIRE(guestCanonical->lifeState == R::LifeState::Departed);

        std::vector<std::vector<std::uint8_t>> fullPayloads;
        D6R_REQUIRE(controller.restoreReplication(1, [&](auto payload) {
            fullPayloads.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        const auto fullStates = deliveredStates(fullPayloads);
        D6R_REQUIRE_EQ(1u, fullStates.size());
        D6R_REQUIRE(fullStates.back().phase == R::Phase::Lobby);
        D6R_REQUIRE_EQ(std::string("Interrupted"), fullStates.back().result.state);
        D6R_REQUIRE_EQ(*retained, fullStates.back().result.serialized);

        const std::size_t payloadCount = incrementals.size();
        D6R_REQUIRE(controller.removeLifecycleParticipants({2}));
        D6R_REQUIRE_EQ(payloadCount, incrementals.size());
        D6R_REQUIRE_EQ(1u, removalCalls);
        D6R_REQUIRE(lifecycle.processLifecycleBatch(Duel6::Network::Lifecycle::Phase::FinalSummary)
                    == Duel6::Network::Lifecycle::RemovalOutcome::NothingChanged);
        D6R_REQUIRE(controller.stage() == HostedMatchStage::Lobby);
        D6R_REQUIRE(controller.currentSessionResult().has_value());
        D6R_REQUIRE(controller.currentSessionResult()->state == ResultState::Interrupted);
        D6R_REQUIRE_EQ(*retained, *serializeSessionResult(*controller.currentSessionResult()));
}

D6R_TEST_CASE("NET-AC-013 interrupted retained result intentional leave lifecycle synchronization") {
    verifyInterruptedRetainedResultLifecycleDeparture(false);
}

D6R_TEST_CASE("NET-AC-013 interrupted retained result authoritative expiry lifecycle synchronization") {
    verifyInterruptedRetainedResultLifecycleDeparture(true);
}

D6R_TEST_CASE("NET-AC-014 NET-AC-018 shutdown and runtime failure discard all retained result representations") {
    const auto verifyDiscard = [](bool runtimeFailure) {
        const auto players = roster(2);
        const std::vector<R::ParticipantState> participants = {
                {1, true, R::ConnectionState::Connected, true, {101}},
                {2, false, R::ConnectionState::Connected, true, {102}}};
        AuthoritativeHostedMatchController controller(1);
        D6R_REQUIRE(controller.initializeReplication(participants, players, config()));
        D6R_REQUIRE(controller.markServiceReady());
        D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(config(), players, manifest()).code);
        std::uint64_t sequence = 1;
        eliminate(*controller.match(), sequence, players[0], players[1]);
        finishDelay(*controller.match());
        D6R_REQUIRE(controller.observeMatchOutcome());
        D6R_REQUIRE(controller.currentSessionResult().has_value());
        D6R_REQUIRE(controller.retainsCompletedResult());

        if (runtimeFailure) {
            D6R_REQUIRE(controller.setParticipantReady(1, true));
            D6R_REQUIRE(controller.setParticipantReady(2, true));
            MatchRuntimeDependencies failure;
            failure.contentPreflight = [](const auto &) { return true; };
            failure.worldStart = [](RoundStartDecision &) { return false; };
            D6R_REQUIRE_EQ(OutcomeCode::RuntimeFailed,
                    controller.start(config(), players, manifest(), std::move(failure)).code);
            D6R_REQUIRE(controller.stage() == HostedMatchStage::UnexpectedStop);
        } else {
            controller.discardSessionResults();
        }

        D6R_REQUIRE(!controller.currentSessionResult().has_value());
        D6R_REQUIRE(!controller.retainsCompletedResult());
        std::vector<std::vector<std::uint8_t>> payloads;
        D6R_REQUIRE(!controller.restoreReplication(3, [&](auto payload) {
            payloads.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }));
        D6R_REQUIRE(payloads.empty());
    };

    verifyDiscard(false);
    verifyDiscard(true);
}

D6R_TEST_CASE("REP-017 REP-025 production canonical one-round survival publishes FinalSummary and retained FollowingMatch result") {
    ProductionCanonicalFixture fixture(1);
    D6R_REQUIRE(fixture.driveToTerminal());
    D6R_REQUIRE(fixture.controller.currentSessionResult().has_value());
    const SessionResult &result = *fixture.controller.currentSessionResult();
    D6R_REQUIRE(result.state == ResultState::Completed);
    D6R_REQUIRE_EQ(1, result.completedRounds);
    D6R_REQUIRE(resultPlayer(result, 102).statistics.survivalTicks > 0);
    D6R_REQUIRE(result.rounds[0].rosterOrder == std::vector<Identity>({101, 102}));

    const auto states = deliveredStates(fixture.payloads);
    const auto *summary = lastPhase(states, R::Phase::FinalSummary);
    const auto *following = lastPhase(states, R::Phase::Lobby);
    D6R_REQUIRE(summary != nullptr);
    D6R_REQUIRE(following != nullptr && following->result.available);
    D6R_REQUIRE_EQ(std::string("Completed"), following->result.state);
    requireCumulativeScoreMatchesResult(*summary, result);
    requireCumulativeScoreMatchesResult(*following, result);
}

D6R_TEST_CASE("REP-017 REP-025 production canonical three-round result publishes cumulative score and preserves roster history matrices") {
    ProductionCanonicalFixture fixture(3);
    D6R_REQUIRE(fixture.driveToTerminal());
    D6R_REQUIRE(fixture.controller.currentSessionResult().has_value());
    const SessionResult &result = *fixture.controller.currentSessionResult();
    D6R_REQUIRE(result.state == ResultState::Completed);
    D6R_REQUIRE_EQ(3, result.completedRounds);
    D6R_REQUIRE_EQ(std::size_t{3}, result.rounds.size());
    for (const auto &round: result.rounds)
        D6R_REQUIRE(round.rosterOrder == std::vector<Identity>({101, 102}));
    for (const auto &row: result.players) {
        D6R_REQUIRE_EQ(std::size_t{3}, row.rounds.size());
        D6R_REQUIRE_EQ(1, row.rounds[0].roundsPlayed);
        D6R_REQUIRE_EQ(1, row.rounds[1].roundsPlayed);
        D6R_REQUIRE_EQ(1, row.rounds[2].roundsPlayed);
    }

    const auto states = deliveredStates(fixture.payloads);
    const auto *summary = lastPhase(states, R::Phase::FinalSummary);
    const auto *following = lastPhase(states, R::Phase::Lobby);
    D6R_REQUIRE(summary != nullptr);
    D6R_REQUIRE(following != nullptr && following->result.available);
    requireCumulativeScoreMatchesResult(*summary, result);
    requireCumulativeScoreMatchesResult(*following, result);
    bool exposesDistinctRoundAndCumulativeScore = false;
    for (const auto &row: result.players) {
        const auto &score = scorePlayer(*summary, row.playerId);
        D6R_REQUIRE_EQ(row.rounds.back().totalPoints(), score.roundPoints);
        D6R_REQUIRE_EQ(row.statistics.totalPoints(), score.cumulativePoints);
        exposesDistinctRoundAndCumulativeScore |= score.roundPoints != score.cumulativePoints;
    }
    D6R_REQUIRE(exposesDistinctRoundAndCumulativeScore);
}

D6R_TEST_CASE("REP-017 REP-025 production canonical interruption discards scored first incomplete round") {
    verifyProductionCanonicalInterruptedRoundDiscardsScore(0);
}

D6R_TEST_CASE("REP-017 REP-025 production canonical interruption discards scored incomplete round after a completed round") {
    verifyProductionCanonicalInterruptedRoundDiscardsScore(1);
}

D6R_TEST_CASE("REP-017 REP-025 production canonical interrupted match retains completed-round cumulative stats and no winner") {
    ProductionCanonicalFixture fixture(3);
    D6R_REQUIRE(fixture.driveToRound(2));
    AuthoritativeMatch *match = fixture.controller.match();
    D6R_REQUIRE(match != nullptr);
    D6R_REQUIRE_EQ(ActionResult::Accepted, match->submit({match->currentTick(), fixture.sequence++, 1, 0,
            ActionKind::RemovePlayer, 101, 0, 0}));
    D6R_REQUIRE(fixture.controller.observeMatchOutcome());
    D6R_REQUIRE(fixture.controller.currentSessionResult().has_value());
    const SessionResult &result = *fixture.controller.currentSessionResult();
    D6R_REQUIRE(result.state == ResultState::Interrupted);
    D6R_REQUIRE(result.finalNoWinner);
    D6R_REQUIRE(result.finalWinnerPlayerIds.empty());
    D6R_REQUIRE(result.finalWinningTeam == Team::None);
    D6R_REQUIRE_EQ(1, result.completedRounds);
    D6R_REQUIRE_EQ(std::size_t{1}, result.rounds.size());
    D6R_REQUIRE(result.rounds[0].rosterOrder == std::vector<Identity>({101, 102}));
    for (const auto &row: result.players) {
        D6R_REQUIRE_EQ(std::size_t{1}, row.rounds.size());
        D6R_REQUIRE_EQ(1, row.rounds[0].roundsPlayed);
    }

    const auto states = deliveredStates(fixture.payloads);
    const auto *following = lastPhase(states, R::Phase::Lobby);
    D6R_REQUIRE(following != nullptr && following->result.available);
    D6R_REQUIRE_EQ(std::string("Interrupted"), following->result.state);
    D6R_REQUIRE(following->score.winner.noWinner);
    D6R_REQUIRE(following->score.winner.winnerPlayerIds.empty());
    requireCumulativeScoreMatchesResult(*following, result);
}
}
