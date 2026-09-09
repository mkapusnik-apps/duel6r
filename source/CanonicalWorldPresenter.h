#ifndef DUEL6_CANONICALWORLDPRESENTER_H
#define DUEL6_CANONICALWORLDPRESENTER_H

#include <memory>
#include <string>
#include <vector>

#include "GameResources.h"
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
                    const Network::Replication::CanonicalState *state);
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
        std::string loadedLevel;
        bool loadedMirror = false;
        bool loadRound(const Network::Replication::RoundState &round);
        const PlayerSkin &skinFor(const Network::Replication::PlayerState &player) const;
        Animation animationFor(const Network::Replication::PlayerState &player) const;
        Texture backgroundTexture() const;
        void renderEntity(const Network::Replication::WorldEntityState &entity) const;
    };
}

#endif
