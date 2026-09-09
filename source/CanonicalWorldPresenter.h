#ifndef DUEL6_CANONICALWORLDPRESENTER_H
#define DUEL6_CANONICALWORLDPRESENTER_H

#include <memory>
#include <map>
#include <string>
#include <vector>

#include "GameResources.h"
#include "Explosion.h"
#include "Level.h"
#include "LevelRenderData.h"
#include "PlayerSkin.h"
#include "network/NetworkResponsiveness.h"
#include "network/StateReplication.h"

namespace Duel6 {
    class AppService;

    class CanonicalWorldPresenter final {
    public:
        CanonicalWorldPresenter(AppService &service, GameResources &resources);
        void update(Float32 elapsedTime,
                    const Network::Replication::CanonicalState *state,
                    const std::vector<Network::Replication::PresentationEvent> &events);
        bool render(const Network::Replication::CanonicalState &state,
                    const Network::Responsiveness::ConnectionPresentationState &presentation,
                    const std::vector<Network::Responsiveness::PresentedPlayerPose> &presentedPlayers,
                    Int32 width, Int32 height) const;

    private:
        AppService &service;
        GameResources &resources;
        Renderer &renderer;
        PlayerAnimations animations;
        std::vector<std::unique_ptr<PlayerSkin>> skins;
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
        bool loadedMirror = false;
        bool loadRound(const Network::Replication::RoundState &round);
        const PlayerSkin &skinFor(const Network::Replication::PlayerState &player) const;
        Animation animationFor(const Network::Replication::PlayerState &player) const;
        Texture backgroundTexture() const;
        const Weapon *weaponFor(const std::string &type) const;
        const Network::Replication::WorldEntityState *entityFor(
                const Network::Replication::CanonicalState &state,
                Network::Replication::Identity identity) const;
        void presentEvent(const Network::Replication::CanonicalState &state,
                          const Network::Replication::PresentationEvent &event);
        void renderEntity(const Network::Replication::WorldEntityState &entity) const;
    };
}

#endif
