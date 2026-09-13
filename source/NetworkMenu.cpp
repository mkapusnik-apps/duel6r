#include "NetworkMenu.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <sstream>

#include "Defines.h"
#include "json/JsonParser.h"
#include "network/NetworkTrustPolicy.h"

namespace Duel6 {
    namespace {
        constexpr Int32 CanvasWidth = 850, CanvasHeight = 700;
        constexpr Float32 CanvasMaximumScale = 1.35f;

        Float32 canvasScale(Int32 width, Int32 height) {
            return std::min(CanvasMaximumScale,
                    std::min(Float32(width) / CanvasWidth, Float32(height) / CanvasHeight));
        }

        std::string onOff(bool value) { return value ? "On" : "Off"; }

        std::string listeningAddressLabel(const std::string &address) {
            if (address.empty()) return "Select an eligible interface";
            return address + (address == "127.0.0.1" ? " (Same machine)" : " (Private LAN)");
        }

        std::string levelDisplayName(std::string value) {
            const auto slash = value.find_last_of("/\\");
            if (slash != std::string::npos) value.erase(0, slash + 1);
            const auto extension = value.rfind('.');
            if (extension != std::string::npos) value.erase(extension);
            bool capitalize = true;
            for (char &character: value) {
                if (character == '_' || character == '-') {
                    character = ' ';
                    capitalize = true;
                } else if (capitalize && std::isalpha(static_cast<unsigned char>(character))) {
                    character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
                    capitalize = false;
                } else capitalize = character == ' ';
            }
            return value.empty() ? "Unknown level" : value;
        }

        std::size_t utf8CharacterBytes(unsigned char lead) {
            return lead < 0x80 ? 1 : (lead & 0xe0) == 0xc0 ? 2 : (lead & 0xf0) == 0xe0 ? 3 : 4;
        }

        std::size_t utf8Length(std::string_view value) {
            std::size_t result = 0;
            for (std::size_t offset = 0; offset < value.size(); ++result) {
                const std::size_t bytes = utf8CharacterBytes(static_cast<unsigned char>(value[offset]));
                offset += std::min(bytes, value.size() - offset);
            }
            return result;
        }

        std::size_t utf8ByteOffset(std::string_view value, std::size_t characters) {
            std::size_t offset = 0;
            while (offset < value.size() && characters-- > 0) {
                const std::size_t bytes = utf8CharacterBytes(static_cast<unsigned char>(value[offset]));
                offset += std::min(bytes, value.size() - offset);
            }
            return offset;
        }

        std::string utf8Clipped(const std::string &value, std::size_t characters) {
            if (utf8Length(value) <= characters) return value;
            if (characters <= 1) return value.substr(0, utf8ByteOffset(value, characters));
            return value.substr(0, utf8ByteOffset(value, characters - 1)) + "…";
        }

        std::string utf8Slice(const std::string &value, std::size_t first, std::size_t characters) {
            const std::size_t begin = utf8ByteOffset(value, first);
            const std::string_view remainder(value.data() + begin, value.size() - begin);
            return value.substr(begin, utf8ByteOffset(remainder, characters));
        }

        bool pointerInside(Int32 x, Int32 y, Int32 left, Int32 bottom, Int32 width, Int32 height) {
            return x >= left && x < left + width && y >= bottom && y < bottom + height;
        }

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

        std::string playerLifecycleLabel(const Network::Replication::CanonicalState &state,
                                         const Network::Replication::PlayerState &player) {
            if (player.lifeState == Network::Replication::LifeState::Departed) return "Departed";
            if (player.lifeState == Network::Replication::LifeState::Dead) return "Dead";
            const auto owner = std::find_if(state.participants.begin(), state.participants.end(), [&](const auto &value) {
                return value.participantId == player.ownerParticipantId;
            });
            return owner != state.participants.end()
                   && owner->connection == Network::Replication::ConnectionState::Reconnecting
                   ? "Reconnecting" : std::string();
        }

        struct RankingLine {
            std::string text;
            Color color;
        };

        std::vector<RankingLine> rankingLines(const Network::Replication::CanonicalState &state) {
            std::vector<RankingLine> lines;
            static const char *teamNames[] = {"", "Alpha", "Bravo", "Charlie", "Delta"};
            if (!state.score.teamRanking.empty()) {
                for (const auto team: state.score.teamRanking) {
                    if (team == 0 || team >= 5) continue;
                    const auto total = team <= state.score.teamTotals.size() ? state.score.teamTotals[team - 1] : 0;
                    const Color color = team == 1 ? Color::RED : team == 2 ? Color::GREEN
                            : team == 3 ? Color::YELLOW : Color::MAGENTA;
                    lines.push_back({std::string(teamNames[team]) + "  " + std::to_string(total), color});
                    for (const auto id: state.score.ranking) {
                        const auto player = std::find_if(state.players.begin(), state.players.end(),
                                [id, team](const auto &entry) { return entry.playerId == id && entry.team == team; });
                        if (player == state.players.end()) continue;
                        const auto score = std::find_if(state.score.players.begin(), state.score.players.end(),
                                [id](const auto &entry) { return entry.playerId == id; });
                        const std::string lifecycle = playerLifecycleLabel(state, *player);
                        lines.push_back({"  " + utf8Clipped(player->displayName, lifecycle.empty() ? 18 : 12) + "  "
                                + (score == state.score.players.end() ? "0" : std::to_string(score->cumulativePoints))
                                + (lifecycle.empty() ? std::string() : " • " + lifecycle),
                                player->lifeState == Network::Replication::LifeState::Alive ? Color::YELLOW : Color::RED});
                    }
                }
            } else {
                for (std::size_t rank = 0; rank < state.score.ranking.size(); ++rank) {
                    const auto id = state.score.ranking[rank];
                    const auto player = std::find_if(state.players.begin(), state.players.end(),
                            [id](const auto &entry) { return entry.playerId == id; });
                    if (player == state.players.end()) continue;
                    const auto score = std::find_if(state.score.players.begin(), state.score.players.end(),
                            [id](const auto &entry) { return entry.playerId == id; });
                    const std::string lifecycle = playerLifecycleLabel(state, *player);
                    lines.push_back({std::to_string(rank + 1) + ". "
                            + utf8Clipped(player->displayName, lifecycle.empty() ? 24 : 16) + "  "
                            + (score == state.score.players.end() ? "0" : std::to_string(score->cumulativePoints))
                            + (lifecycle.empty() ? std::string() : " • " + lifecycle),
                            player->lifeState == Network::Replication::LifeState::Alive ? Color::YELLOW : Color::RED});
                }
            }
            return lines;
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
                                    + levelDisplayName(round.get("level").asString()) + " • "
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

        struct VisibleWindow {
            std::size_t first = 0;
            std::size_t count = 0;
        };

        struct LobbyControlRectangles {
            Int32 bottom = 0;
        };

        constexpr Int32 LobbyPersonLeft = 48;
        constexpr Int32 LobbyPersonWidth = 176;
        constexpr Int32 LobbyControlLeft = 228;
        constexpr Int32 LobbyControlWidth = 170;
        constexpr Int32 LobbyControlRowHeight = 18;

        LobbyControlRectangles lobbyControlRectangles(std::size_t offset, bool retained) {
            return {(retained ? 164 : 243) - static_cast<Int32>(offset) * LobbyControlRowHeight};
        }

        VisibleWindow localControlWindow(int focus, std::size_t playerCount, bool retained) {
            if (playerCount == 0) return {};
            const bool focused = focus >= 0 && focus < static_cast<int>(playerCount) * 2;
            const std::size_t selected = focused ? static_cast<std::size_t>(focus / 2) : 0;
            if (retained) return {selected, 1};
            const std::size_t first = selected > 7 ? selected - 7 : 0;
            return {first, std::min<std::size_t>(8, playerCount - first)};
        }

        VisibleWindow rosterWindow(int focus, std::size_t rosterCount, int focusBase, bool retained) {
            if (rosterCount == 0) return {};
            const bool focused = focus >= focusBase && focus < focusBase + static_cast<int>(rosterCount);
            if (retained) return focused
                    ? VisibleWindow{static_cast<std::size_t>(focus - focusBase), 1} : VisibleWindow{};
            const std::size_t selected = focused ? static_cast<std::size_t>(focus - focusBase) : 0;
            const std::size_t first = selected > 7 ? selected - 7 : 0;
            return {first, std::min<std::size_t>(8, rosterCount - first)};
        }

        struct ResultScrollBounds {
            std::size_t vertical = 0;
            std::size_t horizontal = 0;
            std::size_t visibleRows = 1;
            std::size_t widest = 0;
        };

        ResultScrollBounds resultScrollBounds(
                const Network::Replication::CanonicalState &state, bool retained) {
            const auto lines = resultLines(state);
            const Int32 top = retained ? 365 : 450;
            const Int32 bottom = retained ? 190
                    : state.phase == Network::Replication::Phase::FinalSummary ? 120 : 62;
            const bool finalSummary = !retained && state.phase == Network::Replication::Phase::FinalSummary;
            const Int32 bodyTop = finalSummary ? top - 68 : top - 136;
            const Int32 bodyBottom = bottom + 32;
            const std::size_t visible = bodyTop < bodyBottom ? 1
                    : static_cast<std::size_t>((bodyTop - bodyBottom) / 18 + 1);
            std::size_t widest = 0;
            for (const auto &line: lines) widest = std::max(widest, utf8Length(line));
            return {lines.size() > visible ? lines.size() - visible : 0,
                    widest > 94 ? widest - 94 : 0, visible, widest};
        }

        bool retainedResult(const Client::NetworkRuntimeSnapshot &snapshot) {
            return snapshot.journey == Client::NetworkJourney::Lobby && snapshot.canonical
                    && snapshot.canonical->result.available;
        }

        int resultFocusIndex(const Client::NetworkRuntimeSnapshot &snapshot, std::size_t localPlayerCount) {
            if (snapshot.journey == Client::NetworkJourney::Summary) return snapshot.host ? 2 : 1;
            if (retainedResult(snapshot)) {
                const std::size_t roster = snapshot.host && snapshot.canonical
                        ? snapshot.canonical->players.size() : 0;
                return static_cast<int>(localPlayerCount) * 2
                        + (snapshot.host ? 12 + static_cast<int>(roster) : 2);
            }
            return -1;
        }

        std::size_t rankingMaximumScroll(
                const Network::Replication::CanonicalState &state, Int32 height, bool roundSummary) {
            const auto count = rankingLines(state).size();
            std::size_t visible = static_cast<std::size_t>(std::max<Int32>(1, (height - 160) / 16));
            if (roundSummary) {
                const Int32 panelHeight = std::min<Int32>(
                        128 + static_cast<Int32>(count) * 18, height - 200);
                visible = static_cast<std::size_t>(std::max<Int32>(1, (panelHeight - 118) / 18));
            }
            return count > visible ? count - visible : 0;
        }

        std::size_t lobbyMaximumScroll(const Network::Replication::CanonicalState &state) {
            const std::size_t participantMaximum = state.participants.size() > 3
                    ? state.participants.size() - 3 : 0;
            const std::size_t rosterMaximum = state.players.size() > 6 ? state.players.size() - 6 : 0;
            return std::max(participantMaximum, rosterMaximum);
        }
    }

