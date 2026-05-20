#ifndef DUEL6_CLIENT_LOOPBACKSESSION_H
#define DUEL6_CLIENT_LOOPBACKSESSION_H

#include "ConnectionPlan.h"
#include "../network/Protocol.h"
#include "../server/HeadlessServer.h"

namespace Duel6::Client {
    struct LoopbackConnectionResult {
        bool connected = false;
        bool localServerLaunched = false;
        Network::Endpoint endpoint;
        Network::HandshakeAccept accept;
        Network::HandshakeReject reject;
    };

    class LoopbackSession {
    private:
        Server::HeadlessServer &server;

    public:
        explicit LoopbackSession(Server::HeadlessServer &server);

        LoopbackConnectionResult connect(const ConnectionPlan &plan, const Network::HandshakeRequest &request);
    };
}

#endif
