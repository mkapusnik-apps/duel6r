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
#include <csignal>
using Socket = int;
constexpr Socket BadSocket = -1;
static void closeSocket(Socket s) { if (s != BadSocket) ::close(s); }
#endif
#include <openssl/pem.h>
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
        return removed;
#else
        if (hadFile) setenv("SSL_CERT_FILE", oldFile.c_str(), 1); else unsetenv("SSL_CERT_FILE");
        if (hadDir) setenv("SSL_CERT_DIR", oldDir.c_str(), 1); else unsetenv("SSL_CERT_DIR");
        std::error_code error; std::filesystem::remove_all(root, error);
        return !error;
#endif
    }
    ~TrustFixture() { (void) remove(); }
};

class TlsPeer {
    Socket listener = BadSocket;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ctx{SSL_CTX_new(TLS_server_method()), SSL_CTX_free};
    std::thread worker;
    std::atomic<bool> stopping{false};
public:
    std::uint16_t port = 0;
    std::atomic<unsigned> applicationBytes{0};
    std::atomic<bool> finished{false}, validRequest{false};
    TlsPeer(X509 *cert, EVP_PKEY *key, const std::vector<std::uint8_t> &secret) {
        check(ctx && SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION) == 1, "TLS peer context");
        check(SSL_CTX_use_certificate(ctx.get(), cert) == 1 && SSL_CTX_use_PrivateKey(ctx.get(), key) == 1, "TLS peer identity");
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); check(listener != BadSocket, "TLS listener socket");
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 && listen(listener, 1) == 0, "TLS bind/listen");
#ifdef D6R_TRANSPORT_WINDOWS
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        check(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0, "TLS listener port");
        port = ntohs(address.sin_port);
        worker = std::thread([this, secret] {
            while (!stopping) {
                fd_set readable; FD_ZERO(&readable); FD_SET(listener, &readable);
                timeval wait{0, 100000};
                if (select(static_cast<int>(listener) + 1, &readable, nullptr, nullptr, &wait) > 0) break;
            }
            if (stopping) { finished = true; return; }
            Socket peer = accept(listener, nullptr, nullptr);
            if (peer != BadSocket) {
#ifdef D6R_TRANSPORT_WINDOWS
                DWORD timeout = 5000;
#else
                timeval timeout{5, 0};
#endif
                setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
                setsockopt(peer, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
                std::unique_ptr<SSL, decltype(&SSL_free)> ssl(SSL_new(ctx.get()), SSL_free);
                if (ssl && SSL_set_fd(ssl.get(), static_cast<int>(peer)) == 1 && SSL_accept(ssl.get()) == 1) {
                    std::vector<std::uint8_t> bytes(12 + secret.size());
                    std::size_t offset = 0;
                    while (offset < bytes.size()) {
                        const int count = SSL_read(ssl.get(), bytes.data() + offset, static_cast<int>(bytes.size() - offset));
                        if (count <= 0) break;
                        offset += count; applicationBytes += count;
                    }
                    validRequest = offset == bytes.size() && std::equal(secret.begin(), secret.end(), bytes.begin() + 12);
                    if (validRequest) {
                        // Return the framed application data through TLS, never through logs.
                        offset = 0;
                        while (offset < bytes.size()) {
                            const int count = SSL_write(ssl.get(), bytes.data() + offset, static_cast<int>(bytes.size() - offset));
                            if (count <= 0) break;
                            offset += count;
                        }
                    }
                }
                closeSocket(peer);
            }
            finished = true;
        });
    }
    ~TlsPeer() { stopping = true; if (worker.joinable()) worker.join(); closeSocket(listener); }
};

int main(int argc, char **argv) {
    try {
#ifdef D6R_TRANSPORT_WINDOWS
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
        auto caKey = key(), otherKey = key(), leafKey = key();
        auto ca = certificate(caKey.get(), nullptr, nullptr, true);
        auto otherCa = certificate(otherKey.get(), nullptr, nullptr, true);
        auto valid = certificate(leafKey.get(), ca.get(), caKey.get(), false);
        auto wrong = certificate(leafKey.get(), ca.get(), caKey.get(), false, true);
        auto untrusted = certificate(leafKey.get(), otherCa.get(), otherKey.get(), false);
        TrustFixture trust(ca.get());
        std::vector<std::uint8_t> secret(48); check(RAND_bytes(secret.data(), secret.size()) == 1, "secret fixture generation");
        for (unsigned scenario = 0; scenario < 4; ++scenario) {
            const bool trusted = scenario < 2;
            TlsPeer peer(scenario == 2 ? untrusted.get() : scenario == 3 ? wrong.get() : valid.get(), leafKey.get(), secret);
            SessionTransportDependencies dependencies; dependencies.publicTls = true;
            TcpClient client(dependencies);
            check(client.start({scenario == 1 ? "127.0.0.1" : "localhost", peer.port}), "TLS client start");
            const bool connected = client.waitForConnected(10s);
            if (trusted) {
                check(connected, "trusted native TLS failed");
                check(client.connection()->sendSensitive(secret) == SendResult::Accepted, "sensitive TLS frame not queued");
                TransportFrame frame; bool received = false;
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!(received = client.connection()->receive(frame)) && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
                check(received && frame.payload == secret && peer.validRequest, "verified TLS application round trip failed");
            } else {
                check(!connected && client.failure() == TransportFailure::SecureConnectionFailed, "invalid certificate was not a security failure");
            }
            client.close();
            const auto deadline = std::chrono::steady_clock::now() + 6s;
            while (!peer.finished && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
            check(peer.finished, "TLS peer did not finish");
            if (!trusted) check(peer.applicationBytes == 0, "application secret disclosed before certificate validation");
            std::cout << "PASS native TLS " << (scenario == 0 ? "trusted hostname" : scenario == 1 ? "trusted IP" : scenario == 2 ? "untrusted chain: zero application bytes" : "wrong hostname: zero application bytes") << '\n';
        }
        check(trust.remove(), "temporary test trust cleanup failed");
        std::cout << "PASS native test trust removed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL native TLS: " << error.what() << '\n';
        return 1;
    }
}