    NetworkMenu::NetworkMenu(AppService &value, GameResources &resources,
                             Texture bannerTexture, std::function<void()> backgroundRenderer)
            : service(value), renderer(value.getVideo().getRenderer()), font(value.getFont()),
              controlsManager(value.getControlsManager()), worldPresenter(value, resources),
              menuBannerTexture(bannerTexture), renderMenuBackground(std::move(backgroundRenderer)) {}

    void NetworkMenu::open(std::vector<Client::NetworkLocalPlayer> players,
                           Network::HostComposition::Setup setup,
                           std::vector<std::string> persons,
                           std::vector<std::string> levels) {
        runtime.reset(); localPlayers = std::move(players); hostSetup = std::move(setup);
        availablePersons = std::move(persons); availableLevels = std::move(levels);
        worldPresenter.setCanonicalLevels(availableLevels);
        hostAddress.clear();
        (void) refreshHostAddresses(true);
        for (auto &player: localPlayers)
            if (player.controlDescription.empty() && player.controls)
                player.controlDescription = player.controls->getDescription();
        setupScreen = SetupScreen::Entry; focus = 0; setupScroll = 0; summaryScroll = 0; summaryHorizontal = 0;
        rankingScroll = 0;
        confirmation = Confirmation::None; scoreOverlay = false;
        lastJourney = Client::NetworkJourney::Inactive;
        lastStableJourney = Client::NetworkJourney::Inactive;
        Context::push(*this);
    }

    bool NetworkMenu::refreshHostAddresses(bool initialSelection) {
        const bool hadSelection = !hostAddress.empty();
        const auto available = Network::Trust::localListenerAddresses();
        hostAddresses = available.value_or(std::vector<std::string>{});
        const auto retained = std::find(hostAddresses.begin(), hostAddresses.end(), hostAddress);
        if (retained != hostAddresses.end()) {
            hostAddressHighlight = static_cast<std::size_t>(std::distance(hostAddresses.begin(), retained));
            hostAddressSelectionBecameInvalid = false;
            return true;
        }
        if (initialSelection) {
            const auto loopback = std::find(hostAddresses.begin(), hostAddresses.end(), "127.0.0.1");
            if (loopback != hostAddresses.end()) {
                hostAddress = *loopback;
                hostAddressHighlight = static_cast<std::size_t>(std::distance(hostAddresses.begin(), loopback));
                hostAddressSelectionBecameInvalid = false;
                return true;
            }
        }
        hostAddress.clear();
        hostAddressHighlight = 0;
        hostAddressScroll = 0;
        hostAddressSelectionBecameInvalid = hadSelection && !initialSelection;
        return false;
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
            reason = hostAddressSelectionBecameInvalid
                     ? "Selected listening interface is no longer available. Choose another interface."
                     : "Select an eligible listening interface.";
            return false;
        }
        if (!endpoint(ignored)) {
            reason = setupScreen == SetupScreen::Host ? "Enter a valid port (1–65535)."
                                                       : "Enter a valid address and port (1–65535).";
            return false;
        }
        if (localPlayers.empty()) { reason = "Add at least one local player."; return false; }
        const auto invalidName = std::find_if(localPlayers.begin(), localPlayers.end(),
                [](const auto &player) { return !Network::Trust::validParticipantName(player.name); });
        if (invalidName != localPlayers.end()) {
            reason = "Choose a valid local person for every player."; return false;
        }
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

