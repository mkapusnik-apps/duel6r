#include "CanonicalWorldPresenter.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "AppService.h"
#include "Bonus.h"
#include "Material.h"
#include "Orientation.h"
#include "Sprite.h"

namespace Duel6 {
    namespace {
        constexpr Float32 FixedScale = 65536.0f;

        Float32 worldValue(std::int64_t value) {
            return static_cast<Float32>(value) / FixedScale;
        }

        Color teamColor(std::uint8_t team) {
            switch (team) {
                case 1: return Color(48, 104, 224);
                case 2: return Color(216, 48, 48);
                case 3: return Color(48, 176, 80);
                case 4: return Color(224, 192, 48);
                default: return Color(112, 112, 208);
            }
        }

        std::string weaponKey(std::string value) {
            for (char &character: value) {
                if (character == ' ') character = '-';
                else character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

        Size animationFrame(Animation animation, std::uint64_t phaseTick) {
            std::uint64_t duration = 0;
            for (Size frame = 0; animation[frame] != -1; frame += 2)
                duration += static_cast<std::uint64_t>(std::max(animation[frame + 1], 1));
            if (duration == 0) return 0;
            const std::uint64_t elapsed = (phaseTick * 1000u / 60u) % duration;
            std::uint64_t boundary = 0;
            for (Size frame = 0; animation[frame] != -1; frame += 2) {
                boundary += static_cast<std::uint64_t>(std::max(animation[frame + 1], 1));
                if (elapsed < boundary) return frame;
            }
            return 0;
        }
    }

    CanonicalWorldPresenter::CanonicalWorldPresenter(AppService &value, GameResources &gameResources)
            : service(value), resources(gameResources), renderer(value.getVideo().getRenderer()),
              animations(gameResources.getPlayerAnimation()) {
        skins.emplace_back(std::make_unique<PlayerSkin>(PlayerSkinColors(teamColor(0)),
                                                        value.getTextureManager(), animations));
        for (std::uint8_t team = 1; team <= 4; ++team) {
            skins.emplace_back(std::make_unique<PlayerSkin>(PlayerSkinColors(teamColor(team)),
                                                            value.getTextureManager(), animations));
        }
    }

    bool CanonicalWorldPresenter::loadRound(const Network::Replication::RoundState &round) {
        if (level && loadedLevel == round.level && loadedMirror == round.mirrored) return true;
        levelRenderData.reset();
        level.reset();
        std::string path = round.level;
        if (path.rfind("levels/", 0) != 0) path = "levels/" + path;
        if (path.size() < 5 || path.substr(path.size() - 5) != ".json") path += ".json";
        level = std::make_unique<Level>(path, round.mirrored, resources.getBlockMeta());
        levelRenderData = std::make_unique<LevelRenderData>(*level, renderer, 0.15f);
        levelRenderData->generateFaces();
        loadedLevel = round.level;
        loadedMirror = round.mirrored;
        return true;
    }

    void CanonicalWorldPresenter::update(
            Float32 elapsedTime, const Network::Replication::CanonicalState *state) {
        if (state && state->round && loadRound(*state->round) && levelRenderData)
            levelRenderData->update(elapsedTime);
    }

    const PlayerSkin &CanonicalWorldPresenter::skinFor(
            const Network::Replication::PlayerState &player) const {
        const std::size_t index = player.team > 0 && player.team < skins.size() ? player.team : 0;
        return *skins[index];
    }

    Animation CanonicalWorldPresenter::animationFor(
            const Network::Replication::PlayerState &player) const {
        if (player.lifeState != Network::Replication::LifeState::Alive)
            return animations.getDeadLying().get();
        if (player.crouching) return animations.getDuck().get();
        if (std::abs(player.velocityY) > 4096)
            return player.velocityY > 0 ? animations.getJump().get() : animations.getFall().get();
        if (std::abs(player.velocityX) > 1024) return animations.getWalk().get();
        return animations.getStand().get();
    }

    Texture CanonicalWorldPresenter::backgroundTexture() const {
        return level ? resources.getBcgTextures().at(level->getBackground()) : Texture();
    }

    void CanonicalWorldPresenter::renderEntity(
            const Network::Replication::WorldEntityState &entity) const {
        if (!entity.active) return;
        const Vector centre(worldValue(entity.positionX), worldValue(entity.positionY), 0.65f);
        using Kind = Network::Replication::EntityKind;
        switch (entity.kind) {
            case Kind::Shot:
            case Kind::Projectile:
                renderer.point(centre, 4.0f, Color(255, 232, 128));
                break;
            case Kind::WeaponPickup:
                for (const auto &weapon: Weapon::values()) if (weaponKey(weapon.getName()) == entity.type) {
                    LyingWeapon(weapon, static_cast<Int32>(entity.primaryValue),
                                Vector(centre.x, centre.y)).render(renderer);
                    return;
                }
                renderer.frame(centre - Vector(0.3f, 0.2f), Vector(0.6f, 0.4f), 2.0f, Color(232, 232, 232));
                break;
            case Kind::BonusPickup:
                for (const auto *bonus: BonusType::ALL) if (bonus && bonus->getName() == entity.type) {
                    Bonus(bonus, static_cast<Int32>(entity.primaryValue), Vector(centre.x, centre.y),
                          bonus->getTextureIndex()).render(renderer, resources.getBonusTextures());
                    return;
                }
                renderer.frame(centre - Vector(0.25f, 0.25f), Vector(0.5f, 0.5f), 3.0f, Color(96, 224, 128));
                break;
            case Kind::Elevator:
                renderer.quadXY(centre - Vector(0.5f, 0.12f), Vector(1.0f, 0.24f), Color(192, 144, 64));
                break;
            case Kind::Water:
                if (level && entity.active) {
                    renderer.setBlendFunc(BlendFunc::SrcAlpha);
                    renderer.quadXY(Vector(0.0f, 0.0f, 0.72f),
                                    Vector(static_cast<Float32>(level->getWidth()),
                                           static_cast<Float32>(entity.primaryValue + 1)),
                                    Color(32, 96, 224, 128));
                    renderer.setBlendFunc(BlendFunc::None);
                }
                break;
            case Kind::Fire:
            case Kind::Explosion:
                renderer.point(centre, entity.kind == Kind::Explosion ? 12.0f : 7.0f, Color(240, 96, 32));
                break;
            case Kind::Tree:
                renderer.quadXY(centre - Vector(0.25f, 0.5f), Vector(0.5f, 1.0f), Color(48, 128, 48));
                break;
            case Kind::Hazard:
                renderer.frame(centre - Vector(0.4f, 0.4f), Vector(0.8f, 0.8f), 2.0f, Color(224, 64, 64));
                break;
        }
    }

    bool CanonicalWorldPresenter::render(
            const Network::Replication::CanonicalState &state,
            const Network::Responsiveness::ConnectionPresentationState &presentation,
            const std::vector<Network::Responsiveness::PresentedPlayerPose> &presentedPlayers,
            Int32 width, Int32 height) const {
        if (!state.round || !level || !levelRenderData || width <= 0 || height <= 0) return false;
        const Float32 scale = std::min(static_cast<Float32>(width) / level->getWidth(),
                                      static_cast<Float32>(height) / level->getHeight());
        const Float32 x = (width - level->getWidth() * scale) * 0.5f;
        const Float32 y = (height - level->getHeight() * scale) * 0.5f;

        renderer.setViewMatrix(Matrix::IDENTITY);
        const Texture background = backgroundTexture();
        if (background) renderer.quadXY(Vector(0, 0), Vector(width, height), Vector(0, 1),
                                        Vector(1, -1), Material(background));
        else renderer.quadXY(Vector(0, 0), Vector(width, height), Color(24, 28, 40));

        renderer.setViewMatrix(Matrix::translate(x, y, 0) * Matrix::scale(scale, scale, 1));
        renderer.enableDepthTest(true);
        levelRenderData->getWalls().render(resources.getBlockTextures(), false);
        levelRenderData->getSprites().render(resources.getBlockTextures(), true);
        for (const auto &entity: state.entities) renderEntity(entity);
        for (const auto &player: state.players) {
            if (!player.visible || player.lifeState == Network::Replication::LifeState::Departed) continue;
            const auto pose = std::find_if(presentedPlayers.begin(), presentedPlayers.end(), [&](const auto &value) {
                return value.playerId == player.playerId;
            });
            const Float32 px = pose == presentedPlayers.end() ? worldValue(player.positionX)
                                                               : worldValue(pose->positionX);
            const Float32 py = pose == presentedPlayers.end() ? worldValue(player.positionY)
                                                               : worldValue(pose->positionY);
            auto visualState = player;
            if (pose != presentedPlayers.end()) {
                visualState.facingLeft = pose->facingLeft;
                visualState.crouching = pose->crouching;
            }
            const PlayerSkin &skin = skinFor(visualState);
            const Animation animation = animationFor(visualState);
            Sprite sprite(animation, skin.getTexture());
            sprite.setPosition(Vector(px, py), 0.55f)
                    .setFrame(animationFrame(animation, state.phaseTime))
                    .setOrientation(visualState.facingLeft ? Orientation::Left : Orientation::Right)
                    .setAlpha(static_cast<Float32>(player.presentationAlpha) / 255.0f);
            sprite.render(renderer);
            if (player.invulnerable)
                renderer.frame(Vector(px - 0.08f, py - 0.08f, 0.7f), Vector(1.16f, 1.16f), 2.0f, Color::RED);
        }
        levelRenderData->getWater().render(resources.getBlockTextures(), true);
        renderer.enableDepthTest(false);
        renderer.setViewMatrix(Matrix::IDENTITY);
        (void) presentation;
        return true;
    }
}
