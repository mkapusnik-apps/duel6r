#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "source/network/StateReplication.h"
#include "source/network/StateReplicationProtocol.h"
#include "source/network/NetworkTrustPolicy.h"
#include "source/server/AuthoritativeMatch.h"
#include "source/server/AuthoritativeMatchSerialization.h"
#include "source/server/AuthoritativeReplication.h"
#include "tests/TestHarness.h"

namespace {
    namespace R = Duel6::Network::Replication;
    namespace N = Duel6::Network::Responsiveness;
    namespace A = Duel6::Server::Authoritative;
    using namespace std::chrono_literals;

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

    const N::PresentedPlayerPose &presented(
            const std::vector<N::PresentedPlayerPose> &poses, R::Identity identity) {
        const auto found = std::find_if(poses.begin(), poses.end(), [identity](const auto &pose) {
            return pose.playerId == identity;
        });
        D6R_REQUIRE(found != poses.end());
        return *found;
    }

    const R::WorldEntityState *entity(const R::CanonicalState &state, R::Identity identity) {
        const auto found = std::find_if(state.entities.begin(), state.entities.end(), [identity](const auto &value) {
            return value.entityId == identity;
        });
        return found == state.entities.end() ? nullptr : &*found;
    }

    const R::ScoreRowState *scoreRow(const R::CanonicalState &state, R::Identity identity) {
        const auto found = std::find_if(state.score.players.begin(), state.score.players.end(),
                [identity](const auto &value) { return value.playerId == identity; });
        return found == state.score.players.end() ? nullptr : &*found;
    }

    R::CanonicalState admittedIdentityLobby() {
        auto state = lobbyState();
        state.hostParticipantId = 90;
        state.participants = {
                {90, true, R::ConnectionState::Connected, true, {900}},
                {20, false, R::ConnectionState::Connected, false, {100}}};
        state.players[0].playerId = 900;
        state.players[0].ownerParticipantId = 90;
        state.players[1].playerId = 100;
        state.players[1].ownerParticipantId = 20;
        state.score.ranking = {900, 100};
        return state;
    }

    R::IncrementalUpdate lobbyCreationUpdate(R::StateVersion baseline, R::StateVersion version,
                                               const R::CanonicalState &state,
                                               const R::ParticipantState &participant,
                                               const R::PlayerState &createdPlayer) {
        R::IncrementalUpdate update;
        update.sessionId = state.sessionId;
        update.matchId = state.matchId;
        update.baseline = baseline;
        update.version = version;
        update.phase = state.phase;
        update.currentRoundNumber = state.currentRoundNumber;
        update.completedRounds = state.completedRounds;
        update.phaseTime = state.phaseTime;
        update.roundEndCountdown = state.roundEndCountdown;
        update.participants = {{R::ChangeKind::Create, participant.participantId, participant}};
        update.settings = state.settings;
        update.round = state.round;
        update.players = {{R::ChangeKind::Create, createdPlayer.playerId, createdPlayer}};
        update.score = state.score;
        update.messages = state.messages;
        update.effects = state.effects;
        update.result = state.result;
        return update;
    }

    R::CanonicalState distinctFinalSummary() {
        auto state = activeState();
        state.phase = R::Phase::FinalSummary;
        state.currentRoundNumber = 2;
        state.completedRounds = 2;
        state.round->roundId = 41;
        state.round->roundNumber = 2;
        state.round->outcome.winnerPlayerIds = {102};
        state.score.players[0].roundPoints = 0;
        state.score.players[0].cumulativePoints = 3;
        state.score.players[1].roundPoints = 2;
        state.score.players[1].cumulativePoints = 2;
        state.score.ranking = {101, 102};
        state.score.winner.winnerPlayerIds = {102};
        state.entities.clear();
        state.effects.clear();
        state.messages.status = "FinalSummary";
        state.messages.scoreSummaryVisible = true;
        state.result.available = true;
        state.result.sessionOnly = true;
        state.result.state = "Completed";
        state.result.serialized = "completed-round-1=101;completed-round-2=102;match-outcome=102;cumulative-leader=101";
        return state;
    }

    R::CanonicalState retainedCompletedLobby() {
        auto state = distinctFinalSummary();
        state.phase = R::Phase::Lobby;
        state.participants[0].ready = false;
        state.participants[1].ready = false;
        state.messages.status = "Lobby";
        state.messages.scoreSummaryVisible = false;
        return state;
    }

    R::CanonicalState completedResultWithDepartureLabels() {
        auto state = distinctFinalSummary();
        state.result.serialized =
                "{\"label\":\"match\",\"state\":\"Completed\",\"outcome\":\"player-102\","
                "\"winner\":102,\"ranking\":[101,102],\"score\":[3,2],\"round\":2,\"players\":["
                "{\"rank\":1,\"playerId\":101,\"participantId\":20,\"departed\":false,\"points\":3},"
                "{\"rank\":2,\"playerId\":102,\"participantId\":21,\"departed\":false,\"points\":2}]}";
        return state;
    }

    bool replaceOnce(std::string &value, const std::string &before, const std::string &after) {
        const auto position = value.find(before);
        if (position == std::string::npos || value.find(before, position + before.size()) != std::string::npos)
            return false;
        value.replace(position, before.size(), after);
        return true;
    }

    class JsonValueCounter final {
    public:
        explicit JsonValueCounter(const std::string &source) : source(source) {}

        std::optional<std::size_t> count() {
            std::size_t result = 0;
            if (!value(result) || position != source.size()) return std::nullopt;
            return result;
        }

        std::size_t rootMembers() const noexcept { return rootObjectMembers; }

    private:
        const std::string &source;
        std::size_t position = 0;
        std::size_t rootObjectMembers = 0;

        bool string() {
            if (position >= source.size() || source[position++] != '"') return false;
            while (position < source.size()) {
                const char character = source[position++];
                if (character == '"') return true;
                if (character == '\\') {
                    if (position >= source.size()) return false;
                    ++position;
                }
            }
            return false;
        }

        bool value(std::size_t &result) {
            if (position >= source.size()) return false;
            ++result;
            if (source[position] == '{') return object(result);
            if (source[position] == '[') return array(result);
            if (source[position] == '"') return string();
            const auto start = position;
            while (position < source.size() && source[position] != ','
                   && source[position] != ']' && source[position] != '}') ++position;
            return position != start;
        }

        bool object(std::size_t &result) {
            const bool root = position == 0;
            ++position;
            if (position < source.size() && source[position] == '}') { ++position; return true; }
            while (position < source.size()) {
                if (root) ++rootObjectMembers;
                if (!string() || position >= source.size() || source[position++] != ':' || !value(result))
                    return false;
                if (position >= source.size()) return false;
                if (source[position] == '}') { ++position; return true; }
                if (source[position++] != ',') return false;
            }
            return false;
        }

        bool array(std::size_t &result) {
            ++position;
            if (position < source.size() && source[position] == ']') { ++position; return true; }
            while (position < source.size()) {
                if (!value(result) || position >= source.size()) return false;
                if (source[position] == ']') { ++position; return true; }
                if (source[position++] != ',') return false;
            }
            return false;
        }
    };

    A::SessionResult maximumCompletedResult(bool departed) {
        A::SessionResult result;
        result.label = "Maximum canonical result";
        result.config.mode = A::Mode::TeamDeathmatch;
        result.config.teamCount = 4;
        result.config.roundLimit = 99;
        result.completedRounds = 99;
        std::vector<A::Identity> identities;
        for (std::size_t index = 0; index < A::MaxPlayers; ++index)
            identities.push_back(101 + index);
        result.finalWinnerPlayerIds = identities;
        result.finalWinningTeam = A::Team::Alpha;
        for (std::uint8_t roundNumber = 1; roundNumber <= 99; ++roundNumber) {
            A::RoundResult round;
            round.roundNumber = roundNumber;
            round.level = "levels/maximum.json";
            round.winnerPlayerIds = identities;
            round.winningTeam = A::Team::Alpha;
            round.rosterOrder = identities;
            result.rounds.push_back(std::move(round));
        }
        for (std::size_t index = 0; index < A::MaxPlayers; ++index) {
            A::PlayerResultRow player;
            player.playerId = identities[index];
            player.participantId = 20 + index;
            player.displayName = "Player " + std::to_string(index + 1);
            player.team = static_cast<A::Team>(index % 4 + 1);
            player.departed = departed && index + 1 == A::MaxPlayers;
            player.rosterOrder = static_cast<std::uint8_t>(index);
            player.rounds.resize(99);
            result.players.push_back(std::move(player));
        }
        for (std::uint8_t team = 1; team <= 4; ++team)
            result.teams.push_back({static_cast<A::Team>(team), team, identities});
        return result;
    }

    R::CanonicalState maximumCompletedReplicationState(bool departed) {
        R::CanonicalState state;
        state.sessionId = 10;
        state.hostParticipantId = 20;
        state.matchId = 30;
        state.phase = R::Phase::FinalSummary;
        state.currentRoundNumber = 99;
        state.completedRounds = 99;
        state.settings.mode = "TeamDeathmatch";
        state.settings.teamCount = 4;
        state.settings.levelPlan = "Fixed";
        state.settings.levels = {"levels/maximum.json"};
        state.settings.roundLimit = 99;
        state.score.teamTotals = {4, 3, 3, 3};
        state.score.teamRanking = {1, 2, 3, 4};
        for (std::size_t index = 0; index < A::MaxPlayers; ++index) {
            const R::Identity playerId = 101 + index;
            const R::Identity participantId = 20 + index;
            state.participants.push_back({participantId, index == 0,
                                          R::ConnectionState::Connected, true, {playerId}});
            R::PlayerState player;
            player.playerId = playerId;
            player.ownerParticipantId = participantId;
            player.rosterPosition = static_cast<std::uint8_t>(index);
            player.displayName = "Player " + std::to_string(index + 1);
            player.team = static_cast<std::uint8_t>(index % 4 + 1);
            player.lifeState = departed && index + 1 == A::MaxPlayers
                               ? R::LifeState::Departed : R::LifeState::Alive;
            player.life = 100;
            state.players.push_back(std::move(player));
            state.score.players.push_back({playerId});
            state.score.ranking.push_back(playerId);
        }
        state.score.winner.winnerPlayerIds = {101};
        state.score.winner.winningTeam = 1;
        state.round = R::RoundState{128, 99, "levels/maximum.json", false,
                                    state.score.ranking, state.score.winner};
        state.messages.status = "FinalSummary";
        state.messages.scoreSummaryVisible = true;
        state.result.available = true;
        state.result.sessionOnly = true;
        state.result.state = "Completed";
        const auto serialized = A::serializeSessionResult(maximumCompletedResult(departed));
        D6R_REQUIRE(serialized.has_value());
        state.result.serialized = *serialized;
        return state;
    }

    R::CanonicalState completedResultDeparture(R::CanonicalState state) {
        state.players[1].lifeState = R::LifeState::Departed;
        const bool replaced = replaceOnce(state.result.serialized,
                "\"participantId\":21,\"departed\":false",
                "\"participantId\":21,\"departed\":true");
        D6R_REQUIRE(replaced);
        return state;
    }

    R::CanonicalState retainedResultWithDepartureLabels(bool interrupted) {
        auto state = completedResultWithDepartureLabels();
        state.phase = R::Phase::Lobby;
        state.participants[0].ready = false;
        state.participants[1].ready = false;
        state.messages.status = "Lobby";
        state.messages.scoreSummaryVisible = false;
        if (!interrupted) return state;

        state.currentRoundNumber = 1;
        state.completedRounds = 1;
        state.round->roundId = 40;
        state.round->roundNumber = 1;
        state.round->outcome.winnerPlayerIds = {101};
        state.score.winner = {};
        state.score.winner.noWinner = true;
        state.result.state = "Interrupted";
        state.result.serialized =
                "{\"label\":\"match\",\"state\":\"Interrupted\",\"outcome\":\"no-winner\","
                "\"winner\":null,\"ranking\":[101,102],\"score\":[3,2],\"round\":1,\"players\":["
                "{\"rank\":1,\"playerId\":101,\"participantId\":20,\"departed\":false,\"points\":3},"
                "{\"rank\":2,\"playerId\":102,\"participantId\":21,\"departed\":false,\"points\":2}]}";
        return state;
    }

    R::CanonicalState firstResultWithDepartureLabels(bool interrupted,
                                                       bool canonicalDeparture,
                                                       bool resultDeparture) {
        auto state = interrupted ? retainedResultWithDepartureLabels(true)
                                 : completedResultWithDepartureLabels();
        state.phaseTime = 14;
        if (canonicalDeparture) state.players[1].lifeState = R::LifeState::Departed;
        if (resultDeparture) {
            const bool replaced = replaceOnce(state.result.serialized,
                    "\"participantId\":21,\"departed\":false",
                    "\"participantId\":21,\"departed\":true");
            D6R_REQUIRE(replaced);
        }
        return state;
    }

    R::CanonicalState retainedInterruptedLobby() {
        auto state = retainedCompletedLobby();
        state.currentRoundNumber = 1;
        state.completedRounds = 1;
        state.round->roundId = 40;
        state.round->roundNumber = 1;
        state.round->outcome.winnerPlayerIds = {101};
        state.score.winner = {};
        state.score.winner.noWinner = true;
        state.result.state = "Interrupted";
        state.result.serialized = "interrupted;completed-round-1=101;match-outcome=no-winner";
        return state;
    }

    struct RetainedResultMutation {
        std::string name;
        R::CanonicalState before;
        std::function<void(R::CanonicalState &)> alter;
    };

    std::vector<RetainedResultMutation> retainedResultMutations() {
        return {
                {"score-values", retainedCompletedLobby(), [](auto &state) {
                    state.score.players[0].cumulativePoints++;
                }},
                {"ranking-order", retainedCompletedLobby(), [](auto &state) {
                    std::swap(state.score.players[0], state.score.players[1]);
                    std::swap(state.score.ranking[0], state.score.ranking[1]);
                }},
                {"result-state", retainedCompletedLobby(), [](auto &state) {
                    state.result.state = "Interrupted";
                    state.score.winner = {};
                    state.score.winner.noWinner = true;
                }},
                {"match-outcome", retainedCompletedLobby(), [](auto &state) {
                    state.score.winner.winnerPlayerIds = {101};
                    state.round->outcome = state.score.winner;
                }},
                {"completed-round-outcome", retainedInterruptedLobby(), [](auto &state) {
                    state.round->outcome.winnerPlayerIds = {102};
                }},
                {"serialized-result", retainedInterruptedLobby(), [](auto &state) {
                    state.result.serialized += ";altered=true";
                }},
                {"round-counters-and-number", retainedCompletedLobby(), [](auto &state) {
                    state.currentRoundNumber = 1;
                    state.completedRounds = 1;
                    state.round->roundNumber = 1;
                }},
                {"round-id", retainedCompletedLobby(), [](auto &state) {
                    state.round->roundId = 42;
                }},
                {"round-level", retainedCompletedLobby(), [](auto &state) {
                    state.round->level = "levels/altered.json";
                }},
                {"round-orientation", retainedCompletedLobby(), [](auto &state) {
                    state.round->mirrored = !state.round->mirrored;
                }},
                {"round-roster-order", retainedCompletedLobby(), [](auto &state) {
                    std::swap(state.round->rosterOrder[0], state.round->rosterOrder[1]);
                }}};
    }

    struct RetainedPhaseMutation {
        std::string name;
        R::CanonicalState before;
        R::CanonicalState benign;
        R::CanonicalState altered;
    };

    R::IncrementalUpdate validUpdate(const R::CanonicalState &before, R::CanonicalState after,
                                     std::vector<R::PresentationEvent> events);

    std::vector<RetainedPhaseMutation> finalSummaryPhaseMutations() {
        const auto final = distinctFinalSummary();
        auto followingLobby = retainedCompletedLobby();
        std::vector<RetainedPhaseMutation> result;
        const auto add = [&](const std::string &name, const R::CanonicalState &benign,
                             const std::function<void(R::CanonicalState &)> &alter) {
            auto altered = benign;
            alter(altered);
            result.push_back({name, final, benign, std::move(altered)});
        };
        const std::vector<std::pair<std::string, std::function<void(R::CanonicalState &)>>> mutations = {
                {"score-values", [](auto &state) { state.score.players[0].cumulativePoints++; }},
                {"ranking-order", [](auto &state) {
                    std::swap(state.score.players[0], state.score.players[1]);
                    std::swap(state.score.ranking[0], state.score.ranking[1]);
                }},
                {"match-outcome", [](auto &state) {
                    state.score.winner.winnerPlayerIds = {101};
                    state.round->outcome = state.score.winner;
                }},
                {"serialized-result", [](auto &state) { state.result.serialized += ";altered=true"; }},
                {"round-counters-and-number", [](auto &state) {
                    state.currentRoundNumber = 1;
                    state.completedRounds = 1;
                    state.round->roundNumber = 1;
                }},
                {"round-id", [](auto &state) { state.round->roundId = 42; }},
                {"round-level", [](auto &state) { state.round->level = "levels/altered.json"; }},
                {"round-orientation", [](auto &state) { state.round->mirrored = !state.round->mirrored; }},
                {"round-roster-order", [](auto &state) {
                    std::swap(state.round->rosterOrder[0], state.round->rosterOrder[1]);
                    std::swap(state.players[0].rosterPosition, state.players[1].rosterPosition);
                }}};
        for (const auto &[name, alter]: mutations) {
            auto samePhase = final;
            samePhase.phaseTime++;
            add("FinalSummary-to-FinalSummary/" + name, samePhase, alter);
            add("FinalSummary-to-Lobby/" + name, followingLobby, alter);
        }
        auto interrupted = followingLobby;
        interrupted.result.state = "Interrupted";
        interrupted.result.serialized = "interrupted;completed-rounds=2;match-outcome=no-winner";
        interrupted.score.winner = {};
        interrupted.score.winner.noWinner = true;
        result.push_back({"FinalSummary-to-Lobby/result-state", final, followingLobby, interrupted});
        return result;
    }

    R::IncrementalUpdate retainedAttackUpdate(const RetainedPhaseMutation &scenario,
                                               const R::PresentationEvent &event) {
        auto update = validUpdate(scenario.before, scenario.benign, {event});
        update.currentRoundNumber = scenario.altered.currentRoundNumber;
        update.completedRounds = scenario.altered.completedRounds;
        update.round = scenario.altered.round;
        update.score = scenario.altered.score;
        update.result = scenario.altered.result;
        update.players.clear();
        for (const auto &value: scenario.altered.players)
            update.players.push_back({R::ChangeKind::Update, value.playerId, value});
        return update;
    }

    R::CanonicalState legitimateFollowingLobbyChange(R::CanonicalState state) {
        state.phaseTime++;
        state.settings.assistance = !state.settings.assistance;
        state.settings.quickLiquid = !state.settings.quickLiquid;
        state.participants[0].ready = true;
        state.participants[1].connection = R::ConnectionState::Reconnecting;
        state.players[0].rosterPosition = 1;
        state.players[1].rosterPosition = 0;
        state.players[1].displayName = "Guest (Edited)";
        return state;
    }

    R::CanonicalState freshMatchAfterRetainedResult() {
        auto state = activeState();
        state.matchId = 31;
        state.round->roundId = 42;
        state.result = {};
        return state;
    }

    R::CanonicalState retainedTeamCompletedLobby() {
        auto state = retainedCompletedLobby();
        state.settings.teamCount = 2;
        state.players[0].team = 1;
        state.players[1].team = 2;
        state.score.players[1].cumulativePoints = 4;
        std::swap(state.score.players[0], state.score.players[1]);
        state.score.ranking = {102, 101};
        state.score.teamTotals = {3, 4};
        state.score.teamRanking = {2, 1};
        state.score.winner.winningTeam = 2;
        state.round->outcome.winningTeam = 2;
        return state;
    }

    R::CanonicalState resetTeamFirstRound() {
        auto state = freshMatchAfterRetainedResult();
        state.settings.teamCount = 2;
        state.players[0].team = 1;
        state.players[1].team = 2;
        state.score.teamTotals = {0, 0};
        state.score.teamRanking = {1, 2};
        return state;
    }

    struct StaleNewMatchMutation {
        std::string name;
        std::function<void(R::CanonicalState &)> alter;
    };

    std::vector<StaleNewMatchMutation> staleNewMatchMutations() {
        return {
                {"cumulative-scores", [](auto &state) {
                    state.score.players[0].cumulativePoints = 3;
                }},
                {"ranking", [](auto &state) {
                    std::swap(state.score.players[0], state.score.players[1]);
                    std::swap(state.score.ranking[0], state.score.ranking[1]);
                }},
                {"team-totals", [](auto &state) {
                    state.score.teamTotals = {3, 4};
                }},
                {"round-counters", [](auto &state) {
                    state.currentRoundNumber = 2;
                    state.completedRounds = 1;
                    state.round->roundNumber = 2;
                }}};
    }

    struct FollowingLobbyLifecycleAttack {
        std::string name;
        R::CanonicalState state;
    };

    std::vector<FollowingLobbyLifecycleAttack> followingLobbyLifecycleAttacks() {
        const auto retained = retainedCompletedLobby();

        auto activePriorMatch = retained;
        activePriorMatch.phase = R::Phase::ActiveRound;
        activePriorMatch.currentRoundNumber = 3;
        activePriorMatch.round->roundId = 42;
        activePriorMatch.round->roundNumber = 3;
        activePriorMatch.round->outcome = {};
        activePriorMatch.score.winner = {};
        activePriorMatch.result = {};
        activePriorMatch.messages.status = "ActiveRound";

        auto summaryPriorMatch = activePriorMatch;
        summaryPriorMatch.phase = R::Phase::RoundSummary;
        summaryPriorMatch.round->outcome.winnerPlayerIds = {101};
        summaryPriorMatch.score.winner = summaryPriorMatch.round->outcome;
        summaryPriorMatch.messages.status = "RoundSummary";
        summaryPriorMatch.messages.scoreSummaryVisible = true;

        auto finalAlteredMatch = distinctFinalSummary();
        finalAlteredMatch.matchId = 31;

        return {{"ActiveRound-prior-match", std::move(activePriorMatch)},
                {"RoundSummary-prior-match", std::move(summaryPriorMatch)},
                {"FinalSummary-altered-match", std::move(finalAlteredMatch)}};
    }

    R::IncrementalUpdate stateOnlyUpdate(R::StateVersion baseline, R::StateVersion version,
                                          const R::CanonicalState &state,
                                          std::vector<R::PresentationEvent> events = {}) {
        R::IncrementalUpdate update;
        update.sessionId = state.sessionId;
        update.matchId = state.matchId;
        update.baseline = baseline;
        update.version = version;
        update.phase = state.phase;
        update.currentRoundNumber = state.currentRoundNumber;
        update.completedRounds = state.completedRounds;
        update.phaseTime = state.phaseTime;
        update.roundEndCountdown = state.roundEndCountdown;
        update.settings = state.settings;
        update.round = state.round;
        update.score = state.score;
        update.messages = state.messages;
        update.effects = state.effects;
        update.result = state.result;
        update.events = std::move(events);
        return update;
    }

