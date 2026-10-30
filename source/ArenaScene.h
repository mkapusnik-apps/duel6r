#ifndef DUEL6_ARENASCENE_H
#define DUEL6_ARENASCENE_H

#include <array>
#include <string>
#include <vector>
#include "ArenaPresentation.h"
#include "Bonus.h"
#include "Explosion.h"
#include "Font.h"
#include "PlayerIndicators.h"
#include "Sprite.h"

namespace Duel6 {
    // A value-only projection of a world. Adapters own simulation/transport
    // access; the renderer cannot change either, deliver events or write stats.
    struct ArenaPlayerHud {
        Vector centre, dimensions;
        std::string name;
        Int32 ammunition = 0, kills = 0, bonusTexture = -1;
        Float32 life = 0, air = 0, reload = 0, reloadTotal = 0, bonus = 0, bonusTotal = 0;
        PlayerIndicators indicators;
        bool alive = false, invulnerable = false;
        Float32 locationRemaining = 0;
    };

    struct ArenaScene {
        ArenaGeometryScene geometry;
        Texture elevatorTexture, bonusTexture, explosionTexture;
        std::vector<Vector> elevators;
        std::vector<Bonus> bonuses;
        std::vector<LyingWeapon> weapons;
        std::vector<Sprite> sprites;
        std::vector<ArenaPlayerHud> players;
        std::vector<Explosion> explosions;
    };

    class ArenaRenderer final {
    public:
        static void render(Renderer &renderer, const Font &font, const ArenaScene &scene) {
            renderer.enableDepthTest(true);
            renderer.enableDepthWrite(true);
            renderer.setBlendFunc(BlendFunc::None);
            renderer.setGlobalTime(scene.geometry.worldSeconds);
            for (const auto &position: scene.elevators)
                ArenaPresentation::elevator(renderer, scene.elevatorTexture, position);
            for (const auto &bonus: scene.bonuses) bonus.render(renderer, scene.bonusTexture);
            for (const auto &weapon: scene.weapons) weapon.render(renderer);
            for (bool transparent: {false, true}) {
                renderer.enableDepthWrite(!transparent);
                for (const auto &sprite: scene.sprites)
                    if (sprite.isTransparent() == transparent) sprite.render(renderer);
            }
            renderer.enableDepthWrite(true);
            renderer.setBlendFunc(BlendFunc::None);
            for (const auto &player: scene.players) invulnerability(renderer, player);
            ArenaPresentation::water(renderer, scene.geometry);
            for (const auto &player: scene.players) location(renderer, player);
            for (const auto &player: scene.players) status(renderer, font, player, scene.bonusTexture);
            renderer.enableDepthTest(false);
            for (const auto &explosion: scene.explosions) {
                auto position = explosion.centre - Vector(explosion.now, explosion.now);
                position.z = 0.6f;
                renderer.quadXY(position, Vector(2 * explosion.now, 2 * explosion.now),
                        Vector::ZERO, Vector(1, 1),
                        Material::makeMaskedColoredTexture(scene.explosionTexture, explosion.color));
            }
            renderer.enableDepthTest(true);
        }

    private:
        static void invulnerability(Renderer &renderer, const ArenaPlayerHud &player) {
            if (!player.invulnerable) return;
            Vector points[24];
            const Float32 radius = player.dimensions.length() / 2.0f;
            const Int32 phase = Int32(player.bonus * 30) % 360;
            for (Int32 index = 0; index < 24; ++index) {
                const auto point = player.centre + radius * Vector::direction((phase + index * 15) % 360);
                points[index] = Vector(point.x, point.y, 0.5f);
            }
            renderer.points(points, 24, 2.0f, Color::RED);
        }

        static void location(Renderer &renderer, const ArenaPlayerHud &player) {
            if (player.locationRemaining <= 0) return;
            renderer.enableDepthTest(false);
            const Float32 radius = 0.5f + 0.5f * std::abs(D6_YOU_ARE_HERE_DURATION / 2 - player.locationRemaining);
            Vector points[37], segments[72];
            for (Int32 index = 0; index < 37; ++index) {
                const auto point = player.centre + (index % 2 ? 1.05f : 0.95f) * radius * Vector::direction(index * 10);
                points[index] = Vector(point.x, point.y, 0.5f);
                if (index) { segments[2 * (index - 1)] = points[index - 1]; segments[2 * index - 1] = points[index]; }
            }
            renderer.lines(segments, 72, 3.0f, Color::YELLOW);
            renderer.enableDepthTest(true);
        }

