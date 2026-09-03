#include "StateReplication.h"

#include <algorithm>
#include <limits>
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
        bool containsRemoved(const std::vector<T> &values, const std::set<Identity> &removed, Id id) {
            return std::any_of(values.begin(), values.end(), [&](const auto &value) { return removed.count(id(value)); });
        }

        template<typename T, typename Id>
        bool applyChanges(std::vector<T> &values, const std::vector<EntityChange<T>> &updates,
                          std::set<Identity> &removed, Id id) {
            std::map<Identity, T> indexed;
            for (const auto &value: values) if (!indexed.emplace(id(value), value).second) return false;
            std::set<Identity> changed;
            for (const auto &change: updates) {
                if (change.identity == 0 || !changed.insert(change.identity).second) return false;
                const auto existing = indexed.find(change.identity);
                if (change.kind == ChangeKind::Create) {
                    if (!change.value || id(*change.value) != change.identity || existing != indexed.end()
                        || removed.count(change.identity)) return false;
                    indexed.emplace(change.identity, *change.value);
                } else if (change.kind == ChangeKind::Update) {
                    if (!change.value || id(*change.value) != change.identity || existing == indexed.end()
                        || removed.count(change.identity)) return false;
                    existing->second = *change.value;
                } else {
                    if (change.value || existing == indexed.end() || removed.count(change.identity)) return false;
                    indexed.erase(existing);
                    removed.insert(change.identity);
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
        Identity &candidate = next[category];
        if (candidate == 0) candidate = 1;
        if (candidate == std::numeric_limits<Identity>::max()) return 0;
        const Identity value = candidate++;
        issued[category].insert(value);
        return value;
    }

    bool StableIdentitySource::wasIssued(IdentityCategory category, Identity identity) const noexcept {
        const auto categoryValues = issued.find(category);
        return identity != 0 && categoryValues != issued.end() && categoryValues->second.count(identity);
    }

    bool validateCanonicalState(const CanonicalState &state) noexcept {
        try {
            if (state.sessionId == 0 || state.participants.empty()
                || state.participants.size() > MaxReplicatedParticipants
                || state.players.size() > MaxReplicatedPlayers
                || state.entities.size() > MaxReplicatedEntities
                || state.messages.events.size() > MaxReplicatedMessages
                || state.effects.size() > MaxReplicatedEvents
                || state.result.serialized.size() > MaxReplicatedResultBytes
                || !validText(state.messages.status, 256, true)
                || !validText(state.result.state, 64, true)) return false;
            if (!withinPayloadLimit(state)) return false;
            if ((state.phase == Phase::Lobby && state.matchId != 0)
                || (state.phase != Phase::Lobby && state.matchId == 0)) return false;
            std::set<Identity> participantIds;
            if (!uniqueNonzero(state.participants, [](const auto &value) { return value.participantId; },
                               &participantIds)) return false;
            std::size_t hosts = 0;
            std::set<Identity> ownedPlayerIds;
            for (const auto &participant: state.participants) {
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
                    || !validText(player.activeBonus, 64, true) || player.life < 0) return false;
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
                if ((entity.ownerPlayerId && !playerIds.count(entity.ownerPlayerId)) || !validText(entity.type)
                    || !validText(entity.lifecycle, 64, true)) return false;
            }
            if (state.round) {
                if (state.round->roundId == 0 || state.round->roundNumber == 0 || !validText(state.round->level, 240)
                    || !uniqueNonzero(state.round->rosterOrder, [](Identity value) { return value; })) return false;
                for (Identity player: state.round->rosterOrder) if (!playerIds.count(player)) return false;
                for (Identity player: state.round->outcome.winnerPlayerIds) if (!playerIds.count(player)) return false;
            } else if (state.phase == Phase::ActiveRound || state.phase == Phase::RoundSummary) return false;
            if (!validText(state.settings.mode) || !validText(state.settings.levelPlan)
                || state.settings.levels.size() > 256) return false;
            for (const auto &level: state.settings.levels) if (!validText(level, 240)) return false;
            if (!uniqueNonzero(state.score.players, [](const auto &value) { return value.playerId; })) return false;
            for (const auto &row: state.score.players) if (!playerIds.count(row.playerId)) return false;
            if (!uniqueNonzero(state.score.ranking, [](Identity value) { return value; })) return false;
            for (Identity player: state.score.ranking) if (!playerIds.count(player)) return false;
            for (Identity player: state.score.winner.winnerPlayerIds) if (!playerIds.count(player)) return false;
            for (const auto &message: state.messages.events) if (!validText(message, 256)) return false;
            for (Identity player: state.messages.currentPlayerIndicators) if (!playerIds.count(player)) return false;
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
            return true;
        } catch (...) { return false; }
    }

    bool AuthoritativeStateReplicator::initialize(CanonicalState state) {
        if (current || !validateCanonicalState(state)) return false;
        current = std::move(state);
        currentVersion = 1;
        return true;
    }

    std::optional<IncrementalUpdate> AuthoritativeStateReplicator::publish(
            CanonicalState state, std::vector<PresentationEvent> events) {
        if (!current || currentVersion == std::numeric_limits<StateVersion>::max()
            || !validateCanonicalState(state) || state.sessionId != current->sessionId
            || containsRemoved(state.participants, removedParticipants, [](const auto &value) { return value.participantId; })
            || containsRemoved(state.players, removedPlayers, [](const auto &value) { return value.playerId; })
            || containsRemoved(state.entities, removedEntities, [](const auto &value) { return value.entityId; })
            || events.size() > MaxReplicatedEvents) return std::nullopt;
        std::set<Identity> eventIds;
        std::size_t eventTextBytes = 0;
        for (const auto &event: events) {
            if (!validEvent(event) || !eventIds.insert(event.eventId).second || emittedEvents.count(event.eventId))
                return std::nullopt;
            eventTextBytes += event.type.size();
        }
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
        for (const auto &change: update.participants) if (change.kind == ChangeKind::Remove) removedParticipants.insert(change.identity);
        for (const auto &change: update.players) if (change.kind == ChangeKind::Remove) removedPlayers.insert(change.identity);
        for (const auto &change: update.entities) if (change.kind == ChangeKind::Remove) removedEntities.insert(change.identity);
        for (const auto &event: update.events) emittedEvents.insert(event.eventId);
        current = std::move(state);
        currentVersion = update.version;
        return update;
    }

    std::optional<FullSnapshot> AuthoritativeStateReplicator::fullSnapshot() const {
        if (!current || currentVersion == 0) return std::nullopt;
        return FullSnapshot{currentVersion, *current};
    }

    StateVersion AuthoritativeStateReplicator::version() const noexcept { return currentVersion; }

    ApplyResult ReplicatedState::apply(const FullSnapshot &snapshot) {
        if (snapshot.version == 0 || !validateCanonicalState(snapshot.state)
            || (accepted && snapshot.state.sessionId != accepted->sessionId)
            || (acceptedVersion && snapshot.version <= acceptedVersion)) return ApplyResult::Invalid;
        std::set<Identity> nextRemovedParticipants = removedParticipants;
        std::set<Identity> nextRemovedPlayers = removedPlayers;
        std::set<Identity> nextRemovedEntities = removedEntities;
        if (accepted) {
            for (const auto &value: accepted->participants)
                if (std::none_of(snapshot.state.participants.begin(), snapshot.state.participants.end(), [&](const auto &next) {
                    return next.participantId == value.participantId;
                })) nextRemovedParticipants.insert(value.participantId);
            for (const auto &value: accepted->players)
                if (std::none_of(snapshot.state.players.begin(), snapshot.state.players.end(), [&](const auto &next) {
                    return next.playerId == value.playerId;
                })) nextRemovedPlayers.insert(value.playerId);
            for (const auto &value: accepted->entities)
                if (std::none_of(snapshot.state.entities.begin(), snapshot.state.entities.end(), [&](const auto &next) {
                    return next.entityId == value.entityId;
                })) nextRemovedEntities.insert(value.entityId);
        }
        if (containsRemoved(snapshot.state.participants, nextRemovedParticipants, [](const auto &value) { return value.participantId; })
            || containsRemoved(snapshot.state.players, nextRemovedPlayers, [](const auto &value) { return value.playerId; })
            || containsRemoved(snapshot.state.entities, nextRemovedEntities, [](const auto &value) { return value.entityId; }))
            return ApplyResult::Invalid;
        accepted = snapshot.state; acceptedVersion = snapshot.version;
        removedParticipants = std::move(nextRemovedParticipants);
        removedPlayers = std::move(nextRemovedPlayers);
        removedEntities = std::move(nextRemovedEntities);
        pendingEvents.clear();
        resynchronizing = false;
        return ApplyResult::Applied;
    }

    ApplyResult ReplicatedState::rejectIncremental() noexcept {
        resynchronizing = true;
        pendingEvents.clear();
        return ApplyResult::ResynchronizationRequired;
    }

    ApplyResult ReplicatedState::apply(const IncrementalUpdate &update) {
        if (resynchronizing || !accepted) return ApplyResult::WaitingForSnapshot;
        if (update.sessionId != accepted->sessionId || update.baseline != acceptedVersion
            || update.version <= update.baseline || update.events.size() > MaxReplicatedEvents) return rejectIncremental();
        CanonicalState candidate = *accepted;
        auto nextRemovedParticipants = removedParticipants;
        auto nextRemovedPlayers = removedPlayers;
        auto nextRemovedEntities = removedEntities;
        if (!applyChanges(candidate.participants, update.participants, nextRemovedParticipants,
                          [](const auto &value) { return value.participantId; })
            || !applyChanges(candidate.players, update.players, nextRemovedPlayers,
                             [](const auto &value) { return value.playerId; })
            || !applyChanges(candidate.entities, update.entities, nextRemovedEntities,
                             [](const auto &value) { return value.entityId; })) return rejectIncremental();
        candidate.matchId = update.matchId; candidate.phase = update.phase;
        candidate.currentRoundNumber = update.currentRoundNumber; candidate.completedRounds = update.completedRounds;
        candidate.phaseTime = update.phaseTime; candidate.roundEndCountdown = update.roundEndCountdown;
        candidate.settings = update.settings; candidate.round = update.round; candidate.score = update.score;
        candidate.messages = update.messages; candidate.effects = update.effects; candidate.result = update.result;
        std::set<Identity> eventIds;
        std::size_t eventTextBytes = 0;
        for (const auto &event: update.events) {
            if (!validEvent(event) || !eventIds.insert(event.eventId).second || presentedEvents.count(event.eventId))
                return rejectIncremental();
            if ((event.playerId && std::none_of(candidate.players.begin(), candidate.players.end(), [&](const auto &player) {
                    return player.playerId == event.playerId;
                })) || (event.targetPlayerId && std::none_of(candidate.players.begin(), candidate.players.end(), [&](const auto &player) {
                    return player.playerId == event.targetPlayerId;
                }))) return rejectIncremental();
            eventTextBytes += event.type.size();
        }
        if (!validateCanonicalState(candidate)
            || !withinPayloadLimit(candidate, update.events.size(), eventTextBytes)) return rejectIncremental();
        accepted = std::move(candidate); acceptedVersion = update.version;
        removedParticipants = std::move(nextRemovedParticipants);
        removedPlayers = std::move(nextRemovedPlayers);
        removedEntities = std::move(nextRemovedEntities);
        for (const auto &event: update.events) presentedEvents.insert(event.eventId);
        pendingEvents = update.events;
        return ApplyResult::Applied;
    }

    void ReplicatedState::requireResynchronization() noexcept { resynchronizing = true; pendingEvents.clear(); }
    bool ReplicatedState::resynchronizationRequired() const noexcept { return resynchronizing; }
    bool ReplicatedState::current() const noexcept { return accepted.has_value() && !resynchronizing; }
    StateVersion ReplicatedState::version() const noexcept { return acceptedVersion; }
    const CanonicalState *ReplicatedState::state() const noexcept { return current() ? &*accepted : nullptr; }
    std::vector<PresentationEvent> ReplicatedState::takePresentationEvents() {
        std::vector<PresentationEvent> result;
        result.swap(pendingEvents);
        return result;
    }
}