    void requireRetainedResultEqual(const R::CanonicalState &expected, R::CanonicalState actual) {
        actual.sessionId = expected.sessionId;
        actual.matchId = expected.matchId;
        actual.hostParticipantId = expected.hostParticipantId;
        actual.phase = expected.phase;
        actual.phaseTime = expected.phaseTime;
        actual.roundEndCountdown = expected.roundEndCountdown;
        actual.participants = expected.participants;
        actual.settings = expected.settings;
        actual.players = expected.players;
        actual.entities = expected.entities;
        actual.messages = expected.messages;
        actual.effects = expected.effects;
        D6R_REQUIRE_EQ(R::serializeReplicationSnapshot({1, expected}),
                       R::serializeReplicationSnapshot({1, actual}));
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

    enum class FirstResultDelivery { InitialSnapshot, ResynchronizationSnapshot, IncrementalUpdate };

    bool firstResultDepartureScenario(bool interrupted, bool canonicalDeparture,
                                      bool resultDeparture, FirstResultDelivery delivery) {
        const auto terminal = firstResultWithDepartureLabels(
                interrupted, canonicalDeparture, resultDeparture);
        const auto synchronized = firstResultWithDepartureLabels(interrupted, true, true);
        const bool shouldAccept = canonicalDeparture && resultDeparture;
        D6R_REQUIRE(R::validateCanonicalState(terminal));
        D6R_REQUIRE(R::validateCanonicalState(synchronized));

        R::ReplicatedState client;
        R::StateVersion retainedVersion = 0;
        std::optional<R::CanonicalState> retained;
        const R::PresentationEvent pendingEvent{991, "result-transition", 0, 0, 0, 0};
        const R::PresentationEvent rejectedEvent{992, "result-transition", 0, 0, 0, 0};
        bool applicationCorrect = false;

        if (delivery == FirstResultDelivery::InitialSnapshot) {
            const auto result = client.apply({1, terminal});
            applicationCorrect = shouldAccept
                    ? result == R::ApplyResult::Applied && client.version() == 1
                      && client.current() && client.state()
                    : result == R::ApplyResult::Invalid && client.version() == 0
                      && !client.current() && client.retainedState() == nullptr
                      && client.takePresentationEvents().empty();
            if (!shouldAccept && client.apply({1, synchronized}) != R::ApplyResult::Applied) return false;
        } else {
            const auto active = activeState();
            auto beforeTerminal = active;
            beforeTerminal.phaseTime++;
            D6R_REQUIRE(client.apply({1, active}) == R::ApplyResult::Applied);
            D6R_REQUIRE(client.apply(validUpdate(active, beforeTerminal, {pendingEvent}))
                        == R::ApplyResult::Applied);
            retainedVersion = client.version();
            retained = *client.retainedState();

            R::ApplyResult result = R::ApplyResult::Invalid;
            if (delivery == FirstResultDelivery::ResynchronizationSnapshot) {
                client.requireResynchronization();
                result = client.apply({3, terminal});
            } else {
                auto update = validUpdate(beforeTerminal, terminal, {rejectedEvent});
                update.baseline = 2;
                update.version = 3;
                result = client.apply(update);
            }

            if (shouldAccept) {
                applicationCorrect = result == R::ApplyResult::Applied && client.version() == 3
                        && client.current() && client.state();
            } else {
                const auto expectedResult = delivery == FirstResultDelivery::IncrementalUpdate
                        ? R::ApplyResult::ResynchronizationRequired : R::ApplyResult::Invalid;
                applicationCorrect = result == expectedResult && client.version() == retainedVersion
                        && !client.current() && client.state() == nullptr && client.retainedState()
                        && R::serializeReplicationSnapshot({retainedVersion, *client.retainedState()})
                           == R::serializeReplicationSnapshot({retainedVersion, *retained});
                if (client.apply({3, synchronized}) != R::ApplyResult::Applied) return false;
            }
        }

        if (!applicationCorrect || !client.state()
            || client.state()->players[1].lifeState != R::LifeState::Departed
            || client.state()->result.serialized != synchronized.result.serialized) return false;

        auto followingLobby = synchronized;
        followingLobby.phase = R::Phase::Lobby;
        followingLobby.participants[0].ready = false;
        followingLobby.participants[1].ready = false;
        followingLobby.messages.status = "Lobby";
        followingLobby.messages.scoreSummaryVisible = false;
        followingLobby = legitimateFollowingLobbyChange(std::move(followingLobby));
        const auto followingVersion = client.version() + 1;
        bool followingApplied = false;
        if (delivery == FirstResultDelivery::IncrementalUpdate) {
            auto update = validUpdate(synchronized, followingLobby);
            update.baseline = client.version();
            update.version = followingVersion;
            followingApplied = client.apply(update) == R::ApplyResult::Applied;
        } else {
            followingApplied = client.apply({followingVersion, followingLobby}) == R::ApplyResult::Applied;
        }
        if (!followingApplied || !client.state()
            || client.state()->settings.assistance != followingLobby.settings.assistance
            || !client.state()->participants[0].ready
            || client.state()->players[1].displayName != "Guest (Edited)") return false;

        auto removed = followingLobby;
        removed.participants.pop_back();
        removed.players.pop_back();
        const auto removalVersion = client.version() + 1;
        if (client.apply({removalVersion, removed}) != R::ApplyResult::Applied) return false;
        const auto retainedAfterRemoval = R::serializeReplicationSnapshot(
                {client.version(), *client.retainedState()});
        const bool identityHistoryPreserved = client.apply({client.version() + 1, followingLobby})
                                              == R::ApplyResult::Invalid
                && client.version() == removalVersion && client.retainedState()
                && R::serializeReplicationSnapshot({client.version(), *client.retainedState()})
                   == retainedAfterRemoval;

        const auto events = client.takePresentationEvents();
        const bool pendingEventsPreserved = delivery == FirstResultDelivery::InitialSnapshot
                ? events.empty()
                : shouldAccept && delivery == FirstResultDelivery::IncrementalUpdate
                  ? events.size() == 2 && events[0].eventId == pendingEvent.eventId
                    && events[1].eventId == rejectedEvent.eventId
                  : events.size() == 1 && events.front().eventId == pendingEvent.eventId;
        return identityHistoryPreserved && pendingEventsPreserved;
    }

    std::string firstResultDepartureMatrix(FirstResultDelivery delivery) {
        std::string evidence;
        for (const bool interrupted: {false, true}) {
            if (!evidence.empty()) evidence += ';';
            evidence += std::string(interrupted ? "Interrupted" : "Completed")
                    + "/synchronized="
                    + (firstResultDepartureScenario(interrupted, true, true, delivery) ? "true" : "false")
                    + ",result-only="
                    + (firstResultDepartureScenario(interrupted, false, true, delivery) ? "true" : "false")
                    + ",canonical-only="
                    + (firstResultDepartureScenario(interrupted, true, false, delivery) ? "true" : "false");
        }
        return evidence;
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

    std::vector<std::uint8_t> stagedSnapshotPayload() {
        auto state = distinctFinalSummary();
        // Small enough to pack each 4 MiB connection budget closely, while large enough that
        // eight concurrent clients exercise the shared 32 MiB boundary without excessive frames.
        state.result.serialized.assign(64 * 1024, 'r');
        R::FullSnapshot snapshot{1, state};
        snapshot.authoritativeProducedAt = 1;
        const auto payload = R::serializeReplicationSnapshot(snapshot);
        D6R_REQUIRE(payload.size() < Duel6::Network::MaxPayloadBytes);
        return payload;
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

D6R_TEST_CASE("REP-005..008 admitted identities may arrive non-monotonically but removed identities remain terminal") {
    const auto initial = admittedIdentityLobby();
    auto removed = initial;
    removed.participants.pop_back();
    removed.players.pop_back();
    removed.score.ranking = {900};

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    const auto removal = publisher.publish(removed);
    D6R_REQUIRE(removal.has_value());

    auto fresh = removed;
    const R::ParticipantState freshParticipant{
            50, false, R::ConnectionState::Connected, false, {500}};
    auto freshPlayer = initial.players.front();
    freshPlayer.playerId = 500;
    freshPlayer.ownerParticipantId = 50;
    freshPlayer.rosterPosition = 1;
    freshPlayer.displayName = "Fresh Guest";
    fresh.participants.push_back(freshParticipant);
    fresh.players.push_back(freshPlayer);
    fresh.score.ranking = {900, 500};
    const auto creation = publisher.publish(fresh);
    D6R_REQUIRE(creation.has_value());

    auto updated = fresh;
    updated.participants.back().ready = true;
    updated.players.back().displayName = "Updated Fresh Guest";
    const auto update = publisher.publish(updated);
    D6R_REQUIRE(update.has_value());

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(*removal) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(*creation) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(*update) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(std::string("Updated Fresh Guest"), player(*client.state(), 500)->displayName);
    client.requireResynchronization();
    D6R_REQUIRE(client.apply(*publisher.fullSnapshot()) == R::ApplyResult::Applied);
    D6R_REQUIRE(player(*client.state(), 500) != nullptr);
    D6R_REQUIRE(player(*client.state(), 100) == nullptr);

    auto reused = updated;
    reused.participants.push_back(initial.participants.back());
    reused.players.push_back(initial.players.back());
    reused.score.ranking.push_back(100);
    D6R_REQUIRE(!publisher.publish(reused).has_value());
}

D6R_TEST_CASE("REP-005..008 client accepts fresh lower admitted identities in updates and resync but rejects actual reuse") {
    const auto initial = admittedIdentityLobby();
    auto removed = initial;
    removed.participants.pop_back();
    removed.players.pop_back();
    removed.score.ranking = {900};
    const auto removal = validUpdate(initial, removed);

    auto fresh = removed;
    const R::ParticipantState freshParticipant{
            50, false, R::ConnectionState::Connected, false, {500}};
    auto freshPlayer = initial.players.front();
    freshPlayer.playerId = 500;
    freshPlayer.ownerParticipantId = 50;
    freshPlayer.rosterPosition = 1;
    freshPlayer.displayName = "Fresh Guest";
    fresh.participants.push_back(freshParticipant);
    fresh.players.push_back(freshPlayer);
    fresh.score.ranking = {900, 500};
    const auto creation = lobbyCreationUpdate(2, 3, fresh, freshParticipant, freshPlayer);

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(removal) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::Applied);
    client.requireResynchronization();
    D6R_REQUIRE(client.apply({4, fresh}) == R::ApplyResult::Applied);
    D6R_REQUIRE(player(*client.state(), 500) != nullptr);
    D6R_REQUIRE(player(*client.state(), 100) == nullptr);

    const auto reuse = lobbyCreationUpdate(4, 5, initial, initial.participants.back(), initial.players.back());
    D6R_REQUIRE(client.apply(reuse) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(4u, client.version());
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

D6R_TEST_CASE("REP-005 REP-007 REP-048 unchanged logical round rejects a fresh round identity in updates and resync") {
    auto initial = activeState();
    // Keep entity lifecycle checks orthogonal: this case isolates only logical round identity.
    initial.entities.clear();
    auto replaced = initial;
    replaced.phaseTime++;
    replaced.round->roundId = 41;
    auto replacement = validUpdate(initial, [&] {
        auto next = initial;
        next.phaseTime++;
        return next;
    }());
    replacement.round = replaced.round;

    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(incremental.apply(replacement) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(1u, incremental.version());
    D6R_REQUIRE(!incremental.current());

    R::ReplicatedState snapshot;
    D6R_REQUIRE(snapshot.apply({1, initial}) == R::ApplyResult::Applied);
    snapshot.requireResynchronization();
    D6R_REQUIRE(snapshot.apply({2, replaced}) == R::ApplyResult::Invalid);
    D6R_REQUIRE_EQ(1u, snapshot.version());
    D6R_REQUIRE(!snapshot.current());
}

D6R_TEST_CASE("REP-005 REP-007 REP-048 genuine next-round and next-match identities remain valid") {
    const auto initial = activeState();
    auto nextRound = initial;
    nextRound.phaseTime++;
    nextRound.currentRoundNumber = 2;
    nextRound.completedRounds = 1;
    nextRound.round->roundId = 41;
    nextRound.round->roundNumber = 2;
    nextRound.entities.clear();

    R::ReplicatedState rounds;
    D6R_REQUIRE(rounds.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(rounds.apply(validUpdate(initial, nextRound)) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(41u, rounds.state()->round->roundId);

    auto followingLobby = distinctFinalSummary();
    followingLobby.phase = R::Phase::Lobby;
    followingLobby.participants[0].ready = false;
    followingLobby.participants[1].ready = false;
    followingLobby.messages.status = "Lobby";
    followingLobby.messages.scoreSummaryVisible = false;
    auto nextMatch = activeState();
    nextMatch.matchId = 31;
    nextMatch.round->roundId = 42;
    nextMatch.result = {};

    R::ReplicatedState matches;
    D6R_REQUIRE(matches.apply({1, distinctFinalSummary()}) == R::ApplyResult::Applied);
    D6R_REQUIRE(matches.apply({2, followingLobby}) == R::ApplyResult::Applied);
    auto nextMatchUpdate = validUpdate(followingLobby, nextMatch);
    nextMatchUpdate.baseline = 2;
    nextMatchUpdate.version = 3;
    D6R_REQUIRE(matches.apply(nextMatchUpdate) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(31u, matches.state()->matchId);
    D6R_REQUIRE_EQ(42u, matches.state()->round->roundId);
}

D6R_TEST_CASE("REP-005 REP-006 REP-048 mid-match match identity replacement update is rejected without mutation") {
    const auto initial = activeState();
    auto replaced = initial;
    replaced.matchId = 31;
    replaced.phaseTime++;
    const auto replacement = validUpdate(initial, replaced);

    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(incremental.apply(replacement) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(1u, incremental.version());
    D6R_REQUIRE(!incremental.current());
    incremental.requireResynchronization();
    auto current = initial;
    current.phaseTime += 2;
    D6R_REQUIRE(incremental.apply({2, current}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(30u, incremental.state()->matchId);
}

D6R_TEST_CASE("REP-005 REP-006 REP-048 mid-match match identity replacement resync snapshot is rejected without mutation") {
    const auto initial = activeState();
    auto replaced = initial;
    replaced.matchId = 31;
    replaced.phaseTime++;
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    client.requireResynchronization();
    D6R_REQUIRE(client.apply({2, replaced}) == R::ApplyResult::Invalid);
    D6R_REQUIRE_EQ(1u, client.version());
    D6R_REQUIRE(!client.current());
    auto current = initial;
    current.phaseTime += 2;
    D6R_REQUIRE(client.apply({2, current}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(30u, client.state()->matchId);
}

D6R_TEST_CASE("REP-005 REP-007 REP-010 REP-048 removed round identity cannot return in update or resync snapshot") {
    const auto initial = activeState();
    auto secondRound = initial;
    secondRound.currentRoundNumber = 2;
    secondRound.completedRounds = 1;
    secondRound.phaseTime++;
    secondRound.round->roundId = 41;
    secondRound.round->roundNumber = 2;
    secondRound.entities.clear();
    const auto transition = validUpdate(initial, secondRound);

    auto reused = secondRound;
    reused.phaseTime++;
    reused.round->roundId = 40;
    const auto reuse = validUpdate(secondRound, reused);

    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(incremental.apply(transition) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(41u, incremental.state()->round->roundId);
    D6R_REQUIRE(incremental.apply(reuse) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(2u, incremental.version());
    D6R_REQUIRE(incremental.apply({3, secondRound}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(41u, incremental.state()->round->roundId);

    R::ReplicatedState snapshot;
    D6R_REQUIRE(snapshot.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(snapshot.apply({2, secondRound}) == R::ApplyResult::Applied);
    D6R_REQUIRE(snapshot.apply({3, reused}) == R::ApplyResult::Invalid);
    D6R_REQUIRE_EQ(2u, snapshot.version());
    D6R_REQUIRE_EQ(41u, snapshot.state()->round->roundId);
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
        if (phase == R::Phase::RoundSummary) {
            state.round->outcome.winnerPlayerIds = {101};
            state.score.winner = state.round->outcome;
        } else if (phase == R::Phase::FinalSummary) {
            state.completedRounds = 1;
            state.round->outcome.winnerPlayerIds = {101};
            state.score.winner = state.round->outcome;
            state.result.available = true;
            state.result.sessionOnly = true;
            state.result.state = "Completed";
            state.result.serialized = "canonical-session-result";
            state.entities.clear();
            state.effects.clear();
        } else if (phase == R::Phase::Ended) {
            state.round.reset();
            state.entities.clear();
            state.effects.clear();
        }
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

    auto badReference = validUpdate(initial, next);
    badReference.events = {{61, "explosion", 101, 0, 9999, 1}};
    requireRejectedWithoutVersionMutation(badReference);
}

D6R_TEST_CASE("REP-012 REP-027 REP-030 REP-032 production combat events survive transient projectile removal exactly once") {
    const auto initial = activeState();
    auto afterCapture = initial;
    afterCapture.phaseTime++;
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    // Projectile 777 was created, generated this real production event sequence, and was removed
    // between captures, so no entity create/remove delta can accompany these occurrences.
    const std::vector<R::PresentationEvent> expectedEvents = {
            {600, "shot-fired", 101, 0, 777, 1},
            {601, "shot-hit", 101, 102, 777, 1},
            {602, "player-life-changed", 102, 101, 777, -100},
            {603, "player-died", 102, 101, 777, 0},
            {604, "player-killed", 101, 102, 777, 1}};
    const auto update = publisher.publish(afterCapture, expectedEvents);
    D6R_REQUIRE(update.has_value());

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(*update) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.current());
    D6R_REQUIRE(!client.resynchronizationRequired());
    D6R_REQUIRE(entity(*client.state(), 777) == nullptr);
    const auto events = client.takePresentationEvents();
    D6R_REQUIRE_EQ(expectedEvents.size(), events.size());
    for (std::size_t index = 0; index < expectedEvents.size(); ++index) {
        D6R_REQUIRE_EQ(expectedEvents[index].eventId, events[index].eventId);
        D6R_REQUIRE_EQ(expectedEvents[index].type, events[index].type);
        D6R_REQUIRE_EQ(777u, events[index].entityId);
    }
    D6R_REQUIRE(client.takePresentationEvents().empty());

    client.requireResynchronization();
    D6R_REQUIRE(client.apply(*publisher.fullSnapshot()) == R::ApplyResult::Applied);
    D6R_REQUIRE(entity(*client.state(), 777) == nullptr);
    D6R_REQUIRE(client.takePresentationEvents().empty());
}

D6R_TEST_CASE("REP-005 REP-012 REP-027 transient-only projectile identity is delivered once and cannot later become live") {
    const auto initial = activeState();
    auto afterEvent = initial;
    afterEvent.phaseTime++;
    const auto transient = validUpdate(initial, afterEvent, {
            {700, "shot-fired", 101, 0, 777, 1},
            {701, "shot-hit", 101, 102, 777, 1}});

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(transient) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, client.takePresentationEvents().size());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    auto reused = afterEvent;
    reused.phaseTime++;
    auto projectile = initial.entities.front();
    projectile.entityId = 777;
    reused.entities.push_back(projectile);
    auto creation = validUpdate(afterEvent, reused);
    creation.baseline = 2;
    creation.version = 3;
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(2u, client.version());
}

D6R_TEST_CASE("REP-005 REP-012 REP-027 transient bonus-picked identity is delivered once and cannot later become live") {
    const auto initial = activeState();
    auto afterEvent = initial;
    afterEvent.phaseTime++;
    const auto transient = validUpdate(initial, afterEvent, {{710, "bonus-picked", 101, 0, 778, 1}});

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(transient) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(1u, client.takePresentationEvents().size());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    auto reused = afterEvent;
    reused.phaseTime++;
    R::WorldEntityState bonus;
    bonus.entityId = 778;
    bonus.kind = R::EntityKind::BonusPickup;
    bonus.type = "shield";
    bonus.lifecycle = "available";
    reused.entities.push_back(bonus);
    auto creation = validUpdate(afterEvent, reused);
    creation.baseline = 2;
    creation.version = 3;
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(2u, client.version());
}

D6R_TEST_CASE("REP-005 REP-012 REP-027 transient weapon-picked identity is delivered once and cannot later become live") {
    const auto initial = activeState();
    auto afterEvent = initial;
    afterEvent.phaseTime++;
    const auto transient = validUpdate(initial, afterEvent, {{720, "weapon-picked", 101, 0, 779, 1}});

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(transient) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(1u, client.takePresentationEvents().size());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    auto reused = afterEvent;
    reused.phaseTime++;
    R::WorldEntityState weapon;
    weapon.entityId = 779;
    weapon.kind = R::EntityKind::WeaponPickup;
    weapon.type = "bazooka";
    weapon.lifecycle = "available";
    reused.entities.push_back(weapon);
    auto creation = validUpdate(afterEvent, reused);
    creation.baseline = 2;
    creation.version = 3;
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(2u, client.version());
}

D6R_TEST_CASE("REP-012 REP-027 malformed transient event references fail closed without mutation") {
    const auto initial = activeState();
    auto afterEvent = initial;
    afterEvent.phaseTime++;
    std::vector<bool> rejected;
    for (const R::PresentationEvent &event: {
            R::PresentationEvent{730, "shot-hit", 101, 102, 888, 1},
            R::PresentationEvent{731, "player-life-changed", 102, 101, 889, -1},
            R::PresentationEvent{732, "player-died", 102, 101, 890, 0},
            R::PresentationEvent{733, "player-killed", 101, 102, 891, 1}}) {
        auto malformed = validUpdate(initial, afterEvent);
        malformed.events = {event};
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
        rejected.push_back(client.apply(malformed) == R::ApplyResult::ResynchronizationRequired
                           && client.version() == 1 && !client.current()
                           && client.takePresentationEvents().empty());
    }
    D6R_REQUIRE(rejected == std::vector<bool>({true, true, true, true}));
}

D6R_TEST_CASE("REP-005 REP-012 authoritative transient-only identities cannot later be published as live") {
    const auto initial = activeState();
    const std::vector<std::pair<R::PresentationEvent, R::EntityKind>> scenarios = {
            {{740, "shot-fired", 101, 0, 777, 1}, R::EntityKind::Projectile},
            {{741, "bonus-picked", 101, 0, 778, 1}, R::EntityKind::BonusPickup},
            {{742, "weapon-picked", 101, 0, 779, 1}, R::EntityKind::WeaponPickup}};
    std::vector<bool> rejected;
    for (const auto &scenario: scenarios) {
        auto afterEvent = initial;
        afterEvent.phaseTime++;
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(initial));
        D6R_REQUIRE(publisher.publish(afterEvent, {scenario.first}).has_value());
        auto reused = afterEvent;
        reused.phaseTime++;
        R::WorldEntityState entity;
        entity.entityId = scenario.first.entityId;
        entity.kind = scenario.second;
        entity.type = "transient-reuse";
        entity.lifecycle = "active";
        reused.entities.push_back(entity);
        rejected.push_back(!publisher.publish(reused).has_value());
    }
    D6R_REQUIRE(rejected == std::vector<bool>({true, true, true}));
}

D6R_TEST_CASE("REP-005 REP-007 REP-012 removed live identities cannot return as transient-only events") {
    const std::vector<std::pair<R::WorldEntityState, std::string>> scenarios = [] {
        std::vector<std::pair<R::WorldEntityState, std::string>> values;
        R::WorldEntityState projectile;
        projectile.entityId = 60; projectile.kind = R::EntityKind::Projectile;
        projectile.ownerPlayerId = 101; projectile.type = "rocket"; projectile.lifecycle = "active";
        values.push_back({projectile, "shot-fired"});
        R::WorldEntityState bonus;
        bonus.entityId = 61; bonus.kind = R::EntityKind::BonusPickup;
        bonus.type = "shield"; bonus.lifecycle = "available";
        values.push_back({bonus, "bonus-picked"});
        R::WorldEntityState weapon;
        weapon.entityId = 62; weapon.kind = R::EntityKind::WeaponPickup;
        weapon.type = "bazooka"; weapon.lifecycle = "available";
        values.push_back({weapon, "weapon-picked"});
        return values;
    }();

    std::vector<bool> publisherRejected;
    std::vector<bool> clientRejectedWithoutMutation;
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        auto live = activeState();
        live.entities = {scenarios[index].first};
        auto removed = live;
        removed.phaseTime++;
        removed.entities.clear();
        auto afterEvent = removed;
        afterEvent.phaseTime++;
        const R::PresentationEvent event{
                static_cast<R::Identity>(800 + index), scenarios[index].second, 101, 0,
                scenarios[index].first.entityId, 1};

        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(live));
        const auto removal = publisher.publish(removed);
        D6R_REQUIRE(removal.has_value());
        publisherRejected.push_back(!publisher.publish(afterEvent, {event}).has_value()
                                    && publisher.version() == 2);

        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, live}) == R::ApplyResult::Applied);
        D6R_REQUIRE(client.apply(*removal) == R::ApplyResult::Applied);
        auto transient = validUpdate(removed, afterEvent, {event});
        transient.baseline = 2;
        transient.version = 3;
        clientRejectedWithoutMutation.push_back(
                client.apply(transient) == R::ApplyResult::ResynchronizationRequired
                && client.version() == 2 && !client.current()
                && client.takePresentationEvents().empty());
    }
    const bool publisherAllRejected = publisherRejected == std::vector<bool>({true, true, true});
    const bool clientAllRejected = clientRejectedWithoutMutation == std::vector<bool>({true, true, true});
    const std::string evidence = "publisher=" + std::string(publisherAllRejected ? "true" : "false")
            + ";incremental=" + (clientAllRejected ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("publisher=true;incremental=true"), evidence);
}

D6R_TEST_CASE("REP-012 authoritative publisher rejects client-invalid event references without version mutation") {
    const auto initial = activeState();
    const auto rejectedAtInitialVersion = [&](const R::PresentationEvent &event) {
        auto afterEvent = initial;
        afterEvent.phaseTime++;
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(initial));
        const auto before = publisher.fullSnapshot();
        const bool rejected = !publisher.publish(afterEvent, {event}).has_value();
        const auto after = publisher.fullSnapshot();
        return rejected && publisher.version() == 1 && before && after
                && after->version == before->version
                && after->state.phaseTime == before->state.phaseTime;
    };

    const bool absentSource = rejectedAtInitialVersion({900, "shot-fired", 999, 0, 777, 1});
    const bool absentTarget = rejectedAtInitialVersion({900, "shot-fired", 101, 999, 777, 1});
    const bool firstSeenNonCreation = rejectedAtInitialVersion({900, "shot-hit", 101, 102, 777, 1});

    auto afterShot = initial;
    afterShot.phaseTime++;
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    D6R_REQUIRE(publisher.publish(afterShot, {{900, "shot-fired", 101, 0, 777, 1}}).has_value());
    const auto beforeReuse = publisher.fullSnapshot();
    auto afterReuse = afterShot;
    afterReuse.phaseTime++;
    const bool incompatibleKindReuse = !publisher.publish(
            afterReuse, {{901, "bonus-picked", 101, 0, 777, 1}}).has_value()
            && publisher.version() == 2 && beforeReuse && publisher.fullSnapshot()
            && publisher.fullSnapshot()->version == beforeReuse->version
            && publisher.fullSnapshot()->state.phaseTime == beforeReuse->state.phaseTime;

    const std::string evidence = "absent-source=" + std::string(absentSource ? "true" : "false")
            + ";absent-target=" + (absentTarget ? "true" : "false")
            + ";first-seen-non-creation=" + (firstSeenNonCreation ? "true" : "false")
            + ";incompatible-kind-reuse=" + (incompatibleKindReuse ? "true" : "false");
    D6R_REQUIRE_EQ(std::string(
            "absent-source=true;absent-target=true;first-seen-non-creation=true;incompatible-kind-reuse=true"),
            evidence);
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 final summary keeps final-round match outcome separate from cumulative ranking") {
    const auto final = distinctFinalSummary();
    D6R_REQUIRE(R::validateCanonicalState(final));
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), final.round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), final.score.winner.winnerPlayerIds);
    D6R_REQUIRE_EQ(101u, final.score.ranking.front());

    R::ReplicatedState reconnect;
    D6R_REQUIRE(reconnect.apply({8, final}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), reconnect.state()->round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), reconnect.state()->score.winner.winnerPlayerIds);
    D6R_REQUIRE_EQ(final.result.serialized, reconnect.state()->result.serialized);
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 final-round outcome and cumulative ranking survive incremental and reconnect") {
    auto summary = activeState();
    summary.phase = R::Phase::RoundSummary;
    summary.currentRoundNumber = 2;
    summary.completedRounds = 1;
    summary.round->roundId = 41;
    summary.round->roundNumber = 2;
    summary.round->outcome.winnerPlayerIds = {102};
    summary.score.winner = summary.round->outcome;
    summary.entities.clear();
    summary.effects.clear();
    summary.messages.scoreSummaryVisible = true;
    const auto final = distinctFinalSummary();

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(summary));
    const auto update = publisher.publish(final, {{601, "result-transition", 0, 0, 0, 0}});
    D6R_REQUIRE(update.has_value());
    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply({1, summary}) == R::ApplyResult::Applied);
    D6R_REQUIRE(incremental.apply(*update) == R::ApplyResult::Applied);
    R::ReplicatedState reconnect;
    D6R_REQUIRE(reconnect.apply(*publisher.fullSnapshot()) == R::ApplyResult::Applied);
    requireCoreStateEqual(*reconnect.state(), *incremental.state());
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), incremental.state()->round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), incremental.state()->score.winner.winnerPlayerIds);
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 interruption discards incomplete round and retains completed outcome in following lobby") {
    auto interrupted = distinctFinalSummary();
    interrupted.phase = R::Phase::Lobby;
    interrupted.completedRounds = 1;
    interrupted.currentRoundNumber = 1;
    interrupted.result.state = "Interrupted";
    interrupted.result.serialized = "interrupted;completed-round-1=101;match-outcome=no-winner";
    interrupted.round->roundNumber = 1;
    interrupted.round->roundId = 40;
    interrupted.round->outcome.winnerPlayerIds = {101};
    interrupted.score.winner = {};
    interrupted.score.winner.noWinner = true;
    interrupted.participants[0].ready = false;
    interrupted.participants[1].ready = false;
    interrupted.messages.status = "Lobby";
    D6R_REQUIRE(R::validateCanonicalState(interrupted));
    R::ReplicatedState restored;
    D6R_REQUIRE(restored.apply({12, interrupted}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(std::string("Interrupted"), restored.state()->result.state);
    D6R_REQUIRE(restored.state()->score.winner.noWinner);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({101}), restored.state()->round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(1u, restored.state()->completedRounds);
    D6R_REQUIRE_EQ(101u, restored.state()->score.ranking.front());
    D6R_REQUIRE_EQ(3, scoreRow(*restored.state(), 101)->cumulativePoints);
    D6R_REQUIRE_EQ(2, scoreRow(*restored.state(), 102)->cumulativePoints);
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 completed result survives final summary and following lobby") {
    const auto finalSummary = distinctFinalSummary();
    D6R_REQUIRE(R::validateCanonicalState(finalSummary));
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({8, finalSummary}) == R::ApplyResult::Applied);

    auto followingLobby = finalSummary;
    followingLobby.phase = R::Phase::Lobby;
    followingLobby.participants[0].ready = false;
    followingLobby.participants[1].ready = false;
    followingLobby.messages.status = "Lobby";
    D6R_REQUIRE(R::validateCanonicalState(followingLobby));
    D6R_REQUIRE(client.apply({9, followingLobby}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(std::string("Completed"), client.state()->result.state);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), client.state()->round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), client.state()->score.winner.winnerPlayerIds);
    D6R_REQUIRE_EQ(101u, client.state()->score.ranking.front());
}

D6R_TEST_CASE("REP-017 REP-048 departed-only completed-result transitions are exact for snapshots and updates") {
    const auto initial = completedResultWithDepartureLabels();
    const auto legitimate = completedResultDeparture(initial);
    D6R_REQUIRE(R::validateCanonicalState(initial));
    D6R_REQUIRE(R::validateCanonicalState(legitimate));

    R::ReplicatedState snapshotClient;
    D6R_REQUIRE(snapshotClient.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(snapshotClient.apply({2, legitimate}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, snapshotClient.version());
    D6R_REQUIRE(snapshotClient.state() != nullptr);
    D6R_REQUIRE(snapshotClient.state()->players[1].lifeState == R::LifeState::Departed);
    D6R_REQUIRE_EQ(legitimate.result.serialized, snapshotClient.state()->result.serialized);

    const auto legitimateUpdate = validUpdate(initial, legitimate);
    R::ReplicatedState updateClient;
    D6R_REQUIRE(updateClient.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(updateClient.apply(legitimateUpdate) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, updateClient.version());
    D6R_REQUIRE(updateClient.state() != nullptr);
    D6R_REQUIRE(updateClient.state()->players[1].lifeState == R::LifeState::Departed);
    D6R_REQUIRE_EQ(legitimate.result.serialized, updateClient.state()->result.serialized);

    const std::vector<std::pair<std::string, std::function<void(std::string &)>>> attacks = {
            {"outcome", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"outcome\":\"player-102\"",
                                        "\"outcome\":\"player-101\""));
            }},
            {"winner", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"winner\":102", "\"winner\":101"));
            }},
            {"ranking", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"ranking\":[101,102]", "\"ranking\":[102,101]"));
            }},
            {"unrelated-row", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized,
                        "\"participantId\":20,\"departed\":false",
                        "\"participantId\":20,\"departed\":true"));
            }},
            {"score", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"score\":[3,2]", "\"score\":[4,2]"));
            }},
            {"round", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"round\":2", "\"round\":1"));
            }},
            {"malformed", [](auto &serialized) { serialized.pop_back(); }}};

    std::string evidence;
    const auto initialBytes = R::serializeReplicationSnapshot({1, initial});
    for (const auto &[name, alter]: attacks) {
        auto attacked = legitimate;
        alter(attacked.result.serialized);
        D6R_REQUIRE(R::validateCanonicalState(attacked));

        R::ReplicatedState full;
        D6R_REQUIRE(full.apply({1, initial}) == R::ApplyResult::Applied);
        const bool fullRejected = full.apply({2, attacked}) == R::ApplyResult::Invalid
                && full.version() == 1 && full.current() && full.state()
                && R::serializeReplicationSnapshot({full.version(), *full.state()}) == initialBytes;

        auto attackedUpdate = legitimateUpdate;
        attackedUpdate.result = attacked.result;
        R::ReplicatedState incremental;
        D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
        const bool updateRejected = incremental.apply(attackedUpdate) == R::ApplyResult::ResynchronizationRequired
                && incremental.version() == 1 && !incremental.current() && incremental.state() == nullptr;
        const bool unchangedAfterRecovery = updateRejected
                && incremental.apply({1, initial}) == R::ApplyResult::Applied
                && incremental.state()
                && R::serializeReplicationSnapshot({incremental.version(), *incremental.state()}) == initialBytes;

        if (!evidence.empty()) evidence += ';';
        evidence += name + "=" + (fullRejected && unchangedAfterRecovery ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string("outcome=true;winner=true;ranking=true;unrelated-row=true;score=true;"
                               "round=true;malformed=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-048 retained result departure consistency publisher validation") {
    std::string evidence;
    for (const bool interrupted: {false, true}) {
        const auto initial = retainedResultWithDepartureLabels(interrupted);
        const auto synchronized = completedResultDeparture(initial);
        auto resultOnly = synchronized;
        resultOnly.players[1].lifeState = R::LifeState::Alive;
        auto canonicalOnly = initial;
        canonicalOnly.players[1].lifeState = R::LifeState::Departed;
        auto unauthorizedRevival = synchronized;
        unauthorizedRevival.players[1].lifeState = R::LifeState::Alive;
        D6R_REQUIRE(R::validateCanonicalState(initial));
        D6R_REQUIRE(R::validateCanonicalState(synchronized));
        D6R_REQUIRE(R::validateCanonicalState(resultOnly));
        D6R_REQUIRE(R::validateCanonicalState(canonicalOnly));
        D6R_REQUIRE(R::validateCanonicalState(unauthorizedRevival));

        const auto accepted = [&] {
            R::AuthoritativeStateReplicator publisher;
            return publisher.initialize(initial) && publisher.publish(synchronized).has_value()
                    && publisher.version() == 2;
        }();
        const auto rejected = [&](const R::CanonicalState &candidate,
                                  const R::CanonicalState &baseline) {
            R::AuthoritativeStateReplicator publisher;
            if (!publisher.initialize(baseline)) return false;
            const auto before = publisher.fullSnapshot();
            const bool result = !publisher.publish(candidate).has_value();
            const auto after = publisher.fullSnapshot();
            return result && publisher.version() == 1 && before && after
                    && R::serializeReplicationSnapshot(*before)
                       == R::serializeReplicationSnapshot(*after);
        };
        const std::string prefix = interrupted ? "Interrupted" : "Completed";
        if (!evidence.empty()) evidence += ';';
        evidence += prefix + "/synchronized=" + (accepted ? "true" : "false")
                + ",result-only=" + (rejected(resultOnly, initial) ? "true" : "false")
                + ",canonical-only=" + (rejected(canonicalOnly, initial) ? "true" : "false")
                + ",departed-to-alive=" + (rejected(unauthorizedRevival, synchronized) ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string(
            "Completed/synchronized=true,result-only=true,canonical-only=true,departed-to-alive=true;"
            "Interrupted/synchronized=true,result-only=true,canonical-only=true,departed-to-alive=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-048 REP-066 retained result departure consistency client update validation") {
    std::string evidence;
    for (const bool interrupted: {false, true}) {
        const auto initial = retainedResultWithDepartureLabels(interrupted);
        const auto synchronized = completedResultDeparture(initial);
        const auto synchronizedUpdate = validUpdate(initial, synchronized);

        R::ReplicatedState acceptedClient;
        const bool accepted = acceptedClient.apply({1, initial}) == R::ApplyResult::Applied
                && acceptedClient.apply(synchronizedUpdate) == R::ApplyResult::Applied
                && acceptedClient.version() == 2 && acceptedClient.current() && acceptedClient.state()
                && acceptedClient.state()->players[1].lifeState == R::LifeState::Departed
                && acceptedClient.state()->result.serialized == synchronized.result.serialized;

        const auto rejected = [&](const R::IncrementalUpdate &candidate,
                                  const R::CanonicalState &baseline) {
            R::ReplicatedState client;
            return client.apply({1, baseline}) == R::ApplyResult::Applied
                    && client.apply(candidate) == R::ApplyResult::ResynchronizationRequired
                    && client.version() == 1 && !client.current() && client.state() == nullptr
                    && client.takePresentationEvents().empty();
        };

        auto resultOnly = synchronizedUpdate;
        resultOnly.players.clear();
        auto canonicalOnly = synchronizedUpdate;
        canonicalOnly.result = initial.result;

        auto benignAfterDeparture = synchronized;
        benignAfterDeparture.phaseTime++;
        auto unauthorizedRevival = validUpdate(synchronized, benignAfterDeparture);
        unauthorizedRevival.players.push_back(
                {R::ChangeKind::Update, initial.players[1].playerId, initial.players[1]});

        const std::string prefix = interrupted ? "Interrupted" : "Completed";
        if (!evidence.empty()) evidence += ';';
        evidence += prefix + "/synchronized=" + (accepted ? "true" : "false")
                + ",result-only=" + (rejected(resultOnly, initial) ? "true" : "false")
                + ",canonical-only=" + (rejected(canonicalOnly, initial) ? "true" : "false")
                + ",departed-to-alive=" + (rejected(unauthorizedRevival, synchronized) ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string(
            "Completed/synchronized=true,result-only=true,canonical-only=true,departed-to-alive=true;"
            "Interrupted/synchronized=true,result-only=true,canonical-only=true,departed-to-alive=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-041 REP-042 REP-066 initial first-result snapshot departure consistency") {
    D6R_REQUIRE_EQ(std::string(
            "Completed/synchronized=true,result-only=true,canonical-only=true;"
            "Interrupted/synchronized=true,result-only=true,canonical-only=true"),
            firstResultDepartureMatrix(FirstResultDelivery::InitialSnapshot));
}

D6R_TEST_CASE("REP-017 REP-040..044 REP-066 resynchronized first-result snapshot departure consistency") {
    D6R_REQUIRE_EQ(std::string(
            "Completed/synchronized=true,result-only=true,canonical-only=true;"
            "Interrupted/synchronized=true,result-only=true,canonical-only=true"),
            firstResultDepartureMatrix(FirstResultDelivery::ResynchronizationSnapshot));
}

D6R_TEST_CASE("REP-017 REP-048..051 REP-066 first-result active-to-terminal update departure consistency") {
    D6R_REQUIRE_EQ(std::string(
            "Completed/synchronized=true,result-only=true,canonical-only=true;"
            "Interrupted/synchronized=true,result-only=true,canonical-only=true"),
            firstResultDepartureMatrix(FirstResultDelivery::IncrementalUpdate));
}

D6R_TEST_CASE("REP-017 REP-048 maximum canonical completed result accepts only departed transitions") {
    constexpr std::size_t ExpectedMaximumParsedValues = 23512;
    const auto initial = maximumCompletedReplicationState(false);
    const auto departed = maximumCompletedReplicationState(true);
    D6R_REQUIRE(R::validateCanonicalState(initial));
    D6R_REQUIRE(R::validateCanonicalState(departed));
    const auto initialValues = JsonValueCounter(initial.result.serialized).count();
    const auto departedValues = JsonValueCounter(departed.result.serialized).count();
    D6R_REQUIRE(initialValues.has_value());
    D6R_REQUIRE(departedValues.has_value());
    D6R_REQUIRE_EQ(ExpectedMaximumParsedValues, *initialValues);
    D6R_REQUIRE_EQ(ExpectedMaximumParsedValues, *departedValues);
    D6R_REQUIRE(initial.result.serialized.size() <= R::MaxReplicatedResultBytes);

    R::ReplicatedState full;
    D6R_REQUIRE(full.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(full.apply({2, departed}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, full.version());
    D6R_REQUIRE(full.state() != nullptr);
    D6R_REQUIRE(full.state()->players.back().lifeState == R::LifeState::Departed);
    D6R_REQUIRE_EQ(departed.result.serialized, full.state()->result.serialized);

    const auto legitimateUpdate = validUpdate(initial, departed);
    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(incremental.apply(legitimateUpdate) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, incremental.version());
    D6R_REQUIRE(incremental.state() != nullptr);
    D6R_REQUIRE(incremental.state()->players.back().lifeState == R::LifeState::Departed);
    D6R_REQUIRE_EQ(departed.result.serialized, incremental.state()->result.serialized);

    struct Attack {
        std::string name;
        std::function<void(std::string &)> alter;
        std::optional<std::size_t> expectedValues;
    };
    const std::vector<Attack> attacks = {
            {"over-bound", [](auto &serialized) {
                serialized.insert(serialized.size() - 1, ",\"overBound\":null");
            }, ExpectedMaximumParsedValues + 1},
            {"malformed", [](auto &serialized) { serialized.pop_back(); }, std::nullopt},
            {"mixed-mutation", [](auto &serialized) {
                D6R_REQUIRE(replaceOnce(serialized, "\"completedRounds\":99",
                                        "\"completedRounds\":98"));
            }, ExpectedMaximumParsedValues}};

    const auto acceptedBytes = R::serializeReplicationSnapshot({1, initial});
    std::string evidence;
    for (const auto &attack: attacks) {
        auto attacked = departed;
        attack.alter(attacked.result.serialized);
        D6R_REQUIRE(R::validateCanonicalState(attacked));
        const auto counted = JsonValueCounter(attacked.result.serialized).count();
        D6R_REQUIRE_EQ(attack.expectedValues.has_value(), counted.has_value());
        if (counted) D6R_REQUIRE_EQ(*attack.expectedValues, *counted);

        R::ReplicatedState snapshotClient;
        D6R_REQUIRE(snapshotClient.apply({1, initial}) == R::ApplyResult::Applied);
        const bool snapshotRejected = snapshotClient.apply({2, attacked}) == R::ApplyResult::Invalid
                && snapshotClient.version() == 1 && snapshotClient.current() && snapshotClient.state()
                && R::serializeReplicationSnapshot(
                        {snapshotClient.version(), *snapshotClient.state()}) == acceptedBytes;

        auto attackedUpdate = legitimateUpdate;
        attackedUpdate.result = attacked.result;
        R::ReplicatedState updateClient;
        D6R_REQUIRE(updateClient.apply({1, initial}) == R::ApplyResult::Applied);
        const bool updateRejected = updateClient.apply(attackedUpdate)
                                    == R::ApplyResult::ResynchronizationRequired
                && updateClient.version() == 1 && !updateClient.current() && updateClient.state() == nullptr;
        const bool retained = updateRejected
                && updateClient.apply({1, initial}) == R::ApplyResult::Applied
                && updateClient.state()
                && R::serializeReplicationSnapshot(
                        {updateClient.version(), *updateClient.state()}) == acceptedBytes;

        if (!evidence.empty()) evidence += ';';
        evidence += attack.name + "=" + (snapshotRejected && retained ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string("over-bound=true;malformed=true;mixed-mutation=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-048 near-limit twentieth result member is rejected before state mutation") {
    constexpr std::size_t ExpectedMaximumParsedValues = 23512;
    const auto initial = maximumCompletedReplicationState(false);
    const auto departed = maximumCompletedReplicationState(true);

    JsonValueCounter canonicalShape(departed.result.serialized);
    const auto canonicalValues = canonicalShape.count();
    D6R_REQUIRE(canonicalValues.has_value());
    D6R_REQUIRE_EQ(ExpectedMaximumParsedValues, *canonicalValues);
    D6R_REQUIRE_EQ(19u, canonicalShape.rootMembers());

    auto wide = departed;
    auto wideResult = maximumCompletedResult(true);
    wideResult.finalWinnerPlayerIds.pop_back();
    const auto serializedWideBase = A::serializeSessionResult(wideResult);
    D6R_REQUIRE(serializedWideBase.has_value());
    wide.result.serialized = *serializedWideBase;
    wide.result.serialized.insert(wide.result.serialized.size() - 1, ",\"twentieth\":null");
    JsonValueCounter wideShape(wide.result.serialized);
    const auto wideValues = wideShape.count();
    D6R_REQUIRE(wideValues.has_value());
    D6R_REQUIRE_EQ(ExpectedMaximumParsedValues, *wideValues);
    D6R_REQUIRE_EQ(20u, wideShape.rootMembers());
    D6R_REQUIRE(R::validateCanonicalState(wide));

    auto duplicate = departed;
    D6R_REQUIRE(replaceOnce(duplicate.result.serialized,
            "\"label\":\"Maximum canonical result\"",
            "\"state\":\"Maximum canonical result\""));
    JsonValueCounter duplicateShape(duplicate.result.serialized);
    const auto duplicateValues = duplicateShape.count();
    D6R_REQUIRE(duplicateValues.has_value());
    D6R_REQUIRE_EQ(ExpectedMaximumParsedValues, *duplicateValues);
    D6R_REQUIRE_EQ(19u, duplicateShape.rootMembers());
    D6R_REQUIRE(R::validateCanonicalState(duplicate));

    const auto legitimateUpdate = validUpdate(initial, departed);
    const auto acceptedBytes = R::serializeReplicationSnapshot({1, initial});
    std::string evidence;
    for (const auto &[name, attack]: std::vector<std::pair<std::string, R::CanonicalState>>{
            {"twentieth-member", wide}, {"duplicate-key", duplicate}}) {
        R::ReplicatedState full;
        D6R_REQUIRE(full.apply({1, initial}) == R::ApplyResult::Applied);
        const bool fullRejected = full.apply({2, attack}) == R::ApplyResult::Invalid
                && full.version() == 1 && full.current() && full.state()
                && R::serializeReplicationSnapshot({full.version(), *full.state()}) == acceptedBytes;

        auto attackedUpdate = legitimateUpdate;
        attackedUpdate.result = attack.result;
        R::ReplicatedState incremental;
        D6R_REQUIRE(incremental.apply({1, initial}) == R::ApplyResult::Applied);
        const bool resynchronizing = incremental.apply(attackedUpdate)
                                     == R::ApplyResult::ResynchronizationRequired
                && incremental.version() == 1 && !incremental.current() && incremental.state() == nullptr;
        const bool retained = resynchronizing
                && incremental.apply({1, initial}) == R::ApplyResult::Applied
                && incremental.current() && incremental.state()
                && R::serializeReplicationSnapshot(
                        {incremental.version(), *incremental.state()}) == acceptedBytes;

        if (!evidence.empty()) evidence += ';';
        evidence += name + "=" + (fullRejected && retained ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string("twentieth-member=true;duplicate-key=true"), evidence);

    R::ReplicatedState maximumSnapshot;
    D6R_REQUIRE(maximumSnapshot.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(maximumSnapshot.apply({2, departed}) == R::ApplyResult::Applied);
    R::ReplicatedState maximumIncremental;
    D6R_REQUIRE(maximumIncremental.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(maximumIncremental.apply(legitimateUpdate) == R::ApplyResult::Applied);
}

D6R_TEST_CASE("REP-017 REP-025 REP-048 publisher freezes retained result while following Lobby remains editable") {
    std::string evidence;
    for (const auto &scenario: retainedResultMutations()) {
        auto altered = scenario.before;
        scenario.alter(altered);
        D6R_REQUIRE(R::validateCanonicalState(scenario.before));
        D6R_REQUIRE(R::validateCanonicalState(altered));

        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(scenario.before));
        const auto before = publisher.fullSnapshot();
        const R::PresentationEvent attemptedEvent{970, "result-transition", 0, 0, 0, 0};
        const bool rejected = !publisher.publish(altered, {attemptedEvent}).has_value();
        const auto after = publisher.fullSnapshot();
        const bool atomic = rejected && before && after && publisher.version() == 1
                && R::serializeReplicationSnapshot(*before) == R::serializeReplicationSnapshot(*after);

        bool legitimateChangesApplied = false;
        bool identityAndEventHistoryPreserved = false;
        if (atomic) {
            const auto legitimate = legitimateFollowingLobbyChange(scenario.before);
            const auto accepted = publisher.publish(legitimate, {attemptedEvent});
            legitimateChangesApplied = accepted && publisher.version() == 2
                    && accepted->events.size() == 1 && accepted->events.front().eventId == attemptedEvent.eventId
                    && publisher.fullSnapshot()->state.settings.assistance == legitimate.settings.assistance
                    && publisher.fullSnapshot()->state.participants[0].ready
                    && publisher.fullSnapshot()->state.participants[1].connection == R::ConnectionState::Reconnecting
                    && publisher.fullSnapshot()->state.players[0].rosterPosition == 1
                    && publisher.fullSnapshot()->state.players[1].rosterPosition == 0
                    && publisher.fullSnapshot()->state.players[1].displayName == "Guest (Edited)"
                    && publisher.fullSnapshot()->state.players[1].lifeState == R::LifeState::Alive;
            if (legitimateChangesApplied) {
                requireRetainedResultEqual(scenario.before, publisher.fullSnapshot()->state);
                auto departed = legitimate;
                departed.participants.pop_back();
                departed.players.pop_back();
                const auto removal = publisher.publish(departed);
                const bool rosterRemovalApplied = removal && publisher.version() == 3
                        && publisher.fullSnapshot()->state.participants.size() == 1
                        && publisher.fullSnapshot()->state.players.size() == 1
                        && publisher.fullSnapshot()->state.score.players.size() == 2;
                const auto beforeReuse = publisher.fullSnapshot();
                const bool issuedPlayerCannotReturn = !publisher.publish(legitimate).has_value()
                        && publisher.version() == 3 && beforeReuse && publisher.fullSnapshot()
                        && R::serializeReplicationSnapshot(*beforeReuse)
                           == R::serializeReplicationSnapshot(*publisher.fullSnapshot());
                identityAndEventHistoryPreserved = rosterRemovalApplied && issuedPlayerCannotReturn;
            }
        }
        if (!evidence.empty()) evidence += ';';
        evidence += scenario.name + "=" + (atomic && legitimateChangesApplied
                && identityAndEventHistoryPreserved ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string("score-values=true;ranking-order=true;result-state=true;match-outcome=true;"
                               "completed-round-outcome=true;serialized-result=true;"
                               "round-counters-and-number=true;round-id=true;round-level=true;"
                               "round-orientation=true;round-roster-order=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-025 REP-048 REP-066 client rejects retained result alteration atomically") {
    std::string evidence;
    for (const auto &scenario: retainedResultMutations()) {
        auto altered = scenario.before;
        scenario.alter(altered);
        D6R_REQUIRE(R::validateCanonicalState(scenario.before));
        D6R_REQUIRE(R::validateCanonicalState(altered));

        auto benign = scenario.before;
        benign.phaseTime++;
        const R::PresentationEvent attemptedEvent{971, "result-transition", 0, 0, 0, 0};
        auto attack = validUpdate(scenario.before, benign, {attemptedEvent});
        attack.currentRoundNumber = altered.currentRoundNumber;
        attack.completedRounds = altered.completedRounds;
        attack.round = altered.round;
        attack.score = altered.score;
        attack.result = altered.result;

        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, scenario.before}) == R::ApplyResult::Applied);
        const auto result = client.apply(attack);
        const bool rejected = result == R::ApplyResult::ResynchronizationRequired
                && client.version() == 1 && !client.current() && client.state() == nullptr
                && client.takePresentationEvents().empty();

        bool legitimateChangesApplied = false;
        bool identityAndEventHistoryPreserved = false;
        if (rejected && client.apply({1, scenario.before}) == R::ApplyResult::Applied) {
            requireRetainedResultEqual(scenario.before, *client.state());
            const auto legitimate = legitimateFollowingLobbyChange(scenario.before);
            auto accepted = validUpdate(scenario.before, legitimate, {attemptedEvent});
            legitimateChangesApplied = client.apply(accepted) == R::ApplyResult::Applied
                    && client.version() == 2 && client.state()
                    && client.state()->settings.assistance == legitimate.settings.assistance
                    && client.state()->participants[0].ready
                    && client.state()->participants[1].connection == R::ConnectionState::Reconnecting
                    && client.state()->players[0].rosterPosition == 1
                    && client.state()->players[1].rosterPosition == 0
                    && client.state()->players[1].displayName == "Guest (Edited)"
                    && client.state()->players[1].lifeState == R::LifeState::Alive;
            if (legitimateChangesApplied) {
                const auto delivered = client.takePresentationEvents();
                requireRetainedResultEqual(scenario.before, *client.state());
                auto departed = legitimate;
                departed.participants.pop_back();
                departed.players.pop_back();
                auto removal = validUpdate(legitimate, departed);
                removal.baseline = 2;
                removal.version = 3;
                const bool rosterRemovalApplied = client.apply(removal) == R::ApplyResult::Applied
                        && client.state()->participants.size() == 1 && client.state()->players.size() == 1
                        && client.state()->score.players.size() == 2;
                const auto reuse = lobbyCreationUpdate(3, 4, legitimate,
                        legitimate.participants.back(), legitimate.players.back());
                const bool issuedPlayerCannotReturn = client.apply(reuse) == R::ApplyResult::ResynchronizationRequired
                        && client.version() == 3 && client.takePresentationEvents().empty();
                identityAndEventHistoryPreserved = delivered.size() == 1
                        && delivered.front().eventId == attemptedEvent.eventId
                        && rosterRemovalApplied && issuedPlayerCannotReturn;
            }
        }
        if (!evidence.empty()) evidence += ';';
        evidence += scenario.name + "=" + (rejected && legitimateChangesApplied
                && identityAndEventHistoryPreserved ? "true" : "false");
    }
    D6R_REQUIRE_EQ(std::string("score-values=true;ranking-order=true;result-state=true;match-outcome=true;"
                               "completed-round-outcome=true;serialized-result=true;"
                               "round-counters-and-number=true;round-id=true;round-level=true;"
                               "round-orientation=true;round-roster-order=true"), evidence);
}

D6R_TEST_CASE("REP-017 REP-025 REP-048 publisher freezes Final Summary result through Final Summary and Lobby") {
    std::string failures;
    for (const auto &scenario: finalSummaryPhaseMutations()) {
        D6R_REQUIRE(R::validateCanonicalState(scenario.before));
        D6R_REQUIRE(R::validateCanonicalState(scenario.benign));
        D6R_REQUIRE(R::validateCanonicalState(scenario.altered));
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(scenario.before));
        const auto before = publisher.fullSnapshot();
        const R::PresentationEvent event{972, "result-transition", 0, 0, 0, 0};
        const bool rejected = !publisher.publish(scenario.altered, {event});
        const auto after = publisher.fullSnapshot();
        const bool stateAndVersionUnchanged = rejected && before && after && publisher.version() == 1
                && R::serializeReplicationSnapshot(*before) == R::serializeReplicationSnapshot(*after);

        const auto legitimate = legitimateFollowingLobbyChange(retainedCompletedLobby());
        const auto accepted = stateAndVersionUnchanged ? publisher.publish(legitimate, {event}) : std::nullopt;
        const bool eventHistoryUnchanged = accepted && accepted->events.size() == 1
                && accepted->events.front().eventId == event.eventId;
        bool identityHistoryUnchanged = false;
        if (eventHistoryUnchanged) {
            auto removed = legitimate;
            removed.participants.pop_back();
            removed.players.pop_back();
            const auto removal = publisher.publish(removed);
            const auto beforeReuse = publisher.fullSnapshot();
            identityHistoryUnchanged = removal && publisher.version() == 3
                    && !publisher.publish(legitimate).has_value() && publisher.version() == 3
                    && beforeReuse && publisher.fullSnapshot()
                    && R::serializeReplicationSnapshot(*beforeReuse)
                       == R::serializeReplicationSnapshot(*publisher.fullSnapshot());
        }
        if (!(stateAndVersionUnchanged && eventHistoryUnchanged && identityHistoryUnchanged)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-017 REP-025 REP-048 REP-066 client freezes Final Summary result through Final Summary and Lobby") {
    std::string failures;
    for (const auto &scenario: finalSummaryPhaseMutations()) {
        D6R_REQUIRE(R::validateCanonicalState(scenario.before));
        D6R_REQUIRE(R::validateCanonicalState(scenario.benign));
        D6R_REQUIRE(R::validateCanonicalState(scenario.altered));
        const R::PresentationEvent event{973, "result-transition", 0, 0, 0, 0};
        const auto attack = retainedAttackUpdate(scenario, event);
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, scenario.before}) == R::ApplyResult::Applied);
        const bool rejected = client.apply(attack) == R::ApplyResult::ResynchronizationRequired
                && client.version() == 1 && !client.current() && client.state() == nullptr
                && client.takePresentationEvents().empty();
        const bool restored = rejected && client.apply({1, scenario.before}) == R::ApplyResult::Applied
                && client.state()
                && R::serializeReplicationSnapshot({1, scenario.before})
                   == R::serializeReplicationSnapshot({client.version(), *client.state()});

        const auto legitimate = legitimateFollowingLobbyChange(retainedCompletedLobby());
        const auto acceptedUpdate = validUpdate(scenario.before, legitimate, {event});
        const bool legitimateApplied = restored && client.apply(acceptedUpdate) == R::ApplyResult::Applied
                && client.version() == 2 && client.state()
                && client.state()->settings.assistance == legitimate.settings.assistance;
        const auto delivered = client.takePresentationEvents();
        bool identityHistoryUnchanged = false;
        if (legitimateApplied && delivered.size() == 1 && delivered.front().eventId == event.eventId) {
            auto removed = legitimate;
            removed.participants.pop_back();
            removed.players.pop_back();
            auto removal = validUpdate(legitimate, removed);
            removal.baseline = 2;
            removal.version = 3;
            const bool removedApplied = client.apply(removal) == R::ApplyResult::Applied;
            const auto reuse = lobbyCreationUpdate(3, 4, legitimate,
                    legitimate.participants.back(), legitimate.players.back());
            identityHistoryUnchanged = removedApplied
                    && client.apply(reuse) == R::ApplyResult::ResynchronizationRequired
                    && client.version() == 3 && client.takePresentationEvents().empty();
        }
        if (!(rejected && restored && legitimateApplied && delivered.size() == 1
              && delivered.front().eventId == event.eventId && identityHistoryUnchanged)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-041 REP-044 REP-050 NET-AC-018 publisher rejects following Lobby to Final Summary reversal atomically") {
    const auto followingLobby = retainedCompletedLobby();
    const auto reversedSummary = distinctFinalSummary();
    D6R_REQUIRE(R::validateCanonicalState(followingLobby));
    D6R_REQUIRE(R::validateCanonicalState(reversedSummary));
    D6R_REQUIRE_EQ(followingLobby.matchId, reversedSummary.matchId);
    D6R_REQUIRE_EQ(followingLobby.result.serialized, reversedSummary.result.serialized);

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(followingLobby));
    const auto before = publisher.fullSnapshot();
    const R::PresentationEvent attemptedEvent{980, "result-transition", 0, 0, 0, 0};
    D6R_REQUIRE(!publisher.publish(reversedSummary, {attemptedEvent}).has_value());
    D6R_REQUIRE_EQ(1u, publisher.version());
    D6R_REQUIRE(before.has_value());
    D6R_REQUIRE(publisher.fullSnapshot().has_value());
    D6R_REQUIRE_EQ(R::serializeReplicationSnapshot(*before),
                   R::serializeReplicationSnapshot(*publisher.fullSnapshot()));

    const auto legitimateLobby = legitimateFollowingLobbyChange(followingLobby);
    const auto legitimateUpdate = publisher.publish(legitimateLobby, {attemptedEvent});
    D6R_REQUIRE(legitimateUpdate.has_value());
    D6R_REQUIRE_EQ(2u, publisher.version());
    D6R_REQUIRE_EQ(1u, legitimateUpdate->events.size());
    D6R_REQUIRE_EQ(attemptedEvent.eventId, legitimateUpdate->events.front().eventId);
    requireRetainedResultEqual(followingLobby, publisher.fullSnapshot()->state);

    const auto freshMatch = freshMatchAfterRetainedResult();
    D6R_REQUIRE(R::validateCanonicalState(freshMatch));
    const auto matchStart = publisher.publish(freshMatch);
    D6R_REQUIRE(matchStart.has_value());
    D6R_REQUIRE_EQ(3u, publisher.version());
    D6R_REQUIRE_EQ(31u, publisher.fullSnapshot()->state.matchId);
    D6R_REQUIRE(!publisher.fullSnapshot()->state.result.available);
}

D6R_TEST_CASE("REP-045..050 REP-066 REP-AC-006 REP-AC-007 NET-AC-018 client rejects following Lobby to Final Summary reversal atomically") {
    const auto followingLobby = retainedCompletedLobby();
    const auto reversedSummary = distinctFinalSummary();
    const R::PresentationEvent attemptedEvent{981, "result-transition", 0, 0, 0, 0};
    const auto reversal = stateOnlyUpdate(1, 2, reversedSummary, {attemptedEvent});

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, followingLobby}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(reversal) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(1u, client.version());
    D6R_REQUIRE(!client.current());
    D6R_REQUIRE(client.state() == nullptr);
    D6R_REQUIRE(client.takePresentationEvents().empty());

    D6R_REQUIRE(client.apply({1, followingLobby}) == R::ApplyResult::Applied);
    const auto legitimateLobby = legitimateFollowingLobbyChange(followingLobby);
    const auto legitimateUpdate = validUpdate(followingLobby, legitimateLobby, {attemptedEvent});
    D6R_REQUIRE(client.apply(legitimateUpdate) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, client.version());
    const auto events = client.takePresentationEvents();
    D6R_REQUIRE_EQ(1u, events.size());
    D6R_REQUIRE_EQ(attemptedEvent.eventId, events.front().eventId);
    requireRetainedResultEqual(followingLobby, *client.state());

    const auto freshMatch = freshMatchAfterRetainedResult();
    auto matchStart = validUpdate(legitimateLobby, freshMatch);
    matchStart.baseline = 2;
    matchStart.version = 3;
    D6R_REQUIRE(client.apply(matchStart) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(31u, client.state()->matchId);
    D6R_REQUIRE(!client.state()->result.available);
}

D6R_TEST_CASE("REP-041 REP-042 REP-044 REP-050 REP-066 REP-AC-007 NET-AC-018 full snapshot rejects following Lobby to Final Summary reversal atomically") {
    const auto followingLobby = retainedCompletedLobby();
    const auto reversedSummary = distinctFinalSummary();
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, followingLobby}) == R::ApplyResult::Applied);
    const auto accepted = R::serializeReplicationSnapshot({1, *client.state()});

    D6R_REQUIRE(client.apply({2, reversedSummary}) == R::ApplyResult::Invalid);
    D6R_REQUIRE_EQ(1u, client.version());
    D6R_REQUIRE(client.current());
    D6R_REQUIRE(client.state() != nullptr);
    D6R_REQUIRE_EQ(accepted, R::serializeReplicationSnapshot({client.version(), *client.state()}));

    const auto legitimateLobby = legitimateFollowingLobbyChange(followingLobby);
    D6R_REQUIRE(client.apply({2, legitimateLobby}) == R::ApplyResult::Applied);
    requireRetainedResultEqual(followingLobby, *client.state());

    const auto freshMatch = freshMatchAfterRetainedResult();
    D6R_REQUIRE(client.apply({3, freshMatch}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(31u, client.state()->matchId);
    D6R_REQUIRE(!client.state()->result.available);
}

D6R_TEST_CASE("REP-044 REP-066 NET-AC-018 publisher rejects following Lobby lifecycle rewinds transactionally") {
    const auto retained = retainedCompletedLobby();
    const auto freshMatch = freshMatchAfterRetainedResult();
    D6R_REQUIRE(R::validateCanonicalState(retained));
    D6R_REQUIRE(R::validateCanonicalState(freshMatch));

    std::string failures;
    for (const auto &scenario: followingLobbyLifecycleAttacks()) {
        D6R_REQUIRE(R::validateCanonicalState(scenario.state));
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(retained));
        const auto before = publisher.fullSnapshot();
        const R::PresentationEvent event{982, "match-started", 0, 0, 0, 0};

        const bool rejected = !publisher.publish(scenario.state, {event}).has_value();
        const auto after = publisher.fullSnapshot();
        const bool stateAndVersionUnchanged = rejected && before && after && publisher.version() == 1
                && R::serializeReplicationSnapshot(*before) == R::serializeReplicationSnapshot(*after);

        const auto legitimate = stateAndVersionUnchanged
                ? publisher.publish(freshMatch, {event}) : std::nullopt;
        const bool rejectedIdentityReused = legitimate && scenario.state.round && legitimate->round
                && ((scenario.state.matchId == freshMatch.matchId
                     && legitimate->matchId == scenario.state.matchId)
                    || (scenario.state.round->roundId == freshMatch.round->roundId
                        && legitimate->round->roundId == scenario.state.round->roundId));
        const bool historyUnchanged = legitimate && publisher.version() == 2
                && legitimate->baseline == 1 && legitimate->version == 2
                && rejectedIdentityReused
                && legitimate->events.size() == 1 && legitimate->events.front().eventId == event.eventId;
        const bool newMatchApplied = historyUnchanged && publisher.fullSnapshot()
                && publisher.fullSnapshot()->state.phase == R::Phase::ActiveRound
                && publisher.fullSnapshot()->state.matchId == 31
                && publisher.fullSnapshot()->state.round
                && publisher.fullSnapshot()->state.round->roundId == 42
                && !publisher.fullSnapshot()->state.result.available;
        if (!(stateAndVersionUnchanged && historyUnchanged && newMatchApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-044 REP-066 NET-AC-018 incremental rejects following Lobby lifecycle rewinds transactionally") {
    const auto retained = retainedCompletedLobby();
    const auto freshMatch = freshMatchAfterRetainedResult();
    std::string failures;
    for (const auto &scenario: followingLobbyLifecycleAttacks()) {
        const R::PresentationEvent event{983, "match-started", 0, 0, 0, 0};
        auto attack = stateOnlyUpdate(1, 2, scenario.state, {event});
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, retained}) == R::ApplyResult::Applied);
        const auto acceptedBefore = R::serializeReplicationSnapshot({1, *client.state()});

        const bool rejected = client.apply(attack) == R::ApplyResult::ResynchronizationRequired
                && client.version() == 1 && !client.current() && client.state() == nullptr
                && client.takePresentationEvents().empty();
        const bool restored = rejected && client.apply({1, retained}) == R::ApplyResult::Applied
                && client.current() && client.version() == 1 && client.state()
                && acceptedBefore == R::serializeReplicationSnapshot({client.version(), *client.state()});

        auto legitimate = validUpdate(retained, freshMatch, {event});
        const bool rejectedIdentityReused = scenario.state.round && legitimate.round
                && ((scenario.state.matchId == freshMatch.matchId
                     && legitimate.matchId == scenario.state.matchId)
                    || (scenario.state.round->roundId == freshMatch.round->roundId
                        && legitimate.round->roundId == scenario.state.round->roundId));
        const bool historyUnchanged = restored && client.apply(legitimate) == R::ApplyResult::Applied
                && client.version() == 2 && client.state()
                && rejectedIdentityReused;
        const auto events = client.takePresentationEvents();
        const bool newMatchApplied = historyUnchanged && client.state()->phase == R::Phase::ActiveRound
                && !client.state()->result.available && events.size() == 1
                && events.front().eventId == event.eventId;
        if (!(rejected && restored && historyUnchanged && newMatchApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-042 REP-044 REP-066 NET-AC-018 full snapshot rejects following Lobby lifecycle rewinds transactionally") {
    const auto retained = retainedCompletedLobby();
    const auto freshMatch = freshMatchAfterRetainedResult();
    std::string failures;
    for (const auto &scenario: followingLobbyLifecycleAttacks()) {
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, retained}) == R::ApplyResult::Applied);
        const auto acceptedBefore = R::serializeReplicationSnapshot({1, *client.state()});

        const bool rejected = client.apply({2, scenario.state}) == R::ApplyResult::Invalid;
        const bool stateAndVersionUnchanged = rejected && client.current() && client.version() == 1
                && client.state()
                && acceptedBefore == R::serializeReplicationSnapshot({client.version(), *client.state()});
        const bool rejectedIdentityWillBeReused = scenario.state.round && freshMatch.round
                && (scenario.state.matchId == freshMatch.matchId
                    || scenario.state.round->roundId == freshMatch.round->roundId);
        const bool historyUnchanged = stateAndVersionUnchanged
                && client.apply({2, freshMatch}) == R::ApplyResult::Applied
                && client.current() && client.version() == 2 && client.state()
                && rejectedIdentityWillBeReused;
        const bool newMatchApplied = historyUnchanged && client.state()->phase == R::Phase::ActiveRound
                && !client.state()->result.available;
        if (!(stateAndVersionUnchanged && historyUnchanged && newMatchApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-005 REP-066 NET-AC-018 publisher rejects stale retained values in a fresh first round atomically") {
    const auto retained = retainedTeamCompletedLobby();
    const auto fresh = resetTeamFirstRound();
    D6R_REQUIRE(R::validateCanonicalState(retained));
    D6R_REQUIRE(R::validateCanonicalState(fresh));

    std::string failures;
    for (const auto &scenario: staleNewMatchMutations()) {
        auto stale = fresh;
        scenario.alter(stale);
        D6R_REQUIRE(R::validateCanonicalState(stale));
        D6R_REQUIRE_EQ(31u, stale.matchId);
        D6R_REQUIRE_EQ(42u, stale.round->roundId);

        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(retained));
        const auto before = publisher.fullSnapshot();
        const bool rejected = !publisher.publish(stale).has_value();
        const auto after = publisher.fullSnapshot();
        const bool atomic = rejected && before && after && publisher.version() == 1
                && R::serializeReplicationSnapshot(*before) == R::serializeReplicationSnapshot(*after);
        const auto accepted = atomic ? publisher.publish(fresh) : std::nullopt;
        const bool resetApplied = accepted && publisher.version() == 2
                && accepted->baseline == 1 && accepted->version == 2
                && publisher.fullSnapshot()->state.matchId == 31
                && publisher.fullSnapshot()->state.round->roundId == 42
                && publisher.fullSnapshot()->state.currentRoundNumber == 1
                && publisher.fullSnapshot()->state.completedRounds == 0
                && publisher.fullSnapshot()->state.score.players[0].cumulativePoints == 0
                && publisher.fullSnapshot()->state.score.ranking == std::vector<R::Identity>({101, 102})
                && publisher.fullSnapshot()->state.score.teamTotals == std::vector<std::int64_t>({0, 0});
        if (!(atomic && resetApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-005 REP-066 NET-AC-018 incremental rejects stale retained values in a fresh first round atomically") {
    const auto retained = retainedTeamCompletedLobby();
    const auto fresh = resetTeamFirstRound();
    std::string failures;
    for (const auto &scenario: staleNewMatchMutations()) {
        auto stale = fresh;
        scenario.alter(stale);
        auto attack = stateOnlyUpdate(1, 2, stale);
        for (const auto &value: stale.entities)
            attack.entities.push_back({R::ChangeKind::Create, value.entityId, value});

        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, retained}) == R::ApplyResult::Applied);
        const bool rejected = client.apply(attack) == R::ApplyResult::ResynchronizationRequired
                && client.version() == 1 && !client.current() && client.state() == nullptr
                && client.takePresentationEvents().empty();
        const bool restored = rejected && client.apply({1, retained}) == R::ApplyResult::Applied;
        const auto reset = validUpdate(retained, fresh);
        const bool resetApplied = restored && client.apply(reset) == R::ApplyResult::Applied
                && client.version() == 2 && client.current() && client.state()
                && client.state()->matchId == 31 && client.state()->round->roundId == 42
                && client.state()->currentRoundNumber == 1 && client.state()->completedRounds == 0
                && client.state()->score.players[0].cumulativePoints == 0
                && client.state()->score.ranking == std::vector<R::Identity>({101, 102})
                && client.state()->score.teamTotals == std::vector<std::int64_t>({0, 0});
        if (!(rejected && restored && resetApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-005 REP-042 REP-066 NET-AC-018 full snapshot rejects stale retained values in a fresh first round atomically") {
    const auto retained = retainedTeamCompletedLobby();
    const auto fresh = resetTeamFirstRound();
    std::string failures;
    for (const auto &scenario: staleNewMatchMutations()) {
        auto stale = fresh;
        scenario.alter(stale);
        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, retained}) == R::ApplyResult::Applied);
        const auto before = R::serializeReplicationSnapshot({1, *client.state()});

        const bool rejected = client.apply({2, stale}) == R::ApplyResult::Invalid;
        const bool atomic = rejected && client.version() == 1 && client.current() && client.state()
                && before == R::serializeReplicationSnapshot({client.version(), *client.state()});
        const bool resetApplied = atomic && client.apply({2, fresh}) == R::ApplyResult::Applied
                && client.version() == 2 && client.current() && client.state()
                && client.state()->matchId == 31 && client.state()->round->roundId == 42
                && client.state()->currentRoundNumber == 1 && client.state()->completedRounds == 0
                && client.state()->score.players[0].cumulativePoints == 0
                && client.state()->score.ranking == std::vector<R::Identity>({101, 102})
                && client.state()->score.teamTotals == std::vector<std::int64_t>({0, 0});
        if (!(atomic && resetApplied)) {
            if (!failures.empty()) failures += ';';
            failures += scenario.name;
        }
    }
    D6R_REQUIRE_EQ(std::string(), failures);
}

D6R_TEST_CASE("REP-005 REP-042 REP-066 NET-AC-018 publisher rejects retained Lobby match replacement transactionally") {
    const auto retained = retainedTeamCompletedLobby();
    auto replacement = retained;
    replacement.matchId = 31;
    const auto fresh = resetTeamFirstRound();
    D6R_REQUIRE(R::validateCanonicalState(replacement));
    D6R_REQUIRE_EQ(retained.result.serialized, replacement.result.serialized);

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(retained));
    const auto before = publisher.fullSnapshot();
    D6R_REQUIRE(!publisher.publish(replacement).has_value());
    D6R_REQUIRE_EQ(1u, publisher.version());
    D6R_REQUIRE(before.has_value());
    D6R_REQUIRE(publisher.fullSnapshot().has_value());
    D6R_REQUIRE_EQ(R::serializeReplicationSnapshot(*before),
                   R::serializeReplicationSnapshot(*publisher.fullSnapshot()));

    const auto accepted = publisher.publish(fresh);
    D6R_REQUIRE(accepted.has_value());
    D6R_REQUIRE_EQ(1u, accepted->baseline);
    D6R_REQUIRE_EQ(2u, accepted->version);
    D6R_REQUIRE_EQ(31u, publisher.fullSnapshot()->state.matchId);
    D6R_REQUIRE_EQ(42u, publisher.fullSnapshot()->state.round->roundId);
    D6R_REQUIRE(!publisher.fullSnapshot()->state.result.available);
}

D6R_TEST_CASE("REP-041 REP-044 REP-054 REP-066 REP-AC-008 resync accepts only identical same-version snapshot then recovers at higher version") {
    const auto accepted = retainedCompletedLobby();
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({7, accepted}) == R::ApplyResult::Applied);

    client.requireResynchronization();
    D6R_REQUIRE(client.apply({7, accepted}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.current());
    D6R_REQUIRE_EQ(7u, client.version());
    D6R_REQUIRE_EQ(R::serializeReplicationSnapshot({7, accepted}),
                   R::serializeReplicationSnapshot({client.version(), *client.state()}));

    R::ParticipantState attemptedParticipant{77, false, R::ConnectionState::Connected, false, {777}};
    R::PlayerState attemptedPlayer;
    attemptedPlayer.playerId = 777;
    attemptedPlayer.ownerParticipantId = attemptedParticipant.participantId;
    attemptedPlayer.rosterPosition = 2;
    attemptedPlayer.displayName = "Same-version guest";
    attemptedPlayer.life = 100;
    auto differentSameVersion = accepted;
    differentSameVersion.phaseTime++;
    differentSameVersion.settings.assistance = !differentSameVersion.settings.assistance;
    differentSameVersion.participants.push_back(attemptedParticipant);
    differentSameVersion.players.push_back(attemptedPlayer);
    D6R_REQUIRE(R::validateCanonicalState(differentSameVersion));
    D6R_REQUIRE(R::serializeReplicationSnapshot({7, accepted})
                != R::serializeReplicationSnapshot({7, differentSameVersion}));

    client.requireResynchronization();
    const auto sameVersionResult = client.apply({7, differentSameVersion});
    const bool rejectedWithoutMutation = sameVersionResult == R::ApplyResult::Invalid
            && client.version() == 7 && !client.current() && client.state() == nullptr
            && client.takePresentationEvents().empty();
    if (!client.resynchronizationRequired()) client.requireResynchronization();

    auto current = accepted;
    current.phaseTime += 2;
    const bool recoveredAtHigherVersion = client.apply({8, current}) == R::ApplyResult::Applied
            && client.version() == 8 && client.current() && client.state()
            && R::serializeReplicationSnapshot({8, current})
               == R::serializeReplicationSnapshot({client.version(), *client.state()});

    auto admitted = current;
    admitted.participants.push_back(attemptedParticipant);
    admitted.players.push_back(attemptedPlayer);
    D6R_REQUIRE(R::validateCanonicalState(admitted));
    const auto admission = lobbyCreationUpdate(8, 9, admitted, attemptedParticipant, attemptedPlayer);
    const bool rejectedSnapshotDidNotConsumeIdentity = recoveredAtHigherVersion
            && client.apply(admission) == R::ApplyResult::Applied
            && player(*client.state(), attemptedPlayer.playerId) != nullptr;

    const std::string evidence = "same-version-rejected="
            + std::string(rejectedWithoutMutation ? "true" : "false")
            + ";higher-version-recovered=" + (recoveredAtHigherVersion ? "true" : "false")
            + ";history-unchanged=" + (rejectedSnapshotDidNotConsumeIdentity ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("same-version-rejected=true;higher-version-recovered=true;history-unchanged=true"),
                   evidence);
}

D6R_TEST_CASE("REP-013 REP-014 REP-017 legitimate following Lobby mutations preserve retained results") {
    for (const auto &initial: {retainedCompletedLobby(), retainedInterruptedLobby()}) {
        const auto legitimate = legitimateFollowingLobbyChange(initial);
        D6R_REQUIRE(R::validateCanonicalState(legitimate));

        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(initial));
        const auto update = publisher.publish(legitimate);
        D6R_REQUIRE(update.has_value());
        D6R_REQUIRE_EQ(2u, publisher.version());
        requireRetainedResultEqual(initial, publisher.fullSnapshot()->state);

        R::ReplicatedState client;
        D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
        D6R_REQUIRE(client.apply(*update) == R::ApplyResult::Applied);
        D6R_REQUIRE(client.state() != nullptr);
        D6R_REQUIRE_EQ(legitimate.settings.assistance, client.state()->settings.assistance);
        D6R_REQUIRE_EQ(legitimate.settings.quickLiquid, client.state()->settings.quickLiquid);
        D6R_REQUIRE(client.state()->participants[0].ready);
        D6R_REQUIRE(client.state()->participants[1].connection == R::ConnectionState::Reconnecting);
        D6R_REQUIRE_EQ(1u, client.state()->players[0].rosterPosition);
        D6R_REQUIRE_EQ(0u, client.state()->players[1].rosterPosition);
        D6R_REQUIRE_EQ(std::string("Guest (Edited)"), client.state()->players[1].displayName);
        D6R_REQUIRE(client.state()->players[1].lifeState == R::LifeState::Alive);
        requireRetainedResultEqual(initial, *client.state());

        auto rosterReduced = legitimate;
        rosterReduced.participants.pop_back();
        rosterReduced.players.pop_back();
        const auto removal = publisher.publish(rosterReduced);
        D6R_REQUIRE(removal.has_value());
        D6R_REQUIRE(client.apply(*removal) == R::ApplyResult::Applied);
        D6R_REQUIRE_EQ(1u, client.state()->participants.size());
        D6R_REQUIRE_EQ(1u, client.state()->players.size());
        D6R_REQUIRE_EQ(2u, client.state()->score.players.size());
        requireRetainedResultEqual(initial, *client.state());
    }
}

D6R_TEST_CASE("REP-005 REP-017 reconnect snapshot reserves player identities retained only in prior result rows") {
    auto followingLobby = distinctFinalSummary();
    followingLobby.phase = R::Phase::Lobby;
    followingLobby.participants.erase(followingLobby.participants.begin() + 1);
    followingLobby.participants.front().ownedPlayerIds = {101};
    followingLobby.players.erase(followingLobby.players.begin() + 1);
    followingLobby.messages.status = "Lobby";
    followingLobby.messages.scoreSummaryVisible = false;
    D6R_REQUIRE(R::validateCanonicalState(followingLobby));
    D6R_REQUIRE(std::none_of(followingLobby.players.begin(), followingLobby.players.end(),
            [](const auto &value) { return value.playerId == 102; }));
    D6R_REQUIRE(scoreRow(followingLobby, 102) != nullptr);
    D6R_REQUIRE(std::find(followingLobby.round->rosterOrder.begin(), followingLobby.round->rosterOrder.end(), 102)
                != followingLobby.round->rosterOrder.end());

    R::ReplicatedState reconnect;
    D6R_REQUIRE(reconnect.apply({9, followingLobby}) == R::ApplyResult::Applied);

    R::ParticipantState newcomer{22, false, R::ConnectionState::Connected, false, {102}};
    R::PlayerState reusedPlayer;
    reusedPlayer.playerId = 102;
    reusedPlayer.ownerParticipantId = 22;
    reusedPlayer.rosterPosition = 1;
    reusedPlayer.displayName = "Reused identity";
    reusedPlayer.life = 100;
    auto invalidCandidate = followingLobby;
    invalidCandidate.participants.push_back(newcomer);
    invalidCandidate.players.push_back(reusedPlayer);
    D6R_REQUIRE(R::validateCanonicalState(invalidCandidate));
    const auto reuse = lobbyCreationUpdate(9, 10, invalidCandidate, newcomer, reusedPlayer);
    const auto applied = reconnect.apply(reuse);
    const bool rejectedWithoutMutation = applied == R::ApplyResult::ResynchronizationRequired
            && reconnect.version() == 9 && !reconnect.current() && reconnect.state() == nullptr
            && reconnect.takePresentationEvents().empty();
    bool originalSnapshotRestored = false;
    if (applied == R::ApplyResult::ResynchronizationRequired) {
        originalSnapshotRestored = reconnect.apply({9, followingLobby}) == R::ApplyResult::Applied
                && reconnect.state() && reconnect.state()->players.size() == 1
                && reconnect.state()->score.ranking == std::vector<R::Identity>({101, 102})
                && scoreRow(*reconnect.state(), 102) != nullptr;
    }
    const std::string evidence = "rejected=" + std::string(rejectedWithoutMutation ? "true" : "false")
            + ";restored=" + (originalSnapshotRestored ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("rejected=true;restored=true"), evidence);
}

D6R_TEST_CASE("REP-006 REP-007 REP-042 authoritative retained result rejects a new result-only identity transactionally") {
    auto initial = distinctFinalSummary();
    initial.phase = R::Phase::Lobby;
    initial.participants[0].ready = false;
    initial.participants[1].ready = false;
    initial.messages.status = "Lobby";

    constexpr R::Identity resultOnlyIdentity = 777;
    auto resultOnly = initial;
    R::ScoreRowState retainedRow;
    retainedRow.playerId = resultOnlyIdentity;
    retainedRow.cumulativePoints = 4;
    resultOnly.score.players.insert(resultOnly.score.players.begin(), retainedRow);
    resultOnly.score.ranking.insert(resultOnly.score.ranking.begin(), resultOnlyIdentity);
    resultOnly.score.winner.winnerPlayerIds = {resultOnlyIdentity};
    resultOnly.round->rosterOrder.push_back(resultOnlyIdentity);
    resultOnly.round->outcome.winnerPlayerIds = {resultOnlyIdentity};
    resultOnly.result.serialized = "completed;match-outcome=777;cumulative-ranking=777,101,102";
    D6R_REQUIRE(R::validateCanonicalState(initial));
    D6R_REQUIRE(R::validateCanonicalState(resultOnly));
    D6R_REQUIRE(player(resultOnly, resultOnlyIdentity) == nullptr);

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    const auto before = publisher.fullSnapshot();
    D6R_REQUIRE(before.has_value());
    const R::PresentationEvent attemptedEvent{970, "result-transition", 0, 0, 0, 0};
    D6R_REQUIRE(!publisher.publish(resultOnly, {attemptedEvent}).has_value());
    const auto afterRejection = publisher.fullSnapshot();
    D6R_REQUIRE(afterRejection.has_value());
    D6R_REQUIRE_EQ(1u, publisher.version());
    D6R_REQUIRE_EQ(R::serializeReplicationSnapshot(*before),
                   R::serializeReplicationSnapshot(*afterRejection));

    R::ParticipantState newcomer{22, false, R::ConnectionState::Connected, false, {resultOnlyIdentity}};
    R::PlayerState freshPlayer;
    freshPlayer.playerId = resultOnlyIdentity;
    freshPlayer.ownerParticipantId = newcomer.participantId;
    freshPlayer.rosterPosition = 2;
    freshPlayer.displayName = "Fresh after rejected result";
    freshPlayer.life = 100;
    auto freshLive = initial;
    freshLive.participants.push_back(newcomer);
    freshLive.players.push_back(freshPlayer);
    D6R_REQUIRE(R::validateCanonicalState(freshLive));
    const auto creation = publisher.publish(freshLive, {attemptedEvent});
    D6R_REQUIRE(creation.has_value());
    D6R_REQUIRE_EQ(1u, creation->baseline);
    D6R_REQUIRE_EQ(2u, creation->version);
    D6R_REQUIRE_EQ(1u, creation->events.size());
    D6R_REQUIRE_EQ(attemptedEvent.eventId, creation->events.front().eventId);
    D6R_REQUIRE_EQ(resultOnlyIdentity, creation->players.front().identity);
    D6R_REQUIRE_EQ(2u, publisher.version());
    D6R_REQUIRE(player(publisher.fullSnapshot()->state, resultOnlyIdentity) != nullptr);
}

D6R_TEST_CASE("REP-006 REP-007 REP-048 REP-AC-001/006/007 client rejects a new retained result-only identity transactionally") {
    auto initial = distinctFinalSummary();
    initial.phase = R::Phase::Lobby;
    initial.participants[0].ready = false;
    initial.participants[1].ready = false;
    initial.messages.status = "Lobby";

    constexpr R::Identity resultOnlyIdentity = 778;
    auto resultOnly = initial;
    R::ScoreRowState retainedRow;
    retainedRow.playerId = resultOnlyIdentity;
    retainedRow.cumulativePoints = 4;
    resultOnly.score.players.insert(resultOnly.score.players.begin(), retainedRow);
    resultOnly.score.ranking.insert(resultOnly.score.ranking.begin(), resultOnlyIdentity);
    resultOnly.score.winner.winnerPlayerIds = {resultOnlyIdentity};
    resultOnly.round->rosterOrder.push_back(resultOnlyIdentity);
    resultOnly.round->outcome.winnerPlayerIds = {resultOnlyIdentity};
    resultOnly.result.serialized = "completed;match-outcome=778;cumulative-ranking=778,101,102";
    D6R_REQUIRE(R::validateCanonicalState(initial));
    D6R_REQUIRE(R::validateCanonicalState(resultOnly));
    D6R_REQUIRE(player(resultOnly, resultOnlyIdentity) == nullptr);

    auto introduction = validUpdate(initial, initial);
    introduction.round = resultOnly.round;
    introduction.score = resultOnly.score;
    introduction.result = resultOnly.result;
    const R::PresentationEvent attemptedEvent{971, "result-transition", 0, 0, 0, 0};
    introduction.events = {attemptedEvent};
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(introduction) == R::ApplyResult::ResynchronizationRequired);
    D6R_REQUIRE_EQ(1u, client.version());
    D6R_REQUIRE(!client.current());
    D6R_REQUIRE(client.state() == nullptr);
    D6R_REQUIRE(client.takePresentationEvents().empty());

    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.state() != nullptr);
    D6R_REQUIRE_EQ(R::serializeReplicationSnapshot({1, initial}),
                   R::serializeReplicationSnapshot({client.version(), *client.state()}));

    R::ParticipantState newcomer{22, false, R::ConnectionState::Connected, false, {resultOnlyIdentity}};
    R::PlayerState freshPlayer;
    freshPlayer.playerId = resultOnlyIdentity;
    freshPlayer.ownerParticipantId = newcomer.participantId;
    freshPlayer.rosterPosition = 2;
    freshPlayer.displayName = "Fresh after rejected result";
    freshPlayer.life = 100;
    auto freshLive = initial;
    freshLive.participants.push_back(newcomer);
    freshLive.players.push_back(freshPlayer);
    D6R_REQUIRE(R::validateCanonicalState(freshLive));
    auto creation = lobbyCreationUpdate(1, 2, freshLive, newcomer, freshPlayer);
    creation.events = {attemptedEvent};
    D6R_REQUIRE(client.apply(creation) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(2u, client.version());
    D6R_REQUIRE(client.state() != nullptr);
    D6R_REQUIRE(player(*client.state(), resultOnlyIdentity) != nullptr);
    const auto deliveredEvents = client.takePresentationEvents();
    D6R_REQUIRE_EQ(1u, deliveredEvents.size());
    D6R_REQUIRE_EQ(attemptedEvent.eventId, deliveredEvents.front().eventId);
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 production interruption after a completed round survives update and reconnect") {
    A::MatchConfig requested = matchConfig();
    requested.roundLimit = 3;
    A::AuthoritativeMatch match;
    D6R_REQUIRE(match.start(requested, roster(), manifest()).code == A::OutcomeCode::None);
    A::AuthoritativeReplication replication(900);
    const std::vector<R::ParticipantState> participants = {
            {20, true, R::ConnectionState::Connected, true, {101}},
            {21, false, R::ConnectionState::Connected, true, {102}}};
    D6R_REQUIRE(replication.setLobby(20, participants, roster(), requested));
    R::ReplicatedState incremental;
    D6R_REQUIRE(incremental.apply(*replication.fullSnapshot()) == R::ApplyResult::Applied);
    const auto begin = replication.beginMatch(match);
    D6R_REQUIRE(begin.has_value());
    D6R_REQUIRE(incremental.apply(*begin) == R::ApplyResult::Applied);

    std::uint64_t sequence = 1;
    D6R_REQUIRE(match.submit({match.currentTick(), sequence++, 20, 101, A::ActionKind::ShotDamage,
                              102, 0, A::MaximumLife}) == A::ActionResult::Accepted);
    const auto firstSummary = replication.capture(match);
    D6R_REQUIRE(firstSummary.has_value());
    D6R_REQUIRE(incremental.apply(*firstSummary) == R::ApplyResult::Applied);
    for (std::uint32_t tick = 0; tick < A::RoundEndTotalTicks; ++tick) D6R_REQUIRE(match.advanceOneTick());
    const auto secondRound = replication.capture(match);
    D6R_REQUIRE(secondRound.has_value());
    D6R_REQUIRE(incremental.apply(*secondRound) == R::ApplyResult::Applied);

    D6R_REQUIRE(match.submit({match.currentTick(), sequence++, 20, 0, A::ActionKind::RemovePlayer,
                              102, 0, 0}) == A::ActionResult::Accepted);
    D6R_REQUIRE(match.outcome().code == A::OutcomeCode::InterruptedNoWinner);
    const auto interruption = replication.capture(match);
    D6R_REQUIRE(interruption.has_value());
    D6R_REQUIRE(incremental.apply(*interruption) == R::ApplyResult::Applied);
    R::ReplicatedState reconnect;
    D6R_REQUIRE(reconnect.apply(*replication.fullSnapshot()) == R::ApplyResult::Applied);

    for (const R::CanonicalState *state: {incremental.state(), reconnect.state()}) {
        D6R_REQUIRE(state != nullptr);
        D6R_REQUIRE(state->phase == R::Phase::Lobby);
        D6R_REQUIRE_EQ(1u, state->completedRounds);
        D6R_REQUIRE_EQ(std::string("Interrupted"), state->result.state);
        D6R_REQUIRE(state->score.winner.noWinner);
        D6R_REQUIRE(state->round.has_value());
        D6R_REQUIRE_EQ(std::vector<R::Identity>({101}), state->round->outcome.winnerPlayerIds);
        D6R_REQUIRE_EQ(101u, state->score.ranking.front());
        D6R_REQUIRE(scoreRow(*state, 101)->cumulativePoints > scoreRow(*state, 102)->cumulativePoints);
        D6R_REQUIRE(state->result.serialized.find("\"completedRounds\":1") != std::string::npos);
        D6R_REQUIRE(state->result.serialized.find("\"winnerPlayerIds\":[101]") != std::string::npos);
    }
}

D6R_TEST_CASE("REP-017 REP-018 REP-025 production multi-round capture keeps cumulative leader out of match outcome") {
    A::MatchConfig requested = matchConfig();
    requested.roundLimit = 2;
    A::AuthoritativeMatch match;
    D6R_REQUIRE(match.start(requested, roster(), manifest()).code == A::OutcomeCode::None);
    A::AuthoritativeReplication replication(900);
    const std::vector<R::ParticipantState> participants = {
            {20, true, R::ConnectionState::Connected, true, {101}},
            {21, false, R::ConnectionState::Connected, true, {102}}};
    D6R_REQUIRE(replication.setLobby(20, participants, roster(), requested));
    D6R_REQUIRE(replication.beginMatch(match).has_value());

    std::uint64_t sequence = 1;
    D6R_REQUIRE(match.submit({match.currentTick(), sequence++, 20, 101, A::ActionKind::ShotDamage,
                              102, 0, A::MaximumLife}) == A::ActionResult::Accepted);
    D6R_REQUIRE(replication.capture(match).has_value());
    for (std::uint32_t tick = 0; tick < A::RoundEndTotalTicks; ++tick) D6R_REQUIRE(match.advanceOneTick());
    D6R_REQUIRE(replication.capture(match).has_value());
    D6R_REQUIRE(match.submit({match.currentTick(), sequence++, 21, 102, A::ActionKind::ShotDamage,
                              101, 0, A::MaximumLife}) == A::ActionResult::Accepted);
    D6R_REQUIRE(replication.capture(match).has_value());
    for (std::uint32_t tick = 0; tick < A::RoundEndTotalTicks; ++tick) D6R_REQUIRE(match.advanceOneTick());
    const auto finalUpdate = replication.capture(match);
    D6R_REQUIRE(finalUpdate.has_value());
    const auto final = replication.fullSnapshot();
    D6R_REQUIRE(final.has_value());
    D6R_REQUIRE(final->state.phase == R::Phase::FinalSummary);
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), final->state.round->outcome.winnerPlayerIds);
    D6R_REQUIRE_EQ(101u, final->state.score.ranking.front());
    D6R_REQUIRE_EQ(std::vector<R::Identity>({102}), final->state.score.winner.winnerPlayerIds);
    D6R_REQUIRE(scoreRow(final->state, 101)->cumulativePoints >=
                scoreRow(final->state, 102)->cumulativePoints);
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

D6R_TEST_CASE("REP binary codec round trips complete snapshot and delta schemas deterministically") {
    auto state = activeState();
    state.settings.fixedLevel = "levels/a.json";
    state.settings.assistance = true;
    state.settings.quickLiquid = true;
    state.settings.burnableTrees = false;
    state.roundEndCountdown = 300;
    state.round->mirrored = true;
    state.players[0].velocityX = -12;
    state.players[0].velocityY = 34;
    state.players[0].facingLeft = true;
    state.players[0].crouching = true;
    state.players[0].air = 77;
    state.players[0].actionMask = 5;
    state.players[0].activeBonus = "shield";
    state.players[0].bonusRemaining = 44;
    state.players[0].invulnerable = true;
    state.players[0].visible = false;
    state.players[0].reloadRemaining = 3;
    state.players[0].charge = 2;
    state.players[0].temporaryMovementRemaining = 1;
    state.entities[0].positionX = -50;
    state.entities[0].positionY = 60;
    state.entities[0].velocityX = 7;
    state.entities[0].velocityY = -8;
    state.entities[0].primaryValue = 9;
    state.entities[0].secondaryValue = 10;
    state.score.players[0].roundPoints = 11;
    state.score.players[0].cumulativePoints = 12;
    state.score.players[0].shots = 13;
    state.score.players[0].hits = 14;
    state.score.players[0].kills = 1;
    state.score.players[0].assists = 2;
    state.score.players[0].damage = 99;
    state.messages.events = {"hit", "pickup"};
    state.messages.scoreSummaryVisible = false;
    state.effects = {{70, "shield", 101, 0, 44}};

    const R::FullSnapshot snapshot{17, state};
    const auto payload = R::serializeReplicationSnapshot(snapshot);
    D6R_REQUIRE(payload.size() <= Duel6::Network::MaxPayloadBytes);
    const auto decoded = R::deserializeReplicationFrame(payload);
    D6R_REQUIRE(decoded.has_value());
    D6R_REQUIRE(decoded->kind == R::ReplicationFrameKind::FullSnapshot);
    D6R_REQUIRE(decoded->snapshot.has_value());
    D6R_REQUIRE_EQ(payload, R::serializeReplicationSnapshot(*decoded->snapshot));
    D6R_REQUIRE_EQ(state.settings.fixedLevel, decoded->snapshot->state.settings.fixedLevel);
    D6R_REQUIRE_EQ(state.roundEndCountdown, decoded->snapshot->state.roundEndCountdown);
    D6R_REQUIRE_EQ(state.players[0].temporaryMovementRemaining,
                   decoded->snapshot->state.players[0].temporaryMovementRemaining);
    D6R_REQUIRE_EQ(state.score.players[0].damage, decoded->snapshot->state.score.players[0].damage);
    D6R_REQUIRE_EQ(state.effects[0].remaining, decoded->snapshot->state.effects[0].remaining);

    auto next = state;
    next.phaseTime++;
    next.players[0].positionX++;
    const auto update = validUpdate(state, next, {{80, "hit", 101, 102, 50, 3}});
    const auto updatePayload = R::serializeReplicationUpdate(update);
    const auto decodedUpdate = R::deserializeReplicationFrame(updatePayload);
    D6R_REQUIRE(decodedUpdate && decodedUpdate->update);
    D6R_REQUIRE_EQ(updatePayload, R::serializeReplicationUpdate(*decodedUpdate->update));
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply(snapshot) == R::ApplyResult::Applied);
    auto versionAdjusted = *decodedUpdate->update;
    versionAdjusted.baseline = 17;
    versionAdjusted.version = 18;
    D6R_REQUIRE(client.apply(versionAdjusted) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(next.players[0].positionX, client.state()->players[0].positionX);
}

D6R_TEST_CASE("REP codec rejects truncated trailing oversized and invalid schema payloads") {
    auto payload = R::serializeReplicationSnapshot({1, activeState()});
    payload.pop_back();
    D6R_REQUIRE(!R::deserializeReplicationFrame(payload));
    payload = R::serializeReplicationSnapshot({1, activeState()});
    payload.push_back(0);
    D6R_REQUIRE(!R::deserializeReplicationFrame(payload));
    D6R_REQUIRE(!R::deserializeReplicationFrame({}));
    D6R_REQUIRE(!R::deserializeReplicationFrame(
            std::vector<std::uint8_t>(Duel6::Network::MaxPayloadBytes + 1, 0)));

    auto invalidBoolean = R::serializeReplicationSnapshot({1, activeState()});
    // Header (8), version (8), session/match/host (24), phase/round counts (3),
    // phase time/countdown (16), participant count (4), participant id (8): host Boolean.
    invalidBoolean[71] = 2;
    D6R_REQUIRE(!R::deserializeReplicationFrame(invalidBoolean));
}

D6R_TEST_CASE("REP production connection broadcast isolates failures and mutation policy isolates offender") {
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(activeState()));
    R::AuthoritativeReplicationConnections connections(publisher, [] { return 1000; });
    std::vector<std::vector<std::uint8_t>> firstPayloads;
    std::vector<std::vector<std::uint8_t>> secondPayloads;
    int firstClosed = 0;
    int secondClosed = 0;
    D6R_REQUIRE(connections.restore(20, [&](auto payload) {
        firstPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, [&] { ++firstClosed; }));
    D6R_REQUIRE(connections.restore(21, [&](auto payload) {
        secondPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, [&] { ++secondClosed; }));
    D6R_REQUIRE_EQ(2u, connections.size());
    D6R_REQUIRE(firstPayloads.front() == secondPayloads.front());

    auto next = activeState();
    next.phaseTime++;
    const auto update = publisher.publish(next);
    D6R_REQUIRE(update.has_value());
    D6R_REQUIRE(connections.restore(20, [&](auto payload) {
        firstPayloads.push_back(std::move(payload));
        return Duel6::Network::SendResult::NotConnected;
    }, [&] { ++firstClosed; }) == false);
    // A failed replacement must leave the previously established connection usable.
    D6R_REQUIRE(connections.broadcast(*update));
    D6R_REQUIRE_EQ(0, firstClosed);
    D6R_REQUIRE_EQ(0, secondClosed);

    auto mutation = R::serializeResynchronizationRequest();
    mutation[6] = static_cast<std::uint8_t>(R::ReplicationFrameKind::CanonicalStateMutation);
    D6R_REQUIRE(connections.receive(20, mutation) == R::HostReplicationResult::SessionPolicyViolation);
    D6R_REQUIRE_EQ(1, firstClosed);
    D6R_REQUIRE_EQ(1u, connections.size());
    D6R_REQUIRE(connections.receive(21, R::serializeResynchronizationRequest())
                == R::HostReplicationResult::Accepted);
    D6R_REQUIRE_EQ(0, secondClosed);
}

D6R_TEST_CASE("REP client requests one resynchronization and restores current snapshot") {
    std::vector<std::vector<std::uint8_t>> requests;
    R::ClientReplicationConnection client([&](auto payload) {
        requests.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    });
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({1, activeState()}))
                == R::ClientReplicationResult::Applied);
    auto next = activeState();
    next.phaseTime++;
    auto invalid = validUpdate(activeState(), next);
    invalid.baseline = 99;
    invalid.version = 100;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(invalid))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(1u, requests.size());
    const auto request = R::deserializeReplicationFrame(requests.front());
    D6R_REQUIRE(request && request->kind == R::ReplicationFrameKind::ResynchronizationRequest);
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(invalid))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(1u, requests.size());
    next.phaseTime = 200;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({2, next}))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE_EQ(200u, client.replicatedState().state()->phaseTime);
}

D6R_TEST_CASE("REP failed authoritative lobby mutation is transactional and retryable") {
    A::AuthoritativeReplication replication(900);
    const std::vector<R::ParticipantState> participants = {
            {20, true, R::ConnectionState::Connected, true, {101}},
            {21, false, R::ConnectionState::Connected, false, {102}}};
    D6R_REQUIRE(replication.setLobby(20, participants, roster(), matchConfig()));
    auto invalidRoster = roster();
    invalidRoster[1].displayName.assign(65, 'x');
    D6R_REQUIRE(!replication.updateLobby(participants, invalidRoster, matchConfig()));
    D6R_REQUIRE_EQ(1u, replication.replicator().version());
    auto retryRoster = roster();
    retryRoster[1].displayName = "Retried Guest";
    const auto retry = replication.updateLobby(participants, retryRoster, matchConfig());
    D6R_REQUIRE(retry.has_value());
    D6R_REQUIRE_EQ(1u, retry->baseline);
    D6R_REQUIRE_EQ(2u, retry->version);
    D6R_REQUIRE_EQ("Retried Guest", replication.fullSnapshot()->state.players[1].displayName);
}

D6R_TEST_CASE("REP monotonic tombstone and event watermarks remain bounded over long sessions") {
    auto state = activeState();
    state.entities[0].entityId = 1000;
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, state}) == R::ApplyResult::Applied);
    for (R::Identity sequence = 1; sequence <= 5000; ++sequence) {
        auto next = state;
        next.phaseTime = state.phaseTime + 1;
        next.entities[0].entityId = 1000 + sequence;
        const auto update = publisher.publish(next, {{10000 + sequence, "shot", 101, 0,
                                                       1000 + sequence, 1}});
        D6R_REQUIRE(update.has_value());
        D6R_REQUIRE(client.apply(*update) == R::ApplyResult::Applied);
        D6R_REQUIRE_EQ(1u, client.takePresentationEvents().size());
        state = std::move(next);
    }
    D6R_REQUIRE_EQ(5001u, publisher.version());
    D6R_REQUIRE_EQ(5001u, client.version());
    D6R_REQUIRE_EQ(6000u, client.state()->entities.front().entityId);
}

D6R_TEST_CASE("REP bounded tombstones preserve older live identities when a newer entity is removed") {
    auto initial = activeState();
    initial.entities[0].entityId = 10;
    auto newer = initial.entities[0];
    newer.entityId = 100;
    initial.entities.push_back(newer);
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(initial));
    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);

    auto removedNewer = initial;
    removedNewer.entities.pop_back();
    const auto removal = publisher.publish(removedNewer);
    D6R_REQUIRE(removal.has_value());
    D6R_REQUIRE(client.apply(*removal) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(10u, client.state()->entities.front().entityId);

    auto later = removedNewer;
    later.phaseTime++;
    const auto next = publisher.publish(later);
    D6R_REQUIRE(next.has_value());
    D6R_REQUIRE(client.apply(*next) == R::ApplyResult::Applied);
    client.requireResynchronization();
    D6R_REQUIRE(client.apply(*publisher.fullSnapshot()) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(10u, client.state()->entities.front().entityId);
}

D6R_TEST_CASE("REP recovery snapshot retains a live identity older than an unrelated tombstone") {
    auto initial = activeState();
    initial.entities[0].entityId = 10;
    auto newer = initial.entities[0];
    newer.entityId = 100;
    initial.entities.push_back(newer);
    auto removedNewer = initial;
    removedNewer.entities.pop_back();
    const auto removal = validUpdate(initial, removedNewer);

    R::ReplicatedState client;
    D6R_REQUIRE(client.apply({1, initial}) == R::ApplyResult::Applied);
    D6R_REQUIRE(client.apply(removal) == R::ApplyResult::Applied);
    client.requireResynchronization();
    D6R_REQUIRE(client.apply({3, removedNewer}) == R::ApplyResult::Applied);
    D6R_REQUIRE_EQ(10u, client.state()->entities.front().entityId);
}

D6R_TEST_CASE("REP failed resynchronization enters reconnecting without changing accepted version") {
    R::ClientReplicationConnection client([](auto) { return Duel6::Network::SendResult::NotConnected; });
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({1, activeState()}))
                == R::ClientReplicationResult::Applied);
    auto next = activeState();
    next.phaseTime++;
    auto invalid = validUpdate(activeState(), next);
    invalid.baseline = 99;
    invalid.version = 100;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(invalid))
                == R::ClientReplicationResult::SendFailed);
    D6R_REQUIRE_EQ(1u, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({2, next}))
                == R::ClientReplicationResult::Reconnecting);
}

