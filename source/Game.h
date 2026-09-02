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

#ifndef DUEL6_GAME_H
#define DUEL6_GAME_H

#include <utility>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include "Type.h"
#ifndef D6R_HEADLESS_CORE
#include "Context.h"
#endif
#include "World.h"
#include "Player.h"
#ifndef D6R_HEADLESS_CORE
#include "WorldRenderer.h"
#endif
#include "Water.h"
#ifndef D6R_HEADLESS_CORE
#include "AppService.h"
#endif
#include "GameSettings.h"
#include "GameResources.h"
#include "Round.h"
#include "math/RandomSource.h"

namespace Duel6 {
    class GameMode;

    class Menu;

    class Game
#ifndef D6R_HEADLESS_CORE
            : public Context
#endif
    {
#ifndef D6R_HEADLESS_CORE
    public:
        class PlayerDefinition {
        private:
            Person &person;
            PlayerSkinColors colors;
            const PlayerSounds &sounds;
            const PlayerControls &controls;

        public:
            PlayerDefinition(Person &person, const PlayerSkinColors &colors, const PlayerSounds &sounds,
                             const PlayerControls &controls)
                    : person(person), colors(colors), sounds(sounds), controls(controls) {}

            Person &getPerson() const {
                return person;
            }

            PlayerSkinColors &getColors() {
                return colors;
            }

            const PlayerSkinColors &getColors() const {
                return colors;
            }

            const PlayerSounds &getSounds() const {
                return sounds;
            }

            const PlayerControls &getControls() const {
                return controls;
            }
        };
#endif

    private:
#ifndef D6R_HEADLESS_CORE
        AppService *appService;
#endif
        GameResources &resources;
        GameSettings &settings;
        RandomSource &randomSource;
        GameMode *gameMode;
        std::unique_ptr<Round> round;
        bool headless;
#ifndef D6R_HEADLESS_CORE
        std::unique_ptr<WorldRenderer> worldRenderer;
        const Menu *menu;
        std::vector<std::string> levels;
        std::vector<Size> backgrounds;
#endif

        Int32 currentRound;
        Int32 playedRounds;

        std::vector<Person> headlessPeople;
        std::vector<Player> players;
#ifndef D6R_HEADLESS_CORE
        std::vector<PlayerSkin> skins;
        std::unique_ptr<PlayerAnimations> playerAnimations;
        bool displayScoreTab = false;
#endif

    public:
#ifndef D6R_HEADLESS_CORE
        Game(AppService &appService, GameResources &resources, GameSettings &settings);
#endif
        Game(GameResources &resources, GameSettings &settings);

        Game(GameResources &resources, GameSettings &settings, RandomSource &randomSource);

#ifndef D6R_HEADLESS_CORE
        void start(const std::vector<PlayerDefinition> &playerDefinitions, const std::vector<std::string> &levels,
                   const std::vector<Size> &backgrounds, GameMode &gameMode);
        void startHeadless(const std::vector<std::string> &playerNames, const std::vector<std::string> &levels,
                           GameMode &gameMode);
#endif
        void startHeadlessRound(const std::vector<std::string> &playerNames, const std::string &level,
                                 const std::vector<Size> &rosterSlots, bool mirror, GameMode &gameMode);
        void endHeadlessRound();

#ifndef D6R_HEADLESS_CORE
        void keyEvent(const KeyPressEvent &event) override;

        void textInputEvent(const TextInputEvent &event) override;

        void mouseButtonEvent(const MouseButtonEvent &event) override;

        void mouseMotionEvent(const MouseMotionEvent &event) override;

        void mouseWheelEvent(const MouseWheelEvent &event) override;

        void joyDeviceAddedEvent(const JoyDeviceAddedEvent & event) override;

        void joyDeviceRemovedEvent(const JoyDeviceRemovedEvent & event) override;

        void update(Float32 elapsedTime) override;

        void render() const override;
#else
        void update(Float32 elapsedTime);
#endif

#ifndef D6R_HEADLESS_CORE
        AppService &getAppService() const {
            return *appService;
        }
#endif

        bool isHeadless() const { return headless; }
        void log(const std::string &message) const;

#ifndef D6R_HEADLESS_CORE
        const std::vector<Size> &getBackgrounds() const {
            return backgrounds;
        }
#endif

        std::vector<Player> &getPlayers() {
            return players;
        }

        const std::vector<Player> &getPlayers() const {
            return players;
        }

        GameResources &getResources() {
            return resources;
        }

        const GameResources &getResources() const {
            return resources;
        }

        GameSettings &getSettings() {
            return settings;
        }

        const GameSettings &getSettings() const {
            return settings;
        }

        RandomSource &getRandomSource() const { return randomSource; }

        Round &getRound() {
            return *round;
        }

        const Round &getRound() const {
            return *round;
        }

        Int32 getPlayedRounds() const {
            return playedRounds;
        }

        void setPlayedRounds(Int32 playedRounds) {
            this->playedRounds = playedRounds;
        }

        Int32 getCurrentRound() const;

        bool isOver() const {
            return getRound().isLast() && getRound().isOver();
        }

#ifndef D6R_HEADLESS_CORE
        bool isDisplayingScoreTab() const {
            return displayScoreTab;
        }

        WorldRenderer &getWorldRenderer() {
            return *worldRenderer;
        }

        const WorldRenderer &getWorldRenderer() const {
            return *worldRenderer;
        }
#endif

        GameMode &getMode() {
            return *gameMode;
        }

        const GameMode &getMode() const {
            return *gameMode;
        }

#ifndef D6R_HEADLESS_CORE
        void setMenuReference(const Menu &menu) {
            this->menu = &menu;
        }
#endif

    private:
        void initializeHeadlessPlayers(const std::vector<std::string> &playerNames,
                                       const std::vector<Size> &rosterSlots, GameMode &gameMode);
#ifndef D6R_HEADLESS_CORE
        void beforeStart(Context *prevContext) override;

        void beforeClose(Context *nextContext) override;

        void startRound();

        void nextRound();
#endif

        void endRound();

        void onRoundEnd();
    };
}

#endif
