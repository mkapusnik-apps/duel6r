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
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <stdlib.h>
#include "Sound.h"
#include "Video.h"
#include "input/PlayerControls.h"
#include "TextureManager.h"
#include "Menu.h"
#include "Game.h"
#include "File.h"
#include "Font.h"
#include "json/JsonParser.h"
#include "json/JsonWriter.h"
#include "GameMode.h"
#include "gamemodes/DeathMatch.h"
#include "gamemodes/TeamDeathMatch.h"
#include "gamemodes/Predator.h"
#include "Exception.h"

#define D6_ALL_CHR  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 -=\\~!@#$%^&*()_+|[];',./<>?:{}"
#define D6_NUM_CHR  "0123456789"
#define D6_MENU_WIDTH 850
#define D6_MENU_HEIGHT 700
#define D6_MENU_MAX_SCALE 1.35f
#define D6_MENU_MESSAGE_MAX_WIDTH 790

namespace Duel6 {
    namespace {
        Image coverImage(const Image &source, Size width, Size height) {
            Image result(width, height);
            Float32 sourceAspect = Float32(source.getWidth()) / Float32(source.getHeight());
            Float32 targetAspect = Float32(width) / Float32(height);
            Float32 cropWidth = targetAspect > sourceAspect ? Float32(source.getWidth())
                                                            : Float32(source.getHeight()) * targetAspect;
            Float32 cropHeight = targetAspect > sourceAspect ? Float32(source.getWidth()) / targetAspect
                                                             : Float32(source.getHeight());
            Float32 cropX = (Float32(source.getWidth()) - cropWidth) * 0.5f;
            Float32 cropY = (Float32(source.getHeight()) - cropHeight) * 0.5f;

            for (Size y = 0; y < height; y++) {
                Size sourceY = std::min(source.getHeight() - 1,
                                        Size(cropY + (Float32(y) + 0.5f) * cropHeight / Float32(height)));
                for (Size x = 0; x < width; x++) {
                    Size sourceX = std::min(source.getWidth() - 1,
                                            Size(cropX + (Float32(x) + 0.5f) * cropWidth / Float32(width)));
                    const Color &pixel = source.at(sourceY * source.getWidth() + sourceX);
                    result.at(y * width + x) = Color(pixel.getRed(), pixel.getGreen(), pixel.getBlue(), 255);
                }
            }
            return result;
        }

        Image boxBlur(const Image &source, Int32 radius) {
            Size width = source.getWidth();
            Size height = source.getHeight();
            Image horizontal(width, height);
            Image result(width, height);
            Int32 sampleCount = radius * 2 + 1;

            for (Size y = 0; y < height; y++) {
                Int32 red = 0, green = 0, blue = 0;
                for (Int32 offset = -radius; offset <= radius; offset++) {
                    Size x = Size(std::max<Int32>(0, std::min<Int32>(Int32(width) - 1, offset)));
                    const Color &pixel = source.at(y * width + x);
                    red += pixel.getRed(); green += pixel.getGreen(); blue += pixel.getBlue();
                }
                for (Size x = 0; x < width; x++) {
                    horizontal.at(y * width + x) = Color(red / sampleCount, green / sampleCount, blue / sampleCount);
                    Size removeX = Size(std::max<Int32>(0, Int32(x) - radius));
                    Size addX = Size(std::min<Int32>(Int32(width) - 1, Int32(x) + radius + 1));
                    const Color &remove = source.at(y * width + removeX);
                    const Color &add = source.at(y * width + addX);
                    red += add.getRed() - remove.getRed();
                    green += add.getGreen() - remove.getGreen();
                    blue += add.getBlue() - remove.getBlue();
                }
            }

            for (Size x = 0; x < width; x++) {
                Int32 red = 0, green = 0, blue = 0;
                for (Int32 offset = -radius; offset <= radius; offset++) {
                    Size y = Size(std::max<Int32>(0, std::min<Int32>(Int32(height) - 1, offset)));
                    const Color &pixel = horizontal.at(y * width + x);
                    red += pixel.getRed(); green += pixel.getGreen(); blue += pixel.getBlue();
                }
                for (Size y = 0; y < height; y++) {
                    result.at(y * width + x) = Color(red / sampleCount, green / sampleCount, blue / sampleCount);
                    Size removeY = Size(std::max<Int32>(0, Int32(y) - radius));
                    Size addY = Size(std::min<Int32>(Int32(height) - 1, Int32(y) + radius + 1));
                    const Color &remove = horizontal.at(removeY * width + x);
                    const Color &add = horizontal.at(addY * width + x);
                    red += add.getRed() - remove.getRed();
                    green += add.getGreen() - remove.getGreen();
                    blue += add.getBlue() - remove.getBlue();
                }
            }
            return result;
        }

        Image blurMenuBackground(const Image &source) {
            Image result = source;
            for (Int32 pass = 0; pass < 3; pass++) {
                result = boxBlur(result, 12);
            }
            return result;
        }

        std::vector<std::string> wrapMessage(const std::string &message, Size maxCharacters) {
            if (message.size() <= maxCharacters) {
                return {message};
            }
            std::istringstream words(message);
            std::vector<std::string> lines;
            std::string line;
            std::string word;
            while (words >> word) {
                if (!line.empty() && line.size() + word.size() + 1 > maxCharacters) {
                    lines.push_back(line);
                    line.clear();
                }
                if (!line.empty()) line += " ";
                line += word;
            }
            if (!line.empty()) lines.push_back(line);
            return lines;
        }
    }

