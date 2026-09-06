#include "AuthoritativeMatchSerialization.h"

#include <cstdio>
#include <string_view>

namespace Duel6::Server::Authoritative {
    namespace {
        class Json final {
        public:
            bool append(std::string_view value) {
                if (value.size() > MaxResultBytes || data.size() > MaxResultBytes - value.size()) return false;
                data.append(value.data(), value.size());
                return true;
            }
            bool string(std::string_view value) {
                if (!append("\"")) return false;
                char escaped[7]{};
                for (unsigned char character: value) {
                    switch (character) {
                        case '"': if (!append("\\\"")) return false; break;
                        case '\\': if (!append("\\\\")) return false; break;
                        case '\b': if (!append("\\b")) return false; break;
                        case '\f': if (!append("\\f")) return false; break;
                        case '\n': if (!append("\\n")) return false; break;
                        case '\r': if (!append("\\r")) return false; break;
                        case '\t': if (!append("\\t")) return false; break;
                        default:
                            if (character < 0x20) {
                                std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                                if (!append(escaped)) return false;
                            } else if (!append(std::string_view(reinterpret_cast<const char *>(&character), 1)))
                                return false;
                    }
                }
                return append("\"");
            }
            bool number(std::uint64_t value) { return append(std::to_string(value)); }
            bool signedNumber(std::int64_t value) { return append(std::to_string(value)); }
            bool boolean(bool value) { return append(value ? "true" : "false"); }
            std::string take() { return std::move(data); }
        private:
            std::string data;
        };

        bool key(Json &json, const char *value) { return json.string(value) && json.append(":"); }

        bool statistics(Json &json, const PlayerStatistics &value) {
            return json.append("{")
                   && key(json, "roundsPlayed") && json.number(value.roundsPlayed)
                   && json.append(",") && key(json, "shots") && json.number(value.shots)
                   && json.append(",") && key(json, "hits") && json.number(value.hits)
                   && json.append(",") && key(json, "kills") && json.number(value.kills)
                   && json.append(",") && key(json, "deaths") && json.number(value.deaths)
                   && json.append(",") && key(json, "assists") && json.number(value.assists)
                   && json.append(",") && key(json, "wins") && json.number(value.wins)
                   && json.append(",") && key(json, "penalties") && json.number(value.penalties)
                   && json.append(",") && key(json, "survivalTicks") && json.number(value.survivalTicks)
                   && json.append(",") && key(json, "damage") && json.number(value.damage)
                   && json.append(",") && key(json, "assistedDamage") && json.number(value.assistedDamage)
                   && json.append(",") && key(json, "totalPoints") && json.signedNumber(value.totalPoints())
                   && json.append("}");
        }

        bool identities(Json &json, const std::vector<Identity> &values) {
            if (!json.append("[")) return false;
            for (std::size_t index = 0; index < values.size(); ++index) {
                if ((index && !json.append(",")) || !json.number(values[index])) return false;
            }
            return json.append("]");
        }
    }