        static void status(Renderer &renderer, const Font &font, const ArenaPlayerHud &player, Texture bonuses) {
            if (!player.alive) return;
            const Float32 x = player.centre.x;
            Float32 y = player.centre.y + player.dimensions.y / 2 + 0.15f;
            const auto bar = [&](const Indicator &indicator, Color color, Float32 ratio) {
                if (!indicator.isVisible()) return;
                const auto alpha = static_cast<Uint8>(255 * std::clamp(indicator.getAlpha(), 0.0f, 1.0f));
                renderer.enableDepthWrite(false);
                renderer.setBlendFunc(BlendFunc::SrcAlpha);
                renderer.quadXY(Vector(x - 0.5f, y - 0.1f, 0.5f), Vector(1.0f, 0.1f), Color::BLACK.withAlpha(alpha));
                renderer.quadXY(Vector(x - 0.49f, y - 0.08f, 0.5f), Vector(0.98f * ratio, 0.07f), color.withAlpha(alpha));
                renderer.setBlendFunc(BlendFunc::None);
                renderer.enableDepthWrite(true);
                y += 0.1f;
            };
            const auto &indicators = player.indicators;
            bar(indicators.getReload(), Color::GREEN, 1 - ArenaPresentation::ratio(player.reload, player.reloadTotal));
            bar(indicators.getAir(), Color::BLUE, ArenaPresentation::ratio(player.air, D6_MAX_AIR));
            bar(indicators.getBonus(), Color::MAGENTA, ArenaPresentation::ratio(player.bonus, player.bonusTotal));
            bar(indicators.getHealth(), Color::RED, ArenaPresentation::ratio(player.life, D6_MAX_LIFE));
            const std::string ammunition = std::to_string(player.ammunition);
            const Float32 nameWidth = indicators.getName().isVisible() ? font.getTextWidth(player.name, 0.3f) + 0.08f : 0;
            const Float32 ammoWidth = indicators.getBullets().isVisible() ? font.getTextWidth(ammunition, 0.3f) + 0.08f : 0;
            const Float32 bonusWidth = indicators.getBonus().isVisible() ? 0.38f : 0;
            const Float32 left = x - (nameWidth + ammoWidth + bonusWidth) / 2;
            const auto label = [&](const Indicator &indicator, Float32 start, Float32 allocation,
                                   const std::string &text, Color background, Color foreground) {
                if (!indicator.isVisible()) return;
                const auto alpha = static_cast<Uint8>(255 * std::clamp(indicator.getAlpha(), 0.0f, 1.0f));
                renderer.enableDepthWrite(false);
                renderer.setBlendFunc(BlendFunc::SrcAlpha);
                renderer.quadXY(Vector(start + 0.04f, y, 0.5f), Vector(allocation - 0.08f, 0.3f), background.withAlpha(alpha));
                renderer.setBlendFunc(BlendFunc::None);
                font.print(start + 0.04f, y, 0.5f, foreground.withAlpha(alpha), text, 0.3f);
                renderer.enableDepthWrite(true);
            };
            label(indicators.getName(), left, nameWidth, player.name, Color::BLUE, Color::YELLOW);
            label(indicators.getBullets(), left + nameWidth, ammoWidth, ammunition, Color::YELLOW, Color::BLUE);
            if (bonusWidth && player.bonusTexture >= 0) {
                const auto alpha = static_cast<Uint8>(255 * std::clamp(indicators.getBonus().getAlpha(), 0.0f, 1.0f));
                renderer.enableDepthWrite(false);
                renderer.setBlendFunc(BlendFunc::SrcAlpha);
                renderer.quadXY(Vector(left + nameWidth + ammoWidth + 0.04f, y, 0.5f), Vector(0.3f, 0.3f),
                        Vector(0.3f, 0.7f, Float32(player.bonusTexture)), Vector(0.4f, -0.4f),
                        Material::makeColoredTexture(bonuses, Color::WHITE.withAlpha(alpha)));
                renderer.setBlendFunc(BlendFunc::None);
                renderer.enableDepthWrite(true);
            }
            if (nameWidth || ammoWidth || bonusWidth) y += 0.4f;
            Float32 markX = x + 0.05f - (2 * player.kills - 1) * 0.05f;
            for (Int32 index = 0; index < player.kills; ++index, markX += 0.2f)
                renderer.point(Vector(markX, y + 0.1f, 0.5f), 5.0f, Color::BLUE);
        }
    };
}
#endif
