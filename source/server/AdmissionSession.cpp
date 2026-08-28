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

        AdmissionOffer rejectedOffer(Network::AdmissionResultCode code) {
            return {0, rejection(code)};
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

    IdentitySource sequentialIdentitySource(std::uint64_t firstIdentity) {
        struct State { std::uint64_t next; bool exhausted = false; };
        auto state = std::make_shared<State>(State{firstIdentity, firstIdentity == 0});
        return [state]() -> std::optional<std::uint64_t> {
            if (state->exhausted) return std::nullopt;
            const std::uint64_t value = state->next;
            if (value == std::numeric_limits<std::uint64_t>::max()) state->exhausted = true;
            else ++state->next;
            return value;
        };
    }

    SessionAllocation::SessionAllocation(std::uint8_t hostLocalPlayers, IdentitySource identities)
            : identities(identities ? std::move(identities) : sequentialIdentitySource()) {
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
        for (std::size_t attempt = 0; attempt < Network::Trust::MaxParticipants * 2 + 2; ++attempt) {
            const std::optional<std::uint64_t> candidate = identities ? identities() : std::nullopt;
            if (!candidate) throw std::overflow_error("Session identity space exhausted");
            if (*candidate != 0 && issuedIdentities.insert(*candidate).second) return *candidate;
        }
        throw std::overflow_error("Session identity source did not provide a unique nonzero identity");
    }

    bool SessionAllocation::hasCapacityLocked(std::uint8_t localPlayers) const {
        return localPlayers > 0 && localPlayers <= Network::Trust::MaxParticipants
               && participants.size() + pending.size() < Network::Trust::MaxParticipants
               && localPlayers <= Network::Trust::MaxParticipants - players - pendingPlayers;
    }

    AdmissionOffer SessionAllocation::reserveGuest(std::uint8_t localPlayers) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!hasCapacityLocked(localPlayers)) return rejectedOffer(Network::AdmissionResultCode::SessionFull);
        AdmittedParticipant participant;
        try {
            participant.participantId = takeIdentityLocked();
            participant.playerIds.reserve(localPlayers);
            for (std::uint8_t index = 0; index < localPlayers; ++index)
                participant.playerIds.push_back(takeIdentityLocked());
        } catch (...) {
            return rejectedOffer(Network::AdmissionResultCode::HostPolicyRejected);
        }

        Network::AdmissionResult result;
        result.code = Network::AdmissionResultCode::Admitted;
        result.participantId = participant.participantId;
        result.playerIds = participant.playerIds;
        pendingPlayers += participant.playerIds.size();
        const std::uint64_t transactionId = participant.participantId;
        pending.emplace(transactionId, std::move(participant));
        return {transactionId, std::move(result)};
    }

    bool SessionAllocation::commit(std::uint64_t transactionId) {
        std::lock_guard<std::mutex> lock(mutex);
        auto reservation = pending.find(transactionId);
        if (reservation == pending.end()) return false;
        const std::size_t count = reservation->second.playerIds.size();
        participants.emplace(reservation->second.participantId, std::move(reservation->second));
        pending.erase(reservation);
        pendingPlayers -= count;
        players += count;
        return true;
    }

    bool SessionAllocation::rollback(std::uint64_t transactionId) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto reservation = pending.find(transactionId);
        if (reservation == pending.end()) return false;
        pendingPlayers -= reservation->second.playerIds.size();
        pending.erase(reservation);
        return true;
    }

    std::optional<AdmittedParticipant> SessionAllocation::pendingParticipant(std::uint64_t transactionId) const {
        std::lock_guard<std::mutex> lock(mutex);
        const auto reservation = pending.find(transactionId);
        return reservation == pending.end() ? std::nullopt
                                            : std::optional<AdmittedParticipant>(reservation->second);
    }

    Network::AdmissionResult SessionAllocation::allocateGuest(std::uint8_t localPlayers) {
        AdmissionOffer reservation = reserveGuest(localPlayers);
        if (!reservation.pending()) return reservation.result;
        if (!commit(reservation.transactionId)) return rejection(Network::AdmissionResultCode::HostPolicyRejected);
        return reservation.result;
    }

    bool SessionAllocation::hasCapacity(std::uint8_t localPlayers) const {
        std::lock_guard<std::mutex> lock(mutex);
        return hasCapacityLocked(localPlayers);
    }

    std::size_t SessionAllocation::participantCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return participants.size();
    }

    std::size_t SessionAllocation::playerCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return players;
    }

    std::size_t SessionAllocation::pendingParticipantCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pending.size();
    }

    std::size_t SessionAllocation::pendingPlayerCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return pendingPlayers;
    }

    const AdmittedParticipant &SessionAllocation::hostParticipant() const { return participants.at(hostId); }

    bool SessionAllocation::participantOwnsPlayer(std::uint64_t participantId, std::uint64_t playerId) const {
        std::lock_guard<std::mutex> lock(mutex);
        const auto participant = participants.find(participantId);
        return participant != participants.end()
               && std::find(participant->second.playerIds.begin(), participant->second.playerIds.end(), playerId)
                  != participant->second.playerIds.end();
    }

    AdmissionPolicy::AdmissionPolicy(Network::GameplayManifest frozenHostManifest, std::uint8_t hostLocalPlayers,
                                     IdentitySource identities,
                                     std::shared_ptr<Network::Trust::ConcurrentWorkLimiter> workLimiter,
                                     ValidationWorkGate validationWorkGate)
            : manifest(std::move(frozenHostManifest)), sessionAllocation(hostLocalPlayers, std::move(identities)),
              manifestWork(workLimiter ? std::move(workLimiter)
                                       : std::make_shared<Network::Trust::ConcurrentWorkLimiter>()),
              validationWorkGate(std::move(validationWorkGate)) {
        if (!Network::validCanonicalManifest(manifest))
            throw std::invalid_argument("Host gameplay content manifest is invalid");
        const AdmittedParticipant &host = sessionAllocation.hostParticipant();
        authorization.createLocalHost(0, host.participantId);
        if (!authorization.setOwnedSlots(host.participantId, host.playerIds))
            throw std::logic_error("Host ownership initialization failed");
    }

    AdmissionOffer AdmissionPolicy::evaluateForOffer(const Network::AdmissionRequest &request,
                                                      AdmissionContext context) {
        if (!context.authorized) return rejectedOffer(Network::AdmissionResultCode::NotAuthorized);
        if (request.protocolVersion != Network::AdmissionProtocolVersion)
            return rejectedOffer(Network::AdmissionResultCode::ProtocolIncompatible);
        if (request.networkReleaseId != Network::NetworkReleaseId)
            return rejectedOffer(Network::AdmissionResultCode::NetworkReleaseMismatch);
        if (!Network::hasRequiredAdmissionCapabilities(request.capabilities))
            return rejectedOffer(Network::AdmissionResultCode::RequiredCapabilityUnsupported);
        if (!Network::validCanonicalManifest(request.gameplayManifest))
            return rejectedOffer(Network::AdmissionResultCode::GameplayContentManifestInvalid);

        WorkReservation work(*manifestWork);
        if (!work) return rejectedOffer(Network::AdmissionResultCode::HostPolicyRejected);
        try {
            if (validationWorkGate && !validationWorkGate())
                return rejectedOffer(Network::AdmissionResultCode::HostPolicyRejected);
        } catch (...) {
            return rejectedOffer(Network::AdmissionResultCode::HostPolicyRejected);
        }
        if (!Network::gameplayManifestsEqual(manifest, request.gameplayManifest))
            return rejectedOffer(Network::AdmissionResultCode::GameplayContentMismatch);

        std::lock_guard<std::mutex> lock(policyMutex);
        if (matchStarted) return rejectedOffer(Network::AdmissionResultCode::MatchAlreadyStarted);
        if (!sessionAllocation.hasCapacity(request.localPlayerCount))
            return rejectedOffer(Network::AdmissionResultCode::SessionFull);
        if (!context.hostPolicyAllows) return rejectedOffer(Network::AdmissionResultCode::HostPolicyRejected);
        return sessionAllocation.reserveGuest(request.localPlayerCount);
    }

    AdmissionOffer AdmissionPolicy::offer(const Network::AdmissionRequest &request, AdmissionContext context) {
        return evaluateForOffer(request, context);
    }

    AdmissionOffer AdmissionPolicy::offerPayload(const std::vector<std::uint8_t> &payload, AdmissionContext context) {
        try { return offer(Network::deserializeAdmissionRequest(payload), context); }
        catch (...) { return rejectedOffer(Network::AdmissionResultCode::MalformedRequest); }
    }

    Network::AdmissionResult AdmissionPolicy::evaluate(const Network::AdmissionRequest &request,
                                                        AdmissionContext context) {
        AdmissionOffer pending = offer(request, context);
        if (!pending.pending()) return pending.result;
        if (!commit(pending.transactionId, pending.result.participantId)) {
            rollback(pending.transactionId);
            return rejection(Network::AdmissionResultCode::HostPolicyRejected);
        }
        return pending.result;
    }

    Network::AdmissionResult AdmissionPolicy::evaluatePayload(const std::vector<std::uint8_t> &payload,
                                                               AdmissionContext context) {
        try { return evaluate(Network::deserializeAdmissionRequest(payload), context); }
        catch (...) { return rejection(Network::AdmissionResultCode::MalformedRequest); }
    }

    bool AdmissionPolicy::commit(std::uint64_t transactionId, Network::Trust::ConnectionId connection) {
        std::lock_guard<std::mutex> lock(policyMutex);
        if (connection == 0) return false;
        const std::optional<AdmittedParticipant> participant = sessionAllocation.pendingParticipant(transactionId);
        if (!participant || !authorization.bindGuest(connection, participant->participantId)) return false;
        if (!authorization.setOwnedSlots(participant->participantId, participant->playerIds)
            || !sessionAllocation.commit(transactionId)) {
            authorization.disconnect(connection);
            authorization.removeParticipant(participant->participantId);
            return false;
        }
        return true;
    }

    bool AdmissionPolicy::rollback(std::uint64_t transactionId) { return sessionAllocation.rollback(transactionId); }

    void AdmissionPolicy::disconnect(Network::Trust::ConnectionId connection) {
        std::lock_guard<std::mutex> lock(policyMutex);
        authorization.disconnect(connection);
    }

    bool AdmissionPolicy::authorize(Network::Trust::ConnectionId connection,
                                    Network::Trust::AuthorityAction action,
                                    std::optional<Network::Trust::PlayerSlotId> player) const {
        std::lock_guard<std::mutex> lock(policyMutex);
        return authorization.authorize(connection, action, player);
    }

    void AdmissionPolicy::setMatchStarted(bool started) {
        std::lock_guard<std::mutex> lock(policyMutex);
        matchStarted = started;
    }

    const Network::GameplayManifest &AdmissionPolicy::frozenManifest() const { return manifest; }
    const SessionAllocation &AdmissionPolicy::allocation() const { return sessionAllocation; }
}