    Menu::Menu(AppService &appService)
            : appService(appService), font(appService.getFont()), video(appService.getVideo()),
              renderer(video.getRenderer()), sound(appService.getSound()), gui(video.getRenderer()),
              controlsManager(appService.getControlsManager()),
              defaultPlayerSounds(PlayerSounds::makeDefault(sound)), menuBackgroundTexture(Texture()),
              hasMenuBackground(false), menuBackgroundPreparationActive(false), menuBackgroundFinished(false),
              menuBackgroundInitialFrameRendered(false), menuScale(1.0f), menuTranslationX(0),
              menuTranslationY(0), playMusic(false) {}

    Menu::~Menu() {
        if (menuBackgroundPreparation.valid()) {
            try {
                menuBackgroundPreparation.wait();
                menuBackgroundPreparation.get();
            } catch (...) {
                // Optional background work must not interfere with application teardown.
            }
        }
        if (hasMenuBackground) {
            renderer.freeTexture(menuBackgroundTexture);
        }
    }

    void Menu::loadPersonData(const std::string &filePath) {
        if (!File::exists(filePath)) {
            return;
        }

        personListBox->clear();
        playerListBox->clear();

        Json::Parser parser;
        Json::Value json = parser.parse(filePath);
        persons.fromJson(json.get("persons"), personProfiles);

        for (const Person &person : persons.list()) {
            personListBox->addItem(person.getName());
        }

        Json::Value playing = json.get("playing");
        for (Size i = 0; i < playing.getLength(); i++) {
            std::string name = playing.get(i).asString();
            playerListBox->addItem(name);
            personListBox->removeItem(name);
        }

        updatePlayerCount();

        Int32 playedRounds = json.getOrDefault("rounds", Json::Value::makeNumber(0)).asInt();
        game->setPlayedRounds(playedRounds);
    }

    void Menu::joyRescan() {
        controlsManager.detectJoypads();

        Size controls = controlsManager.getNumAvailable();
        for (Size i = 0; i < D6_MAX_PLAYERS; i++) {
            Gui::Spinner *control = controlSwitch[i];
            for (Size j = 0; j < controls; j++) {
                control->addItem(controlsManager.get(j).getDescription(), j);
            }
        }
    }

