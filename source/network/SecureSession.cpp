#ifdef D6R_TRANSPORT_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "SecureSession.h"
#include "NetworkTrustPolicy.h"
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/version.h>
#include <mbedtls/sha256.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#elif defined(__x86_64__)
#include <cpuid.h>
#endif

#if MBEDTLS_VERSION_NUMBER < 0x03060700 || MBEDTLS_VERSION_NUMBER >= 0x03070000
#error "The session transport requires the maintained Mbed TLS 3.6 LTS configuration, minimum 3.6.7."
#endif
#if !defined(MBEDTLS_ECJPAKE_C) || !defined(MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED) || !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "Mbed TLS must enable EC-JPAKE and TLS 1.2."
#endif
#if defined(MBEDTLS_USE_PSA_CRYPTO) || defined(MBEDTLS_SSL_PROTO_TLS1_3)
#error "The private session TLS profile requires PSA-backed TLS and TLS 1.3 disabled."
#endif
#if !defined(MBEDTLS_AESNI_C) || !defined(MBEDTLS_AES_USE_HARDWARE_ONLY)
#error "The private TLS profile requires AES-NI and hardware-only AES; software table AES is forbidden."
#endif

namespace Duel6::Network {
    namespace {
        using Clock = std::chrono::steady_clock;
        constexpr int CipherSuites[] = {MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8, 0};

        // Framing accounting only: Mbed TLS alone authenticates/decrypts records.
        // Socket operations stop at record boundaries, including partial headers,
        // so one coalesced TCP read cannot evade the per-key record bound.
        struct RecordBudget {
            std::array<unsigned char, 5> header{};
            std::size_t headerBytes = 0, payloadRemaining = 0;
            std::uint64_t records = 0;
            bool invalid = false;
            std::size_t allowance(std::size_t requested, std::uint64_t limit) const {
                if (invalid || (headerBytes == 0 && payloadRemaining == 0 && records >= limit)) return 0;
                return std::min(requested, payloadRemaining ? payloadRemaining : header.size() - headerBytes);
            }
            bool account(const unsigned char *data, std::size_t size) {
                if (payloadRemaining) { payloadRemaining -= size; return true; }
                std::copy(data, data + size, header.begin() + static_cast<std::ptrdiff_t>(headerBytes));
                headerBytes += size;
                if (headerBytes == header.size()) {
                    ++records;
                    payloadRemaining = (static_cast<std::size_t>(header[3]) << 8u) | header[4];
                    headerBytes = 0;
                    invalid = payloadRemaining > 18432; // TLS 1.2 record upper bound.
                }
                return !invalid;
            }
        };
    }

    bool SecureSession::supported(bool hardwarePermitted) {
        if (!hardwarePermitted) return false;
        // This uses no crypto or entropy. AES-NI works with the SSE state that
        // supported x86-64 operating systems already preserve; AVX is not used.
        bool aesni = false;
#if defined(_MSC_VER) && defined(_M_X64)
        int registers[4]{};
        __cpuid(registers, 0);
        if (registers[0] >= 1) { __cpuid(registers, 1); aesni = (registers[2] & (1 << 25)) != 0; }
#elif defined(__x86_64__)
        unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
        aesni = __get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0 && (ecx & bit_AES) != 0;
#endif
        return aesni && mbedtls_ssl_ciphersuite_from_id(CipherSuites[0]) != nullptr;
    }

