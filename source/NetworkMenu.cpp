#include "NetworkMenu.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#include "Defines.h"
#include "json/JsonParser.h"
#include "network/NetworkTrustPolicy.h"

namespace Duel6 {
    namespace {
        constexpr Int32 CanvasWidth = 850, CanvasHeight = 700;

        std::string onOff(bool value) { return value ? "On" : "Off"; }

        std::string outcome(const Network::Replication::CanonicalState &state,
                            const Network::Replication::RoundOutcomeState &value) {
            if (value.noWinner) return "No winner";
            if (value.winningTeam) return "Team " + std::to_string(value.winningTeam);
            std::string result;
            for (const auto id: value.winnerPlayerIds) {
                const auto player = std::find_if(state.players.begin(), state.players.end(),
                        [id](const auto &entry) { return entry.playerId == id; });
                if (!result.empty()) result += ", ";
                result += player == state.players.end() ? "Player " + std::to_string(id)
                                                        : player->displayName;
            }
            return result.empty() ? "Pending" : result;
        }

        std::string participantLabel(const Network::Replication::CanonicalState &state,
                                     Network::Replication::Identity id) {
            const auto found = std::find_if(state.participants.begin(), state.participants.end(),
                    [id](const auto &value) { return value.participantId == id; });
            if (found == state.participants.end()) return "Departed participant";
            return found->host ? "Host" : "Guest " + std::to_string(
                    static_cast<unsigned>(std::distance(state.participants.begin(), found)) + 1);
        }

        std::vector<std::string> resultLines(const Network::Replication::CanonicalState &state) {
            std::vector<std::string> lines;
            if (!state.result.available || state.result.serialized.empty()) return lines;
            try {
                const std::vector<Uint8> bytes(state.result.serialized.begin(), state.result.serialized.end());
                const Json::Value root = Json::Parser().parse(bytes);
                const auto serializedPlayers = root.get("players");
                std::map<Network::Replication::Identity, std::string> resultPlayerNames;
                for (Size index = 0; index < serializedPlayers.getLength(); ++index) {
                    const auto player = serializedPlayers.get(index);
                    resultPlayerNames.emplace(
                            static_cast<Network::Replication::Identity>(player.get("playerId").asDouble()),
                            player.get("displayName").asString());
                }
                const auto resultPlayerName = [&](Network::Replication::Identity id) {
                    const auto found = resultPlayerNames.find(id);
                    return found == resultPlayerNames.end() ? std::string("Departed player") : found->second;
                };
                lines.push_back("MATCH SETTINGS");
                std::ostringstream settings;
                settings << root.get("mode").asString();
                const int teams = root.get("teamCount").asInt();
                if (teams) settings << " • " << teams << " teams • Friendly fire "
                                    << onOff(root.get("friendlyFire").asBoolean());
                settings << " • Rounds " << root.get("roundLimit").asInt()
                         << " • Seed " << static_cast<std::uint64_t>(root.get("seed").asDouble());
                lines.push_back(settings.str());
                lines.push_back("Assistance " + onOff(root.get("assistance").asBoolean())
                                + " • Quick Liquid " + onOff(root.get("quickLiquid").asBoolean())
                                + " • Burnable Trees " + onOff(root.get("burnableTrees").asBoolean()));
                lines.push_back("Level plan: " + root.get("levelPlan").asString()
                                + " • Completed rounds: " + std::to_string(root.get("completedRounds").asInt()));
                lines.push_back("ROUND RESULTS");
                const auto rounds = root.get("rounds");
                for (Size index = 0; index < rounds.getLength(); ++index) {
                    const auto round = rounds.get(index);
                    std::string winner = round.get("noWinner").asBoolean() ? "No winner" : round.get("winningTeam").asString();
                    if (winner == "None") {
                        winner.clear(); const auto winnerIds = round.get("winnerPlayerIds");
                        for (Size winnerIndex = 0; winnerIndex < winnerIds.getLength(); ++winnerIndex) {
                            const auto id = static_cast<Network::Replication::Identity>(winnerIds.get(winnerIndex).asDouble());
                            if (!winner.empty()) winner += ", ";
                            winner += resultPlayerName(id);
                        }
                    }
                    std::string roster; const auto rosterIds = round.get("rosterOrder");
                    for (Size rosterIndex = 0; rosterIndex < rosterIds.getLength(); ++rosterIndex) {
                        const auto id = static_cast<Network::Replication::Identity>(rosterIds.get(rosterIndex).asDouble());
                        if (!roster.empty()) roster += ", ";
                        roster += resultPlayerName(id);
                    }
                    lines.push_back("Round " + std::to_string(round.get("roundNumber").asInt()) + " • "
                                    + round.get("level").asString() + " • "
                                    + round.get("orientation").asString() + " • " + winner
                                    + " • Roster: " + roster);
                }
                lines.push_back("CUMULATIVE PLAYER RESULTS");
                lines.push_back("Rank  Player / owner / team / state     R  Sh  Hi  K  D  A  W  P  Survival  Damage  Assist dmg  Points");
                const auto players = serializedPlayers;
                for (Size index = 0; index < players.getLength(); ++index) {
                    const auto player = players.get(index); const auto stats = player.get("cumulative");
                    const auto ownerId = static_cast<Network::Replication::Identity>(player.get("participantId").asDouble());
                    std::ostringstream row;
                    row << player.get("rank").asInt() << "  " << player.get("displayName").asString()
                        << " / " << participantLabel(state, ownerId);
                    const std::string team = player.get("team").asString();
                    if (team != "None") row << " / " << team;
                    if (player.get("departed").asBoolean()) row << " / Departed";
                    row << "  " << stats.get("roundsPlayed").asInt() << "  " << stats.get("shots").asInt()
                        << "  " << stats.get("hits").asInt() << "  " << stats.get("kills").asInt()
                        << "  " << stats.get("deaths").asInt() << "  " << stats.get("assists").asInt()
                        << "  " << stats.get("wins").asInt() << "  " << stats.get("penalties").asInt()
                        << "  " << stats.get("survivalTicks").asInt() << "  " << stats.get("damage").asInt()
                        << "  " << stats.get("assistedDamage").asInt() << "  " << stats.get("totalPoints").asInt();
                    lines.push_back(row.str());
                    const auto perRound = player.get("rounds");
                    for (Size roundIndex = 0; roundIndex < perRound.getLength(); ++roundIndex) {
                        const auto value = perRound.get(roundIndex); std::ostringstream roundRow;
                        roundRow << "    Round " << (roundIndex + 1)
                                 << "  R " << value.get("roundsPlayed").asInt()
                                 << "  Sh " << value.get("shots").asInt() << "  Hi " << value.get("hits").asInt()
                                 << "  K " << value.get("kills").asInt() << "  D " << value.get("deaths").asInt()
                                 << "  A " << value.get("assists").asInt() << "  W " << value.get("wins").asInt()
                                 << "  P " << value.get("penalties").asInt()
                                 << "  Survival " << value.get("survivalTicks").asInt()
                                 << "  Damage " << value.get("damage").asInt()
                                 << "  Assist dmg " << value.get("assistedDamage").asInt()
                                 << "  Points " << value.get("totalPoints").asInt();
                        lines.push_back(roundRow.str());
                    }
                }
                const auto teamsResult = root.get("teams");
                if (teamsResult.getLength()) {
                    lines.push_back("TEAM TOTALS");
                    for (Size index = 0; index < teamsResult.getLength(); ++index) {
                        const auto team = teamsResult.get(index);
                        lines.push_back("Rank " + std::to_string(team.get("rank").asInt()) + " • "
                                        + team.get("team").asString() + " • "
                                        + std::to_string(team.get("totalPoints").asInt()) + " points");
                    }
                }
            } catch (...) {
                lines.clear();
                lines.push_back("Authoritative result details are unavailable.");
            }
            return lines;
        }
    }

