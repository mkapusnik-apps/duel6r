#include "source/network/PlayerInputProtocol.h"
#include "ObservedPlayerInput.h"
#include <mutex>
#include <map>
#include <deque>
#include <algorithm>

namespace Duel6::Test::ObservedInput {
    namespace {
        struct Submitted {
            Network::Input::Command command;
            unsigned edge;
            std::optional<Network::Input::Outcome> outcome;
        };
        struct Session {
            std::map<Network::Input::Identity, std::deque<Submitted>> history;
        };
        std::mutex mutex;
        bool enabled = false;
        unsigned edge = 0;
        std::map<const Network::Input::ClientCommandSession *, Session> sessions;
    }
    void begin() { std::lock_guard<std::mutex> lock(mutex); enabled = true; edge = 0; sessions.clear(); }
    void end() { std::lock_guard<std::mutex> lock(mutex); enabled = false; sessions.clear(); }
    void secondEdge() { std::lock_guard<std::mutex> lock(mutex); edge = 2; }
    void releaseFirstEdge() { std::lock_guard<std::mutex> lock(mutex); edge = 3; }
    void submitted(const Network::Input::ClientCommandSession *source,
                   const std::optional<Network::Input::Command> &command) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!enabled || !command) return;
        auto &history = sessions[source].history[command->playerId];
        if (history.size() == 128) history.pop_front();
        history.push_back({*command, edge, {}});
    }
    void received(const Network::Input::ClientCommandSession *source,
                  const std::vector<std::uint8_t> &payload, bool consumed) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!enabled || !consumed) return;
        const auto frame = Network::Input::deserializeFrame(payload);
        if (!frame || !frame->outcome) return;
        auto session = sessions.find(source);
        if (session == sessions.end()) return;
        auto player = session->second.history.find(frame->outcome->playerId);
        if (player == session->second.history.end()) return;
        for (auto &entry: player->second)
            if (entry.command.sequence == frame->outcome->sequence) entry.outcome = frame->outcome;
    }
    std::optional<std::uint64_t> appliedSecondJump(Network::Input::Identity player, Network::Input::Identity participant) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto &[source, session]: sessions) {
            const auto found = session.history.find(player);
            if (found == session.history.end()) continue;
            bool released = false;
            for (const auto &entry: found->second) {
                if (!entry.outcome || entry.outcome->category != Network::Input::OutcomeCategory::Applied) continue;
                if (entry.edge == 3 && entry.command.actions == 0) released = true;
                if (released && entry.edge == 2 && entry.command.actions == Network::Input::Jump
                    && entry.command.participantId == participant)
                    return entry.outcome->effectiveTick;
            }
        }
        return {};
    }
}

using Session = Duel6::Network::Input::ClientCommandSession;
using Command = Duel6::Network::Input::Command;
using Identity = Duel6::Network::Input::Identity;
using Tick = Duel6::Network::Input::Tick;
std::optional<Command> realSubmit(Session *, Identity, Tick, std::uint32_t)
    __asm__("__real__ZN5Duel67Network5Input20ClientCommandSession6submitEmmj");
std::optional<Command> wrapSubmit(Session *, Identity, Tick, std::uint32_t)
    __asm__("__wrap__ZN5Duel67Network5Input20ClientCommandSession6submitEmmj");
std::optional<Command> wrapSubmit(Session *source, Identity player, Tick tick, std::uint32_t actions) {
    auto result = realSubmit(source, player, tick, actions);
    Duel6::Test::ObservedInput::submitted(source, result);
    return result;
}
bool realReceive(Session *, const std::vector<std::uint8_t> &)
    __asm__("__real__ZN5Duel67Network5Input20ClientCommandSession7receiveERKSt6vectorIhSaIhEE");
bool wrapReceive(Session *, const std::vector<std::uint8_t> &)
    __asm__("__wrap__ZN5Duel67Network5Input20ClientCommandSession7receiveERKSt6vectorIhSaIhEE");
bool wrapReceive(Session *source, const std::vector<std::uint8_t> &payload) {
    const bool result = realReceive(source, payload);
    Duel6::Test::ObservedInput::received(source, payload, result);
    return result;
}