    void Menu::initialize() {
        appService.getConsole().printLine("\n===Menu initialization===");
        menuBannerTexture = appService.getTextureManager().loadStack(D6_TEXTURE_MENU_PATH, TextureFilter::Linear, true);
        initializePresentation();
        appService.getConsole().printLine("...Starting GUI library");
        gui.screenSize(video.getScreen().getClientWidth(), video.getScreen().getClientHeight(),
                       D6_MENU_WIDTH, D6_MENU_HEIGHT,
                       menuTranslationX, menuTranslationY, menuScale);

        auto eloPanel = new Gui::Panel(gui);
        eloPanel->setPosition(10, 578, 185, 326);
        eloPanel->setCaption("ELO SCOREBOARD");

        auto personsPanel = new Gui::Panel(gui);
        personsPanel->setPosition(200, 578, 185, 326);
        personsPanel->setCaption("PERSONS");

        playersPanel = new Gui::Panel(gui);
        playersPanel->setPosition(390, 578, 255, 326);

        auto gameSettingsPanel = new Gui::Panel(gui);
        gameSettingsPanel->setPosition(650, 578, 190, 326);
        gameSettingsPanel->setCaption("GAME SETTINGS");

        scoreListBox = new Gui::ListBox(gui, true);
        scoreListBox->setPosition(10, 222, 101, 8, 16);

        personListBox = new Gui::ListBox(gui, true);
        personListBox->setPosition(204, 553, 20, 12, 18);
        personListBox->onDoubleClick([this](Int32 index, const std::string &item) {
            addPlayer(index);
        });

        playerListBox = new Gui::ListBox(gui, false);
        playerListBox->setPosition(394, 553, 10, D6_MAX_PLAYERS, 18);
        playerListBox->onDoubleClick([this](Int32 index, const std::string &item) {
            removePlayer(index);
        });

        eloListBox = new Gui::ListBox(gui, true);
        eloListBox->setPosition(14, 553, 20, 15, 18);

        loadPersonProfiles(D6_FILE_PROFILES);

        auto addPlayerButton = new Gui::Button(gui);
        addPlayerButton->setPosition(294, 301, 35, 25);
        addPlayerButton->setCaption(">>");
        addPlayerButton->onClick([this](Gui::Button &) {
            addPlayer(personListBox->selectedIndex());
        });

        auto removePlayerButton = new Gui::Button(gui);
        removePlayerButton->setPosition(257, 301, 35, 25);
        removePlayerButton->setCaption("<<");
        removePlayerButton->onClick([this](Gui::Button &) {
            removePlayer(playerListBox->selectedIndex());
        });

        auto removePersonButton = new Gui::Button(gui);
        removePersonButton->setPosition(204, 301, 51, 25);
        removePersonButton->setCaption("Remove");
        removePersonButton->onClick([this](Gui::Button &) {
            deletePerson();
            rebuildTable();
        });

        auto addPersonButton = new Gui::Button(gui);
        addPersonButton->setPosition(331, 301, 50, 25);
        addPersonButton->setCaption("Add");
        addPersonButton->onClick([this](Gui::Button &) {
            addPerson();
        });

        auto playButton = new Gui::Button(gui);
        playButton->setPosition(50, 70, 150, 50);
        playButton->setCaption("Play (F1)");
        playButton->onClick([this](Gui::Button &) {
            play();
        });

        auto clearButton = new Gui::Button(gui);
        clearButton->setPosition(350, 70, 150, 50);
        clearButton->setCaption("Clear (F3)");
        clearButton->onClick([this](Gui::Button &) {
            if (deleteQuestion()) {
                cleanPersonData();
            }
        });

        auto quitButton = new Gui::Button(gui);
        quitButton->setPosition(650, 70, 150, 50);
        quitButton->setCaption("Quit (ESC)");
        quitButton->onClick([this](Gui::Button &) {
            close();
        });

        auto scoreLabel = new Gui::Label(gui);
        scoreLabel->setPosition(10, 243, 830, 18);
        scoreLabel->setCaption(
                "    Name   |   Elo | Pts | Win | Kill | Assist | Pen | Death |  K/D | Shot | Acc. | GmTm |  Dmg ");

        updatePlayerCount();

        Gui::Button *shuffleButton = new Gui::Button(gui);
        shuffleButton->setCaption("S");
        shuffleButton->setPosition(413, 274, 17, 17);
        shuffleButton->onClick([this](Gui::Button &) {
            shufflePlayers();
        });

        Gui::Button *eloShuffleButton = new Gui::Button(gui);
        eloShuffleButton->setCaption("E");
        eloShuffleButton->setPosition(394, 274, 17, 17);
        eloShuffleButton->onClick([this](Gui::Button &) {
            eloShufflePlayers();
        });

        textbox = new Gui::Textbox(gui);
        textbox->setPosition(204, 326, 20, 10, D6_ALL_CHR);

        // Player controls
        for (Size i = 0; i < D6_MAX_PLAYERS; i++) {
            controlSwitch[i] = new Gui::Spinner(gui);
            controlSwitch[i]->setPosition(478, 553 - Int32(i) * 18, 142, 0);

            Gui::Button *button = new Gui::Button(gui);
            button->setCaption("D");
            button->setPosition(623, 552 - Int32(i) * 18, 17, 17);
            button->onClick([this, i](Gui::Button &) {
                detectControls(i);
            });
        }

        // Button to detect all user's controllers in a batch
        Gui::Button *button = new Gui::Button(gui);
        button->setCaption("D");
        button->setPosition(623, 274, 17, 17);
        button->onClick([this](Gui::Button &) {
            joyRescan();
            Size curPlayersCount = playerListBox->size();
            for (Size j = 0; j < curPlayersCount; j++) {
                detectControls(j);
            }
        });

        initializeGameModes();
        gameModeSwitch = new Gui::Spinner(gui);
        for (Size i = 0; i < gameModes.size(); i++) {
            if (i < 2) {
                gameModeSwitch->addItem(gameModes[i]->getName());
            } else {
                gameModeSwitch->addItem(Format("{0} teams, FF: {1}") << (1 + i / 2) << (i % 2 ? "on" : "off"));
            }
        }
        gameModeSwitch->setPosition(654, 540, 182, 20);
        gameModeSwitch->onToggled([this](Int32 selectedIndex) {
            if (selectedIndex < 2) {
                playerListBox->onColorize(Gui::ListBox::defaultColorize);
            } else {
                Int32 teamCount = 1 + selectedIndex / 2;
                playerListBox->onColorize([teamCount](Int32 index, const std::string &label) {
                    return Gui::ListBox::ItemColor{Color::BLACK, TEAMS[index % teamCount].color};
                });
            }
        });

        globalAssistanceCheckBox = new Gui::CheckBox(gui, true);
        globalAssistanceCheckBox->setLabel("Assistance");
        globalAssistanceCheckBox->setPosition(654, 510, 170, 20);

        quickLiquidCheckBox = new Gui::CheckBox(gui, true);
        quickLiquidCheckBox->setLabel("Quick Liquid");
        quickLiquidCheckBox->setPosition(654, 482, 170, 20);

        burnableTreesCheckBox = new Gui::CheckBox(gui, game->getSettings().isBurnableTrees());
        burnableTreesCheckBox->setLabel("Burnable Trees");
        burnableTreesCheckBox->setPosition(654, 454, 170, 20);

        roundsTextbox = new Gui::Textbox(gui);
        roundsTextbox->setLabel("Rounds");
        roundsTextbox->setLabelLeft(true);
        roundsTextbox->setPosition(792, 424, 4, 4, D6_NUM_CHR);
        updateRoundsTextbox();

        backgroundCount = File::countFiles(D6_TEXTURE_BCG_PATH);
        levelList.initialize(D6_FILE_LEVEL, D6_LEVEL_EXTENSION);

        menuTrack = sound.loadModule("sound/undead.xm");
        startMenuBackgroundPreparation({}, true);
    }

    void Menu::initializePresentation() {
        Int32 clientWidth = video.getScreen().getClientWidth();
        Int32 clientHeight = video.getScreen().getClientHeight();
        menuScale = std::min(D6_MENU_MAX_SCALE,
                             std::min(Float32(clientWidth) / D6_MENU_WIDTH,
                                      Float32(clientHeight) / D6_MENU_HEIGHT));
        menuTranslationX = Int32((Float32(clientWidth) - D6_MENU_WIDTH * menuScale) * 0.5f);
        menuTranslationY = Int32((Float32(clientHeight) - D6_MENU_HEIGHT * menuScale) * 0.5f);
        appService.getConsole().printLine(Format("Menu presentation: scale={0}, origin=({1},{2})")
                                          << menuScale << menuTranslationX << menuTranslationY);
    }