D6R_TEST_CASE("NRP production protocol stamps snapshots updates and probe responses on the host clock") {
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(activeState()));
    std::uint64_t hostTime = 1000;
    R::AuthoritativeReplicationConnections connections(publisher, [&] { return hostTime++; });
    std::vector<std::vector<std::uint8_t>> sent;
    D6R_REQUIRE(connections.restore(21, [&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }));
    auto frame = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(frame && frame->snapshot);
    D6R_REQUIRE_EQ(std::uint64_t{1000}, frame->snapshot->authoritativeProducedAt);

    auto next = activeState();
    next.phaseTime++;
    const auto update = publisher.publish(next);
    D6R_REQUIRE(update.has_value());
    D6R_REQUIRE(connections.broadcast(*update));
    frame = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(frame && frame->update);
    D6R_REQUIRE_EQ(std::uint64_t{1001}, frame->update->authoritativeProducedAt);

    D6R_REQUIRE(connections.receive(21, R::serializeQualityProbe(7))
                == R::HostReplicationResult::Accepted);
    frame = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(frame && frame->kind == R::ReplicationFrameKind::QualityResponse);
    D6R_REQUIRE_EQ(std::uint64_t{7}, *frame->qualitySequence);
    D6R_REQUIRE_EQ(std::uint64_t{1002}, *frame->authoritativeResponseAt);
}

