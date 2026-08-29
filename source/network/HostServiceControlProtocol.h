#ifndef DUEL6_NETWORK_HOSTSERVICECONTROLPROTOCOL_H
#define DUEL6_NETWORK_HOSTSERVICECONTROLPROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace Duel6::Network {
    constexpr std::uint32_t HostServiceControlMagic = 0x44364853u; // D6HS
    constexpr std::uint8_t HostServiceControlVersion = 1;
    constexpr std::size_t HostServiceControlMessageBytes = 8;
    constexpr std::size_t HostServiceStatusMessageBytes = 16;

    enum class HostServiceStatusCode : std::uint8_t {
        HostManifestInvalid = 1,
        PortUnavailable = 2,
        StartFailed = 3,
        Ready = 4
    };

    enum class HostServiceCommandCode : std::uint8_t {
        Stop = 1
    };

    std::array<std::uint8_t, HostServiceStatusMessageBytes> encodeHostServiceStatus(
            HostServiceStatusCode status, std::uint64_t monotonicNanoseconds);
    std::array<std::uint8_t, HostServiceControlMessageBytes> encodeHostServiceCommand(
            HostServiceCommandCode command);
    bool decodeHostServiceStatus(const std::uint8_t *message, std::size_t size,
                                 HostServiceStatusCode &status,
                                 std::uint64_t &monotonicNanoseconds) noexcept;
    bool decodeHostServiceCommand(const std::uint8_t *message, std::size_t size,
                                  HostServiceCommandCode &command) noexcept;
}

#endif