    NetworkMenu::NetworkMenu(AppService &value, GameResources &resources)
            : service(value), renderer(value.getVideo().getRenderer()), font(value.getFont()),
              controlsManager(value.getControlsManager()), worldPresenter(value, resources) {}

    void NetworkMenu::open(std::vector<Client::NetworkLocalPlayer> players,
                           Network::HostComposition::Setup setup,
                           std::vector<std::string> persons,
                           std::vector<std::string> levels) {
        runtime.reset(); localPlayers = std::move(players); hostSetup = std::move(setup);
        availablePersons = std::move(persons); availableLevels = std::move(levels);
        hostAddress = Network::Trust::preferredLocalListenerAddress().value_or(std::string());
        for (auto &player: localPlayers)
            if (player.controlDescription.empty() && player.controls)
                player.controlDescription = player.controls->getDescription();
        setupScreen = SetupScreen::Entry; focus = 0; setupScroll = 0; summaryScroll = 0; summaryHorizontal = 0;
        confirmation = Confirmation::None; scoreOverlay = false;
        lastJourney = Client::NetworkJourney::Inactive;
        Context::push(*this);
    }

    void NetworkMenu::beforeStart(Context *) { SDL_ShowCursor(SDL_ENABLE); SDL_StartTextInput(); }
    void NetworkMenu::beforeClose(Context *) { SDL_StopTextInput(); runtime.reset(); }

