#ifndef DUEL6_SERVER_AUTHORITATIVEMATCH_H
#define DUEL6_SERVER_AUTHORITATIVEMATCH_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
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
        std::function<bool(Tick, bool)> worldTick;
        std::function<bool(Identity, std::uint32_t)> worldInput;
        std::function<bool(Identity)> worldRemove;
        std::function<CanonicalWorldSnapshot()> worldSnapshot;
        std::function<void()> worldEnd;
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
        bool advanceOneTick();
        TerminalOutcome runUntilTerminal(Tick maximumTicks = MaxMatchTicks);
        TerminalOutcome shutdown();

        MatchPhase phase() const noexcept;
        Tick currentTick() const noexcept;
        const MatchConfig &frozenConfig() const noexcept;
        const std::optional<SessionResult> &publishedResult() const noexcept;
        const TerminalOutcome &outcome() const noexcept;
        const RoundStartDecision &roundDecision() const noexcept;
        bool resourcesReleased() const noexcept;
        std::uint64_t currentStateDigest() const noexcept;
        const CanonicalWorldSnapshot *canonicalWorldSnapshot() const noexcept;
        std::uint64_t randomDecisionCount() const noexcept;
        std::uint64_t randomDecisionDigest() const noexcept;
        const std::vector<DeterministicRandom::Decision> &randomDecisionTrace() const noexcept;

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
        };

        MatchRuntimeDependencies dependencies;
        MatchConfig config;
        std::vector<PlayerState> players;
        std::vector<std::string> shuffledLevels;
        std::unique_ptr<DeterministicRandom> random;
        MatchPhase currentPhase = MatchPhase::Lobby;
        Tick tick = 0;
        std::uint64_t lastSequence = 0;
        std::size_t totalActions = 0;
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

        PlayerState *findPlayer(Identity id);
        const PlayerState *findPlayer(Identity id) const;
        bool isHost(Identity participant) const;
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
