#ifndef DUEL6_NETWORK_SESSIONLIFECYCLE_H
#define DUEL6_NETWORK_SESSIONLIFECYCLE_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "NetworkTrustPolicy.h"
#include "AdmissionProtocol.h"

namespace Duel6::Network::Lifecycle {
    using ConnectionId = Trust::ConnectionId;
    using ParticipantId = Trust::ParticipantId;
    using PlayerId = Trust::PlayerSlotId;
    using TimePoint = Trust::TimePoint;
    using Clock = Trust::Clock;

    constexpr auto ReconnectWindow = std::chrono::seconds(30);
    constexpr std::size_t MaximumLifecycleParticipants = Trust::MaxParticipants;

    enum class Phase { Lobby, ActiveRound, NonFinalRoundSummary, FinalSummary, Ended };
    enum class GuestJourney { Established, Reconnecting, ConnectionFailure, HostEnded, Left };
    enum class Destination { CurrentSession, NetworkRoot, ConnectionFailure, HostEnded };
    enum class ReconnectOutcome {
        Accepted,
        RetryableFailure,
        AuthorizationFailed,
        ReservationUnavailable,
        ReleaseMismatch,
        ContentMismatch,
        Expired,
        RestoreFailed
    };
    enum class ReconnectCompatibility { Compatible, ReleaseMismatch, ContentMismatch, TrustRejected };
    enum class RemovalOutcome {
        NothingChanged,
        LobbyUpdated,
        ActiveRoundContinues,
        InterruptedToLobby,
        NextRoundContinues,
        FinalSummaryRetained,
        Failed
    };

    inline constexpr std::string_view ReconnectExpiredCopy =
            "Reconnect time expired. The session could not be restored.";
    inline constexpr std::string_view ReservationUnavailableCopy =
            "Reconnect reservation is no longer available. This session cannot be restored.";
    inline constexpr std::string_view ReleaseMismatchCopy =
            "Network release mismatch. This session cannot be restored.";
    inline constexpr std::string_view ContentMismatchCopy =
            "Gameplay content mismatch. This session cannot be restored.";
    inline constexpr std::string_view HostedServiceStoppedCopy =
            "Hosted session stopped unexpectedly.";

    std::string_view reconnectCopy(ReconnectOutcome outcome) noexcept;

    struct ReconnectGrant {
        std::uint64_t sessionId = 0;
        ParticipantId participantId = 0;
        std::uint64_t reservationId = 0;
        Trust::ReconnectCredential credential;
    };

    struct ReconnectRequest {
        std::uint64_t sessionId = 0;
        ParticipantId participantId = 0;
        std::uint64_t reservationId = 0;
        Trust::ReconnectCredential credential;
    };

    struct IntentionalHostEndNotice { std::uint64_t sessionId = 0; };

    struct ReconnectAttempt {
        ReconnectRequest request;
        AdmissionRequest compatibility;
    };

    struct ReconnectResponse {
        std::uint64_t sessionId = 0;
        ParticipantId participantId = 0;
        ReconnectOutcome outcome = ReconnectOutcome::AuthorizationFailed;
        std::optional<ReconnectGrant> nextGrant;
    };

    std::vector<std::uint8_t> serializeReconnectGrant(const ReconnectGrant &grant);
    std::vector<std::uint8_t> serializeReconnectRequest(const ReconnectRequest &request);
    std::vector<std::uint8_t> serializeIntentionalHostEnd(const IntentionalHostEndNotice &notice);
    std::vector<std::uint8_t> serializeReconnectAttempt(const ReconnectAttempt &attempt);
    std::vector<std::uint8_t> serializeReconnectResponse(const ReconnectResponse &response);
    std::optional<ReconnectGrant> deserializeReconnectGrant(const std::vector<std::uint8_t> &payload) noexcept;
    std::optional<ReconnectRequest> deserializeReconnectRequest(const std::vector<std::uint8_t> &payload) noexcept;
    std::optional<IntentionalHostEndNotice> deserializeIntentionalHostEnd(
            const std::vector<std::uint8_t> &payload) noexcept;
    std::optional<ReconnectAttempt> deserializeReconnectAttempt(
            const std::vector<std::uint8_t> &payload) noexcept;
    std::optional<ReconnectResponse> deserializeReconnectResponse(
            const std::vector<std::uint8_t> &payload) noexcept;
    void eraseLifecycleCredentialPayload(std::vector<std::uint8_t> &payload) noexcept;

    struct HostHooks {
        // Disconnect must synchronously revoke participant input and mark replication Reconnecting.
        std::function<bool(ParticipantId)> disconnect;
        // Restore must atomically bind only this participant, send current full state, and then enable input.
        std::function<bool(ParticipantId, ConnectionId)> restoreCurrent;
        // One call represents one same-host-clock atomic lifecycle batch and one winner evaluation at most.
        std::function<bool(const std::vector<ParticipantId> &, Phase)> removeBatch;
        // Must only enqueue the fixed notice; it must not wait for transport delivery.
        std::function<bool(ConnectionId, const std::vector<std::uint8_t> &)> sendIntentionalHostEnd;
        std::function<void(ConnectionId)> closeConnection;
        std::function<void()> discardSession;
    };

