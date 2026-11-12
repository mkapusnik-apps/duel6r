#ifndef DUEL6_PLAYERAPPEARANCE_H
#define DUEL6_PLAYERAPPEARANCE_H

#include <array>
#include <cstdint>

namespace Duel6 {
    // Body-part order matches PlayerSkinColors through HeadBand. Outline color
    // is derived locally; opacity, resources and animation are never proposals.
    struct PlayerAppearance {
        std::array<std::array<std::uint8_t, 3>, 10> colors{};
        std::uint8_t hair = 0;
        bool headBand = false;

        bool valid() const noexcept { return hair <= 2; }
        bool operator==(const PlayerAppearance &other) const noexcept {
            return colors == other.colors && hair == other.hair && headBand == other.headBand;
        }
        bool operator!=(const PlayerAppearance &other) const noexcept { return !(*this == other); }
    };
}
#endif
