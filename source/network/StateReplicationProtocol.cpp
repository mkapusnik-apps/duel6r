#include "StateReplicationProtocol.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Duel6::Network::Replication {
    namespace {
        class Writer {
        public:
            template<typename T> void integer(T value) {
                using U = std::make_unsigned_t<T>;
                U bits = static_cast<U>(value);
                for (std::size_t index = 0; index < sizeof(T); ++index) {
                    bytes.push_back(static_cast<std::uint8_t>(bits & 0xffu)); bits >>= 8u;
                }
                check();
            }
            void boolean(bool value) { integer<std::uint8_t>(value ? 1 : 0); }
            void string(const std::string &value) {
                if (value.size() > MaxPayloadBytes) throw std::length_error("Replication string is too large");
                integer<std::uint32_t>(static_cast<std::uint32_t>(value.size()));
                bytes.insert(bytes.end(), value.begin(), value.end()); check();
            }
            template<typename T, typename Write> void vector(const std::vector<T> &values, Write write) {
                if (values.size() > MaxReplicatedEntities) throw std::length_error("Replication collection is too large");
                integer<std::uint32_t>(static_cast<std::uint32_t>(values.size()));
                for (const auto &value: values) write(*this, value);
            }
            std::vector<std::uint8_t> finish() { check(); return std::move(bytes); }
        private:
            std::vector<std::uint8_t> bytes;
            void check() const { if (bytes.size() > MaxPayloadBytes) throw std::length_error("Replication payload is too large"); }
        };

        class Reader {
        public:
            explicit Reader(const std::vector<std::uint8_t> &bytes) : bytes(bytes) {
                if (bytes.empty() || bytes.size() > MaxPayloadBytes) throw std::invalid_argument("Invalid replication payload size");
            }
            template<typename T> T integer() {
                require(sizeof(T));
                using U = std::make_unsigned_t<T>;
                U value = 0;
                for (std::size_t index = 0; index < sizeof(T); ++index)
                    value |= static_cast<U>(bytes[offset++]) << (index * 8u);
                return static_cast<T>(value);
            }
            bool boolean() {
                const auto value = integer<std::uint8_t>();
                if (value > 1) throw std::invalid_argument("Invalid replication Boolean");
                return value != 0;
            }
            std::string string(std::size_t maximum = MaxReplicatedStringBytes) {
                const auto size = integer<std::uint32_t>();
                if (size > maximum) throw std::invalid_argument("Replication string exceeds its bound");
                require(size);
                std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
                offset += size; return value;
            }
            template<typename T, typename Read> std::vector<T> vector(std::size_t maximum, Read read) {
                const auto count = integer<std::uint32_t>();
                if (count > maximum) throw std::invalid_argument("Replication collection exceeds its bound");
                std::vector<T> values; values.reserve(count);
                for (std::uint32_t index = 0; index < count; ++index) values.push_back(read(*this));
                return values;
            }
            bool complete() const { return offset == bytes.size(); }
        private:
            const std::vector<std::uint8_t> &bytes;
            std::size_t offset = 0;
            void require(std::size_t count) const {
                if (count > bytes.size() - offset) throw std::invalid_argument("Truncated replication payload");
            }
        };

        void writeIdentity(Writer &writer, Identity value) { writer.integer(value); }
        Identity readIdentity(Reader &reader) { return reader.integer<Identity>(); }

        void writeOutcome(Writer &w, const RoundOutcomeState &v) {
            w.vector(v.winnerPlayerIds, writeIdentity); w.integer(v.winningTeam); w.boolean(v.noWinner);
        }
        RoundOutcomeState readOutcome(Reader &r) {
            RoundOutcomeState v; v.winnerPlayerIds = r.vector<Identity>(MaxReplicatedPlayers, readIdentity);
            v.winningTeam = r.integer<std::uint8_t>(); v.noWinner = r.boolean(); return v;
        }
        void writeParticipant(Writer &w, const ParticipantState &v) {
            w.integer(v.participantId); w.boolean(v.host); w.integer(static_cast<std::uint8_t>(v.connection));
            w.boolean(v.ready); w.vector(v.ownedPlayerIds, writeIdentity);
        }
        ParticipantState readParticipant(Reader &r) {
            ParticipantState v; v.participantId = r.integer<Identity>(); v.host = r.boolean();
            v.connection = static_cast<ConnectionState>(r.integer<std::uint8_t>()); v.ready = r.boolean();
            v.ownedPlayerIds = r.vector<Identity>(MaxReplicatedPlayers, readIdentity); return v;
        }
        void writeSettings(Writer &w, const MatchSettingsState &v) {
            w.string(v.mode); w.integer(v.teamCount); w.boolean(v.friendlyFire); w.string(v.levelPlan);
            w.string(v.fixedLevel); w.vector(v.levels, [](Writer &out, const auto &s) { out.string(s); });
            w.integer(v.roundLimit); w.boolean(v.assistance); w.boolean(v.quickLiquid); w.boolean(v.burnableTrees);
        }
        MatchSettingsState readSettings(Reader &r) {
            MatchSettingsState v; v.mode = r.string(); v.teamCount = r.integer<std::uint8_t>();
            v.friendlyFire = r.boolean(); v.levelPlan = r.string(); v.fixedLevel = r.string();
            v.levels = r.vector<std::string>(MaxReplicatedLevels, [](Reader &in) { return in.string(); });
            v.roundLimit = r.integer<std::uint8_t>(); v.assistance = r.boolean();
            v.quickLiquid = r.boolean(); v.burnableTrees = r.boolean(); return v;
        }
        void writeRound(Writer &w, const std::optional<RoundState> &round) {
            w.boolean(round.has_value()); if (!round) return;
            w.integer(round->roundId); w.integer(round->roundNumber); w.string(round->level); w.boolean(round->mirrored);
            w.vector(round->rosterOrder, writeIdentity); writeOutcome(w, round->outcome);
        }
        std::optional<RoundState> readRound(Reader &r) {
            if (!r.boolean()) return std::nullopt;
            RoundState v; v.roundId = r.integer<Identity>(); v.roundNumber = r.integer<std::uint8_t>();
            v.level = r.string(); v.mirrored = r.boolean();
            v.rosterOrder = r.vector<Identity>(MaxReplicatedPlayers, readIdentity); v.outcome = readOutcome(r); return v;
        }
        void writePlayer(Writer &w, const PlayerState &v) {
            w.integer(v.playerId); w.integer(v.ownerParticipantId); w.integer(v.rosterPosition); w.string(v.displayName);
            w.integer(v.team); w.integer(static_cast<std::uint8_t>(v.lifeState)); w.integer(v.positionX); w.integer(v.positionY);
            w.integer(v.velocityX); w.integer(v.velocityY); w.boolean(v.facingLeft); w.boolean(v.crouching);
            w.integer(v.life); w.integer(v.air); w.string(v.heldWeapon); w.integer(v.ammunition); w.integer(v.actionMask);
            w.string(v.activeBonus); w.integer(v.bonusRemaining); w.boolean(v.invulnerable); w.boolean(v.visible);
            w.integer(v.reloadRemaining); w.integer(v.charge); w.integer(v.temporaryMovementRemaining);
        }
        PlayerState readPlayer(Reader &r) {
            PlayerState v; v.playerId = r.integer<Identity>(); v.ownerParticipantId = r.integer<Identity>();
            v.rosterPosition = r.integer<std::uint8_t>(); v.displayName = r.string(); v.team = r.integer<std::uint8_t>();
            v.lifeState = static_cast<LifeState>(r.integer<std::uint8_t>()); v.positionX = r.integer<std::int64_t>();
            v.positionY = r.integer<std::int64_t>(); v.velocityX = r.integer<std::int64_t>();
            v.velocityY = r.integer<std::int64_t>(); v.facingLeft = r.boolean(); v.crouching = r.boolean();
            v.life = r.integer<std::int32_t>(); v.air = r.integer<std::int64_t>(); v.heldWeapon = r.string();
            v.ammunition = r.integer<std::int32_t>(); v.actionMask = r.integer<std::uint32_t>();
            v.activeBonus = r.string(); v.bonusRemaining = r.integer<std::int64_t>(); v.invulnerable = r.boolean();
            v.visible = r.boolean(); v.reloadRemaining = r.integer<std::int64_t>(); v.charge = r.integer<std::int64_t>();
            v.temporaryMovementRemaining = r.integer<std::int64_t>(); return v;
        }
        void writeEntity(Writer &w, const WorldEntityState &v) {
            w.integer(v.entityId); w.integer(static_cast<std::uint8_t>(v.kind)); w.integer(v.ownerPlayerId); w.string(v.type);
            w.integer(v.positionX); w.integer(v.positionY); w.integer(v.velocityX); w.integer(v.velocityY);
            w.integer(v.primaryValue); w.integer(v.secondaryValue); w.boolean(v.active); w.string(v.lifecycle);
        }
        WorldEntityState readEntity(Reader &r) {
            WorldEntityState v; v.entityId = r.integer<Identity>(); v.kind = static_cast<EntityKind>(r.integer<std::uint8_t>());
            v.ownerPlayerId = r.integer<Identity>(); v.type = r.string(); v.positionX = r.integer<std::int64_t>();
            v.positionY = r.integer<std::int64_t>(); v.velocityX = r.integer<std::int64_t>();
            v.velocityY = r.integer<std::int64_t>(); v.primaryValue = r.integer<std::int64_t>();
            v.secondaryValue = r.integer<std::int64_t>(); v.active = r.boolean(); v.lifecycle = r.string(); return v;
        }
        void writeScoreRow(Writer &w, const ScoreRowState &v) {
            w.integer(v.playerId); w.integer(v.roundPoints); w.integer(v.cumulativePoints); w.integer(v.shots);
            w.integer(v.hits); w.integer(v.kills); w.integer(v.deaths); w.integer(v.assists); w.integer(v.wins);
            w.integer(v.penalties); w.integer(v.survivalTicks); w.integer(v.damage); w.integer(v.assistedDamage);
        }
        ScoreRowState readScoreRow(Reader &r) {
            ScoreRowState v; v.playerId = r.integer<Identity>(); v.roundPoints = r.integer<std::int64_t>();
            v.cumulativePoints = r.integer<std::int64_t>(); v.shots = r.integer<std::uint64_t>();
            v.hits = r.integer<std::uint64_t>(); v.kills = r.integer<std::uint64_t>(); v.deaths = r.integer<std::uint64_t>();
            v.assists = r.integer<std::uint64_t>(); v.wins = r.integer<std::uint64_t>(); v.penalties = r.integer<std::uint64_t>();
            v.survivalTicks = r.integer<std::uint64_t>(); v.damage = r.integer<std::uint64_t>();
            v.assistedDamage = r.integer<std::uint64_t>(); return v;
        }
        void writeScore(Writer &w, const ScoreState &v) {
            w.vector(v.players, writeScoreRow); w.vector(v.ranking, writeIdentity);
            w.vector(v.teamTotals, [](Writer &out, auto n) { out.integer(n); });
            w.vector(v.teamRanking, [](Writer &out, auto n) { out.integer(n); }); writeOutcome(w, v.winner);
        }
        ScoreState readScore(Reader &r) {
            ScoreState v; v.players = r.vector<ScoreRowState>(MaxReplicatedPlayers, readScoreRow);
            v.ranking = r.vector<Identity>(MaxReplicatedPlayers, readIdentity);
            v.teamTotals = r.vector<std::int64_t>(MaxReplicatedPlayers, [](Reader &in) { return in.integer<std::int64_t>(); });
            v.teamRanking = r.vector<std::uint8_t>(MaxReplicatedPlayers, [](Reader &in) { return in.integer<std::uint8_t>(); });
            v.winner = readOutcome(r); return v;
        }
        void writeMessages(Writer &w, const MessageState &v) {
            w.string(v.status); w.vector(v.events, [](Writer &out, const auto &s) { out.string(s); });
            w.vector(v.currentPlayerIndicators, writeIdentity); w.integer(v.roundProgress); w.boolean(v.scoreSummaryVisible);
        }
        MessageState readMessages(Reader &r) {
            MessageState v; v.status = r.string();
            v.events = r.vector<std::string>(MaxReplicatedMessages, [](Reader &in) { return in.string(); });
            v.currentPlayerIndicators = r.vector<Identity>(MaxReplicatedPlayers, readIdentity);
            v.roundProgress = r.integer<std::uint64_t>(); v.scoreSummaryVisible = r.boolean(); return v;
        }
        void writeEffect(Writer &w, const ContinuingEffectState &v) {
            w.integer(v.effectId); w.string(v.type); w.integer(v.playerId); w.integer(v.entityId); w.integer(v.remaining);
        }
        ContinuingEffectState readEffect(Reader &r) {
            ContinuingEffectState v; v.effectId = r.integer<Identity>(); v.type = r.string();
            v.playerId = r.integer<Identity>(); v.entityId = r.integer<Identity>();
            v.remaining = r.integer<std::int64_t>(); return v;
        }
        void writeResult(Writer &w, const ResultState &v) {
            w.boolean(v.available); w.boolean(v.sessionOnly); w.string(v.state); w.string(v.serialized);
        }
        ResultState readResult(Reader &r) {
            ResultState v; v.available = r.boolean(); v.sessionOnly = r.boolean(); v.state = r.string();
            v.serialized = r.string(MaxReplicatedResultBytes); return v;
        }
        void writeEvent(Writer &w, const PresentationEvent &v) {
            w.integer(v.eventId); w.string(v.type); w.integer(v.playerId); w.integer(v.targetPlayerId);
            w.integer(v.entityId); w.integer(v.value);
        }
        PresentationEvent readEvent(Reader &r) {
            PresentationEvent v; v.eventId = r.integer<Identity>(); v.type = r.string();
            v.playerId = r.integer<Identity>(); v.targetPlayerId = r.integer<Identity>();
            v.entityId = r.integer<Identity>(); v.value = r.integer<std::int64_t>(); return v;
        }
        void writeState(Writer &w, const CanonicalState &v) {
            w.integer(v.sessionId); w.integer(v.matchId); w.integer(v.hostParticipantId);
            w.integer(static_cast<std::uint8_t>(v.phase)); w.integer(v.currentRoundNumber); w.integer(v.completedRounds);
            w.integer(v.phaseTime); w.integer(v.roundEndCountdown); w.vector(v.participants, writeParticipant);
            writeSettings(w, v.settings); writeRound(w, v.round); w.vector(v.players, writePlayer);
            w.vector(v.entities, writeEntity); writeScore(w, v.score); writeMessages(w, v.messages);
            w.vector(v.effects, writeEffect); writeResult(w, v.result);
        }
        CanonicalState readState(Reader &r) {
            CanonicalState v; v.sessionId = r.integer<Identity>(); v.matchId = r.integer<Identity>();
            v.hostParticipantId = r.integer<Identity>(); v.phase = static_cast<Phase>(r.integer<std::uint8_t>());
            v.currentRoundNumber = r.integer<std::uint8_t>(); v.completedRounds = r.integer<std::uint8_t>();
            v.phaseTime = r.integer<std::uint64_t>(); v.roundEndCountdown = r.integer<std::uint64_t>();
            v.participants = r.vector<ParticipantState>(MaxReplicatedParticipants, readParticipant);
            v.settings = readSettings(r); v.round = readRound(r);
            v.players = r.vector<PlayerState>(MaxReplicatedPlayers, readPlayer);
            v.entities = r.vector<WorldEntityState>(MaxReplicatedEntities, readEntity); v.score = readScore(r);
            v.messages = readMessages(r); v.effects = r.vector<ContinuingEffectState>(MaxReplicatedEvents, readEffect);
            v.result = readResult(r); return v;
        }
        template<typename T, typename Write>
        void writeChanges(Writer &w, const std::vector<EntityChange<T>> &changes, Write write) {
            w.vector(changes, [&](Writer &out, const auto &change) {
                out.integer(static_cast<std::uint8_t>(change.kind)); out.integer(change.identity);
                out.boolean(change.value.has_value()); if (change.value) write(out, *change.value);
            });
        }
        template<typename T, typename Read>
        std::vector<EntityChange<T>> readChanges(Reader &r, std::size_t maximum, Read read) {
            return r.vector<EntityChange<T>>(maximum, [&](Reader &in) {
                EntityChange<T> change; change.kind = static_cast<ChangeKind>(in.integer<std::uint8_t>());
                change.identity = in.integer<Identity>(); if (in.boolean()) change.value = read(in); return change;
            });
        }
        void header(Writer &w, ReplicationFrameKind kind) {
            w.integer(ReplicationProtocolIdentifier); w.integer(ReplicationProtocolVersion);
            w.integer(static_cast<std::uint16_t>(kind));
        }
        ReplicationFrameKind readHeader(Reader &r) {
            if (r.integer<std::uint32_t>() != ReplicationProtocolIdentifier
                || r.integer<std::uint16_t>() != ReplicationProtocolVersion)
                throw std::invalid_argument("Unsupported replication protocol");
            const auto kind = static_cast<ReplicationFrameKind>(r.integer<std::uint16_t>());
            if (kind < ReplicationFrameKind::FullSnapshot || kind > ReplicationFrameKind::CanonicalStateMutation)
                throw std::invalid_argument("Unknown replication message");
            return kind;
        }
    }

    std::vector<std::uint8_t> serializeReplicationSnapshot(const FullSnapshot &snapshot) {
        if (snapshot.version == 0 || !validateCanonicalState(snapshot.state))
            throw std::invalid_argument("Invalid replication snapshot");
        Writer w; header(w, ReplicationFrameKind::FullSnapshot); w.integer(snapshot.version); writeState(w, snapshot.state);
        return w.finish();
    }

    std::vector<std::uint8_t> serializeReplicationUpdate(const IncrementalUpdate &v) {
        if (v.sessionId == 0 || v.baseline == 0 || v.version <= v.baseline
            || v.participants.size() > MaxReplicatedParticipants || v.players.size() > MaxReplicatedPlayers
            || v.entities.size() > MaxReplicatedEntities || v.effects.size() > MaxReplicatedEvents
            || v.events.size() > MaxReplicatedEvents)
            throw std::invalid_argument("Invalid replication update bounds");
        Writer w; header(w, ReplicationFrameKind::IncrementalUpdate); w.integer(v.sessionId); w.integer(v.matchId);
        w.integer(v.baseline); w.integer(v.version); w.integer(static_cast<std::uint8_t>(v.phase));
        w.integer(v.currentRoundNumber); w.integer(v.completedRounds); w.integer(v.phaseTime); w.integer(v.roundEndCountdown);
        writeChanges(w, v.participants, writeParticipant); writeSettings(w, v.settings); writeRound(w, v.round);
        writeChanges(w, v.players, writePlayer); writeChanges(w, v.entities, writeEntity); writeScore(w, v.score);
        writeMessages(w, v.messages); w.vector(v.effects, writeEffect); writeResult(w, v.result); w.vector(v.events, writeEvent);
        return w.finish();
    }

    std::vector<std::uint8_t> serializeResynchronizationRequest() {
        Writer w; header(w, ReplicationFrameKind::ResynchronizationRequest); return w.finish();
    }

    std::optional<ReplicationFrame> deserializeReplicationFrame(const std::vector<std::uint8_t> &payload) noexcept {
        try {
            Reader r(payload); ReplicationFrame frame; frame.kind = readHeader(r);
            if (frame.kind == ReplicationFrameKind::FullSnapshot) {
                FullSnapshot snapshot; snapshot.version = r.integer<StateVersion>(); snapshot.state = readState(r);
                if (!validateCanonicalState(snapshot.state) || snapshot.version == 0) return std::nullopt;
                frame.snapshot = std::move(snapshot);
            } else if (frame.kind == ReplicationFrameKind::IncrementalUpdate) {
                IncrementalUpdate v; v.sessionId = r.integer<Identity>(); v.matchId = r.integer<Identity>();
                v.baseline = r.integer<StateVersion>(); v.version = r.integer<StateVersion>();
                v.phase = static_cast<Phase>(r.integer<std::uint8_t>()); v.currentRoundNumber = r.integer<std::uint8_t>();
                v.completedRounds = r.integer<std::uint8_t>(); v.phaseTime = r.integer<std::uint64_t>();
                v.roundEndCountdown = r.integer<std::uint64_t>();
                v.participants = readChanges<ParticipantState>(r, MaxReplicatedParticipants, readParticipant);
                v.settings = readSettings(r); v.round = readRound(r);
                v.players = readChanges<PlayerState>(r, MaxReplicatedPlayers, readPlayer);
                v.entities = readChanges<WorldEntityState>(r, MaxReplicatedEntities, readEntity);
                v.score = readScore(r); v.messages = readMessages(r);
                v.effects = r.vector<ContinuingEffectState>(MaxReplicatedEvents, readEffect); v.result = readResult(r);
                v.events = r.vector<PresentationEvent>(MaxReplicatedEvents, readEvent); frame.update = std::move(v);
            }
            if (!r.complete()) return std::nullopt;
            return frame;
        } catch (...) { return std::nullopt; }
    }

    AuthoritativeReplicationConnections::AuthoritativeReplicationConnections(const AuthoritativeStateReplicator &state)
            : state(state) {}

    bool AuthoritativeReplicationConnections::restore(
            Identity participantId, ReplicationSender sender, std::function<void()> close) {
        const auto snapshot = state.fullSnapshot();
        if (participantId == 0 || !sender || !snapshot) return false;
        try {
            if (sender(serializeReplicationSnapshot(*snapshot)) != SendResult::Accepted) return false;
            connections[participantId] = {std::move(sender), std::move(close)}; return true;
        } catch (...) { return false; }
    }

    void AuthoritativeReplicationConnections::disconnect(Identity participantId) noexcept { connections.erase(participantId); }

    bool AuthoritativeReplicationConnections::broadcast(const IncrementalUpdate &update) {
        std::vector<std::uint8_t> payload;
        try { payload = serializeReplicationUpdate(update); } catch (...) { return false; }
        bool allSent = true;
        for (auto iterator = connections.begin(); iterator != connections.end();) {
            try {
                if (iterator->second.sender(payload) == SendResult::Accepted) { ++iterator; continue; }
            } catch (...) {}
            allSent = false;
            try { if (iterator->second.close) iterator->second.close(); } catch (...) {}
            iterator = connections.erase(iterator);
        }
        return allSent;
    }

    HostReplicationResult AuthoritativeReplicationConnections::receive(
            Identity participantId, const std::vector<std::uint8_t> &payload) {
        const auto found = connections.find(participantId);
        if (found == connections.end()) return HostReplicationResult::UnknownConnection;
        const auto frame = deserializeReplicationFrame(payload);
        if (!frame) {
            try { if (found->second.close) found->second.close(); } catch (...) {}
            connections.erase(found); return HostReplicationResult::InvalidMessage;
        }
        if (frame->kind == ReplicationFrameKind::CanonicalStateMutation
            || frame->kind == ReplicationFrameKind::FullSnapshot
            || frame->kind == ReplicationFrameKind::IncrementalUpdate) {
            try { if (found->second.close) found->second.close(); } catch (...) {}
            connections.erase(found); return HostReplicationResult::SessionPolicyViolation;
        }
        const auto snapshot = state.fullSnapshot();
        if (!snapshot) return HostReplicationResult::SendFailed;
        try {
            if (found->second.sender(serializeReplicationSnapshot(*snapshot)) == SendResult::Accepted)
                return HostReplicationResult::Accepted;
        } catch (...) {}
        try { if (found->second.close) found->second.close(); } catch (...) {}
        connections.erase(found); return HostReplicationResult::SendFailed;
    }

    std::size_t AuthoritativeReplicationConnections::size() const noexcept { return connections.size(); }

    ClientReplicationConnection::ClientReplicationConnection(ReplicationSender sender) : sender(std::move(sender)) {}

    ClientReplicationResult ClientReplicationConnection::receive(const std::vector<std::uint8_t> &payload) {
        if (reconnecting) return ClientReplicationResult::Reconnecting;
        const auto frame = deserializeReplicationFrame(payload);
        if (!frame || (frame->kind != ReplicationFrameKind::FullSnapshot
                       && frame->kind != ReplicationFrameKind::IncrementalUpdate)) {
            transportClosed(); return ClientReplicationResult::Reconnecting;
        }
        const ApplyResult applied = frame->snapshot ? replicated.apply(*frame->snapshot) : replicated.apply(*frame->update);
        if (applied == ApplyResult::Applied) { requestPending = false; return ClientReplicationResult::Applied; }
        if (applied == ApplyResult::WaitingForSnapshot) return ClientReplicationResult::WaitingForSnapshot;
        if (!requestPending && replicated.resynchronizationRequired()) {
            requestPending = true;
            try {
                if (!sender || sender(serializeResynchronizationRequest()) != SendResult::Accepted) {
                    transportClosed(); return ClientReplicationResult::SendFailed;
                }
            } catch (...) { transportClosed(); return ClientReplicationResult::SendFailed; }
            return ClientReplicationResult::WaitingForSnapshot;
        }
        transportClosed(); return ClientReplicationResult::Reconnecting;
    }

    void ClientReplicationConnection::transportClosed() noexcept {
        reconnecting = true; requestPending = false; replicated.requireResynchronization();
    }
    const ReplicatedState &ClientReplicationConnection::replicatedState() const noexcept { return replicated; }
}
