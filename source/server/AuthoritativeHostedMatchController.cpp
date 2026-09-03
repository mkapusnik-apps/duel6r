#include "AuthoritativeHostedMatchController.h"

#include <set>
#include <utility>

#include "AuthoritativeMatchValidation.h"

namespace Duel6::Server::Authoritative {
    AuthoritativeHostedMatchController::AuthoritativeHostedMatchController(
            Identity hostParticipantId, MatchRuntimeDependencies dependencies)
            : dependencies(std::move(dependencies)), hostParticipantId(hostParticipantId),
              replication(), replicationConnections(replication.replicator()) {}

    bool AuthoritativeHostedMatchController::initializeReplication(
            std::vector<Network::Replication::ParticipantState> participants,
            std::vector<PlayerDefinition> roster, MatchConfig settings) {
        return currentStage == HostedMatchStage::ServiceStarting
               && replication.setLobby(hostParticipantId, std::move(participants),
                                       std::move(roster), std::move(settings));
    }

    bool AuthoritativeHostedMatchController::restoreReplication(
            Identity participantId, Network::Replication::ReplicationSender sender) {
        return replicationConnections.restore(participantId, std::move(sender));
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
        activeMatch = std::make_unique<AuthoritativeMatch>(dependencies);
        const TerminalOutcome started = activeMatch->start(config, roster, manifest);
        if (started.code == OutcomeCode::None) {
            currentStage = HostedMatchStage::MatchActive;
            if (auto update = replication.beginMatch(*activeMatch)) replicationConnections.broadcast(*update);
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

    void AuthoritativeHostedMatchController::observeMatchOutcome() {
        if (!activeMatch || currentStage != HostedMatchStage::MatchActive) return;
        if (auto update = replication.capture(*activeMatch)) replicationConnections.broadcast(*update);
        if (activeMatch->outcome().code == OutcomeCode::RuntimeFailed
            || activeMatch->outcome().code == OutcomeCode::ShutdownFailed) {
            activeMatch->shutdown();
            currentStage = HostedMatchStage::UnexpectedStop;
        } else if (activeMatch->outcome().code != OutcomeCode::None) {
            const TerminalOutcome stopped = activeMatch->shutdown();
            currentStage = stopped.code == OutcomeCode::ShutdownFailed
                           ? HostedMatchStage::UnexpectedStop : HostedMatchStage::Ended;
        }
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
