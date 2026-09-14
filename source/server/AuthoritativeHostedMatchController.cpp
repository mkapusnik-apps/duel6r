#include "AuthoritativeHostedMatchController.h"

#include <limits>
#include <set>
#include <type_traits>
#include <utility>

#include "AuthoritativeMatchValidation.h"

namespace Duel6::Server::Authoritative {
    namespace {
        LobbyCommitOutcome lobbyOutcome(CanonicalLobbyMutationOutcome outcome) noexcept {
            switch (outcome) {
                case CanonicalLobbyMutationOutcome::Committed: return LobbyCommitOutcome::Committed;
                case CanonicalLobbyMutationOutcome::Rejected: return LobbyCommitOutcome::Rejected;
                case CanonicalLobbyMutationOutcome::VersionFailure: return LobbyCommitOutcome::VersionFailure;
                case CanonicalLobbyMutationOutcome::PublicationFailure:
                    return LobbyCommitOutcome::PublicationFailure;
                case CanonicalLobbyMutationOutcome::InternalFailure: return LobbyCommitOutcome::InternalFailure;
            }
            return LobbyCommitOutcome::InternalFailure;
        }
    }

    PreparedLobbyMutation::PreparedLobbyMutation(
            std::weak_ptr<const void> ownerLifetime, std::uint64_t generation,
            Network::Replication::StateVersion baselineVersion,
            std::map<Identity, bool> baselineReadiness, AuthoritativeReplication replication,
            std::map<Identity, bool> readiness,
            Network::Replication::IncrementalUpdate update) noexcept
            : ownerLifetime(std::move(ownerLifetime)), generation(generation), baselineVersion(baselineVersion),
              baselineReadiness(std::move(baselineReadiness)), replication(std::move(replication)),
              readiness(std::move(readiness)), update(std::move(update)), valid(true) {}

    PreparedLobbyMutation::PreparedLobbyMutation(PreparedLobbyMutation &&other) noexcept
            : ownerLifetime(std::move(other.ownerLifetime)), generation(other.generation),
              baselineVersion(other.baselineVersion),
              baselineReadiness(std::move(other.baselineReadiness)), replication(std::move(other.replication)),
              readiness(std::move(other.readiness)), update(std::move(other.update)), valid(other.valid) {
        other.ownerLifetime.reset();
        other.valid = false;
    }

    PreparedLobbyMutation &PreparedLobbyMutation::operator=(PreparedLobbyMutation &&other) noexcept {
        if (this == &other) return *this;
        ownerLifetime = std::move(other.ownerLifetime);
        generation = other.generation;
        baselineVersion = other.baselineVersion;
        baselineReadiness = std::move(other.baselineReadiness);
        replication = std::move(other.replication);
        readiness = std::move(other.readiness);
        update = std::move(other.update);
        valid = other.valid;
        other.ownerLifetime.reset();
        other.valid = false;
        return *this;
    }

    std::map<Identity, bool> AuthoritativeHostedMatchController::replicatedReadiness(
            const std::vector<Network::Replication::ParticipantState> &participants) {
        std::map<Identity, bool> result;
        for (const auto &participant: participants) result.emplace(participant.participantId, participant.ready);
        return result;
    }

    AuthoritativeHostedMatchController::AuthoritativeHostedMatchController(
            Identity hostParticipantId, MatchRuntimeDependencies dependencies, Identity sessionId)
            : instanceLifetime(std::make_shared<const std::uint8_t>(0)), dependencies(std::move(dependencies)),
              hostParticipantId(hostParticipantId),
              replication(sessionId), replicationConnections(replication.replicator()), playerInput(hostParticipantId) {}

    bool AuthoritativeHostedMatchController::initializeReplication(
            std::vector<Network::Replication::ParticipantState> participants,
            std::vector<PlayerDefinition> roster, MatchConfig settings) {
        if (currentStage != HostedMatchStage::ServiceStarting) return false;
        auto nextReadiness = replicatedReadiness(participants);
        if (!replication.setLobby(hostParticipantId, std::move(participants),
                                  std::move(roster), std::move(settings))) return false;
        readiness = std::move(nextReadiness);
        advanceLobbyMutationGeneration();
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
        advanceLobbyMutationGeneration();
        (void) replicationConnections.broadcast(*update);
        return true;
    }

