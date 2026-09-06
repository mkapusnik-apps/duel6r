#include "LocalPlayerInputSession.h"

#include <stdexcept>
#include <utility>

namespace Duel6::Client {
    namespace {
        std::vector<Network::Input::Identity> playerIds(
                const std::vector<OwnedLocalPlayerControls> &players) {
            std::vector<Network::Input::Identity> result;
            result.reserve(players.size());
            for (const auto &player: players) result.push_back(player.playerId);
            return result;
        }

        Network::Input::PlayerActionState sample(const PlayerControls &controls) {
            Network::Input::PlayerActionState result;
            result.moveLeft = controls.getLeft().isPressed();
            result.moveRight = controls.getRight().isPressed();
            result.jump = controls.getUp().isPressed();
            result.crouch = controls.getDown().isPressed();
            result.shoot = controls.getShoot().isPressed();
            result.pickOrSwapWeapon = controls.getPick().isPressed();
            result.showStatus = controls.getStatus().isPressed();
            return result;
        }
    }

    LocalPlayerInputSession::LocalPlayerInputSession(
            Network::Input::Identity participantId, std::vector<OwnedLocalPlayerControls> players,
            Network::Input::ClientCommandSession::Sender sender)
            : players(std::move(players)),
              commandSession(participantId, playerIds(this->players), std::move(sender)) {
        if (this->players.empty()) throw std::invalid_argument("Owned local player controls are required");
    }

    bool LocalPlayerInputSession::submit(Network::Input::Tick targetTick) {
        for (const auto &player: players)
            if (!commandSession.submit(player.playerId, targetTick, sample(player.controls.get()))) return false;
        return true;
    }

    bool LocalPlayerInputSession::receive(const std::vector<std::uint8_t> &payload) {
        return commandSession.receive(payload);
    }

    const Network::Input::ClientCommandSession &LocalPlayerInputSession::commands() const noexcept {
        return commandSession;
    }
}
