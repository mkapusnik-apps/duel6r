// Real production TcpClient TLS on native Linux/Windows. No synthetic platform
// macros and no plaintext/verification bypass. Windows is excluded by default;
// its CMake opt-in, runtime permission and container preflight are ALL required.
// Use ONLY an approved disposable Windows Docker container with no host trust
// or user-profile mounts. RAII removes the generated CurrentUser ROOT on normal
// and exception exits; mandatory container destruction cleans up after forced
// termination. Never run this certificate-store test on the Windows host.
// Invocation/cleanup contract is in SessionTransportCTestRegistration.cmake.
#ifdef D6R_TRANSPORT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
using Socket = SOCKET;
constexpr Socket BadSocket = INVALID_SOCKET;
static void closeSocket(Socket s) { if (s != BadSocket) closesocket(s); }
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <csignal>
using Socket = int;
constexpr Socket BadSocket = -1;
static void closeSocket(Socket s) { if (s != BadSocket) ::close(s); }
#endif
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "source/network/SessionTransport.h"

using namespace Duel6::Network;
using namespace std::chrono_literals;
static void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static int socketError() {
#ifdef D6R_TRANSPORT_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}
static bool retryableSocketError(int error) {
#ifdef D6R_TRANSPORT_WINDOWS
    return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}
static bool nonblocking(Socket socket) {
#ifdef D6R_TRANSPORT_WINDOWS
    u_long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}
struct SocketOwner {
    Socket value = BadSocket;
    SocketOwner() = default;
    explicit SocketOwner(Socket socket) : value(socket) {}
    SocketOwner(const SocketOwner &) = delete;
    ~SocketOwner() { closeSocket(value); }
};

// SOCKET is UINT_PTR on Windows. Never narrow it through SSL_set_fd(int).
// The BIO borrows a stable SocketOwner and leaves native close ownership to it.
static BIO_METHOD *nativeSocketBio() {
    static std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> method([] {
        std::unique_ptr<BIO_METHOD, decltype(&BIO_meth_free)> m(
            BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(), "test native socket"), BIO_meth_free);
        check(bool(m), "test socket BIO allocation");
        check(BIO_meth_set_create(m.get(), [](BIO *b) { BIO_set_init(b, 1); return 1; }) == 1, "BIO create callback");
        check(BIO_meth_set_destroy(m.get(), [](BIO *) { return 1; }) == 1, "BIO destroy callback");
        check(BIO_meth_set_ctrl(m.get(), [](BIO *, int cmd, long, void *) -> long {
            return cmd == BIO_CTRL_FLUSH ? 1 : 0;
        }) == 1, "BIO control callback");
        check(BIO_meth_set_read(m.get(), [](BIO *b, char *data, int size) {
            BIO_clear_retry_flags(b);
            const auto socket = static_cast<SocketOwner *>(BIO_get_data(b))->value;
            const int count = static_cast<int>(recv(socket, data, size, 0));
            if (count < 0 && retryableSocketError(socketError())) BIO_set_retry_read(b);
            return count;
        }) == 1, "BIO read callback");
        check(BIO_meth_set_write(m.get(), [](BIO *b, const char *data, int size) {
            BIO_clear_retry_flags(b);
            const auto socket = static_cast<SocketOwner *>(BIO_get_data(b))->value;
            const int count = static_cast<int>(send(socket, data, size, 0));
            if (count < 0 && retryableSocketError(socketError())) BIO_set_retry_write(b);
            return count;
        }) == 1, "BIO write callback");
        return m.release();
    }(), BIO_meth_free);
    return method.get();
}
#ifdef D6R_TRANSPORT_WINDOWS
static void requireDisposableWindowsContainer() {
#ifndef D6R_TLS_DISPOSABLE_WINDOWS_OPT_IN
    throw std::runtime_error("Windows TLS trust test was not explicitly enabled at build time");
#else
    const char *permission = std::getenv("D6R_DISPOSABLE_WINDOWS_CONTAINER");
    check(permission && std::string(permission) == "1", "disposable Windows container invocation required");
    // Read isolation metadata, never a certificate store, before permission.
    // Windows container runtimes expose ContainerType for process/Hyper-V
    // isolation. Unknown or absent markers are not accepted as host opt-ins.
    DWORD containerType = 0, size = sizeof(containerType);
    const LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control",
        "ContainerType", RRF_RT_REG_DWORD, nullptr, &containerType, &size);
    check(status == ERROR_SUCCESS && (containerType == 1 || containerType == 2),
          "Windows container isolation marker missing or unsupported; trust store not accessed");
