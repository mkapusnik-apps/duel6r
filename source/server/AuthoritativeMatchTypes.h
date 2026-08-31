#ifndef DUEL6_SERVER_AUTHORITATIVEMATCHTYPES_H
#define DUEL6_SERVER_AUTHORITATIVEMATCHTYPES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Duel6::Server::Authoritative {
    using Identity = std::uint64_t;
    using Tick = std::uint64_t;

    constexpr std::size_t MaxPlayers = 15;
    constexpr std::size_t MaxParticipants = 15;
    constexpr std::size_t MaxLevels = 256;
    constexpr std::size_t MaxActions = 65536;
    constexpr std::size_t MaxActionsPerTick = 64;
    constexpr std::size_t MaxResultBytes = 1024 * 1024;
    constexpr std::size_t MaxDisplayNameBytes = 64;
    constexpr Tick MaxMatchTicks = 60u * 60u * 60u * 24u;
    constexpr std::uint32_t FixedTickRate = 60;
    constexpr std::uint32_t RoundEndActiveTicks = FixedTickRate;
    constexpr std::uint32_t RoundEndFrozenTicks = FixedTickRate * 5u;
    constexpr std::uint32_t RoundEndTotalTicks = RoundEndActiveTicks + RoundEndFrozenTicks;
    constexpr std::int32_t MaximumLife = 100;

    enum class Mode { Deathmatch, Predator, TeamDeathmatch };
    enum class LevelPlan { Fixed, ShuffleAll, Random };
    enum class Team : std::uint8_t { None = 0, Alpha = 1, Bravo = 2, Charlie = 3, Delta = 4 };
    enum class ResultState { Completed, Interrupted };
    enum class MatchPhase { Lobby, ActiveRound, RoundEndActive, RoundEndFrozen, Completed, Failed, Ended };
    enum class OutcomeCode {
        None,
        Completed,
        InterruptedNoWinner,
        EndedIntentionally,
        SettingsInvalid,
        ContentUnavailable,
        RuntimeFailed,
        ShutdownFailed
    };

    enum class ActionKind {
        PlayerInput,
        ShotDamage,
        EnvironmentalDamage,
        RemovePlayer,
        AdvanceRound,
        EndSession,
        RuntimeFailure
    };

    enum PlayerInput : std::uint32_t {
        MoveLeft = 1u << 0u,
        MoveRight = 1u << 1u,
        Jump = 1u << 2u,
        Crouch = 1u << 3u,
        Shoot = 1u << 4u,
        PickOrSwapWeapon = 1u << 5u,
        ShowStatus = 1u << 6u
    };
    constexpr std::uint32_t AllPlayerInputs = MoveLeft | MoveRight | Jump | Crouch | Shoot
                                                    | PickOrSwapWeapon | ShowStatus;

    struct PlayerDefinition {
        Identity participantId = 0;
        Identity playerId = 0;
        std::string displayName;
        std::uint8_t rosterOrder = 0;
    };

    struct MatchConfig {
        Mode mode = Mode::Deathmatch;
        std::uint8_t teamCount = 0;
        bool friendlyFire = false;
        LevelPlan levelPlan = LevelPlan::Fixed;
        std::string fixedLevel;
        std::vector<std::string> playableLevels;
        std::vector<std::string> enabledWeapons;
        std::uint8_t roundLimit = 1;
        bool assistance = false;
        bool quickLiquid = false;
        bool burnableTrees = true;
        bool optionalScriptsEnabled = false;
        Identity hostParticipantId = 0;
        std::uint64_t seed = 0;
    };

    struct AuthoritativeAction {
        Tick tick = 0;
        std::uint64_t sequence = 0;
        Identity participantId = 0;
        Identity playerId = 0;
        ActionKind kind = ActionKind::PlayerInput;
        Identity targetPlayerId = 0;
        std::uint32_t inputMask = 0;
        std::int32_t amount = 0;
    };

    struct PlayerStatistics {
        std::uint64_t roundsPlayed = 0;
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

        std::int64_t totalPoints() const;
    };

    struct CanonicalPlayerSnapshot {
        Identity playerId = 0;
        bool alive = false;
        std::int32_t life = 0;
        std::int64_t positionX = 0;
        std::int64_t positionY = 0;
        PlayerStatistics statistics;
    };

    struct CanonicalWorldSnapshot {
        std::vector<CanonicalPlayerSnapshot> players;
        bool roundOver = false;
        bool valid = false;
        std::uint64_t stateDigest = 0;
        std::size_t dynamicEntityCount = 0;
    };

    struct PlayerResultRow {
        Identity playerId = 0;
        Identity participantId = 0;
        std::string displayName;
        Team team = Team::None;
        bool departed = false;
        std::uint8_t rosterOrder = 0;
        PlayerStatistics statistics;
        std::vector<PlayerStatistics> rounds;
    };

    struct TeamResultRow {
        Team team = Team::None;
        std::int64_t totalPoints = 0;
        std::vector<Identity> rankedPlayerIds;
    };

    struct RoundResult {
        std::uint8_t roundNumber = 0;
        std::string level;
        bool mirrored = false;
        std::vector<Identity> winnerPlayerIds;
        Team winningTeam = Team::None;
        bool noWinner = false;
        std::vector<Identity> rosterOrder;
    };

    struct SessionResult {
        std::string label = "Session only";
        ResultState state = ResultState::Completed;
        MatchConfig config;
        std::uint8_t completedRounds = 0;
        std::vector<RoundResult> rounds;
        std::vector<PlayerResultRow> players;
        std::vector<TeamResultRow> teams;
        std::vector<Identity> finalWinnerPlayerIds;
        Team finalWinningTeam = Team::None;
        bool finalNoWinner = false;
    };

    struct TerminalOutcome {
        OutcomeCode code = OutcomeCode::None;
        int exitStatus = 3;
        std::string identifier;
        std::string copy;
    };

    const char *modeName(Mode mode);
    const char *levelPlanName(LevelPlan plan);
    const char *teamName(Team team);
    const char *phaseName(MatchPhase phase);
    TerminalOutcome terminalOutcome(OutcomeCode code);
}

#endif
