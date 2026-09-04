#include "AuthoritativeReplication.h"

#include <algorithm>
#include <utility>

#include "AuthoritativeMatchSerialization.h"

namespace Duel6::Server::Authoritative {
    namespace R = Network::Replication;

    namespace {
        R::MatchSettingsState replicatedSettings(const MatchConfig &config) {
            R::MatchSettingsState result;
            result.mode = modeName(config.mode); result.teamCount = config.teamCount;
            result.friendlyFire = config.friendlyFire; result.levelPlan = levelPlanName(config.levelPlan);
            result.fixedLevel = config.fixedLevel;
            result.levels = config.playableLevels; result.roundLimit = config.roundLimit;
            result.assistance = config.assistance; result.quickLiquid = config.quickLiquid;
            result.burnableTrees = config.burnableTrees;
            return result;
        }

        R::EntityKind entityKind(const std::string &kind, const std::string &type) {
            if (kind == "projectile") return R::EntityKind::Projectile;
            if (kind == "weapon-pickup") return R::EntityKind::WeaponPickup;
            if (kind == "bonus") return R::EntityKind::BonusPickup;
            if (kind == "elevator") return R::EntityKind::Elevator;
            if (kind == "tree") return R::EntityKind::Tree;
            if (kind == "fire") return R::EntityKind::Fire;
            if (kind == "explosion") return R::EntityKind::Explosion;
            if (type == "water") return R::EntityKind::Water;
            return R::EntityKind::Hazard;
        }

        R::Phase phase(MatchPhase value) {
            if (value == MatchPhase::ActiveRound) return R::Phase::ActiveRound;
            if (value == MatchPhase::RoundEndActive || value == MatchPhase::RoundEndFrozen)
                return R::Phase::RoundSummary;
            if (value == MatchPhase::Completed) return R::Phase::FinalSummary;
            if (value == MatchPhase::Ended || value == MatchPhase::Failed) return R::Phase::Ended;
            return R::Phase::Lobby;
        }

        const PlayerDefinition *definition(const std::vector<PlayerDefinition> &roster, Identity id) {
            const auto found = std::find_if(roster.begin(), roster.end(), [id](const auto &value) {
                return value.playerId == id;
            });
            return found == roster.end() ? nullptr : &*found;
        }

        bool rankedAhead(const R::ScoreRowState &left, const R::ScoreRowState &right,
                         const std::vector<PlayerDefinition> &roster) {
            if (left.cumulativePoints != right.cumulativePoints)
                return left.cumulativePoints > right.cumulativePoints;
            if (left.wins != right.wins) return left.wins > right.wins;
            if (left.damage != right.damage) return left.damage > right.damage;
            const auto *leftDefinition = definition(roster, left.playerId);
            const auto *rightDefinition = definition(roster, right.playerId);
            return leftDefinition && rightDefinition
                   && leftDefinition->rosterOrder < rightDefinition->rosterOrder;
        }
    }

    AuthoritativeReplication::AuthoritativeReplication(Identity sessionId) {
        state.sessionId = sessionId ? sessionId : identities.issue(R::IdentityCategory::Session);
    }

