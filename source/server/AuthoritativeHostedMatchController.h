#ifndef DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H
#define DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "AuthoritativeMatch.h"
#include "AuthoritativeReplication.h"
#include "AuthoritativePlayerInput.h"
#include "NetworkMatchResultRetention.h"
#include "../network/NetworkResponsiveness.h"
#include "../network/StateReplicationProtocol.h"

namespace Duel6::Server::Authoritative {
    enum class HostedMatchStage {
        ServiceStarting,
        Lobby,
        MatchActive,
        FinalSummary,
        ContentBlocked,
        UnexpectedStop,
        Ended
    };
    enum class LobbyCommitOutcome {
        Committed,
        Rejected,
        LifecycleFailure,
        PublicationFailure,
        VersionFailure,
        InternalFailure
    };

    class AuthoritativeHostedMatchController;
    class PreparedLobbyMutation final {
        friend class AuthoritativeHostedMatchController;
    public:
        PreparedLobbyMutation(PreparedLobbyMutation &&other) noexcept;
        PreparedLobbyMutation &operator=(PreparedLobbyMutation &&other) noexcept;
        PreparedLobbyMutation(const PreparedLobbyMutation &) = delete;
        PreparedLobbyMutation &operator=(const PreparedLobbyMutation &) = delete;
    private:
        PreparedLobbyMutation(std::weak_ptr<const void> ownerLifetime,
                               std::uint64_t generation,
                               Network::Replication::StateVersion baselineVersion,
                               std::map<Identity, bool> baselineReadiness,
                               AuthoritativeReplication replication,
                               std::map<Identity, bool> readiness,
                               Network::Replication::IncrementalUpdate update) noexcept;
        std::weak_ptr<const void> ownerLifetime;
        std::uint64_t generation = 0;
        Network::Replication::StateVersion baselineVersion = 0;
        std::map<Identity, bool> baselineReadiness;
        AuthoritativeReplication replication;
        std::map<Identity, bool> readiness;
        Network::Replication::IncrementalUpdate update;
        bool valid = false;
    };

    struct LobbyMutationPreparation {
        LobbyCommitOutcome outcome = LobbyCommitOutcome::InternalFailure;
        std::unique_ptr<PreparedLobbyMutation> mutation;
    };

    class AuthoritativeHostedMatchController final {
    public:
        AuthoritativeHostedMatchController(Identity hostParticipantId,
                                           MatchRuntimeDependencies dependencies = {},
                                           Identity sessionId = 0);
        AuthoritativeHostedMatchController(const AuthoritativeHostedMatchController &) = delete;
        AuthoritativeHostedMatchController &operator=(const AuthoritativeHostedMatchController &) = delete;
        AuthoritativeHostedMatchController(AuthoritativeHostedMatchController &&) = delete;
        AuthoritativeHostedMatchController &operator=(AuthoritativeHostedMatchController &&) = delete;

        bool markServiceReady();
        bool setParticipantReady(Identity participantId, bool ready);
        bool clearReadinessForConfiguration(const std::string &reason =
                "Local player controls changed. Everyone must confirm readiness again.");
        TerminalOutcome start(const MatchConfig &config, const std::vector<PlayerDefinition> &roster,
                               const Network::GameplayManifest &manifest);
        TerminalOutcome start(const MatchConfig &config, const std::vector<PlayerDefinition> &roster,
                              const Network::GameplayManifest &manifest,
                              MatchRuntimeDependencies matchDependencies);
        TerminalOutcome end(Identity participantId);
        bool observeMatchOutcome();
        bool returnToLobby(Identity participantId);
        bool initializeReplication(std::vector<Network::Replication::ParticipantState> participants,
                                   std::vector<PlayerDefinition> roster, MatchConfig settings);
        bool updateReplicationLobby(std::vector<Network::Replication::ParticipantState> participants,
                                    std::vector<PlayerDefinition> roster, MatchConfig settings);
        LobbyMutationPreparation prepareLobbyConfiguration(
                const std::vector<Network::Replication::ParticipantState> &participants,
                const std::vector<PlayerDefinition> &roster, const MatchConfig &settings,
                std::string_view reason) noexcept;
        LobbyMutationPreparation prepareParticipantReady(Identity participantId, bool ready) noexcept;
        bool canCommitPreparedLobbyMutation(const PreparedLobbyMutation &mutation) const noexcept;
        LobbyCommitOutcome commitPreparedLobbyMutation(PreparedLobbyMutation &&mutation) noexcept;
        bool restoreReplication(Identity participantId, Network::Replication::ReplicationSender sender,
                                std::function<void()> close = {});
        void disconnectReplication(Identity participantId) noexcept;
        bool updateReplicationConnection(Identity participantId,
                Network::Replication::ConnectionState connection);
        Network::Replication::HostReplicationResult receiveReplication(
                Identity participantId, const std::vector<std::uint8_t> &payload);
        bool captureReplication();
        bool restorePlayerInput(Identity participantId, AuthoritativePlayerInput::Sender sender,
                                std::function<void()> close = {});
        void disconnectPlayerInput(Identity participantId) noexcept;
        void revokePlayerInput(Identity playerId) noexcept;
        bool removeLifecycleParticipants(const std::vector<Identity> &participantIds);
        bool canRemoveLifecycleParticipants(const std::vector<Identity> &participantIds) const noexcept;
        AuthoritativePlayerInput::ReceiveResult receivePlayerInput(
                Identity participantId, const Network::Input::Command &command, bool remote = true);
        bool advanceOneTick();
        void discardSessionResults() noexcept;