    LobbyMutationPreparation AuthoritativeHostedMatchController::prepareLobbyConfiguration(
            const std::vector<Network::Replication::ParticipantState> &participants,
            const std::vector<PlayerDefinition> &roster, const MatchConfig &settings,
            std::string_view reason) noexcept {
        try {
            if (currentStage != HostedMatchStage::Lobby || reason.empty())
                return {LobbyCommitOutcome::Rejected, {}};
            if (lobbyMutationGeneration == (std::numeric_limits<std::uint64_t>::max)())
                return {LobbyCommitOutcome::InternalFailure, {}};
            const auto baselineVersion = replication.replicator().version();
            auto baselineReadiness = readiness;
            auto nextParticipants = participants;
            for (auto &participant: nextParticipants) participant.ready = false;
            auto nextReadiness = replicatedReadiness(nextParticipants);
            auto nextReplication = replication;
            auto proposal = nextReplication.updateLobbyForConfiguration(
                    nextParticipants, roster, settings, reason);
            if (proposal.outcome != CanonicalLobbyMutationOutcome::Committed)
                return {lobbyOutcome(proposal.outcome), {}};
            if (!proposal.update) return {LobbyCommitOutcome::InternalFailure, {}};
            auto mutation = std::unique_ptr<PreparedLobbyMutation>(new PreparedLobbyMutation(
                    instanceLifetime, lobbyMutationGeneration, baselineVersion, std::move(baselineReadiness),
                    std::move(nextReplication), std::move(nextReadiness), std::move(*proposal.update)));
            return {LobbyCommitOutcome::Committed, std::move(mutation)};
        } catch (...) {
            return {LobbyCommitOutcome::InternalFailure, {}};
        }
    }

    LobbyMutationPreparation AuthoritativeHostedMatchController::prepareParticipantReady(
            Identity participantId, bool ready) noexcept {
        try {
            if (currentStage != HostedMatchStage::Lobby || participantId == 0)
                return {LobbyCommitOutcome::Rejected, {}};
            if (lobbyMutationGeneration == (std::numeric_limits<std::uint64_t>::max)())
                return {LobbyCommitOutcome::InternalFailure, {}};
            const auto baselineVersion = replication.replicator().version();
            auto baselineReadiness = readiness;
            auto nextReadiness = readiness;
            const auto found = nextReadiness.find(participantId);
            if (found == nextReadiness.end()) return {LobbyCommitOutcome::Rejected, {}};
            found->second = ready;
            auto nextReplication = replication;
            auto proposal = nextReplication.setParticipantReadyTransactional(participantId, ready);
            if (proposal.outcome != CanonicalLobbyMutationOutcome::Committed)
                return {lobbyOutcome(proposal.outcome), {}};
            if (!proposal.update) return {LobbyCommitOutcome::InternalFailure, {}};
            auto mutation = std::unique_ptr<PreparedLobbyMutation>(new PreparedLobbyMutation(
                    instanceLifetime, lobbyMutationGeneration, baselineVersion, std::move(baselineReadiness),
                    std::move(nextReplication), std::move(nextReadiness), std::move(*proposal.update)));
            return {LobbyCommitOutcome::Committed, std::move(mutation)};
        } catch (...) {
            return {LobbyCommitOutcome::InternalFailure, {}};
        }
    }

    bool AuthoritativeHostedMatchController::canCommitPreparedLobbyMutation(
            const PreparedLobbyMutation &mutation) const noexcept {
        return mutation.valid && mutation.ownerLifetime.lock() == instanceLifetime
               && currentStage == HostedMatchStage::Lobby
               && mutation.generation == lobbyMutationGeneration
               && mutation.baselineVersion == replication.replicator().version()
               && mutation.baselineReadiness == readiness
               && mutation.baselineVersion != (std::numeric_limits<Network::Replication::StateVersion>::max)()
               && mutation.replication.replicator().version() == mutation.baselineVersion + 1
               && mutation.update.baseline == mutation.baselineVersion
               && mutation.update.version == mutation.baselineVersion + 1;
    }

