#include "TestHarness.h"
#include "source/network/SecureSession.h"
#include <algorithm>
#include <array>
#include <fcntl.h>
#include <future>
#include <atomic>
#include <cerrno>
#include "source/network/SessionLifecycle.h"
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#ifdef D6R_TEST_WRAP_SECURE_SDK
#include <mbedtls/ssl.h>
// Observe actual SDK entries, not only BIO callbacks (pending may not use BIO).
static std::atomic<unsigned> sdkReads{0}, sdkWrites{0}, sdkPending{0};
extern "C" int __real_mbedtls_ssl_read(mbedtls_ssl_context *, unsigned char *, std::size_t);
extern "C" int __real_mbedtls_ssl_write(mbedtls_ssl_context *, const unsigned char *, std::size_t);
extern "C" int __real_mbedtls_ssl_check_pending(const mbedtls_ssl_context *);
extern "C" int __wrap_mbedtls_ssl_read(mbedtls_ssl_context *ctx, unsigned char *data, std::size_t size) {
    ++sdkReads; return __real_mbedtls_ssl_read(ctx, data, size);
}
extern "C" int __wrap_mbedtls_ssl_write(mbedtls_ssl_context *ctx, const unsigned char *data, std::size_t size) {
    ++sdkWrites; return __real_mbedtls_ssl_write(ctx, data, size);
}
extern "C" int __wrap_mbedtls_ssl_check_pending(const mbedtls_ssl_context *ctx) {
    ++sdkPending; return __real_mbedtls_ssl_check_pending(ctx);
}
#endif

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

