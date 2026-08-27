#ifndef DUEL6_NETWORK_RESOLVERPROTOCOL_H
#define DUEL6_NETWORK_RESOLVERPROTOCOL_H

#include <cstddef>
#include <cstdint>

namespace Duel6::Network::ResolverProtocol {
    constexpr std::uint32_t RequestMagic = 0x44365251; // D6RQ
    constexpr std::uint32_t ResponseMagic = 0x44365253; // D6RS
    constexpr std::size_t HeaderBytes = 12;
    constexpr std::size_t MaxAddresses = 64;

    inline void writeU32(std::uint8_t *target, std::uint32_t value) {
        target[0] = static_cast<std::uint8_t>(value >> 24u);
        target[1] = static_cast<std::uint8_t>(value >> 16u);
        target[2] = static_cast<std::uint8_t>(value >> 8u);
        target[3] = static_cast<std::uint8_t>(value);
    }

    inline std::uint32_t readU32(const std::uint8_t *source) {
        return (static_cast<std::uint32_t>(source[0]) << 24u)
               | (static_cast<std::uint32_t>(source[1]) << 16u)
               | (static_cast<std::uint32_t>(source[2]) << 8u)
               | source[3];
    }
}

#endif
