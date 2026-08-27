#ifndef DUEL6_CLIENT_CONNECTIONPLAN_H
#define DUEL6_CLIENT_CONNECTIONPLAN_H

#include <string>
#include <vector>

#include "../network/Protocol.h"

namespace Duel6::Client {
    struct ConnectionPlan {
        Network::ClientConnectionConfig config;
        bool launchesLocalServer = false;
        Network::Endpoint endpoint;
        std::vector<std::string> localServerArguments;
    };

    ConnectionPlan createConnectionPlan(const Network::ClientConnectionConfig &config);
}

#endif
