// Real production TcpClient TLS on native Linux/Windows. No synthetic platform
// macros and no plaintext/verification bypass. Windows is excluded by default;
// its CMake opt-in, runtime permission and container preflight are ALL required.
// Use ONLY an approved disposable Windows Docker container with no host trust
// or registry/profile mounts. RAII removes only the generated root from the
// container's LocalMachine ROOT on normal and exception exits; mandatory
// container destruction cleans up after forced
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
#include "source/network/SessionLifecycle.h"
#ifdef D6R_TEST_OBSERVE_STAGING
#include <mbedtls/ssl.h>
// Observe live caller-owned plaintext immediately before real deallocation.
// SDK calls and allocators delegate unchanged; no bytes/addresses are logged.
namespace StagingObservation {
    std::array<unsigned char, 48> expected{};
    std::atomic<void *> watched{nullptr};
    std::atomic<bool> staged{false}, released{false}, erased{false};
    std::atomic<std::size_t> readSize{47}, allocationBytes{0};
    thread_local bool receivingApplication = false;
    void reset(std::size_t size, const std::vector<std::uint8_t> &value) {
        std::copy(value.begin(), value.end(), expected.begin());
        watched = nullptr; staged = false; released = false; erased = false;
        allocationBytes = 0; readSize = size;
    }
    void beforeDelete(void *pointer, std::size_t size) noexcept {
        void *match = pointer;
        if (pointer && watched.compare_exchange_strong(match, nullptr)) {
            const auto *bytes = static_cast<const unsigned char *>(pointer);
            erased = std::all_of(bytes, bytes + readSize.load(), [](unsigned char byte) { return byte == 0; });
            allocationBytes = size;
            released = true;
        }
    }
}
extern "C" int __real_mbedtls_ssl_read(mbedtls_ssl_context *, unsigned char *, std::size_t);
extern "C" int __wrap_mbedtls_ssl_read(mbedtls_ssl_context *ctx, unsigned char *bytes, std::size_t size) {
    const int result = __real_mbedtls_ssl_read(ctx, bytes, size);
    if (StagingObservation::receivingApplication && result == static_cast<int>(StagingObservation::readSize.load())
        && std::equal(StagingObservation::expected.begin(), StagingObservation::expected.begin() + result, bytes)) {
        StagingObservation::watched = bytes;
        StagingObservation::staged = true;
    }
    return result;
}
void realDelete(void *) noexcept __asm__("__real__ZdlPv");
void wrapDelete(void *) noexcept __asm__("__wrap__ZdlPv");
void wrapDelete(void *pointer) noexcept { StagingObservation::beforeDelete(pointer, 0); realDelete(pointer); }
void realSizedDelete(void *, std::size_t) noexcept __asm__("__real__ZdlPvm");
void wrapSizedDelete(void *, std::size_t) noexcept __asm__("__wrap__ZdlPvm");
void wrapSizedDelete(void *pointer, std::size_t size) noexcept {
    StagingObservation::beforeDelete(pointer, size); realSizedDelete(pointer, size);
}
#endif

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
    static bool productionRootVisibility(PCCERT_CONTEXT identity, bool expectedPresent) {
        // Match TlsStream's real, unchanged system-trust reader. CurrentUser's
        // logical ROOT includes the LocalMachine physical sibling (Microsoft:
        // https://learn.microsoft.com/windows/win32/seccrypto/system-store-locations).
        std::cout << "TRUST CertOpenSystemStoreA(CurrentUser ROOT reader) begin" << std::endl;
        HCERTSTORE reader = CertOpenSystemStoreA(0, "ROOT");
        const DWORD openError = reader ? ERROR_SUCCESS : GetLastError();
        std::cout << "TRUST CertOpenSystemStoreA end win32=" << openError << std::endl;
        if (!reader) return false;
        std::cout << "TRUST CertFindCertificateInStore(production reader) begin" << std::endl;
        const auto found = CertFindCertificateInStore(reader, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                      0, CERT_FIND_EXISTING, identity, nullptr);
        const DWORD findError = found ? ERROR_SUCCESS : GetLastError();
        const bool matched = found != nullptr;
        std::cout << "TRUST CertFindCertificateInStore end present=" << matched << " win32=" << findError << std::endl;
        if (found) CertFreeCertificateContext(found);
        std::cout << "TRUST CertCloseStore(production reader) begin" << std::endl;
        const bool closed = CertCloseStore(reader, 0) != 0;
        const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
        std::cout << "TRUST CertCloseStore end win32=" << closeError << std::endl;
        return closed && (expectedPresent ? matched : !matched && findError == static_cast<DWORD>(CRYPT_E_NOT_FOUND));
    }