    bool NetworkMenu::retryEligible(
            const Client::NetworkRuntimeSnapshot &snapshot, std::string &reason) const {
        if (!snapshot.retryAllowed) {
            reason = "Edit setup before you retry.";
            if (snapshot.retryBlockReason == Client::NetworkRetryBlockReason::CleanupInProgress)
                reason = "Cleanup in progress.";
            else if (snapshot.retryBlockReason == Client::NetworkRetryBlockReason::RestartRequired)
                reason = "Restart the application to try again.";
            else if (snapshot.retryBlockReason == Client::NetworkRetryBlockReason::EndedSession)
                reason = "This ended session cannot be restored. Edit setup to start a new session.";
            else if (snapshot.retryBlockReason == Client::NetworkRetryBlockReason::TerminalReconnect)
                reason = "This session cannot be restored.";
            return false;
        }
        std::string setupReason;
        if (!setupValid(setupReason)) {
            reason = "Edit setup before you retry.";
            return false;
        }
        reason.clear();
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

    void NetworkMenu::cyclePerson(std::size_t playerIndex, int direction) {
        if (playerIndex >= localPlayers.size() || availablePersons.empty()) return;
        const auto current = std::find(availablePersons.begin(), availablePersons.end(), localPlayers[playerIndex].name);
        std::size_t index = current == availablePersons.end() ? 0
                : static_cast<std::size_t>(std::distance(availablePersons.begin(), current));
        for (std::size_t attempt = 0; attempt < availablePersons.size(); ++attempt) {
            index = static_cast<std::size_t>((static_cast<int>(index) + direction
                    + static_cast<int>(availablePersons.size())) % static_cast<int>(availablePersons.size()));
            const bool alreadySelected = !Network::Trust::validParticipantName(availablePersons[index])
                    || std::any_of(localPlayers.begin(), localPlayers.end(), [&](const auto &player) {
                return &player != &localPlayers[playerIndex] && player.name == availablePersons[index];
            });
            if (!alreadySelected) {
                localPlayers[playerIndex].name = availablePersons[index];
                hostSetup.localPlayerNames.clear();
                for (const auto &player: localPlayers) hostSetup.localPlayerNames.push_back(player.name);
                runtime.rebindLocalPlayers(localPlayers);
                runtime.ownedPersonsChanged();
                return;
            }
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
    void NetworkMenu::mouseButtonEvent(const MouseButtonEvent &event) {
        if (!event.isPressed() || event.getButton() != SysEvent::MouseButton::LEFT) return;
        const auto snap = runtime.snapshot();
        const Int32 width = service.getVideo().getScreen().getClientWidth();
        const Int32 height = service.getVideo().getScreen().getClientHeight();
        if (confirmation != Confirmation::None) {
            if (pointerInside(event.getX(), event.getY(), width / 2 - 190, height / 2 - 55, 160, 34)) {
                focus = 0; activate();
            } else if (pointerInside(event.getX(), event.getY(), width / 2 + 30, height / 2 - 55, 160, 34)) {
                focus = 1; activate();
            }
            return;
        }
        if (snap.journey == Client::NetworkJourney::HostEnded) {
            const Int32 panelWidth = std::min<Int32>(640, width - 32);
            const Int32 panelHeight = std::min<Int32>(260, height - 32);
            const Int32 x = (width - panelWidth) / 2, y = (height - panelHeight) / 2;
            if (pointerInside(event.getX(), event.getY(), x + panelWidth / 2 - 110, y + 24, 220, 34)) {
                focus = 0; activate();
            }
            return;
        }
        if (snap.journey == Client::NetworkJourney::Reconnecting) {
            const Int32 panelWidth = std::min<Int32>(640, width - 32);
            const Int32 panelHeight = std::min<Int32>(340, height - 32);
            const Int32 x = (width - panelWidth) / 2, y = (height - panelHeight) / 2;
            if (pointerInside(event.getX(), event.getY(), x + panelWidth / 2 - 100, y + 24, 200, 34)) {
                focus = 0; activate();
            }
            return;
        }
        if (snap.journey == Client::NetworkJourney::Match && snap.canonical) {
            const bool roundSummary = snap.canonical->phase == Network::Replication::Phase::RoundSummary;
            if (snap.host && roundSummary
                && pointerInside(event.getX(), event.getY(), width - 368, 84, 168, 34)) {
                focus = 0; activate(); return;
            }
            if (pointerInside(event.getX(), event.getY(), width - 184, 84, 168, 34)) {
                focus = snap.host && roundSummary ? 1 : 0; activate(); return;
            }
            return;
        }
        const Float32 scale = canvasScale(width, height);
        const Int32 tx = (width - Int32(CanvasWidth * scale)) / 2;
        const Int32 ty = (height - Int32(CanvasHeight * scale)) / 2;
        const Int32 x = Int32((event.getX() - tx) / scale), y = Int32((event.getY() - ty) / scale);
        if (x < 0 || x >= CanvasWidth || y < 0 || y >= CanvasHeight) return;
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Entry) {
            for (int index = 0; index < 3; ++index)
                if (pointerInside(x, y, 275, 325 - index * 45, 300, 32)) {
                    focus = index; activate(); return;
                }
        } else if (snap.journey == Client::NetworkJourney::Inactive) {
            const int fields = 2;
            if (setupScreen == SetupScreen::Host && hostAddressSelectorOpen) {
                const std::size_t visible = std::min<std::size_t>(8, hostAddresses.size());
                const std::size_t maximumFirst = hostAddresses.size() > visible ? hostAddresses.size() - visible : 0;
                const std::size_t first = std::min(hostAddressScroll, maximumFirst);
                for (std::size_t row = 0; row < visible; ++row) {
                    if (pointerInside(x, y, 48, 440 - static_cast<Int32>(row) * 22, 764, 22)) {
                        hostAddressHighlight = first + row;
                        hostAddress = hostAddresses[hostAddressHighlight];
                        hostAddressSelectionBecameInvalid = false;
                        hostAddressSelectorOpen = false;
                        focus = 1;
                        return;
                    }
                }
                return;
            }
            if (pointerInside(x, y, 50, 486, 760, 22)) { focus = 0; return; }
            if (pointerInside(x, y, 50, 462, 760, 22)) {
                focus = 1;
                if (setupScreen == SetupScreen::Host) activate();
                return;
            }
            const int focusedPerson = focus >= fields && focus < fields + static_cast<int>(availablePersons.size())
                                      ? focus - fields : 0;
            const std::size_t firstPerson = static_cast<std::size_t>(std::max(0, focusedPerson - 9));
            for (std::size_t index = firstPerson; index < availablePersons.size() && index < firstPerson + 10; ++index)
                if (pointerInside(x, y, 50, 404 - static_cast<Int32>(index - firstPerson) * 18, 350, 18)) {
                    focus = fields + static_cast<int>(index); activate(); return;
                }
            const int playerBase = fields + static_cast<int>(availablePersons.size());
            const int focusedPlayer = focus >= playerBase && focus < playerBase + static_cast<int>(localPlayers.size()) * 2
                                      ? (focus - playerBase) / 2 : 0;
            const std::size_t firstPlayer = static_cast<std::size_t>(std::max(0, focusedPlayer - 9));
            for (std::size_t index = firstPlayer; index < localPlayers.size() && index < firstPlayer + 10; ++index) {
                const Int32 rowY = 404 - static_cast<Int32>(index - firstPlayer) * 22;
                if (pointerInside(x, y, 430, rowY, 290, 18)) {
                    focus = playerBase + static_cast<int>(index) * 2; activate(); return;
                }
                if (pointerInside(x, y, 724, rowY, 96, 18)) {
                    focus = playerBase + static_cast<int>(index) * 2 + 1; activate(); return;
                }
            }
            const int footer = playerBase + static_cast<int>(localPlayers.size()) * 2;
            std::string reason;
            if (setupValid(reason) && pointerInside(x, y, 275, 82, 300, 32)) {
                focus = footer; activate(); return;
            }
            if (pointerInside(x, y, 275, 38, 300, 32)) { focus = footer + 1; activate(); return; }
        } else if (snap.journey == Client::NetworkJourney::Starting) {
            const Int32 cancelY = !snap.host && setupScreen == SetupScreen::Join ? 38 : 300;
            if (pointerInside(x, y, 275, cancelY, 300, 32)) { focus = 0; activate(); return; }
        } else if (snap.journey == Client::NetworkJourney::Lobby && snap.canonical) {
            const bool retained = snap.canonical->result.available;
            const auto resultBounds = resultScrollBounds(*snap.canonical, true);
            if (retained && pointerInside(x, y, 50, 194, 32, 22)) {
                summaryHorizontal = std::clamp(summaryHorizontal - 8, 0,
                                               static_cast<int>(resultBounds.horizontal));
                focus = resultFocusIndex(snap, localPlayers.size());
                return;
            }
            if (retained && pointerInside(x, y, 574, 194, 32, 22)) {
                summaryHorizontal = std::min<int>(static_cast<int>(resultBounds.horizontal), summaryHorizontal + 8);
                focus = resultFocusIndex(snap, localPlayers.size());
                return;
            }
            if (!retained) {
                std::vector<Network::Replication::PlayerState> roster = snap.canonical->players;
                std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
                    return left.rosterPosition < right.rosterPosition;
                });
                const std::size_t first = std::min<std::size_t>(setupScroll,
                        roster.size() > 6 ? roster.size() - 6 : 0);
                const auto participant = std::find_if(snap.canonical->participants.begin(), snap.canonical->participants.end(),
                        [&snap](const auto &value) { return value.participantId == snap.localParticipantId; });
                for (std::size_t row = 0; row < 6 && first + row < roster.size(); ++row) {
                    const auto &player = roster[first + row];
                    if (participant == snap.canonical->participants.end()
                        || player.ownerParticipantId != snap.localParticipantId) continue;
                    const auto owned = std::find(participant->ownedPlayerIds.begin(), participant->ownedPlayerIds.end(), player.playerId);
                    if (owned == participant->ownedPlayerIds.end()) continue;
                    const auto index = static_cast<std::size_t>(std::distance(participant->ownedPlayerIds.begin(), owned));
                    const Int32 rowY = 351 - static_cast<Int32>(row) * 24;
                    if (pointerInside(x, y, 166, rowY, 180, 20)) {
                        focus = static_cast<int>(index) * 2; activate(); return;
                    }
                    if (pointerInside(x, y, 350, rowY, 170, 20)) {
                        focus = static_cast<int>(index) * 2 + 1; activate(); return;
                    }
                }
            } else {
                const auto controls = localControlWindow(focus, localPlayers.size(), true);
                for (std::size_t offset = 0; offset < controls.count; ++offset) {
                    const std::size_t index = controls.first + offset;
                    const auto rectangles = lobbyControlRectangles(offset, true);
                    if (pointerInside(x, y, LobbyPersonLeft, rectangles.bottom,
                                      LobbyPersonWidth, LobbyControlRowHeight)) {
                        focus = static_cast<int>(index) * 2; activate(); return;
                    }
                    if (pointerInside(x, y, LobbyControlLeft, rectangles.bottom,
                                      LobbyControlWidth, LobbyControlRowHeight)) {
                        focus = static_cast<int>(index) * 2 + 1; activate(); return;
                    }
                }
            }
            const int readyIndex = static_cast<int>(localPlayers.size()) * 2;
            if (pointerInside(x, y, 40, retained ? 140 : 168, 220, 20)) {
                std::string reason; if (localReadyEligible(reason)) { focus = readyIndex; activate(); }
                return;
            }
            if (snap.host) {
                if (!retained) for (int index = 0; index < 9; ++index)
                    if (pointerInside(x, y, 568, 316 - index * 18, 240, 18)) {
                        focus = readyIndex + 1 + index; activate(); return;
                    }
                std::vector<Network::Replication::PlayerState> roster = snap.canonical->players;
                std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
                    return left.rosterPosition < right.rosterPosition;
                });
                const int rosterBase = readyIndex + 10;
                const auto visibleRoster = retained ? rosterWindow(focus, roster.size(), rosterBase, true)
                        : VisibleWindow{std::min<std::size_t>(setupScroll,
                                roster.size() > 6 ? roster.size() - 6 : 0), 0};
                const std::size_t visibleRosterCount = retained ? visibleRoster.count
                        : std::min<std::size_t>(6, roster.size() - visibleRoster.first);
                for (std::size_t offset = 0; offset < visibleRosterCount; ++offset) {
                    const std::size_t index = visibleRoster.first + offset;
                    const Int32 rowY = retained ? 162 : 351 - static_cast<Int32>(offset) * 24;
                    if (pointerInside(x, y, retained ? 408 : 528, rowY,
                                      retained ? 376 : 34, 20)) {
                        focus = readyIndex + 10 + static_cast<int>(index); activate(); return;
                    }
                }
                const int startIndex = readyIndex + 10 + static_cast<int>(roster.size());
                std::string reason;
                if (startEligible(snap, reason) && pointerInside(x, y, 408, 54, 210, 20)) {
                    focus = startIndex; activate(); return;
                }
                if (pointerInside(x, y, 670, 54, 150, 22)) { focus = startIndex + 1; activate(); return; }
            } else if (pointerInside(x, y, 645, 54, 175, 22)) {
                focus = readyIndex + 1; activate(); return;
            }
        } else if (snap.journey == Client::NetworkJourney::Summary) {
            const auto bounds = snap.canonical ? resultScrollBounds(*snap.canonical, false) : ResultScrollBounds{};
            if (pointerInside(x, y, 50, 124, 32, 22)) {
                summaryHorizontal = std::clamp(summaryHorizontal - 8, 0,
                                               static_cast<int>(bounds.horizontal));
                focus = snap.host ? 2 : 1; return;
            }
            if (pointerInside(x, y, 574, 124, 32, 22)) {
                summaryHorizontal = std::min<int>(static_cast<int>(bounds.horizontal), summaryHorizontal + 8);
                focus = snap.host ? 2 : 1; return;
            }
            if (snap.host && pointerInside(x, y, 275, 72, 300, 32)) { focus = 0; activate(); return; }
            if (pointerInside(x, y, 275, 30, 300, 32)) { focus = snap.host ? 1 : 0; activate(); return; }
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            std::string reason; const bool canRetry = retryEligible(snap, reason);
            int selected = 0;
            if (canRetry && pointerInside(x, y, 54, 270, 220, 34)) { focus = selected; activate(); return; }
            if (canRetry) ++selected;
            if (pointerInside(x, y, 315, 270, 220, 34)) { focus = selected++; activate(); return; }
            if (pointerInside(x, y, 576, 270, 220, 34)) { focus = selected; activate(); return; }
        }
    }
    void NetworkMenu::mouseWheelEvent(const MouseWheelEvent &event) {
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::Match && !scoreOverlay) {
            const auto maximum = snap.canonical ? rankingMaximumScroll(
                    *snap.canonical, service.getVideo().getScreen().getClientHeight(),
                    snap.canonical->phase == Network::Replication::Phase::RoundSummary) : 0;
            rankingScroll = std::clamp(rankingScroll - event.getAmountY(), 0, static_cast<int>(maximum));
            return;
        }
        if (snap.journey == Client::NetworkJourney::Lobby
            && (!snap.canonical || !snap.canonical->result.available)) {
            const auto maximum = snap.canonical ? lobbyMaximumScroll(*snap.canonical) : 0;
            setupScroll = std::clamp(setupScroll - event.getAmountY(), 0, static_cast<int>(maximum));
            focus = static_cast<int>(localPlayers.size()) * 2;
            return;
        }
        if (snap.journey != Client::NetworkJourney::Summary
            && !(snap.journey == Client::NetworkJourney::Lobby && snap.canonical && snap.canonical->result.available)) return;
        const bool retained = retainedResult(snap);
        const auto bounds = snap.canonical ? resultScrollBounds(*snap.canonical, retained) : ResultScrollBounds{};
        summaryScroll = std::clamp(summaryScroll - event.getAmountY(), 0, static_cast<int>(bounds.vertical));
        summaryHorizontal = std::clamp(summaryHorizontal + event.getAmountX() * 8,
                                       0, static_cast<int>(bounds.horizontal));
    }

    void NetworkMenu::moveFocus(int direction) {
        const auto snap = runtime.snapshot();
        int count = 1;
        if (confirmation != Confirmation::None) count = 2;
        else if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) count = 3;
            else count = 2
                          + static_cast<int>(availablePersons.size())
                         + static_cast<int>(localPlayers.size()) * 2 + 2;
        } else if (snap.journey == Client::NetworkJourney::Lobby) {
            count = static_cast<int>(localPlayers.size()) * 2 + 2;
            if (snap.host) count += 9 + static_cast<int>(snap.canonical ? snap.canonical->players.size() : 0) + 1;
            if (retainedResult(snap)) ++count;
        } else if (snap.journey == Client::NetworkJourney::Match && snap.canonical) {
            if (snap.canonical->phase == Network::Replication::Phase::RoundSummary)
                count = snap.host ? 3 : 2;
            else count = 1;
        } else if (snap.journey == Client::NetworkJourney::Summary) count = snap.host ? 3 : 2;
        else if (snap.journey == Client::NetworkJourney::Failure) {
            std::string reason;
            count = retryEligible(snap, reason) ? 3 : 2;
        }
        focus = (focus + direction + count) % count;
        syncLobbyScroll(snap);
    }

    void NetworkMenu::syncLobbyScroll(const Client::NetworkRuntimeSnapshot &snap) {
        if (snap.journey != Client::NetworkJourney::Lobby || !snap.canonical
            || snap.canonical->result.available) return;
        std::vector<Network::Replication::PlayerState> roster = snap.canonical->players;
        std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
            return left.rosterPosition < right.rosterPosition;
        });
        std::optional<std::size_t> selected;
        if (focus >= 0 && focus < static_cast<int>(localPlayers.size()) * 2) {
            const auto participant = std::find_if(snap.canonical->participants.begin(), snap.canonical->participants.end(),
                    [&snap](const auto &value) { return value.participantId == snap.localParticipantId; });
            const std::size_t localIndex = static_cast<std::size_t>(focus / 2);
            if (participant != snap.canonical->participants.end()
                && localIndex < participant->ownedPlayerIds.size()) {
                const auto player = std::find_if(roster.begin(), roster.end(), [&](const auto &value) {
                    return value.playerId == participant->ownedPlayerIds[localIndex];
                });
                if (player != roster.end()) selected = static_cast<std::size_t>(std::distance(roster.begin(), player));
            }
        } else if (snap.host) {
            const int rosterBase = static_cast<int>(localPlayers.size()) * 2 + 10;
            if (focus >= rosterBase && focus < rosterBase + static_cast<int>(roster.size()))
                selected = static_cast<std::size_t>(focus - rosterBase);
        }
        const int maximum = static_cast<int>(lobbyMaximumScroll(*snap.canonical));
        setupScroll = std::clamp(setupScroll, 0, maximum);
        if (!selected) return;
        if (*selected < static_cast<std::size_t>(setupScroll)) setupScroll = static_cast<int>(*selected);
        else if (*selected >= static_cast<std::size_t>(setupScroll) + 6)
            setupScroll = static_cast<int>(*selected - 5);
        setupScroll = std::clamp(setupScroll, 0, maximum);
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
            const int fields = 2;
            if (setupScreen == SetupScreen::Host && focus == 1 && !hostAddresses.empty()) {
                if (hostAddressSelectorOpen) {
                    hostAddress = hostAddresses[hostAddressHighlight];
                    hostAddressSelectionBecameInvalid = false;
                    hostAddressSelectorOpen = false;
                    return;
                }
                const auto selected = std::find(hostAddresses.begin(), hostAddresses.end(), hostAddress);
                hostAddressHighlight = selected == hostAddresses.end() ? 0u
                        : static_cast<std::size_t>(std::distance(hostAddresses.begin(), selected));
                hostAddressSelectorOpen = true;
                return;
            }
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
            if (setupScreen == SetupScreen::Host && !refreshHostAddresses(false)) return;
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
            if (focus == resultFocusIndex(snap, localPlayers.size())) { focus = 0; return; }
            if (focus < static_cast<int>(localPlayers.size()) * 2) {
                const auto player = static_cast<std::size_t>(focus / 2);
                if (focus % 2 == 0) cyclePerson(player); else cycleControl(player);
                return;
            }
            int action = focus - static_cast<int>(localPlayers.size()) * 2;
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
        } else if (snap.journey == Client::NetworkJourney::Match) {
            const int rankingFocus = snap.host ? 2 : 1;
            if (snap.canonical && snap.canonical->phase == Network::Replication::Phase::RoundSummary
                && focus == rankingFocus) { focus = 0; return; }
            const bool advance = snap.host && snap.canonical
                    && snap.canonical->phase == Network::Replication::Phase::RoundSummary && focus == 0;
            if (advance) runtime.advanceRound();
            else { confirmation = snap.host ? Confirmation::End : Confirmation::Leave; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Summary) {
            if (snap.host && focus == 0) runtime.returnToLobby();
            else if ((!snap.host && focus == 0) || (snap.host && focus == 1)) {
                confirmation = snap.host ? Confirmation::End : Confirmation::Leave; focus = 0;
            } else if (focus == resultFocusIndex(snap, localPlayers.size())) focus = 0;
        } else if (snap.journey == Client::NetworkJourney::Reconnecting) {
            confirmation = Confirmation::Leave; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            std::string retryReason;
            const bool canRetry = retryEligible(snap, retryReason);
            int action = focus;
            if (!canRetry) ++action;
            if (action == 0) {
                if (snap.host && !refreshHostAddresses(false)) {
                    runtime.reset(); setupScreen = SetupScreen::Host; focus = 0; return;
                }
                Network::Endpoint target; if (!endpoint(target)) return;
                runtime.reset();
                if (snap.host) (void) runtime.startHost(target, serverExecutable(), "resources", hostSetup, localPlayers);
                else (void) runtime.join(target, "resources", localPlayers);
            } else if (action == 1) {
                runtime.reset(); setupScreen = snap.host ? SetupScreen::Host : SetupScreen::Join;
                if (snap.host) (void) refreshHostAddresses(false);
            }
            else { runtime.reset(); setupScreen = SetupScreen::Entry; }
            focus = 0;
        }
    }

    void NetworkMenu::back() {
        const auto snap = runtime.snapshot();
        if (hostAddressSelectorOpen) { hostAddressSelectorOpen = false; focus = 1; return; }
        if (confirmation != Confirmation::None) { confirmation = Confirmation::None; focus = 0; return; }
        if (snap.journey == Client::NetworkJourney::Starting) runtime.cancel();
        else if (snap.journey == Client::NetworkJourney::Inactive) {
            if (setupScreen == SetupScreen::Entry) close(); else { setupScreen = SetupScreen::Entry; focus = 0; }
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::HostEnded) {
            runtime.reset(); setupScreen = SetupScreen::Entry; focus = 0;
        } else if (snap.journey == Client::NetworkJourney::Lobby) {
            focus = static_cast<int>(localPlayers.size()) * 2 + 1;
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
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Host
            && hostAddressSelectorOpen) {
            if (event.getCode() == SDLK_ESCAPE) { hostAddressSelectorOpen = false; focus = 1; return; }
            if ((event.getCode() == SDLK_UP || event.getCode() == SDLK_DOWN) && !hostAddresses.empty()) {
                const int direction = event.getCode() == SDLK_DOWN ? 1 : -1;
                hostAddressHighlight = static_cast<std::size_t>((static_cast<int>(hostAddressHighlight) + direction
                        + static_cast<int>(hostAddresses.size())) % static_cast<int>(hostAddresses.size()));
                if (hostAddressHighlight < hostAddressScroll) hostAddressScroll = hostAddressHighlight;
                if (hostAddressHighlight >= hostAddressScroll + 8) hostAddressScroll = hostAddressHighlight - 7;
                return;
            }
            if (event.getCode() == SDLK_RETURN || event.getCode() == SDLK_SPACE) {
                if (!hostAddresses.empty()) {
                    hostAddress = hostAddresses[hostAddressHighlight];
                    hostAddressSelectionBecameInvalid = false;
                }
                hostAddressSelectorOpen = false;
                focus = 1;
                return;
            }
        }
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
            const bool retained = retainedResult(snap);
            const auto bounds = snap.canonical ? resultScrollBounds(*snap.canonical, retained) : ResultScrollBounds{};
            summaryScroll = std::clamp(summaryScroll + (event.getCode() == SDLK_PAGEDOWN ? 8 : -8),
                                       0, static_cast<int>(bounds.vertical));
            return;
        }
        if (snap.journey == Client::NetworkJourney::Match && !scoreOverlay
            && (event.getCode() == SDLK_PAGEUP || event.getCode() == SDLK_PAGEDOWN)) {
            const auto maximum = snap.canonical ? rankingMaximumScroll(
                    *snap.canonical, service.getVideo().getScreen().getClientHeight(),
                    snap.canonical->phase == Network::Replication::Phase::RoundSummary) : 0;
            rankingScroll = std::clamp(rankingScroll + (event.getCode() == SDLK_PAGEDOWN ? 1 : -1),
                                       0, static_cast<int>(maximum));
            return;
        }
        if ((snap.journey == Client::NetworkJourney::Summary
             || (snap.journey == Client::NetworkJourney::Lobby && snap.canonical && snap.canonical->result.available))
            && (event.getCode() == SDLK_LEFT || event.getCode() == SDLK_RIGHT)) {
            const bool retained = retainedResult(snap);
            const auto bounds = snap.canonical ? resultScrollBounds(*snap.canonical, retained) : ResultScrollBounds{};
            summaryHorizontal = std::clamp(summaryHorizontal + (event.getCode() == SDLK_RIGHT ? 8 : -8),
                                           0, static_cast<int>(bounds.horizontal));
            return;
        }
        if (event.getCode() == SDLK_ESCAPE) back();
        else if (event.getCode() == SDLK_TAB || event.getCode() == SDLK_DOWN) moveFocus(1);
        else if (event.getCode() == SDLK_UP) moveFocus(-1);
        else if (event.getCode() == SDLK_RETURN || event.getCode() == SDLK_SPACE) activate();
        else if ((setupScreen == SetupScreen::Host || setupScreen == SetupScreen::Join)
                 && snap.journey == Client::NetworkJourney::Inactive) {
            std::string *field = setupScreen == SetupScreen::Join && focus == 0 ? &address : &port;
            if (((setupScreen == SetupScreen::Join && (focus == 0 || focus == 1))
                 || (setupScreen == SetupScreen::Host && focus == 0))
                && event.getCode() == SDLK_BACKSPACE && !field->empty()) field->pop_back();
        }
    }

    void NetworkMenu::textInputEvent(const TextInputEvent &event) {
        if (runtime.snapshot().journey != Client::NetworkJourney::Inactive || setupScreen == SetupScreen::Entry) return;
        const bool addressField = setupScreen == SetupScreen::Join && focus == 0;
        const bool portField = setupScreen == SetupScreen::Host ? focus == 0 : focus == 1;
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
        worldPresenter.update(elapsedTime, currentSnapshot.canonical ? &*currentSnapshot.canonical : nullptr,
                              currentSnapshot.presentationEvents);
        bool sessionBack = false, uiConfirm = false, uiBack = false;
        bool uiUp = false, uiDown = false, uiLeft = false, uiRight = false;
        for (const auto &controller: service.getInput().getJoys()) {
            sessionBack = sessionBack || controller.isPressed(GameController::CONTROLLER_BUTTON_BACK);
            uiConfirm = uiConfirm || controller.isPressed(GameController::CONTROLLER_BUTTON_A);
            uiBack = uiBack || controller.isPressed(GameController::CONTROLLER_BUTTON_B);
            uiUp = uiUp || controller.isPressed(GameController::CONTROLLER_BUTTON_DPAD_UP)
                    || controller.getAxis(GameController::CONTROLLER_AXIS_LEFTY) < -16000;
            uiDown = uiDown || controller.isPressed(GameController::CONTROLLER_BUTTON_DPAD_DOWN)
                    || controller.getAxis(GameController::CONTROLLER_AXIS_LEFTY) > 16000;
            uiLeft = uiLeft || controller.isPressed(GameController::CONTROLLER_BUTTON_DPAD_LEFT)
                    || controller.getAxis(GameController::CONTROLLER_AXIS_LEFTX) < -16000;
            uiRight = uiRight || controller.isPressed(GameController::CONTROLLER_BUTTON_DPAD_RIGHT)
                    || controller.getAxis(GameController::CONTROLLER_AXIS_LEFTX) > 16000;
        }
        if (sessionBack && !controllerSessionBack) back();
        controllerSessionBack = sessionBack;
        const bool roundSummary = currentSnapshot.journey == Client::NetworkJourney::Match
                && currentSnapshot.canonical
                && currentSnapshot.canonical->phase == Network::Replication::Phase::RoundSummary;
        if ((currentSnapshot.journey != Client::NetworkJourney::Match
             || confirmation != Confirmation::None || roundSummary)) {
            if (!localPlayers.empty() && localPlayers[0].controls) {
                const auto &c = *localPlayers[0].controls;
                uiConfirm = uiConfirm || c.getShoot().isPressed();
                uiBack = uiBack || c.getPick().isPressed();
                uiUp = uiUp || c.getUp().isPressed(); uiDown = uiDown || c.getDown().isPressed();
                uiLeft = uiLeft || c.getLeft().isPressed(); uiRight = uiRight || c.getRight().isPressed();
            }
            const int resultFocus = resultFocusIndex(currentSnapshot, localPlayers.size());
            const bool resultFocused = confirmation == Confirmation::None && resultFocus >= 0 && focus == resultFocus;
            const int rankingFocus = currentSnapshot.host ? 2 : 1;
            const bool rankingFocused = confirmation == Confirmation::None && roundSummary && focus == rankingFocus;
            if (uiConfirm && !controllerConfirm) activate();
            if (uiBack && !controllerBack) back();
            if (hostAddressSelectorOpen && !hostAddresses.empty()) {
                if ((uiUp && !controllerUp) || (uiDown && !controllerDown)) {
                    const int direction = uiDown ? 1 : -1;
                    hostAddressHighlight = static_cast<std::size_t>((static_cast<int>(hostAddressHighlight) + direction
                            + static_cast<int>(hostAddresses.size())) % static_cast<int>(hostAddresses.size()));
                    if (hostAddressHighlight < hostAddressScroll) hostAddressScroll = hostAddressHighlight;
                    if (hostAddressHighlight >= hostAddressScroll + 8) hostAddressScroll = hostAddressHighlight - 7;
                }
            } else if (resultFocused && currentSnapshot.canonical) {
                const bool retained = retainedResult(currentSnapshot);
                const auto bounds = resultScrollBounds(*currentSnapshot.canonical, retained);
                if (uiUp && !controllerUp)
                    summaryScroll = std::clamp(summaryScroll - 1, 0, static_cast<int>(bounds.vertical));
                if (uiDown && !controllerDown)
                    summaryScroll = std::min<int>(static_cast<int>(bounds.vertical), summaryScroll + 1);
                if (uiLeft && !controllerLeft)
                    summaryHorizontal = std::clamp(summaryHorizontal - 8, 0,
                                                   static_cast<int>(bounds.horizontal));
                if (uiRight && !controllerRight)
                    summaryHorizontal = std::min<int>(static_cast<int>(bounds.horizontal), summaryHorizontal + 8);
            } else if (rankingFocused && currentSnapshot.canonical) {
                const auto maximum = rankingMaximumScroll(*currentSnapshot.canonical,
                        service.getVideo().getScreen().getClientHeight(), true);
                if (uiUp && !controllerUp)
                    rankingScroll = std::clamp(rankingScroll - 1, 0, static_cast<int>(maximum));
                if (uiDown && !controllerDown)
                    rankingScroll = std::min<int>(static_cast<int>(maximum), rankingScroll + 1);
                if (uiLeft && !controllerLeft) moveFocus(-1);
                if (uiRight && !controllerRight) moveFocus(1);
            } else {
                if (uiUp && !controllerUp) moveFocus(-1);
                if (uiDown && !controllerDown) moveFocus(1);
                if ((roundSummary || currentSnapshot.journey == Client::NetworkJourney::Summary)
                    && uiLeft && !controllerLeft) moveFocus(-1);
                if ((roundSummary || currentSnapshot.journey == Client::NetworkJourney::Summary)
                    && uiRight && !controllerRight) moveFocus(1);
            }
            controllerConfirm = uiConfirm; controllerBack = uiBack;
            controllerUp = uiUp; controllerDown = uiDown;
            controllerLeft = uiLeft; controllerRight = uiRight;
        } else {
            // Track held gameplay controls without treating them as session actions. This prevents
            // an already-held button from activating a control when a round summary first appears.
            controllerConfirm = uiConfirm; controllerBack = uiBack;
            controllerUp = uiUp; controllerDown = uiDown;
            controllerLeft = uiLeft; controllerRight = uiRight;
        }
        if (currentSnapshot.journey != lastJourney) {
            std::string retryReason;
            const bool canRetry = currentSnapshot.journey == Client::NetworkJourney::Failure
                                  && retryEligible(currentSnapshot, retryReason);
            focus = canRetry && currentSnapshot.host
                    && currentSnapshot.failure == "The selected port is unavailable. Choose another port and try again." ? 1 : 0;
            confirmation = Confirmation::None; scoreOverlay = false;
            const bool stable = currentSnapshot.journey == Client::NetworkJourney::Lobby
                    || currentSnapshot.journey == Client::NetworkJourney::Match
                    || currentSnapshot.journey == Client::NetworkJourney::Summary;
            if (stable && lastStableJourney != currentSnapshot.journey) {
                if (currentSnapshot.journey == Client::NetworkJourney::Lobby) setupScroll = 0;
                if (currentSnapshot.journey == Client::NetworkJourney::Match) rankingScroll = 0;
                if (currentSnapshot.journey == Client::NetworkJourney::Summary) {
                    summaryScroll = 0; summaryHorizontal = 0;
                }
            }
            if (stable) lastStableJourney = currentSnapshot.journey;
            lastJourney = currentSnapshot.journey;
            previousRetryEligible = canRetry;
        } else if (currentSnapshot.journey == Client::NetworkJourney::Failure) {
            std::string retryReason;
            const bool canRetry = retryEligible(currentSnapshot, retryReason);
            if (canRetry != previousRetryEligible) {
                focus = canRetry && currentSnapshot.host
                        && currentSnapshot.failure == "The selected port is unavailable. Choose another port and try again." ? 1 : 0;
                previousRetryEligible = canRetry;
            } else if (focus >= (canRetry ? 3 : 2)) focus = 0;
        } else {
            previousRetryEligible = false;
        }
    }

    void NetworkMenu::drawText(Int32 x, Int32 y, const std::string &text, Color color) const { font.print(x, y, color, text); }
    void NetworkMenu::drawClippedText(Int32 x, Int32 y, const std::string &text,
                                       std::size_t characters, Color color) const {
        drawText(x, y, utf8Clipped(text, characters), color);
    }
    void NetworkMenu::drawWrappedText(Int32 x, Int32 y, const std::string &text,
                                      std::size_t charactersPerLine, std::size_t maximumLines,
                                      Color color) const {
        std::size_t offset = 0;
        for (std::size_t line = 0; line < maximumLines && offset < text.size(); ++line) {
            const std::size_t candidateEnd = offset + utf8ByteOffset(
                    std::string_view(text).substr(offset), charactersPerLine);
            std::size_t end = candidateEnd;
            if (candidateEnd < text.size()) {
                const auto space = text.rfind(' ', candidateEnd);
                if (space != std::string::npos && space > offset) end = space;
            }
            if (end == offset) end = candidateEnd;
            drawText(x, y - static_cast<Int32>(line) * 18, text.substr(offset, end - offset), color);
            offset = end;
            while (offset < text.size() && text[offset] == ' ') ++offset;
        }
    }
    void NetworkMenu::drawAction(Int32 y, const std::string &text, bool selected) const {
        renderer.quadXY(Vector(275, y), Vector(300, 32), selected ? Color(64, 96, 160) : Color(192));
        drawFocusKeyline(275, y, 300, 32, selected);
        drawText(425 - static_cast<Int32>(utf8Length(text)) * 4, y + 8, text,
                 selected ? Color::WHITE : Color::BLACK);
    }

    void NetworkMenu::drawFocusKeyline(
            Int32 x, Int32 y, Int32 width, Int32 height, bool selected) const {
        if (selected) renderer.frame(Vector(x - 2, y - 2), Vector(width + 4, height + 4), 2.0f, Color::BLACK);
    }

    void NetworkMenu::drawMenuCanvas(Int32 width, Int32 height) const {
        if (renderMenuBackground) renderMenuBackground();
        const Float32 scale = canvasScale(width, height);
        const Int32 tx = (width - Int32(CanvasWidth * scale)) / 2;
        const Int32 ty = (height - Int32(CanvasHeight * scale)) / 2;
        renderer.setViewMatrix(Matrix::translate(Float32(tx), Float32(ty), 0) * Matrix::scale(scale, scale, 1));
        renderer.quadXY(Vector(0, 0), Vector(CanvasWidth, CanvasHeight), Color(192));
        renderer.frame(Vector(0, 0), Vector(CanvasWidth, CanvasHeight), 2.0f, Color::BLACK);
        const std::string version = std::string("version ") + APP_VERSION;
        drawText((CanvasWidth - font.getTextWidth(version, font.getCharHeight())) / 2, 581, version);
        renderer.quadXY(Vector(325, 600), Vector(200, 95), Vector(0, 1), Vector(1, -1),
                        Material::makeTexture(menuBannerTexture));
        renderer.quadXY(Vector(24, 24), Vector(802, 544), Color(224));
    }

    void NetworkMenu::drawPlayers(const Network::Replication::CanonicalState &state) const {
        Int32 y = 456;
        const Int32 bottom = 402;
        const std::size_t first = std::min<std::size_t>(setupScroll,
                state.participants.size() > 3 ? state.participants.size() - 3 : 0);
        for (std::size_t participantIndex = first;
             participantIndex < state.participants.size() && y > bottom; ++participantIndex) {
            const auto &participant = state.participants[participantIndex];
            drawClippedText(42, y, participantLabel(state, participant.participantId), 12);
            drawClippedText(142, y,
                    participant.connection == Network::Replication::ConnectionState::Connected
                    ? "Connected" : "Reconnecting", 13);
            drawText(254, y, participant.ready ? "Ready" : "Not ready");
            drawText(350, y, std::to_string(participant.ownedPlayerIds.size()));
            y -= 18;
        }
    }

    void NetworkMenu::drawMatch(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height, bool interactive,
            const std::string &connectionState, bool showLiveNetworkState) const {
        if (!snap.canonical) return;
        worldPresenter.render(*snap.canonical, snap.presentation, snap.presentedPlayers, width, height);
        renderer.setViewMatrix(Matrix::IDENTITY);
        const auto &state = *snap.canonical;
        if (state.phase != Network::Replication::Phase::RoundSummary) {
            const std::string progress = "Round " + std::to_string(state.currentRoundNumber) + "/"
                                         + std::to_string(state.settings.roundLimit) + " • " + state.messages.status;
            drawText(width / 2 - static_cast<Int32>(utf8Length(progress)) * 4, height - 24,
                     progress, Color::YELLOW);
        }
        Int32 y = height - 24;
        for (const auto &event: state.messages.events) {
            drawText(16, y, event, Color::YELLOW); y -= 16; if (y < height - 88) break;
        }
        if (state.phase == Network::Replication::Phase::RoundSummary) {
            drawRoundSummary(snap, width, height);
        } else {
            const auto lines = rankingLines(state);
            const std::size_t maximumRows = static_cast<std::size_t>(std::max<Int32>(1, (height - 160) / 16));
            const std::size_t maximumFirst = lines.size() > maximumRows ? lines.size() - maximumRows : 0;
            const std::size_t first = std::min<std::size_t>(rankingScroll, maximumFirst);
            Int32 rankY = height - 24;
            for (std::size_t index = first; index < lines.size() && index < first + maximumRows; ++index) {
                drawText(width - 16 - static_cast<Int32>(utf8Length(lines[index].text)) * 8,
                         rankY, lines[index].text, lines[index].color);
                rankY -= 16;
            }
            if (maximumFirst != 0) {
                const std::string position = "Ranking " + std::to_string(first + 1) + "–"
                        + std::to_string(std::min(lines.size(), first + maximumRows)) + "/"
                        + std::to_string(lines.size()) + " • PgUp/PgDn";
                drawText(width - 16 - static_cast<Int32>(utf8Length(position)) * 8, rankY, position, Color::WHITE);
            }
        }
        std::vector<std::string> statusRows;
        statusRows.push_back(std::string(snap.host ? "Host" : "Guest") + " • LAN session • " + connectionState);
        const std::string right = "Session only scores • Optional scripts disabled";
        const bool rightFits = static_cast<Int32>((utf8Length(statusRows.front()) + utf8Length(right)) * 8 + 56) <= width;
        std::string networkState;
        if (showLiveNetworkState && snap.presentation.degraded) networkState = "Network connection degraded.";
        if (showLiveNetworkState && snap.presentation.resynchronizing) {
            if (!networkState.empty()) networkState += " • ";
            networkState += "Last confirmed state • Synchronizing current state…";
        }
        if (!rightFits) statusRows.push_back(right);
        if (!networkState.empty()) statusRows.push_back(networkState);
        const Int32 statusHeight = 8 + static_cast<Int32>(statusRows.size()) * 16;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(16, 16), Vector(width - 32, statusHeight), Color(0, 0, 255, 179));
        renderer.setBlendFunc(BlendFunc::None);
        for (std::size_t row = 0; row < statusRows.size(); ++row)
            drawText(20, 20 + static_cast<Int32>(row) * 16, statusRows[row], Color::YELLOW);
        if (rightFits)
            drawText(width - 20 - static_cast<Int32>(utf8Length(right)) * 8, 20, right, Color::YELLOW);
        if (interactive) {
            const bool roundSummary = state.phase == Network::Replication::Phase::RoundSummary;
            if (snap.host && roundSummary) {
                renderer.quadXY(Vector(width - 368, 84), Vector(168, 34), Color(192));
                drawFocusKeyline(width - 368, 84, 168, 34, focus == 0);
                drawText(width - 350, 92, "Advance round");
            }
            renderer.quadXY(Vector(width - 184, 84), Vector(168, 34), Color(192));
            const bool selected = focus == (snap.host && roundSummary ? 1 : 0);
            drawFocusKeyline(width - 184, 84, 168, 34, selected);
            drawText(width - 168, 92, snap.host ? "End session" : "Leave session");
        }
        if (scoreOverlay) drawResult(state, false);
        if (interactive && confirmation != Confirmation::None) drawConfirmation();
    }

    void NetworkMenu::drawRoundSummary(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        const auto &state = *snap.canonical;
        const auto lines = rankingLines(state);
        const Int32 panelWidth = std::min<Int32>(786, width - 64);
        const Int32 top = height - 40;
        const Int32 desiredHeight = 128 + static_cast<Int32>(lines.size()) * 18;
        const Int32 panelHeight = std::min<Int32>(desiredHeight, height - 200);
        const Int32 bottom = top - panelHeight;
        const Int32 left = (width - panelWidth) / 2;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(left, bottom), Vector(panelWidth, panelHeight), Color(224, 224, 224, 240));
        renderer.setBlendFunc(BlendFunc::None);
        const std::string rounds = "Rounds: " + std::to_string(state.completedRounds) + "|"
                + std::to_string(state.settings.roundLimit);
        if (state.settings.roundLimit > 0)
            drawText(left + panelWidth - 16 - static_cast<Int32>(utf8Length(rounds)) * 8, top - 24, rounds);
        renderer.quadXY(Vector(left + 16, top - 64), Vector(panelWidth - 32, 32), Color::BLUE);
        drawText(left + panelWidth / 2 - 44, top - 56, "---SCORE---", Color::WHITE);
        const std::size_t visibleRows = static_cast<std::size_t>(std::max<Int32>(1, (panelHeight - 118) / 18));
        const std::size_t maximumFirst = lines.size() > visibleRows ? lines.size() - visibleRows : 0;
        const std::size_t first = std::min<std::size_t>(rankingScroll, maximumFirst);
        const int rankingFocus = snap.host ? 2 : 1;
        drawFocusKeyline(left + 24, bottom + 52, panelWidth - 48, panelHeight - 124,
                         focus == rankingFocus);
        Int32 rowY = top - 88;
        for (std::size_t index = first; index < lines.size() && index < first + visibleRows; ++index, rowY -= 18)
            drawText(left + 32, rowY, lines[index].text, lines[index].color);
        const unsigned seconds = static_cast<unsigned>((state.roundEndCountdown + 59) / 60);
        const std::string phase = state.roundEndCountdown > 300 ? "World active • 1s"
                : "Round frozen • Next round in " + std::to_string(seconds) + "s";
        drawText(left + 32, bottom + 30, "Round outcome: " + outcome(state, state.score.winner));
        drawText(left + panelWidth - 32 - static_cast<Int32>(utf8Length(phase)) * 8, bottom + 30, phase);
        if (maximumFirst != 0)
            drawText(left + 32, bottom + 10, "Ranking " + std::to_string(first + 1) + "–"
                     + std::to_string(std::min(lines.size(), first + visibleRows)) + "/" + std::to_string(lines.size()));
    }

    void NetworkMenu::drawReconnectPanel(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        const Int32 panelWidth = std::min<Int32>(640, width - 32);
        const Int32 panelHeight = std::min<Int32>(340, height - 32);
        const Int32 x = (width - panelWidth) / 2;
        const Int32 y = (height - panelHeight) / 2;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(x, y), Vector(panelWidth, panelHeight), Color(224, 224, 224, 244));
        renderer.setBlendFunc(BlendFunc::None);
        const auto columns = static_cast<std::size_t>(std::max<Int32>(12, (panelWidth - 48) / 8));
        drawWrappedText(x + 24, y + panelHeight - 38,
                "Reconnecting to " + snap.endpoint.host + ':' + std::to_string(snap.endpoint.port) + "…",
                columns, 4);
        drawText(x + 24, y + panelHeight - 122,
                 std::to_string(std::max(1u, snap.reconnectSeconds.value_or(1))) + " seconds remaining");
        drawText(x + 24, y + panelHeight - 148, "Your player slots are reserved");
        if (snap.canonical && (snap.canonical->phase == Network::Replication::Phase::ActiveRound
                               || snap.canonical->phase == Network::Replication::Phase::RoundSummary)) {
            drawText(x + 24, y + panelHeight - 174, "Match continues while you reconnect");
            drawWrappedText(x + 24, y + panelHeight - 198,
                    "Reserved players receive no input and remain in play", columns, 2);
        }
        renderer.quadXY(Vector(x + panelWidth / 2 - 100, y + 24), Vector(200, 34), Color(64, 96, 160));
        drawFocusKeyline(x + panelWidth / 2 - 100, y + 24, 200, 34, true);
        drawText(x + panelWidth / 2 - 52, y + 32, "Leave session", Color::WHITE);
    }

    void NetworkMenu::drawHostEndedPanel(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        const Int32 panelWidth = std::min<Int32>(640, width - 32);
        const Int32 panelHeight = std::min<Int32>(260, height - 32);
        const Int32 x = (width - panelWidth) / 2, y = (height - panelHeight) / 2;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(x, y), Vector(panelWidth, panelHeight), Color(224, 224, 224, 248));
        renderer.setBlendFunc(BlendFunc::None);
        drawText(x + 24, y + panelHeight - 40, "HOST ENDED SESSION");
        const auto columns = static_cast<std::size_t>(std::max<Int32>(12, (panelWidth - 48) / 8));
        drawWrappedText(x + 24, y + panelHeight - 76, "The host ended the session", columns, 2);
        drawWrappedText(x + 24, y + panelHeight - 112, "This session cannot be resumed", columns, 2);
        const bool matchActivityBegan = snap.canonical
                && (snap.canonical->round.has_value() || snap.canonical->result.available
                    || snap.canonical->completedRounds != 0
                    || snap.canonical->phase == Network::Replication::Phase::ActiveRound
                    || snap.canonical->phase == Network::Replication::Phase::RoundSummary
                    || snap.canonical->phase == Network::Replication::Phase::FinalSummary);
        if (matchActivityBegan)
            drawWrappedText(x + 24, y + panelHeight - 150,
                    "Session-only results were not saved to local statistics or Elo", columns, 2);
        renderer.quadXY(Vector(x + panelWidth / 2 - 110, y + 24), Vector(220, 34), Color(64, 96, 160));
        drawFocusKeyline(x + panelWidth / 2 - 110, y + 24, 220, 34, true);
        drawText(x + panelWidth / 2 - 68, y + 32, "Return to Network", Color::WHITE);
    }

    void NetworkMenu::drawLobby(const Client::NetworkRuntimeSnapshot &snap) const {
        const auto &state = *snap.canonical;
        drawClippedText(42, 516, (snap.host ? "Host" : "Guest") + std::string(" • LAN session • ")
                                + snap.endpoint.host + ":" + std::to_string(snap.endpoint.port) + " • "
                                + std::to_string(state.participants.size()) + " participants • "
                                + std::to_string(state.players.size()) + " players", 96);
        drawText(42, 482, "Role        Connection     Readiness   Owned");
        drawPlayers(state);
        constexpr Int32 settingsLeft = 584, settingsWidth = 226;
        renderer.quadXY(Vector(settingsLeft - 6, 250), Vector(settingsWidth + 12, 232), Color(216));
        renderer.frame(Vector(settingsLeft - 6, 250), Vector(settingsWidth + 12, 232), 1.0f, Color::BLACK);
        drawText(settingsLeft, 458, "HOST MATCH SETTINGS");
        const std::vector<std::string> settingRows{
                "Mode: " + state.settings.mode,
                "Team count: " + std::to_string(state.settings.teamCount),
                "Friendly fire: " + onOff(state.settings.friendlyFire),
                "Level plan: " + state.settings.levelPlan,
                "Fixed level: " + levelDisplayName(state.settings.fixedLevel),
                "Round limit: " + std::to_string(state.settings.roundLimit),
                "Assistance: " + onOff(state.settings.assistance),
                "Quick Liquid: " + onOff(state.settings.quickLiquid),
                "Burnable Trees: " + onOff(state.settings.burnableTrees)};
        const bool retainedResult = state.result.available;
        if (!retainedResult) {
            drawText(42, 378, "Pos  Owner       Person                  Control");
            drawText(526, 378, "Order");
            std::vector<Network::Replication::PlayerState> visiblePlayers = state.players;
            std::sort(visiblePlayers.begin(), visiblePlayers.end(), [](const auto &left, const auto &right) {
                return left.rosterPosition < right.rosterPosition;
            });
            const std::size_t first = std::min<std::size_t>(setupScroll,
                    visiblePlayers.size() > 6 ? visiblePlayers.size() - 6 : 0);
            const auto localParticipant = std::find_if(state.participants.begin(), state.participants.end(),
                    [&snap](const auto &participant) { return participant.participantId == snap.localParticipantId; });
            for (std::size_t row = 0; row < 6 && first + row < visiblePlayers.size(); ++row) {
                const auto &player = visiblePlayers[first + row];
                const Int32 rowY = 354 - static_cast<Int32>(row) * 24;
                drawText(42, rowY, std::to_string(player.rosterPosition + 1));
                drawClippedText(78, rowY, participantLabel(state, player.ownerParticipantId), 11);
                drawClippedText(170, rowY, player.displayName, 21);
                std::optional<std::size_t> localIndex;
                if (localParticipant != state.participants.end()
                    && player.ownerParticipantId == snap.localParticipantId) {
                    const auto owned = std::find(localParticipant->ownedPlayerIds.begin(),
                                                 localParticipant->ownedPlayerIds.end(), player.playerId);
                    if (owned != localParticipant->ownedPlayerIds.end())
                        localIndex = static_cast<std::size_t>(std::distance(localParticipant->ownedPlayerIds.begin(), owned));
                }
                const std::string control = localIndex && *localIndex < localPlayers.size()
                        && localPlayers[*localIndex].controls
                        ? localPlayers[*localIndex].controls->getDescription()
                        : localIndex ? "No control" : "Read-only";
                drawClippedText(354, rowY, control, 19);
                if (localIndex && *localIndex < localPlayers.size()) {
                    drawFocusKeyline(166, rowY - 3, 180, 20, focus == static_cast<int>(*localIndex) * 2);
                    drawFocusKeyline(350, rowY - 3, 164, 20, focus == static_cast<int>(*localIndex) * 2 + 1);
                }
                if (snap.host) {
                    const int rosterFocus = static_cast<int>(localPlayers.size()) * 2 + 10
                            + static_cast<int>(first + row);
                    drawText(528, rowY, focus == rosterFocus ? "> ↕" : "  ↕");
                    drawFocusKeyline(524, rowY - 3, 38, 20, focus == rosterFocus);
                }
            }
        } else {
            const auto controls = localControlWindow(focus, localPlayers.size(), true);
            for (std::size_t offset = 0; offset < controls.count; ++offset) {
                const std::size_t index = controls.first + offset;
                const auto rectangles = lobbyControlRectangles(offset, true);
                drawClippedText(LobbyPersonLeft + 2, rectangles.bottom + 2, "Person: " + localPlayers[index].name,
                                static_cast<std::size_t>((LobbyPersonWidth - 4) / 8));
                drawClippedText(LobbyControlLeft + 2, rectangles.bottom + 2,
                        "Control: " + (localPlayers[index].controls
                        ? localPlayers[index].controls->getDescription() : "No control"),
                        static_cast<std::size_t>((LobbyControlWidth - 4) / 8));
            }
        }
        const int readyIndex = static_cast<int>(localPlayers.size()) * 2;
        for (int index = 0; index < static_cast<int>(settingRows.size()); ++index) {
            const Int32 rowY = 430 - index * 20;
            const bool selected = snap.host && !retainedResult && focus == readyIndex + 1 + index;
            drawClippedText(settingsLeft + 4, rowY, (selected ? "> " : "  ") + settingRows[index], 26);
            if (snap.host && !retainedResult)
                drawFocusKeyline(settingsLeft, rowY - 3, settingsWidth, 19, selected);
        }
        drawText(42, retainedResult ? 144 : 172, focus == readyIndex ? "> Ready / Not ready" : "Ready / Not ready");
        drawFocusKeyline(40, retainedResult ? 140 : 168, 220, 20, focus == readyIndex);
        if (state.messages.status != "Lobby" && (!snap.host || !retainedResult))
            drawClippedText(42, retainedResult ? 118 : 144, state.messages.status, 96);
        if (snap.host) {
            static const char *labels[] = {"Mode", "Team count", "Friendly fire", "Level plan", "Fixed level",
                                           "Round limit", "Assistance", "Quick Liquid", "Burnable Trees"};
            if (retainedResult && focus >= readyIndex + 1 && focus < readyIndex + 10) {
                drawText(410, 166, std::string("> Change ") + labels[focus - readyIndex - 1]);
                drawFocusKeyline(408, 162, 200, 20, true);
            }
            std::vector<Network::Replication::PlayerState> roster = state.players;
            std::sort(roster.begin(), roster.end(), [](const auto &left, const auto &right) {
                return left.rosterPosition < right.rosterPosition;
            });
            const int rosterBase = readyIndex + 10;
            const auto visibleRoster = rosterWindow(focus, roster.size(), rosterBase, retainedResult);
            if (retainedResult && visibleRoster.count != 0) {
                const auto selectedRoster = visibleRoster.first;
                drawClippedText(410, 166, "> Reorder " + std::to_string(selectedRoster + 1) + ". "
                                + roster[selectedRoster].displayName, 47);
                drawFocusKeyline(408, 162, 376, 20, true);
            }
            std::string reason; const bool eligible = startEligible(snap, reason);
            drawClippedText(42, 116, reason.empty() ? "All participants are ready." : reason, 96);
            drawText(42, 94, "Optional scripts are disabled for network play.");
            const int startIndex = readyIndex + 10 + static_cast<int>(roster.size());
            drawText(410, 58, focus == startIndex ? (eligible ? "> Start match" : "> Start match (disabled)")
                                                   : (eligible ? "Start match" : "Start match (disabled)"));
            drawFocusKeyline(408, 54, 210, 20, eligible && focus == startIndex);
            drawText(675, 58, focus == startIndex + 1 ? "> End session" : "End session");
            drawFocusKeyline(670, 54, 150, 22, focus == startIndex + 1);
        } else {
            drawText(42, 116, "Host settings and authoritative roster order are read-only.");
            drawText(42, 94, "Optional scripts are disabled for network play.");
            drawText(650, 58, focus == readyIndex + 1 ? "> Leave session" : "Leave session");
            drawFocusKeyline(645, 54, 175, 22, focus == readyIndex + 1);
        }
        if (state.result.available) drawResult(state, true);
    }

    void NetworkMenu::drawSummary(const Client::NetworkRuntimeSnapshot &snap) const {
        const auto &state = *snap.canonical;
        drawText(50, 516, "SESSION ONLY • Completed • Not saved to local statistics or Elo");
        drawText(50, 494, "Match outcome: " + outcome(state, state.score.winner));
        if (state.round) drawText(50, 472, "Last completed round " + std::to_string(state.completedRounds)
                                            + ": " + outcome(state, state.round->outcome));
        drawResult(state, false);
        if (snap.host) {
            drawAction(72, "Return to lobby", focus == 0); drawAction(30, "End session", focus == 1);
        } else {
            drawText(50, 70, "Waiting for host to return to lobby or end session");
            drawAction(30, "Leave", focus == 0);
        }
    }

    void NetworkMenu::drawRetainedContext(
            const Client::NetworkRuntimeSnapshot &snap, Int32 width, Int32 height) const {
        renderer.setViewMatrix(Matrix::IDENTITY);
        if (snap.canonical && snap.canonical->round
            && (snap.canonical->phase == Network::Replication::Phase::ActiveRound
                || snap.canonical->phase == Network::Replication::Phase::RoundSummary)) {
            drawMatch(snap, width, height, false, "Last confirmed state", false);
            return;
        }
        drawMenuCanvas(width, height);
        if (!snap.canonical) {
            drawText(50, 542, "LAST CONFIRMED NETWORK CONTEXT");
            renderer.setViewMatrix(Matrix::IDENTITY);
            return;
        }
        const auto &state = *snap.canonical;
        if (state.phase == Network::Replication::Phase::FinalSummary) {
            drawText(294, 542, "MATCH SUMMARY • LAST CONFIRMED");
            drawSummary(snap);
        } else {
            drawText(286, 542, "NETWORK LOBBY • LAST CONFIRMED");
            drawLobby(snap);
        }
        renderer.setViewMatrix(Matrix::IDENTITY);
    }

    void NetworkMenu::drawResult(const Network::Replication::CanonicalState &state, bool retained) const {
        const Int32 top = retained ? 365 : 450;
        const Int32 bottom = retained ? 190 : state.phase == Network::Replication::Phase::FinalSummary ? 120 : 62;
        const bool finalSummary = !retained && state.phase == Network::Replication::Phase::FinalSummary;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(32, bottom), Vector(786, top - bottom), Color(224, 224, 224, 240));
        renderer.setBlendFunc(BlendFunc::None);
        if (!finalSummary) {
            drawText(50, top - 22, retained ? "RETAINED RESULT • SESSION ONLY" : "AUTHORITATIVE SCORE • SESSION ONLY");
            drawText(50, top - 42, "State: " + (state.result.available ? state.result.state : "In progress"));
            drawText(50, top - 62, "Match outcome: " + outcome(state, state.score.winner));
            if (state.round) drawText(50, top - 80, "Last completed round " + std::to_string(state.completedRounds)
                                              + ": " + outcome(state, state.round->outcome));
        }
        auto lines = resultLines(state);
        const auto bounds = resultScrollBounds(state, retained);
        const std::size_t appliedHorizontal = std::min<std::size_t>(summaryHorizontal, bounds.horizontal);
        const std::size_t first = std::min<std::size_t>(summaryScroll, bounds.vertical);
        std::string section = "MATCH SETTINGS";
        for (std::size_t index = 0; index <= first && index < lines.size(); ++index) {
            if (lines[index] == "MATCH SETTINGS" || lines[index] == "ROUND RESULTS"
                || lines[index] == "CUMULATIVE PLAYER RESULTS" || lines[index] == "TEAM TOTALS")
                section = lines[index];
        }
        const std::string columns = section == "ROUND RESULTS"
                ? "Round  Level  Orientation  Outcome  Roster"
                : section == "CUMULATIVE PLAYER RESULTS"
                  ? "Rank  Player / owner / team / state  R Sh Hi K D A W P Survival Damage Assist dmg Points"
                  : section == "TEAM TOTALS" ? "Rank  Team  Points" : "Setting  Value";
        const Int32 stickyBottom = finalSummary ? top - 50 : top - 122;
        const Int32 stickyHeadingY = finalSummary ? top - 26 : top - 98;
        const Int32 stickyColumnsY = finalSummary ? top - 44 : top - 116;
        renderer.quadXY(Vector(42, stickyBottom), Vector(766, 42), Color(208));
        drawText(50, stickyHeadingY, section);
        drawText(50, stickyColumnsY, utf8Clipped(columns, 94));
        Int32 y = finalSummary ? top - 68 : top - 136;
        std::size_t start = first;
        if (start < lines.size() && lines[start] == section) ++start;
        std::size_t shown = 0;
        for (std::size_t index = start; index < lines.size() && shown < bounds.visibleRows; ++index, ++shown, y -= 18) {
            const std::size_t horizontal = std::min<std::size_t>(appliedHorizontal, utf8Length(lines[index]));
            drawText(50, y, utf8Slice(lines[index], horizontal, 94));
        }
        const auto snap = runtime.snapshot();
        const bool focused = focus == resultFocusIndex(snap, localPlayers.size());
        renderer.quadXY(Vector(50, bottom + 4), Vector(32, 22), Color(192));
        renderer.quadXY(Vector(82, bottom + 4), Vector(492, 22), Color(208));
        renderer.quadXY(Vector(574, bottom + 4), Vector(32, 22), Color(192));
        if (bounds.horizontal != 0) {
            const Float32 track = 476.0f;
            const Float32 thumbWidth = std::max(24.0f, track * 94.0f / static_cast<Float32>(bounds.widest));
            const Float32 position = static_cast<Float32>(appliedHorizontal) /
                    static_cast<Float32>(bounds.horizontal) * (track - thumbWidth);
            renderer.quadXY(Vector(90.0f + position, static_cast<Float32>(bottom + 9)),
                            Vector(thumbWidth, 12.0f), Color(64, 96, 160));
        }
        drawFocusKeyline(50, bottom + 4, 556, 22, focused);
        drawText(62, bottom + 8, "<"); drawText(586, bottom + 8, ">");
        const std::string position = "Columns " + std::to_string(bounds.widest == 0 ? 0 : appliedHorizontal + 1) + "–"
                + std::to_string(std::min(bounds.widest, appliedHorizontal + 94)) + "/"
                + std::to_string(bounds.widest) + " • Rows "
                + std::to_string(lines.empty() ? 0 : first + 1) + "–"
                + std::to_string(std::min(lines.size(), first + bounds.visibleRows)) + "/"
                + std::to_string(lines.size());
        drawText(330 - static_cast<Int32>(utf8Length(position)) * 4, bottom + 8, position);
        drawText(620, bottom + 8, "PgUp/PgDn • ←/→");
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
        drawText(width / 2 - static_cast<Int32>(utf8Length(prompt)) * 4, height / 2 + 60, prompt);
        if (!promptSecond.empty())
            drawText(width / 2 - static_cast<Int32>(utf8Length(promptSecond)) * 4, height / 2 + 40, promptSecond);
        renderer.quadXY(Vector(width / 2 - 190, height / 2 - 55), Vector(160, 34), focus == 0 ? Color(64, 96, 160) : Color(192));
        renderer.quadXY(Vector(width / 2 + 30, height / 2 - 55), Vector(160, 34), focus == 1 ? Color(64, 96, 160) : Color(192));
        drawFocusKeyline(width / 2 - 190, height / 2 - 55, 160, 34, focus == 0);
        drawFocusKeyline(width / 2 + 30, height / 2 - 55, 160, 34, focus == 1);
        drawText(width / 2 - 135, height / 2 - 47, confirmation == Confirmation::End ? "End session" : "Leave session",
                 focus == 0 ? Color::WHITE : Color::BLACK);
        drawText(width / 2 + 90, height / 2 - 47, "Cancel", focus == 1 ? Color::WHITE : Color::BLACK);
    }

    void NetworkMenu::render() const {
        const auto width = service.getVideo().getScreen().getClientWidth();
        const auto height = service.getVideo().getScreen().getClientHeight();
        const auto snap = runtime.snapshot();
        if (snap.journey == Client::NetworkJourney::HostEnded) {
            drawRetainedContext(snap, width, height);
            drawHostEndedPanel(snap, width, height);
            return;
        }
        if (snap.journey == Client::NetworkJourney::Reconnecting) {
            drawRetainedContext(snap, width, height);
            drawReconnectPanel(snap, width, height);
            if (confirmation != Confirmation::None) drawConfirmation();
            return;
        }
        if (snap.journey == Client::NetworkJourney::Match && snap.canonical) {
            drawMatch(snap, width, height); return;
        }
        drawMenuCanvas(width, height);
        std::string title = "NETWORK PLAY";
        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Host) title = "HOST NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Join) title = "JOIN NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Starting
                 && !snap.host && setupScreen == SetupScreen::Join) title = "JOIN NETWORK SESSION";
        else if (snap.journey == Client::NetworkJourney::Lobby) title = "NETWORK LOBBY";
        else if (snap.journey == Client::NetworkJourney::Summary) title = "MATCH SUMMARY";
        else if (snap.journey == Client::NetworkJourney::Reconnecting) title = "RECONNECTING";
        else if (snap.journey == Client::NetworkJourney::Failure) title = snap.host ? "SESSION ENDED" : "CONNECTION FAILED";
        else if (snap.journey == Client::NetworkJourney::HostEnded && snap.canonical
                 && snap.canonical->phase == Network::Replication::Phase::Lobby) title = "NETWORK LOBBY";
        else if (snap.journey == Client::NetworkJourney::HostEnded && snap.canonical
                 && snap.canonical->phase == Network::Replication::Phase::FinalSummary) title = "MATCH SUMMARY";
        else if (snap.journey == Client::NetworkJourney::HostEnded) title = "HOST ENDED SESSION";
        drawText(425 - static_cast<Int32>(utf8Length(title)) * 4, 542, title);

        if (snap.journey == Client::NetworkJourney::Inactive && setupScreen == SetupScreen::Entry) {
            drawText(285, 505, "Same machine or private LAN"); drawText(285, 480, "Direct address and port");
            drawText(285, 455, "Linux / Windows x86-64");
            drawText(165, 415, "Player-hosted • Lobby 1–15 • Match 2–15 participants and players");
            drawAction(325, "Host", focus == 0); drawAction(280, "Join", focus == 1); drawAction(235, "Back", focus == 2);
        } else if (snap.journey == Client::NetworkJourney::Inactive) {
            const int fields = 2;
            drawText(50, 516, "Same machine or LAN • Linux / Windows x86-64");
            Int32 y = 490;
            if (setupScreen == SetupScreen::Join) {
                drawClippedText(50, y, "Address: " + address + (focus == 0 ? " <" : ""), 88);
                drawFocusKeyline(48, 486, 764, 22, focus == 0);
                y -= 24;
                drawText(50, y, "Port: " + port + (focus == 1 ? " <" : ""));
                drawFocusKeyline(48, 462, 764, 22, focus == 1);
            } else {
                drawText(50, y, "Port: " + port + (focus == 0 ? " <" : ""));
                drawFocusKeyline(48, 486, 764, 22, focus == 0);
                y -= 24;
                drawClippedText(50, y, "Listening interface: " + listeningAddressLabel(hostAddress)
                        + (focus == 1 ? (hostAddressSelectorOpen ? " < Select" : " < Enter opens") : ""), 92);
                drawFocusKeyline(48, 462, 764, 22, focus == 1);
            }
            drawText(50, 430, "PERSONS"); drawText(430, 430, "LOCAL PLAYERS AND CONTROLS");
            const int focusedPerson = focus >= fields && focus < fields + static_cast<int>(availablePersons.size())
                                      ? focus - fields : 0;
            const std::size_t firstPerson = static_cast<std::size_t>(std::max(0, focusedPerson - 9));
            y = 408;
            for (std::size_t index = firstPerson; index < availablePersons.size() && index < firstPerson + 10; ++index, y -= 18) {
                const bool selected = std::any_of(localPlayers.begin(), localPlayers.end(), [&](const auto &p) { return p.name == availablePersons[index]; });
                drawClippedText(54, y, (focus == fields + static_cast<int>(index) ? "> " : "  ") + availablePersons[index]
                                        + (selected ? " • Selected" : " • Add"), 42);
                drawFocusKeyline(50, y - 4, 350, 18, focus == fields + static_cast<int>(index));
            }
            y = 408; const int playerBase = fields + static_cast<int>(availablePersons.size());
            const int focusedPlayer = focus >= playerBase && focus < playerBase + static_cast<int>(localPlayers.size()) * 2
                                      ? (focus - playerBase) / 2 : 0;
            const std::size_t firstPlayer = static_cast<std::size_t>(std::max(0, focusedPlayer - 9));
            for (std::size_t index = firstPlayer; index < localPlayers.size() && index < firstPlayer + 10; ++index, y -= 22) {
                drawClippedText(434, y, (focus == playerBase + static_cast<int>(index) * 2 ? "> " : "  ")
                                       + localPlayers[index].name + " • "
                                       + (localPlayers[index].controls ? localPlayers[index].controls->getDescription() : "No control"), 35);
                drawText(730, y, focus == playerBase + static_cast<int>(index) * 2 + 1 ? "> Remove" : "Remove");
                drawFocusKeyline(430, y - 4, 290, 18, focus == playerBase + static_cast<int>(index) * 2);
                drawFocusKeyline(724, y - 4, 96, 18, focus == playerBase + static_cast<int>(index) * 2 + 1);
            }
            std::string reason; const bool valid = setupValid(reason);
            drawText(50, 182, "Local players: " + std::to_string(localPlayers.size()) + " • Lobby 1–15 • Match 2–15");
            drawText(50, 160, "Same machine or LAN • Session-only scores • Optional scripts disabled");
            drawText(50, 138, reason);
            const int footer = playerBase + static_cast<int>(localPlayers.size()) * 2;
            drawAction(82, valid ? (setupScreen == SetupScreen::Host ? "Start session" : "Connect")
                                 : (setupScreen == SetupScreen::Host ? "Start session (disabled)" : "Connect (disabled)"), focus == footer);
            drawAction(38, "Back", focus == footer + 1);
            if (setupScreen == SetupScreen::Host && hostAddressSelectorOpen) {
                const std::size_t visible = std::min<std::size_t>(8, hostAddresses.size());
                const std::size_t maximumFirst = hostAddresses.size() > visible ? hostAddresses.size() - visible : 0;
                const std::size_t first = std::min(hostAddressScroll, maximumFirst);
                for (std::size_t row = 0; row < visible; ++row) {
                    const std::size_t index = first + row;
                    const Int32 optionY = 440 - static_cast<Int32>(row) * 22;
                    renderer.quadXY(Vector(48, optionY), Vector(764, 22),
                                    index == hostAddressHighlight ? Color(64, 96, 160) : Color::WHITE);
                    drawClippedText(54, optionY + 3,
                            (index == hostAddressHighlight ? "> " : "  ") + listeningAddressLabel(hostAddresses[index]),
                            92, index == hostAddressHighlight ? Color::WHITE : Color::BLACK);
                    renderer.frame(Vector(48, optionY), Vector(764, 22), 1.0f, Color::BLACK);
                }
            }
        } else if (snap.journey == Client::NetworkJourney::Starting || snap.journey == Client::NetworkJourney::Cancelling) {
            if (snap.journey == Client::NetworkJourney::Starting
                && !snap.host && setupScreen == SetupScreen::Join) {
                drawClippedText(50, 516, "Hostname or address: " + snap.endpoint.host, 68);
                drawText(650, 516, "Port: " + std::to_string(snap.endpoint.port));
                drawText(50, 470, "LOCAL PLAYERS " + std::to_string(localPlayers.size()) + " • LOCKED / READ-ONLY");
                renderer.frame(Vector(48, 158), Vector(764, 304), 1.0f, Color::BLACK);
                drawText(56, 438, "Slot  Person");
                drawText(430, 438, "Control");
                Int32 playerY = 414;
                for (std::size_t index = 0; index < localPlayers.size() && index < 15; ++index, playerY -= 18) {
                    drawText(56, playerY, std::to_string(index + 1));
                    drawClippedText(94, playerY, localPlayers[index].name, 38);
                    drawClippedText(430, playerY, localPlayers[index].controls
                            ? localPlayers[index].controls->getDescription() : "No control", 46);
                }
                drawWrappedText(50, 136, snap.status, 94, 2);
                drawText(50, 94, "Connection deadline: 10 seconds total");
                drawAction(38, "Cancel", true);
            } else {
                drawText(300, 430, snap.status);
                if (snap.journey == Client::NetworkJourney::Starting) {
                    drawText(270, 400, "Startup can take up to 10 seconds."); drawAction(300, "Cancel", true);
                }
            }
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Lobby) {
            drawLobby(snap);
        } else if (snap.canonical && snap.journey == Client::NetworkJourney::Summary) {
            drawSummary(snap);
        } else if (snap.journey == Client::NetworkJourney::Failure) {
            drawWrappedText(130, 470, snap.failure.empty() ? "Connection could not be completed." : snap.failure,
                            72, 3);
            if (!snap.host) drawClippedText(130, 404,
                    "Endpoint: " + snap.endpoint.host + ':' + std::to_string(snap.endpoint.port), 72);
            if (!snap.host) drawWrappedText(130, 374,
                    "Check that the host session is running and the endpoint is correct.", 72, 2);
            std::string retryReason;
            const bool canRetry = retryEligible(snap, retryReason);
            int selected = 0;
            const auto drawFailureAction = [&](Int32 x, Int32 buttonWidth, const std::string &label, bool active) {
                renderer.quadXY(Vector(x, 270), Vector(buttonWidth, 34), active ? Color(64, 96, 160) : Color(192));
                drawFocusKeyline(x, 270, buttonWidth, 34, active);
                drawText(x + buttonWidth / 2 - static_cast<Int32>(utf8Length(label)) * 4, 279, label,
                         active ? Color::WHITE : Color::BLACK);
            };
            drawFailureAction(54, 220, canRetry ? "Retry" : "Retry unavailable", canRetry && focus == selected++);
            drawFailureAction(315, 220, "Edit setup", focus == selected++);
            drawFailureAction(576, 220, "Return to Network", focus == selected);
            if (!canRetry) drawText(54, 238, retryReason);
        }
        if (confirmation != Confirmation::None) drawConfirmation();
        renderer.setViewMatrix(Matrix::IDENTITY);
    }
}
