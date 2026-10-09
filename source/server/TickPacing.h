#ifndef DUEL6_TICK_PACING_H
#define DUEL6_TICK_PACING_H

#include "AuthoritativeMatchTypes.h"
#include "../network/NetworkTrustPolicy.h"
#include <chrono>
#include <array>

namespace Duel6::Server {
    // The match still advances exactly one fixed tick per ingress/lifecycle
    // pass. An overdue pass may have one immediate successor, then must yield.
    // Debt is retained, never discarded or converted into a variable time step.
    class TickPacing {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr unsigned MaximumTicksPerSecond = Authoritative::FixedTickRate * 3 / 2;
        static constexpr auto OrdinaryWait = std::chrono::milliseconds(5);
        static_assert(MaximumTicksPerSecond < Network::Trust::InputsPerOwnedSlotPerSecond);
        static_assert(MaximumTicksPerSecond * Authoritative::MaxPlayers < Network::Trust::GlobalAcceptedInputsPerSecond);

        bool canAdvance(Clock::time_point now) const {
            if (ticks && now < lastAdvance) return false;
            return ticks < MaximumTicksPerSecond || now - history[oldest] >= std::chrono::seconds(1);
        }
        void advanced(Clock::time_point now) {
            history[oldest] = now;
            oldest = (oldest + 1) % MaximumTicksPerSecond;
            if (ticks < MaximumTicksPerSecond) ++ticks;
            lastAdvance = now;
        }
        std::chrono::milliseconds nextWait(Clock::time_point now, Clock::time_point due, bool active) {
            if (active && now >= due && canAdvance(now) && !immediateUsed) {
                immediateUsed = true;
                return std::chrono::milliseconds::zero();
            }
            immediateUsed = false;
            return OrdinaryWait;
        }
    private:
        // A rolling window, not epoch buckets: bursts straddling a second
        // boundary must not double the allowed work or peer input rate.
        std::array<Clock::time_point, MaximumTicksPerSecond> history{};
        Clock::time_point lastAdvance{};
        unsigned ticks = 0, oldest = 0;
        bool immediateUsed = false;
    };
}
#endif
