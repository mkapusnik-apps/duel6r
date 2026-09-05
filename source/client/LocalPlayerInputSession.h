#ifndef DUEL6_CLIENT_LOCALPLAYERINPUTSESSION_H
#define DUEL6_CLIENT_LOCALPLAYERINPUTSESSION_H

#include <functional>
#include <vector>

#include "../input/PlayerControls.h"
#include "../network/PlayerInputProtocol.h"

namespace Duel6::Client {
    struct OwnedLocalPlayerControls {
        Network::Input::Identity playerId = 0;
        std::reference_wrapper<const PlayerControls> controls;
    };

    // Adapts the application's existing keyboard/controller assignments to the
    // common host/guest command and transport path. Local Play never constructs it.
    class LocalPlayerInputSession final {
    public:
        LocalPlayerInputSession(Network::Input::Identity participantId,
                                std::vector<OwnedLocalPlayerControls> players,
                                Network::Input::ClientCommandSession::Sender sender);
        bool submit(Network::Input::Tick targetTick);
        bool receive(const std::vector<std::uint8_t> &payload);
        const Network::Input::ClientCommandSession &commands() const noexcept;

    private:
        std::vector<OwnedLocalPlayerControls> players;
        Network::Input::ClientCommandSession commandSession;
    };
}

#endif
