#ifndef DUEL6_SERVER_AUTHORITATIVEREPLICATION_H
#define DUEL6_SERVER_AUTHORITATIVEREPLICATION_H

#include <map>
#include <optional>
#include <set>

#include "AuthoritativeMatch.h"
#include "../network/StateReplication.h"

namespace Duel6::Server::Authoritative {
    class AuthoritativeReplication final {
    public:
        explicit AuthoritativeReplication(Identity sessionId = 0);

        bool setLobby(Identity hostParticipantId,
                      std::vector<Network::Replication::ParticipantState> participants,
                      std::vector<PlayerDefinition> roster, MatchConfig settings);
        std::optional<Network::Replication::IncrementalUpdate> updateLobby(
                std::vector<Network::Replication::ParticipantState> participants,
                std::vector<PlayerDefinition> roster, MatchConfig settings);
        std::optional<Network::Replication::IncrementalUpdate> setParticipantReady(Identity participantId,
                                                                                    bool ready);
        std::optional<Network::Replication::IncrementalUpdate> setParticipantConnection(
                Identity participantId, Network::Replication::ConnectionState connection);
        std::optional<Network::Replication::IncrementalUpdate> beginMatch(const AuthoritativeMatch &match);
        std::optional<Network::Replication::IncrementalUpdate> capture(const AuthoritativeMatch &match);
        std::optional<Network::Replication::FullSnapshot> fullSnapshot() const;
        const Network::Replication::AuthoritativeStateReplicator &replicator() const noexcept;

    private:
        Network::Replication::StableIdentitySource identities;
        Network::Replication::AuthoritativeStateReplicator publisher;
        Network::Replication::CanonicalState state;
        std::map<std::pair<Identity, std::uint64_t>, Identity> worldIdentities;
        std::uint64_t highestObservedEventSequence = 0;
        std::uint64_t highestObservedTransitionSequence = 0;
        std::uint8_t observedRound = 0;

        bool updateFromMatch(const AuthoritativeMatch &match,
                             std::vector<Network::Replication::PresentationEvent> &events);
        Identity worldIdentity(Identity roundId, std::uint64_t canonicalIdentity);
    };
}

#endif
