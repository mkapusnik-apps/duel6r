#ifndef DUEL6_TESTS_APPEARANCEFIXTURES_H
#define DUEL6_TESTS_APPEARANCEFIXTURES_H
#include "source/PlayerAppearance.h"

namespace Duel6::Test {
    inline PlayerAppearance appearance(unsigned variant = 0) {
        PlayerAppearance result;
        for (unsigned part = 0; part < result.colors.size(); ++part)
            result.colors[part] = {static_cast<std::uint8_t>(20 + part * 19 + variant),
                                   static_cast<std::uint8_t>(230 - part * 17),
                                   static_cast<std::uint8_t>(40 + part * 13)};
        result.hair = static_cast<std::uint8_t>(variant % 3);
        result.headBand = variant % 2 != 0;
        return result;
    }
}
#endif