D6R_TEST_CASE("NRP production client uses synchronized host production age and requires three supported seconds") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = 700;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(30ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(!client.presentationState(at(959ms)).degraded);
    for (R::StateVersion version = 2; version <= 19; ++version) {
        state.phaseTime++;
        auto update = publisher.publish(state);
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = 700 + (version - 1) * 50;
        const auto acceptedAt = 30ms + std::chrono::milliseconds((version - 1) * 50);
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(acceptedAt))
                    == R::ClientReplicationResult::Applied);
    }
    D6R_REQUIRE(client.presentationState(at(960ms)).degraded);
    for (R::StateVersion version = 20; version <= 21; ++version) {
        state.phaseTime++;
        auto update = publisher.publish(state);
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = 700 + (version - 1) * 50;
        const auto acceptedAt = 30ms + std::chrono::milliseconds((version - 1) * 50);
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(acceptedAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE(client.presentationState(at(acceptedAt)).degraded);
    }

    for (R::StateVersion version = 22; version <= 81; ++version) {
        state.phaseTime++;
        auto update = publisher.publish(state);
        D6R_REQUIRE(update.has_value());
        const auto acceptedAt = 1040ms + std::chrono::milliseconds((version - 22) * 50);
        update->authoritativeProducedAt = 990 + static_cast<std::uint64_t>(acceptedAt.count());
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(acceptedAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE(client.presentationState(at(acceptedAt)).degraded);
    }
    state.phaseTime++;
    auto recoveredUpdate = publisher.publish(state);
    D6R_REQUIRE(recoveredUpdate.has_value());
    recoveredUpdate->authoritativeProducedAt = 5030;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*recoveredUpdate), at(4040ms))
                == R::ClientReplicationResult::Applied);
    const auto recovered = client.presentationState(at(4040ms));
    D6R_REQUIRE(!recovered.degraded);
    D6R_REQUIRE(recovered.canonicalStateCurrent);
}

