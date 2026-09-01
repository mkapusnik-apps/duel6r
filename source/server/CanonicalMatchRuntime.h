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
}

namespace Duel6::Server::Authoritative {
    class CanonicalMatchRuntime final : public std::enable_shared_from_this<CanonicalMatchRuntime> {
    public:
        CanonicalMatchRuntime(MatchConfig config, std::vector<PlayerDefinition> roster,
                               std::shared_ptr<const Network::FrozenGameplayContent> frozenContent);
        ~CanonicalMatchRuntime();

        static MatchRuntimeDependencies createDependencies(MatchConfig config,
                std::vector<PlayerDefinition> roster, const Network::ManifestBuildResult &content);

    private:
        MatchConfig config;
        std::vector<PlayerDefinition> roster;
        std::vector<PlayerDefinition> activeRoster;
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
        std::map<Identity, std::int32_t> previousLife;
        std::map<Identity, PlayerStatistics> previousStatistics;
        std::set<std::uint64_t> previousEntities;
        std::map<Identity, std::uint32_t> spawnIdentities;
        std::int32_t previousWaterLevel = 0;
        bool previousSuddenDeath = false;
        bool previousRoundOver = false;
        bool sourceEventsEnabled = false;

        bool startWorld(RoundStartDecision &decision);
        bool tickWorld(Tick tick, bool simulate);
        bool setPlayerInput(Identity playerId, std::uint32_t inputMask);
        bool removePlayer(Identity playerId);
        CanonicalWorldSnapshot snapshot();
        void appendEvent(std::string kind, std::uint64_t entityId = 0, Identity playerId = 0,
                          Identity targetPlayerId = 0, std::string valueCategory = {}, std::int64_t value = 0);
        void endWorld();
        bool cleanup();
        MatchRuntimeDependencies dependencies();
    };
}

#endif
