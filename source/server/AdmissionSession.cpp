#include "AdmissionSession.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Duel6::Server {
    namespace {
        Network::AdmissionResult rejection(Network::AdmissionResultCode code) {
            Network::AdmissionResult result;
            result.code = code;
            return result;
        }

        class WorkReservation {
        public:
            explicit WorkReservation(Network::Trust::ConcurrentWorkLimiter &limiter)
                    : limiter(limiter), reserved(limiter.reserve()) {}
            ~WorkReservation() { if (reserved) limiter.release(); }
            explicit operator bool() const { return reserved; }
        private:
            Network::Trust::ConcurrentWorkLimiter &limiter;
            bool reserved;
        };
    }

    SessionAllocation::SessionAllocation(std::uint8_t hostLocalPlayers) {
        if (hostLocalPlayers == 0 || hostLocalPlayers > Network::Trust::MaxParticipants)
            throw std::invalid_argument("Host local player count must be in range 1..15");
        AdmittedParticipant host;
        host.participantId = takeIdentityLocked();
        host.localHost = true;
        host.playerIds.reserve(hostLocalPlayers);
        for (std::uint8_t index = 0; index < hostLocalPlayers; ++index) host.playerIds.push_back(takeIdentityLocked());
        players = host.playerIds.size();
        hostId = host.participantId;
        participants.emplace(host.participantId, std::move(host));
    }

    std::uint64_t SessionAllocation::takeIdentityLocked() {
        if (nextIdentity == 0 || nextIdentity == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Session identity space exhausted");
        return nextIdentity++;
    }

    Network::AdmissionResult SessionAllocation::allocateGuest(std::uint8_t localPlayers) {
        std::lock_guard<std::mutex> lock(mutex);
        if (localPlayers == 0 || localPlayers > Network::Trust::MaxParticipants
            || participants.size() >= Network::Trust::MaxParticipants
            || localPlayers > Network::Trust::MaxParticipants - players)
            return rejection(Network::AdmissionResultCode::SessionFull);

        AdmittedParticipant participant;
        participant.participantId = takeIdentityLocked();
        participant.playerIds.reserve(localPlayers);
        for (std::uint8_t index = 0; index < localPlayers; ++index)
            participant.playerIds.push_back(takeIdentityLocked());

        Network::AdmissionResult result;
        result.code = Network::AdmissionResultCode::Admitted;
        result.participantId = participant.participantId;
        result.playerIds = participant.playerIds;
        players += participant.playerIds.size();
        participants.emplace(participant.participantId, std::move(participant));
        return result;
    }

    bool SessionAllocation::hasCapacity(std::uint8_t localPlayers) const {
        std::lock_guard<std::mutex> lock(mutex);
        return localPlayers > 0 && localPlayers <= Network::Trust::MaxParticipants
               && participants.size() < Network::Trust::MaxParticipants
               && localPlayers <= Network::Trust::MaxParticipants - players;
    }

    std::size_t SessionAllocation::participantCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return participants.size();
    }

    std::size_t SessionAllocation::playerCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return players;
    }

    const AdmittedParticipant &SessionAllocation::hostParticipant() const {
        return participants.at(hostId);
    }

    bool SessionAllocation::participantOwnsPlayer(std::uint64_t participantId, std::uint64_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex);
        const auto participant = participants.find(participantId);
        return participant != participants.end()
               && std::find(participant->second.playerIds.begin(), participant->second.playerIds.end(), playerId)
                  != participant->second.playerIds.end();
    }

    AdmissionPolicy::AdmissionPolicy(Network::GameplayManifest frozenHostManifest, std::uint8_t hostLocalPlayers)
            : manifest(std::move(frozenHostManifest)), sessionAllocation(hostLocalPlayers) {
        if (!Network::validCanonicalManifest(manifest))
            throw std::invalid_argument("Host gameplay content manifest is invalid");
    }

    Network::AdmissionResult AdmissionPolicy::evaluate(const Network::AdmissionRequest &request,
                                                        AdmissionContext context) {
        if (!context.authorized) return rejection(Network::AdmissionResultCode::NotAuthorized);
        if (request.protocolVersion != Network::AdmissionProtocolVersion)
            return rejection(Network::AdmissionResultCode::ProtocolIncompatible);
        if (request.networkReleaseId != Network::NetworkReleaseId)
            return rejection(Network::AdmissionResultCode::NetworkReleaseMismatch);
        if (!Network::hasRequiredAdmissionCapabilities(request.capabilities))
            return rejection(Network::AdmissionResultCode::RequiredCapabilityUnsupported);
        if (!Network::validCanonicalManifest(request.gameplayManifest))
            return rejection(Network::AdmissionResultCode::GameplayContentManifestInvalid);

        WorkReservation work(manifestWork);
        if (!work) return rejection(Network::AdmissionResultCode::HostPolicyRejected);
        if (!Network::gameplayManifestsEqual(manifest, request.gameplayManifest))
            return rejection(Network::AdmissionResultCode::GameplayContentMismatch);

        std::lock_guard<std::mutex> lock(policyMutex);
        if (matchStarted) return rejection(Network::AdmissionResultCode::MatchAlreadyStarted);
        if (!sessionAllocation.hasCapacity(request.localPlayerCount))
            return rejection(Network::AdmissionResultCode::SessionFull);
        if (!context.hostPolicyAllows) return rejection(Network::AdmissionResultCode::HostPolicyRejected);
        return sessionAllocation.allocateGuest(request.localPlayerCount);
    }

    Network::AdmissionResult AdmissionPolicy::evaluatePayload(const std::vector<std::uint8_t> &payload,
                                                               AdmissionContext context) {
        try {
            return evaluate(Network::deserializeAdmissionRequest(payload), context);
        } catch (...) {
            return rejection(Network::AdmissionResultCode::MalformedRequest);
        }
    }

    void AdmissionPolicy::setMatchStarted(bool started) {
        std::lock_guard<std::mutex> lock(policyMutex);
        matchStarted = started;
    }

    const Network::GameplayManifest &AdmissionPolicy::frozenManifest() const { return manifest; }
    const SessionAllocation &AdmissionPolicy::allocation() const { return sessionAllocation; }
}
