#include "TestHarness.h"
#include "source/network/SecureSession.h"
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <future>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
    using namespace Duel6::Network;
    using namespace std::chrono_literals;
    struct Sockets {
        int fd[2]{-1, -1};
        Sockets() {
            D6R_REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
            for (int value : fd) D6R_REQUIRE(fcntl(value, F_SETFL, O_NONBLOCK) == 0);
        }
        ~Sockets() { for (int value : fd) if (value >= 0) close(value); }
    };
    using Secret = std::shared_ptr<const SessionPassword>;
    Secret secret(const char *value) { return std::make_shared<SessionPassword>(value); }
    bool handshake(SecureSession &host, SecureSession &guest) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        auto server = std::async(std::launch::async, [&] { return host.handshake(deadline, [] { return false; }); });
        const bool client = guest.handshake(deadline, [] { return false; });
        const bool accepted = server.get();
        return client && accepted;
    }
    std::ptrdiff_t receive(SecureSession &session, std::array<unsigned char, 32> &data) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        std::ptrdiff_t count;
        do {
            count = session.receive(data.data(), data.size());
            if (count != -2) return count;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return count;
    }
    void exact(int socket, unsigned char *bytes, std::size_t size, bool writing) {
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (size && std::chrono::steady_clock::now() < deadline) {
            const auto count = writing ? ::send(socket, bytes, size, 0) : ::recv(socket, bytes, size, 0);
            if (count > 0) { size -= static_cast<std::size_t>(count); bytes += count; }
            else std::this_thread::sleep_for(1ms);
        }
        D6R_REQUIRE(size == 0);
    }
}

D6R_TEST_CASE("Native private TLS exchanges protected application data for password and unlocked sessions") {
    D6R_REQUIRE(SecureSession::supported()); // Missing physical AES is a failure, not a skip.
    for (const auto password : {Secret{}, secret("disposable-test-password")}) {
        Sockets sockets;
        SecureSession host(sockets.fd[0], true, password), guest(sockets.fd[1], false, password);
        D6R_REQUIRE(handshake(host, guest));
        const std::array<unsigned char, 3> payload{1, 2, 3};
        D6R_REQUIRE_EQ(3, guest.send(payload.data(), payload.size()));
        std::array<unsigned char, 32> result{};
        D6R_REQUIRE_EQ(3, receive(host, result));
        D6R_REQUIRE(std::equal(payload.begin(), payload.end(), result.begin()));
    }
}

D6R_TEST_CASE("Native private TLS rejects wrong password and entropy/capability restrictions") {
    Sockets sockets;
    SecureSession host(sockets.fd[0], true, secret("host-password"));
    SecureSession guest(sockets.fd[1], false, secret("different-password"));
    D6R_REQUIRE(!handshake(host, guest));
    for (bool missingHardware : {false, true}) {
        SecureSessionLimits limits;
        limits.hardwarePermitted = !missingHardware;
        limits.entropyPermitted = missingHardware;
        SecureSession denied(sockets.fd[0], true, {}, limits);
        D6R_REQUIRE(denied.expired());
        D6R_REQUIRE(!denied.handshake(std::chrono::steady_clock::now() + 10ms, [] { return false; }));
    }
}

D6R_TEST_CASE("Native private TLS rejects modified and replayed real application ciphertext") {
    for (bool replay : {false, true}) {
        Sockets sockets;
        SecureSession host(sockets.fd[0], true, secret("record-test"));
        SecureSession guest(sockets.fd[1], false, secret("record-test"));
        D6R_REQUIRE(handshake(host, guest));
        const unsigned char value = 42;
        D6R_REQUIRE_EQ(1, guest.send(&value, 1));
        std::array<unsigned char, 5> header{};
        exact(sockets.fd[0], header.data(), header.size(), false);
        const std::size_t size = (std::size_t(header[3]) << 8) | header[4];
        D6R_REQUIRE(size > 0 && size <= 18432);
        std::vector<unsigned char> record(header.begin(), header.end());
        record.resize(header.size() + size);
        exact(sockets.fd[0], record.data() + header.size(), size, false);
        std::array<unsigned char, 32> result{};
        if (replay) {
            exact(sockets.fd[1], record.data(), record.size(), true);
            D6R_REQUIRE_EQ(1, receive(host, result));
            D6R_REQUIRE_EQ(value, result[0]);
        } else record.back() ^= 1;
        exact(sockets.fd[1], record.data(), record.size(), true);
        D6R_REQUIRE_EQ(-1, receive(host, result));
        D6R_REQUIRE(host.expired());
    }
}
