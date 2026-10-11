// Test-only maintained-PAKE application endpoint. Python still supplies the real
// verified outer TLS frontend. Both inner legs authenticate with SecureSession;
// no plaintext application bytes are injected below encryption or logged.
#include "source/network/SecureSession.h"
#include "source/network/SessionTransport.h"
#include "source/network/SessionLifecycle.h"
#include "source/network/PublicSession.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Duel6::Network;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
static std::atomic<bool> stopped{false};
static void stop(int) { stopped = true; }
struct Socket {
    int fd;
    ~Socket() { if (fd >= 0) ::close(fd); }
};
static void require(bool value) { if (!value) throw std::runtime_error("authenticated peer fixture failed"); }
static void marker(const std::filesystem::path &root, const char *name) { std::ofstream out(root / name); }
static bool exactProxy(int fd, unsigned char *bytes, std::size_t size, bool writing) {
    const auto deadline = Clock::now() + 2s;
    std::size_t offset = 0;
    while (offset < size && !stopped && Clock::now() < deadline) {
        auto count = writing ? ::send(fd, bytes + offset, size - offset, MSG_NOSIGNAL)
                             : ::recv(fd, bytes + offset, size - offset, 0);
        if (count > 0) offset += static_cast<std::size_t>(count);
        else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) return false;
        else std::this_thread::sleep_for(1ms);
    }
    return offset == size;
}
struct Frame {
    std::vector<std::uint8_t> bytes;
    std::size_t used = 0, total = TransportEnvelopeBytes;
    Frame() : bytes(TransportEnvelopeBytes) {}
    ~Frame() { Trust::secureEraseMemory(bytes.data(), bytes.size()); }
    // Returns complete application/control frame, retry, or terminal. All SDK
    // calls go through fatal-gated SecureSession. Partial bytes prove nothing.
    int read(SecureSession &session) {
        const auto count = session.receive(bytes.data() + used, total - used);
        if (count == -2) return 0;
        if (count <= 0) return -1;
        used += static_cast<std::size_t>(count);
        if (used == TransportEnvelopeBytes && total == TransportEnvelopeBytes) {
            require(bytes[0] == 'D' && bytes[1] == '6' && bytes[2] == 'R' && bytes[3] == 'T'
                && bytes[4] == 0 && bytes[5] == 1 && bytes[6] == 0 && bytes[7] <= 2);
            const std::uint32_t size = (std::uint32_t(bytes[8]) << 24) | (std::uint32_t(bytes[9]) << 16)
                | (std::uint32_t(bytes[10]) << 8) | bytes[11];
            require(size <= MaxPayloadBytes && (bytes[7] == 0 || size == 0));
            total += size; bytes.resize(total);
        }
        return used == total ? 1 : 0;
    }
    void reset() { Trust::secureEraseMemory(bytes.data(), bytes.size()); bytes.assign(TransportEnvelopeBytes, 0); used = 0; total = TransportEnvelopeBytes; }
};
static bool write(SecureSession &session, const std::vector<std::uint8_t> &bytes) {
    std::size_t offset = 0;
    const auto deadline = Clock::now() + 2s; // Stable retry arguments; no replenishment.
    while (offset < bytes.size() && !stopped && Clock::now() < deadline) {
        const auto count = session.send(bytes.data() + offset, bytes.size() - offset);
        if (count > 0) offset += static_cast<std::size_t>(count);
        else if (count != -2) return false;
        else std::this_thread::sleep_for(1ms);
    }
    return offset == bytes.size();
}
static std::vector<std::uint8_t> frame(const std::vector<std::uint8_t> &payload) {
    std::vector<std::uint8_t> bytes{'D','6','R','T',0,1,0,0,0,0,0,0};
    for (int i = 0; i < 4; ++i) bytes[8 + i] = static_cast<std::uint8_t>(payload.size() >> ((3-i)*8));
    bytes.insert(bytes.end(), payload.begin(), payload.end()); return bytes;
}
static bool summary(const Frame &value) {
    // Fixed replication v3 header and phase only. Never dump result/grant bytes.
    const auto &b = value.bytes;
    const auto p = TransportEnvelopeBytes;
    return b.size() >= p + 49 && b[p] == 'P' && b[p+1] == 'R' && b[p+2] == '6' && b[p+3] == 'D'
        && b[p+4] == 3 && b[p+5] == 0 && (b[p+6] == 1 || b[p+6] == 2) && b[p+7] == 0 && b[p+48] == 3;
}
static void bridge(int accepted, unsigned id, unsigned backendPort, const std::filesystem::path &root,
                   const std::string &scenario, const std::shared_ptr<const SessionPassword> &password) {
    Socket front{accepted}, back{::socket(AF_INET, SOCK_STREAM, 0)};
    const char *stage = "proxy";
    try {
        require(back.fd >= 0);
        const int noDelay = 1;
        require(setsockopt(front.fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay)) == 0);
        require(setsockopt(back.fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay)) == 0);
        require(fcntl(front.fd, F_SETFL, O_NONBLOCK) == 0);
        std::array<unsigned char, 28> proxy{};
        require(exactProxy(front.fd, proxy.data(), proxy.size(), false));
        stage = "frontend-auth";
        SecureSession client(front.fd, true, password);
        require(client.handshake(Clock::now() + 3s, [] { return stopped.load(); }));
        stage = "backend-auth";
        sockaddr_in endpoint{}; endpoint.sin_family = AF_INET;
        endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK); endpoint.sin_port = htons(backendPort);
        require(::connect(back.fd, reinterpret_cast<sockaddr *>(&endpoint), sizeof(endpoint)) == 0);
        require(fcntl(back.fd, F_SETFL, O_NONBLOCK) == 0);
        require(exactProxy(back.fd, proxy.data(), proxy.size(), true));
        SecureSession service(back.fd, false, password);
        require(service.handshake(Clock::now() + 3s, [] { return stopped.load(); }));
        Frame incoming, outgoing;
        bool attackReceived = false, raceCompleted = false;
        unsigned clientFrames = 0, serviceFrames = 0;
        std::vector<std::uint8_t> oppositeTraffic;
        while (!stopped) {
            stage = "application-relay";
            if (id == 2) {
                std::ifstream command(root / "action"); unsigned target = 0; std::string action; std::uint64_t session = 0;
                if (command >> target >> action >> session && target == id
                    && (action == "maintenance" || action == "controller" || action == "wrong")) {
                    std::filesystem::remove(root / "action");
                    require(write(client, frame(PublicSession::terminalNotice(session + (action == "wrong"), action != "controller"))));
                    marker(root, "acted");
                }
            }
            const int received = incoming.read(client);
            if (received < 0) return;
            if (received > 0) {
                { std::ofstream out(root / ("peer-client-frame-" + std::to_string(id)));
                  out << ++clientFrames << ':' << incoming.bytes.size() << ':' << unsigned(incoming.bytes[7]); }
                if (std::filesystem::exists(root / "attack.bin")) {
                    std::ifstream attack(root / "attack.bin", std::ios::binary);
                    std::vector<std::uint8_t> expected{std::istreambuf_iterator<char>(attack), {}};
                    attackReceived |= incoming.bytes.size() == TransportEnvelopeBytes + expected.size()
                        && std::equal(expected.begin(), expected.end(), incoming.bytes.begin() + TransportEnvelopeBytes);
                }
                // Schedule real client Ready traffic at the End boundary, not
                // before it. Only these non-secret participant-action frames
                // are retained; ordinary play and admission pass unchanged.
                bool hold = false;
                if (scenario == "flow-race" && id == 3 && incoming.bytes.size() == TransportEnvelopeBytes + 26) {
                    const auto action = Lifecycle::deserializeParticipantAction(
                        {incoming.bytes.begin() + TransportEnvelopeBytes, incoming.bytes.end()});
                    hold = action && (action->kind == Lifecycle::ParticipantActionKind::Ready
                                      || action->kind == Lifecycle::ParticipantActionKind::NotReady);
                }
                if (hold) oppositeTraffic = incoming.bytes;
                else require(write(service, incoming.bytes));
                incoming.reset();
            }
            const int sent = outgoing.read(service);
            if (sent < 0) { if (attackReceived) marker(root, "attack-rejected"); return; }
            if (sent > 0) {
                { std::ofstream out(root / ("peer-service-frame-" + std::to_string(id)));
                  out << ++serviceFrames << ':' << outgoing.bytes.size() << ':' << unsigned(outgoing.bytes[7]); }
                if (scenario == "summary-return-ordered" && summary(outgoing)) {
                    marker(root, id == 1 ? "summary-controller-held" : "summary-guest-seen");
                    const auto deadline = Clock::now() + 2s;
                    while (!stopped && Clock::now() < deadline && (id == 1
                            ? !std::filesystem::exists(root / "summary-release")
                            : !std::filesystem::exists(root / "summary-controller-held"))) std::this_thread::sleep_for(1ms);
                    require(!stopped && Clock::now() < deadline);
                }
                std::optional<Lifecycle::IntentionalHostEndNotice> end;
                if (outgoing.bytes.size() == TransportEnvelopeBytes + 16
                    && std::equal(outgoing.bytes.begin() + TransportEnvelopeBytes,
                                  outgoing.bytes.begin() + TransportEnvelopeBytes + 8, "D6LC\0\1\0\3"))
                    end = Lifecycle::deserializeIntentionalHostEnd({outgoing.bytes.begin() + TransportEnvelopeBytes, outgoing.bytes.end()});
                if (scenario == "flow-race" && id == 3 && end && !raceCompleted) {
                    // The service actually sent End. Preserve that authenticated
                    // staging while genuine opposite participant traffic attempts
                    // a write against a closed OS half, then stop the fatal SDK.
                    stage = "race-shutdown";
                    require(::shutdown(back.fd, SHUT_WR) == 0 || errno == ENOTCONN);
                    const auto deadline = Clock::now() + 2s;
                    stage = "race-opposite-read";
                    int traffic = oppositeTraffic.empty() ? 0 : 1;
                    while (!stopped && traffic == 0 && Clock::now() < deadline) {
                        traffic = incoming.read(client);
                        if (traffic == 0) std::this_thread::sleep_for(1ms);
                    }
                    stage = "race-opposite-write";
                    require(traffic == 1 && !write(service, oppositeTraffic.empty() ? incoming.bytes : oppositeTraffic)
                        && !service.usable());
                    marker(root, "race-write-failed"); raceCompleted = true;
                }
                stage = "authenticated-forward";
                require(write(client, outgoing.bytes));
                if (end) {
                    marker(root, ("end-forwarded-" + std::to_string(id)).c_str());
                    return; // Never query the failed backend context after the race.
                }
                outgoing.reset();
            }
            std::this_thread::sleep_for(1ms);
        }
    } catch (...) {
        if (!stopped) { std::ofstream out(root / ("peer-failed-" + std::to_string(id))); out << stage; }
    }
}
int main(int argc, char **argv) {
    try {
        require(argc == 5);
        const std::filesystem::path root(argv[3]);
        std::string secret; require(static_cast<bool>(std::getline(std::cin, secret)));
        auto password = std::make_shared<SessionPassword>(secret);
        Trust::secureEraseMemory(secret.data(), secret.size()); secret.clear();
        Socket listener{::socket(AF_INET, SOCK_STREAM, 0)}; require(listener.fd >= 0);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(std::stoi(argv[1]));
        require(::bind(listener.fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 && ::listen(listener.fd, 15) == 0);
        std::signal(SIGTERM, stop); std::signal(SIGINT, stop);
        marker(root, "peer-ready");
        std::vector<std::thread> workers; unsigned id = 0;
        while (!stopped) {
            pollfd readable{listener.fd, POLLIN, 0};
            if (::poll(&readable, 1, 100) > 0) {
                const int accepted = ::accept(listener.fd, nullptr, nullptr);
                if (accepted >= 0) workers.emplace_back(bridge, accepted, ++id, std::stoul(argv[2]), root, std::string(argv[4]), password);
            }
        }
        for (auto &worker: workers) worker.join();
        return 0;
    } catch (...) { std::cerr << "authenticated peer startup failed\n"; return 1; }
}