    Menu::PreparedMenuBackground Menu::prepareMenuBackground(Int32 clientWidth, Int32 clientHeight,
                                                             std::vector<std::string> candidates,
                                                             bool discoverCandidates) {
        PreparedMenuBackground result;
        if (discoverCandidates) {
            try {
                candidates = File::listDirectory(D6_TEXTURE_MENU_BACKGROUND_PATH);
                candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [](const std::string &name) {
                    Size dot = name.find_last_of('.');
                    if (dot == std::string::npos) return true;
                    std::string extension = name.substr(dot);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    return extension != ".png" && extension != ".jpg" && extension != ".jpeg";
                }), candidates.end());
                std::shuffle(candidates.begin(), candidates.end(), std::mt19937(std::random_device{}()));
            } catch (...) {
                result.directoryAvailable = false;
                return result;
            }
        }

        while (!candidates.empty()) {
            std::string candidate = candidates.front();
            candidates.erase(candidates.begin());
            try {
                Image source = Image::load(D6_TEXTURE_MENU_BACKGROUND_PATH + candidate);
                if (source.getWidth() == 0 || source.getHeight() == 0) {
                    result.failedCandidates.push_back(candidate);
                    continue;
                }
                Image covered = coverImage(source, clientWidth, clientHeight);
                result.image = blurMenuBackground(covered);
                result.filename = candidate;
                result.remainingCandidates = std::move(candidates);
                result.hasImage = true;
                return result;
            } catch (...) {
                result.failedCandidates.push_back(candidate);
            }
        }
        return result;
    }

    void Menu::startMenuBackgroundPreparation(std::vector<std::string> candidates,
                                              bool discoverCandidates) const noexcept {
        try {
            if (menuBackgroundFinished || menuBackgroundPreparationActive) {
                return;
            }
            Int32 clientWidth = video.getScreen().getClientWidth();
            Int32 clientHeight = video.getScreen().getClientHeight();
            menuBackgroundPreparation = std::async(std::launch::async,
                                                   [clientWidth, clientHeight,
                                                    candidates = std::move(candidates),
                                                    discoverCandidates]() mutable {
                return prepareMenuBackground(clientWidth, clientHeight, std::move(candidates),
                                             discoverCandidates);
            });
            menuBackgroundPreparationActive = true;
        } catch (...) {
            menuBackgroundFinished = true;
            printMenuBackgroundDiagnostic("Menu background worker unavailable; using solid black.");
        }
    }

    void Menu::printMenuBackgroundDiagnostic(const char *message) const noexcept {
        try {
            appService.getConsole().printLine(message);
        } catch (...) {
            // Optional diagnostics must never interfere with menu rendering.
        }
    }

    void Menu::printMenuBackgroundDiagnostic(const char *prefix, const std::string &value,
                                             const char *suffix) const noexcept {
        try {
            appService.getConsole().printLine(std::string(prefix) + value + suffix);
        } catch (...) {
            // Optional diagnostics must never interfere with menu rendering.
        }
    }

    void Menu::freeOptionalTexture(Texture texture) const noexcept {
        if (texture == Texture()) {
            return;
        }
        try {
            renderer.freeTexture(texture);
        } catch (...) {
            // Cleanup remains best-effort for an optional visual enhancement.
        }
    }

    void Menu::retryPreparedMenuBackground(PreparedMenuBackground &prepared) const noexcept {
        if (prepared.remainingCandidates.empty()) {
            menuBackgroundFinished = true;
            printMenuBackgroundDiagnostic("No menu background could be loaded; using solid black.");
            return;
        }
        startMenuBackgroundPreparation(std::move(prepared.remainingCandidates), false);
    }

    void Menu::publishPreparedMenuBackground() const noexcept {
        try {
            publishPreparedMenuBackgroundTransaction();
        } catch (...) {
            menuBackgroundPreparationActive = false;
            menuBackgroundFinished = true;
            printMenuBackgroundDiagnostic("Menu background processing failed; using solid black.");
        }
    }

    void Menu::publishPreparedMenuBackgroundTransaction() const {
        PreparedMenuBackground prepared;
        Texture texture = Texture();
        try {
            if (!menuBackgroundPreparationActive || !menuBackgroundPreparation.valid()) {
                return;
            }
            if (menuBackgroundPreparation.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                return;
            }

            menuBackgroundPreparationActive = false;
            prepared = menuBackgroundPreparation.get();

            for (const std::string &failed : prepared.failedCandidates) {
                printMenuBackgroundDiagnostic("Menu background failed: ", failed, "; trying another.");
            }
            if (!prepared.directoryAvailable) {
                menuBackgroundFinished = true;
                printMenuBackgroundDiagnostic("Menu background directory unavailable; using solid black.");
                return;
            }
            if (!prepared.hasImage) {
                menuBackgroundFinished = true;
                printMenuBackgroundDiagnostic("No menu background could be loaded; using solid black.");
                return;
            }

            texture = renderer.createTexture(prepared.image, TextureFilter::Linear, true);
            if (texture == Texture()) {
                printMenuBackgroundDiagnostic("Menu background upload failed: ", prepared.filename,
                                              "; trying another.");
                retryPreparedMenuBackground(prepared);
                return;
            }

            try {
                menuBackgroundFilename = prepared.filename;
            } catch (...) {
                freeOptionalTexture(texture);
                texture = Texture();
                printMenuBackgroundDiagnostic("Menu background publication failed: ", prepared.filename,
                                              "; trying another.");
                retryPreparedMenuBackground(prepared);
                return;
            }

            menuBackgroundTexture = texture;
            texture = Texture();
            hasMenuBackground = true;
            menuBackgroundFinished = true;
            printMenuBackgroundDiagnostic("Menu background selected: ", menuBackgroundFilename, "");
        } catch (...) {
            freeOptionalTexture(texture);
            menuBackgroundPreparationActive = false;
            if (prepared.hasImage) {
                printMenuBackgroundDiagnostic("Menu background publication failed: ", prepared.filename,
                                              "; trying another.");
                retryPreparedMenuBackground(prepared);
            } else {
                menuBackgroundFinished = true;
                printMenuBackgroundDiagnostic("Menu background processing failed; using solid black.");
            }
        }
    }

    void Menu::renderMenuBackground() const {
        Int32 clientWidth = video.getScreen().getClientWidth();
        Int32 clientHeight = video.getScreen().getClientHeight();
        renderer.setViewMatrix(Matrix::IDENTITY);
        renderer.quadXY(Vector::ZERO, Vector(clientWidth, clientHeight), Color::BLACK);
        if (hasMenuBackground) {
            renderer.quadXY(Vector::ZERO, Vector(clientWidth, clientHeight), Vector(0, 1), Vector(1, -1),
                            Material::makeTexture(menuBackgroundTexture));
            renderer.setBlendFunc(BlendFunc::SrcAlpha);
            renderer.quadXY(Vector::ZERO, Vector(clientWidth, clientHeight), Color(0, 0, 0, 140));
            renderer.setBlendFunc(BlendFunc::None);
        }
    }

    void Menu::initializeGameModes() {
        gameModes.push_back(std::make_unique<DeathMatch>());
        gameModes.push_back(std::make_unique<Predator>());
        gameModes.push_back(std::make_unique<TeamDeathMatch>(2, false));
        gameModes.push_back(std::make_unique<TeamDeathMatch>(2, true));
        gameModes.push_back(std::make_unique<TeamDeathMatch>(3, false));
        gameModes.push_back(std::make_unique<TeamDeathMatch>(3, true));
        gameModes.push_back(std::make_unique<TeamDeathMatch>(4, false));
        gameModes.push_back(std::make_unique<TeamDeathMatch>(4, true));
    }

    void Menu::savePersonData() const {
        Json::Value json = Json::Value::makeObject();
        json.set("persons", persons.toJson());

        Json::Value playing = Json::Value::makeArray();
        for (Size i = 0; i < playerListBox->size(); i++) {
            playing.add(Json::Value::makeString(playerListBox->getItem(i)));
        }
        json.set("playing", playing);

        json.set("rounds", Json::Value::makeNumber(game->getPlayedRounds()));

        Json::Writer writer(true);
        writer.writeToFile(D6_FILE_PHIST, json);
    }

    void Menu::rebuildTable() {
        scoreListBox->clear();
        if (persons.isEmpty())
            return;

        std::vector<const Person *> ranking;

        for (const Person &person : persons.list()) {
            if (person.getGames() > 0) {
                ranking.push_back(&person);
            }
        }

        std::sort(ranking.begin(), ranking.end(), [](const Person *left, const Person *right) {
            return left->hasHigherScoreThan(*right);
        });

        for (auto person : ranking) {
            std::string personStat =
                    Format("{0,-11}|{1,5}{2,1} |{3,4} |{4,4} |{5,5} |{6,7} |{7,4} |{8,6} |{9,5} |{10,5} |{11,4}% |{12,4}% |{13,5} ")
                            << person->getName()
                            << person->getElo()
                            << (person->getEloTrend() > 0 ? '+' : (person->getEloTrend() < 0 ? '-' : '='))
                            << person->getTotalPoints()
                            << person->getWins()
                            << person->getKills()
                            << person->getAssistances()
                            << person->getPenalties()
                            << person->getDeaths()
                            << Person::getKillsToDeathsRatio(person->getKills(), person->getDeaths())
                            << person->getShots()
                            << person->getAccuracy()
                            << person->getAliveRatio()
                            << person->getTotalDamage();
            scoreListBox->addItem(personStat);
        }

        std::vector<const Person *> eloRanking;
        for (const Person &person : persons.list()) {
            if (person.getEloGames() > 0) {
                eloRanking.push_back(&person);
            }
        }
        std::sort(eloRanking.begin(), eloRanking.end(), [](const Person *left, const Person *right) {
            return left->getElo() > right->getElo();
        });

        eloListBox->clear();
        Int32 index = 1;
        for (auto person : eloRanking) {
            auto trend = person->getEloTrend();
            auto sign = trend > 0 ? "+" : "-";
            std::string trendStr = trend == 0 ? std::string() : Format("{0}{1}") << sign << std::abs(trend);
            eloListBox->addItem(Format("{0,2|0}{1,-10}{2,4}{3,4}") << index << person->getName() << person->getElo() << trendStr);
            index++;
        }
    }

    void Menu::showMessage(const std::string &message) {
        Size maxCharacters = (D6_MENU_MESSAGE_MAX_WIDTH - 60) / 8;
        std::vector<std::string> lines = wrapMessage(message, maxCharacters);
        Size longestLine = 0;
        for (const std::string &line : lines) longestLine = std::max(longestLine, line.size());
        Int32 width = std::min(D6_MENU_MESSAGE_MAX_WIDTH, Int32(longestLine) * 8 + 60);
        Int32 height = Int32(lines.size()) * 16 + 4;
        Int32 x = (D6_MENU_WIDTH - width) / 2;
        Int32 y = (D6_MENU_HEIGHT - height) / 2;

        renderer.setViewMatrix(Matrix::translate(Float32(menuTranslationX), Float32(menuTranslationY), 0) *
                               Matrix::scale(menuScale, menuScale, 1.0f));
        renderer.quadXY(Vector(x, y), Vector(width, height), Color(255, 204, 204));
        renderer.frame(Vector(x, y), Vector(width, height), 2, Color::BLACK);
        for (Size line = 0; line < lines.size(); line++) {
            font.print(x + 30, y + 2 + Int32(lines.size() - line - 1) * 16, Color::RED, lines[line]);
        }
        renderer.setViewMatrix(Matrix::IDENTITY);
        video.screenUpdate(appService.getConsole(), font);
    }

    bool Menu::question(const std::string &question) {
        showMessage(question);
        SDL_Event event;
        bool answer;

        while (true) {
            if (SDL_PollEvent(&event)) {
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_y) {
                        answer = true;
                        break;
                    } else if (event.key.keysym.sym == SDLK_n) {
                        answer = false;
                        break;
                    }
                }
            }
        }

        consumeInputEvents();
        return answer;
    }

    bool Menu::deleteQuestion() {
        return question("Really delete? (Y/N)");
    }

    void Menu::cleanPersonData() {
        for (Person &person : persons.list()) {
            person.reset();
        }
        rebuildTable();
        savePersonData();
    }

    void Menu::detectControls(Size playerIndex) {
        render();
        if (playerIndex >= playerListBox->size()) {
            return;
        }
        const std::string &name = playerListBox->getItem(playerIndex);
        showMessage("Player " + name + ": Press any control");
        playPlayersSound(name);

        bool detected = false;

        while (!detected) {
            if (processEvents(true)) {
                for (Size i = 0; i < controlsManager.getNumAvailable(); i++) {
                    const PlayerControls &pc = controlsManager.get(i);

                    if ((!pc.getLeft().isJoyPadAxis() && pc.getLeft().isPressed()) ||
                        (!pc.getRight().isJoyPadAxis() && pc.getRight().isPressed()) ||
                        (!pc.getDown().isJoyPadAxis() && pc.getDown().isPressed()) ||
                        (!pc.getUp().isJoyPadAxis() && pc.getUp().isPressed()) ||
                        pc.getShoot().isPressed() ||
                        pc.getPick().isPressed()) {
                        controlSwitch[playerIndex]->setCurrent((Int32) i);
                        detected = true;
                    }
                }
            }
        }

        consumeInputEvents();
    }

    void Menu::playPlayersSound(const std::string &name) {
        Person &person = persons.getByName(name);
        auto profile = getPersonProfile(person.getName());
        if (profile) {
            profile->getSounds().getRandomSample(PlayerSounds::Type::GotHit).play();
        }
    }

    void Menu::play() {
        play(listMaps());
    }

    void Menu::play(std::vector<std::string> levels) {
        if (!validateStartPrerequisites(levels)) {
            return;
        }

        applyRoundsTextbox();

        if (playerListBox->size() < 2) {
            showMessage("Can't play alone ...");
            SDL_Event event;
            while (true) {
                if (SDL_PollEvent(&event)) {
                    break;
                }
            }

            consumeInputEvents();
            return;
        }
        game->getSettings().setQuickLiquid(quickLiquidCheckBox->isChecked());
        game->getSettings().setBurnableTrees(burnableTreesCheckBox->isChecked());
        game->getSettings().setGlobalAssistances(globalAssistanceCheckBox->isChecked());
        if (game->getSettings().isRoundLimit()) {
            if (game->getPlayedRounds() == 0 || game->getPlayedRounds() >= game->getSettings().getMaxRounds() || !question("Resume previous game? (Y/N)")) {
                cleanPersonData();
                game->setPlayedRounds(0);
            }
        } else {
            if (question("Clear statistics? (Y/N)")) {
                cleanPersonData();
                game->setPlayedRounds(0);
            }
        }

        GameMode &selectedMode = *gameModes[gameModeSwitch->currentItem()];

        std::vector<Game::PlayerDefinition> playerDefinitions;
        for (Size i = 0; i < playerListBox->size(); i++) {
            Person &person = persons.getByName(playerListBox->getItem(i));
            auto profile = getPersonProfile(person.getName());
            const PlayerControls &controls = controlsManager.get(controlSwitch[i]->currentValue().first);
            PlayerSkinColors colors = profile ? profile->getSkinColors() : PlayerSkinColors::makeRandom();
            const PlayerSounds &sounds = profile ? profile->getSounds() : defaultPlayerSounds;
            playerDefinitions.push_back(Game::PlayerDefinition(person, colors, sounds, controls));
        }
        selectedMode.initializePlayers(playerDefinitions);


        // Game backgrounds
        std::vector<Size> backgrounds;
        for (Size i = 0; i < backgroundCount; i++) {
            backgrounds.push_back(i);
        }

        // Clear elo trend
        for (auto &person : persons.list()) {
            person.setEloTrend(0);
        }

        // Start
        Context::push(*game);
        game->start(playerDefinitions, levels, backgrounds, selectedMode);
    }

    bool Menu::validateStartPrerequisites(const std::vector<std::string> &levels) {
        std::string message;
        if (levels.empty()) {
            message = "No usable levels loaded.";
        }
        if (game->getSettings().getEnabledWeapons().empty()) {
            if (!message.empty()) {
                message += " ";
            }
            message += "No weapons enabled.";
        }
        if (message.empty()) {
            return true;
        }

        message += " Correct content/configuration, restart the application, then try again. Press any key.";
        render();
        showMessage(message);

        SDL_Event event;
        while (true) {
            if (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    close();
                    return false;
                }
                if (event.type == SDL_KEYDOWN) {
                    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
                    return false;
                }
            } else {
                SDL_Delay(1);
            }
        }
    }

    void Menu::addPlayer(Int32 index) {
        if (index != -1 && playerListBox->size() < D6_MAX_PLAYERS) {
            const std::string &name = personListBox->getItem(index);
            playerListBox->addItem(name);
            personListBox->removeItem(index);
        }
        updatePlayerCount();
    }

    void Menu::removePlayer(Int32 index) {
        if (index != -1) {
            const std::string &playerName = playerListBox->getItem(index);
            personListBox->addItem(playerName);
            playerListBox->removeItem(index);

            for (Int32 i = index; i + 1 < D6_MAX_PLAYERS; i++) {
                controlSwitch[i]->setCurrent(controlSwitch[i + 1]->currentItem());
            }
        }
        updatePlayerCount();
    }

    void Menu::updatePlayerCount() {
        playersPanel->setCaption("PLAYERS " + std::to_string(playerListBox->size()) + "  CONTROLLER");
    }

    void Menu::updateRoundsTextbox() {
        if (roundsTextbox == nullptr) {
            return;
        }

        roundsTextbox->setText(Format("{0}") << game->getSettings().getMaxRounds());
    }

    void Menu::applyRoundsTextbox() {
        const std::string &roundsText = roundsTextbox->getText();
        Int32 maxRounds = roundsText.empty() ? 0 : std::stoi(roundsText);
        game->getSettings().setMaxRounds(maxRounds);
        roundsTextbox->setFocused(false);
        updateRoundsTextbox();
    }

    void Menu::addPerson() {
        if (!textbox->isFocused()) {
            return;
        }

        const std::string &personName = textbox->getText();

        if (!personName.empty() && !persons.contains(personName)) {
            persons.add(Person(personName, nullptr));
            personListBox->addItem(personName);
            rebuildTable();
            textbox->flush();
        }
    }

    void Menu::deletePerson() {
        Int32 index = personListBox->selectedIndex();
        if (index != -1) {
            if (!deleteQuestion())
                return;

            const std::string &playerName = personListBox->selectedItem();
            persons.remove(playerName);
            personListBox->removeItem(playerName);
        }
    }

    void Menu::beforeStart(Context *prevContext) {
        updateRoundsTextbox();
        loadPersonData(D6_FILE_PHIST);
        joyRescan();
        SDL_ShowCursor(SDL_ENABLE);
        SDL_StartTextInput();
        rebuildTable();
        if (playMusic) {
            menuTrack.play(false);
        }
    }

    void Menu::update(Float32 elapsedTime) {
        gui.update(elapsedTime);
    }

    void Menu::render() const {
        if (menuBackgroundInitialFrameRendered) {
            publishPreparedMenuBackground();
        } else {
            menuBackgroundInitialFrameRendered = true;
        }
        renderMenuBackground();
        gui.draw(font);

        renderer.setViewMatrix(Matrix::translate(Float32(menuTranslationX), Float32(menuTranslationY), 0) *
                               Matrix::scale(menuScale, menuScale, 1.0f));

        std::string version = Format("{0} {1}") << "version" << APP_VERSION;
        font.print((D6_MENU_WIDTH - Int32(version.size()) * 8) / 2, 581, Color::BLACK, version);

        Material material = Material::makeTexture(menuBannerTexture);
        renderer.quadXY(Vector(325, 600), Vector(200, 95), Vector(0, 1), Vector(1, -1), material);

        renderer.setViewMatrix(Matrix::IDENTITY);
    }

    void Menu::keyEvent(const KeyPressEvent &event) {
        gui.keyEvent(event);

        if (event.getCode() == SDLK_RETURN) {
            if (roundsTextbox->isFocused()) {
                applyRoundsTextbox();
            } else {
                addPerson();
            }
        }

        if (event.getCode() == SDLK_F1) {
            play();
        }

        if (event.getCode() == SDLK_F3) {
            if (deleteQuestion()) {
                cleanPersonData();
            }
        }

        if (event.getCode() == SDLK_ESCAPE) {
            close();
        }
    }

    void Menu::textInputEvent(const TextInputEvent &event) {
        gui.textInputEvent(event);
    }

    void Menu::mouseButtonEvent(const MouseButtonEvent &event) {
        bool roundsWasFocused = roundsTextbox->isFocused();
        gui.mouseButtonEvent(event);

        if (!roundsWasFocused && roundsTextbox->isFocused() && roundsTextbox->getText() == "0") {
            roundsTextbox->flush();
        } else if (roundsWasFocused && !roundsTextbox->isFocused() && roundsTextbox->getText().empty()) {
            game->getSettings().setMaxRounds(0);
            updateRoundsTextbox();
        }
    }

    void Menu::mouseMotionEvent(const MouseMotionEvent &event) {
        gui.mouseMotionEvent(event);
    }

    void Menu::mouseWheelEvent(const MouseWheelEvent &event) {
        gui.mouseWheelEvent(event);
    }

    void Menu::joyDeviceAddedEvent(const JoyDeviceAddedEvent &event) {
        joyRescan();
    }

    void Menu::joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &event) {
        joyRescan();
    }

    void Menu::beforeClose(Context *newContext) {
        SDL_StopTextInput();
        sound.stopMusic();
        savePersonData();
    }

    void Menu::enableMusic(bool enable) {
        playMusic = enable;

        if (isCurrent()) {
            if (enable) {
                menuTrack.play(false);
            } else {
                sound.stopMusic();
            }
        }
    }

    void Menu::loadPersonProfiles(const std::string &path) {
        appService.getConsole().printLine("\n===Person profile initialization===");

        std::vector<std::string> profileDirs = File::listDirectory(path, "");
        for (auto &profileName : profileDirs) {
            std::string profilePath = Format("{0}/{1}/") << path << profileName;
            auto profile = std::make_unique<PersonProfile>(profileName, profilePath);
            profile->loadSounds(sound);
            profile->loadSkinColors();
            profile->loadScripts(appService.getScriptManager());
            personProfiles.insert(std::make_pair(profileName, std::move(profile)));
        }

        appService.getConsole().printLine("");
    }

    PersonProfile *Menu::getPersonProfile(const std::string &name) {
        auto profile = personProfiles.find(name);
        if (profile != personProfiles.end()) {
            return profile->second.get();
        }

        return nullptr;
    }

    int Menu::processEvents(bool single) {
        SDL_Event event;
        int result = 0;
        //TODO This logic duplicates event processing logic in Application. Should be refactored.
        while ((result = SDL_PollEvent(&event)) != 0) {
            switch (event.type) {
                case SDL_KEYDOWN: {
                    auto key = event.key.keysym;
                    appService.getInput().setPressed(key.sym, true);
                    break;
                }
                case SDL_KEYUP: {
                    auto key = event.key.keysym;
                    appService.getInput().setPressed(key.sym, false);
                    break;
                }
                case SDL_JOYDEVICEADDED: {
                    appService.getConsole().printLine("Device added");

                    auto deviceIndex = event.jdevice.which;
                    auto joy = SDL_JoystickOpen(deviceIndex);
                    if (SDL_JoystickGetAttached(joy)) {
                        appService.getInput().joyAttached(joy, deviceIndex);
                        joyRescan();
                    } else {
                        appService.getConsole().printLine(
                                Format("Joy attached, but has been detached again -> skipping."));
                        break;
                    }

                    break;
                }
                case SDL_JOYDEVICEREMOVED: {
                    appService.getConsole().printLine("Device removed");
                    auto instanceId = event.jdevice.which;
                    appService.getInput().joyDetached(instanceId);
                    joyRescan();
                    break;
                }
            }
            if (single) {
                return result;
            }
        }
        return result;
    }

    void Menu::consumeInputEvents() {
        processEvents();
    }

    void Menu::shufflePlayers() {
        auto playerCount = playerListBox->size();

        std::vector<Size> shuffle;
        std::vector<std::string> players;
        std::vector<Int32> controls;

        for (Size i = 0; i < playerCount; i++) {
            players.push_back(playerListBox->getItem(i));
            shuffle.push_back(i);
            controls.push_back(controlSwitch[i]->currentItem());
        }

        std::shuffle(shuffle.begin(), shuffle.end(), Math::randomEngine);

        playerListBox->clear();
        for (Size i = 0; i < playerCount; i++) {
            auto pos = shuffle[i];
            playerListBox->addItem(players[pos]);
            controlSwitch[i]->setCurrent(controls[pos]);
        }
    }

    std::vector<std::string> Menu::listMaps() {
        std::vector<std::string> levels;
        for (Size i = 0; i < levelList.getLength(); ++i) {
            levels.push_back(levelList.getPath(i));
        }
        return levels;
    }

    void Menu::eloShufflePlayers() {
        auto playerCount = playerListBox->size();

        std::vector<const Person *> players;
        std::unordered_map<std::string, Int32> controls;

        for (Size i = 0; i < playerCount; i++) {
            std::string name = playerListBox->getItem(i);
            Person &person = persons.getByName(name);
            players.push_back(&person);
            controls[name] = controlSwitch[i]->currentItem();
        }

        std::sort(players.begin(), players.end(), [](const Person *left, const Person *right) {
            return left->getElo() > right->getElo();
        });

        std::vector<Size> shuffle;
        for (Size i = 0; i < playerCount; i++) {
            shuffle.push_back(i);

        }

        Int32 teamPlayerCount = 1 + gameModeSwitch->currentItem() / 2;
        for (Int32 start = 0; start < Int32(playerCount); start += teamPlayerCount) {
            auto span = std::min(teamPlayerCount, Int32(playerCount) - start);
            auto first = shuffle.begin() + start;
            std::shuffle(first, first + span, Math::randomEngine);
        }

        playerListBox->clear();
        for (Size i = 0; i < playerCount; i++) {
            auto pos = shuffle[i];
            std::string name = players[pos]->getName();
            playerListBox->addItem(name);
            controlSwitch[i]->setCurrent(controls[name]);
        }
    }
}