#else
    std::filesystem::path root;
    std::string oldFile, oldDir;
    bool hadFile = false, hadDir = false;
#endif
public:
    explicit TrustFixture(X509 *ca) {
#ifdef D6R_TRANSPORT_WINDOWS
        std::cout << "TRUST disposable-container guard begin" << std::endl;
        requireDisposableWindowsContainer();
        std::cout << "TRUST disposable-container guard complete" << std::endl;
        // Use the documented physical-store provider, NOT raw registry writes,
        // a protected-root-list policy edit, or UNPROTECTED_FLAG. The latter is
        // a read/filter override, not a silent trust-import API. Container-only
        // LocalMachine provisioning avoids the CurrentUser protected-root write
        // path while preserving the normal production ROOT reader and TLS checks.
        // https://learn.microsoft.com/windows/win32/api/wincrypt/nf-wincrypt-certopenstore
        std::cout << "TRUST i2d_X509(size) begin" << std::endl;
        const int size = i2d_X509(ca, nullptr);
        std::cout << "TRUST i2d_X509(size) end bytes=" << size << std::endl;
        check(size > 0, "test root encoding size");
        std::vector<unsigned char> der(static_cast<std::size_t>(size)); auto *cursor = der.data();
        std::cout << "TRUST i2d_X509(encode) begin" << std::endl;
        const int encoded = i2d_X509(ca, &cursor);
        std::cout << "TRUST i2d_X509(encode) end bytes=" << encoded << std::endl;
        check(encoded == size, "test root encoding");
        try {
            std::cout << "TRUST CertOpenStore(container LocalMachine physical ROOT) begin" << std::endl;
            store = CertOpenStore(CERT_STORE_PROV_SYSTEM_REGISTRY_A, 0, 0,
                                  CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_OPEN_EXISTING_FLAG, "ROOT");
            const DWORD openError = store ? ERROR_SUCCESS : GetLastError();
            std::cout << "TRUST CertOpenStore end win32=" << openError << std::endl;
            check(store != nullptr, "container LocalMachine ROOT unavailable; existing write permission required, no elevation/fallback");
            std::cout << "TRUST CertAddEncodedCertificateToStore(ADD_NEW) begin" << std::endl;
            const bool inserted = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, der.data(),
                static_cast<DWORD>(der.size()), CERT_STORE_ADD_NEW, &added) != 0;
            const DWORD addError = inserted ? ERROR_SUCCESS : GetLastError();
            std::cout << "TRUST CertAddEncodedCertificateToStore end win32=" << addError << std::endl;
            check(inserted && added, "temporary container test root could not be added; no replacement/fallback");
            check(productionRootVisibility(added, true), "generated root not visible through production CurrentUser ROOT reader");
        } catch (...) {
            // A failed constructor does not run ~TrustFixture. Remove any root
            // already inserted before propagating a visibility/setup failure.
            (void) remove();
            throw;
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
            std::cout << "TRUST CertDuplicateCertificateContext(cleanup identity) begin" << std::endl;
            const auto identity = CertDuplicateCertificateContext(added);
            const DWORD duplicateError = identity ? ERROR_SUCCESS : GetLastError();
            std::cout << "TRUST CertDuplicateCertificateContext end win32=" << duplicateError << std::endl;
            std::cout << "TRUST CertDeleteCertificateFromStore(container test root only) begin" << std::endl;
            const bool deleted = CertDeleteCertificateFromStore(added) != 0; added = nullptr;
            const DWORD deleteError = deleted ? ERROR_SUCCESS : GetLastError();
            std::cout << "TRUST CertDeleteCertificateFromStore end win32=" << deleteError << std::endl;
            removed = deleted && identity;
            if (identity) {
                std::cout << "TRUST CertFindCertificateInStore(physical ROOT removal) begin" << std::endl;
                const auto remaining = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                                  0, CERT_FIND_EXISTING, identity, nullptr);
                const DWORD findError = remaining ? ERROR_SUCCESS : GetLastError();
                std::cout << "TRUST CertFindCertificateInStore end present=" << (remaining != nullptr)
                          << " win32=" << findError << std::endl;
                removed = removed && !remaining && findError == static_cast<DWORD>(CRYPT_E_NOT_FOUND);
                if (remaining) CertFreeCertificateContext(remaining);
                const bool absentFromReader = productionRootVisibility(identity, false);
                removed = removed && absentFromReader;
                CertFreeCertificateContext(identity);
            }
        }
        if (store) {
            std::cout << "TRUST CertCloseStore(physical ROOT) begin" << std::endl;
            const bool closed = CertCloseStore(store, 0) != 0; store = nullptr;
            const DWORD closeError = closed ? ERROR_SUCCESS : GetLastError();
            std::cout << "TRUST CertCloseStore end win32=" << closeError << std::endl;
            removed = removed && closed;
        }
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
    enum class Exchange { Echo, PartialClose, PartialHold };
    enum class Stage { Starting, Accept, Handshake, Read, Write };
    struct InnerBytes final : SecureByteStream {
        SSL *ssl;
        explicit InnerBytes(SSL *ssl) : ssl(ssl) {}
        static int result(SSL *ssl, int count) {
            if (count > 0) return count;
            const int error = SSL_get_error(ssl, count);
            return error == SSL_ERROR_WANT_READ ? -2 : error == SSL_ERROR_WANT_WRITE ? -3
                 : error == SSL_ERROR_ZERO_RETURN ? 0 : -1;
        }
        std::ptrdiff_t read(unsigned char *data, std::size_t size) override { return result(ssl, SSL_read(ssl, data, static_cast<int>(size))); }
        std::ptrdiff_t write(const unsigned char *data, std::size_t size) override { return result(ssl, SSL_write(ssl, data, static_cast<int>(size))); }
        bool pending() const override { return SSL_pending(ssl) != 0; }
    };
    template<typename Operation>
    int innerIO(Operation operation) {
        while (active()) {
            const int count = static_cast<int>(operation());
            if (count > 0) return count;
            if (count != -2) { outcome = Outcome::TlsFailure; return -1; }
            std::this_thread::sleep_for(5ms);
        }
        return -1;
    }
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
            std::chrono::milliseconds budget = 5s, std::string password = "portable-password", Exchange exchange = Exchange::Echo) {
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
        worker = std::thread([this, secret, password = std::move(password), exchange] {
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
                    auto carrier = std::make_shared<InnerBytes>(ssl.get());
                    SecureSession inner(static_cast<std::intptr_t>(peer.value), true,
                        std::make_shared<SessionPassword>(password), {}, carrier);
                    if (!inner.handshake(deadline, [this] { return !active(); })) {
                        outcome = Outcome::TlsFailure; finished = true; return;
                    }
                    stage = Stage::Read;
                    std::vector<std::uint8_t> bytes(12 + secret.size());
                    std::size_t offset = 0;
                    while (offset < bytes.size()) {
                        const int count = innerIO([&] { return inner.receive(bytes.data() + offset, bytes.size() - offset); });
                        if (count <= 0) break;
                        offset += count; applicationBytes += count;
                    }
                    validRequest = offset == bytes.size() && std::equal(secret.begin(), secret.end(), bytes.begin() + 12);
                    if (validRequest) {
                        // Return the framed application data through TLS, never through logs.
                        stage = Stage::Write;
                        offset = 0;
                        const auto sendSize = exchange == Exchange::Echo ? bytes.size() : bytes.size() - 1;
                        while (offset < sendSize) {
                            const int count = innerIO([&] { return inner.send(bytes.data() + offset, sendSize - offset); });
                            if (count <= 0) break;
                            offset += count;
                        }
                        if (offset == sendSize) outcome = Outcome::Complete;
                        if (exchange == Exchange::PartialHold)
                            while (active()) std::this_thread::sleep_for(1ms);
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

#ifdef D6R_TEST_OBSERVE_STAGING
static void checkStagingOwnership(X509 *cert, EVP_PKEY *key) {
    // Transport-only authenticated protocol peer. This fixture grants no
    // participant authority and manufactures no End or crypto result.
    Lifecycle::ReconnectGrant grant;
    grant.sessionId = 91; grant.participantId = 2; grant.reservationId = 8;
    check(RAND_bytes(grant.credential.bytes.data(), grant.credential.bytes.size()) == 1, "credential fixture entropy");
    auto credential = Lifecycle::serializeReconnectGrant(grant);
    check(credential.size() == 48, "credential fixture framing");
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        const bool complete = scenario >= 3;
        const char *names[]{"partial EOF", "partial cancellation", "partial key-lifetime timeout", "complete transfer", "queued discard"};
        StagingObservation::reset(complete ? 48 : 47, credential);
        const auto budgetBaseline = Trust::processQueueBudget().used();
        const auto absolute = std::chrono::steady_clock::now() + 5s;
        TlsPeer peer(cert, key, credential, 5s, "portable-password", complete ? TlsPeer::Exchange::Echo
                     : scenario == 0 ? TlsPeer::Exchange::PartialClose : TlsPeer::Exchange::PartialHold);
        SessionTransportDependencies dependencies;
        dependencies.publicTls = true; dependencies.secureSession = true;
        dependencies.password = std::make_shared<SessionPassword>("portable-password");
        if (scenario == 2) dependencies.secureLimits.lifetime = 1s; // Restriction only, not a timer extension.
        dependencies.inbound.beforeApplicationRead = [] { StagingObservation::receivingApplication = true; };
        TcpClient client(dependencies);
        check(client.start({"localhost", peer.port}) && client.waitForConnected(5s), "composed staging connection");
        auto connection = client.connection();
        check(connection->sendSensitive(credential) == SendResult::Accepted, "composed credential fixture queued");
        while (!StagingObservation::staged && std::chrono::steady_clock::now() < absolute) std::this_thread::sleep_for(1ms);
        check(StagingObservation::staged && peer.validRequest, "staging observation must qualify");
        TransportFrame frame;
        if (scenario == 1) { client.cancel(); client.close(); }
        else if (scenario == 3) {
            bool received = false;
            while (!(received = connection->receive(frame)) && std::chrono::steady_clock::now() < absolute) std::this_thread::sleep_for(1ms);
            check(received && frame.payload == credential && !StagingObservation::released,
                  "local guard must not wipe consumer-owned complete APP");
            client.close();
            check(frame.payload == credential && !StagingObservation::released, "close must preserve consumer ownership");
            Trust::secureEraseMemory(frame.payload.data(), frame.payload.size());
            std::vector<std::uint8_t>().swap(frame.payload);
        } else if (scenario == 4) {
            (void) connection->sealAndDrainInput(0); // Ordered boundary; retain queued ownership.
            client.close();
        } else {
            while (!StagingObservation::released && std::chrono::steady_clock::now() < absolute) std::this_thread::sleep_for(1ms);
            check(!connection->receive(frame), "partial authenticated staging is not an APP frame");
            client.close();
        }
        peer.stop();
        check(StagingObservation::released && StagingObservation::erased, "credential staging must be erased before release");
        check(StagingObservation::allocationBytes == 48, "observe the actual production staging allocation");
        check(Trust::processQueueBudget().used() == budgetBaseline, "staging ownership must release queue budget exactly once");
        std::cout << "PASS composed staging erasure/ownership " << names[scenario] << " (bytes withheld)" << std::endl;
    }
    Trust::secureEraseMemory(credential.data(), credential.size());
    Trust::secureEraseMemory(StagingObservation::expected.data(), StagingObservation::expected.size());
}
#endif
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
#ifdef D6R_TEST_OBSERVE_STAGING
        stage = "composed staging erasure and ownership";
        checkStagingOwnership(valid.get(), leafKey.get());
#endif
        const char *cases[]{"trusted hostname", "trusted IP unlocked", "untrusted chain: zero application bytes", "wrong hostname: zero application bytes",
                            "wrong password: zero application bytes", "explicit password on unlocked: no fallback"};
        for (unsigned scenario = 0; scenario < 6; ++scenario) {
            stage = cases[scenario];
            std::cout << "CASE " << stage << " begin" << std::endl;
            const bool trusted = scenario < 2 || scenario >= 4;
            TlsPeer peer(scenario == 2 ? untrusted.get() : scenario == 3 ? wrong.get() : valid.get(), leafKey.get(), secret,
                         5s, scenario == 1 || scenario == 5 ? "" : "portable-password");
            SessionTransportDependencies dependencies; dependencies.publicTls = true; dependencies.secureSession = true;
            dependencies.password = std::make_shared<SessionPassword>(scenario >= 4 ? "incorrect" : scenario == 1 ? "" : "portable-password");
            TcpClient client(dependencies);
            try {
                std::cout << "CONNECT begin" << std::endl;
                check(client.start({scenario == 1 ? "127.0.0.1" : "localhost", peer.port}), "TLS client start");
                const bool connected = client.waitForConnected(10s);
                std::cout << "CONNECT connected=" << connected << " state=" << static_cast<int>(client.state())
                          << " failure=" << static_cast<int>(client.failure()) << std::endl;
                if (scenario >= 4) {
                    check(connected, "outer certificate verification failed before password scenario");
                    auto connection = client.connection();
                    check(waitUntil([&] { return connection->state() == ClientState::Failed; }, 6s)
                          && connection->failure() == TransportFailure::NotAuthorized, "mismatched PAKE did not reject");
                    check(!peer.validRequest && peer.applicationBytes == 0, "application admission preceded password enforcement");
                } else if (trusted) {
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
                check(peer.outcome == (trusted && scenario < 2 ? TlsPeer::Outcome::Complete : TlsPeer::Outcome::TlsFailure), "unexpected TLS peer terminal outcome");
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