D6R_TEST_CASE("NRP successful probes do not reprocess canonical versions or reset authoritative state age") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));

    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = 990;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(5ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);

    state.phaseTime++;
    const R::PresentationEvent outcome{701, "shot", 101, 102, 50, 1};
    auto update = publisher.publish(state, {outcome});
    D6R_REQUIRE(update.has_value());
    update->authoritativeProducedAt = 1290;
    D6R_REQUIRE(client.sampleNetwork(at(250ms)));
    probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1250), at(270ms))
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(300ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(std::size_t{1}, client.takePresentationEvents().size());
    const auto acceptedState = R::serializeReplicationSnapshot(
            {client.replicatedState().version(), *client.replicatedState().state()});

    for (const auto sampleAt: {500ms, 750ms, 1000ms, 1250ms}) {
        D6R_REQUIRE(client.sampleNetwork(at(sampleAt)));
        probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, 1000 + static_cast<std::uint64_t>(sampleAt.count())),
                at(sampleAt + 20ms)) == R::ClientReplicationResult::NetworkSampled);
        D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        D6R_REQUIRE(client.takePresentationEvents().empty());
        D6R_REQUIRE_EQ(acceptedState, R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().state()}));
        const auto presentation = client.presentationState(at(sampleAt + 20ms));
        D6R_REQUIRE(!presentation.reconnecting);
        D6R_REQUIRE(presentation.canonicalStateCurrent);
    }

    D6R_REQUIRE(!client.presentationState(at(1549ms)).degraded);
    const auto stale = client.presentationState(at(1550ms));
    D6R_REQUIRE(stale.degraded);
    D6R_REQUIRE(!stale.reconnecting);
    D6R_REQUIRE(stale.canonicalStateCurrent);
}

