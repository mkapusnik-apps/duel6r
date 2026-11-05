#include "CanonicalWorldPresenter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <set>

#include "AppService.h"
#include "Bonus.h"
#include "DataException.h"
#include "Defines.h"
#include "Material.h"
#include "Orientation.h"
#include "Sprite.h"
#include "ArenaPresentation.h"
#include "ArenaScene.h"
#include "Water.h"
#include "network/NetworkTrustPolicy.h"

namespace Duel6 {
    namespace {
        constexpr Float32 FixedScale = 65536.0f;

        Float32 worldValue(std::int64_t value) {
            return static_cast<Float32>(value) / FixedScale;
        }

        Color teamColor(std::uint8_t team) {
            switch (team) {
                case 1: return Color(255, 0, 0);
                case 2: return Color(0, 255, 0);
                case 3: return Color(255, 255, 0);
                case 4: return Color(255, 0, 255);
                default: return Color(112, 112, 208);
            }
        }

        bool canonicalLevelId(const std::string &level, const std::vector<std::string> &manifestLevels) {
            return level.size() > 12 && level.compare(0, 7, "levels/") == 0
                   && level.compare(level.size() - 5, 5, ".json") == 0
                   && Network::Trust::validLogicalPath(level)
                   && std::find(manifestLevels.begin(), manifestLevels.end(), level) != manifestLevels.end();
        }

        void hashIdentity(std::uint64_t &hash, std::uint64_t value) {
            constexpr std::uint64_t Prime = 1099511628211ull;
            for (unsigned shift = 0; shift < 64; shift += 8) {
                hash ^= (value >> shift) & 0xffu;
                hash *= Prime;
            }
        }

        void hashString(std::uint64_t &hash, const std::string &value) {
            constexpr std::uint64_t Prime = 1099511628211ull;
            hashIdentity(hash, value.size());
            for (const unsigned char byte: value) {
                hash ^= byte;
                hash *= Prime;
            }
        }

        std::uint64_t nextPresentationRandom(std::uint64_t &state) {
            state += 0x9e3779b97f4a7c15ull;
            std::uint64_t value = state;
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
            return value ^ (value >> 31u);
        }

        class PresentationRandomSource final : public RandomSource {
        public:
            explicit PresentationRandomSource(std::uint64_t seed) : state(seed) {}

            std::uint64_t next(std::string_view) override {
                return nextPresentationRandom(state);
            }

            std::uint64_t bounded(std::uint64_t upperBound, std::string_view) override {
                if (upperBound == 0) D6_THROW(DataException, "Presentation random bound is invalid");
                const std::uint64_t threshold = (0u - upperBound) % upperBound;
                for (;;) {
                    const std::uint64_t value = nextPresentationRandom(state);
                    if (value >= threshold) return value % upperBound;
                }
            }

        private:
            std::uint64_t state;
        };

        std::uint64_t levelPresentationSeed(const Network::Replication::CanonicalState &state) {
            constexpr std::uint64_t Offset = 14695981039346656037ull;
            std::uint64_t hash = Offset;
            hashString(hash, "duel6r-network-level-presentation-v1");
            hashIdentity(hash, state.sessionId);
            hashIdentity(hash, state.matchId);
            hashIdentity(hash, state.round->roundId);
            hashString(hash, state.round->level);
            return hash;
        }

        std::string selectBackground(const Network::Replication::CanonicalState &state,
                                     const Level &level, const GameResources &resources) {
            const auto &available = resources.getBcgTextures().getTextures();
            const auto named = available.find(level.getBackground());
            if (!level.getBackground().empty() && named != available.end()) return named->first;

            std::vector<std::string> eligible;
            eligible.reserve(available.size());
            for (const auto &entry: available) eligible.push_back(entry.first);
            std::sort(eligible.begin(), eligible.end());
            if (eligible.empty())
                D6_THROW(DataException, "Required default network backgrounds are unavailable");

            constexpr std::uint64_t Offset = 14695981039346656037ull;
            std::uint64_t hash = Offset;
            hashString(hash, "duel6r-network-background-v1");
            hashIdentity(hash, state.sessionId);
            hashIdentity(hash, state.matchId);
            hashString(hash, state.round->level);
            hashIdentity(hash, eligible.size());
            for (const auto &entry: eligible) hashString(hash, entry);
            for (std::size_t remaining = eligible.size(); remaining > 1; --remaining) {
                const std::size_t selected = static_cast<std::size_t>(
                        nextPresentationRandom(hash) % remaining);
                std::swap(eligible[remaining - 1], eligible[selected]);
            }
            return eligible[static_cast<std::size_t>(state.round->roundId % eligible.size())];
        }