#endif
}
#endif
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using Cert = std::unique_ptr<X509, decltype(&X509_free)>;
static Key key() {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    check(bool(ctx), "key context");
    check(EVP_PKEY_keygen_init(ctx.get()) == 1 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) == 1, "key setup");
    EVP_PKEY *raw = nullptr; check(EVP_PKEY_keygen(ctx.get(), &raw) == 1, "key generation");
    return Key(raw, EVP_PKEY_free);
}
static Cert certificate(EVP_PKEY *subjectKey, X509 *issuer, EVP_PKEY *issuerKey, bool ca, bool wrong = false) {
    Cert cert(X509_new(), X509_free); check(bool(cert), "certificate allocation");
    check(X509_set_version(cert.get(), 2) == 1 && ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), ca ? 1 : 2) == 1, "certificate version");
    check(X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60) && X509_gmtime_adj(X509_getm_notAfter(cert.get()), 3600), "certificate validity");
    check(X509_set_pubkey(cert.get(), subjectKey) == 1, "certificate public key");
    auto *name = X509_get_subject_name(cert.get());
    check(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char *>(ca ? "Duel6 disposable native TLS test CA" : "localhost"), -1, -1, 0) == 1, "certificate name");
    check(X509_set_issuer_name(cert.get(), issuer ? X509_get_subject_name(issuer) : name) == 1, "certificate issuer");
    X509V3_CTX context{}; X509V3_set_ctx(&context, issuer ? issuer : cert.get(), cert.get(), nullptr, nullptr, 0);
    auto extension = [&](int nid, const char *value) {
        std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> ext(
            X509V3_EXT_conf_nid(nullptr, &context, nid, const_cast<char *>(value)), X509_EXTENSION_free);
        check(ext && X509_add_ext(cert.get(), ext.get(), -1) == 1, "certificate extension");
    };
    extension(NID_basic_constraints, ca ? "critical,CA:TRUE" : "critical,CA:FALSE");
    extension(NID_key_usage, ca ? "critical,keyCertSign,cRLSign" : "critical,digitalSignature,keyEncipherment");
    if (!ca) {
        extension(NID_ext_key_usage, "serverAuth");
        extension(NID_subject_alt_name, wrong ? "DNS:wrong.fixture.invalid" : "DNS:localhost,IP:127.0.0.1");
    }
    check(X509_sign(cert.get(), issuerKey ? issuerKey : subjectKey, EVP_sha256()) > 0, "certificate signature");
    return cert;
}