        HostedMatchStage stage() const noexcept;
        bool contentStartBlocked() const noexcept;
        bool retainsCompletedResult() const noexcept;
        bool participantReady(Identity participantId) const noexcept;
        AuthoritativeMatch *match() noexcept;
        const AuthoritativeMatch *match() const noexcept;
        const std::optional<SessionResult> &currentSessionResult() const noexcept;
        std::optional<Network::Replication::FullSnapshot> currentSnapshot() const;
        static constexpr bool resultsPersistenceEligible() noexcept {
            return NetworkMatchResultRetention::persistenceEligible();
        }

    private:
        std::shared_ptr<const void> instanceLifetime;
        MatchRuntimeDependencies dependencies;
        HostedMatchStage currentStage = HostedMatchStage::ServiceStarting;
        std::map<Identity, bool> readiness;
        bool explicitReadinessRequired = false;
        const Identity hostParticipantId;
        std::unique_ptr<AuthoritativeMatch> activeMatch;
        AuthoritativeReplication replication;
        Network::Replication::AuthoritativeReplicationConnections replicationConnections;
        AuthoritativePlayerInput playerInput;
        NetworkMatchResultRetention resultRetention;
        std::uint64_t activeResultGeneration = 0;
        std::uint64_t lobbyMutationGeneration = 1;
        Network::Responsiveness::CanonicalUpdatePacer replicationPacer;
        MatchPhase lastReplicatedPhase = MatchPhase::Lobby;

        template<typename PreflightExternal>
        LobbyCommitOutcome commitLobbyConfiguration(
                const std::vector<Network::Replication::ParticipantState> &participants,
                const std::vector<PlayerDefinition> &roster, const MatchConfig &settings,
                std::string_view reason,
                PreflightExternal &&preflightExternal) noexcept {
            auto prepared = prepareLobbyConfiguration(participants, roster, settings, reason);
            if (prepared.outcome != LobbyCommitOutcome::Committed || !prepared.mutation)
                return prepared.outcome;
            LobbyCommitOutcome external = LobbyCommitOutcome::InternalFailure;
            try { external = std::forward<PreflightExternal>(preflightExternal)(); }
            catch (...) { return LobbyCommitOutcome::InternalFailure; }
            if (external != LobbyCommitOutcome::Committed) return external;
            return commitPreparedLobbyMutation(std::move(*prepared.mutation));
        }
        LobbyCommitOutcome commitLobbyConfiguration(
                const std::vector<Network::Replication::ParticipantState> &,
                const std::vector<PlayerDefinition> &, const MatchConfig &, std::string_view,
                std::nullptr_t) noexcept { return LobbyCommitOutcome::InternalFailure; }
        template<typename PreflightExternal>
        LobbyCommitOutcome commitParticipantReady(
                Identity participantId, bool ready,
                PreflightExternal &&preflightExternal) noexcept {
            auto prepared = prepareParticipantReady(participantId, ready);
            if (prepared.outcome != LobbyCommitOutcome::Committed || !prepared.mutation)
                return prepared.outcome;
            LobbyCommitOutcome external = LobbyCommitOutcome::InternalFailure;
            try { external = std::forward<PreflightExternal>(preflightExternal)(); }
            catch (...) { return LobbyCommitOutcome::InternalFailure; }
            if (external != LobbyCommitOutcome::Committed) return external;
            return commitPreparedLobbyMutation(std::move(*prepared.mutation));
        }
        LobbyCommitOutcome commitParticipantReady(
                Identity, bool, std::nullptr_t) noexcept { return LobbyCommitOutcome::InternalFailure; }
        void clearReadiness() noexcept;
        void advanceLobbyMutationGeneration() noexcept;
        bool allParticipantsReady(const std::vector<PlayerDefinition> &roster) const noexcept;
        static std::map<Identity, bool> replicatedReadiness(
                const std::vector<Network::Replication::ParticipantState> &participants);
    };
}

#endif
