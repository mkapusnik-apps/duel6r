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

#include "BonusList.h"
#include "ElevatorList.h"
#include "Game.h"
#include "GameMode.h"
#include "Explosion.h"
#include "gamemodes/DeathMatch.h"
#include "gamemodes/TeamDeathMatch.h"
#include "ArenaPresentation.h"
#include "ArenaScene.h"

namespace Duel6 {
    WorldRenderer::WorldRenderer(Duel6::AppService &appService, const Duel6::Game &game)
            : font(appService.getFont()), video(appService.getVideo()), game(game), renderer(video.getRenderer()),
              target(renderer.makeTarget(video.getScreen())) {}

    void WorldRenderer::setView(const PlayerView &view) const {
        setView(view.getX(), view.getY(), view.getWidth(), view.getHeight());
    }

    void WorldRenderer::setView(int x, int y, int width, int height) const {
#ifdef D6_MACOS_PLATFORM
        // Both the shared arena and its overlays cover the complete drawable.
        // The macOS GL1 target draws directly, without an offscreen framebuffer.
        video.resetDrawableViewport();
#else
        renderer.setViewport(x, y, width, height);
#endif
    }

    void WorldRenderer::background(Texture texture) const {
        Int32 cw = video.getScreen().getClientWidth();
        Int32 ch = video.getScreen().getClientHeight();
        renderer.quadXY(Vector(0, 0), Vector(cw, ch), Vector(0, 1), Vector(1, -1), Material::makeTexture(texture));
    }

    void WorldRenderer::playerRankings() const {
        Float32 fontSize = 16;
        Float32 fontWidth = fontSize / 2;
        Ranking ranking = game.getMode().getRanking(game.getPlayers());
        Int32 maxNameLength = ranking.getMaxLength() + 6;

        const PlayerView &view = game.getPlayers().front().getView();
        Int32 posX = view.getX() + view.getWidth() - fontWidth * maxNameLength - 3;
        Int32 posY = view.getY() + view.getHeight() - 20;

        for (const auto &entry : ranking.entries) {
            posY = renderRankingEntry(entry, posX, posY, maxNameLength, fontSize, false);
            for (const auto &nestedRankingEntry : entry.entries) {
                posY = renderRankingEntry(nestedRankingEntry, posX, posY, maxNameLength, fontSize, false);
            }
        }
    }

    Int32 WorldRenderer::renderRankingEntry(const Ranking::Entry &entry, Int32 posX, Int32 posY, Int32 maxLength,
                                            Float32 charHeight, bool extended) const {
        if (extended) {
            maxLength += 20;
        }
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        Color fontColor(entry.fontColor);
        renderer.quadXY(Vector(posX, posY + 1), Vector((charHeight / 2) * maxLength, charHeight), entry.bcgColor);

        Int32 paddingLeft = entry.isSuperEntry() ? 0 : 5;
        renderer.setBlendFunc(BlendFunc::None);
        font.print(posX + paddingLeft, posY, 0.0f, fontColor, entry.name, charHeight);

        if (extended) {
            renderer.setBlendFunc(BlendFunc::SrcColor);
            if (!entry.isSuperEntry()) {
                fontColor = Color::YELLOW;
            }
            auto killsToDeaths = Person::getKillsToDeathsRatio(entry.kills, entry.deaths);
            font.print(posX + (charHeight / 2) * (maxLength - 23), posY, 0.0f, fontColor,
                       (Format("|{0,3}|{1,3}|{2,3}|{3,5}|{4,4}") << entry.kills << entry.assistances << entry.deaths
                                                                 << killsToDeaths << entry.points), charHeight);
        } else {
            font.print(posX + (charHeight / 2) * (maxLength - 5), posY, 0.0f, fontColor,
                       (Format("|{0,4}") << entry.points), charHeight);
        }
        return posY - charHeight;
    }

