#ifndef DUEL6_PRESENTATIONCLOCK_H
#define DUEL6_PRESENTATIONCLOCK_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Duel6 {
    // The only extrapolated quantity is presentation time, never gameplay.
    // One publication interval bridges ordinary 20Hz snapshots. Retention and
    // frozen phases stop at accepted time; recovery restores rather than replays.
    class PresentationClock final {
    public:
        void reset() { accepted = -1; seconds = 0; ahead = 0; }
        float update(std::int64_t fixedSeconds, float elapsed, bool advancing) {
            const float canonical = static_cast<float>(fixedSeconds) / 65536.0f;
            if (fixedSeconds != accepted) {
                ahead = advancing && accepted >= 0 && fixedSeconds >= accepted
                        ? std::clamp(seconds - canonical, 0.0f, 0.05f) : 0.0f;
                seconds = canonical + ahead;
                accepted = fixedSeconds;
            }
            if (!advancing) { seconds = canonical; ahead = 0; return seconds; }
            const float delta = std::isfinite(elapsed) ? std::max(0.0f, elapsed) : 0.0f;
            ahead = std::min(0.05f, ahead + delta);
            seconds = canonical + ahead;
            return seconds;
        }
        float value() const { return seconds; }
    private:
        std::int64_t accepted = -1;
        float seconds = 0;
        float ahead = 0;
    };
}
#endif
