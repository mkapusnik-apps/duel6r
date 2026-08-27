#include "LoopbackSession.h"

#include <stdexcept>

namespace Duel6::Client {
    LoopbackSession::LoopbackSession(Server::HeadlessServer &server)
            : server(server) {}

    LoopbackConnectionResult LoopbackSession::connect(const ConnectionPlan &plan,
                                                      const Network::HandshakeRequest &request) {
        if (!plan.launchesLocalServer) {
            throw std::invalid_argument("Loopback session requires a local-game connection plan");
        }
        const Network::Endpoint &serverEndpoint = server.getConfig().listenEndpoint;
        if (plan.endpoint.host != serverEndpoint.host || plan.endpoint.port != serverEndpoint.port) {
            throw std::invalid_argument("Loopback connection plan endpoint does not match the in-process server endpoint");
        }

        LoopbackConnectionResult result;
        // This helper is an in-process protocol scaffold. It never starts a process.
        result.localServerLaunched = false;
        result.endpoint = serverEndpoint;

        const Server::HandshakeResult handshake = server.validateHandshake(request);
        if (!handshake.accepted) {
            result.reject = handshake.reject;
            return result;
        }

        result.connected = true;
        result.accept = server.acceptHandshake(request);
        if (result.accept.clientId == 0) {
            throw std::logic_error("Accepted loopback handshake did not assign a client ID");
        }
        return result;
    }
}
