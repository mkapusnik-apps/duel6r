#include "AuthoritativeHostedMatchController.h"

#include <set>
#include <utility>

#include "AuthoritativeMatchValidation.h"

namespace Duel6::Server::Authoritative {
    std::map<Identity, bool> AuthoritativeHostedMatchController::replicatedReadiness(
            const std::vector<Network::Replication::ParticipantState> &participants) {
        std::map<Identity, bool> result;
        for (const auto &participant: participants) result.emplace(participant.participantId, participant.ready);
        return result;
    }

    AuthoritativeHostedMatchController::AuthoritativeHostedMatchController(
            Identity hostParticipantId, MatchRuntimeDependencies dependencies)
            : dependencies(std::move(dependencies)), hostParticipantId(hostParticipantId),
              replication(), replicationConnections(replication.replicator()), playerInput(hostParticipantId) {}

    bool AuthoritativeHostedMatchController::initializeReplication(
            std::vector<Network::Replication::ParticipantState> participants,
            std::vector<PlayerDefinition> roster, MatchConfig settings) {
        if (currentStage != HostedMatchStage::ServiceStarting) return false;
        auto nextReadiness = replicatedReadiness(participants);
        if (!replication.setLobby(hostParticipantId, std::move(participants),
                                  std::move(roster), std::move(settings))) return false;
        readiness = std::move(nextReadiness);
        return true;
    }

    bool AuthoritativeHostedMatchController::restoreReplication(
            Identity participantId, Network::Replication::ReplicationSender sender,
            std::function<void()> close) {
        return replicationConnections.restore(participantId, std::move(sender), std::move(close));
    }

    bool AuthoritativeHostedMatchController::updateReplicationLobby(
            std::vector<Network::Replication::ParticipantState> participants,
            std::vector<PlayerDefinition> roster, MatchConfig settings) {
        if (currentStage != HostedMatchStage::Lobby) return false;
        if (explicitReadinessRequired) {
            for (auto &participant: participants) {
                const auto found = readiness.find(participant.participantId);
                participant.ready = found != readiness.end() && found->second;
            }
        }
        auto nextReadiness = replicatedReadiness(participants);
        const auto update = replication.updateLobby(
                std::move(participants), std::move(roster), std::move(settings));
        if (!update) return false;
        readiness = std::move(nextReadiness);
        (void) replicationConnections.broadcast(*update);
        return true;
    }

    void AuthoritativeHostedMatchController::disconnectReplication(Identity participantId) noexcept {
        replicationConnections.disconnect(participantId);
    }

    bool AuthoritativeHostedMatchController::restorePlayerInput(
            Identity participantId, AuthoritativePlayerInput::Sender sender, std::function<void()> close) {
        return playerInput.restore(participantId, std::move(sender), std::move(close));
    }

    void AuthoritativeHostedMatchController::disconnectPlayerInput(Identity participantId) noexcept {
        playerInput.disconnect(participantId);
    }

    void AuthoritativeHostedMatchController::revokePlayerInput(Identity playerId) noexcept {
        playerInput.revokePlayer(playerId);
    }

    AuthoritativePlayerInput::ReceiveResult AuthoritativeHostedMatchController::receivePlayerInput(
            Identity participantId, const Network::Input::Command &command, bool remote) {
        return playerInput.receive(participantId, command, remote);
    }

    bool AuthoritativeHostedMatchController::updateReplicationConnection(
            Identity participantId, Network::Replication::ConnectionState connection) {
        const auto update = replication.setParticipantConnection(participantId, connection);
        if (!update) return false;
        (void) replicationConnections.broadcast(*update);
        return true;
    }

    Network::Replication::HostReplicationResult AuthoritativeHostedMatchController::receiveReplication(
            Identity participantId, const std::vector<std::uint8_t> &payload) {
        return replicationConnections.receive(participantId, payload);
    }

    bool AuthoritativeHostedMatchController::captureReplication() {
        if (!activeMatch || currentStage != HostedMatchStage::MatchActive) return false;
        const auto update = replication.capture(*activeMatch);
        if (!update) return false;
        (void) replicationConnections.broadcast(*update);
        return true;
    }

    bool AuthoritativeHostedMatchController::markServiceReady() {
        if (currentStage != HostedMatchStage::ServiceStarting) return false;
        currentStage = HostedMatchStage::Lobby;
        return true;
    }

    bool AuthoritativeHostedMatchController::setParticipantReady(Identity participantId, bool ready) {
        if (currentStage != HostedMatchStage::Lobby || participantId == 0) return false;
        const auto previous = readiness.find(participantId);
        const bool hadPrevious = previous != readiness.end();
        const bool previousValue = hadPrevious && previous->second;
        readiness[participantId] = ready;
        if (replication.replicator().version() != 0) {
            const auto update = replication.setParticipantReady(participantId, ready);
            if (!update) {
                if (hadPrevious) readiness[participantId] = previousValue;
                else readiness.erase(participantId);
                return false;
            }
            (void) replicationConnections.broadcast(*update);
        }
        return true;
    }

    void AuthoritativeHostedMatchController::clearReadiness() noexcept {
        for (auto &entry: readiness) entry.second = false;
    }

    bool AuthoritativeHostedMatchController::allParticipantsReady(
            const std::vector<PlayerDefinition> &roster) const noexcept {
        std::set<Identity> participants;
        for (const auto &player: roster) participants.insert(player.participantId);
        if (participants.empty()) return false;
        for (const Identity participant: participants) {
            const auto found = readiness.find(participant);
            if (found == readiness.end() || !found->second) return false;
        }
        return true;
    }

    TerminalOutcome AuthoritativeHostedMatchController::start(const MatchConfig &config,
            const std::vector<PlayerDefinition> &roster, const Network::GameplayManifest &manifest) {
        return start(config, roster, manifest, dependencies);
    }