class TrustFixture {
    bool cleaned = false;
#ifdef D6R_TRANSPORT_WINDOWS
    HCERTSTORE store = nullptr;
    PCCERT_CONTEXT added = nullptr;
#else
    std::filesystem::path root;
    std::string oldFile, oldDir;
    bool hadFile = false, hadDir = false;
#endif
public:
    explicit TrustFixture(X509 *ca) {
#ifdef D6R_TRANSPORT_WINDOWS
        requireDisposableWindowsContainer();
        store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, "ROOT");
        check(store != nullptr, "CurrentUser ROOT unavailable");
        const int size = i2d_X509(ca, nullptr);
        if (size <= 0) { CertCloseStore(store, 0); store = nullptr; throw std::runtime_error("test root encoding"); }
        std::vector<unsigned char> der(static_cast<std::size_t>(size)); auto *cursor = der.data();
        if (size <= 0 || i2d_X509(ca, &cursor) != size || !CertAddEncodedCertificateToStore(store,
            X509_ASN_ENCODING, der.data(), static_cast<DWORD>(der.size()), CERT_STORE_ADD_NEW, &added)) {
            CertCloseStore(store, 0); store = nullptr;
            throw std::runtime_error("temporary CurrentUser test root could not be added");
        }
#else
        std::array<unsigned char, 12> random{}; check(RAND_bytes(random.data(), random.size()) == 1, "temporary identity");
        std::string suffix; for (auto byte : random) suffix += std::to_string(byte) + "-";
        root = std::filesystem::temp_directory_path() / ("duel6r-native-tls-" + suffix);
        check(std::filesystem::create_directory(root), "temporary trust directory");
        std::filesystem::permissions(root, std::filesystem::perms::owner_all);
        std::filesystem::create_directory(root / "empty");
        FILE *file = fopen((root / "ca.pem").c_str(), "wb"); check(file != nullptr, "temporary CA file");
        const int written = PEM_write_X509(file, ca); fclose(file); check(written == 1, "temporary CA encoding");
        if (const char *value = std::getenv("SSL_CERT_FILE")) { hadFile = true; oldFile = value; }
        if (const char *value = std::getenv("SSL_CERT_DIR")) { hadDir = true; oldDir = value; }
        setenv("SSL_CERT_FILE", (root / "ca.pem").c_str(), 1); setenv("SSL_CERT_DIR", (root / "empty").c_str(), 1);
#endif
    }
    bool remove() {
        if (cleaned) return true;
        cleaned = true;
        std::cout << "TRUST cleanup begin" << std::endl;
#ifdef D6R_TRANSPORT_WINDOWS
        bool removed = true;
        if (added) {
            const auto identity = CertDuplicateCertificateContext(added);
            removed = CertDeleteCertificateFromStore(added) != 0; added = nullptr;
            const auto remaining = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, identity, nullptr);
            removed = removed && !remaining;
            if (remaining) CertFreeCertificateContext(remaining);
            CertFreeCertificateContext(identity);
        }
        if (store) { CertCloseStore(store, 0); store = nullptr; }
        std::cout << (removed ? "TRUST cleanup confirmed" : "TRUST cleanup FAILED; disposable container destruction required") << std::endl;
        return removed;
#else
        if (hadFile) setenv("SSL_CERT_FILE", oldFile.c_str(), 1); else unsetenv("SSL_CERT_FILE");
        if (hadDir) setenv("SSL_CERT_DIR", oldDir.c_str(), 1); else unsetenv("SSL_CERT_DIR");
        std::error_code error; std::filesystem::remove_all(root, error);
        std::cout << (error ? "TRUST cleanup FAILED" : "TRUST cleanup confirmed") << std::endl;
        return !error;
#endif
    }
    ~TrustFixture() { (void) remove(); }
};

class TlsPeer {
    SocketOwner listener;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ctx{SSL_CTX_new(TLS_server_method()), SSL_CTX_free};
    std::thread worker;
    std::atomic<bool> stopping{false};
    using Time = std::chrono::steady_clock;
    Time::time_point deadline;

    bool active() {
        if (stopping) { outcome = Outcome::Cancelled; return false; }
        if (Time::now() >= deadline) { outcome = Outcome::Deadline; return false; }
        return true;
    }
    template<typename Operation>
    int io(SSL *ssl, Operation operation) {
        while (active()) {
            ERR_clear_error();
            const int count = operation();
            if (count > 0) return count;
            const int native = socketError();
            const int error = SSL_get_error(ssl, count);
            lastSslError = error; lastSocketError = native;
            if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
                outcome = Outcome::TlsFailure; return -1;
            }
            // Nonblocking BIO; the same arguments are retried until completion.
            // One absolute budget covers accept, handshake, read AND write.
            std::this_thread::sleep_for(5ms);
        }
        return -1;
    }
