#ifndef DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H
#define DUEL6_SERVER_AUTHORITATIVEHOSTEDMATCHCONTROLLER_H

#include <map>
#include <memory>

#include "AuthoritativeMatch.h"
#include "AuthoritativeReplication.h"
#include "AuthoritativePlayerInput.h"
#include "../network/StateReplicationProtocol.h"

namespace Duel6::Server::Authoritative {
    enum class HostedMatchStage {
        ServiceStarting,
        Lobby,
        MatchActive,
        ContentBlocked,
        UnexpectedStop,
        Ended
    };

    class AuthoritativeHostedMatchController final {
    public:
        AuthoritativeHostedMatchController(Identity hostParticipantId,
                                           MatchRuntimeDependencies dependencies = {});

        bool markServiceReady();
        bool setParticipantReady(Identity participantId, bool ready);
        TerminalOutcome start(const MatchConfig &config, const std::vector<PlayerDefinition> &roster,
                               const Network::GameplayManifest &manifest);
        TerminalOutcome start(const MatchConfig &config, const std::vector<PlayerDefinition> &roster,
                              const Network::GameplayManifest &manifest,
                              MatchRuntimeDependencies matchDependencies);
        TerminalOutcome end(Identity participantId);
        bool observeMatchOutcome();
        bool initializeReplication(std::vector<Network::Replication::ParticipantState> participants,
                                   std::vector<PlayerDefinition> roster, MatchConfig settings);
        bool updateReplicationLobby(std::vector<Network::Replication::ParticipantState> participants,
                                    std::vector<PlayerDefinition> roster, MatchConfig settings);
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
        AuthoritativePlayerInput::ReceiveResult receivePlayerInput(
                Identity participantId, const Network::Input::Command &command, bool remote = true);
        bool advanceOneTick();

        HostedMatchStage stage() const noexcept;
        bool contentStartBlocked() const noexcept;
        bool participantReady(Identity participantId) const noexcept;
        AuthoritativeMatch *match() noexcept;
        const AuthoritativeMatch *match() const noexcept;

    private:
        MatchRuntimeDependencies dependencies;
        HostedMatchStage currentStage = HostedMatchStage::ServiceStarting;
        std::map<Identity, bool> readiness;
        bool explicitReadinessRequired = false;
        const Identity hostParticipantId;
        std::unique_ptr<AuthoritativeMatch> activeMatch;
        AuthoritativeReplication replication;
        Network::Replication::AuthoritativeReplicationConnections replicationConnections;
        AuthoritativePlayerInput playerInput;

        void clearReadiness() noexcept;
        bool allParticipantsReady(const std::vector<PlayerDefinition> &roster) const noexcept;
        static std::map<Identity, bool> replicatedReadiness(
                const std::vector<Network::Replication::ParticipantState> &participants);
    };
}

#endif
