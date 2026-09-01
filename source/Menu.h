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

#ifndef DUEL6_MENU_H
#define DUEL6_MENU_H

#include <vector>
#include <unordered_map>
#include <future>
#include "Type.h"
#include "Context.h"
#include "LevelList.h"
#include "PersonList.h"
#include "PersonProfile.h"
#include "input/PlayerControls.h"
#include "PlayerSkinColors.h"
#include "Video.h"
#include "AppService.h"
#include "Defines.h"
#include "gui/Desktop.h"
#include "gui/Button.h"
#include "gui/CheckBox.h"
#include "gui/ListBox.h"
#include "gui/Label.h"
#include "gui/Panel.h"
#include "gui/TextBox.h"
#include "gui/Spinner.h"
#include "GameMode.h"

namespace Duel6 {
    class Game; // Forward, TODO: Remove

    class Menu
            : public Context {
    private:
        struct PreparedMenuBackground {
            Image image;
            std::string filename;
            std::vector<std::string> remainingCandidates;
            std::vector<std::string> failedCandidates;
            bool hasImage = false;
            bool directoryAvailable = true;
        };

        AppService &appService;
        Font &font;
        Video &video;
        Renderer &renderer;
        Sound &sound;
        Game *game;
        std::vector<std::unique_ptr<GameMode>> gameModes;
        Gui::Desktop gui;
        PlayerControlsManager controlsManager;
        PersonProfileList personProfiles;
        PlayerSounds defaultPlayerSounds;
        LevelList levelList;
        PersonList persons;
        Gui::ListBox *personListBox;
        std::vector<std::string> personListNames;
        Gui::ListBox *playerListBox;
        Gui::ListBox *scoreListBox;
        Gui::Spinner *controlSwitch[D6_MAX_PLAYERS];
        Gui::Textbox *textbox;
        Gui::Textbox *roundsTextbox;
        Gui::Spinner *gameModeSwitch;
        Gui::Label *teamCountLabel;
        Gui::Spinner *teamCountSwitch;
        Gui::CheckBox *friendlyFireCheckBox;
        Gui::CheckBox *globalAssistanceCheckBox;
        Gui::CheckBox *quickLiquidCheckBox;
        Gui::CheckBox *burnableTreesCheckBox;
        Gui::Panel *playersPanel;
        Size backgroundCount;
        Texture menuBannerTexture;
        mutable Texture menuBackgroundTexture;
        mutable std::string menuBackgroundFilename;
        mutable bool hasMenuBackground;
        mutable std::future<PreparedMenuBackground> menuBackgroundPreparation;
        mutable bool menuBackgroundPreparationActive;
        mutable bool menuBackgroundFinished;
        mutable bool menuBackgroundInitialFrameRendered;
        Float32 menuScale;
        Int32 menuTranslationX;
        Int32 menuTranslationY;
        Sound::Track menuTrack;
        bool playMusic;

    public:
        explicit Menu(AppService &appService);

        ~Menu() override;

        void setGameReference(Game &game) {
            this->game = &game;
        }

        void initialize();

        void joyRescan();

        void savePersonData() const;

        void keyEvent(const KeyPressEvent &event) override;

        void textInputEvent(const TextInputEvent &event) override;

        void mouseButtonEvent(const MouseButtonEvent &event) override;

        void mouseMotionEvent(const MouseMotionEvent &event) override;

        void mouseWheelEvent(const MouseWheelEvent &event) override;

        void joyDeviceAddedEvent(const JoyDeviceAddedEvent & event) override;

        void joyDeviceRemovedEvent(const JoyDeviceRemovedEvent & event) override;

        void update(Float32 elapsedTime) override;

        void render() const override;

        void enableMusic(bool enable);

        std::unordered_map<std::string, std::unique_ptr<PersonProfile>> &getPersonProfiles() {
            return personProfiles;
        }

        std::vector<std::string> listMaps();

        void play(std::vector<std::string> levels);

    private:
        void beforeStart(Context *prevContext) override;

        void beforeClose(Context *nextContext) override;

        void initializeGameModes();

        bool isTeamModeSelected();

        Int32 selectedTeamCount();

        GameMode &selectedGameMode();

        void updateGameSettingsLayout();

        void updatePlayerColors();

        void initializePresentation();

        void startMenuBackgroundPreparation(std::vector<std::string> candidates,
                                            bool discoverCandidates) const noexcept;

        static PreparedMenuBackground prepareMenuBackground(Int32 clientWidth, Int32 clientHeight,
                                                            std::vector<std::string> candidates,
                                                            bool discoverCandidates);

        void publishPreparedMenuBackground() const noexcept;

        void publishPreparedMenuBackgroundTransaction() const;

        void retryPreparedMenuBackground(PreparedMenuBackground &prepared) const noexcept;

        void freeOptionalTexture(Texture texture) const noexcept;

        void printMenuBackgroundDiagnostic(const char *message) const noexcept;

        void printMenuBackgroundDiagnostic(const char *prefix, const std::string &value,
                                           const char *suffix) const noexcept;

        void renderMenuBackground() const;

        void showMessage(const std::string &message);

        bool validateStartPrerequisites(const std::vector<std::string> &levels);

        void detectControls(Size playerIndex);

        void play();

        void playPlayersSound(const std::string &name);

        void loadPersonProfiles(const std::string &path);

        void loadPersonData(const std::string &filePath);

        PersonProfile *getPersonProfile(const std::string &name);

        void cleanPersonData();

        void addPerson();

        void deletePerson();

        void addPlayer(Int32 index);

        void removePlayer(Int32 c);

        bool isPlayer(const std::string &name) const;

        void updatePlayerCount();

        void updateRoundsTextbox();

        void applyRoundsTextbox();

        void rebuildTable();

        void rebuildPersonList();

        bool question(const std::string &question);

        bool deleteQuestion();

        int processEvents(bool single = true);

        void consumeInputEvents();

        void shufflePlayers();

        void eloShufflePlayers();
    };
}

#endif
