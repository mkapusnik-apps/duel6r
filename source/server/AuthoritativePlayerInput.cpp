#include "AuthoritativePlayerInput.h"

#include <algorithm>
#include <utility>

#include "../network/NetworkTrustPolicy.h"

namespace Duel6::Server::Authoritative {
    namespace {
        AuthoritativePlayerInput::TimePoint realNow() { return std::chrono::steady_clock::now(); }
    }

    AuthoritativePlayerInput::AuthoritativePlayerInput(
            Identity hostParticipantId, std::function<TimePoint()> clock)
            : hostParticipantId(hostParticipantId), clock(clock ? std::move(clock) : realNow) {}

    bool AuthoritativePlayerInput::beginMatch(
            AuthoritativeMatch &authoritativeMatch, const std::vector<PlayerDefinition> &roster) {
        if (hostParticipantId == 0 || roster.empty() || roster.size() > MaxPlayers) return false;
        clear();
        match = &authoritativeMatch;
        for (const auto &player: roster) {
            if (player.participantId == 0 || player.playerId == 0
                || !owners.emplace(player.playerId, player.participantId).second) {
                clear();
                return false;
            }
            highestSequences.emplace(player.playerId, 0);
        }
        return true;
    }

    bool AuthoritativePlayerInput::restore(
            Identity participantId, Sender sender, std::function<void()> close) {
        if (participantId == 0 || !sender) return false;
        connections[participantId] = {std::move(sender), std::move(close), participantId != hostParticipantId};
        return true;
    }

    void AuthoritativePlayerInput::clearParticipantInput(Identity participantId) noexcept {
        if (!match) return;
        for (const auto &owner: owners) if (owner.second == participantId) {
            for (auto iterator = pending.begin(); iterator != pending.end();) {
                iterator = iterator->first.second == owner.first ? pending.erase(iterator) : std::next(iterator);
            }
            (void) match->clearPlayerInput(owner.first);
        }
    }

    void AuthoritativePlayerInput::disconnect(Identity participantId) noexcept {
        connections.erase(participantId);
        clearParticipantInput(participantId);
    }

    void AuthoritativePlayerInput::revokePlayer(Identity playerId) noexcept {
        owners.erase(playerId);
        highestSequences.erase(playerId);
        playerRates.erase(playerId);
        for (auto iterator = pending.begin(); iterator != pending.end();) {
            iterator = iterator->first.second == playerId ? pending.erase(iterator) : std::next(iterator);
        }
        if (match) (void) match->clearPlayerInput(playerId);
    }

