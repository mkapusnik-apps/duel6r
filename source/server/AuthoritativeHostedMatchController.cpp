#include "AuthoritativeHostedMatchController.h"

#include <set>
#include <limits>
#include <utility>

#include "AuthoritativeMatchValidation.h"

namespace Duel6::Server::Authoritative {
    AuthoritativeHostedMatchController::AuthoritativeHostedMatchController(
            Identity hostParticipantId, MatchRuntimeDependencies dependencies)
            : dependencies(std::move(dependencies)), hostParticipantId(hostParticipantId) {}

    bool AuthoritativeHostedMatchController::markServiceReady() {
        if (currentStage != HostedMatchStage::ServiceStarting) return false;
        currentStage = HostedMatchStage::Lobby;
        return true;
    }

    bool AuthoritativeHostedMatchController::setParticipantReady(Identity participantId, bool ready) {
        if (currentStage != HostedMatchStage::Lobby || participantId == 0) return false;
        readiness[participantId] = ready;
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
        if (started.code == OutcomeCode::None) currentStage = HostedMatchStage::MatchActive;
        else if (started.code == OutcomeCode::RuntimeFailed) currentStage = HostedMatchStage::UnexpectedStop;
        else if (started.code == OutcomeCode::ContentUnavailable) currentStage = HostedMatchStage::ContentBlocked;
        else currentStage = HostedMatchStage::Lobby;
        if (currentStage != HostedMatchStage::MatchActive) activeMatch.reset();
        return started;
    }

    TerminalOutcome AuthoritativeHostedMatchController::end(Identity participantId) {
        if (participantId == 0 || participantId != hostParticipantId)
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        if (currentStage == HostedMatchStage::MatchActive && activeMatch) {
            const ActionResult accepted = activeMatch->submit({activeMatch->currentTick(),
                    std::numeric_limits<std::uint64_t>::max(), participantId, 0,
                    ActionKind::EndSession, 0, 0, 0});
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
