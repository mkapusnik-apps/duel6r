#include "SessionLifecycle.h"

#include <algorithm>
#include <utility>

namespace Duel6::Network::Lifecycle {
    namespace {
        constexpr std::uint32_t Magic = 0x44364c43; // D6LC
        constexpr std::uint16_t Version = 1;
        constexpr std::uint16_t GrantKind = 1;
        constexpr std::uint16_t RequestKind = 2;
        constexpr std::uint16_t HostEndKind = 3;

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
    HostSessionLifecycle::~HostSessionLifecycle() { clearAll(); }

    std::unique_ptr<Trust::ReconnectReservation> HostSessionLifecycle::makeDormantReservation(
            ParticipantId participantId, std::uint64_t reservationId,
            const Trust::ReconnectCredential *disallowed) {
        auto value = std::make_unique<Trust::ReconnectReservation>(
                sessionId, participantId, reservationId, clock, random, false, disallowed);
        return value->valid() ? std::move(value) : nullptr;
    }

    std::optional<ReconnectGrant> HostSessionLifecycle::admitGuest(ParticipantId participantId,
            ConnectionId connectionId, std::vector<PlayerId> ownedPlayers, bool readyValue) {
        if (sessionEnded || participantId == 0 || participantId == hostParticipantId || connectionId == 0
            || ownedPlayers.empty() || participants.size() >= MaximumLifecycleParticipants - 1
            || participants.count(participantId) || nextReservationId == 0
            || retainedPlayerCount() + ownedPlayers.size() > Trust::MaxParticipants) return std::nullopt;
        std::set<PlayerId> unique(ownedPlayers.begin(), ownedPlayers.end());
        if (unique.size() != ownedPlayers.size() || unique.count(0)) return std::nullopt;
        for (const auto &[id, participant]: participants)
            for (PlayerId player: participant.players) if (unique.count(player)) return std::nullopt;
        for (PlayerId player: hostOwnedPlayers) if (unique.count(player)) return std::nullopt;
        const std::uint64_t reservationId = nextReservationId++;
        auto reservation = makeDormantReservation(participantId, reservationId);
        if (!reservation) return std::nullopt;
        ReconnectGrant grant{sessionId, participantId, reservationId, reservation->credential()};
        Participant participant;
        participant.connectionId = connectionId; participant.players = std::move(ownedPlayers);
        participant.ready = readyValue; participant.reservationId = reservationId;
        participant.reservation = std::move(reservation);
        participants.emplace(participantId, std::move(participant));
        return grant;
    }

    bool HostSessionLifecycle::transportClosed(ParticipantId participantId, ConnectionId connectionId) {
        const auto found = participants.find(participantId);
        if (sessionEnded || found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId || !found->second.reservation
            || !found->second.reservation->activate()) return false;
        found->second.connected = false;
        bool disconnected = true;
        try { if (hooks.disconnect) disconnected = hooks.disconnect(participantId); } catch (...) { disconnected = false; }
        if (!disconnected) supervisedHostFailure();
        return disconnected;
    }

    HostReconnectResult HostSessionLifecycle::reconnect(
            const ReconnectRequest &request, ConnectionId connectionId,
            ReconnectCompatibility compatibility) {
        HostReconnectResult result;
        const auto reject = [&](ReconnectOutcome outcome) {
            close(connectionId); result.outcome = outcome; return result;
        };
        if (sessionEnded || connectionId == 0) return reject(ReconnectOutcome::AuthorizationFailed);
        const auto found = participants.find(request.participantId);
        if (found == participants.end() || found->second.connected || !found->second.reservation)
            return reject(ReconnectOutcome::AuthorizationFailed);
        Participant &participant = found->second;
        if (request.sessionId != sessionId || request.reservationId != participant.reservationId)
            return reject(ReconnectOutcome::AuthorizationFailed);
        const auto deadline = participant.reservation->deadline();
        if (!deadline) return reject(ReconnectOutcome::AuthorizationFailed);
        if (clock() >= *deadline) {
            participant.reservation->expireIfDue();
            return reject(ReconnectOutcome::AuthorizationFailed);
        }
        if (nextReservationId == 0) return reject(ReconnectOutcome::RetryableFailure);
        const std::uint64_t replacementId = nextReservationId++;
        auto replacement = makeDormantReservation(request.participantId, replacementId, &request.credential);
        if (!replacement) return reject(ReconnectOutcome::RetryableFailure);
        const auto authorization = participant.reservation->authorizeAndConsume(
                request.credential, request.sessionId, request.participantId, request.reservationId);
        if (!authorization.accepted) return reject(ReconnectOutcome::AuthorizationFailed);
        if (compatibility != ReconnectCompatibility::Compatible) {
            pendingLeaves.insert(request.participantId);
            if (compatibility == ReconnectCompatibility::ReleaseMismatch)
                return reject(ReconnectOutcome::ReleaseMismatch);
            if (compatibility == ReconnectCompatibility::ContentMismatch)
                return reject(ReconnectOutcome::ContentMismatch);
            return reject(ReconnectOutcome::AuthorizationFailed);
        }
        bool restored = false;
        try { restored = hooks.restoreCurrent && hooks.restoreCurrent(request.participantId, connectionId); }
        catch (...) { restored = false; }
        if (!restored) {
            participant.reservation.reset();
            pendingLeaves.insert(request.participantId);
            return reject(ReconnectOutcome::RestoreFailed);
        }
        participant.connectionId = connectionId; participant.connected = true;
        participant.reservationId = replacementId; participant.reservation = std::move(replacement);
        result.outcome = ReconnectOutcome::Accepted; result.closeOffendingConnection = false;
        result.nextGrant = ReconnectGrant{sessionId, request.participantId, replacementId,
                                          participant.reservation->credential()};
        return result;
    }