    void WorldRenderer::roundOverSummary(bool showRoundProgress, bool separateTeamGroups, bool clampPanelBounds,
                                          Int32 minimumPanelBottom, Int32 maximumPanelTop) const {
        Float32 fontSize = 32;
        Float32 fontWidth = fontSize / 2;
        Ranking ranking = game.getMode().getRanking(game.getPlayers());
        const bool showTeamSeparators =
                separateTeamGroups && dynamic_cast<const TeamDeathMatch *>(&game.getMode()) != nullptr;
        const Int32 teamSeparatorHeight = 8;
        Int32 maxLength = ranking.getMaxLength() + 6;
        Int32 maxNameLength = maxLength + 20;
        int height = fontSize * 3; // reserve for 'SCORE'
        int tableWidth = maxNameLength * (fontWidth);
        int width = std::max(tableWidth, 200);
        for (const auto &entry : ranking.entries) {
            height += fontSize * (1 + entry.entries.size());
        }
        if (showTeamSeparators && ranking.entries.size() > 1) {
            height += teamSeparatorHeight * Int32(ranking.entries.size() - 1);
        }
        const auto score = "---SCORE---";
        const auto kad = " K    A   D  K/D  PTS  ";
        const auto kadWidth = font.getTextWidth(kad, fontSize);
        const auto scoreWidth = font.getTextWidth(score, fontSize);
        std::string roundProgress;
        Float32 roundProgressWidth = 0;
        if (showRoundProgress) {
            roundProgress = Format("Rounds: {0}|{1}") << game.getPlayedRounds() << game.getSettings().getMaxRounds();
            roundProgressWidth = font.getTextWidth(roundProgress, fontSize);
            width = std::max(width, Int32(roundProgressWidth));
            height += fontSize;
        }

        int x = video.getScreen().getClientWidth() / 2 - width / 2;
        int y = video.getScreen().getClientHeight() / 2 - height / 2;
        const Int32 panelBottom = y - Int32(fontSize);
        const Int32 panelHeight = height + 2 * Int32(fontSize);
        if (clampPanelBounds && panelBottom < minimumPanelBottom) {
            y += minimumPanelBottom - panelBottom;
        }
        const Int32 panelTop = y + height + Int32(fontSize);
        if (clampPanelBounds && panelHeight <= maximumPanelTop - minimumPanelBottom && panelTop > maximumPanelTop) {
            y -= panelTop - maximumPanelTop;
        }

        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(x - fontWidth, y - fontSize), Vector(width + 2 * fontWidth, height + 2 * fontSize),
                        Color(255, 255, 255, 80));
        renderer.quadXY(Vector(x - fontWidth + 2, y - fontSize + 2),
                        Vector(width + 2 * fontWidth - 4, height + 2 * fontSize - 4), Color(0, 0, 255, 80));

        Float32 scoreY = y + height - fontSize * (showRoundProgress ? 2 : 1);
        renderer.quadXY(Vector(x - fontWidth - 5, scoreY),
                         Vector(width + 2 * fontWidth + 10, fontSize + 4), Color(0, 0, 255, 255));
        renderer.setBlendFunc(BlendFunc::SrcColor);

        Int32 posX = video.getScreen().getClientWidth() / 2 - tableWidth / 2;
        Int32 posY = scoreY - fontSize * 2;

        Color fontColor = Color::WHITE;