public:
    enum class Stage { Starting, Accept, Handshake, Read, Write };
    enum class Outcome { Pending, Complete, Cancelled, Deadline, SocketFailure, TlsFailure, InternalFailure };
    std::uint16_t port = 0;
    std::atomic<unsigned> applicationBytes{0};
    std::atomic<bool> finished{false}, validRequest{false};
    std::atomic<Stage> stage{Stage::Starting};
    std::atomic<Outcome> outcome{Outcome::Pending};
    std::atomic<int> lastSslError{0}, lastSocketError{0};
    void describe() const {
        static const char *stages[]{"starting", "accept", "handshake", "read", "write"};
        static const char *outcomes[]{"pending", "complete", "cancelled", "deadline", "socket-failure", "tls-failure", "internal-failure"};
        std::cout << "PEER stage=" << stages[static_cast<unsigned>(stage.load())]
                  << " outcome=" << outcomes[static_cast<unsigned>(outcome.load())]
                  << " last-ssl-error=" << lastSslError.load() << " last-native-error=" << lastSocketError.load()
                  << " application-bytes=" << applicationBytes.load() << std::endl;
    }
    TlsPeer(X509 *cert, EVP_PKEY *key, const std::vector<std::uint8_t> &secret,
            std::chrono::milliseconds budget = 5s) {
        check(ctx && SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION) == 1, "TLS peer context");
        check(SSL_CTX_use_certificate(ctx.get(), cert) == 1 && SSL_CTX_use_PrivateKey(ctx.get(), key) == 1, "TLS peer identity");
        listener.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); check(listener.value != BadSocket, "TLS listener socket");
        check(nonblocking(listener.value), "TLS listener nonblocking configuration");
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(bind(listener.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 && listen(listener.value, 1) == 0, "TLS bind/listen");
#ifdef D6R_TRANSPORT_WINDOWS
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        check(getsockname(listener.value, reinterpret_cast<sockaddr *>(&address), &length) == 0, "TLS listener port");
        port = ntohs(address.sin_port);
        deadline = Time::now() + budget;
        worker = std::thread([this, secret] {
            try {
                SocketOwner peer;
                stage = Stage::Accept;
                while (active()) {
                    peer.value = accept(listener.value, nullptr, nullptr);
                    if (peer.value != BadSocket) break;
                    lastSocketError = socketError();
                    if (!retryableSocketError(lastSocketError)) { outcome = Outcome::SocketFailure; break; }
                    std::this_thread::sleep_for(5ms);
                }
                if (peer.value == BadSocket) { finished = true; return; }
                if (!nonblocking(peer.value)) { lastSocketError = socketError(); outcome = Outcome::SocketFailure; finished = true; return; }
                std::unique_ptr<SSL, decltype(&SSL_free)> ssl(SSL_new(ctx.get()), SSL_free);
                check(bool(ssl), "TLS peer allocation");
                BIO *bio = BIO_new(nativeSocketBio()); check(bio != nullptr, "TLS peer BIO");
                BIO_set_data(bio, &peer); SSL_set_bio(ssl.get(), bio, bio);
                stage = Stage::Handshake;
                if (io(ssl.get(), [&] { return SSL_accept(ssl.get()); }) == 1) {
                    stage = Stage::Read;
                    std::vector<std::uint8_t> bytes(12 + secret.size());
                    std::size_t offset = 0;
                    while (offset < bytes.size()) {
                        const int count = io(ssl.get(), [&] { return SSL_read(ssl.get(), bytes.data() + offset, static_cast<int>(bytes.size() - offset)); });
                        if (count <= 0) break;
                        offset += count; applicationBytes += count;
                    }
                    validRequest = offset == bytes.size() && std::equal(secret.begin(), secret.end(), bytes.begin() + 12);
                    if (validRequest) {
                        // Return the framed application data through TLS, never through logs.
                        stage = Stage::Write;
                        offset = 0;
                        while (offset < bytes.size()) {
                            const int count = io(ssl.get(), [&] { return SSL_write(ssl.get(), bytes.data() + offset, static_cast<int>(bytes.size() - offset)); });
                            if (count <= 0) break;
                            offset += count;
                        }
                        if (offset == bytes.size()) outcome = Outcome::Complete;
                    }
                }
            } catch (...) { outcome = Outcome::InternalFailure; }
            finished = true;
        });
    }
    void stop() { stopping = true; if (worker.joinable()) worker.join(); }
    ~TlsPeer() { stop(); }
};

