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

#ifndef DUEL6_WORLD_H
#define DUEL6_WORLD_H

#include <memory>
#include <functional>

#include "Format.h"
#include "Level.h"
#include "InfoMessageQueue.h"
#ifndef D6R_HEADLESS_CORE
#include "LevelRenderData.h"
#include "Explosion.h"
#endif
#include "Fire.h"
#include "ShotList.h"
#include "BonusList.h"
#include "ElevatorList.h"

namespace Duel6 {
    class Game;

    struct GameplayEvent {
        std::string kind;
        std::string entityKind;
        std::uint64_t localEntityId = 0;
        Size playerRosterSlot = static_cast<Size>(-1);
        Size targetRosterSlot = static_cast<Size>(-1);
        std::string valueCategory;
        std::int64_t value = 0;
    };

    class World {
    private:
        const GameSettings &gameSettings;
        std::vector<Player> &players;
        RandomSource &randomSource;
        Level level;
#ifndef D6R_HEADLESS_CORE
        std::string background;
        std::unique_ptr<LevelRenderData> levelRenderData;
#endif
        InfoMessageQueue messageQueue;
        SpriteList spriteList;
        ShotList shotList;
#ifndef D6R_HEADLESS_CORE
        ExplosionList explosionList;
#endif
        FireList fireList;
        BonusList bonusList;
        ElevatorList elevatorList;
        Float32 time;
        std::function<void(const GameplayEvent &)> gameplayEventSink;

    public:
        World(Game &game, const std::string &levelPath, bool mirror, RandomSource &randomSource);

        void update(Float32 elapsedTime);

        void raiseWater();

        void setGameplayEventSink(std::function<void(const GameplayEvent &)> sink) {
            gameplayEventSink = std::move(sink);
        }

        void emitGameplayEvent(GameplayEvent event) const {
            if (gameplayEventSink) {
                try { gameplayEventSink(event); } catch (...) {}
            }
        }

        const GameSettings &getGameSettings() const {
            return gameSettings;
        }

        RandomSource &getRandomSource() const { return randomSource; }

        std::vector<Player> &getPlayers() {
            return players;
        }

        const std::vector<Player> &getPlayers() const {
            return players;
        }

        Level &getLevel() {
            return level;
        }

        const Level &getLevel() const {
            return level;
        }

#ifndef D6R_HEADLESS_CORE
        LevelRenderData &getLevelRenderData() {
            return *levelRenderData;
        }

        const LevelRenderData &getLevelRenderData() const {
            return *levelRenderData;
        }
#endif

        InfoMessageQueue &getMessageQueue() {
            return messageQueue;
        }

        const InfoMessageQueue &getMessageQueue() const {
            return messageQueue;
        }

        SpriteList &getSpriteList() {
            return spriteList;
        }

        const SpriteList &getSpriteList() const {
            return spriteList;
        }

        ShotList &getShotList() {
            return shotList;
        }

        const ShotList &getShotList() const {
            return shotList;
        }

#ifndef D6R_HEADLESS_CORE
        ExplosionList &getExplosionList() {
            return explosionList;
        }

        const ExplosionList &getExplosionList() const {
            return explosionList;
        }
#endif

        FireList &getFireList() {
            return fireList;
        }

        const FireList &getFireList() const {
            return fireList;
        }

#ifndef D6R_HEADLESS_CORE
        std::string getBackground() const {
            return background;
        }
#endif

        BonusList &getBonusList() {
            return bonusList;
        }

        const BonusList &getBonusList() const {
            return bonusList;
        }

        ElevatorList &getElevatorList() {
            return elevatorList;
        }

        const ElevatorList &getElevatorList() const {
            return elevatorList;
        }

        Float32 getTime() const {
            return time;
        }

    private:
#ifndef D6R_HEADLESS_CORE
        std::string findBackground(const GameResources::BackgroundList &backgrounds);
#endif
    };
}

#endif
