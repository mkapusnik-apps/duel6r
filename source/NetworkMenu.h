#ifndef DUEL6_NETWORKMENU_H
#define DUEL6_NETWORKMENU_H

#include <string>
#include <vector>
#include <functional>

#include "Context.h"
#include "AppService.h"
#include "CanonicalWorldPresenter.h"
#include "client/NetworkSessionRuntime.h"
#include "client/HostDirectory.h"

namespace Duel6 {
    class NetworkMenu final : public Context {
    public:
        NetworkMenu(AppService &service, GameResources &resources,
                    Texture menuBannerTexture, std::function<void()> renderMenuBackground);
        void open(std::vector<Client::NetworkLocalPlayer> players,
                  Network::HostComposition::Setup setup,
                  std::vector<std::string> persons,
                  std::vector<std::string> levels);
        void keyEvent(const KeyPressEvent &event) override;
        void textInputEvent(const TextInputEvent &event) override;
        void mouseButtonEvent(const MouseButtonEvent &event) override;
        void mouseMotionEvent(const MouseMotionEvent &event) override {
            pointerX = event.getX(); pointerY = event.getY();
        }
        void mouseWheelEvent(const MouseWheelEvent &event) override;
        void joyDeviceAddedEvent(const JoyDeviceAddedEvent &) override;
        void joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &) override;
        void update(Float32 elapsedTime) override;
        void render() const override;

    private:
        enum class SetupScreen { Entry, Host, Join, Browser };
        enum class Confirmation { None, Leave, End };
        AppService &service;
        Renderer &renderer;
        Font &font;
        PlayerControlsManager &controlsManager;
        CanonicalWorldPresenter worldPresenter;
        Client::NetworkSessionRuntime runtime;
        Texture menuBannerTexture;
        std::function<void()> renderMenuBackground;
        std::vector<Client::NetworkLocalPlayer> localPlayers;
        std::vector<std::string> availablePersons;
        std::vector<std::string> availableLevels;
        Network::HostComposition::Setup hostSetup;
        std::uint8_t preferredTeamCount = 2;
        bool preferredFriendlyFire = false;
        SetupScreen setupScreen = SetupScreen::Entry;
        std::string address = "127.0.0.1";
        std::string password;
        Client::DirectoryBrowser browser;
        std::string selectedListing;
        std::optional<Client::DirectoryListing> browserSelection;
        bool joinFromBrowser = false;
        int browserScroll = 0;
        std::string hostAddress;
        std::vector<std::string> hostAddresses;
        bool hostAddressSelectorOpen = false;
        bool hostAddressSelectionBecameInvalid = false;
        std::size_t hostAddressHighlight = 0;
        std::size_t hostAddressScroll = 0;
        std::string port = std::to_string(Network::DefaultServerPort);
        int focus = 0;
        int setupScroll = 0;
        int setupPersonsScroll = 0, setupPlayersScroll = 0;
        int summaryScroll = 0;
        int summaryHorizontal = 0;
        int rankingScroll = 0;
        Confirmation confirmation = Confirmation::None;
        bool confirmationInputArmed = false;
        bool scoreOverlay = false;
        std::uint32_t consumedKeyboardActions = 0;
        bool previousRoundSummary = false;
        bool controllerConfirm = false, controllerBack = false, controllerUp = false, controllerDown = false;
        bool controllerLeft = false, controllerRight = false;
        bool controllerSessionBack = false;
        Client::NetworkJourney lastJourney = Client::NetworkJourney::Inactive;
        Client::NetworkJourney lastStableJourney = Client::NetworkJourney::Inactive;
        bool previousRetryEligible = false;
        // Presentation only: remember a pointer hold, never defer activation to release.
        bool pointerHeld = false;
        Int32 pointerX = 0, pointerY = 0;
        SetupScreen pointerScreen = SetupScreen::Entry;
        Confirmation pointerConfirmation = Confirmation::None;
        Client::NetworkJourney pointerJourney = Client::NetworkJourney::Inactive;
        mutable Client::NetworkJourney renderingJourney = Client::NetworkJourney::Inactive;

        void beforeStart(Context *) override;
        void beforeClose(Context *) override;
        void activate();
        void showConfirmation(Confirmation value);
        void back();
        void moveFocus(int direction);
        void syncLobbyScroll(const Client::NetworkRuntimeSnapshot &snapshot);
        void syncSetupScroll();
        void rescanControls();
        void cycleControl(std::size_t playerIndex, int direction = 1);
        void cyclePerson(std::size_t playerIndex, int direction = 1);
        bool setupValid(std::string &reason) const;
        bool retryEligible(const Client::NetworkRuntimeSnapshot &snapshot, std::string &reason) const;
        bool startEligible(const Client::NetworkRuntimeSnapshot &snapshot, std::string &reason) const;
        bool localReadyEligible(std::string &reason) const;
        bool endpoint(Network::Endpoint &result) const;
        bool editingEndpoint(const Client::NetworkRuntimeSnapshot &snapshot) const;
        bool refreshHostAddresses(bool initialSelection);
        std::string serverExecutable() const;
        void drawText(Int32 x, Int32 y, const std::string &text, Color color = Color::BLACK) const;
        void drawClippedText(Int32 x, Int32 y, const std::string &text, std::size_t characters,
                             Color color = Color::BLACK) const;
        void drawWrappedText(Int32 x, Int32 y, const std::string &text,
                             std::size_t charactersPerLine, std::size_t maximumLines,
                             Color color = Color::BLACK) const;
        void drawBevel(Int32 x, Int32 y, Int32 width, Int32 height, bool inset = false) const;
        void drawPanel(Int32 x, Int32 y, Int32 width, Int32 height, const std::string &title) const;
        void drawField(Int32 x, Int32 y, Int32 width, Int32 height, bool selected = false) const;
        void drawButton(Int32 x, Int32 y, Int32 width, Int32 height, const std::string &text,
                        bool selected, bool enabled = true, bool clientSpace = false) const;
        void drawAction(Int32 y, const std::string &text, bool selected, bool enabled = true) const;
        void drawFocusKeyline(Int32 x, Int32 y, Int32 width, Int32 height, bool selected) const;
        void drawMenuCanvas(Int32 width, Int32 height) const;
        void drawPlayers(const Network::Replication::CanonicalState &state, bool host = false) const;
        void drawMatch(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height,
                       bool interactive = true, const std::string &connectionState = "Connected",
                       bool showLiveNetworkState = true) const;
        void drawLobby(const Client::NetworkRuntimeSnapshot &snapshot) const;
        void drawSummary(const Client::NetworkRuntimeSnapshot &snapshot) const;
        void drawRoundSummary(const Client::NetworkRuntimeSnapshot &snapshot,
                              Int32 width, Int32 height) const;
        void drawRetainedContext(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawReconnectPanel(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawHostEndedPanel(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawResult(const Network::Replication::CanonicalState &state, bool retained) const;
        void drawConfirmation() const;
        void drawBrowser() const;
        void selectBrowserRow(int direction);
        void joinSelected();
        bool browserFocusEnabled(int index) const;
    };
}

#endif