template<typename Predicate>
static bool waitUntil(Predicate predicate, std::chrono::milliseconds budget) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    do {
        if (predicate()) return true; // receive() consumes a frame: never call it twice on success.
        std::this_thread::sleep_for(5ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return predicate();
}
static void connectWithoutTls(SocketOwner &raw, std::uint16_t port) {
    raw.value = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(raw.value != BadSocket && nonblocking(raw.value), "stall-probe socket configuration");
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(port);
    const int result = ::connect(raw.value, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    const int error = result == 0 ? 0 : socketError();
#ifdef D6R_TRANSPORT_WINDOWS
    check(result == 0 || error == WSAEWOULDBLOCK || error == WSAEINPROGRESS, "stall-probe connect");
#else
    check(result == 0 || error == EINPROGRESS || error == EINTR, "stall-probe connect");
#endif
}
static void checkPeerTeardown(X509 *cert, EVP_PKEY *key, const std::vector<std::uint8_t> &secret) {
    struct ExpectedUnwind {};
    for (bool connected : {false, true}) {
        std::cout << "BEGIN peer exception-unwind " << (connected ? "stalled handshake" : "no connection") << std::endl;
        SocketOwner raw; // Stay open while the peer unwinds, to expose a stuck join.
        auto unwindAt = std::chrono::steady_clock::now();
        try {
            TlsPeer peer(cert, key, secret);
            if (connected) connectWithoutTls(raw, peer.port);
            check(waitUntil([&] { return peer.stage == (connected ? TlsPeer::Stage::Handshake : TlsPeer::Stage::Accept); }, 2s),
                  "peer did not reach unwind precondition");
            unwindAt = std::chrono::steady_clock::now();
            throw ExpectedUnwind{};
        } catch (const ExpectedUnwind &) {}
        check(std::chrono::steady_clock::now() - unwindAt < 2s, "peer cancellation exceeded teardown bound");
        std::cout << "PASS peer exception-unwind " << (connected ? "stalled handshake" : "no connection") << std::endl;
    }
    std::cout << "BEGIN peer absolute handshake deadline" << std::endl;
    TlsPeer peer(cert, key, secret, 250ms);
    SocketOwner raw; connectWithoutTls(raw, peer.port);
    check(waitUntil([&] { return peer.finished.load(); }, 2s), "peer handshake deadline was not bounded");
    peer.describe();
    check(peer.stage == TlsPeer::Stage::Handshake && peer.outcome == TlsPeer::Outcome::Deadline && peer.applicationBytes == 0,
          "stalled handshake did not select deadline without application bytes");
    std::cout << "PASS peer absolute handshake deadline" << std::endl;
}

int main(int argc, char **argv) {
    const char *stage = "entry";
    // CTest captures stdout through a pipe; newline alone does not flush it.
    std::cout << std::unitbuf; std::cerr << std::unitbuf;
    std::cout << "BEGIN portable TLS; native socket bits=" << sizeof(Socket) * 8
              << "; OpenSSL=" << OpenSSL_version(OPENSSL_VERSION) << std::endl;
    try {
#ifdef D6R_TRANSPORT_WINDOWS
        stage = "disposable Windows preflight";
        std::cout << "BEGIN " << stage << std::endl;
        check(argc == 2 && std::string(argv[1]) == "--allow-disposable-windows-container-test-root",
              "explicit disposable-container trust permission required");
        requireDisposableWindowsContainer();
        struct Winsock {
            Winsock() { WSADATA data{}; check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock startup"); }
            ~Winsock() { WSACleanup(); }
        } winsock;
#else
        (void) argc; (void) argv; std::signal(SIGPIPE, SIG_IGN);
#endif
        stage = "key generation";
        std::cout << "BEGIN CA key generation" << std::endl; auto caKey = key();
        std::cout << "BEGIN untrusted CA key generation" << std::endl; auto otherKey = key();
        std::cout << "BEGIN leaf key generation" << std::endl; auto leafKey = key();
        stage = "certificate generation";
        std::cout << "BEGIN " << stage << std::endl;
        auto ca = certificate(caKey.get(), nullptr, nullptr, true);
        auto otherCa = certificate(otherKey.get(), nullptr, nullptr, true);
        auto valid = certificate(leafKey.get(), ca.get(), caKey.get(), false);
        auto wrong = certificate(leafKey.get(), ca.get(), caKey.get(), false, true);
        auto untrusted = certificate(leafKey.get(), otherCa.get(), otherKey.get(), false);
        std::vector<std::uint8_t> secret(48); check(RAND_bytes(secret.data(), secret.size()) == 1, "secret fixture generation");
        stage = "peer bounded teardown checks";
        checkPeerTeardown(valid.get(), leafKey.get(), secret);
        stage = "temporary test trust setup";
        std::cout << "BEGIN " << stage << std::endl;
        TrustFixture trust(ca.get());
        std::cout << "TRUST setup complete" << std::endl;
        const char *cases[]{"trusted hostname", "trusted IP", "untrusted chain: zero application bytes", "wrong hostname: zero application bytes"};
        for (unsigned scenario = 0; scenario < 4; ++scenario) {
            stage = cases[scenario];
            std::cout << "CASE " << stage << " begin" << std::endl;
            const bool trusted = scenario < 2;
            TlsPeer peer(scenario == 2 ? untrusted.get() : scenario == 3 ? wrong.get() : valid.get(), leafKey.get(), secret);
            SessionTransportDependencies dependencies; dependencies.publicTls = true;
            TcpClient client(dependencies);
            try {
                std::cout << "CONNECT begin" << std::endl;
                check(client.start({scenario == 1 ? "127.0.0.1" : "localhost", peer.port}), "TLS client start");
                const bool connected = client.waitForConnected(10s);
                std::cout << "CONNECT connected=" << connected << " state=" << static_cast<int>(client.state())
                          << " failure=" << static_cast<int>(client.failure()) << std::endl;
                if (trusted) {
                    check(connected, "trusted native TLS failed");
                    std::cout << "APPLICATION round trip begin (payload withheld)" << std::endl;
                    check(client.connection()->sendSensitive(secret) == SendResult::Accepted, "sensitive TLS frame not queued");
                    TransportFrame frame; bool received = false;
                    check(waitUntil([&] { return received = client.connection()->receive(frame); }, 5s)
                        && received && frame.payload == secret && peer.validRequest, "verified TLS application round trip failed");
                } else {
                    check(!connected && client.failure() == TransportFailure::SecureConnectionFailed, "invalid certificate was not a security failure");
                }
                std::cout << "CLIENT close begin" << std::endl;
                client.close();
                std::cout << "CLIENT close complete" << std::endl;
                check(waitUntil([&] { return peer.finished.load(); }, 6s), "TLS peer did not finish");
                peer.describe();
                check(peer.outcome == (trusted ? TlsPeer::Outcome::Complete : TlsPeer::Outcome::TlsFailure), "unexpected TLS peer terminal outcome");
                if (!trusted) check(peer.applicationBytes == 0, "application secret disclosed before certificate validation");
            } catch (...) {
                std::cout << "CASE failure; peer cancellation begin" << std::endl;
                peer.describe(); peer.stop();
                std::cout << "PEER joined; client exception cleanup follows" << std::endl;
                throw;
            }
            std::cout << "PASS native TLS " << stage << std::endl;
        }
        stage = "temporary test trust cleanup";
        check(trust.remove(), "temporary test trust cleanup failed");
        std::cout << "PASS native test trust removed" << std::endl;
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL native TLS at " << stage << ": " << error.what() << std::endl;
        return 1;
    }
}
