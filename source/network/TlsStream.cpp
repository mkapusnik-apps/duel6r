#include "TlsStream.h"
#include <thread>
#include <limits>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#ifdef D6R_TRANSPORT_WINDOWS
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#else
#include <cerrno>
#include <sys/socket.h>
#endif

namespace Duel6::Network {
    namespace {
        // Socket BIO without SIGPIPE; transport retains ownership of the native socket.
        BIO_METHOD *socketMethod() {
            static BIO_METHOD *method = [] {
                auto *m = BIO_meth_new(BIO_TYPE_SOURCE_SINK | BIO_get_new_index(), "duel6r socket");
                BIO_meth_set_create(m, [](BIO *b) { BIO_set_init(b, 1); return 1; });
                BIO_meth_set_destroy(m, [](BIO *) { return 1; });
                BIO_meth_set_ctrl(m, [](BIO *, int command, long, void *) -> long {
                    return command == BIO_CTRL_FLUSH ? 1 : 0;
                });
                BIO_meth_set_read(m, [](BIO *b, char *data, int size) {
                    BIO_clear_retry_flags(b);
                    const auto fd = reinterpret_cast<std::intptr_t>(BIO_get_data(b));
#ifdef D6R_TRANSPORT_WINDOWS
                    const int count = recv(static_cast<SOCKET>(fd), data, size, 0);
                    if (count < 0 && WSAGetLastError() == WSAEWOULDBLOCK) BIO_set_retry_read(b);
#else
                    const int count = static_cast<int>(recv(static_cast<int>(fd), data, size, 0));
                    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) BIO_set_retry_read(b);
#endif
                    return count;
                });
                BIO_meth_set_write(m, [](BIO *b, const char *data, int size) {
                    BIO_clear_retry_flags(b);
                    const auto fd = reinterpret_cast<std::intptr_t>(BIO_get_data(b));
#ifdef D6R_TRANSPORT_WINDOWS
                    const int count = send(static_cast<SOCKET>(fd), data, size, 0);
                    if (count < 0 && WSAGetLastError() == WSAEWOULDBLOCK) BIO_set_retry_write(b);
#else
                    const int count = static_cast<int>(send(static_cast<int>(fd), data, size, MSG_NOSIGNAL));
                    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) BIO_set_retry_write(b);
#endif
                    return count;
                });
                return m;
            }();
            return method;
        }
        bool systemTrust(SSL_CTX *context) {
#ifdef D6R_TRANSPORT_WINDOWS
            HCERTSTORE roots = CertOpenSystemStoreA(0, "ROOT");
            if (!roots) return false;
            PCCERT_CONTEXT cert = nullptr;
            bool loaded = false;
            while ((cert = CertEnumCertificatesInStore(roots, cert)) != nullptr) {
                const unsigned char *data = cert->pbCertEncoded;
                X509 *value = d2i_X509(nullptr, &data, cert->cbCertEncoded);
                if (value) {
                    if (X509_STORE_add_cert(SSL_CTX_get_cert_store(context), value) == 1) loaded = true;
                    X509_free(value);
                }
            }
            CertCloseStore(roots, 0); ERR_clear_error();
            return loaded;
#else
            return SSL_CTX_set_default_verify_paths(context) == 1;
#endif
        }
    }
    TlsStream::~TlsStream() { SSL_free(stream); SSL_CTX_free(context); }
    std::shared_ptr<TlsStream> TlsStream::connect(std::intptr_t socket, const std::string &identity,
            std::chrono::steady_clock::time_point deadline, const std::function<bool()> &cancelled) {
        auto result = std::shared_ptr<TlsStream>(new TlsStream);
        result->context = SSL_CTX_new(TLS_client_method());
        if (!result->context || SSL_CTX_set_min_proto_version(result->context, TLS1_2_VERSION) != 1
            || !systemTrust(result->context)) return {};
        SSL_CTX_set_verify(result->context, SSL_VERIFY_PEER, nullptr);
        result->stream = SSL_new(result->context);
        if (!result->stream) return {};
        auto *parameters = SSL_get0_param(result->stream);
        X509_VERIFY_PARAM_set_hostflags(parameters, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        const bool ip = X509_VERIFY_PARAM_set1_ip_asc(parameters, identity.c_str()) == 1;
        if (!ip && (SSL_set1_host(result->stream, identity.c_str()) != 1
                    || SSL_set_tlsext_host_name(result->stream, identity.c_str()) != 1)) return {};
        auto *bio = BIO_new(socketMethod());
        if (!bio) return {};
        BIO_set_data(bio, reinterpret_cast<void *>(socket));
        SSL_set_bio(result->stream, bio, bio);
        SSL_set_mode(result->stream, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        while (!cancelled() && std::chrono::steady_clock::now() < deadline) {
            ERR_clear_error();
            const int count = SSL_connect(result->stream);
            if (count == 1) return SSL_get_verify_result(result->stream) == X509_V_OK ? result : nullptr;
            const int error = SSL_get_error(result->stream, count);
            if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) return {};
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return {};
    }
    int TlsStream::outcome(int count) {
        if (count > 0) return count;
        const int error = SSL_get_error(stream, count);
        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) return -2;
        return error == SSL_ERROR_ZERO_RETURN ? 0 : -1;
    }
    int TlsStream::read(std::uint8_t *data, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex); ERR_clear_error();
        return outcome(SSL_read(stream, data, static_cast<int>(size)));
    }
    int TlsStream::write(const std::uint8_t *data, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex); ERR_clear_error();
        return outcome(SSL_write(stream, data, static_cast<int>(size)));
    }
}
