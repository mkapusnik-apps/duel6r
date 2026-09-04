#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "source/server/AuthoritativeMatch.h"
#include "source/server/AuthoritativeHostedMatchController.h"
#include "source/server/AuthoritativeMatchSerialization.h"
#include "source/server/AuthoritativeMatchValidation.h"
#include "source/network/StateReplicationProtocol.h"
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
    D6R_REQUIRE(controller.setParticipantReady(2, true));
    D6R_REQUIRE_EQ(OutcomeCode::None, controller.start(requested, players, manifest()).code);
    const auto newMatchStates = deliveredStates(payloads);
    const auto *newMatch = lastPhase(newMatchStates, R::Phase::ActiveRound);
    D6R_REQUIRE(newMatch != nullptr);
    D6R_REQUIRE(!newMatch->result.available);
    D6R_REQUIRE(newMatch->result.serialized.empty());
    D6R_REQUIRE(newMatch->matchId != finalSummary->matchId);
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
}
