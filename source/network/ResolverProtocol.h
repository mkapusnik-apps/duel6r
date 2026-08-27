#ifndef DUEL6_NETWORK_RESOLVERPROTOCOL_H
#define DUEL6_NETWORK_RESOLVERPROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Duel6::Network::ResolverProtocol {
    constexpr std::uint32_t RequestMagic = 0x44365251; // D6RQ
    constexpr std::uint32_t ResponseMagic = 0x44365253; // D6RS
    constexpr std::size_t HeaderBytes = 12;
    constexpr std::size_t MaxAddresses = 64;
    constexpr std::size_t MaxHostBytes = 4096;

    inline bool validHost(std::string_view host) {
        if (host.empty() || host.size() > MaxHostBytes) return false;
        for (unsigned char character: host) {
            bool valid = (character >= 'a' && character <= 'z')
                         || (character >= 'A' && character <= 'Z')
                         || (character >= '0' && character <= '9')
                         || character == '.' || character == '-';
            if (!valid) return false;
        }
        return true;
    }

    inline bool validService(std::string_view service) {
        if (service.empty() || service.size() > 5) return false;
        std::uint32_t value = 0;
        for (unsigned char character: service) {
            if (character < '0' || character > '9') return false;
            value = value * 10u + static_cast<std::uint32_t>(character - '0');
        }
        return value > 0 && value <= 65535;
    }

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