D6R_TEST_CASE("NRP equal RTT probes tolerate changing path asymmetry without replacing confirmed state") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 1000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));

    // Every response has the same supported 20 ms RTT. The host timestamps vary within that
    // RTT to model a request path changing between 1 ms and 19 ms while response delay changes
    // in the opposite direction. This must recalibrate time without treating asymmetry as loss,
    // jitter, a transport failure, or a new canonical state.
    const std::vector<std::pair<std::chrono::milliseconds, std::uint64_t>> probes{
            {0ms, 1}, {250ms, 19}, {500ms, 2}, {750ms, 18}, {1000ms, 19}};
    std::size_t nextProbe = 0;
    const auto completeProbe = [&](std::chrono::milliseconds sentAt, std::uint64_t requestPath) {
        D6R_REQUIRE(client.sampleNetwork(at(sentAt)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        const auto hostResponseAt = HostEpoch
                + static_cast<std::uint64_t>(sentAt.count()) + requestPath;
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, hostResponseAt), at(sentAt + 20ms))
                    == R::ClientReplicationResult::NetworkSampled);
        const auto presentation = client.presentationState(at(sentAt + 20ms));
        D6R_REQUIRE(!presentation.reconnecting);
        D6R_REQUIRE(!presentation.degraded);
    };

    completeProbe(probes.front().first, probes.front().second);
    ++nextProbe;
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = HostEpoch + 20;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(25ms))
                == R::ClientReplicationResult::Applied);

    for (R::StateVersion version = 2; version <= 25; ++version) {
        const auto acceptedAt = 25ms + std::chrono::milliseconds((version - 1) * 50);
        while (nextProbe < probes.size() && probes[nextProbe].first < acceptedAt) {
            completeProbe(probes[nextProbe].first, probes[nextProbe].second);
            ++nextProbe;
        }
        state.phaseTime++;
        state.players[1].positionX = static_cast<std::int64_t>(version * 100);
        auto update = publisher.publish(state);
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = HostEpoch + 20 + (version - 1) * 50;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(acceptedAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(version, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        const auto presentation = client.presentationState(at(acceptedAt));
        D6R_REQUIRE(!presentation.reconnecting);
        D6R_REQUIRE(!presentation.degraded);
        D6R_REQUIRE(presentation.canonicalStateCurrent);
    }
    D6R_REQUIRE_EQ(probes.size(), nextProbe);

    // The first two measurement intervals intersect at local 270 ms in [1269, 1271], whose
    // midpoint is 1270. Every later supported-RTT sample is disjoint and therefore preserves
    // that bounded calibration. The final state accepted at 1225 ms was produced at host 2220,
    // so its exact estimated age is 5 ms. It crosses the 250 ms stale boundary at 1470 ms and
    // degradation begins exactly one second later; recalibration must not restart that clock.
    D6R_REQUIRE(!client.presentationState(at(2469ms)).degraded);
    const auto stale = client.presentationState(at(2470ms));
    D6R_REQUIRE(stale.degraded);
    D6R_REQUIRE(!stale.reconnecting);
    D6R_REQUIRE(stale.canonicalStateCurrent);

    const auto retainedPosition = state.players[1].positionX;
    D6R_REQUIRE_EQ(retainedPosition, client.replicatedState().state()->players[1].positionX);
    D6R_REQUIRE_EQ(retainedPosition, presented(client.presentedPlayers(at(1375ms)), 102).positionX);

    // A genuinely invalid regression in authoritative production time is still rejected.
    // It must route to reconnect while retaining, rather than replacing, canonical context
    // and the movement presentation confirmed above.
    state.phaseTime++;
    state.players[1].positionX = 999999;
    auto rejected = publisher.publish(state);
    D6R_REQUIRE(rejected.has_value());
    rejected->authoritativeProducedAt = HostEpoch + 20 + 23 * 50;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*rejected), at(2470ms))
                == R::ClientReplicationResult::Reconnecting);
    D6R_REQUIRE_EQ(R::StateVersion{25}, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE(client.replicatedState().retainedState() != nullptr);
    D6R_REQUIRE_EQ(retainedPosition,
                   client.replicatedState().retainedState()->players[1].positionX);
    const auto reconnecting = client.presentationState(at(2470ms));
    D6R_REQUIRE(reconnecting.reconnecting);
    D6R_REQUIRE(reconnecting.retainingLastConfirmedState);
    D6R_REQUIRE(!reconnecting.canonicalStateCurrent);
    D6R_REQUIRE_EQ(retainedPosition,
                   presented(client.presentedPlayers(at(2470ms)), 102).positionX);
}

D6R_TEST_CASE("NRP calibration exposes exact interval bounds midpoint age and future decisions") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto calibrate = [&](R::ClientReplicationConnection &client,
                               std::vector<std::vector<std::uint8_t>> &sent) {
        D6R_REQUIRE(client.sampleNetwork(at(0ms)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        // A response timestamp of 1000 received after a 20 ms RTT declares [1000, 1020].
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000),
                                   at(20ms))
                    == R::ClientReplicationResult::NetworkSampled);
    };

    std::vector<std::vector<std::uint8_t>> boundarySent;
    R::ClientReplicationConnection boundary([&](auto payload) {
        boundarySent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    calibrate(boundary, boundarySent);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::FullSnapshot exactUpper{1, state};
    // At local 30 ms the exact extrapolated interval is [1010, 1030].
    exactUpper.authoritativeProducedAt = 1030;
    D6R_REQUIRE(boundary.receive(R::serializeReplicationSnapshot(exactUpper), at(30ms))
                == R::ClientReplicationResult::Applied);
    state.phaseTime++;
    auto future = publisher.publish(state);
    D6R_REQUIRE(future.has_value());
    future->authoritativeProducedAt = 1031;
    D6R_REQUIRE(boundary.receive(R::serializeReplicationUpdate(*future), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{1}, boundary.replicatedState().version());
    D6R_REQUIRE(boundary.replicatedState().current());
    D6R_REQUIRE(!boundary.presentationState(at(30ms)).reconnecting);

    std::vector<std::vector<std::uint8_t>> ageSent;
    R::ClientReplicationConnection age([&](auto payload) {
        ageSent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    calibrate(age, ageSent);
    R::FullSnapshot exactLower{1, activeState()};
    exactLower.authoritativeProducedAt = 1000;
    D6R_REQUIRE(age.receive(R::serializeReplicationSnapshot(exactLower), at(30ms))
                == R::ClientReplicationResult::Applied);
    // The interval midpoint is 1020 at acceptance, making state age exactly 20 ms. Its local
    // production point is therefore 10 ms: 250 ms current age plus 1000 ms delay ends at 1260.
    D6R_REQUIRE(!age.presentationState(at(1259ms)).degraded);
    D6R_REQUIRE(age.presentationState(at(1260ms)).degraded);
}

D6R_TEST_CASE("NRP overlapping asymmetric recalibrations shrink bounds and move estimates both ways") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    const auto complete = [&](std::chrono::milliseconds sentAt,
                              std::chrono::milliseconds receivedAt,
                              std::uint64_t hostResponseAt) {
        D6R_REQUIRE(client.sampleNetwork(at(sentAt)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, hostResponseAt), at(receivedAt))
                    == R::ClientReplicationResult::NetworkSampled);
        D6R_REQUIRE(!client.presentationState(at(receivedAt)).reconnecting);
    };

    complete(0ms, 20ms, 1000);       // [1000, 1020], midpoint 1010.
    complete(250ms, 260ms, 1254);    // Prior [1240, 1260] intersects [1254, 1264]
                                      // as [1254, 1260], midpoint 1257: later and narrower.
    complete(500ms, 520ms, 1495);    // Prior [1514, 1520] intersects [1495, 1515]
                                      // as [1514, 1515], midpoint 1514: earlier, larger RTT.

    R::FullSnapshot upper{1, activeState()};
    upper.authoritativeProducedAt = 1515;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(upper), at(520ms))
                == R::ClientReplicationResult::Applied);
    auto changed = activeState();
    changed.phaseTime++;
    auto future = validUpdate(activeState(), changed);
    future.baseline = 1;
    future.version = 2;
    future.authoritativeProducedAt = 1516;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(future), at(520ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(!client.presentationState(at(520ms)).reconnecting);
}

D6R_TEST_CASE("NRP disjoint supported recalibration preserves prior bounded clock and future bound") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, 1000), at(20ms))
                == R::ClientReplicationResult::NetworkSampled); // [1000, 1020]

    D6R_REQUIRE(client.sampleNetwork(at(250ms)));
    probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, 2000), at(270ms))
                == R::ClientReplicationResult::NetworkSampled); // Disjoint from prior [1250,1270].
    D6R_REQUIRE(!client.presentationState(at(270ms)).reconnecting);

    R::FullSnapshot baseline{1, activeState()};
    baseline.authoritativeProducedAt = 1250;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(baseline), at(270ms))
                == R::ClientReplicationResult::Applied);
    auto changed = activeState();
    changed.phaseTime++;
    auto widenedFuture = validUpdate(activeState(), changed);
    widenedFuture.baseline = 1;
    widenedFuture.version = 2;
    widenedFuture.authoritativeProducedAt = 1271;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(widenedFuture), at(270ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE(!client.presentationState(at(270ms)).reconnecting);
    // Preserved midpoint 1260 gives this baseline an exact 10 ms age and degradation at 1510.
    D6R_REQUIRE(!client.presentationState(at(1509ms)).degraded);
    D6R_REQUIRE(client.presentationState(at(1510ms)).degraded);
}

D6R_TEST_CASE("NRP valid initial snapshot remains staged until clock calibration then applies normally") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostAtProbe = 2000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    client.setLocallyControlledPlayers({101});

    auto state = activeState();
    state.players[0].positionX = 1200;
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = HostAtProbe - 5;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(5ms))
                == R::ClientReplicationResult::Applied);

    // Admission may receive the initial canonical frame before its first clock response. The
    // successful receive only stages it: no canonical, presentation, event, or retained context
    // is observable before calibration.
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE(client.replicatedState().retainedState() == nullptr);
    D6R_REQUIRE(client.presentedPlayers(at(5ms)).empty());
    D6R_REQUIRE(client.takePresentationEvents().empty());
    D6R_REQUIRE(!client.predictLocalMovement({101, 9000, 0, false, false}, at(6ms)));
    const auto stagedPresentation = client.presentationState(at(6ms));
    D6R_REQUIRE(!stagedPresentation.canonicalStateCurrent);
    D6R_REQUIRE(!stagedPresentation.reconnecting);
    D6R_REQUIRE(!stagedPresentation.retainingLastConfirmedState);

    D6R_REQUIRE(client.sampleNetwork(at(10ms)));
    D6R_REQUIRE_EQ(std::size_t{1}, sent.size());
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, HostAtProbe + 10), at(30ms))
                == R::ClientReplicationResult::NetworkSampled);

    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() != nullptr);
    D6R_REQUIRE_EQ(std::int64_t{1200}, client.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE_EQ(std::int64_t{1200}, presented(client.presentedPlayers(at(30ms)), 101).positionX);
    D6R_REQUIRE(client.takePresentationEvents().empty());
    const auto calibratedPresentation = client.presentationState(at(30ms));
    D6R_REQUIRE(calibratedPresentation.canonicalStateCurrent);
    D6R_REQUIRE(!calibratedPresentation.reconnecting);
    D6R_REQUIRE(!calibratedPresentation.resynchronizing);
}

D6R_TEST_CASE("NRP staged payload accounting enforces per-connection and shared process limits and releases every reservation") {
    namespace T = Duel6::Network::Trust;
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto payload = stagedSnapshotPayload();
    auto &budget = T::processQueueBudget();
    const auto baseline = budget.used();
    D6R_REQUIRE_EQ(std::size_t{4 * 1024 * 1024}, Duel6::Network::MaxQueuedTransportPayloadBytes);
    D6R_REQUIRE_EQ(std::size_t{32 * 1024 * 1024}, T::MaxAggregateQueuedBytes);

    // One connection may stage bytes only through its 4 MiB boundary. The crossing frame fails
    // closed and releases all reservations owned by that connection.
    {
        R::ClientReplicationConnection client({}, N::Environment::SameMachine, true);
        std::size_t acceptedBytes = 0;
        for (;;) {
            const auto result = client.receive(payload, at(1ms));
            if (result == R::ClientReplicationResult::Reconnecting) break;
            D6R_REQUIRE_EQ(R::ClientReplicationResult::Applied, result);
            acceptedBytes += payload.size();
            D6R_REQUIRE_EQ(baseline + acceptedBytes, budget.used());
        }
        D6R_REQUIRE(acceptedBytes > 0);
        D6R_REQUIRE(acceptedBytes + payload.size() > Duel6::Network::MaxQueuedTransportPayloadBytes);
        D6R_REQUIRE_EQ(baseline, budget.used());
    }

    // Eight concurrent clients hold almost all of the shared 32 MiB process budget. A ninth
    // remains below its own 4 MiB limit but is declined exactly because the shared remainder is
    // smaller than one complete staged payload. Its partial reservations are released on error.
    std::vector<std::unique_ptr<R::ClientReplicationConnection>> clients;
    const auto framesPerConnection = Duel6::Network::MaxQueuedTransportPayloadBytes / payload.size();
    D6R_REQUIRE(framesPerConnection >= 4);
    for (std::size_t index = 0; index < 8; ++index) {
        clients.push_back(std::make_unique<R::ClientReplicationConnection>(
                R::ReplicationSender{}, N::Environment::SameMachine, true));
        for (std::size_t frame = 0; frame < framesPerConnection; ++frame)
            D6R_REQUIRE(clients.back()->receive(payload, at(1ms)) == R::ClientReplicationResult::Applied);
    }
    const auto heldByEight = budget.used() - baseline;
    D6R_REQUIRE(heldByEight <= T::MaxAggregateQueuedBytes);
    D6R_REQUIRE(T::MaxAggregateQueuedBytes - heldByEight < 4 * payload.size());

    clients.push_back(std::make_unique<R::ClientReplicationConnection>(
            R::ReplicationSender{}, N::Environment::SameMachine, true));
    std::size_t ninthAccepted = 0;
    while (T::MaxAggregateQueuedBytes - (budget.used() - baseline) >= payload.size()) {
        D6R_REQUIRE(clients.back()->receive(payload, at(1ms)) == R::ClientReplicationResult::Applied);
        ninthAccepted += payload.size();
    }
    D6R_REQUIRE(ninthAccepted + payload.size() <= Duel6::Network::MaxQueuedTransportPayloadBytes);
    D6R_REQUIRE(clients.back()->receive(payload, at(1ms)) == R::ClientReplicationResult::Reconnecting);
    D6R_REQUIRE_EQ(baseline + heldByEight, budget.used());

    // Explicit disconnect and object destruction independently return staged bytes.
    clients.front()->transportClosed();
    D6R_REQUIRE_EQ(baseline + heldByEight - framesPerConnection * payload.size(), budget.used());
    clients.clear();
    D6R_REQUIRE_EQ(baseline, budget.used());
    {
        R::ClientReplicationConnection destroyed({}, N::Environment::SameMachine, true);
        D6R_REQUIRE(destroyed.receive(payload, at(1ms)) == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(baseline + payload.size(), budget.used());
    }
    D6R_REQUIRE_EQ(baseline, budget.used());
}

D6R_TEST_CASE("NRP multiple pre-calibration incrementals preserve distinct events once and converge through an eventless tail") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 1000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = HostEpoch + 1;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(1ms))
                == R::ClientReplicationResult::Applied);

    state.phaseTime++;
    state.entities[0].positionX = 100;
    auto first = publisher.publish(state, {{901, "shot", 101, 102, 50, 1}});
    D6R_REQUIRE(first.has_value());
    first->authoritativeProducedAt = HostEpoch + 2;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*first), at(2ms))
                == R::ClientReplicationResult::WaitingForSnapshot);

    state.phaseTime++;
    R::WorldEntityState pickup;
    pickup.entityId = 51;
    pickup.kind = R::EntityKind::WeaponPickup;
    pickup.type = "bazooka";
    pickup.lifecycle = "available";
    state.entities.push_back(pickup);
    auto second = publisher.publish(state, {{902, "weapon-picked", 101, 0, 51, 1}});
    D6R_REQUIRE(second.has_value());
    second->authoritativeProducedAt = HostEpoch + 3;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*second), at(3ms))
                == R::ClientReplicationResult::WaitingForSnapshot);

    state.phaseTime++;
    state.players[1].positionX = 3456;
    state.entities[0].positionX = 200;
    auto eventless = publisher.publish(state);
    D6R_REQUIRE(eventless.has_value());
    eventless->authoritativeProducedAt = HostEpoch + 4;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*eventless), at(4ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, HostEpoch + 10), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE_EQ(R::StateVersion{4}, client.replicatedState().version());
    requireCoreStateEqual(state, *client.replicatedState().state());
    D6R_REQUIRE_EQ(std::int64_t{3456}, player(*client.replicatedState().state(), 102)->positionX);
    D6R_REQUIRE_EQ(std::int64_t{200}, entity(*client.replicatedState().state(), 50)->positionX);
    D6R_REQUIRE(entity(*client.replicatedState().state(), 51) != nullptr);
    const auto events = client.takePresentationEvents();
    D6R_REQUIRE_EQ(std::size_t{2}, events.size());
    D6R_REQUIRE_EQ(R::Identity{901}, events[0].eventId);
    D6R_REQUIRE_EQ(R::Identity{902}, events[1].eventId);
    D6R_REQUIRE(client.takePresentationEvents().empty());
}

