/*
* Copyright (c) 2006, Ondrej Danek (www.ondrej-danek.net)
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Ondrej Danek nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "World.h"
#include "Game.h"
#include "Weapon.h"

namespace Duel6 {
    World::World(Game &game, const std::string &levelPath, bool mirror, RandomSource &randomSource)
            : gameSettings(game.getSettings()), players(game.getPlayers()), randomSource(randomSource),
#ifdef D6R_HEADLESS_CORE
              level(game.getResources().hasFrozenGameplayContent()
                    ? Level(game.getResources().getFrozenGameplayContent(levelPath), mirror,
                            game.getResources().getBlockMeta(), randomSource)
                    : Level(levelPath, mirror, game.getResources().getBlockMeta(), randomSource)),
#else
              level(levelPath, mirror, game.getResources().getBlockMeta(), randomSource),
#endif
              messageQueue(D6_INFO_DURATION),
#ifndef D6R_HEADLESS_CORE
              explosionList(game.isHeadless() ? Texture() : game.getResources().getExplosionTextures(),
                            D6_EXPL_SPEED),
#endif
#ifdef D6R_HEADLESS_CORE
              fireList(spriteList), bonusList(game.getSettings(), *this), elevatorList(Texture()), time(0) {
#else
              fireList(game.isHeadless() ? FireList(spriteList) : FireList(game.getResources(), spriteList)),
              bonusList(game.isHeadless() ? BonusList(game.getSettings(), *this)
                                          : BonusList(game.getSettings(), game.getResources(), *this)),
              elevatorList(game.isHeadless() ? Texture() : game.getResources().getElevatorTextures()), time(0) {
#endif
        game.log(Format("...Width   : {0}") << level.getWidth());
        game.log(Format("...Height  : {0}") << level.getHeight());
#ifndef D6R_HEADLESS_CORE
        if (!game.isHeadless()) {
            game.log("...Preparing faces");
            levelRenderData = std::make_unique<LevelRenderData>(
                    level, game.getAppService().getVideo().getRenderer(), D6_ANM_SPEED);
            levelRenderData->generateFaces();
            game.log(Format("...Walls   : {0}") << levelRenderData->getWalls().getFaces().size());
            game.log(Format("...Sprites : {0}") << levelRenderData->getSprites().getFaces().size());
            game.log(Format("...Water   : {0}") << levelRenderData->getWater().getFaces().size());
        }
#endif

        game.log("...Level initialization");
        game.log("...Loading elevators");
#ifdef D6R_HEADLESS_CORE
        if (game.getResources().hasFrozenGameplayContent())
            elevatorList.load(game.getResources().getFrozenGameplayContent(levelPath), mirror);
        else
#endif
            elevatorList.load(levelPath, mirror);
        fireList.find(level);
        fireList.setBurnedSink([this](Size localIdentity) {
            emitGameplayEvent({"tree-burned", "tree", localIdentity, static_cast<Size>(-1),
                               static_cast<Size>(-1), "burned", 1});
        });
#ifndef D6R_HEADLESS_CORE
        if (!game.isHeadless()) background = findBackground(game.getResources().getBcgTextures());
#endif
    }

    void World::update(Float32 elapsedTime) {
        time += elapsedTime;

        spriteList.update(elapsedTime);
#ifndef D6R_HEADLESS_CORE
        explosionList.update(elapsedTime);
        if (levelRenderData) levelRenderData->update(elapsedTime);
#endif
        shotList.update(*this, elapsedTime);
        elevatorList.update(elapsedTime);
        messageQueue.update(elapsedTime);
        bonusList.update(elapsedTime);

        if (Math::isAuthoritative()) time = Math::quantizeAuthoritative(time);

        // Add new bonuses
        Int32 mod = Int32(3.0f / elapsedTime);
        if (mod != 0 && Math::random(mod, randomSource, "bonus-spawn-timing") == 0) {
            bonusList.addRandomBonus();
        }
    }

    void World::raiseWater() {
        const Int32 previousLevel = level.getWaterLevel();
        level.raiseWater();
        if (previousLevel != level.getWaterLevel())
            emitGameplayEvent({"water-level-changed", "hazard", 1, static_cast<Size>(-1),
                               static_cast<Size>(-1), "level", level.getWaterLevel()});
#ifndef D6R_HEADLESS_CORE
        if (levelRenderData) levelRenderData->generateWater();
#endif
    }

#ifndef D6R_HEADLESS_CORE
    std::string World::findBackground(const GameResources::BackgroundList &backgrounds) {
        const std::string &levelBackground = level.getBackground();
        auto &bcgDict = backgrounds.getTextures();
        if (levelBackground.size() && bcgDict.find(levelBackground) != bcgDict.end()) {
            return levelBackground;
        }

        std::vector<std::string> bcgNames;
        bcgNames.reserve(bcgDict.size());

        for (auto &entry : bcgDict) {
            bcgNames.push_back(entry.first);
        }

        Int32 bcgIndex = Math::random(Int32(bcgNames.size()), randomSource, "background-selection");
        return bcgNames[bcgIndex];
    }
#endif
}
