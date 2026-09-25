#ifndef DUEL6_NETWORK_SECURESESSION_H
#define DUEL6_NETWORK_SECURESESSION_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Duel6::Network {
    inline constexpr const char *SecureNetworkingUnavailableCopy =
        "Secure networking is unavailable. An x86-64 CPU with AES-NI is required.";
    struct SecureSessionLimits {
        std::uint64_t recordsPerDirection = UINT64_C(1) << 20;
        std::uint64_t plaintextBytesPerDirection = UINT64_C(1) << 30;
        std::chrono::milliseconds lifetime = std::chrono::minutes(30);
        // Restriction-only platform policy seam. True never bypasses CPU detection.
        bool hardwarePermitted = true;
        bool entropyPermitted = true; // Can force failure, never supply replacement entropy.
    };
    // Fixed-size, erased storage. Shared ownership avoids incidental string copies
    // while UI, supervisor and a reconnect attempt use the same session secret.
    class SessionPassword final {
    public:
        explicit SessionPassword(const std::string &value);
        ~SessionPassword();
        SessionPassword(const SessionPassword &) = delete;
        SessionPassword &operator=(const SessionPassword &) = delete;
        const char *value() const noexcept { return bytes; }
        bool required() const noexcept { return bytes[0] != 0; }
    private:
        char bytes[129]{};
    };

    class SecureSession final {
    public:
        static bool supported(bool hardwarePermitted = true);
        SecureSession(std::intptr_t socket, bool server, std::shared_ptr<const SessionPassword> password,
                      SecureSessionLimits limits = {});
        ~SecureSession();
        SecureSession(const SecureSession &) = delete;
        SecureSession &operator=(const SecureSession &) = delete;
        bool handshake(std::chrono::steady_clock::time_point deadline, const std::function<bool()> &cancelled);
        // Positive byte count, zero EOF, -1 failure, -2 retry. No library diagnostic
        // or peer-controlled data crosses this boundary.
        std::ptrdiff_t receive(void *buffer, std::size_t size);
        std::ptrdiff_t send(const void *buffer, std::size_t size);
        bool pending() const;
        bool expired() const;
    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };
}
#endif
