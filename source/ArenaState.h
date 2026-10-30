#ifndef DUEL6_ARENASTATE_H
#define DUEL6_ARENASTATE_H
#include <cstdint>
namespace Duel6 {
    // Gameplay meaning, not a texture, block index or profile identity.
    enum class LiquidKind : std::uint8_t { None, Blue, Red, Green };
}
#endif