    LobbyCommitOutcome AuthoritativeHostedMatchController::commitPreparedLobbyMutation(
            PreparedLobbyMutation &&mutation) noexcept {
        if (!canCommitPreparedLobbyMutation(mutation)) {
            mutation.valid = false;
            mutation.ownerLifetime.reset();
            return LobbyCommitOutcome::InternalFailure;
        }
        mutation.valid = false;
        mutation.ownerLifetime.reset();
        static_assert(std::is_nothrow_move_assignable_v<AuthoritativeReplication>);
        static_assert(std::is_nothrow_move_assignable_v<decltype(readiness)>);
        replication = std::move(mutation.replication);
        readiness = std::move(mutation.readiness);
        advanceLobbyMutationGeneration();
        try { (void) replicationConnections.broadcast(mutation.update); } catch (...) {}
        return LobbyCommitOutcome::Committed;
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

    bool AuthoritativeHostedMatchController::removeLifecycleParticipants(
            const std::vector<Identity> &participantIds) {
        if (!canRemoveLifecycleParticipants(participantIds)) return false;
        std::set<Identity> removals(participantIds.begin(), participantIds.end());
        clearReadiness();
        for (Identity participantId: participantIds) readiness.erase(participantId);
        if (!activeMatch || currentStage != HostedMatchStage::MatchActive) {
            if (!replication.retainsSessionResult()) return true;
            if (!replication.resultDepartureUpdateRequired(participantIds)) return true;
            const auto update = replication.markResultParticipantsDeparted(participantIds);
            if (!update) return false;
            if (!resultRetention.markParticipantsDeparted(participantIds)) {
                discardSessionResults();
                return false;
            }
            (void) replicationConnections.broadcastCurrentSnapshot();
            return true;
        }
        std::vector<Identity> players;
        for (const auto &player: activeMatch->rosterDefinitions())
            if (removals.count(player.participantId)) players.push_back(player.playerId);
        if (players.empty()
            || activeMatch->removePlayersBatch(hostParticipantId, players) != ActionResult::Accepted) return false;
        for (const auto playerId: players) playerInput.revokePlayer(playerId);
        if (!captureReplication()) return false;
        return activeMatch->outcome().code == OutcomeCode::None || observeMatchOutcome();
    }

    bool AuthoritativeHostedMatchController::canRemoveLifecycleParticipants(
            const std::vector<Identity> &participantIds) const noexcept {
        try {
            if (participantIds.empty()) return false;
            const std::set<Identity> removals(participantIds.begin(), participantIds.end());
            if (removals.size() != participantIds.size() || removals.count(0)
                || removals.count(hostParticipantId)) return false;
            if (!activeMatch || currentStage != HostedMatchStage::MatchActive) return true;
            std::vector<Identity> players;
            for (const auto &player: activeMatch->rosterDefinitions())
                if (removals.count(player.participantId)) players.push_back(player.playerId);
            return !players.empty() && activeMatch->canRemovePlayersBatch(hostParticipantId, players);
        } catch (...) { return false; }
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
        advanceLobbyMutationGeneration();
        return true;
    }

    bool AuthoritativeHostedMatchController::clearReadinessForConfiguration(const std::string &reason) {
        if (currentStage != HostedMatchStage::Lobby || reason.empty()) return false;
        clearReadiness();
        if (replication.replicator().version() == 0) return true;
        const auto update = replication.setLobbyFailure(reason);
        if (!update) return false;
        (void) replicationConnections.broadcast(*update);
        return true;
    }

    void AuthoritativeHostedMatchController::clearReadiness() noexcept {
        for (auto &entry: readiness) entry.second = false;
        advanceLobbyMutationGeneration();
    }

    void AuthoritativeHostedMatchController::advanceLobbyMutationGeneration() noexcept {
        if (lobbyMutationGeneration != (std::numeric_limits<std::uint64_t>::max)())
            ++lobbyMutationGeneration;
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
            if (replication.replicator().version() != 0) {
                const auto update = replication.setLobbyFailure(
                        "Match settings are invalid. Correct the settings and try again.");
                if (update) (void) replicationConnections.broadcast(*update);
            }
            return terminalOutcome(OutcomeCode::SettingsInvalid);
        }
        const ValidationResult content = validateFrozenContent(config, manifest);
        if (!content.valid) {
            clearReadiness();
            currentStage = HostedMatchStage::ContentBlocked;
            if (replication.replicator().version() != 0) {
                const auto update = replication.setLobbyFailure(
                        "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.");
                if (update) (void) replicationConnections.broadcast(*update);
            }
            return terminalOutcome(OutcomeCode::ContentUnavailable);
        }
        activeMatch = std::make_unique<AuthoritativeMatch>(std::move(matchDependencies));
        const TerminalOutcome started = activeMatch->start(config, roster, manifest);
        if (started.code == OutcomeCode::None) {
            const auto resultGeneration = resultRetention.beginMatch();
            if (!resultGeneration) {
                activeMatch->shutdown();
                activeMatch.reset();
                discardSessionResults();
                currentStage = HostedMatchStage::UnexpectedStop;
                return terminalOutcome(OutcomeCode::RuntimeFailed);
            }
            activeResultGeneration = *resultGeneration;
            if (!playerInput.beginMatch(*activeMatch, roster)) {
                activeMatch->shutdown();
                activeMatch.reset();
                discardSessionResults();
                currentStage = HostedMatchStage::UnexpectedStop;
                return terminalOutcome(OutcomeCode::RuntimeFailed);
            }
            if (replication.fullSnapshot()) {
                const auto update = replication.beginMatch(*activeMatch);
                if (!update) {
                    activeMatch->shutdown();
                    playerInput.clear();
                    activeMatch.reset();
                    discardSessionResults();
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return terminalOutcome(OutcomeCode::RuntimeFailed);
                }
                (void) replicationConnections.broadcast(*update);
                replicationPacer.reset(activeMatch->currentTick());
                lastReplicatedPhase = activeMatch->phase();
            }
            currentStage = HostedMatchStage::MatchActive;
            explicitReadinessRequired = false;
        }
        else if (started.code == OutcomeCode::RuntimeFailed) {
            discardSessionResults();
            currentStage = HostedMatchStage::UnexpectedStop;
        }
        else if (started.code == OutcomeCode::ContentUnavailable) {
            clearReadiness();
            currentStage = HostedMatchStage::ContentBlocked;
        }
        else currentStage = HostedMatchStage::Lobby;
        if (currentStage != HostedMatchStage::MatchActive) {
            activeMatch.reset();
            if (started.code != OutcomeCode::SettingsInvalid
                && started.code != OutcomeCode::ContentUnavailable) discardSessionResults();
        }
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
            discardSessionResults();
            currentStage = stopped.code == OutcomeCode::ShutdownFailed
                           ? HostedMatchStage::UnexpectedStop : HostedMatchStage::Ended;
            return stopped;
        }
        if (currentStage == HostedMatchStage::Lobby || currentStage == HostedMatchStage::FinalSummary
            || currentStage == HostedMatchStage::ContentBlocked) {
            discardSessionResults();
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
            discardSessionResults();
            currentStage = HostedMatchStage::UnexpectedStop;
            return false;
        } else if (activeMatch->outcome().code != OutcomeCode::None) {
            const TerminalOutcome stopped = activeMatch->shutdown();
            playerInput.clear();
            if (stopped.code == OutcomeCode::ShutdownFailed) {
                discardSessionResults();
                currentStage = HostedMatchStage::UnexpectedStop;
                return false;
            }
            else {
                const auto published = activeMatch->publishedResult();
                if (!published || resultRetention.retain(activeResultGeneration, *published)
                                  == ResultRetentionStatus::Rejected) {
                    discardSessionResults();
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return false;
                }
                const auto result = replication.capture(*activeMatch);
                if (!result) {
                    discardSessionResults();
                    currentStage = HostedMatchStage::UnexpectedStop;
                    return false;
                }
                (void) replicationConnections.broadcast(*result);
                const bool interrupted = published->state == ResultState::Interrupted;
                activeMatch.reset();
                if (interrupted) {
                    clearReadiness();
                    explicitReadinessRequired = true;
                    currentStage = HostedMatchStage::Lobby;
                } else currentStage = HostedMatchStage::FinalSummary;
            }
        } else {
            const bool lifecycleTransition = activeMatch->phase() != lastReplicatedPhase;
            if (replicationPacer.shouldPublish(activeMatch->currentTick(), lifecycleTransition)) {
                const auto update = replication.capture(*activeMatch);
                if (!update) return false;
                (void) replicationConnections.broadcast(*update);
                lastReplicatedPhase = activeMatch->phase();
            }
        }
        return true;
    }

    bool AuthoritativeHostedMatchController::returnToLobby(Identity participantId) {
        if (participantId != hostParticipantId || currentStage != HostedMatchStage::FinalSummary) return false;
        const auto lobby = replication.enterFollowingLobby();
        if (!lobby) return false;
        (void) replicationConnections.broadcast(*lobby);
        clearReadiness();
        explicitReadinessRequired = true;
        currentStage = HostedMatchStage::Lobby;
        return true;
    }

    bool AuthoritativeHostedMatchController::advanceOneTick() {
        return activeMatch && currentStage == HostedMatchStage::MatchActive && playerInput.processTick();
    }

    void AuthoritativeHostedMatchController::discardSessionResults() noexcept {
        activeResultGeneration = 0;
        resultRetention.discard();
        replication.discardSessionResults();
    }

    HostedMatchStage AuthoritativeHostedMatchController::stage() const noexcept { return currentStage; }
    bool AuthoritativeHostedMatchController::contentStartBlocked() const noexcept {
        return currentStage == HostedMatchStage::ContentBlocked;
    }
    bool AuthoritativeHostedMatchController::retainsCompletedResult() const noexcept {
        return replication.retainsCompletedResult();
    }
    bool AuthoritativeHostedMatchController::participantReady(Identity participantId) const noexcept {
        const auto found = readiness.find(participantId);
        return found != readiness.end() && found->second;
    }
    AuthoritativeMatch *AuthoritativeHostedMatchController::match() noexcept { return activeMatch.get(); }
    const AuthoritativeMatch *AuthoritativeHostedMatchController::match() const noexcept { return activeMatch.get(); }
    const std::optional<SessionResult> &AuthoritativeHostedMatchController::currentSessionResult() const noexcept {
        return resultRetention.current();
    }

    std::optional<Network::Replication::FullSnapshot>
    AuthoritativeHostedMatchController::currentSnapshot() const {
        return replication.fullSnapshot();
    }
}
