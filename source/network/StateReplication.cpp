#include "StateReplication.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace Duel6::Network::Replication {
    namespace {
        bool validText(const std::string &value, std::size_t maximum = 64, bool emptyAllowed = false) {
            if ((!emptyAllowed && value.empty()) || value.size() > maximum) return false;
            return std::none_of(value.begin(), value.end(), [](unsigned char character) {
                return character < 0x20 || character == 0x7f;
            });
        }

        template<typename T, typename Id>
        bool uniqueNonzero(const std::vector<T> &values, Id id, std::set<Identity> *identities = nullptr) {
            std::set<Identity> local;
            auto &seen = identities ? *identities : local;
            for (const auto &value: values) {
                const Identity identity = id(value);
                if (identity == 0 || !seen.insert(identity).second) return false;
            }
            return true;
        }

        bool participantEqual(const ParticipantState &left, const ParticipantState &right) {
            return left.participantId == right.participantId && left.host == right.host
                   && left.connection == right.connection && left.ready == right.ready
                   && left.ownedPlayerIds == right.ownedPlayerIds;
        }

        bool playerEqual(const PlayerState &left, const PlayerState &right) {
            return left.playerId == right.playerId && left.ownerParticipantId == right.ownerParticipantId
                   && left.rosterPosition == right.rosterPosition && left.displayName == right.displayName
                   && left.team == right.team && left.lifeState == right.lifeState
                   && left.positionX == right.positionX && left.positionY == right.positionY
                   && left.velocityX == right.velocityX && left.velocityY == right.velocityY
                   && left.facingLeft == right.facingLeft && left.crouching == right.crouching
                   && left.life == right.life && left.air == right.air && left.heldWeapon == right.heldWeapon
                   && left.ammunition == right.ammunition && left.actionMask == right.actionMask
                   && left.activeBonus == right.activeBonus && left.bonusRemaining == right.bonusRemaining
                   && left.invulnerable == right.invulnerable && left.visible == right.visible
                   && left.reloadRemaining == right.reloadRemaining && left.charge == right.charge
                   && left.temporaryMovementRemaining == right.temporaryMovementRemaining;
        }

        bool entityEqual(const WorldEntityState &left, const WorldEntityState &right) {
            return left.entityId == right.entityId && left.kind == right.kind
                   && left.ownerPlayerId == right.ownerPlayerId && left.type == right.type
                   && left.positionX == right.positionX && left.positionY == right.positionY
                   && left.velocityX == right.velocityX && left.velocityY == right.velocityY
                   && left.primaryValue == right.primaryValue && left.secondaryValue == right.secondaryValue
                   && left.active == right.active && left.lifecycle == right.lifecycle;
        }

        template<typename T, typename Id, typename Equal>
        bool identifiedValuesEqual(const std::vector<T> &left, const std::vector<T> &right,
                                   Id id, Equal equal) {
            if (left.size() != right.size()) return false;
            std::map<Identity, const T *> indexed;
            for (const auto &value: right) indexed.emplace(id(value), &value);
            return std::all_of(left.begin(), left.end(), [&](const auto &value) {
                const auto found = indexed.find(id(value));
                return found != indexed.end() && equal(value, *found->second);
            });
        }

        template<typename T, typename Id, typename Equal>
        std::vector<EntityChange<T>> changes(const std::vector<T> &before, const std::vector<T> &after,
                                              Id id, Equal equal) {
            std::map<Identity, T> oldValues;
            std::map<Identity, T> newValues;
            for (const auto &value: before) oldValues.emplace(id(value), value);
            for (const auto &value: after) newValues.emplace(id(value), value);
            std::vector<EntityChange<T>> result;
            for (const auto &[identity, value]: oldValues) {
                if (!newValues.count(identity)) result.push_back({ChangeKind::Remove, identity, std::nullopt});
            }
            for (const auto &[identity, value]: newValues) {
                const auto old = oldValues.find(identity);
                if (old == oldValues.end()) result.push_back({ChangeKind::Create, identity, value});
                else if (!equal(old->second, value)) result.push_back({ChangeKind::Update, identity, value});
            }
            return result;
        }

        template<typename T, typename Id>
        Identity highestIdentity(const std::vector<T> &values, Id id) {
            Identity result = 0;
            for (const auto &value: values) result = std::max(result, id(value));
            return result;
        }

        std::set<Identity> referencedPlayerIdentities(const CanonicalState &state) {
            std::set<Identity> result;
            for (const auto &player: state.players) result.insert(player.playerId);
            for (const auto &row: state.score.players) result.insert(row.playerId);
            result.insert(state.score.ranking.begin(), state.score.ranking.end());
            result.insert(state.score.winner.winnerPlayerIds.begin(), state.score.winner.winnerPlayerIds.end());
            if (state.round) {
                result.insert(state.round->rosterOrder.begin(), state.round->rosterOrder.end());
                result.insert(state.round->outcome.winnerPlayerIds.begin(),
                              state.round->outcome.winnerPlayerIds.end());
            }
            return result;
        }

        std::set<Identity> resultPlayerIdentities(const CanonicalState &state) {
            std::set<Identity> result;
            for (const auto &row: state.score.players) result.insert(row.playerId);
            result.insert(state.score.ranking.begin(), state.score.ranking.end());
            result.insert(state.score.winner.winnerPlayerIds.begin(), state.score.winner.winnerPlayerIds.end());
            if (state.round) {
                result.insert(state.round->rosterOrder.begin(), state.round->rosterOrder.end());
                result.insert(state.round->outcome.winnerPlayerIds.begin(),
                              state.round->outcome.winnerPlayerIds.end());
            }
            return result;
        }

        bool introducesUnseenRetainedResultIdentity(const CanonicalState &before,
                                                     const CanonicalState &after,
                                                     const std::set<Identity> &issued) {
            if (!before.result.available
                || (before.result.state != "Completed" && before.result.state != "Interrupted")) return false;
            const auto resultIdentities = resultPlayerIdentities(after);
            return std::any_of(resultIdentities.begin(), resultIdentities.end(), [&](Identity identity) {
                return !issued.count(identity);
            });
        }

        template<typename T, typename Id>
        bool containsNonMonotonicCreation(const std::vector<T> &before, const std::vector<T> &after,
                                           Identity watermark, Id id) {
            std::set<Identity> existing;
            for (const auto &value: before) existing.insert(id(value));
            return std::any_of(after.begin(), after.end(), [&](const auto &value) {
                const Identity identity = id(value);
                return !existing.count(identity) && identity <= watermark;
            });
        }

        template<typename T, typename Id>
        bool containsReusedCreation(const std::vector<T> &before, const std::vector<T> &after,
                                    const std::set<Identity> &issued, Id id) {
            std::set<Identity> existing;
            for (const auto &value: before) existing.insert(id(value));
            return std::any_of(after.begin(), after.end(), [&](const auto &value) {
                const Identity identity = id(value);
                return !existing.count(identity) && issued.count(identity);
            });
        }

        template<typename T, typename Id>
        bool applyChanges(std::vector<T> &values, const std::vector<EntityChange<T>> &updates,
                           Identity &highestIssued, Id id) {
            std::map<Identity, T> indexed;
            for (const auto &value: values) if (!indexed.emplace(id(value), value).second) return false;
            std::set<Identity> changed;
            for (const auto &change: updates) {
                if (change.identity == 0 || !changed.insert(change.identity).second) return false;
                const auto existing = indexed.find(change.identity);
                if (change.kind == ChangeKind::Create) {
                    if (!change.value || id(*change.value) != change.identity || existing != indexed.end()
                        || change.identity <= highestIssued) return false;
                    indexed.emplace(change.identity, *change.value);
                    highestIssued = change.identity;
                } else if (change.kind == ChangeKind::Update) {
                    if (!change.value || id(*change.value) != change.identity || existing == indexed.end()) return false;
                    existing->second = *change.value;
                } else {
                    if (change.value || existing == indexed.end()) return false;
                    indexed.erase(existing);
                }
            }
            values.clear();
            values.reserve(indexed.size());
            for (auto &[identity, value]: indexed) values.push_back(std::move(value));
            return true;
        }

        template<typename T, typename Id>
        bool applyChanges(std::vector<T> &values, const std::vector<EntityChange<T>> &updates,
                          std::set<Identity> &issued, Id id) {
            std::map<Identity, T> indexed;
            for (const auto &value: values) if (!indexed.emplace(id(value), value).second) return false;
            std::set<Identity> changed;
            for (const auto &change: updates) {
                if (change.identity == 0 || !changed.insert(change.identity).second) return false;
                const auto existing = indexed.find(change.identity);
                if (change.kind == ChangeKind::Create) {
                    if (!change.value || id(*change.value) != change.identity || existing != indexed.end()
                        || issued.count(change.identity)
                        || issued.size() >= MaxReplicatedIdentityHistory) return false;
                    indexed.emplace(change.identity, *change.value);
                    issued.insert(change.identity);
                } else if (change.kind == ChangeKind::Update) {
                    if (!change.value || id(*change.value) != change.identity || existing == indexed.end()) return false;
                    existing->second = *change.value;
                } else {
                    if (change.value || existing == indexed.end()) return false;
                    indexed.erase(existing);
                }
            }
            values.clear();
            values.reserve(indexed.size());
            for (auto &[identity, value]: indexed) values.push_back(std::move(value));
            return true;
        }

        bool validEvent(const PresentationEvent &event) {
            return event.eventId != 0 && validText(event.type);
        }

        bool validConnectionState(ConnectionState value) {
            return value == ConnectionState::Connected || value == ConnectionState::Reconnecting;
        }

        bool validPhase(Phase value) {
            return value == Phase::Lobby || value == Phase::ActiveRound || value == Phase::RoundSummary
                   || value == Phase::FinalSummary || value == Phase::Ended;
        }

        bool validLifeState(LifeState value) {
            return value == LifeState::Alive || value == LifeState::Dead || value == LifeState::Departed;
        }

        bool validEntityKind(EntityKind value) {
            return value >= EntityKind::Shot && value <= EntityKind::Explosion;
        }

        template<typename T>
        bool validChangeKinds(const std::vector<EntityChange<T>> &values) {
            return std::all_of(values.begin(), values.end(), [](const auto &value) {
                return value.kind == ChangeKind::Create || value.kind == ChangeKind::Update
                       || value.kind == ChangeKind::Remove;
            });
        }

        bool sameRound(const std::optional<RoundState> &left, const std::optional<RoundState> &right) {
            return (!left && !right) || (left && right && left->roundId == right->roundId);
        }

        bool emptyOutcome(const RoundOutcomeState &outcome);

        bool validMatchTransition(const CanonicalState &before, const CanonicalState &after,
                                  const std::set<Identity> &issued) {
            if (before.matchId == after.matchId) return true;
            return before.phase == Phase::Lobby && after.phase == Phase::ActiveRound
                   && after.matchId != 0 && !issued.count(after.matchId);
        }

        bool validRoundTransition(const CanonicalState &before, const CanonicalState &after,
                                   const std::set<Identity> &issued) {
            if (sameRound(before.round, after.round)) return true;
            if (before.matchId == after.matchId && before.round && after.round
                && before.round->roundNumber == after.round->roundNumber) return false;
            return !after.round || !issued.count(after.round->roundId);
        }

        bool validFollowingLobbyTransition(const CanonicalState &before,
                                            const CanonicalState &after,
                                            const std::set<Identity> &issuedMatches,
                                            const std::set<Identity> &issuedRounds) {
            if (before.phase != Phase::Lobby || !before.result.available
                || (before.result.state != "Completed" && before.result.state != "Interrupted")) return true;
            if (after.phase == Phase::Lobby) return after.matchId == before.matchId;
            const bool resetPlayerScores = std::all_of(after.score.players.begin(), after.score.players.end(),
                    [](const auto &score) { return score.cumulativePoints == 0; });
            const bool resetTeamScores = std::all_of(after.score.teamTotals.begin(), after.score.teamTotals.end(),
                    [](std::int64_t score) { return score == 0; });
            bool resetTeamRanking = after.score.teamRanking.size() == after.score.teamTotals.size();
            for (std::size_t index = 0; resetTeamRanking && index < after.score.teamRanking.size(); ++index)
                resetTeamRanking = after.score.teamRanking[index] == static_cast<std::uint8_t>(index + 1);
            return after.phase == Phase::ActiveRound && !after.result.available
                    && after.result.sessionOnly && after.result.state.empty() && after.result.serialized.empty()
                    && after.matchId != before.matchId && !issuedMatches.count(after.matchId)
                    && after.round && !sameRound(before.round, after.round)
                    && !issuedRounds.count(after.round->roundId)
                    && after.currentRoundNumber == 1 && after.completedRounds == 0
                    && after.round->roundNumber == 1
                    && after.score.ranking == after.round->rosterOrder
                    && resetPlayerScores && resetTeamScores && resetTeamRanking
                    && emptyOutcome(after.score.winner) && emptyOutcome(after.round->outcome);
        }

        bool hasEntity(const CanonicalState &state, Identity identity) {
            return identity == 0 || std::any_of(state.entities.begin(), state.entities.end(), [identity](const auto &entity) {
                return entity.entityId == identity;
            });
        }

        std::optional<EntityKind> transientKind(const std::string &eventType) {
            if (eventType == "shot-fired") return EntityKind::Projectile;
            if (eventType == "bonus-picked") return EntityKind::BonusPickup;
            if (eventType == "weapon-picked") return EntityKind::WeaponPickup;
            return std::nullopt;
        }

        bool validEventForTransient(EntityKind kind, const std::string &eventType) {
            if (kind == EntityKind::Projectile)
                return eventType == "shot-fired" || eventType == "shot-hit"
                       || eventType == "player-life-changed" || eventType == "player-died"
                       || eventType == "player-killed";
            if (kind == EntityKind::BonusPickup) return eventType == "bonus-picked";
            return kind == EntityKind::WeaponPickup && eventType == "weapon-picked";
        }

        bool validEventReferences(const CanonicalState &before, const CanonicalState &after,
                                   const PresentationEvent &event,
                                   std::map<Identity, EntityKind> &transientIdentities,
                                   Identity highestLiveIdentity) {
            const auto hasPlayer = [&](Identity identity) {
                if (identity == 0) return true;
                const auto present = [identity](const auto &player) { return player.playerId == identity; };
                return std::any_of(before.players.begin(), before.players.end(), present)
                       || std::any_of(after.players.begin(), after.players.end(), present);
            };
            if (!hasPlayer(event.playerId) || !hasPlayer(event.targetPlayerId)) return false;
            if (hasEntity(before, event.entityId) || hasEntity(after, event.entityId)) return true;
            const auto known = transientIdentities.find(event.entityId);
            if (known != transientIdentities.end()) return validEventForTransient(known->second, event.type);
            const auto kind = transientKind(event.type);
            if (!kind || event.entityId == 0 || event.entityId <= highestLiveIdentity
                || transientIdentities.size() >= MaxReplicatedIdentityHistory)
                return false;
            transientIdentities.emplace(event.entityId, *kind);
            return true;
        }

        bool emptyOutcome(const RoundOutcomeState &outcome) {
            return outcome.winnerPlayerIds.empty() && outcome.winningTeam == 0 && !outcome.noWinner;
        }

        bool resolvedOutcome(const RoundOutcomeState &outcome) {
            return outcome.noWinner
                   || !outcome.winnerPlayerIds.empty()
                   || outcome.winningTeam != 0;
        }

        bool sameOutcome(const RoundOutcomeState &left, const RoundOutcomeState &right) {
            return left.winnerPlayerIds == right.winnerPlayerIds
                   && left.winningTeam == right.winningTeam
                   && left.noWinner == right.noWinner;
        }

        bool sameCompletedRound(const std::optional<RoundState> &left,
                                const std::optional<RoundState> &right) {
            return left.has_value() == right.has_value()
                   && (!left || (left->roundId == right->roundId
                                 && left->roundNumber == right->roundNumber
                                 && left->level == right->level
                                 && left->mirrored == right->mirrored
                                 && left->rosterOrder == right->rosterOrder
                                 && sameOutcome(left->outcome, right->outcome)));
        }

        bool sameScoreRow(const ScoreRowState &left, const ScoreRowState &right) {
            return left.playerId == right.playerId && left.roundPoints == right.roundPoints
                   && left.cumulativePoints == right.cumulativePoints && left.shots == right.shots
                   && left.hits == right.hits && left.kills == right.kills && left.deaths == right.deaths
                   && left.assists == right.assists && left.wins == right.wins
                   && left.penalties == right.penalties && left.survivalTicks == right.survivalTicks
                   && left.damage == right.damage && left.assistedDamage == right.assistedDamage;
        }

        bool sameScore(const ScoreState &left, const ScoreState &right) {
            return left.players.size() == right.players.size()
                   && std::equal(left.players.begin(), left.players.end(), right.players.begin(), sameScoreRow)
                   && left.ranking == right.ranking && left.teamTotals == right.teamTotals
                   && left.teamRanking == right.teamRanking && sameOutcome(left.winner, right.winner);
        }

        struct ParsedJsonValue {
            enum class Kind { Object, Array, String, Number, Boolean, Null } kind = Kind::Null;
            std::string token;
            bool boolean = false;
            std::size_t start = 0;
            std::size_t end = 0;
            std::vector<std::pair<std::string, ParsedJsonValue>> object;
            std::vector<ParsedJsonValue> array;
        };

        constexpr std::size_t MaxCanonicalResultRounds = 99;
        constexpr std::size_t MaxCanonicalResultTeams = 4;
        // The canonical root is the widest object at 19 members; statistics objects have 12.
        constexpr std::size_t MaxCanonicalResultObjectMembers = 19;
        constexpr std::size_t StatisticsJsonValues = 1 + 12;
        constexpr std::size_t RoundResultJsonValues = 1 + 5
                                                      + 2 * (1 + MaxReplicatedPlayers);
        constexpr std::size_t PlayerResultJsonValues = 1 + 7 + StatisticsJsonValues
                                                       + 1 + MaxCanonicalResultRounds
                                                             * StatisticsJsonValues;
        constexpr std::size_t TeamResultJsonValues = 1 + 3 + 1 + MaxReplicatedPlayers;
        constexpr std::size_t MaxCanonicalResultJsonValues = 1 + 13
                                                              + (1 + MaxReplicatedPlayers) + 2
                                                              + (1 + MaxCanonicalResultRounds
                                                                     * RoundResultJsonValues)
                                                              + (1 + MaxReplicatedPlayers
                                                                     * PlayerResultJsonValues)
                                                              + (1 + MaxCanonicalResultTeams
                                                                     * TeamResultJsonValues);

        class CanonicalJsonParser final {
        public:
            explicit CanonicalJsonParser(const std::string &source) : source(source) {}

            bool parse(ParsedJsonValue &value) {
                return parseValue(value, 0) && position == source.size();
            }

        private:
            const std::string &source;
            std::size_t position = 0;
            std::size_t values = 0;

            bool parseValue(ParsedJsonValue &value, std::size_t depth) {
                if (position >= source.size() || depth > 16
                    || ++values > MaxCanonicalResultJsonValues) return false;
                value.start = position;
                const char first = source[position];
                bool parsed = false;
                if (first == '{') parsed = parseObject(value, depth);
                else if (first == '[') parsed = parseArray(value, depth);
                else if (first == '"') {
                    value.kind = ParsedJsonValue::Kind::String;
                    parsed = parseString(value.token);
                } else if (first == '-' || (first >= '0' && first <= '9')) {
                    value.kind = ParsedJsonValue::Kind::Number;
                    parsed = parseNumber(value.token);
                } else if (source.compare(position, 4, "true") == 0) {
                    value.kind = ParsedJsonValue::Kind::Boolean; value.boolean = true; position += 4; parsed = true;
                } else if (source.compare(position, 5, "false") == 0) {
                    value.kind = ParsedJsonValue::Kind::Boolean; value.boolean = false; position += 5; parsed = true;
                } else if (source.compare(position, 4, "null") == 0) {
                    value.kind = ParsedJsonValue::Kind::Null; position += 4; parsed = true;
                }
                value.end = position;
                return parsed;
            }

            bool parseString(std::string &token) {
                const std::size_t start = position++;
                while (position < source.size()) {
                    const unsigned char character = static_cast<unsigned char>(source[position++]);
                    if (character == '"') {
                        token = source.substr(start, position - start);
                        return true;
                    }
                    if (character < 0x20) return false;
                    if (character != '\\') continue;
                    if (position >= source.size()) return false;
                    const char escaped = source[position++];
                    if (escaped == '"' || escaped == '\\' || escaped == '/' || escaped == 'b'
                        || escaped == 'f' || escaped == 'n' || escaped == 'r' || escaped == 't') continue;
                    if (escaped != 'u' || position + 4 > source.size()) return false;
                    for (std::size_t digit = 0; digit < 4; ++digit) {
                        const char hexadecimal = source[position++];
                        if (!((hexadecimal >= '0' && hexadecimal <= '9')
                              || (hexadecimal >= 'a' && hexadecimal <= 'f')
                              || (hexadecimal >= 'A' && hexadecimal <= 'F'))) return false;
                    }
                }
                return false;
            }

            bool parseNumber(std::string &token) {
                const std::size_t start = position;
                if (source[position] == '-' && (++position == source.size())) return false;
                if (source[position] == '0') ++position;
                else {
                    if (source[position] < '1' || source[position] > '9') return false;
                    do { ++position; }
                    while (position < source.size() && source[position] >= '0' && source[position] <= '9');
                }
                token = source.substr(start, position - start);
                return true;
            }

            bool parseObject(ParsedJsonValue &value, std::size_t depth) {
                value.kind = ParsedJsonValue::Kind::Object;
                ++position;
                if (position < source.size() && source[position] == '}') { ++position; return true; }
                while (position < source.size()) {
                    if (value.object.size() >= MaxCanonicalResultObjectMembers) return false;
                    std::string key;
                    if (source[position] != '"' || !parseString(key) || position >= source.size()
                        || source[position++] != ':') return false;
                    if (std::any_of(value.object.begin(), value.object.end(), [&](const auto &member) {
                        return member.first == key;
                    })) return false;
                    ParsedJsonValue member;
                    if (!parseValue(member, depth + 1)) return false;
                    value.object.emplace_back(std::move(key), std::move(member));
                    if (position >= source.size()) return false;
                    if (source[position] == '}') { ++position; return true; }
                    if (source[position++] != ',') return false;
                }
                return false;
            }

            bool parseArray(ParsedJsonValue &value, std::size_t depth) {
                value.kind = ParsedJsonValue::Kind::Array;
                ++position;
                if (position < source.size() && source[position] == ']') { ++position; return true; }
                while (position < source.size()) {
                    ParsedJsonValue member;
                    if (!parseValue(member, depth + 1)) return false;
                    value.array.push_back(std::move(member));
                    if (position >= source.size()) return false;
                    if (source[position] == ']') { ++position; return true; }
                    if (source[position++] != ',') return false;
                }
                return false;
            }
        };

        const ParsedJsonValue *member(const ParsedJsonValue &object, std::string_view key) {
            const auto found = std::find_if(object.object.begin(), object.object.end(), [&](const auto &entry) {
                return entry.first == key;
            });
            return found == object.object.end() ? nullptr : &found->second;
        }

        std::optional<Identity> positiveIdentity(const ParsedJsonValue *value) {
            if (!value || value->kind != ParsedJsonValue::Kind::Number || value->token.empty()
                || value->token.front() == '-' || (value->token.size() > 1 && value->token.front() == '0'))
                return std::nullopt;
            Identity result = 0;
            for (const char digit: value->token) {
                if (digit < '0' || digit > '9') return std::nullopt;
                const Identity next = static_cast<Identity>(digit - '0');
                if (result > (std::numeric_limits<Identity>::max() - next) / 10u) return std::nullopt;
                result = result * 10u + next;
            }
            return result == 0 ? std::nullopt : std::optional<Identity>(result);
        }

        struct CanonicalResultRowLabel {
            Identity playerId = 0;
            Identity participantId = 0;
            bool departed = false;
            std::size_t labelStart = 0;
            std::size_t labelEnd = 0;
        };

        struct CanonicalResultLabels {
            std::string normalized;
            std::vector<CanonicalResultRowLabel> rows;
        };

        std::optional<CanonicalResultLabels> canonicalResultLabels(const std::string &serialized) {
            ParsedJsonValue root;
            if (!CanonicalJsonParser(serialized).parse(root) || root.kind != ParsedJsonValue::Kind::Object) return std::nullopt;
            const auto *state = member(root, "\"state\"");
            const auto *players = member(root, "\"players\"");
            if (!state || state->kind != ParsedJsonValue::Kind::String
                || (state->token != "\"Completed\"" && state->token != "\"Interrupted\"")
                || !players || players->kind != ParsedJsonValue::Kind::Array) return std::nullopt;
            CanonicalResultLabels result;
            std::set<Identity> playerIds;
            for (const auto &row: players->array) {
                if (row.kind != ParsedJsonValue::Kind::Object) return std::nullopt;
                const auto playerId = positiveIdentity(member(row, "\"playerId\""));
                const auto participantId = positiveIdentity(member(row, "\"participantId\""));
                const auto *departed = member(row, "\"departed\"");
                if (!playerId || !participantId || !departed
                    || departed->kind != ParsedJsonValue::Kind::Boolean || !playerIds.insert(*playerId).second)
                    return std::nullopt;
                result.rows.push_back({*playerId, *participantId, departed->boolean,
                                       departed->start, departed->end});
            }
            std::size_t copied = 0;
            for (const auto &row: result.rows) {
                result.normalized.append(serialized, copied, row.labelStart - copied);
                result.normalized.push_back('?');
                copied = row.labelEnd;
            }
            result.normalized.append(serialized, copied, serialized.size() - copied);
            return result;
        }

        bool consistentCanonicalResultDepartures(const CanonicalState &state) noexcept {
            try {
                const auto result = canonicalResultLabels(state.result.serialized);
                if (!result) return true;
                for (const auto &row: result->rows) {
                    const auto player = std::find_if(state.players.begin(), state.players.end(),
                            [&](const auto &value) { return value.playerId == row.playerId; });
                    if (player != state.players.end()
                        && row.departed != (player->lifeState == LifeState::Departed)) return false;
                }
                return true;
            } catch (...) { return false; }
        }

        std::optional<std::set<Identity>> departedPlayerTransitions(
                const CanonicalState &before, const CanonicalState &after) {
            if (before.players.size() != after.players.size()) return std::nullopt;
            std::set<Identity> changed;
            for (const auto &prior: before.players) {
                const auto current = std::find_if(after.players.begin(), after.players.end(), [&](const auto &player) {
                    return player.playerId == prior.playerId;
                });
                if (current == after.players.end()) return std::nullopt;
                auto expected = prior;
                if (prior.lifeState != current->lifeState) {
                    if (current->lifeState != LifeState::Departed) return std::nullopt;
                    expected.lifeState = LifeState::Departed;
                    changed.insert(prior.playerId);
                }
                if (!playerEqual(expected, *current)) return std::nullopt;
            }
            return changed;
        }

        bool validDepartedResultTransition(const CanonicalState &before, const CanonicalState &after) noexcept {
            try {
                const auto prior = canonicalResultLabels(before.result.serialized);
                const auto current = canonicalResultLabels(after.result.serialized);
                const auto playerTransitions = departedPlayerTransitions(before, after);
                if (!prior || !current || !playerTransitions || prior->normalized != current->normalized
                    || prior->rows.size() != current->rows.size() || playerTransitions->empty()) return false;
                std::set<Identity> resultTransitions;
                for (std::size_t index = 0; index < prior->rows.size(); ++index) {
                    const auto &left = prior->rows[index];
                    const auto &right = current->rows[index];
                    if (left.playerId != right.playerId || left.participantId != right.participantId
                        || (left.departed && !right.departed)) return false;
                    if (left.departed == right.departed) continue;
                    const auto player = std::find_if(after.players.begin(), after.players.end(), [&](const auto &value) {
                        return value.playerId == right.playerId;
                    });
                    if (player == after.players.end() || player->ownerParticipantId != right.participantId
                        || player->lifeState != LifeState::Departed) return false;
                    resultTransitions.insert(right.playerId);
                }
                return !resultTransitions.empty() && resultTransitions == *playerTransitions;
            } catch (...) { return false; }
        }

        bool changesRetainedResultPlayerDeparture(
                const CanonicalState &before, const CanonicalState &after) noexcept {
            try {
                const auto result = canonicalResultLabels(before.result.serialized);
                if (!result) return false;
                for (const auto &row: result->rows) {
                    const auto prior = std::find_if(before.players.begin(), before.players.end(), [&](const auto &player) {
                        return player.playerId == row.playerId;
                    });
                    const auto current = std::find_if(after.players.begin(), after.players.end(), [&](const auto &player) {
                        return player.playerId == row.playerId;
                    });
                    if (prior != before.players.end() && current != after.players.end()
                        && (prior->lifeState == LifeState::Departed)
                           != (current->lifeState == LifeState::Departed)) return true;
                }
                return false;
            } catch (...) { return true; }
        }

        bool sameCanonicalState(const CanonicalState &left, const CanonicalState &right) {
            const auto sameSettings = [](const MatchSettingsState &a, const MatchSettingsState &b) {
                return a.mode == b.mode && a.teamCount == b.teamCount && a.friendlyFire == b.friendlyFire
                       && a.levelPlan == b.levelPlan && a.fixedLevel == b.fixedLevel && a.levels == b.levels
                       && a.roundLimit == b.roundLimit && a.assistance == b.assistance
                       && a.quickLiquid == b.quickLiquid && a.burnableTrees == b.burnableTrees;
            };
            const auto sameRoundState = [](const std::optional<RoundState> &a,
                                           const std::optional<RoundState> &b) {
                return a.has_value() == b.has_value()
                       && (!a || (a->roundId == b->roundId && a->roundNumber == b->roundNumber
                                  && a->level == b->level && a->mirrored == b->mirrored
                                  && a->rosterOrder == b->rosterOrder
                                  && sameOutcome(a->outcome, b->outcome)));
            };
            const auto sameEffect = [](const ContinuingEffectState &a, const ContinuingEffectState &b) {
                return a.effectId == b.effectId && a.type == b.type && a.playerId == b.playerId
                       && a.entityId == b.entityId && a.remaining == b.remaining;
            };
            return left.sessionId == right.sessionId && left.matchId == right.matchId
                   && left.hostParticipantId == right.hostParticipantId && left.phase == right.phase
                   && left.currentRoundNumber == right.currentRoundNumber
                   && left.completedRounds == right.completedRounds && left.phaseTime == right.phaseTime
                   && left.roundEndCountdown == right.roundEndCountdown
                   && identifiedValuesEqual(left.participants, right.participants,
                                            [](const auto &value) { return value.participantId; },
                                            participantEqual)
                   && sameSettings(left.settings, right.settings) && sameRoundState(left.round, right.round)
                   && identifiedValuesEqual(left.players, right.players,
                                            [](const auto &value) { return value.playerId; }, playerEqual)
                   && identifiedValuesEqual(left.entities, right.entities,
                                            [](const auto &value) { return value.entityId; }, entityEqual)
                   && sameScore(left.score, right.score)
                   && left.messages.status == right.messages.status
                   && left.messages.events == right.messages.events
                   && left.messages.currentPlayerIndicators == right.messages.currentPlayerIndicators
                   && left.messages.roundProgress == right.messages.roundProgress
                   && left.messages.scoreSummaryVisible == right.messages.scoreSummaryVisible
                   && identifiedValuesEqual(left.effects, right.effects,
                                            [](const auto &value) { return value.effectId; }, sameEffect)
                   && left.result.available == right.result.available
                   && left.result.sessionOnly == right.result.sessionOnly
                   && left.result.state == right.result.state
                   && left.result.serialized == right.result.serialized;
        }

        bool altersEstablishedResult(const CanonicalState &before, const CanonicalState &after) {
            if (before.phase == Phase::Lobby && before.result.available
                && after.phase == Phase::FinalSummary && before.matchId == after.matchId) return true;
            const bool remainsPresented = (before.phase == Phase::FinalSummary || before.phase == Phase::Lobby)
                    && (after.phase == Phase::FinalSummary || after.phase == Phase::Lobby);
            if (!remainsPresented || !before.result.available
                || (before.result.state != "Completed" && before.result.state != "Interrupted")) return false;
            if (before.currentRoundNumber != after.currentRoundNumber
                    || before.completedRounds != after.completedRounds
                    || !sameCompletedRound(before.round, after.round)
                    || !sameScore(before.score, after.score)
                    || before.result.available != after.result.available
                    || before.result.sessionOnly != after.result.sessionOnly
                    || before.result.state != after.result.state) return true;
            if (before.result.serialized == after.result.serialized)
                return changesRetainedResultPlayerDeparture(before, after);
            return !validDepartedResultTransition(before, after);
        }

        bool validResolvedOutcome(const RoundOutcomeState &outcome, const CanonicalState &state) {
            if (!resolvedOutcome(outcome)) return false;
            if (outcome.noWinner) return true;
            if (outcome.winnerPlayerIds.empty()) return false;
            const bool retainedLobbyResult = state.phase == Phase::Lobby && state.result.available;
            const std::size_t teamCount = retainedLobbyResult
                                          ? state.score.teamTotals.size() : state.settings.teamCount;
            if (teamCount == 0) return outcome.winningTeam == 0;
            if (outcome.winningTeam == 0) return false;
            for (Identity identity: outcome.winnerPlayerIds) {
                if (retainedLobbyResult) {
                    if (std::none_of(state.score.players.begin(), state.score.players.end(),
                            [identity](const auto &value) { return value.playerId == identity; })) return false;
                    continue;
                }
                const auto player = std::find_if(state.players.begin(), state.players.end(), [&](const auto &value) {
                    return value.playerId == identity;
                });
                if (player == state.players.end() || player->team != outcome.winningTeam) return false;
            }
            return true;
        }

        bool withinPayloadLimit(const CanonicalState &state, std::size_t eventCount = 0,
                                std::size_t eventTextBytes = 0) {
            std::size_t bytes = 512;
            const auto add = [&bytes](std::size_t value) {
                if (value > MaxReplicatedResultBytes || bytes > MaxReplicatedResultBytes - value) return false;
                bytes += value;
                return true;
            };
            if (!add(state.participants.size() * 192) || !add(state.players.size() * 512)
                || !add(state.entities.size() * 256) || !add(state.score.players.size() * 192)
                || !add(state.effects.size() * 160) || !add(eventCount * 160)
                || !add(eventTextBytes) || !add(state.result.serialized.size())) return false;
            for (const auto &participant: state.participants) if (!add(participant.ownedPlayerIds.size() * 8)) return false;
            for (const auto &player: state.players)
                if (!add(player.displayName.size() + player.heldWeapon.size() + player.activeBonus.size())) return false;
            for (const auto &entity: state.entities)
                if (!add(entity.type.size() + entity.lifecycle.size())) return false;
            for (const auto &level: state.settings.levels) if (!add(level.size())) return false;
            for (const auto &message: state.messages.events) if (!add(message.size())) return false;
            return add(state.messages.status.size() + state.result.state.size());
        }
    }

    Identity StableIdentitySource::issue(IdentityCategory category) {
        constexpr Identity CounterMask = (Identity{1} << 56u) - 1u;
        Identity &candidate = next[category];
        if (candidate == 0) candidate = 1;
        if (candidate > CounterMask) return 0;
        const Identity prefix = (static_cast<Identity>(category) + 1u) << 56u;
        return prefix | candidate++;
    }

    bool StableIdentitySource::wasIssued(IdentityCategory category, Identity identity) const noexcept {
        constexpr Identity CounterMask = (Identity{1} << 56u) - 1u;
        const auto found = next.find(category);
        const Identity prefix = (static_cast<Identity>(category) + 1u) << 56u;
        const Identity value = identity & CounterMask;
        return found != next.end() && (identity & ~CounterMask) == prefix
               && value != 0 && value < found->second;
    }

    bool validateCanonicalState(const CanonicalState &state) noexcept {
        try {
            if (state.sessionId == 0 || state.participants.empty() || !validPhase(state.phase)
                || state.participants.size() > MaxReplicatedParticipants
                || state.players.size() > MaxReplicatedPlayers
                || state.entities.size() > MaxReplicatedEntities
                || state.messages.events.size() > MaxReplicatedMessages
                || state.effects.size() > MaxReplicatedEvents
                || state.result.serialized.size() > MaxReplicatedResultBytes
                || !validText(state.messages.status, 256, true)
                || !validText(state.result.state, 64, true)) return false;
            if (!withinPayloadLimit(state)) return false;
            if (state.phase == Phase::Lobby) {
                if (state.result.available != (state.matchId != 0)) return false;
            } else if (state.matchId == 0) return false;
            if (state.settings.teamCount > MaxReplicatedPlayers) return false;
            std::set<Identity> participantIds;
            if (!uniqueNonzero(state.participants, [](const auto &value) { return value.participantId; },
                               &participantIds)) return false;
            std::size_t hosts = 0;
            std::set<Identity> ownedPlayerIds;
            for (const auto &participant: state.participants) {
                if (!validConnectionState(participant.connection)
                    || participant.ownedPlayerIds.size() > MaxReplicatedPlayers) return false;
                if (participant.host) { ++hosts; if (participant.participantId != state.hostParticipantId) return false; }
                if (!uniqueNonzero(participant.ownedPlayerIds, [](Identity value) { return value; })) return false;
                for (Identity player: participant.ownedPlayerIds)
                    if (!ownedPlayerIds.insert(player).second) return false;
            }
            if (hosts != 1 || !participantIds.count(state.hostParticipantId)) return false;
            std::set<Identity> playerIds;
            std::set<std::uint8_t> rosterPositions;
            if (!uniqueNonzero(state.players, [](const auto &value) { return value.playerId; }, &playerIds)) return false;
            for (const auto &player: state.players) {
                if (!participantIds.count(player.ownerParticipantId) || !rosterPositions.insert(player.rosterPosition).second
                    || !validText(player.displayName) || !validText(player.heldWeapon, 64, true)
                    || !validText(player.activeBonus, 64, true) || !validLifeState(player.lifeState)
                    || player.life < 0 || player.team > state.settings.teamCount
                    || (state.phase != Phase::Lobby && state.settings.teamCount != 0 && player.team == 0)) return false;
                const auto owner = std::find_if(state.participants.begin(), state.participants.end(), [&](const auto &value) {
                    return value.participantId == player.ownerParticipantId;
                });
                if (owner == state.participants.end()
                    || std::count(owner->ownedPlayerIds.begin(), owner->ownedPlayerIds.end(), player.playerId) != 1) return false;
            }
            for (const auto &participant: state.participants) {
                for (Identity player: participant.ownedPlayerIds) if (!playerIds.count(player)) return false;
            }
            if (!uniqueNonzero(state.entities, [](const auto &value) { return value.entityId; })) return false;
            for (const auto &entity: state.entities) {
                if (!validEntityKind(entity.kind)
                    || (entity.ownerPlayerId && !playerIds.count(entity.ownerPlayerId)) || !validText(entity.type)
                    || !validText(entity.lifecycle, 64, true)) return false;
            }
            if (state.round) {
                if (state.round->roundId == 0 || state.round->roundNumber == 0 || !validText(state.round->level, 240)
                    || state.round->roundNumber != state.currentRoundNumber
                    || !uniqueNonzero(state.round->rosterOrder, [](Identity value) { return value; })
                    || !uniqueNonzero(state.round->outcome.winnerPlayerIds,
                                      [](Identity value) { return value; })) return false;
                if (!(state.phase == Phase::Lobby && state.result.available)) {
                    for (Identity player: state.round->rosterOrder) if (!playerIds.count(player)) return false;
                    for (Identity player: state.round->outcome.winnerPlayerIds)
                        if (!playerIds.count(player)) return false;
                }
            } else if (state.phase == Phase::ActiveRound || state.phase == Phase::RoundSummary) return false;
            if (!validText(state.settings.mode) || !validText(state.settings.levelPlan)
                || !validText(state.settings.fixedLevel, 240, true)
                || state.settings.levels.size() > 256) return false;
            for (const auto &level: state.settings.levels) if (!validText(level, 240)) return false;
            if (!uniqueNonzero(state.score.players, [](const auto &value) { return value.playerId; })) return false;
            const bool retainedLobbyResult = state.phase == Phase::Lobby && state.result.available;
            for (const auto &row: state.score.players)
                if (!retainedLobbyResult && !playerIds.count(row.playerId)) return false;
            if (!uniqueNonzero(state.score.ranking, [](Identity value) { return value; })) return false;
            for (Identity player: state.score.ranking)
                if (!retainedLobbyResult && !playerIds.count(player)) return false;
            if (state.score.players.size() > MaxReplicatedPlayers
                || state.score.teamTotals.size() > MaxReplicatedPlayers
                || state.score.teamRanking.size() != state.score.teamTotals.size()) return false;
            if (!state.score.players.empty() && state.score.ranking.size() != state.score.players.size()) return false;
            std::set<Identity> scorePlayerIds;
            for (const auto &row: state.score.players) scorePlayerIds.insert(row.playerId);
            if (state.phase != Phase::Lobby) {
                if (state.score.players.size() != state.players.size()
                    || state.score.ranking.size() != state.players.size()) return false;
                if (scorePlayerIds != playerIds) return false;
                std::set<Identity> rankedPlayerIds(state.score.ranking.begin(), state.score.ranking.end());
                if (rankedPlayerIds != playerIds) return false;
                for (std::size_t index = 0; index < state.score.players.size(); ++index)
                    if (state.score.players[index].playerId != state.score.ranking[index]) return false;
            } else if (retainedLobbyResult) {
                if (state.score.players.empty() || state.score.ranking.size() != state.score.players.size())
                    return false;
                const std::set<Identity> rankedPlayerIds(state.score.ranking.begin(), state.score.ranking.end());
                if (rankedPlayerIds != scorePlayerIds) return false;
                for (std::size_t index = 0; index < state.score.players.size(); ++index)
                    if (state.score.players[index].playerId != state.score.ranking[index]) return false;
                if (state.round) {
                    for (Identity player: state.round->rosterOrder) if (!scorePlayerIds.count(player)) return false;
                    for (Identity player: state.round->outcome.winnerPlayerIds)
                        if (!scorePlayerIds.count(player)) return false;
                }
            }
            if (state.round && state.phase != Phase::Lobby) {
                std::set<Identity> roundPlayers(state.round->rosterOrder.begin(), state.round->rosterOrder.end());
                for (const auto &player: state.players)
                    if (player.lifeState != LifeState::Departed && !roundPlayers.count(player.playerId)) return false;
                std::uint8_t priorPosition = 0;
                bool first = true;
                for (Identity identity: state.round->rosterOrder) {
                    const auto player = std::find_if(state.players.begin(), state.players.end(), [&](const auto &value) {
                        return value.playerId == identity;
                    });
                    if (player == state.players.end() || (!first && player->rosterPosition <= priorPosition)) return false;
                    first = false; priorPosition = player->rosterPosition;
                }
            }
            std::set<std::uint8_t> rankedTeams;
            for (std::uint8_t team: state.score.teamRanking)
                if (team == 0 || team > state.score.teamTotals.size() || !rankedTeams.insert(team).second) return false;
            if (!uniqueNonzero(state.score.winner.winnerPlayerIds, [](Identity value) { return value; })) return false;
            for (Identity player: state.score.winner.winnerPlayerIds)
                if (!(retainedLobbyResult ? scorePlayerIds : playerIds).count(player)) return false;
            const std::size_t scoreTeamCount = retainedLobbyResult
                                               ? state.score.teamTotals.size() : state.settings.teamCount;
            if (state.score.winner.winningTeam > scoreTeamCount
                || (state.score.winner.noWinner && (!state.score.winner.winnerPlayerIds.empty()
                                                    || state.score.winner.winningTeam != 0))) return false;
            if (state.round && (state.round->outcome.winningTeam > scoreTeamCount
                || (state.round->outcome.noWinner && (!state.round->outcome.winnerPlayerIds.empty()
                                                       || state.round->outcome.winningTeam != 0)))) return false;
            if (scoreTeamCount == 0) {
                if (!state.score.teamTotals.empty() || !state.score.teamRanking.empty()
                    || state.score.winner.winningTeam != 0
                    || (state.round && state.round->outcome.winningTeam != 0)) return false;
            } else if (state.score.teamTotals.size() != scoreTeamCount
                       || state.score.teamRanking.size() != scoreTeamCount) return false;
            if (state.phase == Phase::Lobby) {
                if (!state.entities.empty() || !state.effects.empty() || state.roundEndCountdown != 0) return false;
                if (!state.result.available) {
                    if (state.currentRoundNumber != 0 || state.completedRounds != 0 || state.round
                        || !state.score.players.empty() || !emptyOutcome(state.score.winner)) return false;
                } else if (state.currentRoundNumber != state.completedRounds
                           || (state.completedRounds != 0) != state.round.has_value()
                           || !validResolvedOutcome(state.score.winner, state)
                           || (state.result.state != "Completed" && state.result.state != "Interrupted")
                           || (state.result.state == "Completed"
                               && (!state.round || !sameOutcome(state.round->outcome, state.score.winner)))
                           || (state.result.state == "Interrupted" && !state.score.winner.noWinner)) return false;
            } else {
                if (state.currentRoundNumber == 0 || state.completedRounds > state.currentRoundNumber) return false;
                if (state.phase == Phase::ActiveRound) {
                    if (!state.round || state.completedRounds + 1 != state.currentRoundNumber
                        || !emptyOutcome(state.round->outcome) || !emptyOutcome(state.score.winner)
                        || state.result.available) return false;
                } else if (state.phase == Phase::RoundSummary) {
                    if (!state.round || state.completedRounds + 1 != state.currentRoundNumber
                        || !validResolvedOutcome(state.round->outcome, state)
                        || !sameOutcome(state.round->outcome, state.score.winner)
                        || state.result.available) return false;
                } else if (state.phase == Phase::FinalSummary) {
                    if (!state.result.available || !validResolvedOutcome(state.score.winner, state)
                        || state.result.state != "Completed"
                        || (state.round && !sameOutcome(state.round->outcome, state.score.winner))) return false;
                }
            }
            for (const auto &message: state.messages.events) if (!validText(message, 256)) return false;
            for (Identity player: state.messages.currentPlayerIndicators) if (!playerIds.count(player)) return false;
            if (state.messages.currentPlayerIndicators.size() > MaxReplicatedPlayers
                || !uniqueNonzero(state.messages.currentPlayerIndicators, [](Identity value) { return value; })) return false;
            if (!uniqueNonzero(state.effects, [](const auto &value) { return value.effectId; })) return false;
            for (const auto &effect: state.effects) {
                if (!validText(effect.type) || effect.remaining < 0
                    || (effect.playerId && !playerIds.count(effect.playerId))
                    || (effect.entityId && std::none_of(state.entities.begin(), state.entities.end(), [&](const auto &entity) {
                        return entity.entityId == effect.entityId;
                    }))) return false;
            }
            if (state.result.available != !state.result.serialized.empty()) return false;
            if (state.result.available && !state.result.sessionOnly) return false;
            if ((state.phase == Phase::FinalSummary && !state.result.available)
                || (state.phase != Phase::FinalSummary && state.phase != Phase::Lobby
                    && state.result.available)) return false;
            return true;
        } catch (...) { return false; }
    }

    bool AuthoritativeStateReplicator::initialize(CanonicalState state) {
        if (current || !validateCanonicalState(state)) return false;
        for (const auto &participant: state.participants)
            issuedParticipantIdentities.insert(participant.participantId);
        issuedPlayerIdentities = referencedPlayerIdentities(state);
        if (state.matchId) issuedMatchIdentities.insert(state.matchId);
        if (state.round) issuedRoundIdentities.insert(state.round->roundId);
        highestEntityIdentity = highestIdentity(state.entities, [](const auto &value) { return value.entityId; });
        current = std::move(state);
        currentVersion = 1;
        return true;
    }

    std::optional<IncrementalUpdate> AuthoritativeStateReplicator::publish(
            CanonicalState state, std::vector<PresentationEvent> events) {
        const bool roundChanged = current && !sameRound(current->round, state.round);
        if (roundChanged) {
            std::set<Identity> priorEntities;
            for (const auto &entity: current->entities) priorEntities.insert(entity.entityId);
            for (const auto &entity: state.entities) if (priorEntities.count(entity.entityId)) return std::nullopt;
        }
        if (!current || currentVersion == std::numeric_limits<StateVersion>::max()
            || !validateCanonicalState(state) || state.sessionId != current->sessionId
            || (current->matchId != 0 && state.matchId == current->matchId
                && sameRound(current->round, state.round) && state.phaseTime < current->phaseTime)
            || containsReusedCreation(current->participants, state.participants,
                                      issuedParticipantIdentities,
                                      [](const auto &value) { return value.participantId; })
            || containsReusedCreation(current->players, state.players, issuedPlayerIdentities,
                                      [](const auto &value) { return value.playerId; })
            || introducesUnseenRetainedResultIdentity(*current, state, issuedPlayerIdentities)
            || altersEstablishedResult(*current, state)
            || !validFollowingLobbyTransition(*current, state, issuedMatchIdentities,
                                               issuedRoundIdentities)
            || containsNonMonotonicCreation(current->entities, state.entities, highestEntityIdentity,
                                             [](const auto &value) { return value.entityId; })
            || events.size() > MaxReplicatedEvents) return std::nullopt;
        const auto newParticipantCount = static_cast<std::size_t>(std::count_if(
                state.participants.begin(), state.participants.end(), [&](const auto &participant) {
                    return !issuedParticipantIdentities.count(participant.participantId);
                }));
        const auto referencedPlayers = referencedPlayerIdentities(state);
        const auto newPlayerCount = static_cast<std::size_t>(std::count_if(
                referencedPlayers.begin(), referencedPlayers.end(), [&](Identity identity) {
                    return !issuedPlayerIdentities.count(identity);
                }));
        if (newParticipantCount > MaxReplicatedIdentityHistory - issuedParticipantIdentities.size()
            || newPlayerCount > MaxReplicatedIdentityHistory - issuedPlayerIdentities.size()
            || (state.matchId && !issuedMatchIdentities.count(state.matchId)
                && issuedMatchIdentities.size() >= MaxReplicatedIdentityHistory)
            || (state.round && !issuedRoundIdentities.count(state.round->roundId)
                && issuedRoundIdentities.size() >= MaxReplicatedIdentityHistory))
            return std::nullopt;
        std::set<Identity> eventIds;
        auto nextTransientIdentities = transientEntityIdentities;
        std::size_t eventTextBytes = 0;
        for (const auto &event: events) {
            if (!validEvent(event) || !eventIds.insert(event.eventId).second
                || event.eventId <= highestEmittedEvent
                || !validEventReferences(roundChanged ? state : *current, state, event,
                                         nextTransientIdentities, highestEntityIdentity))
                return std::nullopt;
            eventTextBytes += event.type.size();
        }
        for (const auto &entity: state.entities)
            if (nextTransientIdentities.count(entity.entityId)) return std::nullopt;
        if (!withinPayloadLimit(state, events.size(), eventTextBytes)) return std::nullopt;
        IncrementalUpdate update;
        update.sessionId = state.sessionId; update.matchId = state.matchId;
        update.baseline = currentVersion; update.version = currentVersion + 1;
        update.phase = state.phase; update.currentRoundNumber = state.currentRoundNumber;
        update.completedRounds = state.completedRounds; update.phaseTime = state.phaseTime;
        update.roundEndCountdown = state.roundEndCountdown;
        update.participants = changes(current->participants, state.participants,
                [](const auto &value) { return value.participantId; }, participantEqual);
        update.players = changes(current->players, state.players,
                [](const auto &value) { return value.playerId; }, playerEqual);
        update.entities = changes(current->entities, state.entities,
                [](const auto &value) { return value.entityId; }, entityEqual);
        update.settings = state.settings; update.round = state.round; update.score = state.score;
        update.messages = state.messages; update.effects = state.effects; update.result = state.result;
        update.events = std::move(events);
        for (const auto &participant: state.participants)
            issuedParticipantIdentities.insert(participant.participantId);
        issuedPlayerIdentities.insert(referencedPlayers.begin(), referencedPlayers.end());
        if (state.matchId) issuedMatchIdentities.insert(state.matchId);
        if (state.round) issuedRoundIdentities.insert(state.round->roundId);
        highestEntityIdentity = std::max(highestEntityIdentity,
                highestIdentity(state.entities, [](const auto &value) { return value.entityId; }));
        transientEntityIdentities = std::move(nextTransientIdentities);
        for (const auto &event: update.events) highestEmittedEvent = std::max(highestEmittedEvent, event.eventId);
        current = std::move(state);
        currentVersion = update.version;
        return update;
    }

    std::optional<FullSnapshot> AuthoritativeStateReplicator::fullSnapshot() const {
        if (!current || currentVersion == 0) return std::nullopt;
        return FullSnapshot{currentVersion, *current};
    }

    void AuthoritativeStateReplicator::discard() noexcept {
        currentVersion = 0;
        current.reset();
        highestEmittedEvent = 0;
        issuedParticipantIdentities.clear();
        issuedPlayerIdentities.clear();
        issuedMatchIdentities.clear();
        issuedRoundIdentities.clear();
        transientEntityIdentities.clear();
        highestEntityIdentity = 0;
    }

    StateVersion AuthoritativeStateReplicator::version() const noexcept { return currentVersion; }

    ApplyResult ReplicatedState::apply(const FullSnapshot &snapshot) {
        if (snapshot.version == 0 || !validateCanonicalState(snapshot.state)
            || !consistentCanonicalResultDepartures(snapshot.state)
            || (accepted && snapshot.state.sessionId != accepted->sessionId)
            || (accepted && accepted->matchId != 0 && snapshot.state.matchId == accepted->matchId
                && sameRound(accepted->round, snapshot.state.round)
                && snapshot.state.phaseTime < accepted->phaseTime)
            || (acceptedVersion && (snapshot.version < acceptedVersion
                || (snapshot.version == acceptedVersion
                    && (!resynchronizing || !sameCanonicalState(*accepted, snapshot.state))))))
            return ApplyResult::Invalid;
        const bool roundChanged = accepted && !sameRound(accepted->round, snapshot.state.round);
        if (roundChanged) {
            std::set<Identity> priorEntities;
            for (const auto &entity: accepted->entities) priorEntities.insert(entity.entityId);
            for (const auto &entity: snapshot.state.entities)
                if (priorEntities.count(entity.entityId)) return ApplyResult::Invalid;
        }
        auto nextAcceptedParticipants = acceptedParticipantIdentities;
        auto nextAcceptedPlayers = acceptedPlayerIdentities;
        Identity nextHighestEntities = highestEntityIdentity;
        if (accepted
            && (containsReusedCreation(accepted->participants, snapshot.state.participants,
                                       nextAcceptedParticipants,
                                       [](const auto &value) { return value.participantId; })
                || containsReusedCreation(accepted->players, snapshot.state.players,
                                          nextAcceptedPlayers,
                                          [](const auto &value) { return value.playerId; })
                || containsNonMonotonicCreation(accepted->entities, snapshot.state.entities,
                                                nextHighestEntities,
                                                [](const auto &value) { return value.entityId; })
                || altersEstablishedResult(*accepted, snapshot.state)
                || !validFollowingLobbyTransition(*accepted, snapshot.state,
                                                   acceptedMatchIdentities,
                                                   acceptedRoundIdentities)))
            return ApplyResult::Invalid;
        for (const auto &participant: snapshot.state.participants)
            nextAcceptedParticipants.insert(participant.participantId);
        const auto snapshotPlayerIdentities = referencedPlayerIdentities(snapshot.state);
        nextAcceptedPlayers.insert(snapshotPlayerIdentities.begin(), snapshotPlayerIdentities.end());
        auto nextAcceptedMatches = acceptedMatchIdentities;
        auto nextAcceptedRounds = acceptedRoundIdentities;
        auto nextAcceptedTransientEntities = acceptedTransientEntityIdentities;
        if (accepted && (!validMatchTransition(*accepted, snapshot.state, nextAcceptedMatches)
                         || !validRoundTransition(*accepted, snapshot.state, nextAcceptedRounds)))
            return ApplyResult::Invalid;
        if (snapshot.state.matchId) nextAcceptedMatches.insert(snapshot.state.matchId);
        if (snapshot.state.round) nextAcceptedRounds.insert(snapshot.state.round->roundId);
        if (nextAcceptedParticipants.size() > MaxReplicatedIdentityHistory
            || nextAcceptedPlayers.size() > MaxReplicatedIdentityHistory
            || nextAcceptedMatches.size() > MaxReplicatedIdentityHistory
            || nextAcceptedRounds.size() > MaxReplicatedIdentityHistory) return ApplyResult::Invalid;
        for (const auto &entity: snapshot.state.entities)
            if (nextAcceptedTransientEntities.count(entity.entityId)) return ApplyResult::Invalid;
        nextHighestEntities = std::max(nextHighestEntities, highestIdentity(
                snapshot.state.entities, [](const auto &value) { return value.entityId; }));
        accepted = snapshot.state; acceptedVersion = snapshot.version;
        acceptedParticipantIdentities = std::move(nextAcceptedParticipants);
        acceptedPlayerIdentities = std::move(nextAcceptedPlayers);
        acceptedMatchIdentities = std::move(nextAcceptedMatches);
        acceptedRoundIdentities = std::move(nextAcceptedRounds);
        acceptedTransientEntityIdentities = std::move(nextAcceptedTransientEntities);
        highestEntityIdentity = nextHighestEntities;
        resynchronizing = false;
        return ApplyResult::Applied;
    }

    ApplyResult ReplicatedState::rejectIncremental() noexcept {
        resynchronizing = true;
        return ApplyResult::ResynchronizationRequired;
    }

    ApplyResult ReplicatedState::apply(const IncrementalUpdate &update) {
        if (resynchronizing || !accepted) return ApplyResult::WaitingForSnapshot;
        if (update.sessionId != accepted->sessionId || update.baseline != acceptedVersion
            || update.version <= update.baseline || update.events.size() > MaxReplicatedEvents
            || update.participants.size() > MaxReplicatedParticipants
            || update.players.size() > MaxReplicatedPlayers
            || update.entities.size() > MaxReplicatedEntities
            || update.effects.size() > MaxReplicatedEvents) return rejectIncremental();
        if (!validChangeKinds(update.participants) || !validChangeKinds(update.players)
            || !validChangeKinds(update.entities)) return rejectIncremental();
        CanonicalState candidate = *accepted;
        auto nextAcceptedParticipants = acceptedParticipantIdentities;
        auto nextAcceptedPlayers = acceptedPlayerIdentities;
        auto nextAcceptedMatches = acceptedMatchIdentities;
        auto nextAcceptedRounds = acceptedRoundIdentities;
        auto nextAcceptedTransientEntities = acceptedTransientEntityIdentities;
        auto nextHighestEntities = highestEntityIdentity;
        if (!applyChanges(candidate.participants, update.participants, nextAcceptedParticipants,
                           [](const auto &value) { return value.participantId; })
            || !applyChanges(candidate.players, update.players, nextAcceptedPlayers,
                             [](const auto &value) { return value.playerId; })
            || !applyChanges(candidate.entities, update.entities, nextHighestEntities,
                              [](const auto &value) { return value.entityId; })) return rejectIncremental();
        for (const auto &change: update.entities)
            if (change.kind == ChangeKind::Create && nextAcceptedTransientEntities.count(change.identity))
                return rejectIncremental();
        candidate.matchId = update.matchId; candidate.phase = update.phase;
        candidate.currentRoundNumber = update.currentRoundNumber; candidate.completedRounds = update.completedRounds;
        candidate.phaseTime = update.phaseTime; candidate.roundEndCountdown = update.roundEndCountdown;
        candidate.settings = update.settings; candidate.round = update.round; candidate.score = update.score;
        candidate.messages = update.messages; candidate.effects = update.effects; candidate.result = update.result;
        if (accepted->matchId != 0 && candidate.matchId == accepted->matchId
            && sameRound(accepted->round, candidate.round)
            && candidate.phaseTime < accepted->phaseTime) return rejectIncremental();
        if (introducesUnseenRetainedResultIdentity(*accepted, candidate, acceptedPlayerIdentities)
            || altersEstablishedResult(*accepted, candidate)
            || !validFollowingLobbyTransition(*accepted, candidate, acceptedMatchIdentities,
                                               acceptedRoundIdentities))
            return rejectIncremental();
        const auto referencedPlayers = referencedPlayerIdentities(candidate);
        nextAcceptedPlayers.insert(referencedPlayers.begin(), referencedPlayers.end());
        if (!validMatchTransition(*accepted, candidate, nextAcceptedMatches)
            || !validRoundTransition(*accepted, candidate, nextAcceptedRounds)) return rejectIncremental();
        if (candidate.matchId) nextAcceptedMatches.insert(candidate.matchId);
        if (candidate.round) nextAcceptedRounds.insert(candidate.round->roundId);
        if (nextAcceptedPlayers.size() > MaxReplicatedIdentityHistory
            || nextAcceptedMatches.size() > MaxReplicatedIdentityHistory
            || nextAcceptedRounds.size() > MaxReplicatedIdentityHistory) return rejectIncremental();
        const bool roundChanged = !sameRound(accepted->round, candidate.round);
        if (roundChanged) {
            std::set<Identity> priorEntities;
            for (const auto &entity: accepted->entities) priorEntities.insert(entity.entityId);
            for (const auto &entity: candidate.entities) if (priorEntities.count(entity.entityId)) return rejectIncremental();
        }
        std::set<Identity> eventIds;
        std::size_t eventTextBytes = 0;
        for (const auto &event: update.events) {
            if (!validEvent(event)
                || !validEventReferences(roundChanged ? candidate : *accepted, candidate, event,
                                           nextAcceptedTransientEntities, highestEntityIdentity)
                || !eventIds.insert(event.eventId).second || event.eventId <= highestPresentedEvent)
                return rejectIncremental();
            eventTextBytes += event.type.size();
        }
        if (!validateCanonicalState(candidate) || !consistentCanonicalResultDepartures(candidate)
            || !withinPayloadLimit(candidate, update.events.size(), eventTextBytes)) return rejectIncremental();
        accepted = std::move(candidate); acceptedVersion = update.version;
        acceptedParticipantIdentities = std::move(nextAcceptedParticipants);
        acceptedPlayerIdentities = std::move(nextAcceptedPlayers);
        acceptedMatchIdentities = std::move(nextAcceptedMatches);
        acceptedRoundIdentities = std::move(nextAcceptedRounds);
        acceptedTransientEntityIdentities = std::move(nextAcceptedTransientEntities);
        highestEntityIdentity = nextHighestEntities;
        for (const auto &event: update.events) highestPresentedEvent = std::max(highestPresentedEvent, event.eventId);
        pendingEvents.insert(pendingEvents.end(), update.events.begin(), update.events.end());
        return ApplyResult::Applied;
    }

    void ReplicatedState::requireResynchronization() noexcept { resynchronizing = true; }
    bool ReplicatedState::resynchronizationRequired() const noexcept { return resynchronizing; }
    bool ReplicatedState::current() const noexcept { return accepted.has_value() && !resynchronizing; }
    StateVersion ReplicatedState::version() const noexcept { return acceptedVersion; }
    const CanonicalState *ReplicatedState::state() const noexcept { return current() ? &*accepted : nullptr; }
    const CanonicalState *ReplicatedState::retainedState() const noexcept {
        return accepted ? &*accepted : nullptr;
    }
    std::vector<PresentationEvent> ReplicatedState::takePresentationEvents() {
        std::vector<PresentationEvent> result;
        result.swap(pendingEvents);
        return result;
    }
}
