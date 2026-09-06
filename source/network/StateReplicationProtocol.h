#ifndef DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H
#define DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "Protocol.h"
#include "NetworkResponsiveness.h"
#include "SessionTransport.h"
#include "StateReplication.h"

namespace Duel6::Network::Replication {
    constexpr std::uint32_t ReplicationProtocolIdentifier = 0x44365250; // D6RP
    constexpr std::uint16_t ReplicationProtocolVersion = 2;

    enum class ReplicationFrameKind : std::uint16_t {
        FullSnapshot = 1,
        IncrementalUpdate = 2,
        ResynchronizationRequest = 3,
        CanonicalStateMutation = 4,
        QualityProbe = 5,
        QualityResponse = 6
    };

    struct ReplicationFrame {
        ReplicationFrameKind kind = ReplicationFrameKind::ResynchronizationRequest;
        std::optional<FullSnapshot> snapshot;
        std::optional<IncrementalUpdate> update;
        std::optional<std::uint64_t> qualitySequence;
        std::optional<std::uint64_t> authoritativeResponseAt;
    };

    std::vector<std::uint8_t> serializeReplicationSnapshot(const FullSnapshot &snapshot);
    std::vector<std::uint8_t> serializeReplicationUpdate(const IncrementalUpdate &update);
    std::vector<std::uint8_t> serializeResynchronizationRequest();
    std::vector<std::uint8_t> serializeQualityProbe(std::uint64_t sequence);
    std::vector<std::uint8_t> serializeQualityResponse(std::uint64_t sequence,
                                                       std::uint64_t authoritativeResponseAt = 0);
    std::optional<ReplicationFrame> deserializeReplicationFrame(const std::vector<std::uint8_t> &payload) noexcept;

    using ReplicationSender = std::function<SendResult(std::vector<std::uint8_t>)>;
    enum class HostReplicationResult { Accepted, UnknownConnection, InvalidMessage, SessionPolicyViolation, SendFailed };

    // Owns only the replication side of admitted production connections. Admission and reconnect
    // policy supply the stable participant identity and bind the concrete TcpConnection::send seam.
    class AuthoritativeReplicationConnections final {
    public:
        using AuthoritativeClock = std::function<std::uint64_t()>;
        explicit AuthoritativeReplicationConnections(const AuthoritativeStateReplicator &state,
                                                      AuthoritativeClock clock = {});
        bool restore(Identity participantId, ReplicationSender sender, std::function<void()> close = {});
        void disconnect(Identity participantId) noexcept;
        bool broadcast(const IncrementalUpdate &update);
        HostReplicationResult receive(Identity participantId, const std::vector<std::uint8_t> &payload);
        std::size_t size() const noexcept;
    private:
        const AuthoritativeStateReplicator &state;
        AuthoritativeClock clock;
        struct Connection { ReplicationSender sender; std::function<void()> close; };
        std::map<Identity, Connection> connections;
    };

    enum class ClientReplicationResult { Applied, NetworkSampled, WaitingForSnapshot, Reconnecting, SendFailed };

    class ClientReplicationConnection final {
    public:
        explicit ClientReplicationConnection(
                ReplicationSender sender,
                Responsiveness::Environment environment = Responsiveness::Environment::PrivateLan,
                bool requireAuthoritativeTime = false);
        ClientReplicationResult receive(const std::vector<std::uint8_t> &payload);
        ClientReplicationResult receive(const std::vector<std::uint8_t> &payload,
                                        Responsiveness::TimePoint acceptedAt);
        bool observeNetworkSample(const Responsiveness::NetworkSample &sample,
                                  Responsiveness::TimePoint observedAt) noexcept;
        bool sampleNetwork(Responsiveness::TimePoint now);
        void setLocallyControlledPlayers(std::set<Identity> playerIds);
        bool predictLocalMovement(const Responsiveness::PresentedPlayerPose &pose,
                                  Responsiveness::TimePoint sampledAt) noexcept;
        Responsiveness::ConnectionPresentationState presentationState(
                Responsiveness::TimePoint now) noexcept;
        std::vector<Responsiveness::PresentedPlayerPose> presentedPlayers(
                Responsiveness::TimePoint now) noexcept;
        std::vector<PresentationEvent> takePresentationEvents();
        void transportClosed() noexcept;
        const ReplicatedState &replicatedState() const noexcept;
    private:
        ReplicationSender sender;
        ReplicatedState replicated;
        Responsiveness::ConnectionQualityMonitor quality;
        const std::uint64_t maximumAuthoritativeClockUncertainty;
        Responsiveness::CanonicalMovementPresentation movement;
        std::optional<Responsiveness::TimePoint> qualityProbeSentAt;
        std::optional<Responsiveness::TimePoint> lastQualityProbeAt;
        std::optional<Responsiveness::TimePoint> localClockSynchronizedAt;
        std::optional<std::uint64_t> authoritativeClockAtSynchronization;
        std::optional<FullSnapshot> pendingInitialSnapshot;
        std::optional<Responsiveness::TimePoint> pendingInitialSnapshotAcceptedAt;
        std::optional<std::uint64_t> pendingAuthoritativeProducedAt;
        std::optional<Responsiveness::TimePoint> pendingCanonicalAcceptedAt;
        std::deque<bool> qualityProbeOutcomes;
        std::uint64_t qualityProbeSequence = 0;
        std::uint64_t unansweredQualityProbeCount = 0;
        std::uint64_t latestAuthoritativeProducedAt = 0;
        std::uint64_t authoritativeClockUncertainty = 0;
        bool requireAuthoritativeTime = false;
        bool requestPending = false;
        bool reconnecting = false;

        void beginResynchronization() noexcept;
        ClientReplicationResult requestFullSnapshot(bool replacePendingRequest = false);
        void recordQualityOutcome(bool lost, std::chrono::milliseconds roundTripLatency,
                                  Responsiveness::TimePoint observedAt) noexcept;
        std::optional<std::uint64_t> authoritativeTimeAt(
                Responsiveness::TimePoint localTime) const noexcept;
        std::optional<bool> authoritativeProductionTimeIsPlausible(
                std::uint64_t producedAt, Responsiveness::TimePoint acceptedAt) const noexcept;
        std::optional<std::chrono::milliseconds> authoritativeStateAge(
                std::uint64_t producedAt, Responsiveness::TimePoint acceptedAt) const noexcept;
    };
}

#endif