        std::string weaponKey(std::string value) {
            for (char &character: value) {
                if (character == ' ') character = '-';
                else character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

    }

    CanonicalWorldPresenter::CanonicalWorldPresenter(AppService &value, GameResources &gameResources)
            : service(value), resources(gameResources), renderer(value.getVideo().getRenderer()), font(value.getFont()),
              animations(gameResources.getPlayerAnimation()),
              playerHitSound(value.getSound().loadSample("sound/player/hit.wav")),
              playerDeathSound(value.getSound().loadSample("sound/player/death.wav")),
              bonusSound(value.getSound().loadSample("sound/player/pick-bonus.wav")),
              waterSound(value.getSound().loadSample("sound/game/water-blue.wav")) {
        skins.emplace_back(std::make_unique<PlayerSkin>(PlayerSkinColors(teamColor(0)),
                                                        value.getTextureManager(), animations));
        for (std::uint8_t team = 1; team <= 4; ++team) {
            PlayerSkinColors colors(teamColor(team));
            colors.set(PlayerSkinColors::HairTop, teamColor(team))
                    .set(PlayerSkinColors::Trousers, teamColor(team))
                    .set(PlayerSkinColors::HeadBand, teamColor(team))
                    .setHeadBand(true);
            skins.emplace_back(std::make_unique<PlayerSkin>(colors, value.getTextureManager(), animations));
        }
    }

    void CanonicalWorldPresenter::setCanonicalLevels(std::vector<std::string> levels) {
        std::set<std::string> unique;
        for (const auto &entry: levels) if (!canonicalLevelId(entry, levels) || !unique.insert(entry).second) {
            canonicalLevels.clear();
            return;
        }
        canonicalLevels = std::move(levels);
    }

    bool CanonicalWorldPresenter::loadRound(const Network::Replication::CanonicalState &state) {
        if (!state.round) return false;
        const auto &round = *state.round;
        const std::set<std::string> local(canonicalLevels.begin(), canonicalLevels.end());
        const std::set<std::string> replicated(state.settings.levels.begin(), state.settings.levels.end());
        if (local.empty() || local != replicated || !canonicalLevelId(round.level, canonicalLevels)) return false;
        if (level && loadedLevel == round.level && loadedMirror == round.mirrored
            && loadedSession == state.sessionId && loadedMatch == state.matchId
            && loadedRound == round.roundId) return true;
        levelRenderData.reset();
        level.reset();
        levelRandomSource = std::make_unique<PresentationRandomSource>(levelPresentationSeed(state));
        level = std::make_unique<Level>(round.level, round.mirrored, resources.getBlockMeta(),
                                        *levelRandomSource);
        loadedBackground = selectBackground(state, *level, resources);
        levelRenderData = std::make_unique<LevelRenderData>(*level, renderer);
        levelRenderData->generateFaces();
        loadedLevel = round.level;
        loadedSession = state.sessionId;
        loadedMatch = state.matchId;
        loadedRound = round.roundId;
        loadedMirror = round.mirrored;
        worldClock.reset();
        presentedEntities.clear();
        return true;
    }

    void CanonicalWorldPresenter::update(
            Float32 elapsedTime, const Network::Replication::CanonicalState *state,
            const std::vector<Network::Replication::PresentationEvent> &events, bool retainingWorld) {
        const bool advancing = state && !retainingWorld && Network::Replication::acceptsGameplayInput(*state);
        if (state && state->round && loadRound(*state) && levelRenderData) {
            const auto &round = *state->round;
            const Water *type = round.waterType == LiquidKind::Blue ? Water::BLUE
                              : round.waterType == LiquidKind::Red ? Water::RED
                              : round.waterType == LiquidKind::Green ? Water::GREEN : Water::NONE;
            if (type != Water::NONE) {
                const auto &blocks = resources.getBlockMeta();
                const auto block = std::find_if(blocks.begin(), blocks.end(), [&](const Block &value) {
                    return value.is(Block::Type::Water) && value.getWaterType() == type;
                });
                if (block == blocks.end()) D6_THROW(DataException, "Required liquid material is unavailable");
                levelRenderData->setWaterSurface(*block, round.waterLevel, round.waterRaising);
            }
            const Float32 seconds = worldClock.update(round.worldTime, elapsedTime, advancing);
            auto animation = round.animation;
            animation.ticks += static_cast<std::uint64_t>(std::max(0.0f, seconds - worldValue(round.worldTime))
                                                        * D6_UPDATE_FREQUENCY);
            levelRenderData->setAnimationState(animation);
        }
        if (!state) return;
        if (presentedSession != state->sessionId) {
            presentedSession = state->sessionId;
            highestPresentedEvent = 0;
            presentedEntities.clear();
        }
        for (const auto &event: events) {
            if (event.eventId <= highestPresentedEvent) continue;
            presentEvent(*state, event);
            highestPresentedEvent = event.eventId;
        }
        presentedEntities.clear();
        for (const auto &entity: state->entities) presentedEntities.emplace(entity.entityId, entity);
    }

    const Weapon *CanonicalWorldPresenter::weaponFor(const std::string &type) const {
        const auto found = std::find_if(Weapon::values().begin(), Weapon::values().end(), [&](const Weapon &weapon) {
            return weaponKey(weapon.getName()) == type;
        });
        return found == Weapon::values().end() ? nullptr : &*found;
    }

    const Network::Replication::WorldEntityState *CanonicalWorldPresenter::entityFor(
            const Network::Replication::CanonicalState &state,
            Network::Replication::Identity identity) const {
        const auto found = std::find_if(state.entities.begin(), state.entities.end(), [identity](const auto &entity) {
            return entity.entityId == identity;
        });
        if (found != state.entities.end()) return &*found;
        const auto retained = presentedEntities.find(identity);
        return retained == presentedEntities.end() ? nullptr : &retained->second;
    }

    void CanonicalWorldPresenter::presentEvent(
            const Network::Replication::CanonicalState &state,
            const Network::Replication::PresentationEvent &event) {
        const auto *entity = entityFor(state, event.entityId);
        if (event.type == "round-start") resources.getRoundStartSound().play();
        else if (event.type == "round-ended" || event.type == "round-outcome"
                 || event.type == "result-transition")
            resources.getGameOverSound().play();
        else if (event.type == "shot-fired") {
            const auto source = std::find_if(state.players.begin(), state.players.end(), [&](const auto &value) {
                return value.playerId == event.playerId;
            });
            const Weapon *weapon = entity ? weaponFor(entity->type)
                    : source == state.players.end() ? nullptr : weaponFor(source->heldWeapon);
            if (weapon) weapon->playNetworkShotSound();
        } else if (event.type == "shot-hit" || event.type == "player-life-changed") {
            playerHitSound.play();
        } else if (event.type == "player-died") {
            playerDeathSound.play();
        } else if (event.type == "bonus-picked" || event.type == "weapon-picked") bonusSound.play();
        else if (event.type == "bonus-expired" || event.type == "temporary-slowdown-expired"
                 || event.type == "reload-completed" || event.type == "weapon-charge-ready"
                 || event.type == "weapon-charge-released" || event.type == "air-level-changed") {
            // State-driven bars and labels below present these lifecycle transitions without prediction.
        }
        else if (event.type == "water-entered" || event.type == "water-exited"
                  || event.type == "water-level-changed" || event.type == "sudden-death-started")
            waterSound.play();
        else if (event.type == "explosion") {
            const auto source = std::find_if(state.players.begin(), state.players.end(), [&](const auto &value) {
                return value.playerId == event.playerId;
            });
            const Weapon *weapon = entity ? weaponFor(entity->type)
                    : source == state.players.end() ? nullptr : weaponFor(source->heldWeapon);
            if (weapon) weapon->playNetworkExplosionSound();
        } else if (event.type == "environmental-damage") {
            playerHitSound.play();
        }
    }

    const PlayerSkin &CanonicalWorldPresenter::skinFor(
            const Network::Replication::PlayerState &player) const {
        const std::size_t index = player.team > 0 && player.team < skins.size() ? player.team : 0;
        return *skins[index];
    }

    Animation CanonicalWorldPresenter::animationFor(
            const Network::Replication::PlayerState &player) const {
        return animations.forPose(player.animation.pose);
    }

    Texture CanonicalWorldPresenter::backgroundTexture() const {
        if (!level || loadedBackground.empty()) return Texture();
        const auto &backgrounds = resources.getBcgTextures().getTextures();
        const auto selected = backgrounds.find(loadedBackground);
        if (selected == backgrounds.end())
            D6_THROW(DataException, "Required default network background is unavailable: textures/backgrounds/"
                                    + loadedBackground);
        return selected->second;
    }

    void CanonicalWorldPresenter::appendEntity(
            const Network::Replication::WorldEntityState &entity, ArenaScene &scene) const {
        if (!entity.active && entity.kind != Network::Replication::EntityKind::Tree
            && entity.kind != Network::Replication::EntityKind::Water) return;
        const Vector centre(worldValue(entity.positionX), worldValue(entity.positionY), 0.6f);
        using Kind = Network::Replication::EntityKind;
        switch (entity.kind) {
            case Kind::Shot:
            case Kind::Projectile:
                if (const Weapon *weapon = weaponFor(entity.type)) {
                    const auto visual = weapon->getNetworkProjectileVisual();
                    const auto frame = entity.primaryValue;
                    if (frame < 0 || frame > Network::Replication::MaxProjectileAnimationFrame
                        || frame % 2 != 0) return;
                    // The transport bounds the offset; also check this weapon's
                    // actual animation before indexing its metadata.
                    for (std::int64_t offset = 0; offset <= frame; offset += 2)
                        if (visual.animation[offset] == -1) return;
                    const bool facingLeft = entity.velocityX < 0;
                    // Replication carries the collision centre, not the sprite origin.
                    // Mirror the same weapon-specific rectangle used by LegacyShot.
                    const Vector anchor = visual.collisionRect.getCentre();
                    const Vector origin(centre.x + (facingLeft ? -anchor.x : anchor.x - 1.0f),
                                        centre.y - anchor.y);
                    Sprite sprite(visual.animation, weapon->getNetworkProjectileTexture());
                    sprite.setPosition(origin, 0.6f)
                            .setFrame(static_cast<Size>(frame))
                            .setOrientation(facingLeft ? Orientation::Left : Orientation::Right);
                    scene.sprites.push_back(std::move(sprite));
                }
                break;
            case Kind::WeaponPickup:
                for (const auto &weapon: Weapon::values()) if (weaponKey(weapon.getName()) == entity.type) {
                    scene.weapons.emplace_back(weapon, static_cast<Int32>(entity.primaryValue), Vector(centre.x, centre.y));
                    return;
                }
                break;
            case Kind::BonusPickup:
                for (const auto *bonus: BonusType::ALL) if (bonus && bonus->getName() == entity.type) {
                    scene.bonuses.emplace_back(bonus, static_cast<Int32>(entity.primaryValue), Vector(centre.x, centre.y),
                                              bonus->getTextureIndex());
                    return;
                }
                break;
            case Kind::Elevator:
                scene.elevators.push_back(centre);
                break;
            case Kind::Water:
                // The shared level geometry renders accepted surface and side
                // faces. The legacy hazard entity carries no render rectangle.
                break;
            case Kind::Fire:
            case Kind::Explosion:
                // Continuing effects carry their own age; never invent a
                // persistent rectangle for an effect entity.
                break;
            case Kind::Tree:
                try {
                    const auto type = static_cast<Size>(std::stoul(entity.type));
                    const auto found = resources.getFireTextures().find(type);
                    if (found != resources.getFireTextures().end()) {
                        static const AnimationEntry intact[] = {0, 328, -1, 0}, burned[] = {1, 328, -1, 0};
                        Sprite sprite(entity.primaryValue == 1 ? burned : intact, found->second);
                        sprite.setPosition(Vector(centre.x, centre.y), 0.75f);
                        scene.sprites.push_back(std::move(sprite));
                    }
                } catch (...) {}
                break;
            case Kind::Hazard:
                break;
        }
    }

    void CanonicalWorldPresenter::appendHeldWeapon(
            const Network::Replication::PlayerState &player, Float32 x, Float32 y, ArenaScene &scene) const {
        if (player.lifeState != Network::Replication::LifeState::Alive || player.heldWeapon.empty()
            || !player.animation.weaponVisible) return;
        const Weapon *weapon = weaponFor(player.heldWeapon);
        if (!weapon || !weapon->getNetworkWeaponTexture()) return;
        const bool invisible = player.activeBonus == "invisibility" && player.bonusRemaining > 0;
        SpriteList sprites;
        auto held = weapon->makeSprite(sprites);
        held->setFrame(player.animation.weaponFrame).setPosition(Vector(x, y - (player.crouching ? 0.15f : 0.0f)), 0.5f)
                .setOrientation(player.facingLeft ? Orientation::Left : Orientation::Right)
                .setAlpha(invisible ? 0.2f : 1.0f);
        scene.sprites.push_back(*held);
    }

    void CanonicalWorldPresenter::appendPlayerHud(
            const Network::Replication::CanonicalState &state,
            const Network::Replication::PlayerState &player, Float32 x, Float32 y, ArenaScene &scene) const {
        ArenaPlayerHud hud;
        hud.dimensions = Vector(worldValue(player.width), worldValue(player.height));
        hud.centre = Vector(x, y) + hud.dimensions / 2.0f;
        hud.name = player.displayName; hud.ammunition = player.ammunition; hud.kills = player.roundKills;
        hud.life = player.life; hud.air = worldValue(player.air);
        hud.reload = worldValue(player.reloadRemaining); hud.reloadTotal = worldValue(player.reloadDuration);
        hud.bonus = worldValue(player.bonusRemaining); hud.bonusTotal = worldValue(player.bonusDuration);
        for (const auto *bonus: BonusType::ALL)
            if (bonus && bonus->getName() == player.activeBonus) hud.bonusTexture = bonus->getTextureIndex();
        hud.alive = player.lifeState == Network::Replication::LifeState::Alive;
        hud.invulnerable = player.invulnerable;
        hud.indicators.restore(player.hudIndicators);
        // Network arrivals retain per-player authoritative timing, including a
        // player admitted to an already running round and a fresh observer.
        const auto arrival = std::find_if(state.effects.begin(), state.effects.end(), [&](const auto &effect) {
            return effect.playerId == player.playerId && effect.type == "player-arrival";
        });
        if (hud.alive && arrival != state.effects.end())
            hud.locationRemaining = Float32(std::clamp<std::int64_t>(arrival->remaining, 0, 120)) / 60;
        scene.players.push_back(std::move(hud));
    }

    bool CanonicalWorldPresenter::render(
            const Network::Replication::CanonicalState &state,
            const Network::Responsiveness::ConnectionPresentationState &presentation,
            const std::vector<Network::Responsiveness::PresentedPlayerPose> &presentedPlayers,
            Int32 width, Int32 height) const {
#ifdef D6_MACOS_PLATFORM
        service.getVideo().resetDrawableViewport();
#endif
        if (!state.round || !level || !levelRenderData || width <= 0 || height <= 0) return false;
        const Matrix previousProjection = renderer.getProjectionMatrix();
        renderer.clearBuffers();
        renderer.setProjectionMatrix(Matrix::orthographic(0, width, 0, height, -1, 1));
        renderer.setViewMatrix(Matrix::IDENTITY);
        renderer.setModelMatrix(Matrix::IDENTITY);
        renderer.enableDepthTest(false);
        renderer.setBlendFunc(BlendFunc::None);
        const Texture background = backgroundTexture();
        if (background) renderer.quadXY(Vector(0, 0), Vector(width, height), Vector(0, 1),
                                        Vector(1, -1), Material(background));
        else renderer.quadXY(Vector(0, 0), Vector(width, height), Color(24, 28, 40));

        const auto &view = service.getVideo().getView();
        const Float32 aspect = static_cast<Float32>(width) / height;
        renderer.setProjectionMatrix(Matrix::perspective(view.getFieldOfView(), aspect,
                view.getNearClip(), view.getFarClip()));
        renderer.setViewMatrix(Matrix::lookAt(ArenaPresentation::cameraPosition(level->getWidth(),
                level->getHeight(), aspect, view.getFieldOfView()), -Vector::UNIT_Z, Vector::UNIT_Y));
        const ArenaGeometryScene scene{levelRenderData->getWalls(), levelRenderData->getSprites(),
                levelRenderData->getWater(), resources.getBlockTextures(), worldClock.value()};
        ArenaPresentation::staticGeometry(renderer, scene);
        ArenaScene dynamic{scene};
        dynamic.elevatorTexture = resources.getElevatorTextures();
        dynamic.bonusTexture = resources.getBonusTextures();
        dynamic.explosionTexture = resources.getExplosionTextures();
        for (const auto &entity: state.entities) appendEntity(entity, dynamic);
        for (const auto &player: state.players) {
            if (!player.visible || player.lifeState == Network::Replication::LifeState::Departed
                || player.lifeState == Network::Replication::LifeState::Waiting) continue;
            const auto pose = std::find_if(presentedPlayers.begin(), presentedPlayers.end(), [&](const auto &value) {
                return value.playerId == player.playerId;
            });
            // A retained full snapshot can outlive the responsiveness pose
            // cache during reconnect and host-end presentation. Canonical
            // positions remain complete and are the truthful fallback.
            const Float32 px = worldValue(pose == presentedPlayers.end()
                                          ? player.positionX : pose->positionX);
            const Float32 py = worldValue(pose == presentedPlayers.end()
                                          ? player.positionY : pose->positionY);
            auto visualState = player;
            if (pose != presentedPlayers.end()) {
                visualState.facingLeft = pose->facingLeft;
                visualState.crouching = pose->crouching;
            }
            const PlayerSkin &skin = skinFor(visualState);
            const Animation animation = animationFor(visualState);
            Sprite sprite(animation, skin.getTexture());
            sprite.setPosition(Vector(px, py), 0.5f)
                    .setFrame(player.animation.bodyFrame)
                    .setOrientation(visualState.facingLeft ? Orientation::Left : Orientation::Right)
                    .setAlpha(static_cast<Float32>(player.presentationAlpha) / 255.0f);
            dynamic.sprites.push_back(std::move(sprite));
            appendHeldWeapon(visualState, px, py, dynamic);
            appendPlayerHud(state, player, px, py, dynamic);
        }
        for (const auto &effect: state.effects) {
            if (effect.type == "explosion-ring") {
                const auto &v = effect.visual;
                dynamic.explosions.push_back({Vector(worldValue(v.geometry[0]), worldValue(v.geometry[1])),
                    worldValue(v.geometry[3]), worldValue(v.geometry[4]),
                    Color(v.color[0], v.color[1], v.color[2], v.color[3])});
                continue;
            }
            Texture texture{};
            if (effect.type.compare(0, 12, "weapon-boom:") == 0) {
                if (const auto *weapon = weaponFor(effect.type.substr(12))) texture = weapon->getNetworkExplosionTexture();
            } else if (effect.type == "tree-burning") texture = resources.getBurningTexture();
            else if (effect.type.compare(0, 13, "water-splash:") == 0) {
                for (const Water *water: {Water::BLUE, Water::RED, Water::GREEN})
                    if (effect.type == "water-splash:" + water->getName()) texture = water->getSplashTexture();
            }
            if (texture) {
                static const AnimationEntry still[] = {0, 1, -1, 0};
                Sprite sprite(still, texture);
                sprite.restoreVisual(effect.visual);
                dynamic.sprites.push_back(std::move(sprite));
            }
        }
        ArenaRenderer::render(renderer, font, dynamic);
        // Sprite effects may restore depth testing after their own no-depth
        // draw. Leave the renderer in the orthographic UI state expected by
        // NetworkMenu's HUD and overlays.
        renderer.enableDepthTest(false);
        renderer.setViewMatrix(Matrix::IDENTITY);
        renderer.setProjectionMatrix(previousProjection);
        (void) presentation;
        return true;
    }
}
