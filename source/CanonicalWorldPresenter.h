#ifndef DUEL6_CANONICALWORLDPRESENTER_H
#define DUEL6_CANONICALWORLDPRESENTER_H

#include <memory>
#include <map>
#include <string>
#include <vector>

#include "GameResources.h"
#include "Explosion.h"
#include "Font.h"
#include "Level.h"
#include "LevelRenderData.h"
#include "PlayerSkin.h"
#include "PresentationClock.h"
#include "math/RandomSource.h"
#include "network/NetworkResponsiveness.h"
#include "network/StateReplication.h"

namespace Duel6 {
    class AppService;
    struct ArenaScene;

    class CanonicalWorldPresenter final {
    public:
        CanonicalWorldPresenter(AppService &service, GameResources &resources);
        void setCanonicalLevels(std::vector<std::string> levels);
        void update(Float32 elapsedTime,
                    const Network::Replication::CanonicalState *state,
                    const std::vector<Network::Replication::PresentationEvent> &events,
                    bool retainingWorld = false);
        bool render(const Network::Replication::CanonicalState &state,
                    const Network::Responsiveness::ConnectionPresentationState &presentation,
                    const std::vector<Network::Responsiveness::PresentedPlayerPose> &presentedPlayers,
                    Int32 width, Int32 height) const;
        const std::string &backgroundIdentity() const noexcept { return loadedBackground; }
        Texture backgroundTexture() const;

    private:
        AppService &service;
        GameResources &resources;
        Renderer &renderer;
        Font &font;
        PlayerAnimations animations;
        std::vector<std::unique_ptr<PlayerSkin>> skins;
        std::unique_ptr<RandomSource> levelRandomSource;
        std::unique_ptr<Level> level;
        std::unique_ptr<LevelRenderData> levelRenderData;
        ExplosionList explosions;
        Sound::Sample playerHitSound;
        Sound::Sample playerDeathSound;
        Sound::Sample bonusSound;
        Sound::Sample waterSound;
        Network::Replication::Identity highestPresentedEvent = 0;
        Network::Replication::Identity presentedSession = 0;
        std::map<Network::Replication::Identity, Network::Replication::WorldEntityState> presentedEntities;
        std::string loadedLevel;
        std::string loadedBackground;
        std::vector<std::string> canonicalLevels;
        Network::Replication::Identity loadedSession = 0;
        Network::Replication::Identity loadedMatch = 0;
        Network::Replication::Identity loadedRound = 0;
        bool loadedMirror = false;
        PresentationClock worldClock;
        bool loadRound(const Network::Replication::CanonicalState &state);
        const PlayerSkin &skinFor(const Network::Replication::PlayerState &player) const;
        Animation animationFor(const Network::Replication::PlayerState &player) const;
        const Weapon *weaponFor(const std::string &type) const;
        const Network::Replication::WorldEntityState *entityFor(
                const Network::Replication::CanonicalState &state,
                Network::Replication::Identity identity) const;
        void presentEvent(const Network::Replication::CanonicalState &state,
                          const Network::Replication::PresentationEvent &event);
        void appendEntity(const Network::Replication::WorldEntityState &entity, ArenaScene &scene) const;
        void appendHeldWeapon(const Network::Replication::PlayerState &player,
                              Float32 x, Float32 y, ArenaScene &scene) const;
        void appendPlayerHud(const Network::Replication::CanonicalState &state,
                             const Network::Replication::PlayerState &player,
                             Float32 x, Float32 y, ArenaScene &scene) const;
    };
}

#endif
