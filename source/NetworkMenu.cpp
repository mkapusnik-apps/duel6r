#include "NetworkMenu.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <filesystem>

#include "Defines.h"

namespace Duel6 {
    namespace {
        constexpr Int32 CanvasWidth = 850, CanvasHeight = 700;
        int actionCount(Client::NetworkJourney journey, bool host) {
            if (journey == Client::NetworkJourney::Lobby) return host ? 6 : 2;
            if (journey == Client::NetworkJourney::Summary) return host ? 2 : 1;
            if (journey == Client::NetworkJourney::Failure) return 3;
            if (journey == Client::NetworkJourney::HostEnded) return 1;
            if (journey == Client::NetworkJourney::Reconnecting) return 1;
            return 1;
        }
    }

    NetworkMenu::NetworkMenu(AppService &value)
            : service(value), renderer(value.getVideo().getRenderer()), font(value.getFont()) {}

    void NetworkMenu::open(std::vector<Client::NetworkLocalPlayer> players,
                           Network::HostComposition::Setup setup) {
        runtime.reset(); localPlayers = std::move(players); hostSetup = std::move(setup);
        setupScreen = SetupScreen::Entry; focus = 0; lastJourney = Client::NetworkJourney::Inactive;
        Context::push(*this);
    }

    void NetworkMenu::beforeStart(Context *) { SDL_ShowCursor(SDL_ENABLE); SDL_StartTextInput(); }
    void NetworkMenu::beforeClose(Context *) { SDL_StopTextInput(); runtime.reset(); }

    bool NetworkMenu::endpoint(Network::Endpoint &result) const {
        if (address.empty() || address.size() > 253 || port.empty()) return false;
        try {
            std::size_t used = 0; const auto parsed = std::stoul(port, &used);
            if (used != port.size() || parsed == 0 || parsed > 65535) return false;
            result.host = address; result.port = static_cast<std::uint16_t>(parsed); return true;
        } catch (...) { return false; }
    }

    std::string NetworkMenu::serverExecutable() const {
        char *base = SDL_GetBasePath();
        std::filesystem::path path = base ? std::filesystem::path(base) : std::filesystem::current_path();
        if (base) SDL_free(base);
#ifdef D6R_TRANSPORT_WINDOWS
        return (path / "duel6r-server.exe").lexically_normal().string();
#else
        return (path / "duel6r-server").lexically_normal().string();
#endif
    }

    void NetworkMenu::moveFocus(int direction) {
        const auto snap = runtime.snapshot(); int count = 0;
        if (snap.journey == Client::NetworkJourney::Inactive)
            count = setupScreen == SetupScreen::Entry ? 3 : 4;
        else count = actionCount(snap.journey, snap.host);
        focus = (focus + direction + count) % count;
    }