    SessionPassword::SessionPassword(const std::string &value) {
        if (value.size() > 128 || value.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid session password.");
        std::memcpy(bytes, value.data(), value.size());
    }
    SessionPassword::~SessionPassword() { Trust::secureEraseMemory(bytes, sizeof(bytes)); }

    class SecureSession::Impl {
    public:
        mbedtls_ssl_context session;
        mbedtls_ssl_config config;
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context random;
        const std::intptr_t socket;
        mutable std::mutex mutex;
        RecordBudget incoming, outgoing;
        SecureSessionLimits limits;
        Clock::time_point deadline;
        std::uint64_t received = 0, sent = 0;
        bool contextsInitialized = false, initialized = false, established = false, failed = false;

        Impl(std::intptr_t socket, bool host, const std::shared_ptr<const SessionPassword> &password,
             SecureSessionLimits requested) : socket(socket) {
            // In particular, CTR-DRBG seeding uses AES. Nothing in the library
            // may execute before this gate, including entropy initialization.
            if (!SecureSession::supported(requested.hardwarePermitted)) { failed = true; return; }
            failed = true;
            const SecureSessionLimits maximum;
            limits.entropyPermitted = requested.entropyPermitted;
            limits.recordsPerDirection = std::min(requested.recordsPerDirection, maximum.recordsPerDirection);
            limits.plaintextBytesPerDirection = std::min(requested.plaintextBytesPerDirection, maximum.plaintextBytesPerDirection);
            limits.lifetime = std::max(std::chrono::milliseconds(0), std::min(requested.lifetime, maximum.lifetime));
            deadline = Clock::now() + limits.lifetime;
            mbedtls_ssl_init(&session); mbedtls_ssl_config_init(&config);
            mbedtls_entropy_init(&entropy); mbedtls_ctr_drbg_init(&random);
            contextsInitialized = true;
            static const unsigned char purpose[] = "duel6r-session-ecjpake-v1";
            if (mbedtls_ctr_drbg_seed(&random, collectEntropy, this, purpose, sizeof(purpose) - 1) != 0
                || mbedtls_ssl_config_defaults(&config, host ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) return;
            mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
            mbedtls_ssl_conf_min_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
            mbedtls_ssl_conf_max_tls_version(&config, MBEDTLS_SSL_VERSION_TLS1_2);
            mbedtls_ssl_conf_ciphersuites(&config, CipherSuites);
#if defined(MBEDTLS_SSL_RENEGOTIATION)
            mbedtls_ssl_conf_renegotiation(&config, MBEDTLS_SSL_RENEGOTIATION_DISABLED);
#endif
#if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
            if (!host) mbedtls_ssl_conf_session_tickets(&config, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
#endif
            // No session-cache/ticket callbacks are installed. The only suite is
            // PAKE-authenticated and uses no certificate or DNS-name identity.
            if (mbedtls_ssl_setup(&session, &config) != 0) return;
#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(MBEDTLS_SSL_SERVER_NAME_INDICATION)
            if (!host && mbedtls_ssl_set_hostname(&session, nullptr) != 0) return;
#endif
            char secret[160]{};
            const bool locked = password && password->required();
            std::strcpy(secret, locked ? "duel6r-password:" : "duel6r-unlocked-public-value");
            if (locked) std::strcat(secret, password->value());
            unsigned char mappedSecret[32]{};
            const int hashed = mbedtls_sha256(reinterpret_cast<const unsigned char *>(secret), std::strlen(secret), mappedSecret, 0);
            Trust::secureEraseMemory(secret, sizeof(secret));
            const int configured = hashed == 0 ? mbedtls_ssl_set_hs_ecjpake_password(&session, mappedSecret, sizeof(mappedSecret)) : hashed;
            Trust::secureEraseMemory(mappedSecret, sizeof(mappedSecret));
            if (configured != 0) return;
            mbedtls_ssl_set_bio(&session, this, push, pull, nullptr);
            initialized = true;
            failed = false;
        }
        ~Impl() {
            if (!contextsInitialized) return;
            mbedtls_ssl_free(&session); mbedtls_ssl_config_free(&config);
            mbedtls_ctr_drbg_free(&random); mbedtls_entropy_free(&entropy);
        }
        bool exhausted() const {
            return failed || Clock::now() >= deadline
                || received >= limits.plaintextBytesPerDirection || sent >= limits.plaintextBytesPerDirection
                || incoming.allowance(1, limits.recordsPerDirection) == 0
                || outgoing.allowance(1, limits.recordsPerDirection) == 0;
        }
        static int collectEntropy(void *context, unsigned char *output, std::size_t size) {
            auto &self = *static_cast<Impl *>(context);
            if (!self.limits.entropyPermitted) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            return mbedtls_entropy_func(&self.entropy, output, size);
        }
        static int pull(void *context, unsigned char *data, std::size_t size) {
            auto &self = *static_cast<Impl *>(context);
            if (self.failed || Clock::now() >= self.deadline) return MBEDTLS_ERR_NET_RECV_FAILED;
            size = self.incoming.allowance(size, self.limits.recordsPerDirection);
            if (size == 0) return MBEDTLS_ERR_NET_RECV_FAILED;
#ifdef D6R_TRANSPORT_WINDOWS
            const int count = ::recv(static_cast<SOCKET>(self.socket), reinterpret_cast<char *>(data), static_cast<int>(size), 0);
            if (count == SOCKET_ERROR) {
                const auto error = WSAGetLastError();
                return error == WSAEWOULDBLOCK || error == WSAEINTR ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
            }
#else
            const auto count = ::recv(static_cast<int>(self.socket), data, size, 0);
            if (count < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR
                ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
#endif
            if (count > 0 && !self.incoming.account(data, static_cast<std::size_t>(count))) return MBEDTLS_ERR_NET_RECV_FAILED;
            return static_cast<int>(count);
        }
        static int push(void *context, const unsigned char *data, std::size_t size) {
            auto &self = *static_cast<Impl *>(context);
            if (self.failed || Clock::now() >= self.deadline) return MBEDTLS_ERR_NET_SEND_FAILED;
            size = self.outgoing.allowance(size, self.limits.recordsPerDirection);
            if (size == 0) return MBEDTLS_ERR_NET_SEND_FAILED;
#ifdef D6R_TRANSPORT_WINDOWS
            const int count = ::send(static_cast<SOCKET>(self.socket), reinterpret_cast<const char *>(data), static_cast<int>(size), 0);
            if (count == SOCKET_ERROR) {
                const auto error = WSAGetLastError();
                return error == WSAEWOULDBLOCK || error == WSAEINTR ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
            }
#else
            const auto count = ::send(static_cast<int>(self.socket), data, size, MSG_NOSIGNAL);
            if (count < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR
                ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
#endif
            if (count > 0 && !self.outgoing.account(data, static_cast<std::size_t>(count))) return MBEDTLS_ERR_NET_SEND_FAILED;
            return count == 0 ? MBEDTLS_ERR_SSL_WANT_WRITE : static_cast<int>(count);
        }
        std::ptrdiff_t result(int value) {
            if (value >= 0) return value;
            if (value == MBEDTLS_ERR_SSL_WANT_READ || value == MBEDTLS_ERR_SSL_WANT_WRITE) return -2;
            failed = true; // Never process a second forgery attempt with these keys.
            return value == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ? 0 : -1;
        }
    };

    SecureSession::SecureSession(std::intptr_t socket, bool server, std::shared_ptr<const SessionPassword> password,
                                 SecureSessionLimits limits) : impl(std::make_unique<Impl>(socket, server, password, limits)) {}
    SecureSession::~SecureSession() = default;
    bool SecureSession::handshake(Clock::time_point deadline, const std::function<bool()> &cancelled) {
        if (!impl->initialized) return false;
        while (!cancelled() && Clock::now() < deadline && !impl->exhausted()) {
            const int result = mbedtls_ssl_handshake(&impl->session);
            if (result == 0) {
                const char *suite = mbedtls_ssl_get_ciphersuite(&impl->session);
                if (!suite || mbedtls_ssl_get_ciphersuite_id(suite) != CipherSuites[0]) {
                    impl->failed = true; return false;
                }
                impl->established = !cancelled() && Clock::now() < deadline;
                return impl->established;
            }
            if (result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE) {
                impl->failed = true; return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        impl->failed = true; return false;
    }
    std::ptrdiff_t SecureSession::receive(void *data, std::size_t size) {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->established || impl->exhausted()) return -1;
        size = std::min<std::size_t>(size, impl->limits.plaintextBytesPerDirection - impl->received);
        const auto count = impl->result(mbedtls_ssl_read(&impl->session, static_cast<unsigned char *>(data), size));
        if (count > 0) impl->received += static_cast<std::uint64_t>(count);
        return count;
    }
    std::ptrdiff_t SecureSession::send(const void *data, std::size_t size) {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (!impl->established || impl->exhausted()) return -1;
        size = std::min<std::size_t>(size, impl->limits.plaintextBytesPerDirection - impl->sent);
        const auto count = impl->result(mbedtls_ssl_write(&impl->session, static_cast<const unsigned char *>(data), size));
        if (count > 0) impl->sent += static_cast<std::uint64_t>(count);
        return count;
    }
    bool SecureSession::pending() const {
        std::lock_guard<std::mutex> lock(impl->mutex);
        return impl->established && mbedtls_ssl_check_pending(&impl->session) != 0;
    }
    bool SecureSession::expired() const {
        std::lock_guard<std::mutex> lock(impl->mutex);
        return impl->exhausted();
    }
}
