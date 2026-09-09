#include "CanonicalWorldPresenter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

#include "AppService.h"
#include "Bonus.h"
#include "Defines.h"
#include "Material.h"
#include "Orientation.h"
#include "Sprite.h"
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

        Float32 unitRatio(std::int64_t value, Float32 maximum) {
            return std::clamp(static_cast<Float32>(value) / maximum, 0.0f, 1.0f);
        }

        std::string utf8Prefix(const std::string &value, std::size_t maximumCodePoints) {
            std::size_t offset = 0, count = 0;
            while (offset < value.size() && count < maximumCodePoints) {
                const auto lead = static_cast<unsigned char>(value[offset]);
                const std::size_t bytes = lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2
                        : (lead & 0xf0) == 0xe0 ? 3 : 4;
                if (bytes > value.size() - offset) break;
                offset += bytes; ++count;
            }
            return value.substr(0, offset);
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
            : service(value), resources(gameResources), renderer(value.getVideo().getRenderer()), font(value.getFont()),
              animations(gameResources.getPlayerAnimation()), explosions(gameResources, 4.0f),
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

    bool CanonicalWorldPresenter::loadRound(
            const Network::Replication::RoundState &round,
            const std::vector<std::string> &replicatedLevels) {
        const std::set<std::string> local(canonicalLevels.begin(), canonicalLevels.end());
        const std::set<std::string> replicated(replicatedLevels.begin(), replicatedLevels.end());
        if (local.empty() || local != replicated || !canonicalLevelId(round.level, canonicalLevels)) return false;
        if (level && loadedLevel == round.level && loadedMirror == round.mirrored) return true;
        levelRenderData.reset();
        level.reset();
        level = std::make_unique<Level>(round.level, round.mirrored, resources.getBlockMeta());
        levelRenderData = std::make_unique<LevelRenderData>(*level, renderer, 0.15f);
        levelRenderData->generateFaces();
        loadedLevel = round.level;
        loadedMirror = round.mirrored;
        return true;
    }

    void CanonicalWorldPresenter::update(
            Float32 elapsedTime, const Network::Replication::CanonicalState *state,
            const std::vector<Network::Replication::PresentationEvent> &events) {
        for (auto iterator = playerStatusRemaining.begin(); iterator != playerStatusRemaining.end();) {
            iterator->second -= elapsedTime;
            if (iterator->second <= 0) iterator = playerStatusRemaining.erase(iterator); else ++iterator;
        }
        if (state && state->round && loadRound(*state->round, state->settings.levels) && levelRenderData)
            levelRenderData->update(elapsedTime);
        explosions.update(elapsedTime);
        if (!state) return;
        if (presentedSession != state->sessionId) {
            presentedSession = state->sessionId;
            highestPresentedEvent = 0;
            presentedRound = 0;
            presentedRoundStartedAt = 0;
            presentedEntities.clear();
            playerStatusRemaining.clear();
        }
        if (state->round && presentedRound != state->round->roundId) {
            presentedRound = state->round->roundId;
            presentedRoundStartedAt = state->phaseTime;
        }
        for (const auto playerId: state->messages.currentPlayerIndicators)
            playerStatusRemaining[playerId] = 5.0f;
        for (const auto &event: events) {
            if (event.eventId <= highestPresentedEvent) continue;
            presentEvent(*state, event);
            const auto affected = event.targetPlayerId ? event.targetPlayerId : event.playerId;
            if (affected) playerStatusRemaining[affected] = 5.0f;
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
        const auto player = std::find_if(state.players.begin(), state.players.end(), [&](const auto &value) {
            return value.playerId == (event.targetPlayerId ? event.targetPlayerId : event.playerId);
        });
        const Vector centre = entity ? Vector(worldValue(entity->positionX), worldValue(entity->positionY))
                : player != state.players.end() ? Vector(worldValue(player->positionX), worldValue(player->positionY))
                                                : Vector::ZERO;
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
            explosions.add(centre, 0.1f, 0.35f, Color::RED);
        } else if (event.type == "player-died") {
            playerDeathSound.play();
            explosions.add(centre, 0.2f, 0.8f, Color::RED);
        } else if (event.type == "player-killed") {
            explosions.add(centre, 0.12f, 0.48f, Color::BLUE);
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
            explosions.add(centre, 0.3f, 1.2f, Color(255, 176, 64));
        } else if (event.type == "tree-burned") {
            explosions.add(centre, 0.15f, 0.55f, Color(255, 96, 32));
        } else if (event.type == "environmental-damage") {
            playerHitSound.play();
        } else if (event.type == "player-spawned") {
            explosions.add(centre, 0.1f, 0.45f, Color::WHITE);
        } else if (event.type == "elevator-velocity-changed") {
            explosions.add(centre, 0.04f, 0.2f, Color(235, 235, 235));
        }
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
        if (!entity.active && entity.kind != Network::Replication::EntityKind::Tree
            && entity.kind != Network::Replication::EntityKind::Water) return;
        const Vector centre(worldValue(entity.positionX), worldValue(entity.positionY), 0.65f);
        using Kind = Network::Replication::EntityKind;
        switch (entity.kind) {
            case Kind::Shot:
            case Kind::Projectile:
                if (const Weapon *weapon = weaponFor(entity.type))
                    renderer.quadXY(centre - Vector(0.18f, 0.12f), Vector(0.36f, 0.24f),
                                    Vector(0, 1, 0), Vector(1, -1),
                                    Material::makeMaskedTexture(weapon->getNetworkProjectileTexture()));
                else renderer.point(centre, 4.0f, Color(255, 232, 128));
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
                renderer.quadXY(centre - Vector(0.5f, 0.12f), Vector(1.0f, 0.24f),
                                Vector(0, 1, 0), Vector(1, -1),
                                Material::makeMaskedTexture(resources.getElevatorTextures()));
                break;
            case Kind::Water:
                if (level) {
                    renderer.setBlendFunc(BlendFunc::SrcAlpha);
                    renderer.quadXY(Vector(0.0f, 0.0f, 0.72f),
                                    Vector(static_cast<Float32>(level->getWidth()),
                                           static_cast<Float32>(entity.primaryValue + 1)),
                                    Color(32, 96, 224, 128));
                    renderer.setBlendFunc(BlendFunc::None);
                }
                break;
            case Kind::Fire:
                renderer.quadXY(centre - Vector(0.5f, 0.5f), Vector(1, 1), Vector(0, 1, 0),
                                Vector(1, -1), Material::makeMaskedTexture(resources.getBurningTexture()));
                break;
            case Kind::Explosion:
                renderer.quadXY(centre - Vector(0.6f, 0.6f), Vector(1.2f, 1.2f), Vector(0, 1, 0),
                                Vector(1, -1), Material::makeMaskedTexture(resources.getExplosionTextures()));
                break;
            case Kind::Tree:
                try {
                    const auto type = static_cast<Size>(std::stoul(entity.type));
                    const auto found = resources.getFireTextures().find(type);
                    if (found != resources.getFireTextures().end())
                        renderer.quadXY(centre - Vector(0.5f, 0.5f), Vector(1, 1),
                                        Vector(0, 1, entity.active ? 0 : 1), Vector(1, -1),
                                        Material::makeMaskedTexture(found->second));
                } catch (...) {}
                break;
            case Kind::Hazard:
                renderer.frame(centre - Vector(0.4f, 0.4f), Vector(0.8f, 0.8f), 2.0f, Color(224, 64, 64));
                break;
        }
    }

    void CanonicalWorldPresenter::renderHeldWeapon(
            const Network::Replication::PlayerState &player, Float32 x, Float32 y) const {
        if (player.lifeState != Network::Replication::LifeState::Alive || player.heldWeapon.empty()) return;
        const Weapon *weapon = weaponFor(player.heldWeapon);
        if (!weapon || !weapon->getNetworkWeaponTexture()) return;
        const Float32 direction = player.facingLeft ? -1.0f : 1.0f;
        renderer.quadXY(Vector(x + direction * 0.38f - 0.26f, y + 0.28f, 0.64f), Vector(0.52f, 0.28f),
                        player.facingLeft ? Vector(1, 1, 0) : Vector(0, 1, 0),
                        player.facingLeft ? Vector(-1, -1) : Vector(1, -1),
                        Material::makeMaskedTexture(weapon->getNetworkWeaponTexture()));
    }

    void CanonicalWorldPresenter::renderPlayerEffects(
            const Network::Replication::CanonicalState &state,
            const Network::Replication::PlayerState &player, Float32 x, Float32 y) const {
        if (player.lifeState != Network::Replication::LifeState::Alive) return;
        const std::uint64_t roundAge = state.phaseTime >= presentedRoundStartedAt
                                       ? state.phaseTime - presentedRoundStartedAt : 120;
        if (state.round && state.round->roundId == presentedRound && roundAge < 120) {
            const Float32 radius = 0.15f + 0.75f * static_cast<Float32>(roundAge) / 120.0f;
            for (Int32 angle = 0; angle < 360; angle += 24) {
                const Vector point = Vector(x, y) + radius * Vector::direction(angle);
                renderer.point(Vector(point.x, point.y, 0.7f), 3.0f, Color::YELLOW);
            }
        }
        if (player.invulnerable) {
            const Int32 phase = static_cast<Int32>((state.phaseTime * 6u) % 360u);
            for (Int32 angle = phase; angle < phase + 360; angle += 15) {
                const Vector point = Vector(x + 0.5f, y + 0.5f) + 0.72f * Vector::direction(angle % 360);
                renderer.point(Vector(point.x, point.y, 0.71f), 2.0f, Color::RED);
            }
        }
        for (const auto &effect: state.effects) {
            if (effect.playerId != player.playerId || effect.remaining <= 0) continue;
            const Color color = effect.type == "invisibility" ? Color(192, 192, 192)
                    : effect.type == "invulnerability" ? Color::RED : Color::MAGENTA;
            renderer.point(Vector(x + 0.5f, y + 1.08f, 0.72f), 4.0f, color);
        }
    }

    void CanonicalWorldPresenter::renderPlayerStatus(
            const Network::Replication::CanonicalState &state,
            const Network::Replication::PlayerState &player, Float32 x, Float32 y) const {
        if (player.lifeState != Network::Replication::LifeState::Alive
            || playerStatusRemaining.find(player.playerId) == playerStatusRemaining.end()) return;
        constexpr Float32 TextHeight = 0.30f, BarWidth = 0.92f, BarHeight = 0.075f;
        Float32 statusY = y + 1.2f;
        const auto bar = [&](Color color, Float32 value) {
            renderer.quadXY(Vector(x + 0.04f, statusY, 0.74f), Vector(BarWidth, BarHeight), Color(16, 16, 32, 190));
            renderer.quadXY(Vector(x + 0.04f, statusY, 0.75f), Vector(BarWidth * std::clamp(value, 0.0f, 1.0f), BarHeight), color);
            statusY += 0.10f;
        };
        if (player.reloadRemaining > 0) {
            const Weapon *weapon = weaponFor(player.heldWeapon);
            const Float32 total = weapon ? std::max(1.0f, weapon->getReloadInterval() * 60.0f) : 60.0f;
            bar(Color::GREEN, 1.0f - unitRatio(player.reloadRemaining, total));
        }
        if (player.air < D6_MAX_AIR) bar(Color::BLUE, unitRatio(player.air, D6_MAX_AIR));
        if (!player.activeBonus.empty() && player.bonusRemaining > 0)
            bar(Color::MAGENTA, unitRatio(player.bonusRemaining, 600.0f));
        bar(Color::RED, unitRatio(player.life, D6_MAX_LIFE));

        const std::string name = utf8Prefix(player.displayName, 18);
        const Float32 nameWidth = std::max(0.24f, font.getTextWidth(name, TextHeight) + 0.08f);
        const std::string ammunition = std::to_string(player.ammunition);
        const Float32 ammoWidth = std::max(0.24f, font.getTextWidth(ammunition, TextHeight) + 0.08f);
        const Float32 left = x + 0.5f - (nameWidth + ammoWidth) * 0.5f;
        renderer.quadXY(Vector(left, statusY, 0.74f), Vector(nameWidth, TextHeight), Color(0, 0, 200, 220));
        font.print(left + 0.04f, statusY, 0.75f, Color::YELLOW, name, TextHeight);
        renderer.quadXY(Vector(left + nameWidth, statusY, 0.74f), Vector(ammoWidth, TextHeight), Color::YELLOW);
        font.print(left + nameWidth + 0.04f, statusY, 0.75f, Color::BLUE, ammunition, TextHeight);
        statusY += TextHeight + 0.08f;
        const auto score = std::find_if(state.score.players.begin(), state.score.players.end(), [&](const auto &row) {
            return row.playerId == player.playerId;
        });
        const std::int64_t marks = score == state.score.players.end() ? 0
                : std::clamp<std::int64_t>(score->roundPoints, 0, 15);
        for (std::int64_t mark = 0; mark < marks; ++mark)
            renderer.point(Vector(x + 0.5f - static_cast<Float32>(marks - 1) * 0.08f
                                  + static_cast<Float32>(mark) * 0.16f, statusY, 0.75f), 5.0f, Color::BLUE);
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
            if (pose == presentedPlayers.end()) continue;
            const Float32 px = worldValue(pose->positionX);
            const Float32 py = worldValue(pose->positionY);
            auto visualState = player;
            visualState.facingLeft = pose->facingLeft;
            visualState.crouching = pose->crouching;
            const PlayerSkin &skin = skinFor(visualState);
            const Animation animation = animationFor(visualState);
            Sprite sprite(animation, skin.getTexture());
            sprite.setPosition(Vector(px, py), 0.55f)
                    .setFrame(animationFrame(animation, state.phaseTime))
                    .setOrientation(visualState.facingLeft ? Orientation::Left : Orientation::Right)
                    .setAlpha(static_cast<Float32>(player.presentationAlpha) / 255.0f);
            sprite.render(renderer);
            renderHeldWeapon(visualState, px, py);
            renderPlayerEffects(state, visualState, px, py);
            renderPlayerStatus(state, visualState, px, py);
        }
        levelRenderData->getWater().render(resources.getBlockTextures(), true);
        explosions.render(renderer);
        renderer.enableDepthTest(false);
        renderer.setViewMatrix(Matrix::IDENTITY);
        (void) presentation;
        return true;
    }
}
