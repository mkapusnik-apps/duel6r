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

#include <algorithm>

#include "Round.h"
#include "Game.h"
#include "GameException.h"
#include "GameMode.h"
#include "Weapon.h"
#ifndef D6R_HEADLESS_CORE
#include "PersonProfile.h"
#endif

namespace Duel6 {
    Round::Round(Game &game, Int32 roundNumber, const std::string &levelPath, bool mirror)
            : game(game), roundNumber(roundNumber), world(game, levelPath, mirror),
              suddenDeathMode(false), waterFillWait(0), showYouAreHere(D6_YOU_ARE_HERE_DURATION), gameOverWait(0),
              winner(false)
#ifndef D6R_HEADLESS_CORE
              , scriptContext(world)
#endif
              {}

    void Round::start() {
#ifdef D6R_HEADLESS_CORE
        startTime = 0;
#else
        startTime = game.isHeadless() ? 0 : SDL_GetTicks();
#endif
        auto &players = world.getPlayers();
        game.getMode().initializePlayerPositions(game, players, world);
        setPlayerViews();
        game.getMode().initializeRound(game, players, world);
        scriptStart();
#ifndef D6R_HEADLESS_CORE
        if (!game.isHeadless()) game.getResources().getRoundStartSound().play();
#endif
    }

    void Round::end() {
        scriptEnd();

        auto &players = world.getPlayers();
        for (Player &player : players) {
            player.endRound();
        }
    }

    void Round::scriptStart() {
#ifdef D6R_HEADLESS_CORE
        return;
#else
        if (game.isHeadless()) return;
        auto &players = world.getPlayers();
        for (auto &player : players) {
            PersonProfile *profile = player.getPerson().getProfile();
            if (profile != nullptr) {
                auto &personScripts = profile->getScripts();
                for (auto &script : personScripts) {
                    script->roundStart(player, scriptContext);
                }
            }
        }
#endif
    }

    void Round::scriptUpdate(Player &player) {
#ifdef D6R_HEADLESS_CORE
        (void) player;
        return;
#else
        if (game.isHeadless()) return;
        Uint32 roundTime = SDL_GetTicks() - startTime;
        PersonProfile *profile = player.getPerson().getProfile();
        if (profile != nullptr) {
            auto &personScripts = profile->getScripts();
            for (auto &script : personScripts) {
                script->roundUpdate(roundTime, player, scriptContext);
            }
        }
#endif
    }

    void Round::scriptEnd() {
#ifdef D6R_HEADLESS_CORE
        return;
#else
        if (game.isHeadless()) return;
        Uint32 roundTime = SDL_GetTicks() - startTime;
        auto &players = world.getPlayers();
        for (auto &player : players) {
            PersonProfile *profile = player.getPerson().getProfile();
            if (profile != nullptr) {
                auto &personScripts = profile->getScripts();
                for (auto &script : personScripts) {
                    script->roundEnd(roundTime, player, scriptContext);
                }
            }
        }
#endif
    }

    void Round::setPlayerViews() {
#ifdef D6R_HEADLESS_CORE
        return;
#else
        if (game.isHeadless()) return;
        const Video &video = game.getAppService().getVideo();
        std::vector<Player> &players = world.getPlayers();

        for (Player &player : players) {
            player.prepareCam(video, world.getLevel().getWidth(), world.getLevel().getHeight());
            player.setView(PlayerView(0, 0, video.getScreen().getClientWidth(), video.getScreen().getClientHeight()));
        }
#endif
    }

    void Round::checkWinner() {
        std::vector<Player> &allPlayers = world.getPlayers();

        alivePlayers.clear();

        // todo: rewrite to copy_if if it is possible to do it that way without billion lines of compile errors:-)
        for (Player &player : allPlayers) {
            if (player.isAlive()) {
                alivePlayers.push_back(&player);
            }
        }

        if (!suddenDeathMode && game.getMode().checkForSuddenDeathMode(world, alivePlayers)) {
            suddenDeathMode = true;
        }

        if (game.getMode().checkRoundOver(world, alivePlayers)) {
            winner = true;
            gameOverWait = D6_GAME_OVER_WAIT;

#ifndef D6R_HEADLESS_CORE
            if (!game.isHeadless()) game.getResources().getGameOverSound().play();
#endif
            onRoundEnd();
        }
    }

    void Round::update(Float32 elapsedTime) {
        // Check if there's a winner
        if (!hasWinner()) {
            checkWinner();
        } else {
            gameOverWait = std::max(gameOverWait - elapsedTime, 0.0f);
            if (gameOverWait < (D6_GAME_OVER_WAIT - D6_ROUND_OVER_WAIT)) {
                return;
            }
        }

        for (Player &player : world.getPlayers()) {
            player.updateControllerStatus();
            scriptUpdate(player);
            player.update(world, elapsedTime);
            if (game.getSettings().isGhostEnabled() && !player.isInGame() && !player.isGhost()) {
                player.makeGhost();
            }
        }

        world.update(elapsedTime);
#ifndef D6R_HEADLESS_CORE
        if (!game.isHeadless()) game.getAppService().getVideo().getRenderer().setGlobalTime(world.getTime());
#endif

        if (suddenDeathMode) {
            waterFillWait += elapsedTime;
            if (waterFillWait > D6_RAISE_WATER_WAIT) {
                waterFillWait = 0;
                world.raiseWater();
            }
        }

        showYouAreHere = std::max(showYouAreHere - 3 * elapsedTime, 0.0f);
        if (Math::isAuthoritative()) {
            waterFillWait = Math::quantizeAuthoritative(waterFillWait);
            showYouAreHere = Math::quantizeAuthoritative(showYouAreHere);
            gameOverWait = Math::quantizeAuthoritative(gameOverWait);
        }
    }

#ifndef D6R_HEADLESS_CORE
    void Round::keyEvent(const KeyPressEvent &event) {
        // Turn on/off player statistics
        if (event.getCode() == SDLK_F4) {
            game.getSettings().setShowRanking(!game.getSettings().isShowRanking());
        }

        // Save screenshot
        if (event.getCode() == SDLK_F10) {
            Image image = game.getAppService().getVideo().getRenderer().makeScreenshot();
            std::string name = image.saveScreenshot();
            game.getAppService().getConsole().printLine(Format("Screenshot saved to {0}") << name);
        }
    }
#endif

    bool Round::isOver() const {
        return hasWinner() && gameOverWait <= 0;
    }

    bool Round::isLast() const {
        return game.getSettings().isRoundLimit() && roundNumber + 1 == game.getSettings().getMaxRounds();
    }
}