    std::int64_t AuthoritativePlayerInput::windowIndex(TimePoint value) noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch()).count();
    }

    bool AuthoritativePlayerInput::consume(RateWindow &window, std::int64_t index, std::size_t limit) {
        if (!window.index || *window.index != index) {
            window.index = index;
            window.used = 0;
        }
        if (window.used >= limit) return false;
        ++window.used;
        return true;
    }

    bool AuthoritativePlayerInput::send(
            Identity participantId, const Network::Input::Outcome &outcome) noexcept {
        const auto found = connections.find(participantId);
        if (found == connections.end()) return participantId == hostParticipantId;
        try { return found->second.sender(Network::Input::serializeOutcome(outcome)) == Network::SendResult::Accepted; }
        catch (...) { return false; }
    }

    bool AuthoritativePlayerInput::sendPolicyViolation(Identity participantId) noexcept {
        const auto found = connections.find(participantId);
        if (found == connections.end()) return participantId == hostParticipantId;
        try {
            return found->second.sender(Network::Input::serializeSessionPolicyViolation())
                   == Network::SendResult::Accepted;
        } catch (...) { return false; }
    }

    void AuthoritativePlayerInput::observeParticipantWindow(
            Identity participantId, std::int64_t index) noexcept {
        const auto previous = participantObservedWindow.find(participantId);
        if (previous == participantObservedWindow.end()) {
            participantOverLimitWindow.erase(participantId);
            participantObservedWindow[participantId] = index;
        } else if (previous->second != index) {
            if (previous->second + 1 != index) participantOverLimitWindow.erase(participantId);
            previous->second = index;
        }
    }

    AuthoritativePlayerInput::ReceiveResult AuthoritativePlayerInput::reject(
            Identity participantId, const Network::Input::Command &command,
            Network::Input::OutcomeCategory category, bool closeConnection) {
        if (category == Network::Input::OutcomeCategory::Unauthorized)
            (void) sendPolicyViolation(participantId);
        else
            (void) send(participantId, {category, command.playerId, command.sequence, 0});
        if (closeConnection) {
            const auto found = connections.find(participantId);
            try { if (found != connections.end() && found->second.close) found->second.close(); } catch (...) {}
        }
        return {category, closeConnection};
    }

    AuthoritativePlayerInput::ReceiveResult AuthoritativePlayerInput::receive(
            Identity connectionParticipantId, const Network::Input::Command &command, bool remote) {
        if (connectionParticipantId == 0 || command.participantId == 0 || command.playerId == 0)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Invalid);
        const auto owner = owners.find(command.playerId);
        if (command.participantId != connectionParticipantId
            || owner == owners.end() || owner->second != connectionParticipantId)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Unauthorized, true);
        if (connections.find(connectionParticipantId) == connections.end())
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Unavailable);
        const TimePoint now = clock();
        const std::int64_t window = windowIndex(now);
        observeParticipantWindow(connectionParticipantId, window);
        if (!consume(playerRates[command.playerId], window, Network::Trust::InputsPerOwnedSlotPerSecond)) {
            bool close = false;
            if (remote) {
                const auto previous = participantOverLimitWindow.find(connectionParticipantId);
                close = previous != participantOverLimitWindow.end() && previous->second + 1 == window;
                participantOverLimitWindow[connectionParticipantId] = window;
            }
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::OverLimit, close);
        }
        if (!match || command.sequence == 0 || (command.actions & ~Network::Input::AllActions) != 0)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Invalid);
        if (!match->canAcceptPlayerInput(connectionParticipantId, command.playerId))
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Unavailable);

        const Tick next = match->currentTick();
        const Tick earliest = next > 2 ? next - 2 : 0;
        if (command.targetTick < earliest)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Stale);
        if (command.targetTick > next && command.targetTick - next > 1)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::TooFuture);
        const auto sequence = highestSequences.find(command.playerId);
        if (sequence == highestSequences.end() || command.sequence <= sequence->second)
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::Duplicate);

        if (!consume(globalRate, window, Network::Trust::GlobalAcceptedInputsPerSecond)) {
            if (remote) {
                const auto previous = participantOverLimitWindow.find(connectionParticipantId);
                const bool consecutive = previous != participantOverLimitWindow.end()
                                         && previous->second + 1 == window;
                participantOverLimitWindow[connectionParticipantId] = window;
                return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::OverLimit,
                              consecutive);
            }
            return reject(connectionParticipantId, command, Network::Input::OutcomeCategory::OverLimit);
        }

        const Tick effective = command.targetTick <= next ? next : next + 1;
        auto key = std::make_pair(effective, command.playerId);
        auto existing = pending.find(key);
        if (existing != pending.end()) {
            (void) send(connectionParticipantId, {Network::Input::OutcomeCategory::Superseded,
                    existing->second.command.playerId, existing->second.command.sequence, effective});
            existing->second = {command, effective};
        } else pending.emplace(key, Pending{command, effective});
        sequence->second = command.sequence;
        (void) send(connectionParticipantId,
                    {Network::Input::OutcomeCategory::Pending, command.playerId, command.sequence, effective});
        return {Network::Input::OutcomeCategory::Pending, false};
    }

    bool AuthoritativePlayerInput::processTick() {
        if (!match) return false;
        const Tick effective = match->currentTick();
        std::vector<Pending> applying;
        for (auto iterator = pending.begin(); iterator != pending.end() && iterator->first.first <= effective;) {
            applying.push_back(iterator->second);
            iterator = pending.erase(iterator);
        }
        std::sort(applying.begin(), applying.end(), [](const auto &left, const auto &right) {
            return left.command.playerId < right.command.playerId;
        });
        std::vector<Pending> applied;
        for (const auto &input: applying) {
            if (!match->canAcceptPlayerInput(input.command.participantId, input.command.playerId)) {
                (void) send(input.command.participantId, {Network::Input::OutcomeCategory::Unavailable,
                        input.command.playerId, input.command.sequence, 0});
                continue;
            }
            AuthoritativeAction action;
            action.tick = effective;
            action.sequence = input.command.sequence;
            action.participantId = input.command.participantId;
            action.playerId = input.command.playerId;
            action.inputMask = input.command.actions;
            if (match->submit(action) != ActionResult::Accepted) {
                (void) send(input.command.participantId, {Network::Input::OutcomeCategory::Unavailable,
                        input.command.playerId, input.command.sequence, 0});
                continue;
            }
            applied.push_back(input);
        }
        if (!match->advanceOneTick()) return false;
        for (const auto &input: applied)
            (void) send(input.command.participantId, {Network::Input::OutcomeCategory::Applied,
                    input.command.playerId, input.command.sequence, input.effectiveTick});
        return true;
    }

    void AuthoritativePlayerInput::clear() noexcept {
        if (match) for (const auto &owner: owners) (void) match->clearPlayerInput(owner.first);
        match = nullptr;
        owners.clear();
        highestSequences.clear();
        pending.clear();
        playerRates.clear();
        globalRate = {};
        participantObservedWindow.clear();
        participantOverLimitWindow.clear();
    }
}