D6R_TEST_CASE("NRP late asymmetric calibration outside budget cannot understate uncertainty or admit a future timestamp") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 2000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);

    auto hostile = activeState();
    hostile.players[0].positionX = 9000;
    R::FullSnapshot staged{1, hostile};
    // At local acceptance 1 ms, a valid later calibration maps host time to 2001 ms with 10 ms
    // uncertainty. This timestamp is therefore one millisecond beyond the admissible future.
    staged.authoritativeProducedAt = HostEpoch + 12;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(staged), at(1ms))
                == R::ClientReplicationResult::Applied);

    // A 21 ms same-machine RTT is outside the approved 20 ms budget. Put the host response at
    // the extreme end of that path: it must not establish an understated 11 ms calibration.
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, HostEpoch + 21), at(21ms))
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
    D6R_REQUIRE_EQ(std::size_t{1}, sent.size());

    D6R_REQUIRE(client.sampleNetwork(at(250ms)));
    probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(
            *probe->qualitySequence, HostEpoch + 260), at(270ms))
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE(client.takePresentationEvents().empty());
    D6R_REQUIRE_EQ(std::size_t{3}, sent.size());
    const auto request = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(request && request->kind == R::ReplicationFrameKind::ResynchronizationRequest);
}

D6R_TEST_CASE("AC-021 sealed invalid baseline defers one request until connected resumption and then recovers current") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 3000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    auto baselineState = activeState();
    R::FullSnapshot baseline{1, baselineState};
    baseline.authoritativeProducedAt = HostEpoch + 1;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationSnapshot(baseline), at(1ms), false)
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(R::serializeQualityResponse(
            *probe->qualitySequence, HostEpoch + 10), at(20ms), false)
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE(client.initialAdmissionState() != nullptr);

    auto invalid = validUpdate(baselineState, baselineState);
    invalid.baseline = 99;
    invalid.version = 100;
    invalid.authoritativeProducedAt = HostEpoch + 21;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationUpdate(invalid), at(21ms), false)
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationUpdate(invalid), at(21ms), false)
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(std::size_t{1}, sent.size());
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.initialAdmissionState() != nullptr);

    client.resumeOutboundProcessing();
    client.resumeOutboundProcessing();
    D6R_REQUIRE_EQ(std::size_t{2}, sent.size());
    const auto request = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(request && request->kind == R::ReplicationFrameKind::ResynchronizationRequest);

    auto recoveredState = baselineState;
    recoveredState.phaseTime++;
    recoveredState.players[0].positionX = 4321;
    R::FullSnapshot recovered{2, recoveredState};
    recovered.authoritativeProducedAt = HostEpoch + 22;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(recovered), at(22ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE_EQ(std::int64_t{4321}, client.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE_EQ(std::int64_t{1000}, client.initialAdmissionState()->players[0].positionX);
    D6R_REQUIRE_EQ(std::size_t{2}, sent.size());
}

D6R_TEST_CASE("AC-002 AC-021 NRP predeadline calibration obeys the 20 ms same-machine budget and later sampling") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostResponseAt = 2000;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);

    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto firstProbe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(firstProbe && firstProbe->qualitySequence);

    auto state = activeState();
    state.players[0].positionX = 1200;
    R::FullSnapshot staged{1, state};
    // A response at the approved 20 ms boundary synchronizes at host time 2010. At the
    // snapshot's receipt time the calibrated host clock is 1991, so this frame remains valid.
    staged.authoritativeProducedAt = 1990;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationSnapshot(staged), at(1ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());

    D6R_REQUIRE(client.receiveInitialAdmissionFrame(R::serializeQualityResponse(
            *firstProbe->qualitySequence, HostResponseAt), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE_EQ(std::int64_t{1200},
                   client.replicatedState().state()->players[0].positionX);

    // Sampling at the same application time starts the next ordinary probe but must not erase
    // the completed admission calibration or reprocess the staged canonical state.
    D6R_REQUIRE(client.sampleNetwork(at(250ms)));
    D6R_REQUIRE_EQ(std::size_t{2}, sent.size());
    const auto secondProbe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(secondProbe && secondProbe->qualitySequence
                && *secondProbe->qualitySequence == *firstProbe->qualitySequence + 1);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE(client.takePresentationEvents().empty());

    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    state.phaseTime++;
    state.players[0].positionX = 1400;
    auto update = publisher.publish(state, {{710, "shot", 101, 102, 0, 1}});
    D6R_REQUIRE(update.has_value());
    update->authoritativeProducedAt = 2241;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(251ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
    D6R_REQUIRE_EQ(std::int64_t{1400},
                   client.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE_EQ(std::size_t{1}, client.takePresentationEvents().size());
}

D6R_TEST_CASE("REP-042 NRP future full snapshots are atomic before admission and on established connections") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 4000;
    const auto samePresentation = [](const N::ConnectionPresentationState &left,
                                     const N::ConnectionPresentationState &right) {
        return left.canonicalStateCurrent == right.canonicalStateCurrent
               && left.degraded == right.degraded
               && left.resynchronizing == right.resynchronizing
               && left.reconnecting == right.reconnecting
               && left.retainingLastConfirmedState == right.retainingLastConfirmedState
               && left.degradedText == right.degradedText
               && left.retainedStateText == right.retainedStateText
               && left.synchronizationText == right.synchronizationText
               && left.reconnectingText == right.reconnectingText;
    };
    const auto samePoses = [](const std::vector<N::PresentedPlayerPose> &left,
                              const std::vector<N::PresentedPlayerPose> &right) {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            if (left[index].playerId != right[index].playerId
                || left[index].positionX != right[index].positionX
                || left[index].positionY != right[index].positionY
                || left[index].facingLeft != right[index].facingLeft
                || left[index].crouching != right[index].crouching)
                return false;
        }
        return true;
    };
    const auto calibrate = [&](R::ClientReplicationConnection &client,
                               std::vector<std::vector<std::uint8_t>> &sent) {
        D6R_REQUIRE(client.sampleNetwork(at(0ms)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, HostEpoch + 10), at(20ms))
                    == R::ClientReplicationResult::NetworkSampled);
    };

    // Before initial admission, rejection must not manufacture any current, retained, event,
    // movement, prediction, quality, degradation, synchronization, or reconnecting surface.
    std::vector<std::vector<std::uint8_t>> freshSent;
    R::ClientReplicationConnection fresh([&](auto payload) {
        freshSent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    fresh.setLocallyControlledPlayers({101});
    calibrate(fresh, freshSent);
    const auto freshPresentationBefore = fresh.presentationState(at(30ms));
    const auto freshPosesBefore = fresh.presentedPlayers(at(30ms));
    D6R_REQUIRE(!fresh.predictLocalMovement({101, 5000, 0, true, true}, at(30ms)));
    D6R_REQUIRE(fresh.takePresentationEvents().empty());

    auto hostileFreshState = activeState();
    hostileFreshState.players[0].positionX = 9000;
    R::FullSnapshot hostileFresh{9, hostileFreshState};
    hostileFresh.authoritativeProducedAt = HostEpoch + 41;
    D6R_REQUIRE(fresh.receive(R::serializeReplicationSnapshot(hostileFresh), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{0}, fresh.replicatedState().version());
    D6R_REQUIRE(!fresh.replicatedState().current());
    D6R_REQUIRE(fresh.replicatedState().state() == nullptr);
    D6R_REQUIRE(fresh.replicatedState().retainedState() == nullptr);
    D6R_REQUIRE(samePoses(freshPosesBefore, fresh.presentedPlayers(at(30ms))));
    D6R_REQUIRE(!fresh.predictLocalMovement({101, 5000, 0, true, true}, at(30ms)));
    D6R_REQUIRE(fresh.takePresentationEvents().empty());
    D6R_REQUIRE(samePresentation(freshPresentationBefore, fresh.presentationState(at(30ms))));

    auto admittedState = activeState();
    admittedState.players[0].positionX = 1300;
    R::FullSnapshot admitted{1, admittedState};
    admitted.authoritativeProducedAt = HostEpoch + 30;
    D6R_REQUIRE(fresh.receive(R::serializeReplicationSnapshot(admitted), at(30ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{1}, fresh.replicatedState().version());
    D6R_REQUIRE_EQ(std::int64_t{1300},
                   fresh.replicatedState().state()->players[0].positionX);

    // Repeat on a current established connection with retained canonical context and an active
    // local prediction. The rejected complete frame must not start synchronization or degradation.
    std::vector<std::vector<std::uint8_t>> establishedSent;
    R::ClientReplicationConnection established([&](auto payload) {
        establishedSent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    established.setLocallyControlledPlayers({101});
    calibrate(established, establishedSent);
    R::FullSnapshot baseline{1, admittedState};
    baseline.authoritativeProducedAt = HostEpoch + 20;
    D6R_REQUIRE(established.receive(R::serializeReplicationSnapshot(baseline), at(20ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(established.predictLocalMovement({101, 1500, 25, true, true}, at(21ms)));
    const auto canonicalBefore = R::serializeReplicationSnapshot(
            {1, *established.replicatedState().state()});
    const auto retainedBefore = R::serializeReplicationSnapshot(
            {1, *established.replicatedState().retainedState()});
    const auto posesBefore = established.presentedPlayers(at(30ms));
    const auto presentationBefore = established.presentationState(at(30ms));
    D6R_REQUIRE(established.takePresentationEvents().empty());

    auto hostileEstablishedState = admittedState;
    hostileEstablishedState.phaseTime = 999;
    hostileEstablishedState.players[0].positionX = 9999;
    R::FullSnapshot hostileEstablished{8, hostileEstablishedState};
    hostileEstablished.authoritativeProducedAt = HostEpoch + 41;
    D6R_REQUIRE(established.receive(
            R::serializeReplicationSnapshot(hostileEstablished), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{1}, established.replicatedState().version());
    D6R_REQUIRE(established.replicatedState().current());
    D6R_REQUIRE_EQ(canonicalBefore, R::serializeReplicationSnapshot(
            {1, *established.replicatedState().state()}));
    D6R_REQUIRE_EQ(retainedBefore, R::serializeReplicationSnapshot(
            {1, *established.replicatedState().retainedState()}));
    D6R_REQUIRE(samePoses(posesBefore, established.presentedPlayers(at(30ms))));
    D6R_REQUIRE(established.takePresentationEvents().empty());
    D6R_REQUIRE(samePresentation(presentationBefore, established.presentationState(at(30ms))));

    auto recoveredState = admittedState;
    recoveredState.phaseTime++;
    recoveredState.players[0].positionX = 1600;
    R::FullSnapshot recovered{2, recoveredState};
    recovered.authoritativeProducedAt = HostEpoch + 30;
    D6R_REQUIRE(established.receive(R::serializeReplicationSnapshot(recovered), at(30ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{2}, established.replicatedState().version());
    D6R_REQUIRE_EQ(std::int64_t{1600},
                   established.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE(established.presentationState(at(30ms)).canonicalStateCurrent);
}

D6R_TEST_CASE("AC-021 sealed admission drain cannot start replacement exchanges or replace timeout with send failure") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostResponseAt = 2000;
    std::size_t sendAttempts = 0;
    bool failSends = false;
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        ++sendAttempts;
        if (failSends) return Duel6::Network::SendResult::NotConnected;
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);

    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    failSends = true;

    auto state = activeState();
    auto incremental = validUpdate(state, state);
    incremental.baseline = 1;
    incremental.version = 2;
    incremental.authoritativeProducedAt = 1801;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationUpdate(incremental), at(1ms), false)
                == R::ClientReplicationResult::WaitingForSnapshot);

    state.players[0].positionX = 9000;
    R::FullSnapshot stagedFuture{9, state};
    stagedFuture.authoritativeProducedAt = 2201;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationSnapshot(stagedFuture), at(2ms), false)
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(R::serializeQualityResponse(
            *probe->qualitySequence, HostResponseAt), at(20ms), false)
                == R::ClientReplicationResult::NetworkSampled);

    R::FullSnapshot futureAfterCalibration{10, state};
    futureAfterCalibration.authoritativeProducedAt = 2022;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationSnapshot(futureAfterCalibration), at(21ms), false)
                == R::ClientReplicationResult::WaitingForSnapshot);

    // A sealed drain is read-only with respect to transport output. In particular, the failed
    // sender was never called for a new probe or replacement request, so the caller can retain
    // its selected admission-timeout outcome rather than observing SendFailed.
    D6R_REQUIRE_EQ(std::size_t{1}, sendAttempts);
    D6R_REQUIRE_EQ(std::size_t{1}, sent.size());
    D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
    D6R_REQUIRE(!client.presentationState(at(21ms)).reconnecting);

    auto legitimateState = activeState();
    legitimateState.players[0].positionX = 1400;
    R::FullSnapshot legitimate{1, legitimateState};
    legitimate.authoritativeProducedAt = 2011;
    D6R_REQUIRE(client.receiveInitialAdmissionFrame(
            R::serializeReplicationSnapshot(legitimate), at(21ms), false)
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE_EQ(std::int64_t{1400},
                   client.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE_EQ(std::size_t{1}, sendAttempts);
}

D6R_TEST_CASE("NRP implausibly future staged initial snapshot is discarded without admission deadlock or mutation") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto verifyEnvironment = [&](N::Environment environment,
                                       std::chrono::milliseconds roundTrip,
                                       std::uint64_t uncertainty) {
        constexpr std::uint64_t HostAtProbe = 2000;
        const auto snapshotAcceptedAt = 5ms;
        const auto probeSentAt = 10ms;
        const auto responseAt = probeSentAt + roundTrip;
        const auto halfRoundTrip = static_cast<std::uint64_t>(roundTrip.count() / 2);
        const auto hostAtSnapshotAcceptance = HostAtProbe - 5;
        std::vector<std::vector<std::uint8_t>> sent;
        R::ClientReplicationConnection client([&](auto payload) {
            sent.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }, environment, true);
        client.setLocallyControlledPlayers({101});

        auto state = activeState();
        state.players[0].positionX = 8000;
        R::FullSnapshot staged{1, state};
        staged.authoritativeProducedAt = hostAtSnapshotAcceptance + uncertainty + 1;
        D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(staged), at(snapshotAcceptedAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().state() == nullptr);
        D6R_REQUIRE(client.replicatedState().retainedState() == nullptr);
        D6R_REQUIRE(client.presentedPlayers(at(snapshotAcceptedAt)).empty());
        D6R_REQUIRE(client.takePresentationEvents().empty());

        D6R_REQUIRE(client.sampleNetwork(at(probeSentAt)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, HostAtProbe + halfRoundTrip), at(responseAt))
                    == R::ClientReplicationResult::NetworkSampled);

        // Calibration rejects and discards the staged frame, requests a replacement, and leaves
        // every canonical/presentation surface pristine without closing the admitted connection.
        D6R_REQUIRE_EQ(std::size_t{2}, sent.size());
        const auto request = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(request && request->kind == R::ReplicationFrameKind::ResynchronizationRequest);
        D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());
        D6R_REQUIRE(!client.replicatedState().current());
        D6R_REQUIRE(client.replicatedState().state() == nullptr);
        D6R_REQUIRE(client.replicatedState().retainedState() == nullptr);
        D6R_REQUIRE(client.presentedPlayers(at(responseAt)).empty());
        D6R_REQUIRE(client.takePresentationEvents().empty());
        D6R_REQUIRE(!client.predictLocalMovement({101, 9000, 0, false, false}, at(responseAt)));
        const auto rejectedPresentation = client.presentationState(at(responseAt));
        D6R_REQUIRE(!rejectedPresentation.canonicalStateCurrent);
        D6R_REQUIRE(!rejectedPresentation.reconnecting);
        D6R_REQUIRE(!rejectedPresentation.retainingLastConfirmedState);

        // An earlier acceptance time and lower production timestamp than the rejected frame prove
        // that neither timing watermark was consumed. Recovery succeeds on this connection.
        auto legitimateState = activeState();
        legitimateState.players[0].positionX = 1400;
        R::FullSnapshot legitimate{1, legitimateState};
        legitimate.authoritativeProducedAt = hostAtSnapshotAcceptance - 1;
        D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(legitimate), at(4ms))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        D6R_REQUIRE_EQ(std::int64_t{1400},
                       presented(client.presentedPlayers(at(responseAt)), 101).positionX);
        D6R_REQUIRE(!client.presentationState(at(responseAt)).reconnecting);

        auto updatedState = legitimateState;
        updatedState.phaseTime++;
        updatedState.players[0].positionX = 1600;
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(legitimateState));
        auto update = publisher.publish(updatedState, {{702, "shot", 101, 102, 0, 1}});
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = HostAtProbe + static_cast<std::uint64_t>(roundTrip.count()) + 1;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(responseAt + 1ms))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        D6R_REQUIRE(!client.presentationState(at(responseAt + 1ms)).reconnecting);
        const auto events = client.takePresentationEvents();
        D6R_REQUIRE_EQ(std::size_t{1}, events.size());
        D6R_REQUIRE_EQ(R::Identity{702}, events.front().eventId);
    };

    verifyEnvironment(N::Environment::SameMachine, 20ms, 10);
    verifyEnvironment(N::Environment::PrivateLan, 100ms, 50);
}

D6R_TEST_CASE("REP-042 REP-054 REP-059 NRP future full snapshots renew recovery without mutating fresh or retained state") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    constexpr std::uint64_t HostEpoch = 4000;
    const auto calibrate = [&](R::ClientReplicationConnection &client,
                               std::vector<std::vector<std::uint8_t>> &sent) {
        D6R_REQUIRE(client.sampleNetwork(at(0ms)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, HostEpoch + 10), at(20ms))
                    == R::ClientReplicationResult::NetworkSampled);
    };

    // A calibrated client with no baseline rejects a future complete frame, requests a
    // replacement, and keeps every state, movement, event, and timing surface pristine.
    std::vector<std::vector<std::uint8_t>> freshSent;
    R::ClientReplicationConnection fresh([&](auto payload) {
        freshSent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    fresh.setLocallyControlledPlayers({101});
    calibrate(fresh, freshSent);
    auto rejectedFreshState = activeState();
    rejectedFreshState.phaseTime = 900;
    rejectedFreshState.players[0].positionX = 9000;
    R::WorldEntityState staleEntity;
    staleEntity.entityId = 77;
    staleEntity.kind = R::EntityKind::Explosion;
    staleEntity.type = "stale";
    staleEntity.positionX = 9;
    staleEntity.positionY = 9;
    staleEntity.lifecycle = "active";
    rejectedFreshState.entities.push_back(staleEntity);
    R::FullSnapshot rejectedFresh{9, rejectedFreshState};
    rejectedFresh.authoritativeProducedAt = HostEpoch + 41;
    D6R_REQUIRE(fresh.receive(R::serializeReplicationSnapshot(rejectedFresh), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(std::size_t{2}, freshSent.size());
    D6R_REQUIRE_EQ(R::StateVersion{0}, fresh.replicatedState().version());
    D6R_REQUIRE(!fresh.replicatedState().current());
    D6R_REQUIRE(fresh.replicatedState().state() == nullptr);
    D6R_REQUIRE(fresh.replicatedState().retainedState() == nullptr);
    D6R_REQUIRE(fresh.presentedPlayers(at(30ms)).empty());
    D6R_REQUIRE(!fresh.predictLocalMovement({101, 1234, 0, false, false}, at(30ms)));
    D6R_REQUIRE(fresh.takePresentationEvents().empty());

    auto freshValidState = activeState();
    freshValidState.phaseTime = 31;
    freshValidState.players[0].positionX = 1300;
    R::FullSnapshot freshValid{1, freshValidState};
    freshValid.authoritativeProducedAt = HostEpoch + 30;
    D6R_REQUIRE(fresh.receive(R::serializeReplicationSnapshot(freshValid), at(30ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{1}, fresh.replicatedState().version());
    D6R_REQUIRE_EQ(std::int64_t{1300}, presented(fresh.presentedPlayers(at(30ms)), 101).positionX);
    D6R_REQUIRE(entity(*fresh.replicatedState().state(), 77) == nullptr);
    D6R_REQUIRE(fresh.takePresentationEvents().empty());

    // During an already active resynchronization, each future full snapshot is a failed
    // replacement rather than completion of the attempt, so it renews exactly one request.
    std::vector<std::vector<std::uint8_t>> activeSent;
    R::ClientReplicationConnection active([&](auto payload) {
        activeSent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    active.setLocallyControlledPlayers({101});
    calibrate(active, activeSent);
    auto baselineState = activeState();
    baselineState.players[0].positionX = 1400;
    R::FullSnapshot baseline{1, baselineState};
    baseline.authoritativeProducedAt = HostEpoch + 20;
    D6R_REQUIRE(active.receive(R::serializeReplicationSnapshot(baseline), at(20ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(active.predictLocalMovement({101, 1450, 50, true, true}, at(21ms)));
    const auto retainedBytes = R::serializeReplicationSnapshot({1, *active.replicatedState().state()});

    auto invalidUpdate = validUpdate(baselineState, baselineState, {{801, "shot", 101, 102, 0, 1}});
    invalidUpdate.baseline = 99;
    invalidUpdate.version = 100;
    invalidUpdate.authoritativeProducedAt = HostEpoch + 30;
    D6R_REQUIRE(active.receive(R::serializeReplicationUpdate(invalidUpdate), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(std::size_t{2}, activeSent.size());
    D6R_REQUIRE(!active.replicatedState().current());
    D6R_REQUIRE(active.replicatedState().state() == nullptr);
    D6R_REQUIRE_EQ(retainedBytes, R::serializeReplicationSnapshot(
            {1, *active.replicatedState().retainedState()}));
    D6R_REQUIRE(active.takePresentationEvents().empty());
    const auto retainedPose = presented(active.presentedPlayers(at(30ms)), 101);

    auto rejectedReplacementState = baselineState;
    rejectedReplacementState.phaseTime = 999;
    rejectedReplacementState.players[0].positionX = 9999;
    rejectedReplacementState.entities.clear();
    R::FullSnapshot rejectedReplacement{8, rejectedReplacementState};
    rejectedReplacement.authoritativeProducedAt = HostEpoch + 51;
    D6R_REQUIRE(active.receive(R::serializeReplicationSnapshot(rejectedReplacement), at(40ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(std::size_t{3}, activeSent.size());
    const auto renewedRequest = R::deserializeReplicationFrame(activeSent.back());
    D6R_REQUIRE(renewedRequest);
    D6R_REQUIRE_EQ(R::ReplicationFrameKind::ResynchronizationRequest, renewedRequest->kind);
    D6R_REQUIRE_EQ(R::StateVersion{1}, active.replicatedState().version());
    D6R_REQUIRE(!active.replicatedState().current());
    D6R_REQUIRE_EQ(retainedBytes, R::serializeReplicationSnapshot(
            {1, *active.replicatedState().retainedState()}));
    const auto rejectedPose = presented(active.presentedPlayers(at(40ms)), 101);
    D6R_REQUIRE_EQ(retainedPose.positionX, rejectedPose.positionX);
    D6R_REQUIRE_EQ(retainedPose.positionY, rejectedPose.positionY);
    D6R_REQUIRE(active.takePresentationEvents().empty());

    auto convergedState = baselineState;
    convergedState.phaseTime = 40;
    convergedState.players[0].positionX = 1600;
    convergedState.entities.clear();
    R::FullSnapshot converged{2, convergedState};
    converged.authoritativeProducedAt = HostEpoch + 40;
    D6R_REQUIRE(active.receive(R::serializeReplicationSnapshot(converged), at(40ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{2}, active.replicatedState().version());
    D6R_REQUIRE(active.replicatedState().current());
    D6R_REQUIRE_EQ(std::uint64_t{40}, active.replicatedState().state()->phaseTime);
    D6R_REQUIRE_EQ(std::int64_t{1600}, presented(active.presentedPlayers(at(40ms)), 101).positionX);
    D6R_REQUIRE(active.replicatedState().state()->entities.empty());
    D6R_REQUIRE(active.takePresentationEvents().empty());
    const auto presentation = active.presentationState(at(40ms));
    D6R_REQUIRE(presentation.canonicalStateCurrent);
    D6R_REQUIRE(!presentation.resynchronizing);
    D6R_REQUIRE(!presentation.reconnecting);
}

D6R_TEST_CASE("NRP calibrated clients reject bounded-future timestamps without consuming state or time") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto verifyEnvironment = [&](N::Environment environment,
                                       std::chrono::milliseconds roundTrip,
                                       std::uint64_t uncertainty) {
        constexpr std::uint64_t HostEpoch = 1000;
        std::vector<std::vector<std::uint8_t>> sent;
        R::ClientReplicationConnection client([&](auto payload) {
            sent.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }, environment, true);
        client.setLocallyControlledPlayers({101});

        D6R_REQUIRE(client.sampleNetwork(at(0ms)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);
        const auto halfRoundTrip = static_cast<std::uint64_t>(roundTrip.count() / 2);
        const auto synchronizedHostTime = HostEpoch + halfRoundTrip * 2;
        D6R_REQUIRE(client.receive(R::serializeQualityResponse(
                *probe->qualitySequence, HostEpoch + halfRoundTrip), at(roundTrip))
                    == R::ClientReplicationResult::NetworkSampled);

        auto state = activeState();
        R::AuthoritativeStateReplicator publisher;
        D6R_REQUIRE(publisher.initialize(state));
        R::FullSnapshot snapshot{1, state};
        snapshot.authoritativeProducedAt = synchronizedHostTime;
        D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(roundTrip))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE(client.predictLocalMovement(
                {101, 3333, 4444, true, false}, at(roundTrip + 1ms)));

        const auto rejectedAt = roundTrip + 10ms;
        const auto hostAtRejection = synchronizedHostTime + 10;
        const auto retainedState = R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().retainedState()});
        const auto retainedMovement = presented(client.presentedPlayers(at(rejectedAt)), 101);
        D6R_REQUIRE(client.takePresentationEvents().empty());

        state.phaseTime++;
        state.players[0].positionX = 9000;
        const R::PresentationEvent outcome{701, "shot", 101, 102, 50, 1};
        auto update = publisher.publish(state, {outcome});
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = hostAtRejection + uncertainty + 1;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(rejectedAt))
                    == R::ClientReplicationResult::WaitingForSnapshot);

        D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        D6R_REQUIRE_EQ(retainedState, R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().state()}));
        D6R_REQUIRE_EQ(retainedState, R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().retainedState()}));
        const auto movementAfterRejection = presented(client.presentedPlayers(at(rejectedAt)), 101);
        D6R_REQUIRE_EQ(retainedMovement.positionX, movementAfterRejection.positionX);
        D6R_REQUIRE_EQ(retainedMovement.positionY, movementAfterRejection.positionY);
        D6R_REQUIRE_EQ(retainedMovement.facingLeft, movementAfterRejection.facingLeft);
        D6R_REQUIRE_EQ(retainedMovement.crouching, movementAfterRejection.crouching);
        D6R_REQUIRE(client.takePresentationEvents().empty());
        const auto currentPresentation = client.presentationState(at(rejectedAt));
        D6R_REQUIRE(!currentPresentation.reconnecting);
        D6R_REQUIRE(currentPresentation.canonicalStateCurrent);
        D6R_REQUIRE(!currentPresentation.retainingLastConfirmedState);

        // This timestamp is monotonic from the accepted snapshot but lower than the rejected
        // timestamp. Acceptance proves that rejection consumed neither the version nor the
        // authoritative-production-time watermark and does not require a reconnect.
        update->authoritativeProducedAt = hostAtRejection;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(rejectedAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());
        D6R_REQUIRE(!client.presentationState(at(rejectedAt)).reconnecting);
        const auto acceptedEvents = client.takePresentationEvents();
        D6R_REQUIRE_EQ(std::size_t{1}, acceptedEvents.size());
        D6R_REQUIRE_EQ(R::Identity{701}, acceptedEvents.front().eventId);

        state.phaseTime++;
        state.players[0].positionX = 10000;
        auto boundary = publisher.publish(state);
        D6R_REQUIRE(boundary.has_value());
        const auto boundaryAt = roundTrip + 20ms;
        boundary->authoritativeProducedAt = synchronizedHostTime + 20 + uncertainty;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*boundary), at(boundaryAt))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{3}, client.replicatedState().version());

        state.phaseTime++;
        state.players[0].positionX = 11000;
        auto beyondBoundary = publisher.publish(state);
        D6R_REQUIRE(beyondBoundary.has_value());
        const auto boundaryState = R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().state()});
        const auto beyondBoundaryAt = roundTrip + 30ms;
        beyondBoundary->authoritativeProducedAt = synchronizedHostTime + 30 + uncertainty + 1;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*beyondBoundary), at(beyondBoundaryAt))
                    == R::ClientReplicationResult::WaitingForSnapshot);
        D6R_REQUIRE_EQ(R::StateVersion{3}, client.replicatedState().version());
        D6R_REQUIRE_EQ(boundaryState, R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().state()}));
        D6R_REQUIRE(!client.presentationState(at(beyondBoundaryAt)).reconnecting);

        const auto movementBeforeOverflow = presented(
                client.presentedPlayers(at(beyondBoundaryAt)), 101);
        beyondBoundary->authoritativeProducedAt = std::numeric_limits<std::uint64_t>::max();
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*beyondBoundary), at(beyondBoundaryAt))
                    == R::ClientReplicationResult::Reconnecting);
        D6R_REQUIRE_EQ(R::StateVersion{3}, client.replicatedState().version());
        D6R_REQUIRE(!client.replicatedState().current());
        D6R_REQUIRE(client.replicatedState().state() == nullptr);
        D6R_REQUIRE_EQ(boundaryState, R::serializeReplicationSnapshot(
                {client.replicatedState().version(), *client.replicatedState().retainedState()}));
        const auto movementAfterOverflow = presented(
                client.presentedPlayers(at(beyondBoundaryAt)), 101);
        D6R_REQUIRE_EQ(movementBeforeOverflow.positionX, movementAfterOverflow.positionX);
        D6R_REQUIRE_EQ(movementBeforeOverflow.positionY, movementAfterOverflow.positionY);
        D6R_REQUIRE(client.takePresentationEvents().empty());
        const auto overflowPresentation = client.presentationState(at(beyondBoundaryAt));
        D6R_REQUIRE(overflowPresentation.reconnecting);
        D6R_REQUIRE(overflowPresentation.retainingLastConfirmedState);
    };

    verifyEnvironment(N::Environment::SameMachine, 20ms, 10);
    verifyEnvironment(N::Environment::PrivateLan, 100ms, 50);
}

