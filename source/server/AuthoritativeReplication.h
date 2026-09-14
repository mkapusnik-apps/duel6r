#ifndef DUEL6_SERVER_AUTHORITATIVEREPLICATION_H
#define DUEL6_SERVER_AUTHORITATIVEREPLICATION_H

#include <map>
#include <optional>
#include <set>
#include <string_view>

#include "AuthoritativeMatch.h"
#include "../network/StateReplication.h"

namespace Duel6::Server::Authoritative {
    enum class CanonicalLobbyMutationOutcome {
        Committed,
        Rejected,
        VersionFailure,
        PublicationFailure,
        InternalFailure
    };

    struct CanonicalLobbyMutationResult {
        CanonicalLobbyMutationOutcome outcome = CanonicalLobbyMutationOutcome::InternalFailure;
        std::optional<Network::Replication::IncrementalUpdate> update;
    };

    class AuthoritativeReplication final {
    public:
        explicit AuthoritativeReplication(Identity sessionId = 0);

        bool setLobby(Identity hostParticipantId,
                      std::vector<Network::Replication::ParticipantState> participants,
                      std::vector<PlayerDefinition> roster, MatchConfig settings);
        std::optional<Network::Replication::IncrementalUpdate> updateLobby(
                std::vector<Network::Replication::ParticipantState> participants,
                std::vector<PlayerDefinition> roster, MatchConfig settings);
        CanonicalLobbyMutationResult updateLobbyForConfiguration(
                const std::vector<Network::Replication::ParticipantState> &participants,
                const std::vector<PlayerDefinition> &roster, const MatchConfig &settings,
                std::string_view reason) noexcept;
        CanonicalLobbyMutationResult setParticipantReadyTransactional(Identity participantId, bool ready) noexcept;
        std::optional<Network::Replication::IncrementalUpdate> setParticipantReady(Identity participantId,
                                                                                     bool ready);
        std::optional<Network::Replication::IncrementalUpdate> setLobbyFailure(const std::string &message);
        std::optional<Network::Replication::IncrementalUpdate> setParticipantConnection(
                Identity participantId, Network::Replication::ConnectionState connection);
        std::optional<Network::Replication::IncrementalUpdate> beginMatch(const AuthoritativeMatch &match);
        std::optional<Network::Replication::IncrementalUpdate> capture(const AuthoritativeMatch &match);
        std::optional<Network::Replication::IncrementalUpdate> markResultParticipantsDeparted(
                const std::vector<Identity> &participantIds);
        std::optional<Network::Replication::IncrementalUpdate> enterFollowingLobby();
        void discardSessionResults() noexcept;
        std::optional<Network::Replication::FullSnapshot> fullSnapshot() const;
        const Network::Replication::AuthoritativeStateReplicator &replicator() const noexcept;
        bool retainsSessionResult() const noexcept;
        bool retainsCompletedResult() const noexcept;
        bool resultDepartureUpdateRequired(const std::vector<Identity> &participantIds) const noexcept;

    private:
        Network::Replication::StableIdentitySource identities;
        Network::Replication::AuthoritativeStateReplicator publisher;
        Network::Replication::CanonicalState state;
        std::optional<SessionResult> retainedResult;
        std::map<std::uint8_t, Identity> roundIdentities;
        std::map<std::pair<Identity, std::uint64_t>, Identity> worldIdentities;
        std::uint64_t highestObservedEventSequence = 0;
        std::uint64_t highestObservedTransitionSequence = 0;
        std::uint8_t observedRound = 0;

        bool updateFromMatch(const AuthoritativeMatch &match,
                             std::vector<Network::Replication::PresentationEvent> &events);
        CanonicalLobbyMutationResult updateLobbyStateTransactional(
                const std::vector<Network::Replication::ParticipantState> &participants,
                const std::vector<PlayerDefinition> &roster, const MatchConfig &settings,
                std::string_view status, bool clearReadiness) noexcept;
        Identity worldIdentity(Identity roundId, std::uint64_t canonicalIdentity);
    };
}

#endif
