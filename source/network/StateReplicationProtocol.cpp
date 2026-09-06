#include "StateReplicationProtocol.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Duel6::Network::Replication {
    namespace {
        constexpr std::uint64_t MaximumAuthoritativeTimestamp =
                static_cast<std::uint64_t>(std::chrono::milliseconds::max().count());

        std::optional<std::chrono::milliseconds> elapsedMilliseconds(
                Responsiveness::TimePoint startedAt,
                Responsiveness::TimePoint finishedAt) noexcept {
            if (finishedAt < startedAt) return std::nullopt;
            using Clock = Responsiveness::Clock;
            using Rep = Clock::duration::rep;
            using Unsigned = std::make_unsigned_t<Rep>;
            static_assert(std::numeric_limits<Rep>::is_signed,
                          "The responsiveness clock must use a signed duration");
            const Rep started = startedAt.time_since_epoch().count();
            const Rep finished = finishedAt.time_since_epoch().count();
            Unsigned ticks;
            if (started < 0 && finished >= 0) {
                const Unsigned beforeEpoch = static_cast<Unsigned>(-(started + 1)) + 1;
                const Unsigned afterEpoch = static_cast<Unsigned>(finished);
                const Unsigned maximum = static_cast<Unsigned>(
                        std::numeric_limits<Rep>::max());
                if (beforeEpoch > maximum - afterEpoch) return std::nullopt;
                ticks = beforeEpoch + afterEpoch;
            } else {
                ticks = static_cast<Unsigned>(finished - started);
            }
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::duration{static_cast<Rep>(ticks)});
        }

        bool canSubtractMilliseconds(Responsiveness::TimePoint from,
                                     std::chrono::milliseconds amount) noexcept {
            using Clock = Responsiveness::Clock;
            if (amount < std::chrono::milliseconds::zero()
                || amount > std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::duration::max())) return false;
            const auto delta = std::chrono::duration_cast<Clock::duration>(amount).count();
            return from.time_since_epoch().count() >= Clock::duration::min().count() + delta;
        }

        std::optional<Responsiveness::TimePoint> addMilliseconds(
                Responsiveness::TimePoint from, std::chrono::milliseconds amount) noexcept {
            using Clock = Responsiveness::Clock;
            if (amount < std::chrono::milliseconds::zero()
                || amount > std::chrono::duration_cast<std::chrono::milliseconds>(
                        Clock::duration::max())) return std::nullopt;
            const auto delta = std::chrono::duration_cast<Clock::duration>(amount).count();
            const auto fromCount = from.time_since_epoch().count();
            if (fromCount > Clock::duration::max().count() - delta) return std::nullopt;
            return Responsiveness::TimePoint{Clock::duration{fromCount + delta}};
        }

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
            if (kind < ReplicationFrameKind::FullSnapshot || kind > ReplicationFrameKind::QualityResponse)
                throw std::invalid_argument("Unknown replication message");
            return kind;
        }
    }

    std::vector<std::uint8_t> serializeReplicationSnapshot(const FullSnapshot &snapshot) {
        if (snapshot.version == 0 || !validateCanonicalState(snapshot.state))
            throw std::invalid_argument("Invalid replication snapshot");
        Writer w; header(w, ReplicationFrameKind::FullSnapshot); w.integer(snapshot.version);
        w.integer(snapshot.authoritativeProducedAt); writeState(w, snapshot.state);
        return w.finish();
    }

    std::vector<std::uint8_t> serializeReplicationUpdate(const IncrementalUpdate &v) {
        if (v.sessionId == 0 || v.baseline == 0 || v.version <= v.baseline
            || v.participants.size() > MaxReplicatedParticipants || v.players.size() > MaxReplicatedPlayers
            || v.entities.size() > MaxReplicatedEntities || v.effects.size() > MaxReplicatedEvents
            || v.events.size() > MaxReplicatedEvents)
            throw std::invalid_argument("Invalid replication update bounds");
        Writer w; header(w, ReplicationFrameKind::IncrementalUpdate); w.integer(v.sessionId); w.integer(v.matchId);
        w.integer(v.baseline); w.integer(v.version); w.integer(v.authoritativeProducedAt);
        w.integer(static_cast<std::uint8_t>(v.phase));
        w.integer(v.currentRoundNumber); w.integer(v.completedRounds); w.integer(v.phaseTime); w.integer(v.roundEndCountdown);
        writeChanges(w, v.participants, writeParticipant); writeSettings(w, v.settings); writeRound(w, v.round);
        writeChanges(w, v.players, writePlayer); writeChanges(w, v.entities, writeEntity); writeScore(w, v.score);
        writeMessages(w, v.messages); w.vector(v.effects, writeEffect); writeResult(w, v.result); w.vector(v.events, writeEvent);
        return w.finish();
    }

    std::vector<std::uint8_t> serializeResynchronizationRequest() {
        Writer w; header(w, ReplicationFrameKind::ResynchronizationRequest); return w.finish();
    }

    std::vector<std::uint8_t> serializeQualityProbe(std::uint64_t sequence) {
        if (sequence == 0) throw std::invalid_argument("Invalid quality probe sequence");
        Writer w; header(w, ReplicationFrameKind::QualityProbe); w.integer(sequence); return w.finish();
    }

    std::vector<std::uint8_t> serializeQualityResponse(
            std::uint64_t sequence, std::uint64_t authoritativeResponseAt) {
        if (sequence == 0) throw std::invalid_argument("Invalid quality response sequence");
        Writer w; header(w, ReplicationFrameKind::QualityResponse); w.integer(sequence);
        w.integer(authoritativeResponseAt); return w.finish();
    }

    std::optional<ReplicationFrame> deserializeReplicationFrame(const std::vector<std::uint8_t> &payload) noexcept {
        try {
            Reader r(payload); ReplicationFrame frame; frame.kind = readHeader(r);
            if (frame.kind == ReplicationFrameKind::FullSnapshot) {
                FullSnapshot snapshot; snapshot.version = r.integer<StateVersion>();
                snapshot.authoritativeProducedAt = r.integer<std::uint64_t>(); snapshot.state = readState(r);
                if (!validateCanonicalState(snapshot.state) || snapshot.version == 0) return std::nullopt;
                frame.snapshot = std::move(snapshot);
            } else if (frame.kind == ReplicationFrameKind::IncrementalUpdate) {
                IncrementalUpdate v; v.sessionId = r.integer<Identity>(); v.matchId = r.integer<Identity>();
                v.baseline = r.integer<StateVersion>(); v.version = r.integer<StateVersion>();
                v.authoritativeProducedAt = r.integer<std::uint64_t>();
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
            } else if (frame.kind == ReplicationFrameKind::QualityProbe
                       || frame.kind == ReplicationFrameKind::QualityResponse) {
                const auto sequence = r.integer<std::uint64_t>();
                if (sequence == 0) return std::nullopt;
                frame.qualitySequence = sequence;
                if (frame.kind == ReplicationFrameKind::QualityResponse)
                    frame.authoritativeResponseAt = r.integer<std::uint64_t>();
            }
            if (!r.complete()) return std::nullopt;
            return frame;
        } catch (...) { return std::nullopt; }
    }

    AuthoritativeReplicationConnections::AuthoritativeReplicationConnections(
            const AuthoritativeStateReplicator &state, AuthoritativeClock clock)
            : state(state), clock(std::move(clock)) {
        if (!this->clock) {
            this->clock = [] {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        Responsiveness::Clock::now().time_since_epoch()).count();
                return static_cast<std::uint64_t>(std::max<std::int64_t>(1, elapsed));
            };
        }
    }

    bool AuthoritativeReplicationConnections::restore(
            Identity participantId, ReplicationSender sender, std::function<void()> close) {
        auto snapshot = state.fullSnapshot();
        if (participantId == 0 || !sender || !snapshot) return false;
        try {
            snapshot->authoritativeProducedAt = clock();
            if (snapshot->authoritativeProducedAt == 0) return false;
            if (sender(serializeReplicationSnapshot(*snapshot)) != SendResult::Accepted) return false;
            connections[participantId] = {std::move(sender), std::move(close)}; return true;
        } catch (...) { return false; }
    }

    void AuthoritativeReplicationConnections::disconnect(Identity participantId) noexcept { connections.erase(participantId); }

    bool AuthoritativeReplicationConnections::broadcast(const IncrementalUpdate &update) {
        std::vector<std::uint8_t> payload;
        try {
            auto produced = update;
            produced.authoritativeProducedAt = clock();
            if (produced.authoritativeProducedAt == 0) return false;
            payload = serializeReplicationUpdate(produced);
        } catch (...) { return false; }
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
        if (frame->kind == ReplicationFrameKind::QualityProbe && frame->qualitySequence) {
            try {
                const auto respondedAt = clock();
                if (respondedAt != 0
                    && found->second.sender(serializeQualityResponse(
                            *frame->qualitySequence, respondedAt)) == SendResult::Accepted)
                    return HostReplicationResult::Accepted;
            } catch (...) {}
            try { if (found->second.close) found->second.close(); } catch (...) {}
            connections.erase(found);
            return HostReplicationResult::SendFailed;
        }
        if (frame->kind == ReplicationFrameKind::CanonicalStateMutation
            || frame->kind == ReplicationFrameKind::FullSnapshot
            || frame->kind == ReplicationFrameKind::IncrementalUpdate
            || frame->kind == ReplicationFrameKind::QualityResponse) {
            try { if (found->second.close) found->second.close(); } catch (...) {}
            connections.erase(found); return HostReplicationResult::SessionPolicyViolation;
        }
        auto snapshot = state.fullSnapshot();
        if (!snapshot) return HostReplicationResult::SendFailed;
        try {
            snapshot->authoritativeProducedAt = clock();
            if (snapshot->authoritativeProducedAt == 0) return HostReplicationResult::SendFailed;
            if (found->second.sender(serializeReplicationSnapshot(*snapshot)) == SendResult::Accepted)
                return HostReplicationResult::Accepted;
        } catch (...) {}
        try { if (found->second.close) found->second.close(); } catch (...) {}
        connections.erase(found); return HostReplicationResult::SendFailed;
    }

    std::size_t AuthoritativeReplicationConnections::size() const noexcept { return connections.size(); }

    ClientReplicationConnection::ClientReplicationConnection(
            ReplicationSender sender, Responsiveness::Environment environment,
            bool requireAuthoritativeTime)
            : sender(std::move(sender)), quality(environment),
              maximumCalibrationRoundTrip(Responsiveness::budget(environment).roundTripLatency),
              requireAuthoritativeTime(requireAuthoritativeTime) {}

    ClientReplicationResult ClientReplicationConnection::receive(const std::vector<std::uint8_t> &payload) {
        return receive(payload, Responsiveness::Clock::now());
    }

    ClientReplicationResult ClientReplicationConnection::receive(
            const std::vector<std::uint8_t> &payload, Responsiveness::TimePoint acceptedAt) {
        return receive(payload, acceptedAt, false, true);
    }

    ClientReplicationResult ClientReplicationConnection::receiveInitialAdmissionFrame(
            const std::vector<std::uint8_t> &payload, Responsiveness::TimePoint acceptedAt,
            bool allowOutboundExchange) {
        return receive(payload, acceptedAt, true, allowOutboundExchange);
    }

    ClientReplicationResult ClientReplicationConnection::receive(
            const std::vector<std::uint8_t> &payload, Responsiveness::TimePoint acceptedAt,
            bool initialAdmissionCalibration, bool allowOutboundExchange) {
        if (reconnecting) return ClientReplicationResult::Reconnecting;
        const auto frame = deserializeReplicationFrame(payload);
        if (!frame) {
            transportClosed(); return ClientReplicationResult::Reconnecting;
        }
        if (frame->kind == ReplicationFrameKind::QualityResponse && frame->qualitySequence) {
            if (*frame->qualitySequence > qualityProbeSequence) {
                transportClosed();
                return ClientReplicationResult::Reconnecting;
            }
            if (!qualityProbeSentAt || *frame->qualitySequence < qualityProbeSequence)
                return ClientReplicationResult::NetworkSampled;
            if (acceptedAt < *qualityProbeSentAt
                || (requireAuthoritativeTime
                    && (!frame->authoritativeResponseAt || *frame->authoritativeResponseAt == 0))) {
                transportClosed();
                return ClientReplicationResult::Reconnecting;
            }
            const auto elapsedValue = elapsedMilliseconds(*qualityProbeSentAt, acceptedAt);
            if (!elapsedValue) {
                transportClosed();
                return ClientReplicationResult::Reconnecting;
            }
            const auto elapsed = *elapsedValue;
            bool missedQualityDeadline = false;
            if (elapsed >= Responsiveness::QualityProbeDeadline) {
                const auto deadline = addMilliseconds(
                        *qualityProbeSentAt, Responsiveness::QualityProbeDeadline);
                if (!deadline) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                recordQualityOutcome(true, quality.currentRoundTripLatency().value_or(
                        Responsiveness::QualityProbeDeadline),
                        *deadline);
                missedQualityDeadline = true;
                if (!initialAdmissionCalibration || localClockSynchronizedAt) {
                    qualityProbeSentAt.reset();
                    return ClientReplicationResult::NetworkSampled;
                }
            }
            if (elapsed > maximumCalibrationRoundTrip) {
                qualityProbeSentAt.reset();
                if (!missedQualityDeadline) recordQualityOutcome(false, elapsed, acceptedAt);
                return ClientReplicationResult::NetworkSampled;
            }
            if (frame->authoritativeResponseAt && *frame->authoritativeResponseAt != 0) {
                const auto halfRoundTrip = (static_cast<std::uint64_t>(elapsed.count()) + 1u) / 2u;
                if (*frame->authoritativeResponseAt > MaximumAuthoritativeTimestamp - halfRoundTrip) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                std::uint64_t synchronizedTime = *frame->authoritativeResponseAt + halfRoundTrip;
                if (localClockSynchronizedAt && authoritativeClockAtSynchronization) {
                    const auto priorTime = authoritativeTimeAt(acceptedAt);
                    if (!priorTime) {
                        transportClosed();
                        return ClientReplicationResult::Reconnecting;
                    }
                    synchronizedTime = std::max(synchronizedTime, *priorTime);
                }
                authoritativeClockAtSynchronization = synchronizedTime;
                localClockSynchronizedAt = acceptedAt;
                // A supported RTT makes ceil(RTT/2) both measurement-bounded and no
                // greater than the environment's admitted uncertainty. Never clamp an
                // unsupported measurement into a seemingly precise calibration.
                authoritativeClockUncertainty = halfRoundTrip;
            }
            qualityProbeSentAt.reset();
            if (!missedQualityDeadline) recordQualityOutcome(false, elapsed, acceptedAt);
            if (pendingAuthoritativeProducedAt && pendingCanonicalAcceptedAt) {
                const auto age = authoritativeStateAge(
                        *pendingAuthoritativeProducedAt, *pendingCanonicalAcceptedAt);
                if (!age || !quality.observeCanonicalState(
                        replicated.version(), *age, *pendingCanonicalAcceptedAt)) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                pendingAuthoritativeProducedAt.reset();
                pendingCanonicalAcceptedAt.reset();
            }
            if (pendingInitialSnapshot && pendingInitialSnapshotAcceptedAt) {
                auto snapshot = std::move(*pendingInitialSnapshot);
                const auto snapshotAcceptedAt = *pendingInitialSnapshotAcceptedAt;
                const auto snapshotBytes = pendingInitialSnapshotReservedBytes;
                auto snapshotReservation = std::move(pendingInitialSnapshotBudgetReservation);
                pendingInitialSnapshot.reset();
                pendingInitialSnapshotAcceptedAt.reset();
                pendingInitialSnapshotReservedBytes = 0;
                releasePendingInitialPayload(snapshotBytes);
                ClientReplicationResult result;
                try {
                    result = receive(serializeReplicationSnapshot(snapshot), snapshotAcceptedAt,
                                     false, allowOutboundExchange);
                } catch (...) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                if (result == ClientReplicationResult::Reconnecting
                    || result == ClientReplicationResult::SendFailed)
                    return result;
                if (result == ClientReplicationResult::Applied && replicated.state())
                    acceptedInitialAdmissionState = *replicated.state();
                else {
                    pendingInitialFrames.clear();
                    pendingInitialPayloadBytes = 0;
                }
                while (acceptedInitialAdmissionState && !pendingInitialFrames.empty()) {
                    auto pending = std::move(pendingInitialFrames.front());
                    pendingInitialFrames.pop_front();
                    releasePendingInitialPayload(pending.reservedBytes);
                    try {
                        result = receive(pending.payload, pending.acceptedAt,
                                         false, allowOutboundExchange);
                    } catch (...) {
                        transportClosed();
                        return ClientReplicationResult::Reconnecting;
                    }
                    if (result == ClientReplicationResult::Reconnecting
                        || result == ClientReplicationResult::SendFailed)
                        return result;
                }
            }
            return ClientReplicationResult::NetworkSampled;
        }
        if (frame->kind != ReplicationFrameKind::FullSnapshot
            && frame->kind != ReplicationFrameKind::IncrementalUpdate) {
            transportClosed(); return ClientReplicationResult::Reconnecting;
        }
        const std::uint64_t authoritativeProducedAt = frame->snapshot
                ? frame->snapshot->authoritativeProducedAt : frame->update->authoritativeProducedAt;
        if ((requireAuthoritativeTime && authoritativeProducedAt == 0)
            || authoritativeProducedAt > MaximumAuthoritativeTimestamp
            || (authoritativeProducedAt != 0 && latestAuthoritativeProducedAt != 0
                && authoritativeProducedAt < latestAuthoritativeProducedAt)) {
            transportClosed(); return ClientReplicationResult::Reconnecting;
        }
        if (requireAuthoritativeTime && replicated.version() == 0 && authoritativeProducedAt != 0
            && (!localClockSynchronizedAt || !authoritativeClockAtSynchronization)) {
            if (frame->snapshot) {
                if (!pendingInitialSnapshot) {
                    auto reservation = reservePendingInitialPayload(payload.size());
                    if (!reservation) {
                        transportClosed();
                        return ClientReplicationResult::Reconnecting;
                    }
                    pendingInitialSnapshot = *frame->snapshot;
                    pendingInitialSnapshotAcceptedAt = acceptedAt;
                    pendingInitialSnapshotBudgetReservation = std::move(reservation);
                    pendingInitialSnapshotReservedBytes = payload.size();
                    pendingInitialPayloadBytes += payload.size();
                } else {
                    if (pendingInitialFrames.size() >= MaxQueuedTransportFrames - 1u) {
                        transportClosed();
                        return ClientReplicationResult::Reconnecting;
                    }
                    auto reservation = reservePendingInitialPayload(payload.size());
                    if (!reservation) {
                        transportClosed();
                        return ClientReplicationResult::Reconnecting;
                    }
                    pendingInitialFrames.push_back(
                            {payload, acceptedAt, std::move(reservation), payload.size()});
                    pendingInitialPayloadBytes += payload.size();
                }
                return ClientReplicationResult::Applied;
            }
            if (pendingInitialSnapshot) {
                if (pendingInitialFrames.size() >= MaxQueuedTransportFrames - 1u) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                auto reservation = reservePendingInitialPayload(payload.size());
                if (!reservation) {
                    transportClosed();
                    return ClientReplicationResult::Reconnecting;
                }
                pendingInitialFrames.push_back(
                        {payload, acceptedAt, std::move(reservation), payload.size()});
                pendingInitialPayloadBytes += payload.size();
                return ClientReplicationResult::WaitingForSnapshot;
            }
            if (!allowOutboundExchange) {
                replicated.requireResynchronization();
                beginResynchronization();
                requestPending = true;
                deferredFullSnapshotRequest = true;
                return ClientReplicationResult::WaitingForSnapshot;
            }
            replicated.requireResynchronization();
            beginResynchronization();
            return requestFullSnapshot();
        }
        std::optional<std::chrono::milliseconds> authoritativeAge;
        const Replication::StateVersion incomingVersion = frame->snapshot
                ? frame->snapshot->version : frame->update->version;
        if (authoritativeProducedAt != 0
            && localClockSynchronizedAt && authoritativeClockAtSynchronization) {
            const auto plausibleProductionTime = authoritativeProductionTimeIsPlausible(
                    authoritativeProducedAt, acceptedAt);
            if (plausibleProductionTime && !*plausibleProductionTime) {
                if (frame->snapshot) {
                    if (allowOutboundExchange) signalFullSnapshotRequest();
                    return ClientReplicationResult::WaitingForSnapshot;
                }
                return ClientReplicationResult::WaitingForSnapshot;
            }
            authoritativeAge = authoritativeStateAge(authoritativeProducedAt, acceptedAt);
            if (!authoritativeAge || !quality.canObserveCanonicalState(
                    incomingVersion, *authoritativeAge, acceptedAt)) {
                transportClosed();
                return ClientReplicationResult::Reconnecting;
            }
        } else if (authoritativeProducedAt != 0) {
            if (!quality.canObserveCanonicalVersion(incomingVersion, acceptedAt)) {
                transportClosed();
                return ClientReplicationResult::Reconnecting;
            }
        } else if (!quality.canObserveCanonicalState(
                incomingVersion, std::chrono::milliseconds::zero(), acceptedAt)) {
            transportClosed();
            return ClientReplicationResult::Reconnecting;
        }
        const ApplyResult applied = frame->snapshot ? replicated.apply(*frame->snapshot) : replicated.apply(*frame->update);
        if (applied == ApplyResult::Applied) {
            const auto *state = replicated.state();
            if (!state || !movement.accept(replicated.version(), *state, acceptedAt)) {
                replicated.requireResynchronization();
                beginResynchronization();
            } else {
                if (authoritativeProducedAt != 0) {
                    latestAuthoritativeProducedAt = authoritativeProducedAt;
                    if (authoritativeAge) {
                        (void) quality.observeCanonicalState(
                                replicated.version(), *authoritativeAge, acceptedAt);
                        pendingAuthoritativeProducedAt.reset();
                        pendingCanonicalAcceptedAt.reset();
                    } else {
                        (void) quality.observeCanonicalVersion(replicated.version(), acceptedAt);
                        pendingAuthoritativeProducedAt = authoritativeProducedAt;
                        pendingCanonicalAcceptedAt = acceptedAt;
                    }
                } else {
                    (void) quality.observeCanonicalState(replicated.version(), acceptedAt);
                }
                requestPending = false;
                return ClientReplicationResult::Applied;
            }
        }
        if (applied == ApplyResult::WaitingForSnapshot) return ClientReplicationResult::WaitingForSnapshot;
        if (replicated.resynchronizationRequired()) {
            if (!requestPending) {
                beginResynchronization();
                if (!allowOutboundExchange) {
                    requestPending = true;
                    deferredFullSnapshotRequest = true;
                } else {
                    return requestFullSnapshot();
                }
            }
            return ClientReplicationResult::WaitingForSnapshot;
        }
        transportClosed(); return ClientReplicationResult::Reconnecting;
    }

    bool ClientReplicationConnection::observeNetworkSample(
            const Responsiveness::NetworkSample &sample,
            Responsiveness::TimePoint observedAt) noexcept {
        return quality.observeNetworkSample(sample, observedAt);
    }

    bool ClientReplicationConnection::sampleNetwork(Responsiveness::TimePoint now) {
        if (reconnecting) return false;
        if (qualityProbeSentAt) {
            const auto elapsed = elapsedMilliseconds(*qualityProbeSentAt, now);
            if (!elapsed) {
                transportClosed();
                return false;
            }
            if (*elapsed < Responsiveness::QualityProbeDeadline) return true;
            const auto deadline = addMilliseconds(
                    *qualityProbeSentAt, Responsiveness::QualityProbeDeadline);
            if (!deadline) {
                transportClosed();
                return false;
            }
            recordQualityOutcome(true, quality.currentRoundTripLatency().value_or(
                    Responsiveness::QualityProbeDeadline),
                    *deadline);
            qualityProbeSentAt.reset();
        }
        if (lastQualityProbeAt) {
            const auto elapsed = elapsedMilliseconds(*lastQualityProbeAt, now);
            if (!elapsed) {
                transportClosed();
                return false;
            }
            if (*elapsed < Responsiveness::QualityProbeInterval) return true;
        }
        if (qualityProbeSequence == std::numeric_limits<std::uint64_t>::max()) {
            transportClosed();
            return false;
        }
        ++qualityProbeSequence;
        try {
            if (!sender || sender(serializeQualityProbe(qualityProbeSequence)) != SendResult::Accepted) {
                transportClosed();
                return false;
            }
        } catch (...) {
            transportClosed();
            return false;
        }
        qualityProbeSentAt = now;
        lastQualityProbeAt = now;
        return true;
    }

    void ClientReplicationConnection::recordQualityOutcome(
            bool lost, std::chrono::milliseconds roundTripLatency,
            Responsiveness::TimePoint observedAt) noexcept {
        constexpr std::size_t QualityWindowSize = 4;
        qualityProbeOutcomes.push_back(lost);
        if (qualityProbeOutcomes.size() > QualityWindowSize) qualityProbeOutcomes.pop_front();
        if (lost) ++unansweredQualityProbeCount;
        const auto losses = static_cast<std::uint64_t>(std::count(
                qualityProbeOutcomes.begin(), qualityProbeOutcomes.end(), true));
        (void) quality.observeNetworkSample({roundTripLatency,
                static_cast<std::uint64_t>(qualityProbeOutcomes.size()), losses}, observedAt);
    }

    std::optional<std::chrono::milliseconds> ClientReplicationConnection::authoritativeStateAge(
            std::uint64_t producedAt, Responsiveness::TimePoint acceptedAt) const noexcept {
        const auto authoritativeAcceptedAt = authoritativeTimeAt(acceptedAt);
        if (!authoritativeAcceptedAt) return std::nullopt;
        if (producedAt >= *authoritativeAcceptedAt) return std::chrono::milliseconds::zero();
        const auto age = *authoritativeAcceptedAt - producedAt;
        const auto result = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(age));
        if (!canSubtractMilliseconds(acceptedAt, result)) return std::nullopt;
        return result;
    }

    std::optional<bool> ClientReplicationConnection::authoritativeProductionTimeIsPlausible(
            std::uint64_t producedAt, Responsiveness::TimePoint acceptedAt) const noexcept {
        const auto authoritativeAcceptedAt = authoritativeTimeAt(acceptedAt);
        if (!authoritativeAcceptedAt) return std::nullopt;
        if (producedAt <= *authoritativeAcceptedAt) return true;
        return producedAt - *authoritativeAcceptedAt <= authoritativeClockUncertainty;
    }

    std::optional<std::uint64_t> ClientReplicationConnection::authoritativeTimeAt(
            Responsiveness::TimePoint localTime) const noexcept {
        if (!localClockSynchronizedAt || !authoritativeClockAtSynchronization) return std::nullopt;
        std::uint64_t authoritativeTime = *authoritativeClockAtSynchronization;
        if (localTime >= *localClockSynchronizedAt) {
            const auto elapsed = elapsedMilliseconds(*localClockSynchronizedAt, localTime);
            if (!elapsed || static_cast<std::uint64_t>(elapsed->count())
                > MaximumAuthoritativeTimestamp - authoritativeTime)
                return std::nullopt;
            authoritativeTime += static_cast<std::uint64_t>(elapsed->count());
        } else {
            const auto elapsed = elapsedMilliseconds(localTime, *localClockSynchronizedAt);
            if (!elapsed || static_cast<std::uint64_t>(elapsed->count()) > authoritativeTime)
                return std::nullopt;
            authoritativeTime -= static_cast<std::uint64_t>(elapsed->count());
        }
        return authoritativeTime;
    }

    void ClientReplicationConnection::setLocallyControlledPlayers(std::set<Identity> playerIds) {
        movement.setLocallyControlledPlayers(std::move(playerIds));
    }

    bool ClientReplicationConnection::predictLocalMovement(
            const Responsiveness::PresentedPlayerPose &pose,
            Responsiveness::TimePoint sampledAt) noexcept {
        return movement.predictLocalMovement(pose, sampledAt);
    }

    Responsiveness::ConnectionPresentationState ClientReplicationConnection::presentationState(
            Responsiveness::TimePoint now) noexcept {
        return quality.update(now);
    }

    std::vector<Responsiveness::PresentedPlayerPose> ClientReplicationConnection::presentedPlayers(
            Responsiveness::TimePoint now) noexcept {
        return movement.sample(now);
    }

    std::vector<PresentationEvent> ClientReplicationConnection::takePresentationEvents() {
        return replicated.takePresentationEvents();
    }

    void ClientReplicationConnection::resumeOutboundProcessing() {
        if (reconnecting || !deferredFullSnapshotRequest) return;
        deferredFullSnapshotRequest = false;
        try {
            if (!sender || sender(serializeResynchronizationRequest()) != SendResult::Accepted)
                transportClosed();
        } catch (...) { transportClosed(); }
    }

    void ClientReplicationConnection::beginResynchronization() noexcept {
        quality.beginResynchronization();
        movement.beginResynchronization();
    }

    std::shared_ptr<void> ClientReplicationConnection::reservePendingInitialPayload(std::size_t bytes) {
        if (bytes > MaxQueuedTransportPayloadBytes
            || pendingInitialPayloadBytes > MaxQueuedTransportPayloadBytes - bytes
            || !Trust::processQueueBudget().reserve(bytes)) return {};
        try {
            return std::shared_ptr<void>(new std::size_t(bytes), [](void *reservation) {
                const auto amount = *static_cast<std::size_t *>(reservation);
                delete static_cast<std::size_t *>(reservation);
                Trust::processQueueBudget().release(amount);
            });
        } catch (...) {
            Trust::processQueueBudget().release(bytes);
            throw;
        }
    }

    void ClientReplicationConnection::releasePendingInitialPayload(std::size_t bytes) noexcept {
        pendingInitialPayloadBytes = bytes > pendingInitialPayloadBytes
                                     ? 0 : pendingInitialPayloadBytes - bytes;
    }

    void ClientReplicationConnection::clearPendingInitialPayloads() noexcept {
        pendingInitialSnapshot.reset();
        pendingInitialSnapshotAcceptedAt.reset();
        pendingInitialSnapshotBudgetReservation.reset();
        pendingInitialSnapshotReservedBytes = 0;
        pendingInitialFrames.clear();
        pendingInitialPayloadBytes = 0;
    }

    ClientReplicationResult ClientReplicationConnection::requestFullSnapshot(bool replacePendingRequest) {
        if (requestPending && !replacePendingRequest)
            return ClientReplicationResult::WaitingForSnapshot;
        requestPending = true;
        try {
            if (!sender || sender(serializeResynchronizationRequest()) != SendResult::Accepted) {
                transportClosed();
                return ClientReplicationResult::SendFailed;
            }
        } catch (...) {
            transportClosed();
            return ClientReplicationResult::SendFailed;
        }
        return ClientReplicationResult::WaitingForSnapshot;
    }

    void ClientReplicationConnection::signalFullSnapshotRequest() noexcept {
        try {
            if (sender) (void) sender(serializeResynchronizationRequest());
        } catch (...) {}
    }

    void ClientReplicationConnection::transportClosed() noexcept {
        reconnecting = true;
        requestPending = false;
        deferredFullSnapshotRequest = false;
        clearPendingInitialPayloads();
        replicated.requireResynchronization();
        movement.beginResynchronization();
        quality.transportClosed();
    }
    const ReplicatedState &ClientReplicationConnection::replicatedState() const noexcept { return replicated; }
    const CanonicalState *ClientReplicationConnection::initialAdmissionState() const noexcept {
        return acceptedInitialAdmissionState ? &*acceptedInitialAdmissionState : nullptr;
    }
}
