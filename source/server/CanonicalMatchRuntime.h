#ifndef DUEL6_SERVER_CANONICALMATCHRUNTIME_H
#define DUEL6_SERVER_CANONICALMATCHRUNTIME_H

#include <memory>
#include <string>
#include <vector>

#include "AuthoritativeMatch.h"

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
                              std::string resourcesPath);
        ~CanonicalMatchRuntime();

        static MatchRuntimeDependencies createDependencies(MatchConfig config,
                std::vector<PlayerDefinition> roster, std::string resourcesPath);

    private:
        MatchConfig config;
        std::vector<PlayerDefinition> roster;
        std::vector<PlayerDefinition> activeRoster;
        std::string resourcesPath;
        std::unique_ptr<GameResources> resources;
        std::unique_ptr<GameSettings> settings;
        std::unique_ptr<GameMode> mode;
        std::unique_ptr<Game> game;
        bool initialized = false;

        bool startWorld(RoundStartDecision &decision);
        bool tickWorld(Tick tick, bool simulate);
        bool setPlayerInput(Identity playerId, std::uint32_t inputMask);
        bool removePlayer(Identity playerId);
        CanonicalWorldSnapshot snapshot() const;
        void endWorld();
        bool cleanup();
        MatchRuntimeDependencies dependencies();
    };
}

#endif