D6R_TEST_CASE("Common Password validates exact UTF8 bytes spaces C0 C1 and scalar boundaries") {
    for (const std::string &value : {std::string(), std::string(" leading trailing "), std::string(u8"é雪😀"),
                                     std::string(128, 'x'), std::string(126, 'x') + u8"é"}) {
        D6R_REQUIRE(SessionPassword::valid(value));
        SessionPassword stored(value); D6R_REQUIRE(std::string(stored.value()) == value);
    }
    for (const std::string &value : {std::string(129, 'x'), std::string("a\0b", 3), std::string("\n"),
        std::string("\x7f"), std::string("\xc2\x80"), std::string("\xc2\x9f"), std::string("\xc0\xaf"),
        std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\xc3"), std::string("\x80")}) {
        D6R_REQUIRE(!SessionPassword::valid(value));
        D6R_REQUIRE_THROW((void) SessionPassword{value}, std::invalid_argument);
    }
}

namespace {
    class ByteSocket final : public SecureByteStream {
    public:
        int socket;
        std::atomic<bool> failWrites{false};
        std::atomic<unsigned> reads{0}, writes{0}, pendingQueries{0};
        std::function<void()> beforeWriteFailure;
        explicit ByteSocket(int socket) : socket(socket) {}
        std::ptrdiff_t read(unsigned char *data, std::size_t size) override {
            ++reads; const auto count = ::recv(socket, data, std::min<std::size_t>(size, 3), 0);
            return count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ? -2 : count;
        }
        std::ptrdiff_t write(const unsigned char *data, std::size_t size) override {
            ++writes; if (failWrites) {
                if (beforeWriteFailure) beforeWriteFailure();
                return -1;
            }
            const auto count = ::send(socket, data, std::min<std::size_t>(size, 7), MSG_NOSIGNAL);
            return count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ? -3 : count;
        }
        bool pending() const override { ++const_cast<ByteSocket *>(this)->pendingQueries; return false; }
    };
}

D6R_TEST_CASE("Common PAKE byte BIO shorts authenticate and fatal write preserves accepted End with zero later context operations") {
    Sockets sockets;
    auto stream = std::make_shared<ByteSocket>(sockets.fd[1]);
    SecureSession host(sockets.fd[0], true, secret(" Unicode-é "));
    SecureSession guest(sockets.fd[1], false, secret(" Unicode-é "), {}, stream);
    D6R_REQUIRE(handshake(host, guest));
    const auto notice = Lifecycle::serializeIntentionalHostEnd({91});
    D6R_REQUIRE_EQ(static_cast<std::ptrdiff_t>(notice.size()), host.send(notice.data(), notice.size()));
    std::array<unsigned char, 32> buffer{};
    std::vector<unsigned char> accepted;
    std::promise<void> staged, release;
    auto released = release.get_future().share();
    auto reading = std::async(std::launch::async, [&] {
        std::ptrdiff_t count = -2;
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (count == -2 && std::chrono::steady_clock::now() < deadline) count = guest.receive(buffer.data(), buffer.size(), [&](std::size_t bytes) {
            accepted.assign(buffer.begin(), buffer.begin() + bytes);
            staged.set_value(); released.wait();
        });
        return count;
    });
    const bool received = staged.get_future().wait_for(2s) == std::future_status::ready;
    if (!received) release.set_value();
    D6R_REQUIRE(received);
    stream->failWrites = true;
    auto writing = std::async(std::launch::async, [&] { const unsigned char value = 1; return guest.send(&value, 1); });
    const bool blocked = writing.wait_for(20ms) == std::future_status::timeout;
    release.set_value();
    D6R_REQUIRE(blocked); // Fatal cannot outrun authenticated staging.
    D6R_REQUIRE_EQ(static_cast<std::ptrdiff_t>(notice.size()), reading.get());
    D6R_REQUIRE_EQ(-1, writing.get());
    D6R_REQUIRE(Lifecycle::deserializeIntentionalHostEnd(accepted).has_value());
    const auto reads = stream->reads.load(), writes = stream->writes.load(), pending = stream->pendingQueries.load();
    D6R_REQUIRE_EQ(-1, guest.receive(buffer.data(), buffer.size()));
    D6R_REQUIRE_EQ(-1, guest.send(buffer.data(), 1));
    D6R_REQUIRE(!guest.pending() && !guest.usable() && guest.expired());
    D6R_REQUIRE_EQ(reads, stream->reads.load()); D6R_REQUIRE_EQ(writes, stream->writes.load());
    D6R_REQUIRE_EQ(pending, stream->pendingQueries.load());
}

D6R_TEST_CASE("Common PAKE reader waiting behind fatal writer makes zero subsequent SDK operations") {
    Sockets sockets;
    auto stream = std::make_shared<ByteSocket>(sockets.fd[1]);
    SecureSession host(sockets.fd[0], true, secret("fatal-test"));
    SecureSession guest(sockets.fd[1], false, secret("fatal-test"), {}, stream);
    D6R_REQUIRE(handshake(host, guest));
    std::promise<void> inWrite, release;
    auto released = release.get_future().share();
    stream->beforeWriteFailure = [&] { inWrite.set_value(); released.wait_for(3s); };
    stream->failWrites = true;
    auto writing = std::async(std::launch::async, [&] { const unsigned char value = 1; return guest.send(&value, 1); });
    const bool entered = inWrite.get_future().wait_for(2s) == std::future_status::ready;
    if (!entered) release.set_value();
    D6R_REQUIRE(entered);
    const auto reads = stream->reads.load(), writes = stream->writes.load(), pending = stream->pendingQueries.load();
#ifdef D6R_TEST_WRAP_SECURE_SDK
    const auto directReads = sdkReads.load(), directWrites = sdkWrites.load(), directPending = sdkPending.load();
#endif
    std::promise<void> readerStarted;
    auto reading = std::async(std::launch::async, [&] {
        std::array<unsigned char, 32> buffer{};
        readerStarted.set_value();
        return guest.receive(buffer.data(), buffer.size());
    });
    readerStarted.get_future().wait();
    const bool blocked = reading.wait_for(20ms) == std::future_status::timeout;
    release.set_value();
    D6R_REQUIRE(blocked);
    D6R_REQUIRE_EQ(-1, writing.get()); D6R_REQUIRE_EQ(-1, reading.get());
    const unsigned char byte = 0;
    D6R_REQUIRE_EQ(-1, guest.send(&byte, 1));
    D6R_REQUIRE(!guest.pending() && !guest.usable() && guest.expired());
    D6R_REQUIRE_EQ(reads, stream->reads.load()); D6R_REQUIRE_EQ(writes, stream->writes.load());
    D6R_REQUIRE_EQ(pending, stream->pendingQueries.load());
#ifdef D6R_TEST_WRAP_SECURE_SDK
    D6R_REQUIRE_EQ(directReads, sdkReads.load()); D6R_REQUIRE_EQ(directWrites, sdkWrites.load());
    D6R_REQUIRE_EQ(directPending, sdkPending.load());
#endif
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
