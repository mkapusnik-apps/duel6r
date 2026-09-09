#ifndef DUEL6_NETWORKMENU_H
#define DUEL6_NETWORKMENU_H

#include <string>
#include <vector>

#include "Context.h"
#include "AppService.h"
#include "client/NetworkSessionRuntime.h"

namespace Duel6 {
    class NetworkMenu final : public Context {
    public:
        explicit NetworkMenu(AppService &service);
        void open(std::vector<Client::NetworkLocalPlayer> players,
                  Network::HostComposition::Setup setup);
        void keyEvent(const KeyPressEvent &event) override;
        void textInputEvent(const TextInputEvent &event) override;
        void mouseButtonEvent(const MouseButtonEvent &) override {}
        void mouseMotionEvent(const MouseMotionEvent &) override {}
        void mouseWheelEvent(const MouseWheelEvent &) override {}
        void joyDeviceAddedEvent(const JoyDeviceAddedEvent &) override {}
        void joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &) override {}
        void update(Float32 elapsedTime) override;
        void render() const override;

    private:
        enum class SetupScreen { Entry, Host, Join };
        AppService &service;
        Renderer &renderer;
        Font &font;
        Client::NetworkSessionRuntime runtime;
        std::vector<Client::NetworkLocalPlayer> localPlayers;
        Network::HostComposition::Setup hostSetup;
        SetupScreen setupScreen = SetupScreen::Entry;
        std::string address = "127.0.0.1";
        std::string port = std::to_string(Network::DefaultServerPort);
        int focus = 0;
        bool controllerConfirm = false, controllerBack = false, controllerUp = false, controllerDown = false;
        Client::NetworkJourney lastJourney = Client::NetworkJourney::Inactive;

        void beforeStart(Context *) override;
        void beforeClose(Context *) override;
        void activate();
        void back();
        void moveFocus(int direction);
        bool endpoint(Network::Endpoint &result) const;
        std::string serverExecutable() const;
        void drawText(Int32 x, Int32 y, const std::string &text, Color color = Color::BLACK) const;
        void drawAction(Int32 y, const std::string &text, bool selected) const;
        void drawPlayers(const Network::Replication::CanonicalState &state) const;
    };
}

#endif