    void NetworkMenu::activate() {
        auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) {
                if (focus == 0) { setupScreen = SetupScreen::Host; focus = 0; }
                else if (focus == 1) { setupScreen = SetupScreen::Join; focus = 0; }
                else close();
                return;
            }
            if (focus < 2) { focus = (focus + 1) % 2; return; }
            if (focus == 3) { setupScreen = SetupScreen::Entry; focus = 0; return; }
            Network::Endpoint target;
            if (!endpoint(target) || localPlayers.empty()) return;
            if (setupScreen == SetupScreen::Host)
                (void) runtime.startHost(target, serverExecutable(), "resources", hostSetup, localPlayers);
            else (void) runtime.join(target, "resources", localPlayers);
            focus = 0; return;
        }
        if (snap.journey == Client::NetworkJourney::Starting) { runtime.cancel(); return; }
        if (snap.journey == Client::NetworkJourney::Lobby) {
            bool ready = false;
            if (snap.canonical) {
                const auto participant = std::find_if(snap.canonical->participants.begin(), snap.canonical->participants.end(),
                        [&snap](const auto &value) { return value.participantId == snap.localParticipantId; });
                if (participant != snap.canonical->participants.end()) ready = participant->ready;
            }
            if (snap.host) {
                if (focus == 0) runtime.setReady(!ready);
                else if (focus == 1) {
                    hostSetup.mode = hostSetup.mode == "Deathmatch" ? "Predator"
                                     : hostSetup.mode == "Predator" ? "Team deathmatch" : "Deathmatch";
                    hostSetup.teamCount = hostSetup.mode == "Team deathmatch" ? 2 : 0;
                    runtime.updateHostSetup(hostSetup);
                } else if (focus == 2) {
                    hostSetup.roundLimit = hostSetup.roundLimit == 99 ? 1 : hostSetup.roundLimit + 1;
                    runtime.updateHostSetup(hostSetup);
                } else if (focus == 3) {
                    hostSetup.levelPlan = hostSetup.levelPlan == "Fixed level" ? "Shuffle all levels"
                                          : hostSetup.levelPlan == "Shuffle all levels" ? "Random level" : "Fixed level";
                    runtime.updateHostSetup(hostSetup);
                } else if (focus == 4) runtime.startMatch();
                else { setupScreen = SetupScreen::Entry; runtime.endSession(); }
            } else {
                if (focus == 0) runtime.setReady(!ready);
                else { setupScreen = SetupScreen::Entry; runtime.leave(); }
            }
        } else if (snap.journey == Client::NetworkJourney::Summary) {
            if (snap.host && focus == 0) runtime.returnToLobby();
            else if (snap.host) { setupScreen = SetupScreen::Entry; runtime.endSession(); }
            else { setupScreen = SetupScreen::Entry; runtime.leave(); }
        } else if (snap.journey == Client::NetworkJourney::Reconnecting) {
            setupScreen = SetupScreen::Entry; runtime.leave();
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            runtime.reset(); setupScreen = SetupScreen::Entry;
        }
        else if (snap.journey == Client::NetworkJourney::Failure) {
            if (focus == 0 && snap.retryAllowed) {
                Network::Endpoint target; if (!endpoint(target)) return;
                runtime.reset();
                if (snap.host) (void) runtime.startHost(target, serverExecutable(), "resources", hostSetup, localPlayers);
                else (void) runtime.join(target, "resources", localPlayers);
            } else if (focus == 1) { runtime.reset(); setupScreen = snap.host ? SetupScreen::Host : SetupScreen::Join; }
            else { runtime.reset(); setupScreen = SetupScreen::Entry; }
            focus = 0;
        }
    }

    void NetworkMenu::back() {
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Starting) runtime.cancel();
        else if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) close(); else { setupScreen = SetupScreen::Entry; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        }
    }

    void NetworkMenu::keyEvent(const KeyPressEvent &event) {
        if (!event.isPressed()) return;
        if (event.getCode() == SDLK_ESCAPE) back();
        else if (event.getCode() == SDLK_TAB || event.getCode() == SDLK_DOWN) moveFocus(1);
        else if (event.getCode() == SDLK_UP) moveFocus(-1);
        else if (event.getCode() == SDLK_RETURN || event.getCode() == SDLK_SPACE) activate();
        else if ((setupScreen == SetupScreen::Host || setupScreen == SetupScreen::Join)
                 && runtime.snapshot().journey == Client::NetworkJourney::Inactive) {
            std::string *field = focus == 0 && setupScreen == SetupScreen::Join ? &address : &port;
            if (event.getCode() == SDLK_BACKSPACE && !field->empty()) field->pop_back();
        }
    }

    void NetworkMenu::textInputEvent(const TextInputEvent &event) {
        if (runtime.snapshot().journey != Client::NetworkJourney::Inactive
            || setupScreen == SetupScreen::Entry) return;
        std::string *field = focus == 0 && setupScreen == SetupScreen::Join ? &address : &port;
        const std::string value = event.getText();
        for (char c: value) {
            const bool allowed = field == &port ? c >= '0' && c <= '9'
                                                : std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == ':';
            if (allowed && field->size() < (field == &port ? 5u : 253u)) field->push_back(c);
        }
    }

    void NetworkMenu::update(Float32) {
        runtime.update();
        if (!localPlayers.empty() && localPlayers[0].controls) {
            const auto &c = *localPlayers[0].controls;
            const bool confirm = c.getShoot().isPressed(), backNow = c.getPick().isPressed();
            const bool up = c.getUp().isPressed(), down = c.getDown().isPressed();
            if (confirm && !controllerConfirm) activate();
            if (backNow && !controllerBack) back();
            if (up && !controllerUp) moveFocus(-1);
            if (down && !controllerDown) moveFocus(1);
            controllerConfirm = confirm; controllerBack = backNow; controllerUp = up; controllerDown = down;
        }
        const auto snap = runtime.snapshot();
        if (snap.journey != lastJourney) {
            if (snap.journey == Client::NetworkJourney::Inactive
                && lastJourney == Client::NetworkJourney::Cancelling) focus = 0;
            else focus = 0;
            lastJourney = snap.journey;
        }
    }

    void NetworkMenu::drawText(Int32 x, Int32 y, const std::string &text, Color color) const {
        font.print(x, y, color, text);
    }
    void NetworkMenu::drawAction(Int32 y, const std::string &text, bool selected) const {
        renderer.quadXY(Vector(275, y + 22), Vector(300, 32), selected ? Color(64, 96, 160) : Color(192));
        drawText(425 - static_cast<Int32>(text.size()) * 4, y + 8, text, selected ? Color::WHITE : Color::BLACK);
    }
    void NetworkMenu::drawPlayers(const Network::Replication::CanonicalState &state) const {
        Int32 y = 490;
        for (const auto &participant: state.participants) {
            drawText(80, y, participant.host ? "Host" : "Guest");
            drawText(190, y, participant.connection == Network::Replication::ConnectionState::Connected ? "Connected" : "Reconnecting");
            drawText(340, y, participant.ready ? "Ready" : "Not ready"); y -= 20;
            for (auto id: participant.ownedPlayerIds) {
                const auto player = std::find_if(state.players.begin(), state.players.end(), [id](const auto &p) { return p.playerId == id; });
                if (player != state.players.end()) drawText(110, y, player->displayName + " • Player " + std::to_string(player->rosterPosition + 1));
                y -= 18;
            }
        }
    }

    void NetworkMenu::render() const {
        const auto width = service.getVideo().getScreen().getClientWidth();
        const auto height = service.getVideo().getScreen().getClientHeight();
        const Float32 scale = std::min(Float32(width) / CanvasWidth, Float32(height) / CanvasHeight);
        const Int32 tx = (width - Int32(CanvasWidth * scale)) / 2, ty = (height - Int32(CanvasHeight * scale)) / 2;
        renderer.setViewMatrix(Matrix::translate(Float32(tx), Float32(ty), 0) * Matrix::scale(scale, scale, 1));
        renderer.quadXY(Vector(0, 0), Vector(CanvasWidth, CanvasHeight), Color(192));
        renderer.quadXY(Vector(24, 650), Vector(802, 602), Color(224));
        const auto snap = runtime.snapshot();
        std::string title = "NETWORK PLAY";
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Host) title = "HOST NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Join) title = "JOIN NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Lobby) title = "NETWORK LOBBY";
        else if (snap.journey == Client::NetworkJourney::Match) title = "NETWORK MATCH";
        else if (snap.journey == Client::NetworkJourney::Summary) title = "MATCH SUMMARY";
        else if (snap.journey == Client::NetworkJourney::Reconnecting) title = "RECONNECTING";
        else if (snap.journey == Client::NetworkJourney::Failure) title = "CONNECTION FAILED";
        else if (snap.journey == Client::NetworkJourney::HostEnded) title = "HOST ENDED SESSION";
        drawText(425 - static_cast<Int32>(title.size()) * 4, 620, title);
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Entry) {
            drawText(285, 555, "Same machine or LAN"); drawText(285, 530, "Direct address and port");
            drawText(285, 505, "Linux / Windows x86-64");
            drawText(165, 465, "Player-hosted • Lobby 1–15 • Match 2–15 participants and players");
            drawAction(375, "Host", focus == 0); drawAction(330, "Join", focus == 1); drawAction(285, "Back", focus == 2);
        } else if (snap.journey == Client::NetworkJourney::Inactive) {
            Int32 y = 550;
            if (setupScreen == SetupScreen::Join) { drawText(150, y, "Address: " + address + (focus == 0 ? " <" : "")); y -= 35; }
            drawText(150, y, "Port: " + port + (focus == (setupScreen == SetupScreen::Join ? 1 : 0) ? " <" : ""));
            y -= 40; drawText(150, y, "Local players: " + std::to_string(localPlayers.size()) + " • Lobby 1–15 • Match 2–15");
            for (const auto &player: localPlayers) { y -= 24; drawText(175, y, player.name + " • " + (player.controls ? player.controls->getDescription() : "No control")); }
            Network::Endpoint ignored; const bool valid = endpoint(ignored) && !localPlayers.empty();
            drawText(150, 240, valid ? "Same machine or LAN • Session-only scores • Optional scripts disabled"
                                    : "Enter a valid endpoint and add at least one local player.");
            drawAction(170, setupScreen == SetupScreen::Host ? "Start session" : "Connect", focus == 2);
            drawAction(125, "Back", focus == 3);
        } else if (snap.journey == Client::NetworkJourney::Starting || snap.journey == Client::NetworkJourney::Cancelling) {
            drawText(300, 430, snap.status); if (snap.journey == Client::NetworkJourney::Starting)
                drawText(270, 400, "Startup can take up to 10 seconds.");
            if (snap.journey == Client::NetworkJourney::Starting) drawAction(300, "Cancel", true);
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Lobby) {
            drawText(80, 560, "Role        Connection       Readiness"); drawPlayers(*snap.canonical);
            drawText(80, 190, snap.canonical->settings.mode + " • " + snap.canonical->settings.levelPlan
                     + " • Rounds " + std::to_string(snap.canonical->settings.roundLimit));
            drawAction(160, "Ready / Not ready", focus == 0);
            if (snap.host) {
                drawAction(125, "Mode: " + snap.canonical->settings.mode, focus == 1);
                drawAction(90, "Rounds: " + std::to_string(snap.canonical->settings.roundLimit), focus == 2);
                drawAction(55, "Level: " + snap.canonical->settings.levelPlan, focus == 3);
                drawAction(20, "Start match", focus == 4);
                drawText(650, 30, focus == 5 ? "> End session" : "End session");
            }
            else drawAction(105, "Leave session", focus == 1);
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Match) {
            drawText(70, 570, "One shared authoritative arena • " + snap.canonical->settings.mode);
            for (const auto &player: snap.canonical->players) {
                const Int32 x = 80 + static_cast<Int32>((player.positionX % 650 + 650) % 650);
                const Int32 y = 150 + static_cast<Int32>((player.positionY % 350 + 350) % 350);
                renderer.quadXY(Vector(x, y), Vector(12, 20), player.lifeState == Network::Replication::LifeState::Alive ? Color(32, 96, 192) : Color(96));
            }
            drawText(70, 100, snap.host ? "Host • LAN session • Connected" : "Guest • LAN session • Connected");
            drawText(70, 75, "Session only scores • Optional scripts disabled");
            if (snap.presentation.degraded) drawText(70, 50, "Network connection degraded.");
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Summary) {
            drawText(80, 560, "Completed • Session only • Results are not saved");
            Int32 y = 510; for (auto id: snap.canonical->score.ranking) {
                const auto p = std::find_if(snap.canonical->players.begin(), snap.canonical->players.end(), [id](const auto &v) { return v.playerId == id; });
                if (p != snap.canonical->players.end()) { drawText(130, y, p->displayName); y -= 22; }
            }
            if (snap.host) { drawAction(130, "Return to lobby", focus == 0); drawAction(85, "End session", focus == 1); }
            else drawAction(130, "Leave session", true);
        } else if (snap.journey == Client::NetworkJourney::Reconnecting) {
            drawText(235, 430, "Connection interrupted. Reconnecting…");
            drawText(265, 395, std::to_string(snap.reconnectSeconds.value_or(0)) + " seconds remaining");
            drawAction(300, "Leave session", true);
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            drawText(130, 470, snap.failure.empty() ? "Connection could not be completed." : snap.failure);
            drawText(130, 440, "Endpoint: " + snap.endpoint.host + ':' + std::to_string(snap.endpoint.port));
            drawAction(340, snap.retryAllowed ? "Retry" : "Retry unavailable", focus == 0);
            drawAction(295, "Edit setup", focus == 1); drawAction(250, "Return to Network", focus == 2);
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            drawText(175, 440, "The host ended this session. It cannot migrate or resume.");
            drawText(245, 410, "Session-only scores were not saved."); drawAction(315, "Return to Network", true);
        }
        renderer.setViewMatrix(Matrix::IDENTITY);
    }
}