D6R_TEST_CASE("NRP positive sub-millisecond RTT across a host millisecond boundary admits initial and established state") {
    const auto at = [](std::chrono::nanoseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto exercise = [&](bool initialAdmission) {
        std::vector<std::vector<std::uint8_t>> sent;
        R::ClientReplicationConnection client([&](auto payload) {
            sent.push_back(std::move(payload));
            return Duel6::Network::SendResult::Accepted;
        }, N::Environment::SameMachine, true);

        D6R_REQUIRE(client.sampleNetwork(at(750us)));
        const auto probe = R::deserializeReplicationFrame(sent.back());
        D6R_REQUIRE(probe && probe->qualitySequence);

        auto state = activeState();
        R::FullSnapshot snapshot{1, state};
        // The positive 500 us path crosses host millisecond 2001. Its conservative one-
        // millisecond interval admits 2002 at the upper bound; truncating the RTT to zero does not.
        snapshot.authoritativeProducedAt = 2002;
        const auto snapshotPayload = R::serializeReplicationSnapshot(snapshot);
        const auto snapshotAt = at(1100us);
        const auto staged = initialAdmission
                            ? client.receiveInitialAdmissionFrame(snapshotPayload, snapshotAt)
                            : client.receive(snapshotPayload, snapshotAt);
        D6R_REQUIRE_EQ(R::ClientReplicationResult::Applied, staged);
        D6R_REQUIRE_EQ(R::StateVersion{0}, client.replicatedState().version());

        const auto response = R::serializeQualityResponse(*probe->qualitySequence, 2001);
        const auto sampled = initialAdmission
                             ? client.receiveInitialAdmissionFrame(response, at(1250us))
                             : client.receive(response, at(1250us));
        D6R_REQUIRE_EQ(R::ClientReplicationResult::NetworkSampled, sampled);
        D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
        D6R_REQUIRE(client.replicatedState().current());

        state.phaseTime++;
        state.players[0].positionX = 1300;
        auto update = validUpdate(activeState(), state);
        update.baseline = 1;
        update.version = 2;
        update.authoritativeProducedAt = 2003;
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(update), at(1300us))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
        D6R_REQUIRE_EQ(std::int64_t{1300}, client.replicatedState().state()->players[0].positionX);
    };

    exercise(true);
    exercise(false);
}

D6R_TEST_CASE("REP-048 REP-054 NRP same-round phase regressions retain every accepted client surface") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto samePresentation = [](const N::ConnectionPresentationState &left,
                                     const N::ConnectionPresentationState &right) {
        return left.canonicalStateCurrent == right.canonicalStateCurrent
               && left.degraded == right.degraded
               && left.resynchronizing == right.resynchronizing
               && left.reconnecting == right.reconnecting
               && left.retainingLastConfirmedState == right.retainingLastConfirmedState
               && left.degradedText == right.degradedText
               && left.retainedStateText == right.retainedStateText
               && left.synchronizationText == right.synchronizationText
               && left.reconnectingText == right.reconnectingText;
    };
    const auto samePose = [](const N::PresentedPlayerPose &left,
                             const N::PresentedPlayerPose &right) {
        return left.playerId == right.playerId && left.positionX == right.positionX
               && left.positionY == right.positionY && left.facingLeft == right.facingLeft
               && left.crouching == right.crouching;
    };

    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    client.setLocallyControlledPlayers({101});
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    const auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);

    auto baselineState = activeState();
    baselineState.phaseTime = 50;
    baselineState.players[0].positionX = 1200;
    R::FullSnapshot baseline{1, baselineState};
    baseline.authoritativeProducedAt = 1020;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(baseline), at(20ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(client.predictLocalMovement({101, 1700, 25, true, true}, at(21ms)));
    D6R_REQUIRE(client.takePresentationEvents().empty());
    const auto acceptedBytes = R::serializeReplicationSnapshot({1, baselineState});
    const auto predictedBefore = presented(client.presentedPlayers(at(30ms)), 101);

    auto regressedState = baselineState;
    regressedState.phaseTime--;
    regressedState.players[0].positionX = 9000;
    auto regressedUpdate = stateOnlyUpdate(
            1, 2, regressedState, {{800, "shot", 101, 102, 50, 1}});
    regressedUpdate.authoritativeProducedAt = 1030;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(regressedUpdate), at(30ms))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE_EQ(acceptedBytes, R::serializeReplicationSnapshot(
            {1, *client.replicatedState().retainedState()}));
    D6R_REQUIRE(samePose(predictedBefore, presented(client.presentedPlayers(at(30ms)), 101)));
    D6R_REQUIRE(client.takePresentationEvents().empty());
    D6R_REQUIRE_EQ(std::size_t{2}, sent.size());

    const auto presentationBeforeReplacement = client.presentationState(at(30ms));
    const auto retainedBeforeReplacement = R::serializeReplicationSnapshot(
            {1, *client.replicatedState().retainedState()});
    const auto movementBeforeReplacement = presented(client.presentedPlayers(at(30ms)), 101);
    R::FullSnapshot regressedReplacement{2, regressedState};
    regressedReplacement.authoritativeProducedAt = 1031;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(regressedReplacement), at(31ms))
                == R::ClientReplicationResult::WaitingForSnapshot);

    D6R_REQUIRE_EQ(R::StateVersion{1}, client.replicatedState().version());
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE_EQ(retainedBeforeReplacement, R::serializeReplicationSnapshot(
            {1, *client.replicatedState().retainedState()}));
    D6R_REQUIRE(samePose(movementBeforeReplacement,
                         presented(client.presentedPlayers(at(30ms)), 101)));
    D6R_REQUIRE(client.takePresentationEvents().empty());
    D6R_REQUIRE(samePresentation(presentationBeforeReplacement,
                                 client.presentationState(at(30ms))));
    D6R_REQUIRE_EQ(std::size_t{2}, sent.size());

    // A valid version with lower production time than the rejected replacement proves that its
    // version and timing watermarks, quality age, movement correction, and prediction were atomic.
    auto recoveredState = baselineState;
    recoveredState.phaseTime++;
    recoveredState.players[0].positionX = 1400;
    R::FullSnapshot recovered{2, recoveredState};
    recovered.authoritativeProducedAt = 1030;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(recovered), at(31ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE_EQ(R::StateVersion{2}, client.replicatedState().version());
    D6R_REQUIRE(client.replicatedState().current());
    D6R_REQUIRE_EQ(std::uint64_t{51}, client.replicatedState().state()->phaseTime);
    D6R_REQUIRE_EQ(std::int64_t{1400}, client.replicatedState().state()->players[0].positionX);
    D6R_REQUIRE(client.takePresentationEvents().empty());
}

D6R_TEST_CASE("REP-048 production no-callback event drain stays bounded while canonical state advances") {
    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::ClientReplicationConnection client({});
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({1, state}))
                == R::ClientReplicationResult::Applied);

    constexpr R::StateVersion SustainedVersions = R::MaxReplicatedEvents + 257;
    for (R::StateVersion version = 2; version <= SustainedVersions; ++version) {
        state.phaseTime++;
        state.players[1].positionX++;
        auto update = publisher.publish(
                state, {{10000 + version, "shot", 101, 102, 50, 1}});
        D6R_REQUIRE(update.has_value());
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update))
                    == R::ClientReplicationResult::Applied);
        // This is the production HeadlessServer no-guestPresentation branch: outcomes are
        // deliberately consumed rather than accumulated when no presentation callback exists.
        (void) client.takePresentationEvents();
        D6R_REQUIRE(client.takePresentationEvents().empty());
        D6R_REQUIRE_EQ(version, client.replicatedState().version());
        D6R_REQUIRE_EQ(state.phaseTime, client.replicatedState().state()->phaseTime);
        D6R_REQUIRE_EQ(state.players[1].positionX,
                       client.replicatedState().state()->players[1].positionX);
    }
    D6R_REQUIRE_EQ(SustainedVersions, publisher.version());
    D6R_REQUIRE(client.replicatedState().current());
}

D6R_TEST_CASE("NRP local presentation bounds int64 extremes without changing canonical state or outcomes") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    auto canonical = activeState();
    canonical.players[0].positionX = minimum;
    canonical.players[0].positionY = maximum;
    canonical.players[0].velocityX = minimum;
    canonical.players[0].velocityY = maximum;
    const auto canonicalBefore = R::serializeReplicationSnapshot({1, canonical});

    N::CanonicalMovementPresentation movement;
    movement.setLocallyControlledPlayers({101});
    D6R_REQUIRE(movement.accept(1, canonical, at(0ms)));
    D6R_REQUIRE(movement.predictLocalMovement(
            {101, maximum, minimum, true, true}, at(1ms)));

    auto corrected = canonical;
    corrected.phaseTime++;
    corrected.players[0].positionX = maximum;
    corrected.players[0].positionY = minimum;
    corrected.players[0].velocityX = maximum;
    corrected.players[0].velocityY = minimum;
    const auto correctedBefore = R::serializeReplicationSnapshot({2, corrected});
    D6R_REQUIRE(movement.accept(2, corrected, at(10ms)));

    const auto poseAt = [&](std::chrono::milliseconds elapsed) {
        const auto poses = movement.sample(at(elapsed));
        const auto found = std::find_if(poses.begin(), poses.end(), [](const auto &pose) {
            return pose.playerId == 101;
        });
        D6R_REQUIRE(found != poses.end());
        return *found;
    };
    const auto start = poseAt(10ms);
    D6R_REQUIRE_EQ(maximum, start.positionX);
    D6R_REQUIRE_EQ(minimum, start.positionY);
    const auto midpoint = poseAt(85ms);
    const auto repeatedMidpoint = poseAt(85ms);
    D6R_REQUIRE_EQ(midpoint.positionX, repeatedMidpoint.positionX);
    D6R_REQUIRE_EQ(midpoint.positionY, repeatedMidpoint.positionY);
    D6R_REQUIRE(midpoint.positionX >= minimum && midpoint.positionX <= maximum);
    D6R_REQUIRE(midpoint.positionY >= minimum && midpoint.positionY <= maximum);
    const auto finished = poseAt(160ms);
    D6R_REQUIRE_EQ(maximum, finished.positionX);
    D6R_REQUIRE_EQ(minimum, finished.positionY);

    D6R_REQUIRE_EQ(canonicalBefore, R::serializeReplicationSnapshot({1, canonical}));
    D6R_REQUIRE_EQ(correctedBefore, R::serializeReplicationSnapshot({2, corrected}));
    D6R_REQUIRE_EQ(maximum, corrected.players[0].velocityX);
    D6R_REQUIRE_EQ(minimum, corrected.players[0].velocityY);
    D6R_REQUIRE(!corrected.round->outcome.noWinner);
    D6R_REQUIRE(corrected.round->outcome.winnerPlayerIds.empty());
    D6R_REQUIRE(!corrected.score.winner.noWinner);
    D6R_REQUIRE(corrected.score.winner.winnerPlayerIds.empty());
}

D6R_TEST_CASE("NRP production client bounds adversarial peer timestamps and local clock extremes") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    const auto maximumTimestamp = static_cast<std::uint64_t>(
            std::chrono::milliseconds::max().count());

    R::FullSnapshot extremeSnapshot{1, activeState()};
    extremeSnapshot.authoritativeProducedAt = std::numeric_limits<std::uint64_t>::max();
    const auto extremePayload = R::serializeReplicationSnapshot(extremeSnapshot);
    const auto decodedExtreme = R::deserializeReplicationFrame(extremePayload);
    D6R_REQUIRE(decodedExtreme && decodedExtreme->snapshot);
    D6R_REQUIRE_EQ(std::numeric_limits<std::uint64_t>::max(),
                   decodedExtreme->snapshot->authoritativeProducedAt);
    R::ClientReplicationConnection unsignedOverflow({}, N::Environment::SameMachine, true);
    D6R_REQUIRE(unsignedOverflow.receive(extremePayload, at(0ms))
                == R::ClientReplicationResult::Reconnecting);
    D6R_REQUIRE_EQ(R::StateVersion{0}, unsignedOverflow.replicatedState().version());

    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection responseOverflow([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(responseOverflow.sampleNetwork(at(0ms)));
    auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(responseOverflow.receive(R::serializeQualityResponse(
            *probe->qualitySequence, maximumTimestamp - 9), at(20ms))
                == R::ClientReplicationResult::Reconnecting);

    sent.clear();
    R::ClientReplicationConnection clampedFuture([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(clampedFuture.sampleNetwork(at(0ms)));
    probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(clampedFuture.receive(R::serializeQualityResponse(
            *probe->qualitySequence, maximumTimestamp - 10), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);
    R::FullSnapshot maximumSnapshot{1, activeState()};
    maximumSnapshot.authoritativeProducedAt = maximumTimestamp;
    D6R_REQUIRE(clampedFuture.receive(R::serializeReplicationSnapshot(maximumSnapshot), at(20ms))
                == R::ClientReplicationResult::Applied);
    D6R_REQUIRE(!clampedFuture.presentationState(at(20ms)).degraded);

    auto state = activeState();
    state.phaseTime++;
    auto maximumUpdate = validUpdate(activeState(), state);
    maximumUpdate.baseline = 1;
    maximumUpdate.version = 2;
    maximumUpdate.authoritativeProducedAt = maximumTimestamp;
    D6R_REQUIRE(clampedFuture.receive(R::serializeReplicationUpdate(maximumUpdate), at(21ms))
                == R::ClientReplicationResult::Reconnecting);
    D6R_REQUIRE_EQ(R::StateVersion{1}, clampedFuture.replicatedState().version());

    sent.clear();
    R::ClientReplicationConnection localSpan([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    const auto nearMinimum = N::TimePoint::min() + 20ms;
    D6R_REQUIRE(localSpan.sampleNetwork(N::TimePoint::min()));
    probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(localSpan.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000),
                                  nearMinimum)
                == R::ClientReplicationResult::NetworkSampled);
    R::FullSnapshot ordinarySnapshot{1, activeState()};
    ordinarySnapshot.authoritativeProducedAt = 1000;
    D6R_REQUIRE(localSpan.receive(R::serializeReplicationSnapshot(ordinarySnapshot),
                                  N::TimePoint::max())
                == R::ClientReplicationResult::Reconnecting);
    D6R_REQUIRE_EQ(R::StateVersion{0}, localSpan.replicatedState().version());
}

D6R_TEST_CASE("NRP unanswered 250 ms probes degrade despite continuously arriving canonical traffic") {
    const auto at = [](std::chrono::milliseconds elapsed) { return N::TimePoint{} + elapsed; };
    std::vector<std::vector<std::uint8_t>> sent;
    R::ClientReplicationConnection client([&](auto payload) {
        sent.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    }, N::Environment::SameMachine, true);
    D6R_REQUIRE(client.sampleNetwork(at(0ms)));
    auto probe = R::deserializeReplicationFrame(sent.back());
    D6R_REQUIRE(probe && probe->qualitySequence);
    D6R_REQUIRE(client.receive(R::serializeQualityResponse(*probe->qualitySequence, 1000), at(20ms))
                == R::ClientReplicationResult::NetworkSampled);

    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    R::FullSnapshot snapshot{1, state};
    snapshot.authoritativeProducedAt = 1020;
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot(snapshot), at(30ms))
                == R::ClientReplicationResult::Applied);

    R::StateVersion version = 2;
    for (auto elapsed = 50ms; elapsed <= 3500ms; elapsed += 50ms) {
        state.phaseTime++;
        auto update = publisher.publish(state);
        D6R_REQUIRE(update.has_value());
        update->authoritativeProducedAt = 990 + static_cast<std::uint64_t>(elapsed.count());
        D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update), at(elapsed))
                    == R::ClientReplicationResult::Applied);
        D6R_REQUIRE(client.sampleNetwork(at(elapsed)));
        ++version;
        if (elapsed == 3450ms) D6R_REQUIRE(!client.presentationState(at(elapsed)).degraded);
    }
    const auto degraded = client.presentationState(at(3500ms));
    D6R_REQUIRE(degraded.degraded);
    D6R_REQUIRE_EQ(std::string("Network connection degraded."), degraded.degradedText);
    D6R_REQUIRE(degraded.canonicalStateCurrent);
    const auto qualityProbeCount = std::count_if(sent.begin(), sent.end(), [](const auto &payload) {
        const auto frame = R::deserializeReplicationFrame(payload);
        return frame && frame->kind == R::ReplicationFrameKind::QualityProbe;
    });
    D6R_REQUIRE_EQ(15, qualityProbeCount);
}

D6R_TEST_CASE("NRP client handoff retains complete non-current context and delivers authoritative events once") {
    std::vector<std::vector<std::uint8_t>> requests;
    R::ClientReplicationConnection client([&](auto payload) {
        requests.push_back(std::move(payload));
        return Duel6::Network::SendResult::Accepted;
    });
    auto state = activeState();
    R::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(state));
    D6R_REQUIRE(client.receive(R::serializeReplicationSnapshot({1, state}))
                == R::ClientReplicationResult::Applied);
    state.phaseTime++;
    state.entities.clear();
    const auto event = R::PresentationEvent{1, "shot", 101, 102, 50, 1};
    auto update = publisher.publish(state, {event});
    D6R_REQUIRE(update.has_value());
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(*update))
                == R::ClientReplicationResult::Applied);
    const auto events = client.takePresentationEvents();
    D6R_REQUIRE_EQ(std::size_t{1}, events.size());
    D6R_REQUIRE_EQ(std::uint64_t{1}, events.front().eventId);
    D6R_REQUIRE(client.takePresentationEvents().empty());

    auto invalid = *update;
    invalid.baseline = 999;
    invalid.version = 1000;
    D6R_REQUIRE(client.receive(R::serializeReplicationUpdate(invalid))
                == R::ClientReplicationResult::WaitingForSnapshot);
    D6R_REQUIRE(!client.replicatedState().current());
    D6R_REQUIRE(client.replicatedState().state() == nullptr);
    D6R_REQUIRE(client.replicatedState().retainedState() != nullptr);
    D6R_REQUIRE_EQ(state.phaseTime, client.replicatedState().retainedState()->phaseTime);
    D6R_REQUIRE(client.replicatedState().retainedState()->entities.empty());
    const auto synchronizing = client.presentationState(N::Clock::now());
    D6R_REQUIRE(synchronizing.resynchronizing);
    D6R_REQUIRE(!synchronizing.canonicalStateCurrent);
    D6R_REQUIRE(synchronizing.retainingLastConfirmedState);
    D6R_REQUIRE_EQ(std::size_t{2}, client.presentedPlayers(N::Clock::now()).size());

    client.transportClosed();
    const auto reconnecting = client.presentationState(N::Clock::now());
    D6R_REQUIRE(reconnecting.reconnecting);
    D6R_REQUIRE(reconnecting.resynchronizing);
    D6R_REQUIRE(!reconnecting.canonicalStateCurrent);
    D6R_REQUIRE(reconnecting.retainingLastConfirmedState);
    D6R_REQUIRE_EQ(std::string("Reconnecting\xE2\x80\xA6"), reconnecting.reconnectingText);
    D6R_REQUIRE(client.replicatedState().retainedState()->entities.empty());
}
