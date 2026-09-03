#ifndef DUEL6_NETWORK_STATEREPLICATION_H
#define DUEL6_NETWORK_STATEREPLICATION_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Duel6::Network::Replication {
    using Identity = std::uint64_t;
    using StateVersion = std::uint64_t;

    constexpr std::size_t MaxReplicatedParticipants = 15;
    constexpr std::size_t MaxReplicatedPlayers = 15;
    constexpr std::size_t MaxReplicatedEntities = 100000;
    constexpr std::size_t MaxReplicatedEvents = 4096;
    constexpr std::size_t MaxReplicatedMessages = 256;
    constexpr std::size_t MaxReplicatedResultBytes = 1024 * 1024;
    constexpr std::size_t MaxReplicatedStringBytes = 4096;
    constexpr std::size_t MaxReplicatedLevels = 256;

    enum class IdentityCategory { Session, Match, Round, Participant, Player, WorldEntity, PresentationEvent };
    enum class ConnectionState { Connected, Reconnecting };
    enum class Phase { Lobby, ActiveRound, RoundSummary, FinalSummary, Ended };
    enum class EntityKind { Shot, Projectile, WeaponPickup, BonusPickup, Elevator, Hazard, Water, Tree, Fire, Explosion };
    enum class LifeState { Alive, Dead, Departed };
    enum class ChangeKind { Create, Update, Remove };

    class StableIdentitySource final {
    public:
        Identity issue(IdentityCategory category);
        bool wasIssued(IdentityCategory category, Identity identity) const noexcept;
    private:
        std::map<IdentityCategory, Identity> next;
    };

    struct ParticipantState {
        Identity participantId = 0;
        bool host = false;
        ConnectionState connection = ConnectionState::Connected;
        bool ready = false;
        std::vector<Identity> ownedPlayerIds;
    };

    struct MatchSettingsState {
        std::string mode;
        std::uint8_t teamCount = 0;
        bool friendlyFire = false;
        std::string levelPlan;
        std::string fixedLevel;
        std::vector<std::string> levels;
        std::uint8_t roundLimit = 0;
        bool assistance = false;
        bool quickLiquid = false;
        bool burnableTrees = true;
    };

    struct RoundOutcomeState {
        std::vector<Identity> winnerPlayerIds;
        std::uint8_t winningTeam = 0;
        bool noWinner = false;
    };

    struct RoundState {
        Identity roundId = 0;
        std::uint8_t roundNumber = 0;
        std::string level;
        bool mirrored = false;
        std::vector<Identity> rosterOrder;
        RoundOutcomeState outcome;
    };

    struct PlayerState {
        Identity playerId = 0;
        Identity ownerParticipantId = 0;
        std::uint8_t rosterPosition = 0;
        std::string displayName;
        std::uint8_t team = 0;
        LifeState lifeState = LifeState::Alive;
        std::int64_t positionX = 0;
        std::int64_t positionY = 0;
        std::int64_t velocityX = 0;
        std::int64_t velocityY = 0;
        bool facingLeft = false;
        bool crouching = false;
        std::int32_t life = 0;
        std::int64_t air = 0;
        std::string heldWeapon;
        std::int32_t ammunition = 0;
        std::uint32_t actionMask = 0;
        std::string activeBonus;
        std::int64_t bonusRemaining = 0;
        bool invulnerable = false;
        bool visible = true;
        std::int64_t reloadRemaining = 0;
        std::int64_t charge = 0;
        std::int64_t temporaryMovementRemaining = 0;
    };

    struct WorldEntityState {
        Identity entityId = 0;
        EntityKind kind = EntityKind::Projectile;
        Identity ownerPlayerId = 0;
        std::string type;
        std::int64_t positionX = 0;
        std::int64_t positionY = 0;
        std::int64_t velocityX = 0;
        std::int64_t velocityY = 0;
        std::int64_t primaryValue = 0;
        std::int64_t secondaryValue = 0;
        bool active = true;
        std::string lifecycle;
    };

    struct ScoreRowState {
        Identity playerId = 0;
        std::int64_t roundPoints = 0;
        std::int64_t cumulativePoints = 0;
        std::uint64_t shots = 0;
        std::uint64_t hits = 0;
        std::uint64_t kills = 0;
        std::uint64_t deaths = 0;
        std::uint64_t assists = 0;
        std::uint64_t wins = 0;
        std::uint64_t penalties = 0;
        std::uint64_t survivalTicks = 0;
        std::uint64_t damage = 0;
        std::uint64_t assistedDamage = 0;
    };

    struct ScoreState {
        std::vector<ScoreRowState> players;
        std::vector<Identity> ranking;
        std::vector<std::int64_t> teamTotals;
        std::vector<std::uint8_t> teamRanking;
        RoundOutcomeState winner;
    };

    struct MessageState {
        std::string status;
        std::vector<std::string> events;
        std::vector<Identity> currentPlayerIndicators;
        std::uint64_t roundProgress = 0;
        bool scoreSummaryVisible = false;
    };

    struct ContinuingEffectState {
        Identity effectId = 0;
        std::string type;
        Identity playerId = 0;
        Identity entityId = 0;
        std::int64_t remaining = 0;
    };

    struct PresentationEvent {
        Identity eventId = 0;
        std::string type;
        Identity playerId = 0;
        Identity targetPlayerId = 0;
        Identity entityId = 0;
        std::int64_t value = 0;
    };

    struct ResultState {
        bool available = false;
        bool sessionOnly = true;
        std::string state;
        std::string serialized;
    };

    struct CanonicalState {
        Identity sessionId = 0;
        Identity matchId = 0;
        Identity hostParticipantId = 0;
        Phase phase = Phase::Lobby;
        std::uint8_t currentRoundNumber = 0;
        std::uint8_t completedRounds = 0;
        std::uint64_t phaseTime = 0;
        std::uint64_t roundEndCountdown = 0;
        std::vector<ParticipantState> participants;
        MatchSettingsState settings;
        std::optional<RoundState> round;
        std::vector<PlayerState> players;
        std::vector<WorldEntityState> entities;
        ScoreState score;
        MessageState messages;
        std::vector<ContinuingEffectState> effects;
        ResultState result;
    };

    struct FullSnapshot {
        StateVersion version = 0;
        CanonicalState state;
    };

    template<typename T>
    struct EntityChange {
        ChangeKind kind = ChangeKind::Update;
        Identity identity = 0;
        std::optional<T> value;
    };

    struct IncrementalUpdate {
        Identity sessionId = 0;
        Identity matchId = 0;
        StateVersion baseline = 0;
        StateVersion version = 0;
        Phase phase = Phase::Lobby;
        std::uint8_t currentRoundNumber = 0;
        std::uint8_t completedRounds = 0;
        std::uint64_t phaseTime = 0;
        std::uint64_t roundEndCountdown = 0;
        std::vector<EntityChange<ParticipantState>> participants;
        MatchSettingsState settings;
        std::optional<RoundState> round;
        std::vector<EntityChange<PlayerState>> players;
        std::vector<EntityChange<WorldEntityState>> entities;
        ScoreState score;
        MessageState messages;
        std::vector<ContinuingEffectState> effects;
        ResultState result;
        std::vector<PresentationEvent> events;
    };

    enum class ApplyResult { Applied, Invalid, ResynchronizationRequired, WaitingForSnapshot };

    bool validateCanonicalState(const CanonicalState &state) noexcept;

    class AuthoritativeStateReplicator final {
    public:
        bool initialize(CanonicalState state);
        std::optional<IncrementalUpdate> publish(CanonicalState state,
                                                 std::vector<PresentationEvent> events = {});
        std::optional<FullSnapshot> fullSnapshot() const;
        StateVersion version() const noexcept;
    private:
        StateVersion currentVersion = 0;
        std::optional<CanonicalState> current;
        Identity highestEmittedEvent = 0;
        Identity removedParticipantHighWatermark = 0;
        Identity removedPlayerHighWatermark = 0;
        Identity removedEntityHighWatermark = 0;
    };

    class ReplicatedState final {
    public:
        ApplyResult apply(const FullSnapshot &snapshot);
        ApplyResult apply(const IncrementalUpdate &update);
        void requireResynchronization() noexcept;
        bool resynchronizationRequired() const noexcept;
        bool current() const noexcept;
        StateVersion version() const noexcept;
        const CanonicalState *state() const noexcept;
        std::vector<PresentationEvent> takePresentationEvents();
    private:
        std::optional<CanonicalState> accepted;
        StateVersion acceptedVersion = 0;
        bool resynchronizing = false;
        Identity removedParticipantHighWatermark = 0;
        Identity removedPlayerHighWatermark = 0;
        Identity removedEntityHighWatermark = 0;
        Identity highestPresentedEvent = 0;
        std::vector<PresentationEvent> pendingEvents;
        ApplyResult rejectIncremental() noexcept;
    };
}

#endif