    bool NetworkMenu::endpoint(Network::Endpoint &result) const {
        const std::string host = setupScreen == SetupScreen::Host ? hostAddress : address;
        if (host.empty() || host.size() > 253 || port.empty()) return false;
        try {
            std::size_t used = 0; const auto parsed = std::stoul(port, &used);
            if (used != port.size() || parsed == 0 || parsed > 65535) return false;
            result.host = host; result.port = static_cast<std::uint16_t>(parsed); return true;
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

    bool NetworkMenu::setupValid(std::string &reason) const {
        Network::Endpoint ignored;
        if (setupScreen == SetupScreen::Host && hostAddress.empty()) {
            reason = "No supported local loopback or private-LAN address is available."; return false;
        }
        if (!endpoint(ignored)) {
            reason = setupScreen == SetupScreen::Host ? "Enter a valid port (1–65535)."
                                                       : "Enter a valid address and port (1–65535).";
            return false;
        }
        if (localPlayers.empty()) { reason = "Add at least one local player."; return false; }
        const auto invalid = std::find_if(localPlayers.begin(), localPlayers.end(),
                [](const auto &player) { return player.controls == nullptr; });
        if (invalid != localPlayers.end()) {
            reason = "Assign a valid control to every local player."; return false;
        }
        reason = "Ready to start a same-machine or private-LAN session.";
        return true;
    }

    bool NetworkMenu::localReadyEligible(std::string &reason) const {
        const auto invalid = std::find_if(localPlayers.begin(), localPlayers.end(),
                [](const auto &player) { return player.controls == nullptr; });
        if (invalid != localPlayers.end()) {
            reason = "Assign a control to " + invalid->name + "."; return false;
        }
        return true;
    }

    bool NetworkMenu::startEligible(const Client::NetworkRuntimeSnapshot &snap, std::string &reason) const {
        if (!snap.canonical) { reason = "Waiting for authoritative lobby state."; return false; }
        const auto &state = *snap.canonical;
        if (state.messages.status == "Match settings are invalid. Correct the settings and try again.") {
            reason = state.messages.status; return false;
        }
        if (state.messages.status == "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.") {
            reason = state.messages.status; return false;
        }
        const auto connected = std::count_if(state.participants.begin(), state.participants.end(), [](const auto &p) {
            return p.connection == Network::Replication::ConnectionState::Connected;
        });
        if (connected < 2 || state.players.size() < 2) {
            reason = "Waiting for at least 2 connected participants and players."; return false;
        }
        const auto emptyOwner = std::find_if(state.participants.begin(), state.participants.end(), [](const auto &p) {
            return p.ownedPlayerIds.empty();
        });
        if (emptyOwner != state.participants.end()) {
            reason = "Waiting for every participant to own at least one player."; return false;
        }
        const auto reconnecting = std::find_if(state.participants.begin(), state.participants.end(), [](const auto &p) {
            return p.connection == Network::Replication::ConnectionState::Reconnecting;
        });
        if (reconnecting != state.participants.end()) {
            reason = "Waiting for " + participantLabel(state, reconnecting->participantId) + " to reconnect."; return false;
        }
        const auto unready = std::find_if(state.participants.begin(), state.participants.end(), [](const auto &p) {
            return !p.ready;
        });
        if (unready != state.participants.end()) {
            reason = "Waiting for " + participantLabel(state, unready->participantId) + " to be ready."; return false;
        }
        return true;
    }

    void NetworkMenu::cycleControl(std::size_t playerIndex, int direction) {
        if (playerIndex >= localPlayers.size()) return;
        const Size count = controlsManager.getNumAvailable();
        if (!count) { localPlayers[playerIndex].controls = nullptr; localPlayers[playerIndex].controlDescription.clear(); return; }
        Size current = 0;
        for (Size index = 0; index < count; ++index)
            if (controlsManager.get(index).getDescription() == localPlayers[playerIndex].controlDescription) current = index;
        current = static_cast<Size>((static_cast<int>(current) + direction + static_cast<int>(count)) % static_cast<int>(count));
        localPlayers[playerIndex].controls = &controlsManager.get(current);
        localPlayers[playerIndex].controlDescription = localPlayers[playerIndex].controls->getDescription();
        if (runtime.snapshot().journey == Client::NetworkJourney::Lobby) {
            runtime.rebindLocalPlayers(localPlayers);
            runtime.localConfigurationChanged();
        }
    }

    void NetworkMenu::rescanControls() {
        std::vector<std::string> retained;
        for (const auto &player: localPlayers) retained.push_back(player.controlDescription);
        controlsManager.detectJoypads();
        const Size count = controlsManager.getNumAvailable();
        for (std::size_t player = 0; player < localPlayers.size(); ++player) {
            localPlayers[player].controls = nullptr;
            for (Size control = 0; control < count; ++control)
                if (controlsManager.get(control).getDescription() == retained[player]) {
                    localPlayers[player].controls = &controlsManager.get(control); break;
                }
        }
        const auto journey = runtime.snapshot().journey;
        if (journey == Client::NetworkJourney::Lobby || journey == Client::NetworkJourney::Match
            || journey == Client::NetworkJourney::Summary)
            runtime.rebindLocalPlayers(localPlayers);
    }

    void NetworkMenu::joyDeviceAddedEvent(const JoyDeviceAddedEvent &) { rescanControls(); }
    void NetworkMenu::joyDeviceRemovedEvent(const JoyDeviceRemovedEvent &) { rescanControls(); }
    void NetworkMenu::mouseWheelEvent(const MouseWheelEvent &event) {
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Lobby
            && (!snap.canonical || !snap.canonical->result.available)) {
            setupScroll = std::max(0, setupScroll - event.getAmountY());
            return;
        }
        if (snap.journey != Client::NetworkJourney::Summary
            && !(snap.journey == Client::NetworkJourney::Lobby && snap.canonical && snap.canonical->result.available)) return;
        summaryScroll = std::max(0, summaryScroll - event.getAmountY());
        summaryHorizontal = std::max(0, summaryHorizontal + event.getAmountX() * 8);
    }

    void NetworkMenu::moveFocus(int direction) {
        const auto snap = runtime.snapshot();
        int count = 1;
        if (confirmation != Confirmation::None) count = 2;
        else if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) count = 3;
            else count = (setupScreen == SetupScreen::Join ? 2 : 1)
                         + static_cast<int>(availablePersons.size())
                         + static_cast<int>(localPlayers.size()) * 2 + 2;
        } else if (snap.journey == Client::NetworkJourney::Lobby) {
            count = static_cast<int>(localPlayers.size()) + 2;
            if (snap.host) count += 9 + static_cast<int>(snap.canonical ? snap.canonical->players.size() : 0) + 1;
        } else if (snap.journey == Client::NetworkJourney::Summary) count = snap.host ? 2 : 1;
        else if (snap.journey == Client::NetworkJourney::Failure) count = snap.retryAllowed ? 3 : 2;
        focus = (focus + direction + count) % count;
    }

    void NetworkMenu::activate() {
        auto snap = runtime.snapshot();
        if (confirmation != Confirmation::None) {
            if (focus == 0) {
                const auto accepted = confirmation; confirmation = Confirmation::None;
                setupScreen = SetupScreen::Entry;
                if (accepted == Confirmation::End) runtime.endSession(); else runtime.leave();
            } else { confirmation = Confirmation::None; focus = 0; }
            return;
        }
        if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) {
                if (focus == 0) setupScreen = SetupScreen::Host;
                else if (focus == 1) setupScreen = SetupScreen::Join;
                else { close(); return; }
                focus = 0; return;
            }
            const int fields = setupScreen == SetupScreen::Join ? 2 : 1;
            if (focus < fields) return;
            int action = focus - fields;
            if (action < static_cast<int>(availablePersons.size())) {
                const std::string &name = availablePersons[action];
                const auto selected = std::find_if(localPlayers.begin(), localPlayers.end(), [&](const auto &p) { return p.name == name; });
                if (selected == localPlayers.end() && localPlayers.size() < Network::MaxNetworkPlayers) {
                    Client::NetworkLocalPlayer player; player.name = name;
                    localPlayers.push_back(std::move(player)); cycleControl(localPlayers.size() - 1);
                }
                return;
            }
            action -= static_cast<int>(availablePersons.size());
            if (action < static_cast<int>(localPlayers.size()) * 2) {
                const std::size_t player = static_cast<std::size_t>(action / 2);
                if (action % 2 == 0) cycleControl(player);
                else localPlayers.erase(localPlayers.begin() + static_cast<std::ptrdiff_t>(player));
                return;
            }
            action -= static_cast<int>(localPlayers.size()) * 2;
            if (action == 1) { setupScreen = SetupScreen::Entry; focus = 0; return; }
            std::string reason; Network::Endpoint target;
            if (!setupValid(reason) || !endpoint(target)) return;
            hostSetup.localPlayerNames.clear();
            for (const auto &player: localPlayers) hostSetup.localPlayerNames.push_back(player.name);
            if (setupScreen == SetupScreen::Host)
                (void) runtime.startHost(target, serverExecutable(), "resources", hostSetup, localPlayers);
            else (void) runtime.join(target, "resources", localPlayers);
            focus = 0; return;
        }
        if (snap.journey == Client::NetworkJourney::Starting) { runtime.cancel(); return; }
        if (snap.journey == Client::NetworkJourney::Lobby) {
            if (focus < static_cast<int>(localPlayers.size())) { cycleControl(static_cast<std::size_t>(focus)); return; }
            int action = focus - static_cast<int>(localPlayers.size());
            bool ready = false;
            if (snap.canonical) {
                const auto participant = std::find_if(snap.canonical->participants.begin(), snap.canonical->participants.end(),
                        [&snap](const auto &value) { return value.participantId == snap.localParticipantId; });
                if (participant != snap.canonical->participants.end()) ready = participant->ready;
            }
            if (action == 0) { std::string reason; if (localReadyEligible(reason)) runtime.setReady(!ready); return; }
            --action;
            if (snap.host) {
                const bool contentBlocked = snap.canonical
                        && snap.canonical->messages.status == "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.";
                const int rosterCount = static_cast<int>(snap.canonical ? snap.canonical->players.size() : 0);
                if (contentBlocked && action < 10 + rosterCount) return;
                bool changed = true;
                if (action == 0) {
                    hostSetup.mode = hostSetup.mode == "Deathmatch" ? "Predator"
                                     : hostSetup.mode == "Predator" ? "Team deathmatch" : "Deathmatch";
                    hostSetup.teamCount = hostSetup.mode == "Team deathmatch" ? 2 : 0;
                } else if (action == 1) {
                    if (hostSetup.mode != "Team deathmatch") { hostSetup.mode = "Team deathmatch"; hostSetup.teamCount = 2; }
                    else hostSetup.teamCount = hostSetup.teamCount >= 4 ? 2 : hostSetup.teamCount + 1;
                }
                else if (action == 2) {
                    if (hostSetup.mode != "Team deathmatch") { hostSetup.mode = "Team deathmatch"; hostSetup.teamCount = 2; }
                    hostSetup.friendlyFire = !hostSetup.friendlyFire;
                }
                else if (action == 3) hostSetup.levelPlan = hostSetup.levelPlan == "Fixed level" ? "Shuffle all levels"
                                                        : hostSetup.levelPlan == "Shuffle all levels" ? "Random level" : "Fixed level";
                else if (action == 4 && !availableLevels.empty()) {
                    hostSetup.levelPlan = "Fixed level";
                    const auto current = std::find(availableLevels.begin(), availableLevels.end(), hostSetup.fixedLevel);
                    const std::size_t index = current == availableLevels.end() ? 0
                            : (static_cast<std::size_t>(std::distance(availableLevels.begin(), current)) + 1) % availableLevels.size();
                    hostSetup.fixedLevel = availableLevels[index];
                } else if (action == 5) hostSetup.roundLimit = hostSetup.roundLimit >= 99 ? 1 : hostSetup.roundLimit + 1;
                else if (action == 6) hostSetup.assistance = !hostSetup.assistance;
                else if (action == 7) hostSetup.quickLiquid = !hostSetup.quickLiquid;
                else if (action == 8) hostSetup.burnableTrees = !hostSetup.burnableTrees;
                else changed = false;
                if (changed) runtime.updateHostSetup(hostSetup);
                else if (action >= 9 && action < 9 + rosterCount && snap.canonical) {
                    std::vector<Network::Replication::PlayerState> roster = snap.canonical->players;
                    std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
                        return left.rosterPosition < right.rosterPosition;
                    });
                    const int rosterIndex = action - 9;
                    runtime.moveRosterPlayer(roster[static_cast<std::size_t>(rosterIndex)].playerId,
                                             rosterIndex == 0 ? 1 : -1);
                }
                else if (action == 9 + rosterCount) {
                    std::string reason; if (startEligible(snap, reason)) runtime.startMatch();
                }
                else { confirmation = Confirmation::End; focus = 0; }
            } else { confirmation = Confirmation::Leave; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Summary) {
            if (snap.host && focus == 0) runtime.returnToLobby();
            else { confirmation = snap.host ? Confirmation::End : Confirmation::Leave; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Reconnecting) {
            confirmation = Confirmation::Leave; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            int action = focus;
            if (!snap.retryAllowed) ++action;
            if (action == 0) {
                Network::Endpoint target; if (!endpoint(target)) return;
                runtime.reset();
                if (snap.host) (void) runtime.startHost(target, serverExecutable(), "resources", hostSetup, localPlayers);
                else (void) runtime.join(target, "resources", localPlayers);
            } else if (action == 1) { runtime.reset(); setupScreen = snap.host ? SetupScreen::Host : SetupScreen::Join; }
            else { runtime.reset(); setupScreen = SetupScreen::Entry; }
            focus = 0;
        }
    }

    void NetworkMenu::back() {
        const auto snap = runtime.snapshot();
        if (confirmation != Confirmation::None) { confirmation = Confirmation::None; focus = 0; return; }
        if (snap.journey == Client::NetworkJourney::Starting) runtime.cancel();
        else if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) close(); else { setupScreen = SetupScreen::Entry; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::Lobby) {
            focus = static_cast<int>(localPlayers.size()) + 1;
            if (snap.host) focus += 10 + static_cast<int>(snap.canonical ? snap.canonical->players.size() : 0);
        } else if (snap.journey == Client::NetworkJourney::Match
                   || snap.journey == Client::NetworkJourney::Summary
                   || snap.journey == Client::NetworkJourney::Reconnecting) {
            confirmation = snap.host ? Confirmation::End : Confirmation::Leave; focus = 0;
        }
    }

    void NetworkMenu::keyEvent(const KeyPressEvent &event) {
        if (!event.isPressed()) {
            if (event.getCode() == SDLK_TAB) scoreOverlay = false;
            return;
        }
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Match && confirmation == Confirmation::None) {
            if (event.getCode() == SDLK_TAB) { scoreOverlay = true; return; }
            if (event.getCode() == SDLK_F9 && snap.host && snap.canonical && snap.canonical->round
                && (!snap.canonical->round->outcome.winnerPlayerIds.empty()
                    || snap.canonical->round->outcome.winningTeam || snap.canonical->round->outcome.noWinner)) {
                runtime.advanceRound(); return;
            }
        }
        if ((snap.journey == Client::NetworkJourney::Summary
             || (snap.journey == Client::NetworkJourney::Lobby && snap.canonical && snap.canonical->result.available))
            && (event.getCode() == SDLK_PAGEUP || event.getCode() == SDLK_PAGEDOWN)) {
            summaryScroll = std::max(0, summaryScroll + (event.getCode() == SDLK_PAGEDOWN ? 8 : -8));
            return;
        }
        if ((snap.journey == Client::NetworkJourney::Summary
             || (snap.journey == Client::NetworkJourney::Lobby && snap.canonical && snap.canonical->result.available))
            && (event.getCode() == SDLK_LEFT || event.getCode() == SDLK_RIGHT)) {
            summaryHorizontal = std::max(0, summaryHorizontal + (event.getCode() == SDLK_RIGHT ? 8 : -8));
            return;
        }
        if (event.getCode() == SDLK_ESCAPE) back();
        else if (event.getCode() == SDLK_TAB || event.getCode() == SDLK_DOWN) moveFocus(1);
        else if (event.getCode() == SDLK_UP) moveFocus(-1);
        else if (event.getCode() == SDLK_RETURN || event.getCode() == SDLK_SPACE) activate();
        else if ((setupScreen == SetupScreen::Host || setupScreen == SetupScreen::Join)
                 && snap.journey == Client::NetworkJourney::Inactive) {
            std::string *field = setupScreen == SetupScreen::Join && focus == 0 ? &address : &port;
            if ((focus == 0 || (setupScreen == SetupScreen::Join && focus == 1))
                && event.getCode() == SDLK_BACKSPACE && !field->empty()) field->pop_back();
        }
    }

    void NetworkMenu::textInputEvent(const TextInputEvent &event) {
        if (runtime.snapshot().journey != Client::NetworkJourney::Inactive || setupScreen == SetupScreen::Entry) return;
        const bool addressField = setupScreen == SetupScreen::Join && focus == 0;
        const bool portField = focus == (setupScreen == SetupScreen::Join ? 1 : 0);
        if (!addressField && !portField) return;
        std::string *field = addressField ? &address : &port;
        for (char c: std::string(event.getText())) {
            const bool allowed = portField ? c >= '0' && c <= '9'
                                           : std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == ':';
            if (allowed && field->size() < (portField ? 5u : 253u)) field->push_back(c);
        }
    }

    void NetworkMenu::update(Float32 elapsedTime) {
        runtime.update();
        const auto currentSnapshot = runtime.snapshot();
        worldPresenter.update(elapsedTime, currentSnapshot.canonical ? &*currentSnapshot.canonical : nullptr);
        for (auto iterator = playerStatusRemaining.begin(); iterator != playerStatusRemaining.end();) {
            iterator->second -= elapsedTime;
            if (iterator->second <= 0) iterator = playerStatusRemaining.erase(iterator); else ++iterator;
        }
        if (currentSnapshot.canonical)
            for (const auto playerId: currentSnapshot.canonical->messages.currentPlayerIndicators)
                playerStatusRemaining[playerId] = 5.0f;
        bool sessionBack = false;
        for (const auto &controller: service.getInput().getJoys())
            if (controller.isPressed(GameController::CONTROLLER_BUTTON_BACK)) {
                sessionBack = true; break;
            }
        if (sessionBack && !controllerSessionBack) back();
        controllerSessionBack = sessionBack;
        if (!localPlayers.empty() && localPlayers[0].controls
            && (currentSnapshot.journey != Client::NetworkJourney::Match || confirmation != Confirmation::None)) {
            const auto &c = *localPlayers[0].controls;
            const bool confirmNow = c.getShoot().isPressed(), backNow = c.getPick().isPressed();
            const bool up = c.getUp().isPressed(), down = c.getDown().isPressed();
            if (confirmNow && !controllerConfirm) activate();
            if (backNow && !controllerBack) back();
            if (up && !controllerUp) moveFocus(-1);
            if (down && !controllerDown) moveFocus(1);
            controllerConfirm = confirmNow; controllerBack = backNow; controllerUp = up; controllerDown = down;
        }
        if (currentSnapshot.journey != lastJourney) {
            focus = currentSnapshot.journey == Client::NetworkJourney::Failure
                    && currentSnapshot.host && currentSnapshot.retryAllowed
                    && currentSnapshot.failure == "The selected port is unavailable. Choose another port and try again."
                    ? 1 : 0;
            confirmation = Confirmation::None; scoreOverlay = false; summaryScroll = 0; summaryHorizontal = 0;
            if (currentSnapshot.journey != Client::NetworkJourney::Match) playerStatusRemaining.clear();
            lastJourney = currentSnapshot.journey;
        }
    }

    void NetworkMenu::drawText(Int32 x, Int32 y, const std::string &text, Color color) const { font.print(x, y, color, text); }
    void NetworkMenu::drawAction(Int32 y, const std::string &text, bool selected) const {
        renderer.quadXY(Vector(275, y), Vector(300, 32), selected ? Color(64, 96, 160) : Color(192));
        drawText(425 - static_cast<Int32>(text.size()) * 4, y + 8, text, selected ? Color::WHITE : Color::BLACK);
    }

    void NetworkMenu::drawPlayers(const Network::Replication::CanonicalState &state) const {
        Int32 y = 492;
        int row = 0;
        for (std::size_t participantIndex = 0; participantIndex < state.participants.size() && y > 270; ++participantIndex) {
            const auto &participant = state.participants[participantIndex];
            if (row++ >= setupScroll) {
                drawText(54, y, participantLabel(state, participant.participantId));
                drawText(155, y, participant.connection == Network::Replication::ConnectionState::Connected ? "Connected" : "Reconnecting");
                drawText(270, y, participant.ready ? "Ready" : "Not ready"); y -= 18;
            }
            for (auto id: participant.ownedPlayerIds) {
                const auto player = std::find_if(state.players.begin(), state.players.end(), [id](const auto &p) { return p.playerId == id; });
                if (row++ >= setupScroll && y > 270) {
                    if (player != state.players.end()) drawText(72, y, std::to_string(player->rosterPosition + 1) + ". " + player->displayName);
                    y -= 16;
                }
            }
        }
    }

    void NetworkMenu::drawMatch(const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        if (!snap.canonical) return;
        worldPresenter.render(*snap.canonical, snap.presentation, snap.presentedPlayers, width, height);
        renderer.setViewMatrix(Matrix::IDENTITY);
        const auto &state = *snap.canonical;
        Int32 y = height - 24;
        drawText(16, y, "Round " + std::to_string(state.currentRoundNumber) + "/" + std::to_string(state.settings.roundLimit)
                         + " • " + state.messages.status, Color::WHITE);
        y -= 18;
        for (const auto &event: state.messages.events) {
            drawText(16, y, event, Color::WHITE); y -= 16; if (y < height - 72) break;
        }
        Int32 rankY = height - 24;
        for (std::size_t rank = 0; rank < state.score.ranking.size() && rank < 8; ++rank) {
            const auto id = state.score.ranking[rank];
            const auto player = std::find_if(state.players.begin(), state.players.end(), [id](const auto &p) { return p.playerId == id; });
            const auto score = std::find_if(state.score.players.begin(), state.score.players.end(), [id](const auto &p) { return p.playerId == id; });
            if (player != state.players.end()) {
                const std::string line = std::to_string(rank + 1) + ". " + player->displayName + "  "
                        + (score == state.score.players.end() ? "0" : std::to_string(score->cumulativePoints));
                drawText(width - 16 - static_cast<Int32>(line.size()) * 8, rankY, line, Color::WHITE); rankY -= 16;
            }
        }
        Int32 statusY = 82;
        for (const auto &player: state.players) {
            const auto status = playerStatusRemaining.find(player.playerId);
            if (status == playerStatusRemaining.end()) continue;
            drawText(16, statusY, player.displayName + " • Life " + std::to_string(player.life)
                             + " • " + player.heldWeapon + " " + std::to_string(player.ammunition), Color::WHITE);
            statusY += 16;
        }
        if (state.phase == Network::Replication::Phase::RoundSummary) {
            const unsigned seconds = static_cast<unsigned>((state.roundEndCountdown + 59) / 60);
            drawText(width / 2 - 120, height / 2 + 40, "Round outcome: " + outcome(state, state.score.winner), Color::WHITE);
            drawText(width / 2 - 80, height / 2 + 18, "Next round in " + std::to_string(seconds), Color::WHITE);
            if (snap.host) drawText(width / 2 - 115, height / 2 - 4, "F9 Advance round • Esc End session", Color::WHITE);
        }
        const Int32 statusHeight = snap.presentation.resynchronizing || snap.presentation.degraded ? 44 : 26;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(16, 16), Vector(width - 32, statusHeight), Color(16, 20, 28, 208));
        renderer.setBlendFunc(BlendFunc::None);
        drawText(20, 20, std::string(snap.host ? "Host" : "Guest") + " • LAN session • Connected", Color::WHITE);
        const std::string right = "Session only scores • Optional scripts disabled";
        drawText(std::max(20, width - 20 - static_cast<Int32>(right.size()) * 8), 20, right, Color::WHITE);
        if (snap.presentation.degraded) drawText(20, 38, "Network connection degraded.", Color::WHITE);
        if (snap.presentation.resynchronizing) {
            drawText(20, 38, "Last confirmed state • Synchronizing current state…", Color::WHITE);
        }
        if (scoreOverlay) drawResult(state, false);
        if (confirmation != Confirmation::None) drawConfirmation();
    }

    void NetworkMenu::drawReconnectPanel(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        const Int32 panelWidth = std::min<Int32>(640, width - 32);
        const Int32 panelHeight = std::min<Int32>(250, height - 32);
        const Int32 x = (width - panelWidth) / 2;
        const Int32 y = (height - panelHeight) / 2;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(x, y), Vector(panelWidth, panelHeight), Color(224, 224, 224, 244));
        renderer.setBlendFunc(BlendFunc::None);
        drawText(x + 24, y + panelHeight - 38,
                 "Reconnecting to " + snap.endpoint.host + ':' + std::to_string(snap.endpoint.port) + "…");
        drawText(x + 24, y + panelHeight - 70,
                 std::to_string(std::max(1u, snap.reconnectSeconds.value_or(1))) + " seconds remaining");
        drawText(x + 24, y + panelHeight - 102, "Your player slots are reserved");
        if (snap.canonical && (snap.canonical->phase == Network::Replication::Phase::ActiveRound
                               || snap.canonical->phase == Network::Replication::Phase::RoundSummary)) {
            drawText(x + 24, y + panelHeight - 130, "Match continues while you reconnect");
            drawText(x + 24, y + panelHeight - 154,
                     "Reserved players receive no input and remain in play");
        }
        renderer.quadXY(Vector(x + panelWidth / 2 - 100, y + 24), Vector(200, 34), Color(64, 96, 160));
        drawText(x + panelWidth / 2 - 52, y + 32, "Leave session", Color::WHITE);
    }

    void NetworkMenu::drawResult(const Network::Replication::CanonicalState &state, bool retained) const {
        const Int32 top = retained ? 365 : 610;
        const Int32 bottom = retained || state.phase == Network::Replication::Phase::FinalSummary ? 120 : 62;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(32, bottom), Vector(786, top - bottom), Color(224, 224, 224, 240));
        renderer.setBlendFunc(BlendFunc::None);
        drawText(50, top - 28, retained ? "RETAINED RESULT • SESSION ONLY" : "AUTHORITATIVE SCORE • SESSION ONLY");
        drawText(50, top - 52, "State: " + (state.result.available ? state.result.state : "In progress"));
        drawText(50, top - 74, "Match outcome: " + outcome(state, state.score.winner));
        if (state.round) drawText(50, top - 96, "Last completed round " + std::to_string(state.completedRounds)
                                          + ": " + outcome(state, state.round->outcome));
        auto lines = resultLines(state);
        Int32 y = top - 128;
        const std::size_t first = std::min<std::size_t>(summaryScroll, lines.size());
        for (std::size_t index = first; index < lines.size() && y > bottom + 18; ++index, y -= 18) {
            const std::size_t horizontal = std::min<std::size_t>(summaryHorizontal, lines[index].size());
            drawText(50, y, lines[index].substr(horizontal, 94));
        }
        drawText(620, bottom + 8, "Arrows / wheel: scroll");
    }

    void NetworkMenu::drawConfirmation() const {
        renderer.setViewMatrix(Matrix::IDENTITY);
        const auto width = service.getVideo().getScreen().getClientWidth();
        const auto height = service.getVideo().getScreen().getClientHeight();
        renderer.quadXY(Vector(width / 2 - 260, height / 2 - 90), Vector(520, 180), Color(224));
        const auto journey = runtime.snapshot().journey;
        std::string prompt = confirmation == Confirmation::End ? "End session for everyone?"
                : journey == Client::NetworkJourney::Reconnecting
                  ? "Leave session? Your reserved players will be removed now"
                : journey == Client::NetworkJourney::Match
                  ? "Leave session? Your players will be removed immediately"
                  : "Leave session? Your players will be removed";
        std::string promptSecond = confirmation == Confirmation::End ? std::string()
                : journey == Client::NetworkJourney::Reconnecting
                  ? "and reconnect will stop."
                : journey == Client::NetworkJourney::Match
                  ? "and the match will continue without reconnect."
                  : "and you will return to Network.";
        drawText(width / 2 - static_cast<Int32>(prompt.size()) * 4, height / 2 + 60, prompt);
        if (!promptSecond.empty())
            drawText(width / 2 - static_cast<Int32>(promptSecond.size()) * 4, height / 2 + 40, promptSecond);
        renderer.quadXY(Vector(width / 2 - 190, height / 2 - 55), Vector(160, 34), focus == 0 ? Color(64, 96, 160) : Color(192));
        renderer.quadXY(Vector(width / 2 + 30, height / 2 - 55), Vector(160, 34), focus == 1 ? Color(64, 96, 160) : Color(192));
        drawText(width / 2 - 135, height / 2 - 47, confirmation == Confirmation::End ? "End session" : "Leave session",
                 focus == 0 ? Color::WHITE : Color::BLACK);
        drawText(width / 2 + 90, height / 2 - 47, "Cancel", focus == 1 ? Color::WHITE : Color::BLACK);
    }

    void NetworkMenu::render() const {
        const auto width = service.getVideo().getScreen().getClientWidth();
        const auto height = service.getVideo().getScreen().getClientHeight();
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Reconnecting && snap.canonical
            && (snap.canonical->phase == Network::Replication::Phase::ActiveRound
                || snap.canonical->phase == Network::Replication::Phase::RoundSummary)) {
            worldPresenter.render(*snap.canonical, snap.presentation, snap.presentedPlayers, width, height);
            renderer.setViewMatrix(Matrix::IDENTITY);
            drawReconnectPanel(snap, width, height);
            if (confirmation != Confirmation::None) drawConfirmation();
            return;
        }
        if (snap.journey == Client::NetworkJourney::Match && snap.canonical) {
            drawMatch(snap, width, height); return;
        }
        const Float32 scale = std::min(Float32(width) / CanvasWidth, Float32(height) / CanvasHeight);
        const Int32 tx = (width - Int32(CanvasWidth * scale)) / 2, ty = (height - Int32(CanvasHeight * scale)) / 2;
        renderer.setViewMatrix(Matrix::translate(Float32(tx), Float32(ty), 0) * Matrix::scale(scale, scale, 1));
        renderer.quadXY(Vector(0, 0), Vector(CanvasWidth, CanvasHeight), Color(192));
        renderer.quadXY(Vector(24, 48), Vector(802, 602), Color(224));
        std::string title = "NETWORK PLAY";
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Host) title = "HOST NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Join) title = "JOIN NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Lobby) title = "NETWORK LOBBY";
        else if (snap.journey == Client::NetworkJourney::Summary) title = "MATCH SUMMARY";
        else if (snap.journey == Client::NetworkJourney::Reconnecting) title = "RECONNECTING";
        else if (snap.journey == Client::NetworkJourney::Failure) title = snap.host ? "SESSION ENDED" : "CONNECTION FAILED";
        else if (snap.journey == Client::NetworkJourney::HostEnded) title = "HOST ENDED SESSION";
        drawText(425 - static_cast<Int32>(title.size()) * 4, 620, title);

        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Entry) {
            drawText(285, 555, "Same machine or private LAN"); drawText(285, 530, "Direct address and port");
            drawText(285, 505, "Linux / Windows x86-64");
            drawText(165, 465, "Player-hosted • Lobby 1–15 • Match 2–15 participants and players");
            drawAction(375, "Host", focus == 0); drawAction(330, "Join", focus == 1); drawAction(285, "Back", focus == 2);
        } else if (snap.journey == Client::NetworkJourney::Inactive) {
            const int fields = setupScreen == SetupScreen::Join ? 2 : 1;
            Int32 y = 570;
            if (setupScreen == SetupScreen::Join) { drawText(50, y, "Address: " + address + (focus == 0 ? " <" : "")); y -= 24; }
            drawText(50, y, "Port: " + port + (focus == fields - 1 ? " <" : ""));
            if (setupScreen == SetupScreen::Host && !hostAddress.empty())
                drawText(430, y, "Listening address: " + hostAddress);
            drawText(50, 512, "PERSONS"); drawText(430, 512, "LOCAL PLAYERS AND CONTROLS");
            const int focusedPerson = focus >= fields && focus < fields + static_cast<int>(availablePersons.size())
                                      ? focus - fields : 0;
            const std::size_t firstPerson = static_cast<std::size_t>(std::max(0, focusedPerson - 9));
            y = 490;
            for (std::size_t index = firstPerson; index < availablePersons.size() && index < firstPerson + 10; ++index, y -= 18) {
                const bool selected = std::any_of(localPlayers.begin(), localPlayers.end(), [&](const auto &p) { return p.name == availablePersons[index]; });
                drawText(54, y, (focus == fields + static_cast<int>(index) ? "> " : "  ") + availablePersons[index]
                                + (selected ? " • Selected" : " • Add"));
            }
            y = 490; const int playerBase = fields + static_cast<int>(availablePersons.size());
            const int focusedPlayer = focus >= playerBase && focus < playerBase + static_cast<int>(localPlayers.size()) * 2
                                      ? (focus - playerBase) / 2 : 0;
            const std::size_t firstPlayer = static_cast<std::size_t>(std::max(0, focusedPlayer - 9));
            for (std::size_t index = firstPlayer; index < localPlayers.size() && index < firstPlayer + 10; ++index, y -= 22) {
                drawText(434, y, (focus == playerBase + static_cast<int>(index) * 2 ? "> " : "  ")
                                + localPlayers[index].name + " • "
                                + (localPlayers[index].controls ? localPlayers[index].controls->getDescription() : "No control"));
                drawText(730, y, focus == playerBase + static_cast<int>(index) * 2 + 1 ? "> Remove" : "Remove");
            }
            std::string reason; const bool valid = setupValid(reason);
            drawText(50, 182, "Local players: " + std::to_string(localPlayers.size()) + " • Lobby 1–15 • Match 2–15");
            drawText(50, 160, "Same machine or LAN • Session-only scores • Optional scripts disabled");
            drawText(50, 138, reason);
            const int footer = playerBase + static_cast<int>(localPlayers.size()) * 2;
            drawAction(82, valid ? (setupScreen == SetupScreen::Host ? "Start session" : "Connect")
                                 : (setupScreen == SetupScreen::Host ? "Start session (disabled)" : "Connect (disabled)"), focus == footer);
            drawAction(38, "Back", focus == footer + 1);
        } else if (snap.journey == Client::NetworkJourney::Starting || snap.journey == Client::NetworkJourney::Cancelling) {
            drawText(300, 430, snap.status);
            if (snap.journey == Client::NetworkJourney::Starting) {
                drawText(270, 400, "Startup can take up to 10 seconds."); drawAction(300, "Cancel", true);
            }
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Lobby) {
            const auto &state = *snap.canonical;
            drawText(50, 582, (snap.host ? "Host" : "Guest") + std::string(" • LAN session • ")
                             + snap.endpoint.host + ":" + std::to_string(snap.endpoint.port) + " • "
                             + std::to_string(state.participants.size()) + " participants • "
                             + std::to_string(state.players.size()) + " players");
            drawText(50, 552, "Role        Connection       Readiness"); drawPlayers(state);
            drawText(410, 552, "HOST MATCH SETTINGS");
            drawText(410, 526, "Mode: " + state.settings.mode);
            drawText(410, 506, "Teams: " + std::to_string(state.settings.teamCount) + " • Friendly fire " + onOff(state.settings.friendlyFire));
            drawText(410, 486, "Level plan: " + state.settings.levelPlan);
            if (state.settings.levelPlan == "Fixed level") drawText(410, 466, "Level: " + state.settings.fixedLevel);
            drawText(410, 446, "Rounds: " + std::to_string(state.settings.roundLimit));
            drawText(410, 426, "Assistance " + onOff(state.settings.assistance) + " • Quick Liquid " + onOff(state.settings.quickLiquid));
            drawText(410, 406, "Burnable Trees " + onOff(state.settings.burnableTrees));
            const std::size_t firstControl = focus < static_cast<int>(localPlayers.size())
                                             ? static_cast<std::size_t>(std::max(0, focus - 7)) : 0;
            Int32 cy = 245;
            for (std::size_t index = firstControl; index < localPlayers.size() && index < firstControl + 8; ++index, cy -= 18)
                drawText(50, cy, (focus == static_cast<int>(index) ? "> " : "  ") + localPlayers[index].name
                                + " control: " + (localPlayers[index].controls ? localPlayers[index].controls->getDescription() : "No control"));
            const int readyIndex = static_cast<int>(localPlayers.size());
            drawText(50, 192, focus == readyIndex ? "> Ready / Not ready" : "Ready / Not ready");
            if (state.messages.status != "Lobby") drawText(50, 142, state.messages.status);
            if (snap.host) {
                static const char *labels[] = {"Mode", "Team count", "Friendly fire", "Level plan", "Fixed level",
                                               "Round limit", "Assistance", "Quick Liquid", "Burnable Trees"};
                for (int index = 0; index < 9; ++index)
                    drawText(410, 370 - index * 18, focus == readyIndex + 1 + index ? std::string("> ") + labels[index] : labels[index]);
                std::vector<Network::Replication::PlayerState> roster = state.players;
                std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
                    return left.rosterPosition < right.rosterPosition;
                });
                drawText(575, 370, "ROSTER ORDER • Enter reorders");
                const int focusedRoster = focus >= readyIndex + 10
                                          && focus < readyIndex + 10 + static_cast<int>(roster.size())
                                          ? focus - readyIndex - 10 : 0;
                const std::size_t firstRoster = static_cast<std::size_t>(std::max(0, focusedRoster - 7));
                for (std::size_t index = firstRoster; index < roster.size() && index < firstRoster + 8; ++index)
                    drawText(575, 350 - static_cast<Int32>(index - firstRoster) * 18,
                             (focus == readyIndex + 10 + static_cast<int>(index) ? "> " : "  ")
                             + std::to_string(index + 1) + ". " + roster[index].displayName);
                std::string reason; const bool eligible = startEligible(snap, reason);
                drawText(50, 118, reason.empty() ? "All participants are ready." : reason);
                drawText(50, 98, "Optional scripts are disabled for network play.");
                const int startIndex = readyIndex + 10 + static_cast<int>(roster.size());
                drawText(410, 98, focus == startIndex ? (eligible ? "> Start match" : "> Start match (disabled)")
                                                          : (eligible ? "Start match" : "Start match (disabled)"));
                drawText(675, 62, focus == startIndex + 1 ? "> End session" : "End session");
            } else {
                drawText(50, 118, "Host settings and authoritative roster order are read-only.");
                drawText(50, 98, "Optional scripts are disabled for network play.");
                drawText(650, 62, focus == readyIndex + 1 ? "> Leave session" : "Leave session");
            }
            if (state.result.available) drawResult(state, true);
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Summary) {
            const auto &state = *snap.canonical;
            drawText(50, 588, "SESSION ONLY • Completed • Not saved to local statistics or Elo");
            drawText(50, 566, "Match outcome: " + outcome(state, state.score.winner));
            if (state.round) drawText(50, 544, "Last completed round " + std::to_string(state.completedRounds)
                                               + ": " + outcome(state, state.round->outcome));
            drawResult(state, false);
            if (snap.host) { drawAction(72, "Return to lobby", focus == 0); drawAction(30, "End session", focus == 1); }
            else { drawText(50, 70, "Waiting for host to return to lobby or end session"); drawAction(30, "Leave", true); }
        } else if (snap.journey == Client::NetworkJourney::Reconnecting) {
            if (snap.canonical && snap.canonical->phase == Network::Replication::Phase::FinalSummary)
                drawResult(*snap.canonical, false);
            else if (snap.canonical && snap.canonical->phase == Network::Replication::Phase::Lobby)
                drawPlayers(*snap.canonical);
            drawReconnectPanel(snap, CanvasWidth, CanvasHeight);
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            drawText(130, 470, snap.failure.empty() ? "Connection could not be completed." : snap.failure);
            if (!snap.host) drawText(130, 440, "Endpoint: " + snap.endpoint.host + ':' + std::to_string(snap.endpoint.port));
            int selected = 0;
            drawAction(340, snap.retryAllowed ? "Retry" : "Retry unavailable", snap.retryAllowed && focus == selected++);
            if (!snap.retryAllowed) {
                const bool restart = snap.failure.find("gameplay content is invalid") != std::string::npos;
                drawText(275, 320, restart ? "Restart the application to try again."
                                           : "This session cannot be restored. Edit setup to start a new session.");
            }
            drawAction(275, "Edit setup", focus == selected++); drawAction(230, "Return to Network", focus == selected);
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            drawText(175, 440, "The host ended this session. It cannot migrate or resume.");
            drawText(245, 410, "Session-only scores were not saved."); drawAction(315, "Return to Network", true);
        }
        if (confirmation != Confirmation::None) drawConfirmation();
        renderer.setViewMatrix(Matrix::IDENTITY);
    }
}
