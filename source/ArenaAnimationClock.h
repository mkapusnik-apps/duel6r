#ifndef DUEL6_ARENAANIMATIONCLOCK_H
#define DUEL6_ARENAANIMATIONCLOCK_H
#include <cmath>
#include "Defines.h"
#include "ArenaState.h"

namespace Duel6 {
    // Cosmetic reference clock, not a second simulation. The original local
    // animator tests > .328 and drops the remainder at 90 Hz: exactly 30 ticks.
    // Regeneration resets water frames only, preserving the cycle phase.
    class ArenaAnimationClock final {
    public:
        static constexpr std::uint64_t TicksPerFrame =
                static_cast<std::uint64_t>(D6_ANM_SPEED * D6_UPDATE_FREQUENCY) + 1;
        void update(double seconds) {
            if (std::isfinite(seconds) && seconds > 0) referenceTicks += seconds * D6_UPDATE_FREQUENCY;
        }
        ArenaAnimationState state() const {
            return {static_cast<std::uint64_t>(std::floor(referenceTicks + 0.00001)), waterResetStep};
        }
        void resetWater() { waterResetStep = state().ticks / TicksPerFrame; }
        static std::uint64_t frame(const ArenaAnimationState &state) { return state.ticks / TicksPerFrame; }
        static std::uint64_t waterFrame(const ArenaAnimationState &state) { return frame(state) - state.waterResetStep; }
    private:
        double referenceTicks = 0;
        std::uint64_t waterResetStep = 0;
    };
}
#endif
