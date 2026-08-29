#ifndef DUEL6_SERVER_ADMISSIONSESSION_H
#define DUEL6_SERVER_ADMISSIONSESSION_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include "../network/AdmissionProtocol.h"
#include "../network/NetworkTrustPolicy.h"

namespace Duel6::Server {
    using IdentitySource = std::function<std::optional<std::uint64_t>()>;
    using ValidationWorkGate = std::function<bool()>;
    IdentitySource sequentialIdentitySource(std::uint64_t firstIdentity = 1);

    struct AdmittedParticipant {
        std::uint64_t participantId = 0;
        std::vector<std::uint64_t> playerIds;
        bool localHost = false;
    };

    struct AdmissionContext {
        bool authorized = true;
        bool hostPolicyAllows = true;
    };

    struct AdmissionOffer {
        std::uint64_t transactionId = 0;
        Network::AdmissionResult result;

        bool pending() const { return transactionId != 0 && result.admitted(); }
    };

    class SessionAllocation {
    public:
        explicit SessionAllocation(std::uint8_t hostLocalPlayers, IdentitySource identities = {});

        AdmissionOffer reserveGuest(std::uint8_t localPlayers);
        bool commit(std::uint64_t transactionId);
        bool rollback(std::uint64_t transactionId);
        std::optional<AdmittedParticipant> pendingParticipant(std::uint64_t transactionId) const;
        Network::AdmissionResult allocateGuest(std::uint8_t localPlayers);
        bool hasCapacity(std::uint8_t localPlayers) const;
        std::size_t participantCount() const;
        std::size_t playerCount() const;
        std::size_t pendingParticipantCount() const;
        std::size_t pendingPlayerCount() const;
        const AdmittedParticipant &hostParticipant() const;
        bool participantOwnsPlayer(std::uint64_t participantId, std::uint64_t playerId) const;

    private:
        std::uint64_t takeIdentityLocked();
        bool hasCapacityLocked(std::uint8_t localPlayers) const;

        mutable std::mutex mutex;
        IdentitySource identities;
        std::set<std::uint64_t> issuedIdentities;
        std::map<std::uint64_t, AdmittedParticipant> participants;
        std::map<std::uint64_t, AdmittedParticipant> pending;
        std::size_t players = 0;
        std::size_t pendingPlayers = 0;
        std::uint64_t hostId = 0;
    };

    class AdmissionPolicy {
    public:
        AdmissionPolicy(Network::GameplayManifest frozenHostManifest, std::uint8_t hostLocalPlayers,
                        IdentitySource identities = {},
                        std::shared_ptr<Network::Trust::ConcurrentWorkLimiter> workLimiter = {},
                        ValidationWorkGate validationWorkGate = {});

        Network::AdmissionResult evaluate(const Network::AdmissionRequest &request,
                                          AdmissionContext context = {});
        Network::AdmissionResult evaluatePayload(const std::vector<std::uint8_t> &payload,
                                                  AdmissionContext context = {});
        AdmissionOffer offer(const Network::AdmissionRequest &request, AdmissionContext context = {});
        AdmissionOffer offerPayload(const std::vector<std::uint8_t> &payload, AdmissionContext context = {});
        bool commit(std::uint64_t transactionId, Network::Trust::ConnectionId connection);
        bool rollback(std::uint64_t transactionId);
        void disconnect(Network::Trust::ConnectionId connection);
        bool authorize(Network::Trust::ConnectionId connection, Network::Trust::AuthorityAction action,
                       std::optional<Network::Trust::PlayerSlotId> player = std::nullopt) const;

        void setMatchStarted(bool started);
        const Network::GameplayManifest &frozenManifest() const;
        const SessionAllocation &allocation() const;

    private:
        AdmissionOffer evaluateForOffer(const Network::AdmissionRequest &request, AdmissionContext context);

        Network::GameplayManifest manifest;
        SessionAllocation sessionAllocation;
        std::shared_ptr<Network::Trust::ConcurrentWorkLimiter> manifestWork;
        ValidationWorkGate validationWorkGate;
        Network::Trust::AuthorizationPolicy authorization;
        bool matchStarted = false;
        mutable std::mutex policyMutex;
    };
}

#endif
