#ifndef DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H
#define DUEL6_NETWORK_STATEREPLICATIONPROTOCOL_H

#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "Protocol.h"
#include "SessionTransport.h"
#include "StateReplication.h"

namespace Duel6::Network::Replication {
    constexpr std::uint32_t ReplicationProtocolIdentifier = 0x44365250; // D6RP
    constexpr std::uint16_t ReplicationProtocolVersion = 1;

    enum class ReplicationFrameKind : std::uint16_t {
        FullSnapshot = 1,
        IncrementalUpdate = 2,
        ResynchronizationRequest = 3,
        CanonicalStateMutation = 4
    };

    struct ReplicationFrame {
        ReplicationFrameKind kind = ReplicationFrameKind::ResynchronizationRequest;
        std::optional<FullSnapshot> snapshot;
        std::optional<IncrementalUpdate> update;
    };

    std::vector<std::uint8_t> serializeReplicationSnapshot(const FullSnapshot &snapshot);
    std::vector<std::uint8_t> serializeReplicationUpdate(const IncrementalUpdate &update);
    std::vector<std::uint8_t> serializeResynchronizationRequest();
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

    enum class ClientReplicationResult { Applied, WaitingForSnapshot, Reconnecting, SendFailed };

    class ClientReplicationConnection final {
    public:
        explicit ClientReplicationConnection(ReplicationSender sender);
        ClientReplicationResult receive(const std::vector<std::uint8_t> &payload);
        void transportClosed() noexcept;
        const ReplicatedState &replicatedState() const noexcept;
    private:
        ReplicationSender sender;
        ReplicatedState replicated;
        bool requestPending = false;
        bool reconnecting = false;
    };
}

#endif
