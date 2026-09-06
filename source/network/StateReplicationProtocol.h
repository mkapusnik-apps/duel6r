#ifndef DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H
#define DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H

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
    constexpr std::uint16_t ReplicationProtocolVersion = 1;

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
    };

    std::vector<std::uint8_t> serializeReplicationSnapshot(const FullSnapshot &snapshot);
    std::vector<std::uint8_t> serializeReplicationUpdate(const IncrementalUpdate &update);
    std::vector<std::uint8_t> serializeResynchronizationRequest();
    std::vector<std::uint8_t> serializeQualityProbe(std::uint64_t sequence);
    std::vector<std::uint8_t> serializeQualityResponse(std::uint64_t sequence);
    std::optional<ReplicationFrame> deserializeReplicationFrame(const std::vector<std::uint8_t> &payload) noexcept;

    using ReplicationSender = std::function<SendResult(std::vector<std::uint8_t>)>;
    enum class HostReplicationResult { Accepted, UnknownConnection, InvalidMessage, SessionPolicyViolation, SendFailed };

    // Owns only the replication side of admitted production connections. Admission and reconnect
    // policy supply the stable participant identity and bind the concrete TcpConnection::send seam.
    class AuthoritativeReplicationConnections final {
    public:
        explicit AuthoritativeReplicationConnections(const AuthoritativeStateReplicator &state);
        bool restore(Identity participantId, ReplicationSender sender, std::function<void()> close = {});
        void disconnect(Identity participantId) noexcept;
        bool broadcast(const IncrementalUpdate &update);
        HostReplicationResult receive(Identity participantId, const std::vector<std::uint8_t> &payload);
        std::size_t size() const noexcept;
    private:
        const AuthoritativeStateReplicator &state;
        struct Connection { ReplicationSender sender; std::function<void()> close; };
        std::map<Identity, Connection> connections;
    };

    enum class ClientReplicationResult { Applied, NetworkSampled, WaitingForSnapshot, Reconnecting, SendFailed };

    class ClientReplicationConnection final {
    public:
        explicit ClientReplicationConnection(
                ReplicationSender sender,
                Responsiveness::Environment environment = Responsiveness::Environment::PrivateLan);
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
        void transportClosed() noexcept;
        const ReplicatedState &replicatedState() const noexcept;
    private:
        ReplicationSender sender;
        ReplicatedState replicated;
        Responsiveness::ConnectionQualityMonitor quality;
        Responsiveness::CanonicalMovementPresentation movement;
        std::optional<Responsiveness::TimePoint> qualityProbeSentAt;
        std::optional<Responsiveness::TimePoint> lastQualityProbeAt;
        std::uint64_t qualityProbeSequence = 0;
        std::uint64_t qualityProbeCount = 0;
        bool requestPending = false;
        bool reconnecting = false;

        void beginResynchronization() noexcept;
    };
}

#endif
