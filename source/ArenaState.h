#ifndef DUEL6_ARENASTATE_H
#define DUEL6_ARENASTATE_H
#include <cstdint>
#include <array>
namespace Duel6 {
    // Gameplay meaning, not a texture, block index or profile identity.
    enum class LiquidKind : std::uint8_t { None, Blue, Red, Green };
    struct HudIndicatorState {
        std::int64_t elapsed = 0, total = 0; // Q16 seconds, not wall time.
        bool operator==(const HudIndicatorState &other) const {
            return elapsed == other.elapsed && total == other.total;
        }
    };
    // Stable semantic order: reload, air, bonus, health, name, ammunition.
    using HudIndicators = std::array<HudIndicatorState, 6>;
    struct ArenaAnimationState {
        std::uint64_t ticks = 0, waterResetStep = 0;
        bool operator==(const ArenaAnimationState &other) const {
            return ticks == other.ticks && waterResetStep == other.waterResetStep;
        }
    };
}
#endif
