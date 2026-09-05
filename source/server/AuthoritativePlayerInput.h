#ifndef DUEL6_SERVER_AUTHORITATIVEPLAYERINPUT_H
#define DUEL6_SERVER_AUTHORITATIVEPLAYERINPUT_H

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "AuthoritativeMatch.h"
#include "../network/PlayerInputProtocol.h"

namespace Duel6::Server::Authoritative {
    class AuthoritativePlayerInput final {
    public:
        using TimePoint = std::chrono::steady_clock::time_point;
        using Sender = std::function<Network::SendResult(std::vector<std::uint8_t>)>;
        struct ReceiveResult {
            Network::Input::OutcomeCategory category = Network::Input::OutcomeCategory::Invalid;
            bool closeConnection = false;
        };

        explicit AuthoritativePlayerInput(Identity hostParticipantId,
                                          std::function<TimePoint()> clock = {});
        bool beginMatch(AuthoritativeMatch &match, const std::vector<PlayerDefinition> &roster);
        bool restore(Identity participantId, Sender sender, std::function<void()> close = {});
        void disconnect(Identity participantId) noexcept;
        void revokePlayer(Identity playerId) noexcept;
        ReceiveResult receive(Identity connectionParticipantId, const Network::Input::Command &command,
                              bool remote = true);
        bool processTick();
        void clear() noexcept;
    private:
        struct RateWindow { TimePoint start{}; std::size_t used = 0; };
        struct ParticipantConnection { Sender sender; std::function<void()> close; bool remote = true; };
        struct Pending { Network::Input::Command command; Tick effectiveTick = 0; };

        Identity hostParticipantId;
        std::function<TimePoint()> clock;
        AuthoritativeMatch *match = nullptr;
        std::map<Identity, Identity> owners;
        std::map<Identity, std::uint64_t> highestSequences;
        std::map<std::pair<Tick, Identity>, Pending> pending;
        std::map<Identity, RateWindow> playerRates;
        RateWindow globalRate;
        std::map<Identity, ParticipantConnection> connections;
        std::map<Identity, std::int64_t> participantOverLimitWindow;
        std::set<std::pair<Identity, std::int64_t>> overLimitWindows;

        static std::int64_t windowIndex(TimePoint value) noexcept;
        static bool consume(RateWindow &window, TimePoint now, std::size_t limit);
        bool send(Identity participantId, const Network::Input::Outcome &outcome) noexcept;
        void clearParticipantInput(Identity participantId) noexcept;
        ReceiveResult reject(Identity participantId, const Network::Input::Command &command,
                             Network::Input::OutcomeCategory category, bool closeConnection = false);
    };
}

#endif
