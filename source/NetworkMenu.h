#ifndef DUEL6_NETWORKMENU_H
#define DUEL6_NETWORKMENU_H

#include <string>
#include <vector>

#include "Context.h"
#include "AppService.h"
#include "CanonicalWorldPresenter.h"
#include "client/NetworkSessionRuntime.h"

namespace Duel6 {
    class NetworkMenu final : public Context {
    public:
        NetworkMenu(AppService &service, GameResources &resources);
        void open(std::vector<Client::NetworkLocalPlayer> players,
                  Network::HostComposition::Setup setup,
                  std::vector<std::string> persons,
                  std::vector<std::string> levels);
        void keyEvent(const KeyPressEvent &event) override;
        void textInputEvent(const TextInputEvent &event) override;
        void mouseButtonEvent(const MouseButtonEvent &event) override;
        void mouseMotionEvent(const MouseMotionEvent &) override {}
        void mouseWheelEvent(const MouseWheelEvent &event) override;
        void joyDeviceAddedEvent(const JoyDeviceAddedEvent &) override;
        void joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &) override;
        void update(Float32 elapsedTime) override;
        void render() const override;

    private:
        enum class SetupScreen { Entry, Host, Join };
        enum class Confirmation { None, Leave, End };
        AppService &service;
        Renderer &renderer;
        Font &font;
        PlayerControlsManager &controlsManager;
        CanonicalWorldPresenter worldPresenter;
        Client::NetworkSessionRuntime runtime;
        std::vector<Client::NetworkLocalPlayer> localPlayers;
        std::vector<std::string> availablePersons;
        std::vector<std::string> availableLevels;
        Network::HostComposition::Setup hostSetup;
        SetupScreen setupScreen = SetupScreen::Entry;
        std::string address = "127.0.0.1";
        std::string hostAddress;
        std::vector<std::string> hostAddresses;
        std::string port = std::to_string(Network::DefaultServerPort);
        int focus = 0;
        int setupScroll = 0;
        int summaryScroll = 0;
        int summaryHorizontal = 0;
        int rankingScroll = 0;
        Confirmation confirmation = Confirmation::None;
        bool scoreOverlay = false;
        bool controllerConfirm = false, controllerBack = false, controllerUp = false, controllerDown = false;
        bool controllerLeft = false, controllerRight = false;
        bool controllerSessionBack = false;
        Client::NetworkJourney lastJourney = Client::NetworkJourney::Inactive;
        Client::NetworkJourney lastStableJourney = Client::NetworkJourney::Inactive;
        bool previousRetryEligible = false;

        void beforeStart(Context *) override;
        void beforeClose(Context *) override;
        void activate();
        void back();
        void moveFocus(int direction);
        void rescanControls();
        void cycleControl(std::size_t playerIndex, int direction = 1);
        void cyclePerson(std::size_t playerIndex, int direction = 1);
        bool setupValid(std::string &reason) const;
        bool retryEligible(const Client::NetworkRuntimeSnapshot &snapshot, std::string &reason) const;
        bool startEligible(const Client::NetworkRuntimeSnapshot &snapshot, std::string &reason) const;
        bool localReadyEligible(std::string &reason) const;
        bool endpoint(Network::Endpoint &result) const;
        std::string serverExecutable() const;
        void drawText(Int32 x, Int32 y, const std::string &text, Color color = Color::BLACK) const;
        void drawClippedText(Int32 x, Int32 y, const std::string &text, std::size_t characters,
                             Color color = Color::BLACK) const;
        void drawWrappedText(Int32 x, Int32 y, const std::string &text,
                             std::size_t charactersPerLine, std::size_t maximumLines,
                             Color color = Color::BLACK) const;
        void drawAction(Int32 y, const std::string &text, bool selected) const;
        void drawFocusKeyline(Int32 x, Int32 y, Int32 width, Int32 height, bool selected) const;
        void drawPlayers(const Network::Replication::CanonicalState &state) const;
        void drawMatch(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height,
                       bool interactive = true) const;
        void drawLobby(const Client::NetworkRuntimeSnapshot &snapshot) const;
        void drawSummary(const Client::NetworkRuntimeSnapshot &snapshot) const;
        void drawRoundSummary(const Client::NetworkRuntimeSnapshot &snapshot,
                              Int32 width, Int32 height) const;
        void drawRetainedContext(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawReconnectPanel(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawHostEndedPanel(const Client::NetworkRuntimeSnapshot &snapshot, Int32 width, Int32 height) const;
        void drawResult(const Network::Replication::CanonicalState &state, bool retained) const;
        void drawConfirmation() const;
    };
}

#endif