        font.print(x + (width - scoreWidth) / 2, scoreY, 0.0f, fontColor, score, fontSize);
        if (showRoundProgress) {
            font.print(x + width - roundProgressWidth, scoreY + fontSize, 0.0f, fontColor, roundProgress, fontSize);
        }
        font.print(posX + tableWidth - kadWidth, scoreY - fontSize, 0.0f, fontColor, kad, fontSize);
        for (Size index = 0; index < ranking.entries.size(); index++) {
            const auto &entry = ranking.entries[index];
            posY = renderRankingEntry(entry, posX, posY, maxLength, fontSize, true);
            for (const auto &nestedRankingEntry : entry.entries) {
                posY = renderRankingEntry(nestedRankingEntry, posX, posY, maxLength, fontSize, true);
            }
            if (showTeamSeparators && index + 1 < ranking.entries.size()) {
                posY -= teamSeparatorHeight;
                renderer.setBlendFunc(BlendFunc::SrcAlpha);
                renderer.quadXY(Vector(posX, posY + Int32(fontSize) + 4), Vector(tableWidth, 2),
                                Color(255, 255, 255, 178));
            }
        }
    }

    void WorldRenderer::gameOverSummary() const {
        const bool teamDeathMatch = dynamic_cast<const TeamDeathMatch *>(&game.getMode()) != nullptr;
        const bool deathMatch = dynamic_cast<const DeathMatch *>(&game.getMode()) != nullptr;
        if (!deathMatch && !teamDeathMatch) {
            roundOverSummary(false, false);
            return;
        }

        const std::string notice = "End of Game";
        const Int32 fontSize = 32;
        const Int32 horizontalPadding = 16;
        const Int32 verticalPadding = 8;
        const Int32 bottomInset = 16;
        const Int32 panelGap = 16;
        const Int32 noticeHeight = fontSize + 2 * verticalPadding;
        const Float32 noticeTextWidth = font.getTextWidth(notice, fontSize);
        const Float32 noticeWidth = noticeTextWidth + 2 * horizontalPadding;
        const Float32 noticeX = video.getScreen().getClientWidth() / 2.0f - noticeWidth / 2.0f;
        const Int32 counterBottom = finalRoundCounter();

        roundOverSummary(false, teamDeathMatch, true, bottomInset + noticeHeight + panelGap, counterBottom - panelGap);
        // Keep counter text above an oversized score panel's intersecting backing surface.
        finalRoundCounter();

        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(noticeX, Float32(bottomInset)), Vector(noticeWidth, Float32(noticeHeight)),
                        Color(0, 0, 255, 255));
        renderer.setBlendFunc(BlendFunc::None);
        font.print(noticeX + horizontalPadding, bottomInset + verticalPadding, 0.0f, Color::WHITE, notice, fontSize);
    }

    void WorldRenderer::roundsPlayed() const {
        int width = 134;
        int x = video.getScreen().getClientWidth() / 2 - width / 2;
        int y = video.getScreen().getClientHeight() - 20;

        renderer.quadXY(Vector(x - 1, y - 1), Vector(width + 2, 18), Color::BLACK);
        font.print(x + 8, y, Color::WHITE,
                   Format("Rounds: {0,3}|{1,3}") << game.getCurrentRound() + 1 << game.getSettings().getMaxRounds());
    }

    Int32 WorldRenderer::finalRoundCounter() const {
        const std::string counter = Format("Rounds: {0,3}|{1,3}") << game.getCurrentRound() + 1
                                                                  << game.getSettings().getMaxRounds();
        const Int32 fontSize = 32;
        const Int32 horizontalPadding = 8;
        const Int32 verticalPadding = 2;
        const Int32 topInset = 2;
        const Int32 width = font.getTextWidth(counter, fontSize) + 2 * horizontalPadding;
        const Int32 height = fontSize + 2 * verticalPadding;
        const Int32 x = video.getScreen().getClientWidth() / 2 - width / 2;
        const Int32 y = video.getScreen().getClientHeight() - topInset - height;

        renderer.quadXY(Vector(x, y), Vector(width, height), Color::BLACK);
        font.print(x + horizontalPadding, y + verticalPadding, 0.0f, Color::WHITE, counter, fontSize);
        return y;
    }

    void WorldRenderer::fpsCounter() const {
        std::string fpsCount = Format("FPS - {0}") << Int32(video.getFps());
        Int32 width = 8 * Int32(fpsCount.size()) + 2;

        Int32 x = Int32(video.getScreen().getClientWidth()) - width;
        Int32 y = Int32(video.getScreen().getClientHeight()) - 20;

        renderer.quadXY(Vector(x - 1, y - 1), Vector(width + 2, 18), Color::BLACK);
        font.print(x, y, Color::WHITE, fpsCount);
    }

    void WorldRenderer::screenCurtain(const Color &color) const {
        const auto &screen = video.getScreen();

        video.setMode(Video::Mode::Orthogonal);
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector::ZERO, Vector(screen.getClientWidth(), screen.getClientHeight()), color);
        renderer.setBlendFunc(BlendFunc::None);
        video.setMode(Video::Mode::Perspective);
    }

    void WorldRenderer::infoMessages() const {
        const InfoMessageQueue &messageQueue = game.getRound().getWorld().getMessageQueue();

        messageQueue.renderAllMessages(renderer, game.getPlayers().front().getView(), 20, font);
    }

    void WorldRenderer::shotCollisionBox(const ShotList &shotList) const {
        shotList.forEach([this](const Shot &shot) -> bool {
            const auto &rect = shot.getCollisionRect();
            renderer.frame(Vector(rect.left.x, rect.left.y, 0.6f), rect.getSize(), 1.0f, Color::RED);
            return true;
        });
    }

    void WorldRenderer::setPlayerCamera(const Player &player) const {
        const Camera &camera = player.getCamera();
        Matrix viewMatrix = Matrix::lookAt(camera.getPosition(), camera.getFront(), camera.getUp());
        renderer.setViewMatrix(viewMatrix);
    }

    void WorldRenderer::renderStaticGeometry() const {
        if (game.getSettings().isWireframe()) {
            renderer.enableWireframe(true);
        }

        const World &world = game.getRound().getWorld();
        const auto &geometry = world.getLevelRenderData();
        ArenaPresentation::staticGeometry(renderer, {geometry.getWalls(), geometry.getSprites(), geometry.getWater(),
                game.getResources().getBlockTextures(), world.getTime()});

        renderer.enableWireframe(false);
    }

    void WorldRenderer::renderBackground() const {
        const Player &player = game.getPlayers().front();
        setView(player.getView());
        background(game.getResources().getBcgTextures().at(game.getRound().getWorld().getBackground()));
        video.setMode(Video::Mode::Perspective);

        setPlayerCamera(player);
        renderStaticGeometry();

        video.setMode(Video::Mode::Orthogonal);
    }

    void WorldRenderer::view() const {
        const World &world = game.getRound().getWorld();
        const auto &geometry = world.getLevelRenderData();
        ArenaScene scene{{geometry.getWalls(), geometry.getSprites(), geometry.getWater(),
                game.getResources().getBlockTextures(), world.getTime()}};
        scene.elevatorTexture = game.getResources().getElevatorTextures();
        scene.bonusTexture = game.getResources().getBonusTextures();
        scene.explosionTexture = game.getResources().getExplosionTextures();
        for (const auto &elevator: world.getElevatorList().values()) scene.elevators.push_back(elevator.getPosition());
        const auto &bonuses = world.getBonusList();
        scene.bonuses.assign(bonuses.getBonuses().begin(), bonuses.getBonuses().end());
        scene.weapons.assign(bonuses.getWeapons().begin(), bonuses.getWeapons().end());
        scene.sprites.assign(world.getSpriteList().values().begin(), world.getSpriteList().values().end());
        scene.explosions.assign(world.getExplosionList().values().begin(), world.getExplosionList().values().end());
        for (const auto &actor: game.getPlayers()) {
            ArenaPlayerHud hud;
            hud.centre = actor.getCentre(); hud.dimensions = actor.getDimensions();
            hud.name = actor.getPerson().getName(); hud.ammunition = actor.getAmmo(); hud.kills = actor.getRoundKills();
            hud.life = actor.getLife(); hud.air = actor.getAir();
            hud.reload = actor.getReloadTime(); hud.reloadTotal = actor.getReloadInterval();
            hud.bonus = actor.getBonusRemainingTime(); hud.bonusTotal = actor.getBonusDuration();
            hud.bonusTexture = actor.getBonus() == BonusType::NONE ? -1 : actor.getBonus()->getTextureIndex();
            hud.indicators = actor.getIndicators(); hud.alive = actor.isAlive(); hud.invulnerable = actor.isInvulnerable();
            hud.locationRemaining = game.getRound().getRemainingYouAreHere();
            scene.players.push_back(std::move(hud));
        }
        ArenaRenderer::render(renderer, font, scene);
    }

    Color WorldRenderer::getGameOverOverlay() const {
        Float32 overlay = game.getRound().getRemainingGameOverWait() / D6_GAME_OVER_WAIT;
        return Color(128, 0, 0, Uint8(200 - 200 * overlay));
    }

    Color WorldRenderer::getRoundStartFadeColor(Float32 remainingTime) const {
        if (remainingTime <= 0) {
            return Color::WHITE;
        }
        Float32 intensity = 4 * (remainingTime / D6_YOU_ARE_HERE_DURATION);

        Uint8 c = 255.0f * std::max(1.0f - intensity, 0.0f);
        Color result = Color(c, c, 100 + 155.0f * std::max(1.0f - intensity, 0.0f)); //Blue darkness
        return result;
    }

    void WorldRenderer::sharedArena() const {
        const Player &player = game.getPlayers().front();
        Float32 remainingTime = game.getRound().getRemainingYouAreHere();
        setView(player.getView());

        renderer.clearBuffers(); // attempt to resolve rendering issues in Alcatraz
        Color fadeColor = remainingTime > 0 ? getRoundStartFadeColor(remainingTime) : Color::WHITE;
        target->apply(fadeColor);

        renderer.enableDepthTest(true);
        video.setMode(Video::Mode::Perspective);
        setPlayerCamera(player);
        view();

        if (game.getRound().hasWinner()) {
            Color overlayColor = getGameOverOverlay();
            screenCurtain(overlayColor);
        }
    }

    void WorldRenderer::prerender() const {
        target->record([this]() {
            renderBackground();
        });
    }

    void WorldRenderer::render() const {
        const GameSettings &settings = game.getSettings();
        const bool showRoundSummaryProgress = game.getRound().hasWinner() && !game.isOver() &&
                                              settings.isRoundLimit() && !game.getRound().isLast();

        sharedArena();

        video.setMode(Video::Mode::Orthogonal);
        setView(0, 0, video.getScreen().getClientWidth(), video.getScreen().getClientHeight());

        infoMessages();

        if (settings.isShowFps()) {
            fpsCounter();
        }

        if (settings.isShowRanking()) {
            playerRankings();
        }

        const bool localizedFinalSummary = game.getRound().hasWinner() && game.getRound().isLast() &&
                                           (dynamic_cast<const DeathMatch *>(&game.getMode()) != nullptr ||
                                            dynamic_cast<const TeamDeathMatch *>(&game.getMode()) != nullptr);
        if (settings.isRoundLimit() && !showRoundSummaryProgress && !localizedFinalSummary) {
            roundsPlayed();
        }

        if (game.getRound().hasWinner()) {
            if (game.getRound().isLast()) {
                gameOverSummary();
            } else {
                roundOverSummary(showRoundSummaryProgress, true);
            }
        } else if (game.isDisplayingScoreTab()) {
            roundOverSummary(false, true);
        }
    }
}
