#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <fstream>
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
#include <cerrno>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using RawSocket = int;
static constexpr RawSocket InvalidRawSocket = -1;
#endif

namespace {
using namespace Duel6::Network;
using namespace std::chrono_literals;

#ifdef D6R_TRANSPORT_WINDOWS
constexpr auto NativeOperationWait = 10s;
#else
constexpr auto NativeOperationWait = 2s;
#endif

class Failure : public std::runtime_error { public: using std::runtime_error::runtime_error; };
#define CHECK(value) do { if (!(value)) throw Failure(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #value); } while (false)

void closeRaw(RawSocket socket) {
#ifdef D6R_TRANSPORT_WINDOWS
    if (socket != InvalidRawSocket) closesocket(socket);
#else
    if (socket != InvalidRawSocket) ::close(socket);
#endif
}

void shutdownRaw(RawSocket socket) {
#ifdef D6R_TRANSPORT_WINDOWS
    if (socket != InvalidRawSocket) ::shutdown(socket, SD_BOTH);
#else
    if (socket != InvalidRawSocket) ::shutdown(socket, SHUT_RDWR);
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
    CHECK(waitUntil([&] { result = listener.acceptConnection(); return bool(result); }, NativeOperationWait));
    return result;
}

ResolvedIpv4Endpoint fakeLoopback(std::uint16_t port) {
    return {{127, 0, 0, 1}, port};
}

class FakeClock {
public:
    TransportTimePoint now() const { return TransportTimePoint{} + std::chrono::milliseconds(ticks.load()); }
    void advance(std::chrono::milliseconds amount) { ticks.fetch_add(amount.count()); }
    void set(TransportTimePoint value) {
        ticks.store(std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count());
    }
private:
    std::atomic<std::int64_t> ticks{0};
};

void deterministicResolverCancellation() {
    auto verifyClient = [] {
        std::atomic<int> activeResolvers{0};
        std::atomic<int> connectorCalls{0};
        SessionTransportDependencies dependencies;
        dependencies.resolve = [&](const std::string &, std::uint16_t, TransportTimePoint,
                                   const std::function<bool()> &cancelled) {
            activeResolvers.fetch_add(1);
            while (!cancelled()) std::this_thread::yield();
            activeResolvers.fetch_sub(1);
            return ResolveOutcome{ResolveStatus::Cancelled, {}};
        };
        dependencies.connect = [&](const auto &, auto, const auto &) {
            connectorCalls.fetch_add(1);
            return ConnectOutcome{};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"deterministically-stalled.test", 12345}));
        CHECK(waitUntil([&] { return activeResolvers.load() == 1; }, 1s));
        const auto started = std::chrono::steady_clock::now();
        client.cancel(); client.cancel(); client.cancel();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(activeResolvers.load() == 0);
        CHECK(connectorCalls.load() == 0);
        CHECK(client.state() == ClientState::Cancelled);
        std::this_thread::sleep_for(50ms);
        CHECK(client.state() == ClientState::Cancelled);
        CHECK(!client.connection());
    };
    auto verifyListener = [] {
        std::atomic<int> activeResolvers{0};
        SessionTransportDependencies dependencies;
        dependencies.resolve = [&](const std::string &, std::uint16_t, TransportTimePoint,
                                   const std::function<bool()> &cancelled) {
            activeResolvers.fetch_add(1);
            while (!cancelled()) std::this_thread::yield();
            activeResolvers.fetch_sub(1);
            return ResolveOutcome{ResolveStatus::Cancelled, {}};
        };
        TcpListener listener(1, dependencies);
        CHECK(listener.start({"deterministically-stalled.test", 12345}));
        CHECK(waitUntil([&] { return activeResolvers.load() == 1; }, 1s));
        const auto started = std::chrono::steady_clock::now();
        listener.cancel(); listener.cancel(); listener.shutdown(); listener.shutdown();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(activeResolvers.load() == 0);
        CHECK(listener.state() == ListenerState::Cancelled);
        std::this_thread::sleep_for(50ms);
        CHECK(listener.state() == ListenerState::Cancelled);
        CHECK(!listener.acceptConnection());
    };
    verifyClient();
    verifyListener();
}

