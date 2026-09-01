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

#include "Sound.h"
#include "WorldRenderer.h"
#include "Game.h"
#include "Menu.h"
#include "GameMode.h"
#include <stdexcept>

namespace Duel6 {
    Game::Game(AppService &appService, GameResources &resources, GameSettings &settings)
            : appService(&appService), resources(resources), settings(settings), gameMode(nullptr),
              worldRenderer(std::make_unique<WorldRenderer>(appService, *this)), menu(nullptr), headless(false),
              currentRound(0), playedRounds(0) {}

    Game::Game(GameResources &resources, GameSettings &settings)
            : appService(nullptr), resources(resources), settings(settings), gameMode(nullptr), menu(nullptr),
              headless(true), currentRound(0), playedRounds(0) {}

    void Game::log(const std::string &message) const {
        if (appService) appService->getConsole().printLine(message);
    }

    void Game::beforeStart(Context *prevContext) {
        if (!headless) SDL_ShowCursor(SDL_DISABLE);
    }

    void Game::beforeClose(Context *nextContext) {
        endRound();
    }

    void Game::render() const {
        if (worldRenderer) worldRenderer->render();
    }

    void Game::update(Float32 elapsedTime) {
        if (getRound().isOver()) {
            if (!getRound().isLast()) {
                nextRound();

            }
        } else {
            getRound().update(elapsedTime);
        }
    }

    void Game::keyEvent(const KeyPressEvent &event) {
        if (event.getCode() == SDLK_ESCAPE && (isOver() || event.withShift())) {
            close();
            return;
        }
        if (event.getCode() == SDLK_TAB && (!getRound().hasWinner())) {
            displayScoreTab = !displayScoreTab;
        }

        if (!getRound().isLast()) {
            if (event.getCode() == SDLK_F1 && (getRound().hasWinner() || event.withShift())) {
                nextRound();
                return;
            }
            if (getRound().hasWinner() && ((D6_GAME_OVER_WAIT - getRound().getRemainingGameOverWait()) > 3.0f)) {
                nextRound();
                return;
            }
        }

        getRound().keyEvent(event);
    }

    void Game::textInputEvent(const TextInputEvent &event) {}

    void Game::mouseButtonEvent(const MouseButtonEvent &event) {}

    void Game::mouseMotionEvent(const MouseMotionEvent &event) {}

    void Game::mouseWheelEvent(const MouseWheelEvent &event) {}

    void Game::joyDeviceAddedEvent(const JoyDeviceAddedEvent &event) {}

    void Game::joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &event) {}

    void Game::start(const std::vector<PlayerDefinition> &playerDefinitions, const std::vector<std::string> &levels,
                     const std::vector<Size> &backgrounds, GameMode &gameMode) {
        Console &console = appService->getConsole();
        console.printLine("\n=== Starting new game ===");
        console.printLine(Format("...Rounds: {0}") << settings.getMaxRounds());
        TextureManager &textureManager = appService->getTextureManager();
        players.clear();

        for (auto &skin : skins) {
            textureManager.dispose(skin.getTexture());
        }
        skins.clear();

        Size playerIndex = 0;
        players.reserve(playerDefinitions.size());
        skins.reserve(playerDefinitions.size());
        playerAnimations = std::make_unique<PlayerAnimations>(resources.getPlayerAnimation());
        for (const PlayerDefinition &playerDef : playerDefinitions) {
            console.printLine(Format("...Generating player for person: {0}") << playerDef.getPerson().getName());
            skins.push_back(PlayerSkin(playerDef.getColors(), textureManager, *playerAnimations));
            players.emplace_back(
                    playerDef.getPerson(), skins.back(), playerDef.getSounds(), playerDef.getControls(), playerIndex);
            playerIndex++;
        }

        this->levels = levels;
        Math::shuffle(this->levels, "local-level-shuffle");

        this->backgrounds = backgrounds;
        this->gameMode = &gameMode;
        gameMode.initializeGame(*this, players, settings.isQuickLiquid(), settings.isGlobalAssistances());
        startRound();
    }

    void Game::startHeadless(const std::vector<std::string> &playerNames,
                             const std::vector<std::string> &levels, GameMode &gameMode) {
        std::vector<Size> rosterSlots(playerNames.size());
        for (Size index = 0; index < rosterSlots.size(); ++index) rosterSlots[index] = index;
        initializeHeadlessPlayers(playerNames, rosterSlots, gameMode);
        this->levels = levels;
        Math::shuffle(this->levels, "headless-level-shuffle");
        startRound();
    }

    void Game::initializeHeadlessPlayers(const std::vector<std::string> &playerNames,
                                         const std::vector<Size> &rosterSlots, GameMode &gameMode) {
        if (playerNames.size() != rosterSlots.size()) throw std::invalid_argument("Headless roster mismatch");
        players.clear();
        headlessPeople.clear();
        headlessPeople.reserve(playerNames.size());
        players.reserve(playerNames.size());
        for (const auto &name: playerNames) headlessPeople.emplace_back(name, nullptr);
        for (Size index = 0; index < headlessPeople.size(); ++index)
            players.emplace_back(headlessPeople[index], rosterSlots[index]);
        backgrounds.clear();
        this->gameMode = &gameMode;
        gameMode.initializeGame(*this, players, settings.isQuickLiquid(), settings.isGlobalAssistances());
    }

    void Game::startHeadlessRound(const std::vector<std::string> &playerNames, const std::string &level,
                                  const std::vector<Size> &rosterSlots, bool mirror, GameMode &gameMode) {
        initializeHeadlessPlayers(playerNames, rosterSlots, gameMode);
        currentRound = playedRounds;
        round = std::make_unique<Round>(*this, playedRounds, level, mirror);
        round->setOnRoundEnd([this]() { onRoundEnd(); });
        round->start();
    }

    void Game::endHeadlessRound() {
        if (round) endRound();
    }

    void Game::startRound() {
        currentRound = playedRounds;
        displayScoreTab = false;

        bool shuffle = settings.getLevelSelectionMode() == LevelSelectionMode::Shuffle;
        Int32 level = shuffle ? playedRounds % Int32(levels.size())
                              : Math::random(Int32(levels.size()), "round-level");
        const std::string levelPath = levels[level];
        bool mirror = Math::random(2, "round-orientation") == 0;

        log(Format("\n===Loading level {0}===") << levelPath);
        log(Format("...Parameters: mirror: {0}") << mirror);

        round = std::make_unique<Round>(*this, playedRounds, levelPath, mirror);
        round->setOnRoundEnd([this]() {
            onRoundEnd();
        });
        round->start();
        if (worldRenderer) worldRenderer->prerender();
    }

    void Game::endRound() {
        if (round) round->end();
    }

    void Game::onRoundEnd() {
        playedRounds++;
        if (round->isLast()) {
            getMode().updateElo(players);
        }
        if (!headless && menu) menu->savePersonData();
    }

    void Game::nextRound() {
        endRound();
        startRound();
    }

    Int32 Game::getCurrentRound() const {
        return currentRound;
    }
}