    bool AuthoritativeReplication::setLobby(Identity hostParticipantId,
            std::vector<R::ParticipantState> participants, std::vector<PlayerDefinition> roster,
            MatchConfig settings) {
        const AuthoritativeReplication before = *this;
        if (publisher.version() != 0 || hostParticipantId == 0) return false;
        state.hostParticipantId = hostParticipantId; state.participants = std::move(participants);
        state.settings = replicatedSettings(settings); state.phase = R::Phase::Lobby;
        state.matchId = 0; state.players.clear();
        for (const auto &entry: roster) {
            R::PlayerState player;
            player.playerId = entry.playerId; player.ownerParticipantId = entry.participantId;
            player.rosterPosition = entry.rosterOrder; player.displayName = entry.displayName;
            player.life = MaximumLife; player.lifeState = R::LifeState::Alive;
            state.players.push_back(std::move(player));
        }
        state.score.ranking.clear();
        for (const auto &player: state.players) state.score.ranking.push_back(player.playerId);
        if (publisher.initialize(state)) return true;
        *this = before;
        return false;
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::updateLobby(
            std::vector<R::ParticipantState> participants, std::vector<PlayerDefinition> roster,
            MatchConfig settings) {
        if (publisher.version() == 0 || state.phase != R::Phase::Lobby) return std::nullopt;
        const AuthoritativeReplication before = *this;
        state.participants = std::move(participants); state.settings = replicatedSettings(settings);
        state.players.clear();
        for (const auto &entry: roster) {
            R::PlayerState player;
            player.playerId = entry.playerId; player.ownerParticipantId = entry.participantId;
            player.rosterPosition = entry.rosterOrder; player.displayName = entry.displayName;
            player.life = MaximumLife; player.lifeState = R::LifeState::Alive;
            state.players.push_back(std::move(player));
        }
        if (!state.result.available) {
            state.score.players.clear(); state.score.ranking.clear();
            for (const auto &player: state.players) state.score.ranking.push_back(player.playerId);
        }
        auto update = publisher.publish(state);
        if (!update) *this = before;
        return update;
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::setParticipantReady(
            Identity participantId, bool ready) {
        if (publisher.version() == 0 || state.phase != R::Phase::Lobby) return std::nullopt;
        const AuthoritativeReplication before = *this;
        const auto found = std::find_if(state.participants.begin(), state.participants.end(),
                [participantId](const auto &participant) { return participant.participantId == participantId; });
        if (found == state.participants.end()) return std::nullopt;
        found->ready = ready;
        auto update = publisher.publish(state);
        if (!update) *this = before;
        return update;
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::setParticipantConnection(
            Identity participantId, R::ConnectionState connection) {
        if (publisher.version() == 0) return std::nullopt;
        const AuthoritativeReplication before = *this;
        const auto found = std::find_if(state.participants.begin(), state.participants.end(),
                [participantId](const auto &participant) { return participant.participantId == participantId; });
        if (found == state.participants.end()) return std::nullopt;
        found->connection = connection;
        auto update = publisher.publish(state);
        if (!update) *this = before;
        return update;
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::beginMatch(const AuthoritativeMatch &match) {
        if (publisher.version() == 0 || state.phase != R::Phase::Lobby || match.phase() == MatchPhase::Lobby)
            return std::nullopt;
        const AuthoritativeReplication before = *this;
        state.matchId = identities.issue(R::IdentityCategory::Match);
        if (state.matchId == 0) { *this = before; return std::nullopt; }
        state.result = {};
        state.round.reset();
        state.score = {};
        observedRound = 0;
        worldIdentities.clear();
        highestObservedEventSequence = 0;
        highestObservedTransitionSequence = 0;
        std::vector<R::PresentationEvent> events;
        if (!updateFromMatch(match, events)) { *this = before; return std::nullopt; }
        auto update = publisher.publish(state, std::move(events));
        if (!update) *this = before;
        return update;
    }

    Identity AuthoritativeReplication::worldIdentity(Identity roundId, std::uint64_t canonicalIdentity) {
        const auto key = std::make_pair(roundId, canonicalIdentity);
        const auto found = worldIdentities.find(key);
        if (found != worldIdentities.end()) return found->second;
        const Identity issued = identities.issue(R::IdentityCategory::WorldEntity);
        if (issued) worldIdentities.emplace(key, issued);
        return issued;
    }

    bool AuthoritativeReplication::updateFromMatch(const AuthoritativeMatch &match,
                                                    std::vector<R::PresentationEvent> &events) {
        if (state.matchId == 0) return false;
        const R::Phase priorPhase = state.phase;
        const bool resultWasAvailable = state.result.available;
        const auto roster = match.rosterDefinitions();
        const auto cumulativeStatistics = match.playerStatistics();
        const MatchConfig &config = match.frozenConfig();
        state.settings = replicatedSettings(config); state.phase = phase(match.phase());
        const bool interrupted = match.publishedResult()
                                 && match.publishedResult()->state == ResultState::Interrupted;
        if (interrupted) state.phase = R::Phase::Lobby;
        state.phaseTime = match.currentTick();
        state.messages.status = phaseName(match.phase());
        state.messages.currentPlayerIndicators.clear();
        state.messages.events.clear();
        state.currentRoundNumber = match.roundDecision().roundNumber;
        state.completedRounds = match.publishedResult() ? match.publishedResult()->completedRounds
                                                       : static_cast<std::uint8_t>(state.currentRoundNumber > 0
                                                             ? state.currentRoundNumber - 1 : 0);
        if (match.phase() == MatchPhase::RoundEndActive || match.phase() == MatchPhase::RoundEndFrozen)
            state.roundEndCountdown = match.roundEndTicksRemaining();
        else state.roundEndCountdown = 0;
        if (!interrupted && observedRound != state.currentRoundNumber) {
            state.entities.clear(); state.effects.clear();
            worldIdentities.clear();
            highestObservedEventSequence = 0; highestObservedTransitionSequence = 0;
            observedRound = state.currentRoundNumber;
            if (observedRound) {
                R::RoundState round;
                round.roundId = identities.issue(R::IdentityCategory::Round);
                if (round.roundId == 0) return false;
                round.roundNumber = observedRound; round.level = match.roundDecision().level;
                round.mirrored = match.roundDecision().mirrored; round.rosterOrder = match.roundDecision().rosterOrder;
                state.round = std::move(round);
                R::PresentationEvent event;
                event.eventId = identities.issue(R::IdentityCategory::PresentationEvent);
                event.type = "round-start";
                if (event.eventId == 0) return false;
                events.push_back(std::move(event));
            }
        }
        if (state.round) {
            const RoundResult outcome = match.currentRoundResult();
            state.score.winner.winnerPlayerIds = outcome.winnerPlayerIds;
            state.score.winner.winningTeam = static_cast<std::uint8_t>(outcome.winningTeam);
            state.score.winner.noWinner = outcome.noWinner;
            state.round->outcome = state.score.winner;
        }
        const CanonicalWorldSnapshot *world = match.canonicalWorldSnapshot();
        if (world && state.round) {
            state.players.clear(); state.score.players.clear(); state.score.ranking.clear();
            for (const auto &source: world->players) {
                const PlayerDefinition *owner = definition(roster, source.playerId);
                if (!owner) return false;
                R::PlayerState player;
                player.playerId = source.playerId; player.ownerParticipantId = owner->participantId;
                player.rosterPosition = owner->rosterOrder; player.displayName = owner->displayName;
                player.team = static_cast<std::uint8_t>(source.team);
                player.lifeState = source.departed ? R::LifeState::Departed
                                                   : (source.alive ? R::LifeState::Alive : R::LifeState::Dead);
                player.positionX = source.positionX; player.positionY = source.positionY;
                player.velocityX = source.velocityX; player.velocityY = source.velocityY;
                player.facingLeft = source.facingLeft; player.crouching = source.crouching;
                player.life = source.life; player.air = source.air;
                player.heldWeapon = source.weapon; player.ammunition = source.ammo;
                player.actionMask = source.actionMask;
                player.activeBonus = source.timedBonus; player.bonusRemaining = source.bonusRemaining;
                player.reloadRemaining = source.reload; player.charge = source.charge;
                player.temporaryMovementRemaining = source.temporarySlowdownRemaining;
                player.visible = source.visible; player.invulnerable = source.invulnerable;
                state.players.push_back(std::move(player));
                if ((source.actionMask & ShowStatus) != 0) state.messages.currentPlayerIndicators.push_back(source.playerId);
                R::ScoreRowState score;
                score.playerId = source.playerId; score.roundPoints = source.statistics.totalPoints();
                const auto cumulative = cumulativeStatistics.find(source.playerId);
                if (cumulative == cumulativeStatistics.end()) return false;
                score.cumulativePoints = cumulative->second.totalPoints();
                score.shots = source.statistics.shots; score.hits = source.statistics.hits;
                score.kills = source.statistics.kills; score.deaths = source.statistics.deaths;
                score.assists = source.statistics.assists; score.wins = source.statistics.wins;
                score.penalties = source.statistics.penalties; score.survivalTicks = source.statistics.survivalTicks;
                score.damage = source.statistics.damage; score.assistedDamage = source.statistics.assistedDamage;
                state.score.players.push_back(score);
            }
            std::sort(state.score.players.begin(), state.score.players.end(), [&](const auto &left, const auto &right) {
                return rankedAhead(left, right, roster);
            });
            for (const auto &row: state.score.players) state.score.ranking.push_back(row.playerId);
            state.score.teamTotals.assign(config.teamCount, 0);
            for (const auto &player: state.players) if (player.team > 0 && player.team <= state.score.teamTotals.size()) {
                const auto row = std::find_if(state.score.players.begin(), state.score.players.end(), [&](const auto &value) {
                    return value.playerId == player.playerId;
                });
                if (row != state.score.players.end()) state.score.teamTotals[player.team - 1] += row->cumulativePoints;
            }
            state.score.teamRanking.clear();
            for (std::uint8_t team = 1; team <= state.score.teamTotals.size(); ++team)
                state.score.teamRanking.push_back(team);
            std::stable_sort(state.score.teamRanking.begin(), state.score.teamRanking.end(), [&](auto left, auto right) {
                return state.score.teamTotals[left - 1] > state.score.teamTotals[right - 1];
            });
            const RoundResult outcome = match.currentRoundResult();
            state.score.winner.winnerPlayerIds = outcome.winnerPlayerIds;
            state.score.winner.winningTeam = static_cast<std::uint8_t>(outcome.winningTeam);
            state.score.winner.noWinner = outcome.noWinner;
            state.round->outcome = state.score.winner;
            state.entities.clear();
            const auto appendEntities = [&](const std::vector<CanonicalEntitySnapshot> &sources) {
                for (const auto &source: sources) {
                    R::WorldEntityState entity;
                    entity.entityId = worldIdentity(state.round->roundId, source.stableId);
                    entity.kind = entityKind(source.kind, source.type); entity.ownerPlayerId = source.ownerPlayerId;
                    entity.type = source.type; entity.positionX = source.positionX; entity.positionY = source.positionY;
                    entity.velocityX = source.velocityX; entity.velocityY = source.velocityY;
                    entity.primaryValue = source.primaryValue; entity.secondaryValue = source.secondaryValue;
                    entity.active = source.active; entity.lifecycle = source.active ? "active" : "inactive";
                    state.entities.push_back(std::move(entity));
                }
            };
            appendEntities(world->projectiles); appendEntities(world->pickups); appendEntities(world->elevators);
            appendEntities(world->hazards); appendEntities(world->trees);
            for (const auto &source: world->effects) {
                R::WorldEntityState entity;
                entity.entityId = worldIdentity(state.round->roundId, source.stableId);
                entity.kind = source.type == "explosion" ? R::EntityKind::Explosion : R::EntityKind::Fire;
                entity.type = source.type; entity.ownerPlayerId = source.ownerPlayerId;
                entity.positionX = source.positionX; entity.positionY = source.positionY;
                entity.primaryValue = static_cast<std::int64_t>(source.remainingTicks);
                entity.lifecycle = "active";
                state.entities.push_back(entity);
            }
            state.effects.clear();
            for (const auto &player: state.players) if (!player.activeBonus.empty() && player.bonusRemaining > 0) {
                R::ContinuingEffectState effect;
                effect.effectId = worldIdentity(state.round->roundId, (UINT64_C(0xff) << 48u) | player.playerId);
                effect.type = player.activeBonus; effect.playerId = player.playerId; effect.remaining = player.bonusRemaining;
                state.effects.push_back(std::move(effect));
            }
            for (const auto &source: world->effects) {
                R::ContinuingEffectState effect;
                effect.effectId = worldIdentity(state.round->roundId,
                        (UINT64_C(0xfd) << 48u) | (source.stableId & UINT64_C(0x0000ffffffffffff)));
                effect.type = source.type; effect.playerId = source.ownerPlayerId;
                effect.entityId = worldIdentity(state.round->roundId, source.stableId);
                effect.remaining = static_cast<std::int64_t>(source.remainingTicks);
                state.effects.push_back(std::move(effect));
            }
            const auto appendEvents = [&](const std::vector<CanonicalEvent> &sources, bool transition) {
                auto &highestObserved = transition ? highestObservedTransitionSequence
                                                   : highestObservedEventSequence;
                for (const auto &source: sources) {
                    if (source.sequence <= highestObserved) continue;
                    Identity eventId = identities.issue(R::IdentityCategory::PresentationEvent);
                    if (eventId == 0) return false;
                    R::PresentationEvent event;
                    event.eventId = eventId; event.type = source.kind; event.playerId = source.playerId;
                    event.targetPlayerId = source.targetPlayerId; event.entityId = source.entityId
                            ? worldIdentity(state.round->roundId, source.entityId) : 0;
                    event.value = source.value; events.push_back(std::move(event));
                    highestObserved = source.sequence;
                }
                return true;
            };
            if (!appendEvents(world->events, false) || !appendEvents(world->transitions, true)) return false;
            const auto appendMessages = [&](const std::vector<CanonicalEvent> &sources) {
                const std::size_t first = sources.size() > R::MaxReplicatedMessages
                                          ? sources.size() - R::MaxReplicatedMessages : 0;
                for (std::size_t index = first; index < sources.size()
                     && state.messages.events.size() < R::MaxReplicatedMessages; ++index)
                    state.messages.events.push_back(sources[index].kind);
            };
            appendMessages(world->events);
            appendMessages(world->transitions);
            std::set<Identity> retainedWorldIdentities;
            for (const auto &entity: state.entities) retainedWorldIdentities.insert(entity.entityId);
            for (const auto &effect: state.effects) retainedWorldIdentities.insert(effect.effectId);
            for (auto iterator = worldIdentities.begin(); iterator != worldIdentities.end();) {
                if (retainedWorldIdentities.count(iterator->second)) ++iterator;
                else iterator = worldIdentities.erase(iterator);
            }
        } else {
            state.entities.clear();
            state.effects.clear();
            state.score.players.clear(); state.score.ranking.clear();
            for (const auto &player: state.players) {
                const auto cumulative = cumulativeStatistics.find(player.playerId);
                if (cumulative == cumulativeStatistics.end()) return false;
                R::ScoreRowState score;
                score.playerId = player.playerId; score.cumulativePoints = cumulative->second.totalPoints();
                score.shots = cumulative->second.shots; score.hits = cumulative->second.hits;
                score.kills = cumulative->second.kills; score.deaths = cumulative->second.deaths;
                score.assists = cumulative->second.assists; score.wins = cumulative->second.wins;
                score.penalties = cumulative->second.penalties;
                score.survivalTicks = cumulative->second.survivalTicks;
                score.damage = cumulative->second.damage; score.assistedDamage = cumulative->second.assistedDamage;
                state.score.players.push_back(std::move(score));
            }
            std::sort(state.score.players.begin(), state.score.players.end(), [&](const auto &left, const auto &right) {
                return rankedAhead(left, right, roster);
            });
            for (const auto &row: state.score.players) state.score.ranking.push_back(row.playerId);
        }
        if (match.publishedResult()) {
            const auto serialized = serializeSessionResult(*match.publishedResult());
            if (!serialized) return false;
            state.result.available = true; state.result.sessionOnly = true;
            state.result.state = match.publishedResult()->state == ResultState::Completed ? "Completed" : "Interrupted";
            state.result.serialized = *serialized;
            state.score.winner.winnerPlayerIds = match.publishedResult()->finalWinnerPlayerIds;
            state.score.winner.winningTeam = static_cast<std::uint8_t>(match.publishedResult()->finalWinningTeam);
            state.score.winner.noWinner = match.publishedResult()->finalNoWinner;
            if (interrupted) {
                for (auto &participant: state.participants) participant.ready = false;
                state.currentRoundNumber = state.completedRounds;
                state.entities.clear(); state.effects.clear();
                if (match.publishedResult()->rounds.empty()) state.round.reset();
                else {
                    const auto &lastRound = match.publishedResult()->rounds.back();
                    R::RoundState retainedRound;
                    retainedRound.roundId = state.round && state.round->roundNumber == lastRound.roundNumber
                                            ? state.round->roundId
                                            : identities.issue(R::IdentityCategory::Round);
                    if (retainedRound.roundId == 0) return false;
                    retainedRound.roundNumber = lastRound.roundNumber;
                    retainedRound.level = lastRound.level; retainedRound.mirrored = lastRound.mirrored;
                    retainedRound.rosterOrder = lastRound.rosterOrder;
                    retainedRound.outcome.winnerPlayerIds = lastRound.winnerPlayerIds;
                    retainedRound.outcome.winningTeam = static_cast<std::uint8_t>(lastRound.winningTeam);
                    retainedRound.outcome.noWinner = lastRound.noWinner;
                    state.round = std::move(retainedRound);
                }
            } else if (state.round) state.round->outcome = state.score.winner;
        }
        state.messages.roundProgress = state.phaseTime;
        state.messages.scoreSummaryVisible = state.phase == R::Phase::RoundSummary
                                             || state.phase == R::Phase::FinalSummary;
        if (priorPhase != state.phase && state.phase == R::Phase::RoundSummary) {
            R::PresentationEvent event;
            event.eventId = identities.issue(R::IdentityCategory::PresentationEvent); event.type = "round-outcome";
            if (event.eventId == 0) return false;
            events.push_back(std::move(event));
        }
        if (!resultWasAvailable && state.result.available) {
            R::PresentationEvent event;
            event.eventId = identities.issue(R::IdentityCategory::PresentationEvent); event.type = "result-transition";
            if (event.eventId == 0) return false;
            events.push_back(std::move(event));
        }
        return R::validateCanonicalState(state);
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::capture(const AuthoritativeMatch &match) {
        if (publisher.version() == 0 || state.matchId == 0) return std::nullopt;
        const AuthoritativeReplication before = *this;
        std::vector<R::PresentationEvent> events;
        if (!updateFromMatch(match, events)) { *this = before; return std::nullopt; }
        auto update = publisher.publish(state, std::move(events));
        if (!update) *this = before;
        return update;
    }

    std::optional<R::IncrementalUpdate> AuthoritativeReplication::enterFollowingLobby() {
        if (publisher.version() == 0 || (state.phase != R::Phase::FinalSummary && state.phase != R::Phase::Lobby)
            || !state.result.available)
            return std::nullopt;
        const AuthoritativeReplication before = *this;
        state.phase = R::Phase::Lobby;
        state.messages.status = "lobby";
        state.messages.scoreSummaryVisible = false;
        state.entities.clear();
        state.effects.clear();
        state.roundEndCountdown = 0;
        for (auto &participant: state.participants) participant.ready = false;
        auto update = publisher.publish(state);
        if (!update) *this = before;
        return update;
    }

    std::optional<R::FullSnapshot> AuthoritativeReplication::fullSnapshot() const { return publisher.fullSnapshot(); }
    const R::AuthoritativeStateReplicator &AuthoritativeReplication::replicator() const noexcept { return publisher; }
}
