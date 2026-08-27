#ifndef DUEL6_SERVER_SERVERCONFIG_H
#define DUEL6_SERVER_SERVERCONFIG_H

#include <cstdint>
#include <string>

#include "../network/Protocol.h"

namespace Duel6::Server {
    struct ServerConfig {
        std::string serverName = "Duel 6 Reloaded Server";
        std::string buildVersion = Network::PrototypeBuildVersion;
        Network::Endpoint listenEndpoint;
        std::string resourcePath = "resources";
        std::string authToken;
        std::uint32_t tickRate = 60;
        std::uint32_t maxClients = 15;
        bool localOnly = false;
        bool transportEnabled = false;
        bool transportEcho = false;
    };

    ServerConfig parseServerConfig(int argc, char **argv);
}

#endif
