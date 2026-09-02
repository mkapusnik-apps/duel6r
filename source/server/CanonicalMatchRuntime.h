#ifndef DUEL6_SERVER_CANONICALMATCHRUNTIME_H
#define DUEL6_SERVER_CANONICALMATCHRUNTIME_H

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "AuthoritativeMatch.h"
#include "../network/CompatibilityManifest.h"

namespace Duel6 {
    class Game;
    class GameMode;
    class GameResources;
    class GameSettings;
    class Player;
}

namespace Duel6::Server::Authoritative {
    class CanonicalMatchRuntime final : public std::enable_shared_from_this<CanonicalMatchRuntime> {
    public:
        CanonicalMatchRuntime(MatchConfig config, std::vector<PlayerDefinition> roster,
                               Network::GameplayManifest frozenManifest,
                               std::shared_ptr<const Network::FrozenGameplayContent> frozenContent);
        ~CanonicalMatchRuntime();

        static MatchRuntimeDependencies createDependencies(MatchConfig config,
                std::vector<PlayerDefinition> roster, const Network::ManifestBuildResult &content);

    private:
        MatchConfig config;
        std::vector<PlayerDefinition> roster;
        std::vector<PlayerDefinition> activeRoster;
        std::map<Identity, Player *> canonicalPlayersById;
        std::map<Identity, std::uint32_t> heldInputsByPlayerId;
        std::set<Identity> departedPlayerIds;
        Network::GameplayManifest frozenManifest;
        std::shared_ptr<const Network::FrozenGameplayContent> frozenContent;
        std::unique_ptr<GameResources> resources;
        std::unique_ptr<GameSettings> settings;
        std::unique_ptr<GameMode> mode;
        std::unique_ptr<Game> game;
        bool initialized = false;
        Tick worldTick = 0;
        std::uint8_t authoritativeRound = 0;
        std::uint64_t nextEventSequence = 1;
        std::vector<CanonicalEvent> eventTrace;
        std::uint64_t nextTransitionSequence = 1;
        std::vector<CanonicalEvent> transitionTrace;
        std::map<Identity, std::int32_t> previousLife;
        std::map<Identity, PlayerStatistics> previousStatistics;
        std::set<std::uint64_t> previousEntities;
        std::map<std::uint64_t, std::pair<std::int64_t, std::int64_t>> previousElevatorVelocities;
        std::map<Identity, std::uint32_t> spawnIdentities;
        std::int32_t previousWaterLevel = 0;
        bool previousSuddenDeath = false;
        bool previousRoundOver = false;
        bool sourceEventsEnabled = false;

        bool preflightContent(const Network::GameplayManifest &manifest);
        bool startWorld(RoundStartDecision &decision, RandomSource &randomSource);
        bool tickWorld(Tick tick, bool simulate, RandomSource &randomSource);
        bool setPlayerInput(Identity playerId, std::uint32_t inputMask);
        bool removePlayer(Identity playerId);
        CanonicalWorldSnapshot snapshot();
        void appendEvent(std::string kind, std::uint64_t entityId = 0, Identity playerId = 0,
                          Identity targetPlayerId = 0, std::string valueCategory = {}, std::int64_t value = 0);
        void appendTransition(std::string kind, std::uint64_t entityId = 0, Identity playerId = 0,
                              Identity targetPlayerId = 0, std::string valueCategory = {},
                              std::int64_t value = 0);
        void endWorld(RandomSource *randomSource = nullptr);
        bool cleanup();
        MatchRuntimeDependencies dependencies();
    };
}

#endif
