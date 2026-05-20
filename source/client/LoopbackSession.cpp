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

        LoopbackConnectionResult result;
        result.localServerLaunched = true;
        result.endpoint = plan.endpoint;

        const Server::HandshakeResult handshake = server.validateHandshake(request);
        if (!handshake.accepted) {
            result.reject = handshake.reject;
            return result;
        }

        result.connected = true;
        result.accept = server.acceptHandshake(request);
        return result;
    }
}
