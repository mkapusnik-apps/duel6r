#include "SessionLifecycle.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Duel6::Network::Lifecycle {
    namespace {
        constexpr std::uint32_t Magic = 0x44364c43; // D6LC
        constexpr std::uint16_t Version = 1;
        constexpr std::uint16_t GrantKind = 1;
        constexpr std::uint16_t RequestKind = 2;
        constexpr std::uint16_t HostEndKind = 3;
        constexpr std::uint16_t AttemptKind = 4;
        constexpr std::uint16_t ResponseKind = 5;
        constexpr std::uint16_t ParticipantActionKindValue = 6;

        class OperationGuard final {
        public:
            explicit OperationGuard(bool &active) : active(active) { active = true; }
            ~OperationGuard() { active = false; }
        private:
            bool &active;
        };

        TimePoint realNow() { return std::chrono::steady_clock::now(); }

        void append16(std::vector<std::uint8_t> &out, std::uint16_t value) {
            out.push_back(static_cast<std::uint8_t>(value >> 8u));
            out.push_back(static_cast<std::uint8_t>(value));
        }
        void append32(std::vector<std::uint8_t> &out, std::uint32_t value) {
            for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
        }
        void append64(std::vector<std::uint8_t> &out, std::uint64_t value) {
            for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
        }
        bool read16(const std::vector<std::uint8_t> &in, std::size_t &at, std::uint16_t &value) {
            if (at + 2 > in.size()) return false;
            value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[at]) << 8u) | in[at + 1]);
            at += 2; return true;
        }
        bool read32(const std::vector<std::uint8_t> &in, std::size_t &at, std::uint32_t &value) {
            if (at + 4 > in.size()) return false;
            value = 0; for (int i = 0; i < 4; ++i) value = (value << 8u) | in[at++];
            return true;
        }
        bool read64(const std::vector<std::uint8_t> &in, std::size_t &at, std::uint64_t &value) {
            if (at + 8 > in.size()) return false;
            value = 0; for (int i = 0; i < 8; ++i) value = (value << 8u) | in[at++];
            return true;
        }
        std::vector<std::uint8_t> serializeCredential(std::uint16_t kind, std::uint64_t session,
                ParticipantId participant, std::uint64_t reservation, const Trust::ReconnectCredential &credential) {
            bool nonzero = false; for (auto byte: credential.bytes) nonzero = nonzero || byte != 0;
            if (session == 0 || participant == 0 || reservation == 0 || !nonzero) return {};
            std::vector<std::uint8_t> out;
            out.reserve(48); append32(out, Magic); append16(out, Version); append16(out, kind);
            append64(out, session); append64(out, participant); append64(out, reservation);
            out.insert(out.end(), credential.bytes.begin(), credential.bytes.end());
            return out;
        }
        bool validOutcome(std::uint16_t value) {
            return value <= static_cast<std::uint16_t>(ReconnectOutcome::RestoreFailed);
        }
        template<typename T>
        std::optional<T> deserializeCredential(const std::vector<std::uint8_t> &payload, std::uint16_t expected) {
            if (payload.size() != 48) return std::nullopt;
            std::size_t at = 0; std::uint32_t magic = 0; std::uint16_t version = 0, kind = 0;
            T value;
            if (!read32(payload, at, magic) || !read16(payload, at, version) || !read16(payload, at, kind)
                || !read64(payload, at, value.sessionId) || !read64(payload, at, value.participantId)
                || !read64(payload, at, value.reservationId) || magic != Magic || version != Version
                || kind != expected || value.sessionId == 0 || value.participantId == 0
                || value.reservationId == 0) return std::nullopt;
            std::copy(payload.begin() + static_cast<std::ptrdiff_t>(at), payload.end(), value.credential.bytes.begin());
            bool nonzero = false; for (auto byte: value.credential.bytes) nonzero = nonzero || byte != 0;
            if (!nonzero) return std::nullopt;
            return value;
        }
    }

    std::string_view reconnectCopy(ReconnectOutcome outcome) noexcept {
        switch (outcome) {
            case ReconnectOutcome::AuthorizationFailed: return Trust::ReconnectAuthorizationFailureCopy;
            case ReconnectOutcome::ReservationUnavailable:
            case ReconnectOutcome::RestoreFailed: return ReservationUnavailableCopy;
            case ReconnectOutcome::ReleaseMismatch: return ReleaseMismatchCopy;
            case ReconnectOutcome::ContentMismatch: return ContentMismatchCopy;
            case ReconnectOutcome::Expired: return ReconnectExpiredCopy;
            default: return {};
        }
    }

    std::vector<std::uint8_t> serializeReconnectGrant(const ReconnectGrant &grant) {
        return serializeCredential(GrantKind, grant.sessionId, grant.participantId,
                                   grant.reservationId, grant.credential);
    }
    std::vector<std::uint8_t> serializeReconnectRequest(const ReconnectRequest &request) {
        return serializeCredential(RequestKind, request.sessionId, request.participantId,
                                   request.reservationId, request.credential);
    }
    std::vector<std::uint8_t> serializeIntentionalHostEnd(const IntentionalHostEndNotice &notice) {
        if (notice.sessionId == 0) return {};
        std::vector<std::uint8_t> out; out.reserve(16);
        append32(out, Magic); append16(out, Version); append16(out, HostEndKind); append64(out, notice.sessionId);
        return out;
    }
    std::vector<std::uint8_t> serializeReconnectAttempt(const ReconnectAttempt &attempt) {
        auto request = serializeReconnectRequest(attempt.request);
        if (request.empty()) return {};
        auto compatibility = serializeAdmissionRequest(attempt.compatibility);
        if (compatibility.empty() || compatibility.size() > Trust::MaxAdmissionPayloadBytes
            || compatibility.size() > std::numeric_limits<std::uint32_t>::max()) {
            eraseLifecycleCredentialPayload(request);
            return {};
        }
        std::vector<std::uint8_t> out;
        out.reserve(12 + request.size() + compatibility.size());
        append32(out, Magic); append16(out, Version); append16(out, AttemptKind);
        append32(out, static_cast<std::uint32_t>(compatibility.size()));
        out.insert(out.end(), request.begin() + 8, request.end());
        out.insert(out.end(), compatibility.begin(), compatibility.end());
        eraseLifecycleCredentialPayload(request);
        return out;
    }
    std::vector<std::uint8_t> serializeReconnectResponse(const ReconnectResponse &response) {
        if (!validOutcome(static_cast<std::uint16_t>(response.outcome))
            || response.sessionId == 0 || response.participantId == 0
            || (response.outcome == ReconnectOutcome::Accepted) != response.nextGrant.has_value()
            || (response.nextGrant && (response.nextGrant->sessionId != response.sessionId
                || response.nextGrant->participantId != response.participantId))) return {};
        std::vector<std::uint8_t> out;
        out.reserve(response.nextGrant ? 52 : 28);
        append32(out, Magic); append16(out, Version); append16(out, ResponseKind);
        append16(out, static_cast<std::uint16_t>(response.outcome)); append16(out, 0);
        append64(out, response.sessionId); append64(out, response.participantId);
        if (response.nextGrant) {
            append64(out, response.nextGrant->reservationId);
            out.insert(out.end(), response.nextGrant->credential.bytes.begin(),
                       response.nextGrant->credential.bytes.end());
        }
        return out;
    }
    std::vector<std::uint8_t> serializeParticipantAction(const ParticipantAction &action) {
        if (action.sessionId == 0 || action.participantId == 0
            || action.kind < ParticipantActionKind::Ready || action.kind > ParticipantActionKind::Leave)
            return {};
        std::vector<std::uint8_t> out;
        out.reserve(26);
        append32(out, Magic); append16(out, Version); append16(out, ParticipantActionKindValue);
        append16(out, static_cast<std::uint16_t>(action.kind));
        append64(out, action.sessionId); append64(out, action.participantId);
        return out;
    }
    std::optional<ReconnectGrant> deserializeReconnectGrant(const std::vector<std::uint8_t> &payload) noexcept {
        try { return deserializeCredential<ReconnectGrant>(payload, GrantKind); } catch (...) { return std::nullopt; }
    }
    std::optional<ReconnectRequest> deserializeReconnectRequest(const std::vector<std::uint8_t> &payload) noexcept {
        try { return deserializeCredential<ReconnectRequest>(payload, RequestKind); } catch (...) { return std::nullopt; }
    }
    std::optional<IntentionalHostEndNotice> deserializeIntentionalHostEnd(
            const std::vector<std::uint8_t> &payload) noexcept {
        try {
            if (payload.size() != 16) return std::nullopt;
            std::size_t at = 0; std::uint32_t magic = 0; std::uint16_t version = 0, kind = 0;
            IntentionalHostEndNotice value;
            if (!read32(payload, at, magic) || !read16(payload, at, version) || !read16(payload, at, kind)
                || !read64(payload, at, value.sessionId) || magic != Magic || version != Version
                || kind != HostEndKind || value.sessionId == 0) return std::nullopt;
            return value;
        } catch (...) { return std::nullopt; }
    }
    std::optional<ReconnectAttempt> deserializeReconnectAttempt(
            const std::vector<std::uint8_t> &payload) noexcept {
        try {
            if (payload.size() < 52) return std::nullopt;
            std::size_t at = 0; std::uint32_t magic = 0, compatibilitySize = 0;
            std::uint16_t version = 0, kind = 0;
            if (!read32(payload, at, magic) || !read16(payload, at, version) || !read16(payload, at, kind)
                || !read32(payload, at, compatibilitySize) || magic != Magic || version != Version
                || kind != AttemptKind || compatibilitySize == 0
                || compatibilitySize > Trust::MaxAdmissionPayloadBytes
                || payload.size() != 52u + compatibilitySize) return std::nullopt;
            std::vector<std::uint8_t> requestPayload;
            requestPayload.reserve(48);
            append32(requestPayload, Magic); append16(requestPayload, Version); append16(requestPayload, RequestKind);
            requestPayload.insert(requestPayload.end(), payload.begin() + 12, payload.begin() + 52);
            auto request = deserializeReconnectRequest(requestPayload);
            eraseLifecycleCredentialPayload(requestPayload);
            if (!request) return std::nullopt;
            std::vector<std::uint8_t> compatibilityPayload(payload.begin() + 52, payload.end());
            ReconnectAttempt result{std::move(*request), deserializeAdmissionRequest(compatibilityPayload)};
            return result;
        } catch (...) { return std::nullopt; }
    }
    std::optional<ReconnectResponse> deserializeReconnectResponse(
            const std::vector<std::uint8_t> &payload) noexcept {
        try {
            if (payload.size() != 28 && payload.size() != 52) return std::nullopt;
            std::size_t at = 0; std::uint32_t magic = 0; std::uint16_t version = 0, kind = 0, outcome = 0, reserved = 0;
            ReconnectResponse result;
            if (!read32(payload, at, magic) || !read16(payload, at, version) || !read16(payload, at, kind)
                || !read16(payload, at, outcome) || !read16(payload, at, reserved)
                || !read64(payload, at, result.sessionId) || !read64(payload, at, result.participantId)
                || magic != Magic || version != Version || kind != ResponseKind || reserved != 0
                || !validOutcome(outcome) || result.sessionId == 0 || result.participantId == 0)
                return std::nullopt;
            result.outcome = static_cast<ReconnectOutcome>(outcome);
            if (payload.size() == 52) {
                ReconnectGrant grant;
                grant.sessionId = result.sessionId; grant.participantId = result.participantId;
                if (!read64(payload, at, grant.reservationId) || grant.reservationId == 0) return std::nullopt;
                std::copy(payload.begin() + static_cast<std::ptrdiff_t>(at), payload.end(), grant.credential.bytes.begin());
                bool nonzero = false; for (auto byte: grant.credential.bytes) nonzero = nonzero || byte != 0;
                if (!nonzero) return std::nullopt;
                result.nextGrant = std::move(grant);
            }
            if ((result.outcome == ReconnectOutcome::Accepted) != result.nextGrant.has_value()) return std::nullopt;
            return result;
        } catch (...) { return std::nullopt; }
    }
    std::optional<ParticipantAction> deserializeParticipantAction(
            const std::vector<std::uint8_t> &payload) noexcept {
        try {
            if (payload.size() != 26) return std::nullopt;
            std::size_t at = 0;
            std::uint32_t magic = 0;
            std::uint16_t version = 0, kind = 0, actionKind = 0;
            ParticipantAction action;
            if (!read32(payload, at, magic) || !read16(payload, at, version) || !read16(payload, at, kind)
                || !read16(payload, at, actionKind) || !read64(payload, at, action.sessionId)
                || !read64(payload, at, action.participantId) || magic != Magic || version != Version
                || kind != ParticipantActionKindValue || action.sessionId == 0 || action.participantId == 0
                || actionKind < static_cast<std::uint16_t>(ParticipantActionKind::Ready)
                || actionKind > static_cast<std::uint16_t>(ParticipantActionKind::Leave)) return std::nullopt;
            action.kind = static_cast<ParticipantActionKind>(actionKind);
            return action;
        } catch (...) { return std::nullopt; }
    }
    void eraseLifecycleCredentialPayload(std::vector<std::uint8_t> &payload) noexcept {
        if (payload.size() < 8 || payload[0] != 0x44 || payload[1] != 0x36
            || payload[2] != 0x4c || payload[3] != 0x43) return;
        if (payload.size() == 48 && payload[6] == 0
            && (payload[7] == GrantKind || payload[7] == RequestKind))
            Trust::secureEraseMemory(payload.data() + 32, 16);
        else if (payload.size() >= 52 && payload[6] == 0 && payload[7] == AttemptKind)
            Trust::secureEraseMemory(payload.data() + 36, 16);
        else if (payload.size() == 52 && payload[6] == 0 && payload[7] == ResponseKind)
            Trust::secureEraseMemory(payload.data() + 36, 16);
    }

    HostSessionLifecycle::HostSessionLifecycle(std::uint64_t sessionId, ParticipantId hostParticipantId,
            ConnectionId hostConnectionId, std::vector<PlayerId> hostOwnedPlayers, Clock clock,
            Trust::RandomFill random, HostHooks hooks)
            : sessionId(sessionId), hostParticipantId(hostParticipantId), hostConnectionId(hostConnectionId),
              hostOwnedPlayers(std::move(hostOwnedPlayers)),
              clock(clock ? std::move(clock) : Clock(realNow)), random(std::move(random)), hooks(std::move(hooks)) {
        const std::set<PlayerId> unique(this->hostOwnedPlayers.begin(), this->hostOwnedPlayers.end());
        if (sessionId == 0 || hostParticipantId == 0 || hostConnectionId == 0
            || this->hostOwnedPlayers.empty() || this->hostOwnedPlayers.size() > Trust::MaxParticipants
            || unique.size() != this->hostOwnedPlayers.size() || unique.count(0)) sessionEnded = true;
    }
    HostSessionLifecycle::~HostSessionLifecycle() {
        sessionEnded = true;
        operationActive = true;
        clearAll();
    }

    std::unique_ptr<Trust::ReconnectReservation> HostSessionLifecycle::makeDormantReservation(
            ParticipantId participantId, std::uint64_t reservationId,
            const Trust::ReconnectCredential *disallowed) {
        auto value = std::make_unique<Trust::ReconnectReservation>(
                sessionId, participantId, reservationId, clock, random, false, disallowed);
        return value->valid() ? std::move(value) : nullptr;
    }

    std::optional<ReconnectGrant> HostSessionLifecycle::admitGuest(ParticipantId participantId,
            ConnectionId connectionId, std::vector<PlayerId> ownedPlayers, bool readyValue) {
        if (sessionEnded || operationActive || participantId == 0 || participantId == hostParticipantId || connectionId == 0
            || ownedPlayers.empty() || participants.size() >= MaximumLifecycleParticipants - 1
            || participants.count(participantId) || nextReservationId == 0
            || retainedPlayerCount() + ownedPlayers.size() > Trust::MaxParticipants) return std::nullopt;
        std::set<PlayerId> unique(ownedPlayers.begin(), ownedPlayers.end());
        if (unique.size() != ownedPlayers.size() || unique.count(0)) return std::nullopt;
        for (const auto &[id, participant]: participants)
            for (PlayerId player: participant.players) if (unique.count(player)) return std::nullopt;
        for (PlayerId player: hostOwnedPlayers) if (unique.count(player)) return std::nullopt;
        OperationGuard operation(operationActive);
        const std::uint64_t reservationId = nextReservationId++;
        auto reservation = makeDormantReservation(participantId, reservationId);
        if (!reservation) return std::nullopt;
        ReconnectGrant grant{sessionId, participantId, reservationId, reservation->credential()};
        (void) readyValue;
        hostReady = false;
        for (auto &[id, participant]: participants) participant.ready = false;
        Participant participant;
        participant.connectionId = connectionId; participant.players = std::move(ownedPlayers);
        participant.ready = false; participant.reservationId = reservationId;
        participant.reservation = std::move(reservation);
        participants.emplace(participantId, std::move(participant));
        return grant;
    }

    bool HostSessionLifecycle::transportClosed(ParticipantId participantId, ConnectionId connectionId) {
        const auto found = participants.find(participantId);
        if (sessionEnded || operationActive || found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId || !found->second.reservation) return false;
        OperationGuard operation(operationActive);
        if (!found->second.reservation->activate()) return false;
        found->second.connected = false;
        bool disconnected = true;
        try { if (hooks.disconnect) disconnected = hooks.disconnect(participantId); } catch (...) { disconnected = false; }
        if (!disconnected) failSession();
        return disconnected;
    }

    HostReconnectResult HostSessionLifecycle::reconnect(
            const ReconnectRequest &request, ConnectionId connectionId,
            ReconnectCompatibility compatibility) {
        HostReconnectResult result;
        const auto reject = [&](ReconnectOutcome outcome) {
            close(connectionId); result.outcome = outcome; return result;
        };
        if (operationActive) return reject(ReconnectOutcome::AuthorizationFailed);
        if (sessionEnded || connectionId == 0)
            return reject(ReconnectOutcome::AuthorizationFailed);
        OperationGuard operation(operationActive);
        const auto found = participants.find(request.participantId);
        if (found == participants.end() || found->second.connected || !found->second.reservation)
            return reject(ReconnectOutcome::AuthorizationFailed);
        if (request.sessionId != sessionId || request.reservationId != found->second.reservationId)
            return reject(ReconnectOutcome::AuthorizationFailed);
        std::optional<TimePoint> deadline;
        try { deadline = found->second.reservation->deadline(); } catch (...) {}
        if (!deadline) return reject(ReconnectOutcome::AuthorizationFailed);
        TimePoint now = TimePoint::max();
        try { now = clock(); } catch (...) {}
        if (now >= *deadline) {
            try { found->second.reservation->expireIfDue(); } catch (...) {}
            return reject(ReconnectOutcome::AuthorizationFailed);
        }
        Trust::ReconnectAuthorizationResult proof;
        try {
            proof = found->second.reservation->authorize(
                    request.credential, request.sessionId, request.participantId, request.reservationId);
        } catch (...) {}
        if (!proof.accepted) return reject(ReconnectOutcome::AuthorizationFailed);
        if (compatibility != ReconnectCompatibility::Compatible) {
            if (compatibility == ReconnectCompatibility::ReleaseMismatch)
                return reject(ReconnectOutcome::ReleaseMismatch);
            if (compatibility == ReconnectCompatibility::ContentMismatch)
                return reject(ReconnectOutcome::ContentMismatch);
            return reject(ReconnectOutcome::AuthorizationFailed);
        }
        if (nextReservationId == 0) return reject(ReconnectOutcome::RetryableFailure);
        const std::uint64_t replacementId = nextReservationId++;
        auto replacement = makeDormantReservation(request.participantId, replacementId, &request.credential);
        if (!replacement) return reject(ReconnectOutcome::RetryableFailure);
        const auto authorization = found->second.reservation->authorizeAndSuspend(
                request.credential, request.sessionId, request.participantId, request.reservationId);
        if (!authorization.accepted) return reject(ReconnectOutcome::AuthorizationFailed);
        bool restored = false;
        try { restored = hooks.restoreCurrent && hooks.restoreCurrent(request.participantId, connectionId); }
        catch (...) { restored = false; }
        TimePoint restoredAt = TimePoint::max();
        try { restoredAt = clock(); } catch (...) {}
        const auto current = participants.find(request.participantId);
        if (!restored || restoredAt >= *deadline || sessionEnded || current == participants.end()) {
            try { if (hooks.disconnect) (void) hooks.disconnect(request.participantId); } catch (...) {}
            if (current != participants.end()) {
                (void) current->second.reservation->consumeSuspended();
                current->second.reservation.reset();
            }
            pendingLeaves.insert(request.participantId);
            return reject(restoredAt >= *deadline ? ReconnectOutcome::Expired : ReconnectOutcome::RestoreFailed);
        }
        current->second.connectionId = connectionId; current->second.connected = true;
        current->second.rollbackReservationId = current->second.reservationId;
        current->second.rollbackReservation = std::move(current->second.reservation);
        current->second.reservationId = replacementId; current->second.reservation = std::move(replacement);
        result.outcome = ReconnectOutcome::Accepted; result.closeOffendingConnection = false;
        result.nextGrant = ReconnectGrant{sessionId, request.participantId, replacementId,
                                          current->second.reservation->credential()};
        return result;
    }

    bool HostSessionLifecycle::queueIntentionalLeave(
            ParticipantId participantId, ConnectionId connectionId) {
        const auto found = participants.find(participantId);
        if (sessionEnded || operationActive || found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId) return false;
        OperationGuard operation(operationActive);
        return pendingLeaves.insert(participantId).second;
    }

    bool HostSessionLifecycle::queueReservedLeave(
            const ReconnectRequest &request, ConnectionId attemptConnectionId) {
        if (operationActive) {
            close(attemptConnectionId);
            return false;
        }
        const auto found = participants.find(request.participantId);
        if (sessionEnded || found == participants.end() || found->second.connected || !found->second.reservation
            || request.sessionId != sessionId || request.reservationId != found->second.reservationId) {
            close(attemptConnectionId); return false;
        }
        OperationGuard operation(operationActive);
        if (!found->second.reservation->consume(request.credential, request.sessionId,
                                                request.participantId, request.reservationId)) {
            close(attemptConnectionId); return false;
        }
        const bool queued = pendingLeaves.insert(request.participantId).second;
        close(attemptConnectionId);
        return queued;
    }

    bool HostSessionLifecycle::applyParticipantAction(
            const ParticipantAction &action, ConnectionId connectionId) noexcept {
        try {
            if (action.sessionId != sessionId) return false;
            if (action.kind == ParticipantActionKind::Leave)
                return queueIntentionalLeave(action.participantId, connectionId);
            return setReady(action.participantId, connectionId,
                            action.kind == ParticipantActionKind::Ready);
        } catch (...) { return false; }
    }

    bool HostSessionLifecycle::setReady(
            ParticipantId participantId, ConnectionId connectionId, bool readyValue) noexcept {
        if (sessionEnded || operationActive || participantId == 0 || connectionId == 0) return false;
        OperationGuard operation(operationActive);
        if (participantId == hostParticipantId) {
            if (connectionId != hostConnectionId) return false;
            hostReady = readyValue;
            return true;
        }
        const auto found = participants.find(participantId);
        if (found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId) return false;
        found->second.ready = readyValue;
        return true;
    }

    bool HostSessionLifecycle::clearReadiness() noexcept {
        if (sessionEnded || operationActive) return false;
        OperationGuard operation(operationActive);
        hostReady = false;
        for (auto &[id, participant]: participants) participant.ready = false;
        return true;
    }

    bool HostSessionLifecycle::allConnectedAndReady() const noexcept {
        if (sessionEnded || !hostReady || retainedPlayerCount() < 2
            || retainedPlayerCount() > Trust::MaxParticipants) return false;
        return std::all_of(participants.begin(), participants.end(), [](const auto &entry) {
            return entry.second.connected && entry.second.ready;
        });
    }

    bool HostSessionLifecycle::reconnectDeliverySucceeded(
            ParticipantId participantId, ConnectionId connectionId) noexcept {
        if (sessionEnded || operationActive) return false;
        const auto found = participants.find(participantId);
        if (found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId || !found->second.rollbackReservation) return false;
        OperationGuard operation(operationActive);
        const bool consumed = found->second.rollbackReservation->consumeSuspended();
        found->second.rollbackReservation.reset();
        found->second.rollbackReservationId = 0;
        if (!consumed) failSession();
        return consumed;
    }

    bool HostSessionLifecycle::reconnectDeliveryFailed(
            ParticipantId participantId, ConnectionId connectionId) noexcept {
        if (sessionEnded || operationActive) return false;
        const auto found = participants.find(participantId);
        if (found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId || !found->second.reservation
            || !found->second.rollbackReservation || found->second.rollbackReservationId == 0) return false;
        OperationGuard operation(operationActive);
        found->second.reservation.reset();
        found->second.reservation = std::move(found->second.rollbackReservation);
        found->second.reservationId = found->second.rollbackReservationId;
        found->second.rollbackReservationId = 0;
        const bool reservationRestored = found->second.reservation->restoreSuspended();
        if (!reservationRestored) pendingLeaves.insert(participantId);
        found->second.connected = false;
        bool disconnected = true;
        try { if (hooks.disconnect) disconnected = hooks.disconnect(participantId); }
        catch (...) { disconnected = false; }
        if (!disconnected) failSession();
        close(connectionId);
        return reservationRestored && disconnected;
    }

    RemovalOutcome HostSessionLifecycle::processLifecycleBatch(Phase phase) {
        if (sessionEnded || operationActive || phase == Phase::Ended) return RemovalOutcome::NothingChanged;
        OperationGuard operation(operationActive);
        std::set<ParticipantId> removals = pendingLeaves;
        try {
            for (auto &[id, participant]: participants)
                if (!participant.connected && participant.reservation && participant.reservation->expireIfDue())
                    removals.insert(id);
        } catch (...) {
            pendingLeaves.insert(removals.begin(), removals.end());
            return RemovalOutcome::Failed;
        }
        pendingLeaves.clear();
        if (removals.empty()) return RemovalOutcome::NothingChanged;
        std::vector<ParticipantId> batch(removals.begin(), removals.end());
        bool applied = false;
        try { applied = hooks.removeBatch && hooks.removeBatch(batch, phase); } catch (...) { applied = false; }
        if (!applied) {
            pendingLeaves.insert(removals.begin(), removals.end());
            return RemovalOutcome::Failed;
        }
        std::vector<ConnectionId> connectionsToClose;
        for (ParticipantId id: batch) {
            const auto found = participants.find(id);
            if (found == participants.end()) continue;
            if (found->second.connected) connectionsToClose.push_back(found->second.connectionId);
            found->second.reservation.reset();
            participants.erase(found);
        }
        for (ConnectionId connection: connectionsToClose) close(connection);
        const std::size_t roster = retainedPlayerCount();
        hostReady = false;
        for (auto &[id, participant]: participants) participant.ready = false;
        if (phase == Phase::Lobby) return RemovalOutcome::LobbyUpdated;
        if (phase == Phase::FinalSummary) return RemovalOutcome::FinalSummaryRetained;
        if (roster < 2) return RemovalOutcome::InterruptedToLobby;
        if (phase == Phase::NonFinalRoundSummary) return RemovalOutcome::NextRoundContinues;
        return RemovalOutcome::ActiveRoundContinues;
    }

    HostEndResult HostSessionLifecycle::endSession(ParticipantId participantId, ConnectionId connectionId) {
        HostEndResult result;
        if (sessionEnded || operationActive || participantId != hostParticipantId || connectionId != hostConnectionId)
            return result;
        OperationGuard operation(operationActive);
        result.accepted = true; result.notice.sessionId = sessionId;
        const auto payload = serializeIntentionalHostEnd(result.notice);
        sessionEnded = true;
        std::vector<ConnectionId> established;
        for (const auto &[id, participant]: participants)
            if (participant.connected) established.push_back(participant.connectionId);
        for (ConnectionId connection: established) {
            bool accepted = false;
            try {
                accepted = hooks.sendIntentionalHostEnd
                           && hooks.sendIntentionalHostEnd(connection, payload);
            } catch (...) { accepted = false; }
            if (accepted) result.establishedGuestConnections.push_back(connection);
        }
        clearAll();
        try { if (hooks.discardSession) hooks.discardSession(); } catch (...) {}
        return result;
    }

    std::string_view HostSessionLifecycle::supervisedHostFailure() {
        if (sessionEnded || operationActive) return {};
        OperationGuard operation(operationActive);
        failSession();
        return HostedServiceStoppedCopy;
    }
    void HostSessionLifecycle::shutdown() {
        if (sessionEnded || operationActive) return;
        OperationGuard operation(operationActive);
        failSession();
    }

    bool HostSessionLifecycle::connected(ParticipantId participantId) const noexcept {
        const auto found = participants.find(participantId);
        return found != participants.end() && found->second.connected;
    }
    bool HostSessionLifecycle::reserved(ParticipantId participantId) {
        if (operationActive) return false;
        OperationGuard operation(operationActive);
        const auto found = participants.find(participantId);
        return found != participants.end() && !found->second.connected && found->second.reservation
               && found->second.reservation->active();
    }
    bool HostSessionLifecycle::ready(ParticipantId participantId) const noexcept {
        if (participantId == hostParticipantId) return hostReady;
        const auto found = participants.find(participantId);
        return found != participants.end() && found->second.ready;
    }
    std::size_t HostSessionLifecycle::retainedPlayerCount() const noexcept {
        std::size_t count = hostOwnedPlayers.size();
        for (const auto &[id, participant]: participants) count += participant.players.size();
        return count;
    }
    bool HostSessionLifecycle::ended() const noexcept { return sessionEnded; }
    void HostSessionLifecycle::close(ConnectionId connectionId) noexcept {
        if (!connectionId) return;
        try { if (hooks.closeConnection) hooks.closeConnection(connectionId); } catch (...) {}
    }
    void HostSessionLifecycle::clearAll() noexcept {
        pendingLeaves.clear();
        auto removed = std::move(participants);
        participants.clear();
        for (auto &[id, participant]: removed) {
            participant.reservation.reset();
            if (participant.connected) close(participant.connectionId);
        }
    }
    void HostSessionLifecycle::failSession() noexcept {
        if (sessionEnded) return;
        sessionEnded = true;
        clearAll();
        try { if (hooks.discardSession) hooks.discardSession(); } catch (...) {}
    }

    GuestSessionRecovery::GuestSessionRecovery(std::uint64_t sessionId, ParticipantId participantId,
            ConnectionId establishedConnectionId, bool currentCompleteStateAvailable)
            : sessionId(sessionId), participantId(participantId),
              establishedConnectionId(establishedConnectionId),
              retainedCompleteState(currentCompleteStateAvailable) {
        if (sessionId == 0 || participantId == 0 || establishedConnectionId == 0)
            currentJourney = GuestJourney::ConnectionFailure;
    }
    void GuestSessionRecovery::observeCurrentCompleteState() noexcept {
        if (currentJourney == GuestJourney::Established) retainedCompleteState = true;
    }
    bool GuestSessionRecovery::acceptGrant(ReconnectGrant value) {
        if (currentJourney != GuestJourney::Established || value.sessionId != sessionId
            || value.participantId != participantId || value.reservationId == 0) return false;
        bool nonzero = false; for (auto byte: value.credential.bytes) nonzero = nonzero || byte != 0;
        if (!nonzero) return false;
        grant = std::move(value); return true;
    }
    bool GuestSessionRecovery::transportClosed(TimePoint hostClockNow, bool retainedState) {
        if (currentJourney != GuestJourney::Established) return false;
        disconnectedAt = hostClockNow; deadline = hostClockNow + ReconnectWindow;
        retainedCompleteState = retainedCompleteState || retainedState;
        currentJourney = GuestJourney::Reconnecting;
        return true;
    }
    std::optional<ReconnectRequest> GuestSessionRecovery::retry() const {
        if (currentJourney != GuestJourney::Reconnecting || !grant) return std::nullopt;
        return ReconnectRequest{grant->sessionId, grant->participantId, grant->reservationId, grant->credential};
    }
    bool GuestSessionRecovery::applyReconnectResult(ReconnectOutcome outcome, TimePoint hostClockNow,
            bool currentFullStateAccepted, ConnectionId newEstablishedConnectionId,
            std::optional<ReconnectGrant> nextGrant) {
        if (currentJourney != GuestJourney::Reconnecting) return false;
        update(hostClockNow);
        if (currentJourney != GuestJourney::Reconnecting) return false;
        if (outcome == ReconnectOutcome::RetryableFailure) return true;
        if (outcome == ReconnectOutcome::Accepted) {
            bool nonzeroCredential = false;
            if (nextGrant) for (auto byte: nextGrant->credential.bytes)
                nonzeroCredential = nonzeroCredential || byte != 0;
            if (!currentFullStateAccepted || newEstablishedConnectionId == 0 || !nextGrant
                || nextGrant->sessionId != sessionId
                || nextGrant->participantId != participantId || nextGrant->reservationId == 0
                || !nonzeroCredential || hostClockNow >= *deadline) return false;
            grant = std::move(nextGrant); disconnectedAt.reset(); deadline.reset();
            establishedConnectionId = newEstablishedConnectionId;
            retainedCompleteState = true; currentJourney = GuestJourney::Established; return true;
        }
        terminalOutcome = outcome; currentJourney = GuestJourney::ConnectionFailure;
        grant.reset(); return true;
    }
    bool GuestSessionRecovery::acceptIntentionalHostEnd(const IntentionalHostEndNotice &notice,
            ConnectionId sourceConnectionId, TimePoint receivedAt) {
        if (notice.sessionId != sessionId || sourceConnectionId != establishedConnectionId
            || (currentJourney != GuestJourney::Established
                && (currentJourney != GuestJourney::Reconnecting || !disconnectedAt
                    || receivedAt > *disconnectedAt))) return false;
        currentJourney = GuestJourney::HostEnded; grant.reset(); deadline.reset(); return true;
    }
    void GuestSessionRecovery::leave() noexcept {
        currentJourney = GuestJourney::Left; grant.reset(); deadline.reset(); retainedCompleteState = false;
    }
    void GuestSessionRecovery::update(TimePoint hostClockNow) noexcept {
        if (currentJourney == GuestJourney::Reconnecting && deadline && hostClockNow >= *deadline) {
            terminalOutcome = ReconnectOutcome::Expired; currentJourney = GuestJourney::ConnectionFailure;
            grant.reset();
        }
    }
    GuestJourney GuestSessionRecovery::journey() const noexcept { return currentJourney; }
    Destination GuestSessionRecovery::destination() const noexcept {
        if (currentJourney == GuestJourney::ConnectionFailure) return Destination::ConnectionFailure;
        if (currentJourney == GuestJourney::HostEnded) return Destination::HostEnded;
        if (currentJourney == GuestJourney::Left) return Destination::NetworkRoot;
        return Destination::CurrentSession;
    }
    bool GuestSessionRecovery::retainedContextIsCurrent() const noexcept {
        return retainedCompleteState && currentJourney == GuestJourney::Established;
    }
    bool GuestSessionRecovery::hasRetainedContext() const noexcept {
        return retainedCompleteState && (currentJourney == GuestJourney::Reconnecting
                                          || currentJourney == GuestJourney::HostEnded);
    }
    std::optional<unsigned> GuestSessionRecovery::positiveSecondsRemaining(TimePoint now) const noexcept {
        if (currentJourney != GuestJourney::Reconnecting || !deadline || now >= *deadline) return std::nullopt;
        const auto seconds = std::chrono::ceil<std::chrono::seconds>(*deadline - now).count();
        return static_cast<unsigned>((std::min<std::int64_t>)(seconds,
                static_cast<std::int64_t>((std::numeric_limits<unsigned>::max)())));
    }
    std::string_view GuestSessionRecovery::failureCopy() const noexcept {
        return currentJourney == GuestJourney::ConnectionFailure ? reconnectCopy(terminalOutcome) : std::string_view{};
    }
}
