#ifndef DUEL6_SERVER_ADMISSIONSESSION_H
#define DUEL6_SERVER_ADMISSIONSESSION_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "../network/AdmissionProtocol.h"
#include "../network/NetworkTrustPolicy.h"

namespace Duel6::Server {
    struct AdmittedParticipant {
        std::uint64_t participantId = 0;
        std::vector<std::uint64_t> playerIds;
        bool localHost = false;
    };

    struct AdmissionContext {
        bool authorized = true;
        bool hostPolicyAllows = true;
    };

    class SessionAllocation {
    public:
        explicit SessionAllocation(std::uint8_t hostLocalPlayers);

        Network::AdmissionResult allocateGuest(std::uint8_t localPlayers);
        bool hasCapacity(std::uint8_t localPlayers) const;
        std::size_t participantCount() const;
        std::size_t playerCount() const;
        const AdmittedParticipant &hostParticipant() const;
        bool participantOwnsPlayer(std::uint64_t participantId, std::uint64_t playerId) const;

    private:
        std::uint64_t takeIdentityLocked();

        mutable std::mutex mutex;
        std::uint64_t nextIdentity = 1;
        std::map<std::uint64_t, AdmittedParticipant> participants;
        std::size_t players = 0;
        std::uint64_t hostId = 0;
    };

    class AdmissionPolicy {
    public:
        AdmissionPolicy(Network::GameplayManifest frozenHostManifest, std::uint8_t hostLocalPlayers);

        Network::AdmissionResult evaluate(const Network::AdmissionRequest &request,
                                          AdmissionContext context = {});
        Network::AdmissionResult evaluatePayload(const std::vector<std::uint8_t> &payload,
                                                 AdmissionContext context = {});

        void setMatchStarted(bool started);
        const Network::GameplayManifest &frozenManifest() const;
        const SessionAllocation &allocation() const;

    private:
        Network::GameplayManifest manifest;
        SessionAllocation sessionAllocation;
        Network::Trust::ConcurrentWorkLimiter manifestWork;
        bool matchStarted = false;
        mutable std::mutex policyMutex;
    };
}

#endif
