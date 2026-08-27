#ifndef DUEL6_NETWORK_SESSIONTRANSPORT_H
#define DUEL6_NETWORK_SESSIONTRANSPORT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Protocol.h"

namespace Duel6::Network {
    constexpr std::uint32_t TransportFramingIdentifier = 0x44365254; // D6RT
    constexpr std::uint16_t TransportFramingVersion = 1;
    constexpr std::size_t TransportEnvelopeBytes = 12;
    constexpr std::size_t MaxQueuedTransportFrames = 256;
    constexpr std::size_t MaxQueuedTransportPayloadBytes = 4 * 1024 * 1024;
    constexpr std::size_t MaxTransportConnections = 15;

    enum class ListenerState {
        NotStarted,
        Starting,
        Ready,
        Stopping,
        Stopped,
        Failed,
        Cancelled,
        TimedOut
    };

    enum class ClientState {
        NotStarted,
        Resolving,
        Connecting,
        Connected,
        Closing,
        Closed,
        Failed,
        Cancelled,
        TimedOut
    };

    enum class TransportFailure {
        None,
        InvalidEndpoint,
        BindFailed,
        ResolveFailed,
        ConnectionRefused,
        Unreachable,
        ProtocolViolation,
        PeerClosed,
        InboundStalled,
        OutboundStalled,
        IdleTimedOut,
        SystemError
    };

    enum class SendResult {
        Accepted,
        Backpressure,
        PayloadTooLarge,
        NotConnected,
        Closing
    };

    struct TransportFrame {
        std::vector<std::uint8_t> payload;
    };

    class TcpConnection {
    public:
        ~TcpConnection();

        TcpConnection(const TcpConnection &) = delete;
        TcpConnection &operator=(const TcpConnection &) = delete;

        SendResult send(std::vector<std::uint8_t> payload);
        bool receive(TransportFrame &frame);
        ClientState state() const;
        TransportFailure failure() const;

        // Idempotent. Accepted output is flushed in order for at most two seconds.
        void requestClose();
        void close();

    private:
        class Impl;
        explicit TcpConnection(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl;

        friend class TcpClient;
        friend class TcpListener;
    };

    class TcpClient {
    public:
        TcpClient();
        ~TcpClient();

        TcpClient(const TcpClient &) = delete;
        TcpClient &operator=(const TcpClient &) = delete;

        // Starts one asynchronous attempt. DNS and TCP share one ten-second deadline.
        bool start(const Endpoint &endpoint);
        void cancel();
        void close();
        ClientState state() const;
        TransportFailure failure() const;
        std::shared_ptr<TcpConnection> connection() const;
        bool waitForConnected(std::chrono::milliseconds timeout);

    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };

    class TcpListener {
    public:
        explicit TcpListener(std::size_t maxConnections = MaxTransportConnections);
        ~TcpListener();

        TcpListener(const TcpListener &) = delete;
        TcpListener &operator=(const TcpListener &) = delete;

        // Starts one asynchronous listener startup. Ready means bind/listen succeeded and
        // the accept worker is processing. Startup must complete before ten seconds.
        bool start(const Endpoint &endpoint);
        void cancel();
        void shutdown();
        ListenerState state() const;
        TransportFailure failure() const;
        std::shared_ptr<TcpConnection> acceptConnection();
        bool waitForReady(std::chrono::milliseconds timeout);

    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };
}

#endif
