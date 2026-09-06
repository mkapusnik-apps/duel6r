#ifndef DUEL6_SERVER_SERVERCONFIG_H
#define DUEL6_SERVER_SERVERCONFIG_H

#include <cstdint>
#include <string>
#include <vector>

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
        std::uint8_t localPlayers = 1;
        std::vector<std::string> enabledGameplayScripts;
        bool localOnly = false;
        bool transportEnabled = false;
        bool transportEcho = false;
        bool admissionClient = false;
        bool hostedServiceIpc = false;
        std::uint64_t hostedServiceParent = 0;
#ifdef D6R_TRANSPORT_WINDOWS
        std::uint64_t hostedServiceStatusHandle = 0;
        std::uint64_t hostedServiceControlHandle = 0;
#endif
    };

    ServerConfig parseServerConfig(int argc, char **argv);
}

#endif
