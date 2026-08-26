#include "ConnectionPlan.h"

#include <stdexcept>

namespace Duel6::Client {
    ConnectionPlan createConnectionPlan(const Network::ClientConnectionConfig &config) {
        if (!config.authToken.empty()) {
            throw std::invalid_argument(
                    "Authentication tokens are unsupported by the networking scaffold and will not be placed in process arguments");
        }

        ConnectionPlan plan;
        plan.config = config;

        if (config.mode == Network::ConnectionMode::LocalGame) {
            if (config.localServerExecutable.empty() || config.localEndpoint.host.empty()
                || config.localEndpoint.port == 0) {
                throw std::invalid_argument("Local server connection configuration is incomplete");
            }
            plan.launchesLocalServer = true;
            plan.endpoint = config.localEndpoint;
            plan.localServerArguments.push_back(config.localServerExecutable);
            plan.localServerArguments.push_back("--local-only");
            plan.localServerArguments.push_back("--host=" + config.localEndpoint.host);
            plan.localServerArguments.push_back("--port=" + std::to_string(config.localEndpoint.port));
        } else {
            if (config.remoteEndpoint.host.empty() || config.remoteEndpoint.port == 0) {
                throw std::invalid_argument("Remote server endpoint is incomplete");
            }
            plan.launchesLocalServer = false;
            plan.endpoint = config.remoteEndpoint;
        }

        return plan;
    }
}