    std::optional<std::string> serializeSessionResult(const SessionResult &result) {
        if (result.players.size() > MaxPlayers || result.rounds.size() > 99
            || result.teams.size() > 4 || result.completedRounds != result.rounds.size()) return std::nullopt;
        Json json;
        if (!json.append("{")
            || !key(json, "label") || !json.string(result.label)
            || !json.append(",") || !key(json, "state")
            || !json.string(result.state == ResultState::Completed ? "Completed" : "Interrupted")
            || !json.append(",") || !key(json, "mode") || !json.string(modeName(result.config.mode))
            || !json.append(",") || !key(json, "teamCount") || !json.number(result.config.teamCount)
            || !json.append(",") || !key(json, "friendlyFire") || !json.boolean(result.config.friendlyFire)
            || !json.append(",") || !key(json, "assistance") || !json.boolean(result.config.assistance)
            || !json.append(",") || !key(json, "quickLiquid") || !json.boolean(result.config.quickLiquid)
            || !json.append(",") || !key(json, "burnableTrees") || !json.boolean(result.config.burnableTrees)
            || !json.append(",") || !key(json, "optionalScriptsEnabled")
            || !json.boolean(result.config.optionalScriptsEnabled)
            || !json.append(",") || !key(json, "levelPlan") || !json.string(levelPlanName(result.config.levelPlan))
            || !json.append(",") || !key(json, "roundLimit") || !json.number(result.config.roundLimit)
            || !json.append(",") || !key(json, "seed") || !json.number(result.config.seed)
            || !json.append(",") || !key(json, "completedRounds") || !json.number(result.completedRounds)
            || !json.append(",") || !key(json, "finalWinnerPlayerIds")
            || !identities(json, result.finalWinnerPlayerIds)
            || !json.append(",") || !key(json, "finalWinningTeam")
            || !json.string(teamName(result.finalWinningTeam))
            || !json.append(",") || !key(json, "finalNoWinner") || !json.boolean(result.finalNoWinner)
            || !json.append(",") || !key(json, "rounds") || !json.append("[")) return std::nullopt;
        for (std::size_t index = 0; index < result.rounds.size(); ++index) {
            const auto &round = result.rounds[index];
            if ((index && !json.append(",")) || !json.append("{")
                || !key(json, "roundNumber") || !json.number(round.roundNumber)
                || !json.append(",") || !key(json, "level") || !json.string(round.level)
                || !json.append(",") || !key(json, "orientation")
                || !json.string(round.mirrored ? "Mirrored" : "Normal")
                || !json.append(",") || !key(json, "winnerPlayerIds")
                || !identities(json, round.winnerPlayerIds)
                || !json.append(",") || !key(json, "winningTeam") || !json.string(teamName(round.winningTeam))
                || !json.append(",") || !key(json, "noWinner") || !json.boolean(round.noWinner)
                || !json.append(",") || !key(json, "rosterOrder") || !identities(json, round.rosterOrder)
                || !json.append("}")) return std::nullopt;
        }
        if (!json.append("],") || !key(json, "players") || !json.append("[")) return std::nullopt;
        for (std::size_t index = 0; index < result.players.size(); ++index) {
            const auto &player = result.players[index];
            if ((index && !json.append(",")) || !json.append("{")
                || !key(json, "rank") || !json.number(index + 1)
                || !json.append(",") || !key(json, "playerId") || !json.number(player.playerId)
                || !json.append(",") || !key(json, "participantId") || !json.number(player.participantId)
                || !json.append(",") || !key(json, "displayName") || !json.string(player.displayName)
                || !json.append(",") || !key(json, "team") || !json.string(teamName(player.team))
                || !json.append(",") || !key(json, "departed") || !json.boolean(player.departed)
                || !json.append(",") || !key(json, "rosterOrder") || !json.number(player.rosterOrder)
                || !json.append(",") || !key(json, "cumulative") || !statistics(json, player.statistics)
                || !json.append(",") || !key(json, "rounds") || !json.append("[")) return std::nullopt;
            for (std::size_t round = 0; round < player.rounds.size(); ++round) {
                if ((round && !json.append(",")) || !statistics(json, player.rounds[round])) return std::nullopt;
            }
            if (!json.append("]}")) return std::nullopt;
        }
        if (!json.append("],") || !key(json, "teams") || !json.append("[")) return std::nullopt;
        for (std::size_t index = 0; index < result.teams.size(); ++index) {
            const auto &team = result.teams[index];
            if ((index && !json.append(",")) || !json.append("{")
                || !key(json, "rank") || !json.number(index + 1)
                || !json.append(",") || !key(json, "team") || !json.string(teamName(team.team))
                || !json.append(",") || !key(json, "totalPoints") || !json.signedNumber(team.totalPoints)
                || !json.append(",") || !key(json, "rankedPlayerIds")
                || !identities(json, team.rankedPlayerIds) || !json.append("}")) return std::nullopt;
        }
        if (!json.append("]}")) return std::nullopt;
        std::string serialized = json.take();
        if (serialized.size() > MaxResultBytes) return std::nullopt;
        return serialized;
    }
}
