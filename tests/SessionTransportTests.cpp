#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "source/network/SessionTransport.h"

#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using RawSocket = SOCKET;
static constexpr RawSocket InvalidRawSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using RawSocket = int;
static constexpr RawSocket InvalidRawSocket = -1;
#endif

namespace {
using namespace Duel6::Network;
using namespace std::chrono_literals;

class Failure : public std::runtime_error { public: using std::runtime_error::runtime_error; };
#define CHECK(value) do { if (!(value)) throw Failure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #value); } while (false)

void closeRaw(RawSocket socket) {
#ifdef D6R_TRANSPORT_WINDOWS
    if (socket != InvalidRawSocket) closesocket(socket);
#else
    if (socket != InvalidRawSocket) ::close(socket);
#endif
}

class RawSocketOwner {
public:
    explicit RawSocketOwner(RawSocket value = InvalidRawSocket) : value(value) {}
    ~RawSocketOwner() { closeRaw(value); }
    RawSocketOwner(const RawSocketOwner &) = delete;
    RawSocket get() const { return value; }
    void close() { closeRaw(value); value = InvalidRawSocket; }
private:
    RawSocket value;
};

bool waitUntil(const std::function<bool()> &condition, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        if (condition()) return true;
        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return condition();
}

std::uint16_t unusedPort() {
    RawSocketOwner socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    CHECK(socket.get() != InvalidRawSocket);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(::bind(socket.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
#ifdef D6R_TRANSPORT_WINDOWS
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    CHECK(getsockname(socket.get(), reinterpret_cast<sockaddr *>(&address), &size) == 0);
    return ntohs(address.sin_port);
}

RawSocket connectRaw(std::uint16_t port) {
    RawSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(socket != InvalidRawSocket);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        closeRaw(socket);
        throw Failure("raw loopback connect failed");
    }
    return socket;
}

void sendAll(RawSocket socket, const std::uint8_t *data, std::size_t size) {
    while (size != 0) {
#ifdef D6R_TRANSPORT_WINDOWS
        int count = ::send(socket, reinterpret_cast<const char *>(data), static_cast<int>(size), 0);
#else
        ssize_t count = ::send(socket, data, size, MSG_NOSIGNAL);
#endif
        CHECK(count > 0);
        data += count;
        size -= static_cast<std::size_t>(count);
    }
}

std::array<std::uint8_t, TransportEnvelopeBytes> envelope(std::uint32_t magic, std::uint16_t version,
                                                           std::uint16_t kind, std::uint32_t size) {
    std::array<std::uint8_t, TransportEnvelopeBytes> data{};
    data[0] = static_cast<std::uint8_t>(magic >> 24u); data[1] = static_cast<std::uint8_t>(magic >> 16u);
    data[2] = static_cast<std::uint8_t>(magic >> 8u); data[3] = static_cast<std::uint8_t>(magic);
    data[4] = static_cast<std::uint8_t>(version >> 8u); data[5] = static_cast<std::uint8_t>(version);
    data[6] = static_cast<std::uint8_t>(kind >> 8u); data[7] = static_cast<std::uint8_t>(kind);
    data[8] = static_cast<std::uint8_t>(size >> 24u); data[9] = static_cast<std::uint8_t>(size >> 16u);
    data[10] = static_cast<std::uint8_t>(size >> 8u); data[11] = static_cast<std::uint8_t>(size);
    return data;
}

void sendFrame(RawSocket socket, const std::vector<std::uint8_t> &payload, std::uint16_t kind = 0) {
    auto header = envelope(TransportFramingIdentifier, TransportFramingVersion, kind,
                           static_cast<std::uint32_t>(payload.size()));
    sendAll(socket, header.data(), header.size());
    if (!payload.empty()) sendAll(socket, payload.data(), payload.size());
}

std::shared_ptr<TcpConnection> awaitAccept(TcpListener &listener) {
    std::shared_ptr<TcpConnection> result;
    CHECK(waitUntil([&] { result = listener.acceptConnection(); return bool(result); }, 2s));
    return result;
}

void startListener(TcpListener &listener, std::uint16_t port, const std::string &host = "127.0.0.1") {
    CHECK(listener.start({host, port}));
    CHECK(listener.waitForReady(2s));
    CHECK(listener.state() == ListenerState::Ready);
}

void lifecycleAndFailures() {
    {
        TcpClient cancelled;
        const auto started = std::chrono::steady_clock::now();
        CHECK(cancelled.start({"localhost", unusedPort()}));
        cancelled.cancel(); cancelled.cancel();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(cancelled.state() == ClientState::Cancelled);
        std::this_thread::sleep_for(100ms);
        CHECK(cancelled.state() == ClientState::Cancelled);
        CHECK(!cancelled.waitForConnected(10ms));
    }
    {
        TcpListener cancelled;
        const auto started = std::chrono::steady_clock::now();
        CHECK(cancelled.start({"localhost", unusedPort()}));
        cancelled.cancel(); cancelled.cancel();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(cancelled.state() == ListenerState::Cancelled);
        std::this_thread::sleep_for(100ms);
        CHECK(cancelled.state() == ListenerState::Cancelled);
        CHECK(!cancelled.waitForReady(10ms));
        cancelled.shutdown(); cancelled.shutdown();
        CHECK(cancelled.state() == ListenerState::Cancelled);
    }
    TcpListener invalid;
    CHECK(invalid.start({"", 1}));
    CHECK(!invalid.waitForReady(2s));
    CHECK(invalid.state() == ListenerState::Failed);
    CHECK(invalid.failure() == TransportFailure::InvalidEndpoint);
    invalid.cancel(); invalid.cancel(); invalid.shutdown(); invalid.shutdown();
    CHECK(invalid.state() == ListenerState::Failed);
    CHECK(!invalid.start({"127.0.0.1", unusedPort()}));

    const auto occupiedPort = unusedPort();
    TcpListener first;
    startListener(first, occupiedPort);
    TcpListener collision;
    CHECK(collision.start({"127.0.0.1", occupiedPort}));
    CHECK(!collision.waitForReady(2s));
    CHECK(collision.failure() == TransportFailure::BindFailed);

    TcpClient invalidClient;
    CHECK(invalidClient.start({"", occupiedPort}));
    CHECK(!invalidClient.waitForConnected(2s));
    CHECK(invalidClient.failure() == TransportFailure::InvalidEndpoint);
    invalidClient.cancel(); invalidClient.cancel();
    CHECK(!invalidClient.start({"127.0.0.1", occupiedPort}));

    TcpClient unresolved;
    CHECK(unresolved.start({"invalid host name !", occupiedPort}));
    CHECK(!unresolved.waitForConnected(3s));
    CHECK(unresolved.failure() == TransportFailure::ResolveFailed);

    const auto refusedPort = unusedPort();
    TcpClient refused;
    CHECK(refused.start({"127.0.0.1", refusedPort}));
    CHECK(!refused.waitForConnected(3s));
    CHECK(refused.failure() == TransportFailure::ConnectionRefused);

    first.shutdown(); first.shutdown();
    CHECK(first.state() == ListenerState::Stopped);
    TcpClient afterShutdown;
    CHECK(afterShutdown.start({"127.0.0.1", occupiedPort}));
    CHECK(!afterShutdown.waitForConnected(3s));
    CHECK(afterShutdown.failure() == TransportFailure::ConnectionRefused);
}

void fifteenIsolatedConnections() {
    const auto port = unusedPort();
    TcpListener listener(15);
    startListener(listener, port, "localhost");
    std::vector<std::unique_ptr<TcpClient>> clients;
    std::vector<std::shared_ptr<TcpConnection>> servers;
    for (std::uint8_t index = 0; index < 15; ++index) {
        auto client = std::make_unique<TcpClient>();
        CHECK(client->start({index % 2 == 0 ? "localhost" : "127.0.0.1", port}));
        CHECK(client->waitForConnected(2s));
        servers.push_back(awaitAccept(listener));
        clients.push_back(std::move(client));
    }
    for (std::uint8_t index = 0; index < 15; ++index) {
        CHECK(clients[index]->connection()->send({index, 0, 255, static_cast<std::uint8_t>(14 - index)}) == SendResult::Accepted);
    }
    for (std::uint8_t index = 0; index < 15; ++index) {
        TransportFrame frame;
        CHECK(waitUntil([&] { return servers[index]->receive(frame); }, 2s));
        CHECK(frame.payload == (std::vector<std::uint8_t>{index, 0, 255, static_cast<std::uint8_t>(14 - index)}));
    }
    listener.shutdown();
}

void queueBoundaries() {
    const auto port = unusedPort();
    TcpListener listener(3);
    startListener(listener, port);
    RawSocketOwner framePeer(connectRaw(port));
    auto frameConnection = awaitAccept(listener);
    for (std::size_t index = 0; index <= MaxQueuedTransportFrames; ++index)
        sendFrame(framePeer.get(), {static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(index >> 8u)});
    std::this_thread::sleep_for(100ms);
    for (std::size_t index = 0; index <= MaxQueuedTransportFrames; ++index) {
        TransportFrame frame;
        CHECK(waitUntil([&] { return frameConnection->receive(frame); }, 2s));
        CHECK(frame.payload == (std::vector<std::uint8_t>{static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(index >> 8u)}));
    }

    RawSocketOwner bytePeer(connectRaw(port));
    auto byteConnection = awaitAccept(listener);
    std::vector<std::uint8_t> maximum(MaxPayloadBytes, 0xA5);
    for (int index = 0; index < 4; ++index) sendFrame(bytePeer.get(), maximum);
    sendFrame(bytePeer.get(), {0x5A});
    std::this_thread::sleep_for(100ms);
    for (int index = 0; index < 4; ++index) {
        TransportFrame frame;
        CHECK(waitUntil([&] { return byteConnection->receive(frame); }, 2s));
        CHECK(frame.payload == maximum);
    }
    TransportFrame finalFrame;
    CHECK(waitUntil([&] { return byteConnection->receive(finalFrame); }, 2s));
    CHECK(finalFrame.payload == std::vector<std::uint8_t>{0x5A});
    CHECK(byteConnection->send(std::vector<std::uint8_t>(MaxPayloadBytes + 1)) == SendResult::PayloadTooLarge);

    RawSocketOwner stalledReader(connectRaw(port));
    auto stalledWriter = awaitAccept(listener);
    bool backpressured = false;
    std::size_t accepted = 0;
    for (std::size_t index = 0; index < 64; ++index) {
        const auto result = stalledWriter->send(maximum);
        if (result == SendResult::Backpressure) { backpressured = true; break; }
        CHECK(result == SendResult::Accepted);
        ++accepted;
    }
    CHECK(backpressured);
    CHECK(accepted > 0);
    listener.shutdown();
}

void malformedPeersAreIsolated() {
    const auto port = unusedPort();
    TcpListener listener(15);
    startListener(listener, port);
    std::vector<std::unique_ptr<RawSocketOwner>> peers;
    std::vector<std::shared_ptr<TcpConnection>> offenders;
    auto add = [&](const std::vector<std::uint8_t> &bytes) {
        auto peer = std::make_unique<RawSocketOwner>(connectRaw(port));
        if (!bytes.empty()) sendAll(peer->get(), bytes.data(), bytes.size());
        peers.push_back(std::move(peer)); offenders.push_back(awaitAccept(listener));
    };
    for (auto header: {envelope(0, TransportFramingVersion, 0, 0),
                       envelope(TransportFramingIdentifier, TransportFramingVersion + 1, 0, 0),
                       envelope(TransportFramingIdentifier, TransportFramingVersion, 99, 0),
                       envelope(TransportFramingIdentifier, TransportFramingVersion, 0, 0xFFFFFFFFu)})
        add(std::vector<std::uint8_t>(header.begin(), header.end()));
    auto partialHeader = envelope(TransportFramingIdentifier, TransportFramingVersion, 0, 0);
    add(std::vector<std::uint8_t>(partialHeader.begin(), partialHeader.begin() + 5));
    auto partialPayload = envelope(TransportFramingIdentifier, TransportFramingVersion, 0, 10);
    std::vector<std::uint8_t> partial(partialPayload.begin(), partialPayload.end());
    partial.insert(partial.end(), {1, 2, 3}); add(partial);

    CHECK(waitUntil([&] {
        for (const auto &connection: offenders)
            if (connection->state() != ClientState::Failed && connection->state() != ClientState::TimedOut) return false;
        return true;
    }, 7s));
    for (std::size_t index = 0; index < offenders.size(); ++index)
        CHECK(offenders[index]->failure() == (index < 4 ? TransportFailure::ProtocolViolation : TransportFailure::InboundStalled));

    TcpClient healthy;
    CHECK(healthy.start({"127.0.0.1", port})); CHECK(healthy.waitForConnected(2s));
    auto healthyServer = awaitAccept(listener);
    CHECK(healthy.connection()->send({9, 8, 7}) == SendResult::Accepted);
    TransportFrame frame; CHECK(waitUntil([&] { return healthyServer->receive(frame); }, 2s));
    CHECK(frame.payload == (std::vector<std::uint8_t>{9, 8, 7}));

    RawSocketOwner closedPeer(connectRaw(port)); auto closedConnection = awaitAccept(listener); closedPeer.close();
    CHECK(waitUntil([&] { return closedConnection->state() == ClientState::Failed; }, 2s));
    CHECK(closedConnection->failure() == TransportFailure::PeerClosed);
    listener.shutdown();
}

void stallsAndLiveness() {
    const auto port = unusedPort();
    TcpListener listener(4); startListener(listener, port);
    RawSocketOwner stalledFrames(connectRaw(port)); auto stalledFrameConnection = awaitAccept(listener);
    for (std::size_t index = 0; index <= MaxQueuedTransportFrames; ++index) sendFrame(stalledFrames.get(), {});
    RawSocketOwner stalledBytes(connectRaw(port)); auto stalledByteConnection = awaitAccept(listener);
    std::vector<std::uint8_t> maximum(MaxPayloadBytes, 0x6B);
    for (int index = 0; index < 4; ++index) sendFrame(stalledBytes.get(), maximum);
    sendFrame(stalledBytes.get(), {1});
    TcpClient quietClient; CHECK(quietClient.start({"127.0.0.1", port})); CHECK(quietClient.waitForConnected(2s));
    auto quietServer = awaitAccept(listener);
    RawSocketOwner idlePeer(connectRaw(port)); auto idleConnection = awaitAccept(listener);
    CHECK(waitUntil([&] { return stalledFrameConnection->state() == ClientState::TimedOut
                                && stalledByteConnection->state() == ClientState::TimedOut; }, 7s));
    CHECK(stalledFrameConnection->failure() == TransportFailure::InboundStalled);
    CHECK(stalledByteConnection->failure() == TransportFailure::InboundStalled);
    CHECK(waitUntil([&] { return idleConnection->state() == ClientState::TimedOut; }, 27s));
    CHECK(idleConnection->failure() == TransportFailure::IdleTimedOut);
    CHECK(quietClient.state() == ClientState::Connected); CHECK(quietServer->state() == ClientState::Connected);
    TransportFrame hidden; CHECK(!quietClient.connection()->receive(hidden)); CHECK(!quietServer->receive(hidden));
    listener.shutdown();
}

void closeAndShutdownBounds() {
    const auto port = unusedPort(); TcpListener listener; startListener(listener, port);
    TcpClient client; CHECK(client.start({"localhost", port})); CHECK(client.waitForConnected(2s));
    auto server = awaitAccept(listener);
    CHECK(client.connection()->send({1}) == SendResult::Accepted); CHECK(client.connection()->send({2}) == SendResult::Accepted);
    client.connection()->requestClose(); client.connection()->requestClose();
    CHECK(client.connection()->send({3}) == SendResult::Closing);
    TransportFrame first, second;
    CHECK(waitUntil([&] { return server->receive(first); }, 2s)); CHECK(waitUntil([&] { return server->receive(second); }, 2s));
    CHECK(first.payload == std::vector<std::uint8_t>{1}); CHECK(second.payload == std::vector<std::uint8_t>{2});
    auto started = std::chrono::steady_clock::now(); client.close(); client.close();
    CHECK(std::chrono::steady_clock::now() - started < 2500ms);
    started = std::chrono::steady_clock::now(); listener.shutdown(); listener.shutdown();
    CHECK(std::chrono::steady_clock::now() - started < 3s); CHECK(listener.state() == ListenerState::Stopped);
}
}

int main() {
#ifdef D6R_TRANSPORT_WINDOWS
    WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 2;
#endif
    const std::vector<std::pair<const char *, void (*)()>> tests = {
        {"lifecycle and failures", lifecycleAndFailures}, {"15 isolated connections", fifteenIsolatedConnections},
        {"queue boundaries", queueBoundaries}, {"malformed isolation", malformedPeersAreIsolated},
        {"stalls and liveness", stallsAndLiveness}, {"close and shutdown bounds", closeAndShutdownBounds}};
    int failures = 0;
    for (const auto &test: tests) try { test.second(); std::cout << "[PASS] " << test.first << '\n'; }
        catch (const std::exception &error) { ++failures; std::cerr << "[FAIL] " << test.first << "\n  " << error.what() << '\n'; }
#ifdef D6R_TRANSPORT_WINDOWS
    WSACleanup();
#endif
    std::cout << "Executed " << tests.size() << " transport test(s), failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