    struct HostReconnectResult {
        ReconnectOutcome outcome = ReconnectOutcome::AuthorizationFailed;
        std::optional<ReconnectGrant> nextGrant;
        bool closeOffendingConnection = true;
    };

    struct HostEndResult {
        bool accepted = false;
        IntentionalHostEndNotice notice;
        std::vector<ConnectionId> establishedGuestConnections;
    };

    // Called serially by the authoritative host session loop; peer workers hand events to that loop.
    class HostSessionLifecycle final {
    public:
        HostSessionLifecycle(std::uint64_t sessionId, ParticipantId hostParticipantId,
                             ConnectionId hostConnectionId, std::vector<PlayerId> hostOwnedPlayers,
                             Clock clock = {}, Trust::RandomFill random = {}, HostHooks hooks = {});
        ~HostSessionLifecycle();

        std::optional<ReconnectGrant> admitGuest(ParticipantId participantId, ConnectionId connectionId,
                                                  std::vector<PlayerId> ownedPlayers, bool ready);
        bool transportClosed(ParticipantId participantId, ConnectionId connectionId);
        HostReconnectResult reconnect(const ReconnectRequest &request, ConnectionId connectionId,
                                      ReconnectCompatibility compatibility = ReconnectCompatibility::Compatible);
        bool queueIntentionalLeave(ParticipantId participantId, ConnectionId connectionId);
        bool queueReservedLeave(const ReconnectRequest &request, ConnectionId attemptConnectionId);
        bool clearReadiness() noexcept;
        RemovalOutcome processLifecycleBatch(Phase phase);
        HostEndResult endSession(ParticipantId participantId, ConnectionId connectionId);
        std::string_view supervisedHostFailure();
        void shutdown();

        bool connected(ParticipantId participantId) const noexcept;
        bool reserved(ParticipantId participantId);
        bool ready(ParticipantId participantId) const noexcept;
        std::size_t retainedPlayerCount() const noexcept;
        bool ended() const noexcept;

    private:
        struct Participant {
            ConnectionId connectionId = 0;
            std::vector<PlayerId> players;
            bool connected = true;
            bool ready = false;
            std::uint64_t reservationId = 0;
            std::unique_ptr<Trust::ReconnectReservation> reservation;
        };

        std::uint64_t sessionId;
        ParticipantId hostParticipantId;
        ConnectionId hostConnectionId;
        std::vector<PlayerId> hostOwnedPlayers;
        Clock clock;
        Trust::RandomFill random;
        HostHooks hooks;
        std::map<ParticipantId, Participant> participants;
        std::set<ParticipantId> pendingLeaves;
        std::uint64_t nextReservationId = 1;
        bool sessionEnded = false;
        bool operationActive = false;

        std::unique_ptr<Trust::ReconnectReservation> makeDormantReservation(
                ParticipantId participantId, std::uint64_t reservationId,
                const Trust::ReconnectCredential *disallowed = nullptr);
        void close(ConnectionId connectionId) noexcept;
        void clearAll() noexcept;
        void failSession() noexcept;
    };

    class GuestSessionRecovery final {
    public:
        GuestSessionRecovery(std::uint64_t sessionId, ParticipantId participantId,
                             ConnectionId establishedConnectionId,
                             bool currentCompleteStateAvailable = false);
        void observeCurrentCompleteState() noexcept;
        bool acceptGrant(ReconnectGrant grant);
        bool transportClosed(TimePoint hostClockNow, bool retainedCompleteState);
        std::optional<ReconnectRequest> retry() const;
        bool applyReconnectResult(ReconnectOutcome outcome, TimePoint hostClockNow,
                                  bool currentFullStateAccepted,
                                  ConnectionId newEstablishedConnectionId,
                                  std::optional<ReconnectGrant> nextGrant = std::nullopt);
        bool acceptIntentionalHostEnd(const IntentionalHostEndNotice &notice, ConnectionId sourceConnectionId,
                                      TimePoint receivedAt);
        void leave() noexcept;
        void update(TimePoint hostClockNow) noexcept;

        GuestJourney journey() const noexcept;
        Destination destination() const noexcept;
        bool retainedContextIsCurrent() const noexcept;
        bool hasRetainedContext() const noexcept;
        std::optional<unsigned> positiveSecondsRemaining(TimePoint hostClockNow) const noexcept;
        std::string_view failureCopy() const noexcept;

    private:
        std::uint64_t sessionId;
        ParticipantId participantId;
        ConnectionId establishedConnectionId;
        std::optional<ReconnectGrant> grant;
        std::optional<TimePoint> disconnectedAt;
        std::optional<TimePoint> deadline;
        GuestJourney currentJourney = GuestJourney::Established;
        ReconnectOutcome terminalOutcome = ReconnectOutcome::RetryableFailure;
        bool retainedCompleteState = false;
    };
}

#endif
