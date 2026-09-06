#ifndef DUEL6_SERVER_AUTHORITATIVEMATCH_H
#define DUEL6_SERVER_AUTHORITATIVEMATCH_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "AuthoritativeMatchTypes.h"
#include "DeterministicRandom.h"
#include "../network/CompatibilityManifest.h"

namespace Duel6::Server::Authoritative {
    struct RoundStartDecision {
        std::uint8_t roundNumber = 0;
        std::string level;
        bool mirrored = false;
        Identity predatorPlayerId = 0;
        std::vector<Identity> rosterOrder;
        std::vector<std::uint32_t> startingWeaponIndices;
        std::vector<std::uint32_t> startingAmmo;
        std::vector<std::uint32_t> startingPositionOrder;
    };

    struct MatchRuntimeDependencies {
        std::function<std::uint64_t()> seedSource;
        std::function<bool(RoundStartDecision &)> worldStart;
        std::function<bool(RoundStartDecision &, RandomSource &)> worldStartWithRandom;
        std::function<bool(Tick, bool)> worldTick;
        std::function<bool(Tick, bool, RandomSource &)> worldTickWithRandom;
        std::function<bool(Identity, std::uint32_t)> worldInput;
        std::function<bool(Identity)> worldRemove;
        std::function<CanonicalWorldSnapshot()> worldSnapshot;
        std::function<void()> worldEnd;
        std::function<void(RandomSource &)> worldEndWithRandom;
        std::function<bool(const Network::GameplayManifest &)> contentPreflight;
        std::function<bool()> cleanup;
        std::function<std::vector<AuthoritativeAction>(Tick)> actionSource;
        std::function<Tick()> clock;
    };

    enum class ActionResult {
        Accepted,
        RejectedPhase,
        RejectedOrder,
        RejectedAuthority,
        RejectedValue,
        RejectedLimit,
        RuntimeFailed
    };

    class AuthoritativeMatch final {
    public:
        explicit AuthoritativeMatch(MatchRuntimeDependencies dependencies = {});
        ~AuthoritativeMatch();

        TerminalOutcome start(MatchConfig config, std::vector<PlayerDefinition> roster,
                              const Network::GameplayManifest &manifest);
        ActionResult submit(const AuthoritativeAction &action);
        ActionResult submitHostControl(Identity participantId, ActionKind kind, Identity targetPlayerId = 0);
        bool advanceOneTick();
        TerminalOutcome runUntilTerminal(Tick maximumTicks = MaxMatchTicks);
        TerminalOutcome shutdown();

        MatchPhase phase() const noexcept;
        Tick currentTick() const noexcept;
        const MatchConfig &frozenConfig() const noexcept;
        std::vector<PlayerDefinition> rosterDefinitions() const;
        std::uint32_t roundEndTicksRemaining() const noexcept;
        std::map<Identity, PlayerStatistics> playerStatistics() const;
        RoundResult currentRoundResult() const;
        const std::optional<SessionResult> &publishedResult() const noexcept;
        const TerminalOutcome &outcome() const noexcept;
        const RoundStartDecision &roundDecision() const noexcept;
        bool resourcesReleased() const noexcept;
        std::uint64_t currentStateDigest() const noexcept;
        const CanonicalWorldSnapshot *canonicalWorldSnapshot() const noexcept;
        const std::vector<CanonicalStateCheckpoint> &stateCheckpoints() const noexcept;
        std::uint64_t randomDecisionCount() const noexcept;
        std::uint64_t randomDecisionDigest() const noexcept;
        const std::vector<DeterministicRandom::Decision> &randomDecisionTrace() const noexcept;
        std::uint64_t acceptedActionCount() const noexcept;
        std::uint64_t rejectedActionCount() const noexcept;
        bool canAcceptPlayerInput(Identity participantId, Identity playerId) const noexcept;
        bool clearPlayerInput(Identity playerId) noexcept;

    private:
        struct AttackerRecord {
            std::uint64_t damage = 0;
        };
        struct PlayerState {
            PlayerDefinition definition;
            Team team = Team::None;
            bool alive = true;
            bool departed = false;
            std::int32_t life = MaximumLife;
            PlayerStatistics total;
            PlayerStatistics roundStart;
            std::map<Identity, AttackerRecord> attackers;
            Tick lastInputTick = static_cast<Tick>(-1);
            std::uint32_t inputMask = 0;
        };

        MatchRuntimeDependencies dependencies;
        MatchConfig config;
        std::vector<PlayerState> players;
        std::vector<std::string> shuffledLevels;
        std::unique_ptr<DeterministicRandom> random;
        MatchPhase currentPhase = MatchPhase::Lobby;
        Tick tick = 0;
        using SequenceDomain = std::tuple<std::uint8_t, Identity, Identity>;
        std::map<SequenceDomain, std::uint64_t> acceptedSequences;
        std::uint64_t internalHostControlSequence = 0;
        std::uint64_t totalActions = 0;
        std::uint64_t acceptedExternalActions = 0;
        std::uint64_t rejectedActions = 0;
        std::size_t actionsThisTick = 0;
        Tick actionTick = 0;
        std::uint32_t roundEndTicks = 0;
        std::uint8_t completedRoundCount = 0;
        Identity predatorPlayer = 0;
        bool currentRoundNoWinner = false;
        std::vector<Identity> currentRoundWinners;
        Team currentRoundWinningTeam = Team::None;
        RoundStartDecision currentRoundDecision;
        std::vector<RoundResult> completedRounds;
        std::vector<std::vector<PlayerStatistics>> completedPlayerStatistics;
        std::optional<SessionResult> result;
        TerminalOutcome terminal;
        bool worldActive = false;
        bool released = true;
        bool cleanupAttempted = false;
        bool contentStartBlocked = false;
        std::uint64_t latestStateDigest = 0;
        std::optional<CanonicalWorldSnapshot> latestCanonicalSnapshot;
        std::vector<CanonicalStateCheckpoint> checkpoints;

        PlayerState *findPlayer(Identity id);
        const PlayerState *findPlayer(Identity id) const;
        bool isHost(Identity participant) const;
        ActionResult submitImpl(const AuthoritativeAction &action, bool internalHostControl);
        ActionResult validateAction(const AuthoritativeAction &action, bool internalHostControl) const;
        ActionResult reject(ActionResult result) noexcept;
        bool accept(const AuthoritativeAction &action, bool internalHostControl) noexcept;
        SequenceDomain sequenceDomain(const AuthoritativeAction &action, bool internalHostControl) const noexcept;
        bool checkedAdd(std::uint64_t &target, std::uint64_t amount);
        bool startRound();
        bool recordRound();
        void finishRound();
        void evaluateRoundOutcome();
        void establishRoundOutcome(std::vector<Identity> winners, Team winningTeam, bool noWinner);
        bool applyPlayerInput(PlayerState &player, const AuthoritativeAction &action);
        bool synchronizeCanonicalWorld();
        bool applyShotDamage(PlayerState &source, PlayerState &target, std::int32_t amount);
        bool applyEnvironmentalDamage(PlayerState &target, std::int32_t amount);
        bool applyDeath(PlayerState &target, PlayerState *killer, bool environmental);
        bool awardAssists(PlayerState &target, PlayerState *killer, bool suicide);
        void interruptIfRosterTooSmall();
        bool publishResult(ResultState state);
        void failRuntime();
        void endWorld() noexcept;
    };
}

#endif
