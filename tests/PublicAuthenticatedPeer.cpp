// Test-only maintained-PAKE application endpoint. Both encrypted legs use SecureSession;
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
#include <deque>

using namespace Duel6::Network;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;
static std::atomic<bool> stopped{false};
static void stop(int) { stopped = true; }
struct Socket {
    int fd;
    ~Socket() { if (fd >= 0) ::close(fd); }
};
struct PeerReceipt {
    const std::filesystem::path &root;
    unsigned alias;
    const char *const &stage;
    Clock::time_point began = Clock::now(), ioBegan = began;
    const char *operation = "none";
    std::ptrdiff_t mapped = -99;
    std::size_t requested = 0, progress = 0;
    unsigned clientFrames = 0, serviceFrames = 0, emitted = 0;
    bool budgetExpired = false, attackSeen = false;
    void emit(const char *event, unsigned line) noexcept {
        // Fixed aliases/stages and numeric state only. No bytes, paths, session
        // identities or credential data; at most three receipts per connection.
        if (alias > 8 || emitted >= 3) return;
        ++emitted;
        try {
            std::ofstream out(root / ("peer-receipt-" + std::to_string(alias)), std::ios::app);
            out << "peer-receipt version=1 event=" << event << " line=" << line
                << " stage=" << stage << " operation=" << operation << " mapped=" << mapped
                << " requested=" << requested << " progress=" << progress
                << " elapsed-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - began).count()
                << " io-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - ioBegan).count()
                << " budget-expired=" << budgetExpired << " stopped=" << stopped.load()
                << " client-frames=" << clientFrames << " service-frames=" << serviceFrames
                << " attack-seen=" << attackSeen << '\n';
        } catch (...) {} // Logging failure must never replace the original failure.
    }
};
static thread_local PeerReceipt *activeReceipt = nullptr;
static void require(bool value, unsigned line) {
    if (!value) {
        if (activeReceipt) activeReceipt->emit("require-failed", line);
        throw std::runtime_error("authenticated peer fixture failed");
    }
}
#define require(value) require((value), __LINE__)
static void marker(const std::filesystem::path &root, const char *name) { std::ofstream out(root / name); }
struct Frame {
    std::vector<std::uint8_t> bytes;
    std::size_t used = 0, total = TransportEnvelopeBytes;
    Frame() : bytes(TransportEnvelopeBytes) {}
    ~Frame() { Trust::secureEraseMemory(bytes.data(), bytes.size()); }
    // Returns complete application/control frame, retry, or terminal. All SDK
    // calls go through fatal-gated SecureSession. Partial bytes prove nothing.
    int read(SecureSession &session, const char *operation = "read") {
        if (activeReceipt) {
            activeReceipt->operation = operation; activeReceipt->ioBegan = Clock::now();
            activeReceipt->requested = total - used; activeReceipt->progress = used;
            activeReceipt->budgetExpired = false;
        }
        const auto count = session.receive(bytes.data() + used, total - used);
        if (activeReceipt) activeReceipt->mapped = count;
        if (count == -2) return 0;
        if (count <= 0) {
            if (activeReceipt) activeReceipt->emit(count == 0 ? "read-eof" : "read-failed", __LINE__);
            return -1;
        }
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
static bool write(SecureSession &session, const std::vector<std::uint8_t> &bytes,
                  const char *operation = "write") {
    std::size_t offset = 0;
    const auto deadline = Clock::now() + 2s; // Stable retry arguments; no replenishment.
    if (activeReceipt) {
        activeReceipt->operation = operation; activeReceipt->ioBegan = Clock::now();
        activeReceipt->requested = bytes.size(); activeReceipt->progress = 0;
        activeReceipt->mapped = -99; activeReceipt->budgetExpired = false;
    }
    while (offset < bytes.size() && !stopped && Clock::now() < deadline) {
        const auto count = session.send(bytes.data() + offset, bytes.size() - offset);
        if (activeReceipt) activeReceipt->mapped = count;
        if (count > 0) offset += static_cast<std::size_t>(count);
        else if (count != -2) {
            if (activeReceipt) activeReceipt->emit("write-failed", __LINE__);
            return false;
        }
        else std::this_thread::sleep_for(1ms);
        if (activeReceipt) activeReceipt->progress = offset;
    }
    if (offset != bytes.size() && activeReceipt) {
        activeReceipt->budgetExpired = Clock::now() >= deadline;
        activeReceipt->emit("write-incomplete", __LINE__);
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
    PeerReceipt receipt{root, id, stage};
    struct ReceiptScope {
        PeerReceipt *previous;
        explicit ReceiptScope(PeerReceipt &current) : previous(activeReceipt) { activeReceipt = &current; }
        ~ReceiptScope() { activeReceipt = previous; }
    } receiptScope(receipt);
    try {
        require(back.fd >= 0);
        const int noDelay = 1;
        require(setsockopt(front.fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay)) == 0);
        require(setsockopt(back.fd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay)) == 0);
        require(fcntl(front.fd, F_SETFL, O_NONBLOCK) == 0);
        stage = "frontend-auth";
        SecureSession client(front.fd, true, password);
        require(client.handshake(Clock::now() + 3s, [] { return stopped.load(); }));
        stage = "backend-auth";
        sockaddr_in endpoint{}; endpoint.sin_family = AF_INET;
        endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK); endpoint.sin_port = htons(backendPort);
        sockaddr_in source{}; source.sin_family = AF_INET;
        source.sin_addr.s_addr = htonl(0x7f0000b0u + id);
        require(::bind(back.fd, reinterpret_cast<sockaddr *>(&source), sizeof(source)) == 0);
        require(::connect(back.fd, reinterpret_cast<sockaddr *>(&endpoint), sizeof(endpoint)) == 0);
        require(fcntl(back.fd, F_SETFL, O_NONBLOCK) == 0);
        SecureSession service(back.fd, false, password);
        require(service.handshake(Clock::now() + 3s, [] { return stopped.load(); }));
        Frame incoming, outgoing;
        bool attackReceived = false, raceCompleted = false;
        const bool positiveRace = scenario == "flow-race";
        const bool negativeRace = scenario == "flow-fatal-before-end";
        bool gated = false;
        Clock::time_point coordinationDeadline{};
        std::deque<std::vector<std::uint8_t>> reverseTraffic;
        std::size_t reverseBytes = 0;
        struct EraseDeferred {
            std::deque<std::vector<std::uint8_t>> &frames;
            ~EraseDeferred() { for (auto &bytes: frames) Trust::secureEraseMemory(bytes.data(), bytes.size()); }
        } eraseDeferred{reverseTraffic};
        unsigned clientFrames = 0, serviceFrames = 0;
        while (!stopped) {
            stage = "application-relay";
            if (id == 3 && (positiveRace || negativeRace) && !gated
                && std::filesystem::exists(root / "flow-arm")) {
                gated = true;
                coordinationDeadline = Clock::now() + 2s; // One original close/drain budget, never restarted.
                marker(root, "flow-armed");
            }
            if (gated) require(Clock::now() < coordinationDeadline);
            if (id == 2) {
                std::ifstream command(root / "action"); unsigned target = 0; std::string action; std::uint64_t session = 0;
                if (command >> target >> action >> session && target == id
                    && (action == "maintenance" || action == "controller" || action == "wrong")) {
                    std::filesystem::remove(root / "action");
                    require(write(client, frame(PublicSession::terminalNotice(session + (action == "wrong"), action != "controller")), "notice-write"));
                    marker(root, "acted");
                }
            }
            const int received = incoming.read(client, "frontend-read");
            if (received < 0) return;
            if (received > 0) {
                { std::ofstream out(root / ("peer-client-frame-" + std::to_string(id)));
                  out << ++clientFrames << ':' << incoming.bytes.size() << ':' << unsigned(incoming.bytes[7]); }
                receipt.clientFrames = clientFrames;
                if (std::filesystem::exists(root / "attack.bin")) {
                    std::ifstream attack(root / "attack.bin", std::ios::binary);
                    std::vector<std::uint8_t> expected{std::istreambuf_iterator<char>(attack), {}};
                    attackReceived |= incoming.bytes.size() == TransportEnvelopeBytes + expected.size()
                        && std::equal(expected.begin(), expected.end(), incoming.bytes.begin() + TransportEnvelopeBytes);
                }
                receipt.attackSeen = attackReceived;
                // All competing reverse traffic is bounded and owned, not only
                // Ready. No normal write may race ahead of qualified service End.
                if (gated) {
                    require(reverseTraffic.size() < MaxQueuedTransportFrames
                        && reverseBytes + incoming.bytes.size() <= MaxQueuedTransportPayloadBytes);
                    reverseBytes += incoming.bytes.size(); reverseTraffic.push_back(incoming.bytes);
                }
                else {
                    require(write(service, incoming.bytes, "backend-write"));
                    if (incoming.bytes.size() == TransportEnvelopeBytes + 26
                        && std::equal(incoming.bytes.begin() + TransportEnvelopeBytes,
                                       incoming.bytes.begin() + TransportEnvelopeBytes + 10, "D6LC\0\2\0\6\0\3")) {
                        const auto action = Lifecycle::deserializeParticipantAction(
                            {incoming.bytes.begin() + TransportEnvelopeBytes, incoming.bytes.end()});
                        if (action) marker(root, ("peer-Leave-forwarded-" + std::to_string(id)).c_str());
                    }
                }
                incoming.reset();
            }
            if (gated && negativeRace && !reverseTraffic.empty()
                && std::filesystem::exists(root / "flow-trigger-fatal")) {
                stage = "fatal-before-End";
                require(::shutdown(back.fd, SHUT_WR) == 0 || errno == ENOTCONN);
                require(!write(service, reverseTraffic.front()) && !service.usable());
                marker(root, "flow-fatal-before-End");
                // Never read/query pending on the failed service context. Keep
                // the distinct front context usable until actual End on peer2
                // proves server invalidation, then expose ambiguous loss to3.
                while (!stopped && Clock::now() < coordinationDeadline
                    && !std::filesystem::exists(root / "flow-service-End")) std::this_thread::sleep_for(1ms);
                require(std::filesystem::exists(root / "flow-service-End"));
                marker(root, "flow-negative-qualified");
                return;
            }
            const int sent = outgoing.read(service, "backend-read");
            if (sent < 0) { if (attackReceived) marker(root, "attack-rejected"); return; }
            if (sent > 0) {
                { std::ofstream out(root / ("peer-service-frame-" + std::to_string(id)));
                  out << ++serviceFrames << ':' << outgoing.bytes.size() << ':' << unsigned(outgoing.bytes[7]); }
                receipt.serviceFrames = serviceFrames;
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
                                   outgoing.bytes.begin() + TransportEnvelopeBytes + 8, "D6LC\0\2\0\3"))
                    end = Lifecycle::deserializeIntentionalHostEnd({outgoing.bytes.begin() + TransportEnvelopeBytes, outgoing.bytes.end()});
                const auto endDeadline = Clock::now() + 2s;
                if (end) marker(root, ("peer-End-received-" + std::to_string(id)).c_str());
                if (end && id == 2) marker(root, "flow-service-End");
                if (positiveRace && id == 3 && end && !raceCompleted) {
                    require(gated && Clock::now() < coordinationDeadline);
                    marker(root, "flow-service-End-before-fatal");
                    // The service actually sent End. Preserve that authenticated
                    // staging while genuine opposite participant traffic attempts
                    // a write against a closed OS half, then stop the fatal SDK.
                    stage = "race-shutdown";
                    require(::shutdown(back.fd, SHUT_WR) == 0 || errno == ENOTCONN);
                    stage = "race-opposite-read";
                    int traffic = reverseTraffic.empty() ? 0 : 1;
                    while (!stopped && traffic == 0 && Clock::now() < coordinationDeadline) {
                        traffic = incoming.read(client);
                        if (traffic == 0) std::this_thread::sleep_for(1ms);
                    }
                    stage = "race-opposite-write";
                    require(traffic == 1 && !write(service, reverseTraffic.empty() ? incoming.bytes : reverseTraffic.front())
                        && !service.usable());
                    marker(root, "race-write-failed"); raceCompleted = true;
                }
                stage = "authenticated-forward";
                require(write(client, outgoing.bytes, "frontend-write"));
                if (end) {
                    marker(root, ("end-forwarded-" + std::to_string(id)).c_str());
                    // Do not create an unrelated frontend fatal-write race by
                    // closing it as soon as End is sent. Consume its already
                    // accepted ordinary output until it closes, within the same
                    // original two-second boundary. Never touch fatal backend
                    // SDK state or manufacture an application acknowledgement.
                    const auto drainDeadline = gated ? coordinationDeadline : endDeadline;
                    while (!stopped && Clock::now() < drainDeadline) {
                        const int remaining = incoming.read(client);
                        if (remaining < 0) break;
                        if (remaining > 0) incoming.reset();
                        else std::this_thread::sleep_for(1ms);
                    }
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
