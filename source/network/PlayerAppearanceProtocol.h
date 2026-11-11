#ifndef DUEL6_NETWORK_PLAYERAPPEARANCEPROTOCOL_H
#define DUEL6_NETWORK_PLAYERAPPEARANCEPROTOCOL_H

#include "../PlayerAppearance.h"
#include <stdexcept>

namespace Duel6::Network {
    template<typename WriteByte>
    void writeAppearance(const PlayerAppearance &appearance, WriteByte write) {
        if (!appearance.valid()) throw std::invalid_argument("Invalid player appearance");
        for (const auto &color: appearance.colors) for (const auto channel: color) write(channel);
        write(appearance.hair);
        write(static_cast<std::uint8_t>(appearance.headBand));
    }

    template<typename ReadByte>
    PlayerAppearance readAppearance(ReadByte read) {
        PlayerAppearance appearance;
        for (auto &color: appearance.colors) for (auto &channel: color) channel = read();
        appearance.hair = read();
        const auto headBand = read();
        if (!appearance.valid() || headBand > 1) throw std::invalid_argument("Invalid player appearance");
        appearance.headBand = headBand != 0;
        return appearance;
    }
}
#endif