    TerminalOutcome AuthoritativeHostedMatchController::start(const MatchConfig &config,
            const std::vector<PlayerDefinition> &roster, const Network::GameplayManifest &manifest,
            MatchRuntimeDependencies matchDependencies) {
        if (currentStage == HostedMatchStage::ContentBlocked)
            return terminalOutcome(OutcomeCode::ContentUnavailable);
        if (currentStage != HostedMatchStage::Lobby || activeMatch)
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        if (hostParticipantId == 0 || config.hostParticipantId != hostParticipantId)
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        const ValidationResult settings = validateMatchConfig(config, roster);
        if (!settings.valid || !allParticipantsReady(roster)) {
            clearReadiness();
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        }
        const ValidationResult content = validateFrozenContent(config, manifest);
        if (!content.valid) {
            clearReadiness();
            currentStage = HostedMatchStage::ContentBlocked;
            return terminalOutcome(OutcomeCode::ContentUnavailable);
        }
        activeMatch = std::make_unique<AuthoritativeMatch>(std::move(matchDependencies));
        const TerminalOutcome started = activeMatch->start(config, roster, manifest);
        if (started.code == OutcomeCode::None) {
            if (!playerInput.beginMatch(*activeMatch, roster)) {
                activeMatch->shutdown();
                activeMatch.reset();
                currentStage = HostedMatchStage::UnexpectedStop;
                return terminalOutcome(OutcomeCode::RuntimeFailed);
            }
            if (replication.fullSnapshot()) {
                const auto update = replication.beginMatch(*activeMatch);
                if (!update) {
                    activeMatch->shutdown();
                    playerInput.clear();
                    activeMatch.reset();
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return terminalOutcome(OutcomeCode::RuntimeFailed);
                }
                (void) replicationConnections.broadcast(*update);
            }
            currentStage = HostedMatchStage::MatchActive;
            explicitReadinessRequired = false;
        }
        else if (started.code == OutcomeCode::RuntimeFailed) currentStage = HostedMatchStage::UnexpectedStop;
        else if (started.code == OutcomeCode::ContentUnavailable) {
            clearReadiness();
            currentStage = HostedMatchStage::ContentBlocked;
        }
        else currentStage = HostedMatchStage::Lobby;
        if (currentStage != HostedMatchStage::MatchActive) activeMatch.reset();
        return started;
    }

    TerminalOutcome AuthoritativeHostedMatchController::end(Identity participantId) {
        if (participantId == 0 || participantId != hostParticipantId)
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        if (currentStage == HostedMatchStage::MatchActive && activeMatch) {
            const ActionResult accepted = activeMatch->submitHostControl(participantId, ActionKind::EndSession);
            if (accepted != ActionResult::Accepted) return activeMatch->outcome();
            const TerminalOutcome stopped = activeMatch->shutdown();
            playerInput.clear();
            currentStage = stopped.code == OutcomeCode::ShutdownFailed
                           ? HostedMatchStage::UnexpectedStop : HostedMatchStage::Ended;
            return stopped;
        }
        if (currentStage == HostedMatchStage::Lobby || currentStage == HostedMatchStage::ContentBlocked) {
            currentStage = HostedMatchStage::Ended;
            return terminalOutcome(OutcomeCode::EndedIntentionally);
        }
        return activeMatch ? activeMatch->outcome() : terminalOutcome(OutcomeCode::RuntimeFailed);
    }

    bool AuthoritativeHostedMatchController::observeMatchOutcome() {
        if (!activeMatch || currentStage != HostedMatchStage::MatchActive) return false;
        if (activeMatch->outcome().code == OutcomeCode::RuntimeFailed
            || activeMatch->outcome().code == OutcomeCode::ShutdownFailed) {
            activeMatch->shutdown();
            playerInput.clear();
            currentStage = HostedMatchStage::UnexpectedStop;
            return false;
        } else if (activeMatch->outcome().code != OutcomeCode::None) {
            const TerminalOutcome stopped = activeMatch->shutdown();
            playerInput.clear();
            if (stopped.code == OutcomeCode::ShutdownFailed) {
                currentStage = HostedMatchStage::UnexpectedStop;
                return false;
            }
            else {
                const auto result = replication.capture(*activeMatch);
                if (!result) {
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return false;
                }
                (void) replicationConnections.broadcast(*result);
                const auto lobby = replication.enterFollowingLobby();
                if (!lobby) {
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return false;
                }
                (void) replicationConnections.broadcast(*lobby);
                clearReadiness();
                explicitReadinessRequired = true;
                activeMatch.reset();
                currentStage = HostedMatchStage::Lobby;
            }
        } else {
            const auto update = replication.capture(*activeMatch);
            if (!update) return false;
            (void) replicationConnections.broadcast(*update);
        }
        return true;
    }

    bool AuthoritativeHostedMatchController::advanceOneTick() {
        return activeMatch && currentStage == HostedMatchStage::MatchActive && playerInput.processTick();
    }

    HostedMatchStage AuthoritativeHostedMatchController::stage() const noexcept { return currentStage; }
    bool AuthoritativeHostedMatchController::contentStartBlocked() const noexcept {
        return currentStage == HostedMatchStage::ContentBlocked;
    }
    bool AuthoritativeHostedMatchController::participantReady(Identity participantId) const noexcept {
        const auto found = readiness.find(participantId);
        return found != readiness.end() && found->second;
    }
    AuthoritativeMatch *AuthoritativeHostedMatchController::match() noexcept { return activeMatch.get(); }
    const AuthoritativeMatch *AuthoritativeHostedMatchController::match() const noexcept { return activeMatch.get(); }
}