    bool HostSessionLifecycle::queueIntentionalLeave(
            ParticipantId participantId, ConnectionId connectionId) {
        const auto found = participants.find(participantId);
        if (sessionEnded || found == participants.end() || !found->second.connected
            || found->second.connectionId != connectionId) return false;
        return pendingLeaves.insert(participantId).second;
    }

    bool HostSessionLifecycle::queueReservedLeave(
            const ReconnectRequest &request, ConnectionId attemptConnectionId) {
        const auto found = participants.find(request.participantId);
        if (sessionEnded || found == participants.end() || found->second.connected || !found->second.reservation
            || request.sessionId != sessionId || request.reservationId != found->second.reservationId) {
            close(attemptConnectionId); return false;
        }
        if (!found->second.reservation->consume(request.credential, request.sessionId,
                                                request.participantId, request.reservationId)) {
            close(attemptConnectionId); return false;
        }
        return pendingLeaves.insert(request.participantId).second;
    }

    RemovalOutcome HostSessionLifecycle::processLifecycleBatch(Phase phase) {
        if (sessionEnded || phase == Phase::Ended) return RemovalOutcome::NothingChanged;
        std::set<ParticipantId> removals = pendingLeaves;
        for (auto &[id, participant]: participants)
            if (!participant.connected && participant.reservation && participant.reservation->expireIfDue())
                removals.insert(id);
        pendingLeaves.clear();
        if (removals.empty()) return RemovalOutcome::NothingChanged;
        std::vector<ParticipantId> batch(removals.begin(), removals.end());
        bool applied = false;
        try { applied = hooks.removeBatch && hooks.removeBatch(batch, phase); } catch (...) { applied = false; }
        if (!applied) {
            pendingLeaves = std::move(removals);
            return RemovalOutcome::Failed;
        }
        for (ParticipantId id: batch) {
            const auto found = participants.find(id);
            if (found == participants.end()) continue;
            if (found->second.connected) close(found->second.connectionId);
            found->second.reservation.reset();
            participants.erase(found);
        }
        const std::size_t roster = retainedPlayerCount();
        if (phase == Phase::Lobby || ((phase == Phase::ActiveRound
            || phase == Phase::NonFinalRoundSummary) && roster < 2))
            for (auto &[id, participant]: participants) participant.ready = false;
        if (phase == Phase::Lobby) return RemovalOutcome::LobbyUpdated;
        if (phase == Phase::FinalSummary) return RemovalOutcome::FinalSummaryRetained;
        if (roster < 2) return RemovalOutcome::InterruptedToLobby;
        if (phase == Phase::NonFinalRoundSummary) return RemovalOutcome::NextRoundContinues;
        return RemovalOutcome::ActiveRoundContinues;
    }

    HostEndResult HostSessionLifecycle::endSession(ParticipantId participantId, ConnectionId connectionId) {
        HostEndResult result;
        if (sessionEnded || participantId != hostParticipantId || connectionId != hostConnectionId) return result;
        result.accepted = true; result.notice.sessionId = sessionId;
        const auto payload = serializeIntentionalHostEnd(result.notice);
        for (const auto &[id, participant]: participants) if (participant.connected) {
            bool accepted = false;
            try {
                accepted = hooks.sendIntentionalHostEnd
                           && hooks.sendIntentionalHostEnd(participant.connectionId, payload);
            } catch (...) { accepted = false; }
            if (accepted) result.establishedGuestConnections.push_back(participant.connectionId);
        }
        sessionEnded = true;
        clearAll();
        try { if (hooks.discardSession) hooks.discardSession(); } catch (...) {}
        return result;
    }

    std::string_view HostSessionLifecycle::supervisedHostFailure() {
        if (sessionEnded) return {};
        sessionEnded = true; clearAll();
        try { if (hooks.discardSession) hooks.discardSession(); } catch (...) {}
        return HostedServiceStoppedCopy;
    }
    void HostSessionLifecycle::shutdown() {
        if (sessionEnded) return;
        sessionEnded = true; clearAll();
        try { if (hooks.discardSession) hooks.discardSession(); } catch (...) {}
    }

    bool HostSessionLifecycle::connected(ParticipantId participantId) const noexcept {
        const auto found = participants.find(participantId);
        return found != participants.end() && found->second.connected;
    }
    bool HostSessionLifecycle::reserved(ParticipantId participantId) {
        const auto found = participants.find(participantId);
        return found != participants.end() && !found->second.connected && found->second.reservation
               && found->second.reservation->active();
    }
    bool HostSessionLifecycle::ready(ParticipantId participantId) const noexcept {
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
        for (auto &[id, participant]: participants) {
            participant.reservation.reset();
            if (participant.connected) close(participant.connectionId);
        }
        participants.clear();
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
        if (currentJourney != GuestJourney::Established || !grant) return false;
        disconnectedAt = hostClockNow; deadline = hostClockNow + ReconnectWindow;
        retainedCompleteState = retainedState; currentJourney = GuestJourney::Reconnecting;
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
        const auto remaining = *deadline - now;
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
        return static_cast<unsigned>((millis + 999) / 1000);
    }
    std::string_view GuestSessionRecovery::failureCopy() const noexcept {
        return currentJourney == GuestJourney::ConnectionFailure ? reconnectCopy(terminalOutcome) : std::string_view{};
    }
}