void sharedDeadlineAndClassifications() {
    auto classifyConnect = [](ConnectStatus status, ClientState expectedState, TransportFailure expectedFailure) {
        FakeClock clock;
        TransportTimePoint resolverDeadline{};
        TransportTimePoint connectorDeadline{};
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t port, TransportTimePoint deadline,
                                   const std::function<bool()> &) {
            resolverDeadline = deadline;
            clock.advance(9s);
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        dependencies.connect = [&](const auto &, TransportTimePoint deadline, const auto &) {
            connectorDeadline = deadline;
            CHECK(deadline == resolverDeadline);
            CHECK(deadline - clock.now() == 1s);
            return ConnectOutcome{status, -1};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"seam.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == expectedState);
        CHECK(client.failure() == expectedFailure);
        CHECK(connectorDeadline == resolverDeadline);
        client.cancel(); client.cancel();
        CHECK(client.state() == expectedState);
    };
    classifyConnect(ConnectStatus::ConnectionRefused, ClientState::Failed, TransportFailure::ConnectionRefused);
    classifyConnect(ConnectStatus::Unreachable, ClientState::Failed, TransportFailure::Unreachable);
    classifyConnect(ConnectStatus::Failed, ClientState::Failed, TransportFailure::SystemError);
    classifyConnect(ConnectStatus::TimedOut, ClientState::TimedOut, TransportFailure::None);

    {
        FakeClock clock;
        std::atomic<int> connectorCalls{0};
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t port, TransportTimePoint,
                                   const std::function<bool()> &) {
            clock.advance(10s);
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        dependencies.connect = [&](const auto &, auto, const auto &) {
            connectorCalls.fetch_add(1);
            return ConnectOutcome{ConnectStatus::ConnectionRefused, -1};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"deadline.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == ClientState::TimedOut);
        CHECK(client.failure() == TransportFailure::None);
        CHECK(connectorCalls.load() == 0);
    }
    {
        FakeClock clock;
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t port, TransportTimePoint,
                                   const std::function<bool()> &) {
            clock.advance(9999ms);
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        dependencies.connect = [&](const auto &, TransportTimePoint deadline, const auto &) {
            CHECK(deadline - clock.now() == 1ms);
            clock.advance(1ms);
            return ConnectOutcome{ConnectStatus::ConnectionRefused, -1};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"deadline-precedence.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == ClientState::TimedOut);
        CHECK(client.failure() == TransportFailure::None);
    }
    {
        SessionTransportDependencies dependencies;
        dependencies.resolve = [](const std::string &, std::uint16_t, TransportTimePoint,
                                  const std::function<bool()> &) {
            return ResolveOutcome{ResolveStatus::Failed, {}};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"resolve-failure.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == ClientState::Failed);
        CHECK(client.failure() == TransportFailure::ResolveFailed);
    }
    {
        FakeClock clock;
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t port, TransportTimePoint,
                                   const std::function<bool()> &) {
            clock.advance(10s);
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        TcpListener listener(1, dependencies);
        CHECK(listener.start({"listener-deadline.test", 12345}));
        CHECK(!listener.waitForReady(1s));
        CHECK(listener.state() == ListenerState::TimedOut);
        CHECK(listener.failure() == TransportFailure::None);
        listener.shutdown(); listener.shutdown();
        CHECK(listener.state() == ListenerState::TimedOut);
    }
}

void cancellationDeadlineRacesAreTerminalAndJoined() {
    {
        FakeClock clock;
        std::atomic<int> activeResolvers{0};
        std::atomic<bool> entered{false};
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t, TransportTimePoint deadline,
                                   const std::function<bool()> &cancelled) {
            activeResolvers.fetch_add(1);
            entered.store(true);
            while (!cancelled()) std::this_thread::yield();
            clock.set(deadline);
            activeResolvers.fetch_sub(1);
            return ResolveOutcome{ResolveStatus::TimedOut, {}};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"race.test", 12345}));
        CHECK(waitUntil([&] { return entered.load(); }, 1s));
        client.cancel(); client.cancel();
        CHECK(activeResolvers.load() == 0);
        CHECK(client.state() == ClientState::Cancelled);
        std::this_thread::sleep_for(50ms);
        CHECK(client.state() == ClientState::Cancelled);
    }
    {
        FakeClock clock;
        std::atomic<int> activeConnectors{0};
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [](const std::string &, std::uint16_t port, TransportTimePoint,
                                  const std::function<bool()> &) {
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        dependencies.connect = [&](const auto &, TransportTimePoint deadline, const std::function<bool()> &cancelled) {
            activeConnectors.fetch_add(1);
            while (!cancelled()) std::this_thread::yield();
            clock.set(deadline);
            activeConnectors.fetch_sub(1);
            return ConnectOutcome{ConnectStatus::TimedOut, -1};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"connector-race.test", 12345}));
        CHECK(waitUntil([&] { return activeConnectors.load() == 1; }, 1s));
        const auto started = std::chrono::steady_clock::now();
        client.cancel(); client.cancel(); client.cancel();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(activeConnectors.load() == 0);
        CHECK(client.state() == ClientState::Cancelled);
        CHECK(!client.connection());
    }
    {
        FakeClock clock;
        SessionTransportDependencies dependencies;
        dependencies.now = [&] { return clock.now(); };
        dependencies.resolve = [&](const std::string &, std::uint16_t, TransportTimePoint deadline,
                                   const std::function<bool()> &) {
            clock.set(deadline);
            return ResolveOutcome{ResolveStatus::TimedOut, {}};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"deadline-wins.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == ClientState::TimedOut);
        client.cancel(); client.cancel();
        CHECK(client.state() == ClientState::TimedOut);
    }
}

void startListener(TcpListener &listener, std::uint16_t port, const std::string &host = "127.0.0.1") {
    CHECK(listener.start({host, port}));
    CHECK(listener.waitForReady(NativeOperationWait));
    CHECK(listener.state() == ListenerState::Ready);
}

void mandatorySocketConfigurationFailure() {
    for (int iteration = 0; iteration < 100; ++iteration) {
        SessionTransportDependencies dependencies;
        dependencies.resolve = [](const std::string &, std::uint16_t port, TransportTimePoint,
                                  const std::function<bool()> &) {
            return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
        };
        dependencies.connect = [](const auto &, TransportTimePoint, const auto &) {
            RawSocket invalid = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            CHECK(invalid != InvalidRawSocket);
            closeRaw(invalid);
            return ConnectOutcome{ConnectStatus::Connected, static_cast<std::intptr_t>(invalid)};
        };
        TcpClient client(dependencies);
        CHECK(client.start({"configuration-failure.test", 12345}));
        CHECK(!client.waitForConnected(1s));
        CHECK(client.state() == ClientState::Failed);
        CHECK(client.failure() == TransportFailure::SystemError);
        CHECK(!client.connection());
        client.cancel(); client.cancel();
        CHECK(client.state() == ClientState::Failed);
    }
}

#ifndef D6R_TRANSPORT_WINDOWS
std::vector<pid_t> resolverPids(const std::string &path) {
    std::ifstream input(path);
    std::vector<pid_t> result;
    pid_t value = 0;
    while (input >> value) result.push_back(value);
    return result;
}

void realResolverHelperCleanupAndDescriptorEof() {
    const char *pidPathValue = std::getenv("D6R_FAKE_RESOLVER_PID_FILE");
    CHECK(pidPathValue != nullptr);
    const std::string pidPath(pidPathValue);

    SessionTransportDependencies listenerDependencies;
    listenerDependencies.resolve = [](const std::string &, std::uint16_t port, TransportTimePoint,
                                      const std::function<bool()> &) {
        return ResolveOutcome{ResolveStatus::Resolved, {fakeLoopback(port)}};
    };
    const auto port = unusedPort();
    TcpListener listener(1, listenerDependencies);
    startListener(listener, port);
    RawSocketOwner peer(connectRaw(port));
    auto connection = awaitAccept(listener);

    std::size_t expectedPidCount = 0;
    auto cancelRealHelper = [&] {
        TcpClient client;
        CHECK(client.start({"resolver-will-stall.test", port}));
        ++expectedPidCount;
        CHECK(waitUntil([&] { return resolverPids(pidPath).size() >= expectedPidCount; }, 1s));
        const pid_t process = resolverPids(pidPath).at(expectedPidCount - 1);
        const auto started = std::chrono::steady_clock::now();
        client.cancel(); client.cancel();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
        CHECK(client.state() == ClientState::Cancelled);
        CHECK(waitUntil([&] { return kill(process, 0) < 0 && errno == ESRCH; }, 1s));
    };

    cancelRealHelper();
    connection->close();
    timeval timeout{1, 0};
    CHECK(setsockopt(peer.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    std::uint8_t byte = 0;
    CHECK(recv(peer.get(), &byte, 1, 0) == 0);
    for (int iteration = 0; iteration < 24; ++iteration) cancelRealHelper();

    std::vector<std::unique_ptr<TcpClient>> cappedClients;
    for (int iteration = 0; iteration < 33; ++iteration) {
        auto client = std::make_unique<TcpClient>();
        CHECK(client->start({"resolver-cap-stall.test", port}));
        cappedClients.push_back(std::move(client));
    }
    CHECK(waitUntil([&] { return resolverPids(pidPath).size() >= expectedPidCount + 32; }, 3s));
    CHECK(waitUntil([&] {
        int failed = 0;
        for (const auto &client: cappedClients)
            if (client->state() == ClientState::Failed
                && client->failure() == TransportFailure::ResolveFailed) ++failed;
        return failed == 1;
    }, 2s));
    const auto cappedPids = resolverPids(pidPath);
    const auto cleanupStarted = std::chrono::steady_clock::now();
    for (auto &client: cappedClients) { client->cancel(); client->cancel(); }
    CHECK(std::chrono::steady_clock::now() - cleanupStarted < 4s);
    for (std::size_t index = expectedPidCount; index < expectedPidCount + 32; ++index) {
        const pid_t process = cappedPids.at(index);
        CHECK(waitUntil([&] { return kill(process, 0) < 0 && errno == ESRCH; }, 2s));
    }
    expectedPidCount += 32;
    cancelRealHelper();
    listener.shutdown(); listener.shutdown();
}

void malformedRealResolverResponseFails() {
    TcpClient client;
    CHECK(client.start({"malformed-helper-response.test", 12345}));
    CHECK(!client.waitForConnected(2s));
    CHECK(client.state() == ClientState::Failed);
    CHECK(client.failure() == TransportFailure::ResolveFailed);
}
#endif

void agedConnectionGetsFreshInboundQueueWindow() {
    const auto port = unusedPort();
    TcpListener listener(1);
    startListener(listener, port);
    RawSocketOwner peer(connectRaw(port));
    auto connection = awaitAccept(listener);
    std::this_thread::sleep_for(5500ms);
    for (std::size_t index = 0; index <= MaxQueuedTransportFrames; ++index) sendFrame(peer.get(), {});
    std::this_thread::sleep_for(4200ms);
    CHECK(connection->state() == ClientState::Connected);
    CHECK(connection->failure() == TransportFailure::None);
    CHECK(waitUntil([&] { return connection->state() == ClientState::TimedOut; }, 2s));
    CHECK(connection->failure() == TransportFailure::InboundStalled);
    listener.shutdown();
}

void continuousOneWayOutputKeepsReceiveQuietPeerAlive() {
    const auto port = unusedPort();
    TcpListener listener(1);
    startListener(listener, port);
    RawSocketOwner peer(connectRaw(port));
    auto server = awaitAccept(listener);
    int receiveBuffer = 32 * 1024;
    CHECK(setsockopt(peer.get(), SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char *>(&receiveBuffer), sizeof(receiveBuffer)) == 0);
    auto applicationPayload = [](std::uint64_t sequence) {
        std::vector<std::uint8_t> payload(sequence == 0 ? MaxPayloadBytes : 1024, 0xA6);
        for (std::size_t index = 0; index < 8; ++index)
            payload[index] = static_cast<std::uint8_t>(sequence >> ((7u - index) * 8u));
        return payload;
    };
    std::size_t accepted = 0;
    std::uint64_t nextSequence = 0;
    bool backpressured = false;
    for (std::size_t attempt = 0; attempt < 1024; ++attempt) {
        SendResult result = server->send(applicationPayload(nextSequence));
        if (result == SendResult::Backpressure) { backpressured = true; break; }
        CHECK(result == SendResult::Accepted);
        ++accepted;
        ++nextSequence;
    }
    CHECK(backpressured);
    CHECK(accepted >= MaxQueuedTransportFrames - 8);

    std::atomic<bool> readerFailed{false};
    std::atomic<std::size_t> applicationFrames{0};
    std::atomic<std::size_t> pingFrames{0};
    std::atomic<std::size_t> pongFrames{0};
    std::atomic<std::size_t> maximumBytesReceived{0};
    std::atomic<std::int64_t> maximumReadGapMilliseconds{0};
    std::atomic<bool> controlPrecededQueuedApplication{false};
    std::atomic<bool> slowReader{true};
    std::thread reader([&] {
        try {
        auto receiveExact = [&](std::uint8_t *target, std::size_t size) {
            while (size > 0) {
#ifdef D6R_TRANSPORT_WINDOWS
                int count = recv(peer.get(), reinterpret_cast<char *>(target), static_cast<int>(size), 0);
#else
                ssize_t count = recv(peer.get(), target, size, 0);
#endif
                if (count <= 0) return false;
                target += count;
                size -= static_cast<std::size_t>(count);
            }
            return true;
        };
        while (true) {
            std::array<std::uint8_t, TransportEnvelopeBytes> header{};
            if (!receiveExact(header.data(), header.size())) break;
            const std::uint32_t magic = (std::uint32_t(header[0]) << 24u) | (std::uint32_t(header[1]) << 16u)
                                        | (std::uint32_t(header[2]) << 8u) | header[3];
            const std::uint16_t kind = std::uint16_t((header[6] << 8u) | header[7]);
            const std::uint32_t size = (std::uint32_t(header[8]) << 24u) | (std::uint32_t(header[9]) << 16u)
                                       | (std::uint32_t(header[10]) << 8u) | header[11];
            if (magic != TransportFramingIdentifier || size > MaxPayloadBytes) { readerFailed.store(true); break; }
            std::vector<std::uint8_t> payload(size);
            if (size == MaxPayloadBytes && slowReader.load()) {
                auto previousRead = std::chrono::steady_clock::now();
                std::size_t offset = 0;
                while (offset < payload.size()) {
                    const std::size_t chunk = std::min<std::size_t>(2 * 1024, payload.size() - offset);
                    if (!receiveExact(payload.data() + offset, chunk)) { readerFailed.store(true); break; }
                    const auto readCompleted = std::chrono::steady_clock::now();
                    const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(readCompleted - previousRead).count();
                    auto observedGap = maximumReadGapMilliseconds.load();
                    while (gap > observedGap && !maximumReadGapMilliseconds.compare_exchange_weak(observedGap, gap)) {}
                    previousRead = readCompleted;
                    offset += chunk;
                    maximumBytesReceived.store(offset);
                    std::this_thread::sleep_for(70ms);
                }
                if (readerFailed.load()) break;
            } else if (size > 0 && !receiveExact(payload.data(), payload.size())) {
                readerFailed.store(true);
                break;
            }
            if (kind == 0) {
                std::uint64_t sequence = 0;
                for (std::size_t index = 0; index < 8; ++index)
                    sequence = (sequence << 8u) | payload[index];
                const std::size_t expectedSize = sequence == 0 ? MaxPayloadBytes : 1024;
                if (payload.size() != expectedSize || sequence != applicationFrames.load()
                    || !std::all_of(payload.begin() + 8, payload.end(), [](std::uint8_t value) {
                        return value == 0xA6;
                    })) { readerFailed.store(true); break; }
                applicationFrames.fetch_add(1);
                if (slowReader.load()) std::this_thread::sleep_for(100ms);
            } else if (kind == 1) {
                pingFrames.fetch_add(1);
                sendFrame(peer.get(), {}, 2);
            } else if (kind == 2) {
                pongFrames.fetch_add(1);
                if (applicationFrames.load() == 1) controlPrecededQueuedApplication.store(true);
            } else {
                readerFailed.store(true);
                break;
            }
        }
        } catch (...) {
            readerFailed.store(true);
        }
    });

    const auto deadline = std::chrono::steady_clock::now() + 34s;
    std::size_t sustainedAccepted = 0;
    std::size_t sustainedBackpressure = 0;
    std::size_t progressAtTenSeconds = 0;
    std::size_t progressAtTwentySeconds = 0;
    std::size_t progressAtThirtySeconds = 0;
    const auto started = std::chrono::steady_clock::now();
    bool unexpectedSendResult = false;
    SendResult unexpectedResult = SendResult::Accepted;
    bool peerPingSent = false;
    ClientState stateAfterThirtySeconds = ClientState::NotStarted;
    while (std::chrono::steady_clock::now() < deadline) {
        SendResult result = server->send(applicationPayload(nextSequence));
        if (result == SendResult::Accepted) { ++sustainedAccepted; ++nextSequence; }
        else if (result == SendResult::Backpressure) ++sustainedBackpressure;
        else { unexpectedSendResult = true; unexpectedResult = result; break; }
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (elapsed >= 10s && progressAtTenSeconds == 0) progressAtTenSeconds = maximumBytesReceived.load();
        if (elapsed >= 15s && !peerPingSent) { sendFrame(peer.get(), {}, 1); peerPingSent = true; }
        if (elapsed >= 20s && progressAtTwentySeconds == 0) progressAtTwentySeconds = maximumBytesReceived.load();
        if (elapsed >= 30s && progressAtThirtySeconds == 0) {
            progressAtThirtySeconds = maximumBytesReceived.load();
            stateAfterThirtySeconds = server->state();
        }
        std::this_thread::sleep_for(10ms);
    }
    const ClientState finalState = server->state();
    const TransportFailure finalFailure = server->failure();
    TransportFrame hidden;
    const bool controlHidden = !server->receive(hidden);
    int drainReceiveBuffer = 1024 * 1024;
    const bool drainBufferExpanded = setsockopt(peer.get(), SOL_SOCKET, SO_RCVBUF,
                                                reinterpret_cast<const char *>(&drainReceiveBuffer),
                                                sizeof(drainReceiveBuffer)) == 0;
    slowReader.store(false);
    const bool allAcceptedDelivered = waitUntil([&] {
        return readerFailed.load() || applicationFrames.load() == nextSequence;
    }, 45s) && !readerFailed.load() && applicationFrames.load() == nextSequence;
    shutdownRaw(peer.get());
    server->close();
    reader.join();
    listener.shutdown();
    if (unexpectedSendResult) {
        throw Failure("saturated liveness send became terminal: send="
                      + std::to_string(static_cast<int>(unexpectedResult))
                      + ", state=" + std::to_string(static_cast<int>(finalState))
                      + ", failure=" + std::to_string(static_cast<int>(finalFailure))
                      + ", pings=" + std::to_string(pingFrames.load())
                      + ", applicationFrames=" + std::to_string(applicationFrames.load())
                      + ", maximumBytesReceived=" + std::to_string(maximumBytesReceived.load())
                      + ", maximumReadGapMs=" + std::to_string(maximumReadGapMilliseconds.load()));
    }
    CHECK(sustainedAccepted > 0);
    CHECK(sustainedBackpressure > 0);
    CHECK(finalState == ClientState::Connected);
    CHECK(controlHidden);
    CHECK(drainBufferExpanded);
    CHECK(peerPingSent);
    CHECK(pongFrames.load() >= 1);
    CHECK(controlPrecededQueuedApplication.load());
    CHECK(progressAtTenSeconds > 0);
    CHECK(progressAtTwentySeconds > progressAtTenSeconds);
    CHECK(progressAtThirtySeconds > progressAtTwentySeconds);
    CHECK(progressAtThirtySeconds < MaxPayloadBytes);
    CHECK(maximumReadGapMilliseconds.load() < 4000);
    CHECK(stateAfterThirtySeconds == ClientState::Connected);
    if (!allAcceptedDelivered) {
        throw Failure("accepted application frames did not drain: accepted=" + std::to_string(nextSequence)
                      + ", delivered=" + std::to_string(applicationFrames.load()));
    }
    CHECK(!readerFailed.load());
}

void concurrentTerminalPublicationIsCoherent() {
    const auto port = unusedPort();
    TcpListener listener(15);
    startListener(listener, port);
    for (int iteration = 0; iteration < 100; ++iteration) {
        RawSocketOwner peer(connectRaw(port));
        auto connection = awaitAccept(listener);
        std::atomic<bool> incoherent{false};
        std::thread observer([&] {
            while (connection->state() == ClientState::Connected) std::this_thread::yield();
            const ClientState observed = connection->state();
            if ((observed == ClientState::Failed || observed == ClientState::TimedOut)
                && connection->failure() == TransportFailure::None) incoherent.store(true);
        });
        auto malformed = envelope(0, TransportFramingVersion, 0, 0);
        sendAll(peer.get(), malformed.data(), malformed.size());
        CHECK(waitUntil([&] { return connection->state() == ClientState::Failed; }, 1s));
        observer.join();
        CHECK(!incoherent.load());
        CHECK(connection->failure() == TransportFailure::ProtocolViolation);
    }
    listener.shutdown();
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
    CHECK(!collision.waitForReady(NativeOperationWait));
    CHECK(collision.failure() == TransportFailure::BindFailed);

    TcpClient invalidClient;
    CHECK(invalidClient.start({"", occupiedPort}));
    CHECK(!invalidClient.waitForConnected(NativeOperationWait));
    CHECK(invalidClient.failure() == TransportFailure::InvalidEndpoint);
    invalidClient.cancel(); invalidClient.cancel();
    CHECK(!invalidClient.start({"127.0.0.1", occupiedPort}));

    TcpClient unresolved;
    CHECK(unresolved.start({"invalid host name !", occupiedPort}));
    CHECK(!unresolved.waitForConnected(NativeOperationWait));
    CHECK(unresolved.failure() == TransportFailure::ResolveFailed);

    const auto refusedPort = unusedPort();
    TcpClient refused;
    CHECK(refused.start({"127.0.0.1", refusedPort}));
    CHECK(!refused.waitForConnected(NativeOperationWait));
    CHECK(refused.failure() == TransportFailure::ConnectionRefused);

    first.shutdown(); first.shutdown();
    CHECK(first.state() == ListenerState::Stopped);
    TcpClient afterShutdown;
    CHECK(afterShutdown.start({"127.0.0.1", occupiedPort}));
    CHECK(!afterShutdown.waitForConnected(NativeOperationWait));
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
        CHECK(client->waitForConnected(NativeOperationWait));
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
    CHECK(waitUntil([&] { return stalledWriter->state() == ClientState::TimedOut; }, 7s));
    CHECK(stalledWriter->failure() == TransportFailure::OutboundStalled);
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
    CHECK(healthy.start({"127.0.0.1", port})); CHECK(healthy.waitForConnected(NativeOperationWait));
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
    TcpClient quietClient; CHECK(quietClient.start({"127.0.0.1", port})); CHECK(quietClient.waitForConnected(NativeOperationWait));
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
    TcpClient client; CHECK(client.start({"localhost", port})); CHECK(client.waitForConnected(NativeOperationWait));
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
#ifndef D6R_TRANSPORT_WINDOWS
    const char *fakeResolverRun = std::getenv("D6R_RUN_FAKE_RESOLVER_TEST");
    const char *malformedResolverRun = std::getenv("D6R_RUN_MALFORMED_RESOLVER_TEST");
    if (malformedResolverRun != nullptr && std::string(malformedResolverRun) == "1") {
        try {
            malformedRealResolverResponseFails();
            std::cout << "[PASS] malformed real resolver response rejected\n";
            return 0;
        } catch (const std::exception &error) {
            std::cerr << "[FAIL] malformed real resolver response rejected\n  " << error.what() << '\n';
            return 1;
        }
    }
    if (fakeResolverRun != nullptr && std::string(fakeResolverRun) == "1") {
        try {
            realResolverHelperCleanupAndDescriptorEof();
            std::cout << "[PASS] real resolver helper cleanup and descriptor EOF\n";
            return 0;
        } catch (const std::exception &error) {
            std::cerr << "[FAIL] real resolver helper cleanup and descriptor EOF\n  " << error.what() << '\n';
            return 1;
        }
    }
#endif
    const std::vector<std::pair<const char *, void (*)()>> tests = {
        {"mandatory socket configuration failure", mandatorySocketConfigurationFailure},
        {"aged queue receives fresh progress window", agedConnectionGetsFreshInboundQueueWindow},
        {"continuous one-way output liveness", continuousOneWayOutputKeepsReceiveQuietPeerAlive},
        {"concurrent terminal publication", concurrentTerminalPublicationIsCoherent},
        {"deterministic resolver cancellation", deterministicResolverCancellation},
        {"shared deadline and classifications", sharedDeadlineAndClassifications},
        {"cancellation deadline races", cancellationDeadlineRacesAreTerminalAndJoined},
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
