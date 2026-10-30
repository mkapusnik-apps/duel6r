#ifndef DUEL6_ARENASTATE_H
#define DUEL6_ARENASTATE_H
#include <cstdint>
#include <array>
namespace Duel6 {
    // Gameplay meaning, not a texture, block index or profile identity.
    enum class LiquidKind : std::uint8_t { None, Blue, Red, Green };
    // Semantic animation state, never a profile or animation-resource identity.
    enum class PlayerPose : std::uint8_t {
        Stand, HitStand, Walk, Fall, Jump, Duck, HitDuck, DeadFall, DeadHit, DeadLying, Dying, Pick
    };
    struct PlayerAnimationState {
        PlayerPose pose = PlayerPose::Stand;
        std::uint16_t bodyFrame = 0, weaponFrame = 12; // pair offsets within the semantic clip
        bool weaponVisible = false;
        bool operator==(const PlayerAnimationState &other) const {
            return pose == other.pose && bodyFrame == other.bodyFrame
                && weaponFrame == other.weaponFrame && weaponVisible == other.weaponVisible;
        }
    };
    inline bool validPlayerAnimation(const PlayerAnimationState &state) {
        constexpr std::uint16_t frames[] = {11, 1, 4, 3, 3, 9, 1, 4, 10, 1, 16, 11};
        const auto pose = static_cast<std::uint8_t>(state.pose);
        return pose < sizeof(frames) / sizeof(frames[0])
            && state.bodyFrame % 2 == 0 && state.bodyFrame / 2 < frames[pose]
            && state.weaponFrame % 2 == 0 && state.weaponFrame <= 12;
    }
    struct HudIndicatorState {
        std::int64_t elapsed = 0, total = 0; // Q16 seconds, not wall time.
        bool operator==(const HudIndicatorState &other) const {
            return elapsed == other.elapsed && total == other.total;
        }
    };
    // Stable semantic order: reload, air, bonus, health, name, ammunition.
    using HudIndicators = std::array<HudIndicatorState, 6>;
    struct EffectVisualState {
        // Q16 x, y, z, width, height, rotation, rotation-centre x/y.
        std::array<std::int64_t, 8> geometry{};
        std::uint16_t frame = 0;
        std::uint64_t ageTicks = 0;
        std::uint8_t alpha = 255, blend = 0;
        std::array<std::uint8_t, 4> color{255, 255, 255, 255};
        bool facingLeft = true, noDepth = false;
        bool operator==(const EffectVisualState &other) const {
            return geometry == other.geometry && frame == other.frame && ageTicks == other.ageTicks && alpha == other.alpha
                && blend == other.blend && color == other.color && facingLeft == other.facingLeft && noDepth == other.noDepth;
        }
    };
    struct ArenaAnimationState {
        std::uint64_t ticks = 0, waterResetStep = 0;
        bool operator==(const ArenaAnimationState &other) const {
            return ticks == other.ticks && waterResetStep == other.waterResetStep;
        }
    };
}
#endif
