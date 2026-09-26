#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <list>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#endif

#include "tests/TestHarness.h"
#include "source/client/HostServiceSupervisor.h"
#include "source/DataException.h"
#define private public
#include "source/Font.h"
#undef private
#include "source/input/PlayerControls.h"
#include "source/network/HostCompositionProtocol.h"
#include "source/network/NetworkResponsiveness.h"
#include "source/network/PlayerInputProtocol.h"
#include "source/network/Protocol.h"
#include "source/network/SessionLifecycle.h"
#include "source/network/StateReplication.h"
#include "source/network/StateReplicationProtocol.h"

// This test-only access exposes queued application commands and snapshots without
// adding a production diagnostic API solely for QA.
#define private public
#include "source/client/NetworkSessionRuntime.h"
#include "source/Application.h"
#include "source/CanonicalWorldPresenter.h"
#include "source/NetworkMenu.h"
#undef private
#include "tests/RecordingRenderer.h"
#include "tests/CanonicalMotionTrace.h"
#include "source/server/AuthoritativeMatchSerialization.h"
#include "source/math/Camera.h"
#include <cmath>

namespace {
    using namespace Duel6;

    Network::Replication::CanonicalState canonical(std::uint64_t sessionId,
                                                     Network::Replication::Phase phase) {
        Network::Replication::CanonicalState state;
        state.sessionId = sessionId;
        state.phase = phase;
        return state;
    }

    Client::NetworkLocalPlayer player(std::string name) {
        Client::NetworkLocalPlayer value;
        value.name = std::move(name);
        return value;
    }

#ifndef _WIN32
    std::uint16_t unusedLoopbackPort() {
        const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
        D6R_REQUIRE(descriptor >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        D6R_REQUIRE(::bind(descriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0);
        socklen_t size = sizeof(address);
        D6R_REQUIRE(::getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size) == 0);
        const auto port = ntohs(address.sin_port);
        ::close(descriptor);
        D6R_REQUIRE(port != 0);
        return port;
    }

    template<typename Predicate>
    bool pumpRuntimes(Client::NetworkSessionRuntime &host,
                      Client::NetworkSessionRuntime &first,
                      Client::NetworkSessionRuntime &second,
                      std::chrono::milliseconds timeout,
                      Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            host.update(); first.update(); second.update();
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        host.update(); first.update(); second.update();
        return predicate();
    }

    bool sixPlayerLobby(const Client::NetworkRuntimeSnapshot &snapshot) {
        return snapshot.journey == Client::NetworkJourney::Lobby && snapshot.canonical
               && snapshot.canonical->phase == Network::Replication::Phase::Lobby
               && snapshot.canonical->participants.size() == 3
               && snapshot.canonical->players.size() == 6;
    }

    bool participantReady(const Client::NetworkRuntimeSnapshot &snapshot,
                          Network::Replication::Identity participantId) {
        if (!snapshot.canonical) return false;
        const auto found = std::find_if(snapshot.canonical->participants.begin(),
                snapshot.canonical->participants.end(), [participantId](const auto &participant) {
                    return participant.participantId == participantId;
                });
        return found != snapshot.canonical->participants.end() && found->ready;
    }

    bool everyParticipantReady(const Client::NetworkRuntimeSnapshot &snapshot, bool ready) {
        return snapshot.canonical && std::all_of(snapshot.canonical->participants.begin(),
                snapshot.canonical->participants.end(), [ready](const auto &participant) {
                    return participant.ready == ready;
                });
    }

    std::vector<std::uint8_t> canonicalFingerprint(const Client::NetworkRuntimeSnapshot &snapshot) {
        D6R_REQUIRE(snapshot.canonical.has_value());
        return Network::Replication::serializeReplicationSnapshot(
                {1, *snapshot.canonical, snapshot.canonical->phaseTime});
    }

    std::string lobbyConfigurationFingerprint(const Client::NetworkRuntimeSnapshot &snapshot) {
        D6R_REQUIRE(snapshot.canonical.has_value());
        const auto &state = *snapshot.canonical;
        std::ostringstream result;
        result << state.settings.mode << ':' << unsigned(state.settings.teamCount)
               << ':' << state.settings.friendlyFire << ':' << state.settings.levelPlan
               << ':' << state.settings.fixedLevel << ':' << unsigned(state.settings.roundLimit)
               << ':' << state.settings.assistance << ':' << state.settings.quickLiquid
               << ':' << state.settings.burnableTrees << ';';
        auto participants = state.participants;
        std::sort(participants.begin(), participants.end(), [](const auto &left, const auto &right) {
            return left.participantId < right.participantId;
        });
        for (const auto &participant: participants) {
            result << 'p' << participant.participantId << ':' << participant.host << ':'
                   << unsigned(participant.connection) << ':' << participant.ready << '[';
            for (const auto id: participant.ownedPlayerIds) result << id << ',';
            result << "];";
        }
        auto players = state.players;
        std::sort(players.begin(), players.end(), [](const auto &left, const auto &right) {
            return left.playerId < right.playerId;
        });
        for (const auto &slot: players)
            result << 's' << slot.playerId << ':' << slot.ownerParticipantId << ':'
                   << unsigned(slot.rosterPosition) << ':' << slot.displayName << ';';
        result << "totals[";
        for (const auto total: state.score.teamTotals) result << total << ',';
        result << "];ranking[";
        for (const auto team: state.score.teamRanking) result << unsigned(team) << ',';
        result << ']';
        return result.str();
    }

    class ScopedLevelVariant final {
    public:
        explicit ScopedLevelVariant(const std::string &sourceLevel)
                : prior(std::filesystem::current_path()),
                  root(std::filesystem::temp_directory_path()
                       / ("duel6r-background-test-" + std::to_string(::getpid()))) {
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root / "levels");
            const auto resourceRoot = std::filesystem::path(sourceLevel).parent_path().parent_path();
            std::filesystem::create_directory_symlink(resourceRoot / "sound", root / "sound");
            std::ifstream input(sourceLevel);
            original.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            D6R_REQUIRE(!original.empty());
            write("");
            std::filesystem::current_path(root);
        }

        ~ScopedLevelVariant() {
            std::filesystem::current_path(prior);
            std::filesystem::remove_all(root);
        }

        void write(const std::string &background) const {
            std::string value = original;
            if (!background.empty()) {
                const auto opening = value.find('{');
                D6R_REQUIRE(opening != std::string::npos);
                value.insert(opening + 1, "\n  \"background\" : \"" + background + "\",");
            }
            std::ofstream output(root / "levels/duel_01.json", std::ios::trunc);
            output << value;
            D6R_REQUIRE(output.good());
        }

    private:
        std::filesystem::path prior;
        std::filesystem::path root;
        std::string original;
    };
#endif
}

D6R_TEST_CASE("issue-38 guest participant commands preserve order and reconnect Leave preempts queued work") {
    Client::NetworkSessionRuntime runtime;
    runtime.current.localParticipantId = 17;
    runtime.current.canonical = canonical(91, Network::Replication::Phase::Lobby);

    runtime.setReady(true);
    runtime.localConfigurationChanged();
    runtime.setReady(false);
    D6R_REQUIRE_EQ(3u, runtime.pendingGuestCommands.size());

    const std::vector<Network::Lifecycle::ParticipantActionKind> expected{
            Network::Lifecycle::ParticipantActionKind::Ready,
            Network::Lifecycle::ParticipantActionKind::ConfigurationChanged,
            Network::Lifecycle::ParticipantActionKind::NotReady};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto action = Network::Lifecycle::deserializeParticipantAction(
                runtime.pendingGuestCommands[index]);
        D6R_REQUIRE(action.has_value());
        D6R_REQUIRE_EQ(91u, action->sessionId);
        D6R_REQUIRE_EQ(17u, action->participantId);
        D6R_REQUIRE(action->kind == expected[index]);
    }

    runtime.current.journey = Client::NetworkJourney::Reconnecting;
    runtime.leave();
    D6R_REQUIRE_EQ(1u, runtime.pendingGuestCommands.size());
    const auto leave = Network::Lifecycle::deserializeParticipantAction(
            runtime.pendingGuestCommands.front());
    D6R_REQUIRE(leave.has_value());
    D6R_REQUIRE(leave->kind == Network::Lifecycle::ParticipantActionKind::Leave);
    D6R_REQUIRE(runtime.current.journey == Client::NetworkJourney::Cancelling);
    D6R_REQUIRE_EQ(std::string("Leaving session…"), runtime.current.status);
}

D6R_TEST_CASE("issue-38 admitted local slot count is immutable while valid same-count rebinding remains editable") {
    Client::NetworkSessionRuntime runtime;
    runtime.players = {player("Alpha"), player("Beta")};
    runtime.sampledActions = {1, 2};

    runtime.rebindLocalPlayers({player("Only one")});
    D6R_REQUIRE_EQ(2u, runtime.players.size());
    D6R_REQUIRE_EQ(std::string("Alpha"), runtime.players[0].name);

    runtime.rebindLocalPlayers({player("Gamma"), player(std::string("bad\nname"))});
    D6R_REQUIRE_EQ(std::string("Alpha"), runtime.players[0].name);
    D6R_REQUIRE_EQ(std::string("Beta"), runtime.players[1].name);

    runtime.rebindLocalPlayers({player("Gamma"), player("Delta")});
    D6R_REQUIRE_EQ(2u, runtime.players.size());
    D6R_REQUIRE_EQ(std::string("Gamma"), runtime.players[0].name);
    D6R_REQUIRE_EQ(std::string("Delta"), runtime.players[1].name);
    D6R_REQUIRE_EQ(2u, runtime.sampledActions.size());
    D6R_REQUIRE_EQ(0u, runtime.sampledActions[0]);
    D6R_REQUIRE_EQ(0u, runtime.sampledActions[1]);
}

D6R_TEST_CASE("issue-38 canonical phase mapping and presenter event retention are deterministic and bounded") {
    Client::NetworkSessionRuntime runtime;
    runtime.current.host = true;
    runtime.current.journey = Client::NetworkJourney::Starting;

    const std::vector<std::pair<Network::Replication::Phase, Client::NetworkJourney>> phases{
            {Network::Replication::Phase::Lobby, Client::NetworkJourney::Lobby},
            {Network::Replication::Phase::ActiveRound, Client::NetworkJourney::Match},
            {Network::Replication::Phase::RoundSummary, Client::NetworkJourney::Match},
            {Network::Replication::Phase::FinalSummary, Client::NetworkJourney::Summary},
            {Network::Replication::Phase::Ended, Client::NetworkJourney::HostEnded}};
    for (const auto &[phase, journey]: phases) {
        auto state = canonical(91, phase);
        state.hostParticipantId = 7;
        runtime.applyCanonical(state);
        const auto snapshot = runtime.snapshot();
        D6R_REQUIRE(snapshot.journey == journey);
        D6R_REQUIRE_EQ(7u, snapshot.localParticipantId);
        D6R_REQUIRE_EQ(std::string("Connected"), snapshot.status);
    }

    std::vector<Network::Replication::PresentationEvent> events;
    for (std::size_t index = 0; index < Network::Replication::MaxReplicatedEvents + 3; ++index) {
        Network::Replication::PresentationEvent event;
        event.eventId = index + 1;
        events.push_back(event);
    }
    runtime.applyCanonical(canonical(91, Network::Replication::Phase::Lobby), {}, {}, events);
    const auto snapshot = runtime.snapshot();
    D6R_REQUIRE_EQ(Network::Replication::MaxReplicatedEvents, snapshot.presentationEvents.size());
    D6R_REQUIRE_EQ(4u, snapshot.presentationEvents.front().eventId);
}

D6R_TEST_CASE("issue-38 host retry categories distinguish cleanup restart invalid setup and ended session") {
    Client::NetworkSessionRuntime runtime;

    Client::HostServiceSnapshot observed;
    observed.state = Client::HostServiceState::Stopping;
    observed.stopReason = Client::HostServiceStopReason::StartupFailure;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.journey == Client::NetworkJourney::Failure);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::CleanupInProgress);
    D6R_REQUIRE(!runtime.current.retryAllowed);

    observed = {};
    observed.state = Client::HostServiceState::StartupFailed;
    observed.outcome = Client::HostServiceOutcome::HostManifestInvalid;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::RestartRequired);

    observed.outcome = Client::HostServiceOutcome::PortUnavailable;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::InvalidSetup);

    observed = {};
    observed.state = Client::HostServiceState::SessionFailed;
    observed.outcome = Client::HostServiceOutcome::StoppedUnexpectedly;
    runtime.observeHostLifecycle(observed);
    D6R_REQUIRE(runtime.current.retryBlockReason == Client::NetworkRetryBlockReason::EndedSession);
    D6R_REQUIRE_EQ(std::string("Hosted session stopped unexpectedly."), runtime.current.failure);
}

D6R_TEST_CASE("issue-38 host composition framing is deterministic for every UI action and setup boundary") {
    Network::HostComposition::Setup setup;
    for (unsigned index = 0; index < Network::MaxNetworkPlayers; ++index)
        setup.localPlayerNames.push_back(std::string(63, 'A' + index) + char('a' + index));
    setup.fixedLevel = std::string(60, 'a') + '/' + std::string(59, 'b') + '/'
                       + std::string(59, 'c') + '/' + std::string(59, 'd');
    D6R_REQUIRE_EQ(240u, setup.fixedLevel.size());

    const auto encodedSetup = Network::HostComposition::serializeSetup(setup);
    const auto decodedSetup = Network::HostComposition::deserialize(encodedSetup);
    D6R_REQUIRE(decodedSetup.has_value());
    D6R_REQUIRE(decodedSetup->kind == Network::HostComposition::Kind::Setup);
    D6R_REQUIRE(decodedSetup->setup.has_value());
    D6R_REQUIRE_EQ(Network::MaxNetworkPlayers, decodedSetup->setup->localPlayerNames.size());
    D6R_REQUIRE_EQ(setup.fixedLevel, decodedSetup->setup->fixedLevel);
    D6R_REQUIRE_EQ(encodedSetup, Network::HostComposition::serializeSetup(*decodedSetup->setup));

    const std::vector<Network::HostComposition::Kind> actions{
            Network::HostComposition::Kind::StartMatch,
            Network::HostComposition::Kind::ReturnToLobby,
            Network::HostComposition::Kind::AdvanceRound,
            Network::HostComposition::Kind::ConfigurationChanged,
            Network::HostComposition::Kind::Ready,
            Network::HostComposition::Kind::NotReady};
    for (const auto kind: actions) {
        const auto payload = Network::HostComposition::serializeAction(kind);
        D6R_REQUIRE_EQ(8u, payload.size());
        const auto decoded = Network::HostComposition::deserialize(payload);
        D6R_REQUIRE(decoded.has_value());
        D6R_REQUIRE(decoded->kind == kind);
        auto trailing = payload;
        trailing.push_back(0);
        D6R_REQUIRE(!Network::HostComposition::deserialize(trailing).has_value());
    }

    auto unknown = Network::HostComposition::serializeAction(Network::HostComposition::Kind::NotReady);
    unknown[6] = 0;
    unknown[7] = 14;
    D6R_REQUIRE(!Network::HostComposition::deserialize(unknown).has_value());
}

D6R_TEST_CASE("NET-DIR browser retains failed results but never grants stale eligibility") {
    using Clock = std::chrono::steady_clock;
    Client::DirectoryBrowser browser;
    D6R_REQUIRE(browser.stale());
    Client::DirectoryListing row;
    row.id = std::string(32, 'a'); row.sessionId = std::string(32, 'b');
    row.endpoint = {"127.0.0.1", 25000}; row.mode = "predator"; row.phase = "first-round";
    row.players = 2; row.capacity = 15; row.passwordRequired = true;
    row.expiresAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    browser.page = {true, {row}, {}};
    browser.requested = browser.received = Clock::now();
    D6R_REQUIRE(!browser.stale() && row.joinable());
    auto full = row; full.players = full.capacity; D6R_REQUIRE(!full.joinable());
    auto closed = row; closed.phase = "closed"; D6R_REQUIRE(!closed.joinable());
    auto expired = row; expired.expiresAt = 0; D6R_REQUIRE(!expired.joinable());
    browser.received -= std::chrono::seconds(30);
    D6R_REQUIRE(browser.stale());
    browser.received = Clock::now();
    std::promise<Client::DirectoryPage> response;
    browser.pending = response.get_future();
    D6R_REQUIRE(browser.loading());
    response.set_value({}); browser.update();
    D6R_REQUIRE(browser.stale() && !browser.loading());
    D6R_REQUIRE_EQ(std::size_t(1), browser.result().listings.size());
    D6R_REQUIRE_EQ(row.id, browser.result().listings[0].id);
}

D6R_TEST_CASE("NET-DIR response decoder bounds nesting pages cursors and numeric fields") {
    const std::string row = "{\"id\":\"" + std::string(32, 'a') + "\",\"sessionId\":\"" + std::string(32, 'b')
        + "\",\"address\":\"127.0.0.1\",\"port\":25000,\"mode\":\"predator\",\"phase\":\"closed\","
          "\"players\":2,\"capacity\":15,\"passwordRequired\":true,\"revision\":1,\"expiresAt\":2000000000000}";
    const std::string valid = "{\"listings\":[" + row + "],\"nextCursor\":null}";
    D6R_REQUIRE(Client::decodeDirectoryPage(valid).available);
    D6R_REQUIRE(!Client::decodeDirectoryPage(valid + "{}").available);
    D6R_REQUIRE(!Client::decodeDirectoryPage(std::string(20000, '[') + std::string(20000, ']')).available);
    D6R_REQUIRE(!Client::decodeDirectoryPage(std::string(65537, ' ')).available);
    std::string fractional = valid;
    fractional.replace(fractional.find("\"players\":2"), std::string("\"players\":2").size(), "\"players\":2.5");
    D6R_REQUIRE(!Client::decodeDirectoryPage(fractional).available);
    std::string tooMany = "{\"listings\":[";
    for (int index = 0; index < 26; ++index) tooMany += (index == 0 ? "" : ",") + row;
    tooMany += "],\"nextCursor\":null}";
    D6R_REQUIRE(!Client::decodeDirectoryPage(tooMany).available);
    D6R_REQUIRE(!Client::decodeDirectoryPage("{\"listings\":[],\"nextCursor\":\"../bad\"}").available);
}

#ifndef _WIN32
D6R_TEST_CASE("NET-DIR reviewed menu dispatch renders browser states and preserves navigation focus") {
    char name[] = "duel6r-browser-render-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recording = std::make_unique<Test::RecordingRenderer>();
    auto &recorder = *recording; video.renderer = std::move(recording);
    Font font(recorder); font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), video,
        original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    const auto texts = [&] {
        recorder.draws.clear(); menu.render();
        std::string result;
        for (const auto &draw: recorder.draws) {
            const auto found = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                [&](const auto &entry) { return entry.texture == draw.material.getTexture(); });
            if (found != font.fontCache.entryList.end()) result += found->text + "\n";
        }
        const auto origin = recorder.getViewMatrix() * Vector(0, 0);
        D6R_REQUIRE_EQ(0.0f, origin.x); D6R_REQUIRE_EQ(0.0f, origin.y);
        return result;
    };
    auto entry = texts();
    D6R_REQUIRE(entry.find("Host") < entry.find("Browse sessions"));
    D6R_REQUIRE(entry.find("Browse sessions") < entry.find("Direct connect"));
    menu.focus = 1; menu.activate();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    D6R_REQUIRE_EQ(3, menu.focus);
    std::promise<Client::DirectoryPage> response;
    menu.browser.pending = response.get_future();
    D6R_REQUIRE(texts().find("Loading sessions...") != std::string::npos);
    D6R_REQUIRE(!menu.browserFocusEnabled(2));
    response.set_value({true, {}, {}}); menu.browser.update();
    D6R_REQUIRE(texts().find("No active sessions listed") != std::string::npos);
    Client::DirectoryListing row;
    row.id = std::string(32, 'a'); row.sessionId = std::string(32, 'b');
    row.endpoint = {"127.0.0.1", 25000}; row.phase = "first-round"; row.mode = "predator";
    row.players = 2; row.capacity = 15; row.passwordRequired = true;
    row.expiresAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    menu.browser.page = {true, {row}, {}}; menu.selectedListing = row.id;
    auto results = texts();
    D6R_REQUIRE(results.find("BROWSE SESSIONS") != std::string::npos);
    D6R_REQUIRE(results.find("Same-machine endpoint") != std::string::npos);
    menu.focus = 1; menu.activate();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Join);
    D6R_REQUIRE_EQ(2, menu.focus);
    menu.runtime.current.journey = Client::NetworkJourney::Failure;
    menu.runtime.current.failure = "Connection not authorized.";
    menu.update(0);
    std::string reason;
    D6R_REQUIRE_EQ(menu.retryEligible(menu.runtime.current, reason) ? 1 : 0, menu.focus);
    auto controls = PlayerControls::keyboardControls("K1: Arrows", application.input,
        SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
    menu.localPlayers = {{"Guest", controls.get(), "K1: Arrows"}};
    const auto originalAddress = menu.address, originalPort = menu.port;
    menu.activate(); D6R_REQUIRE_EQ(2, menu.focus);
    menu.update(0); // The normal next frame must not overwrite the edit destination.
    D6R_REQUIRE_EQ(2, menu.focus);
    menu.textInputEvent(TextInputEvent("corrected-password"));
    menu.update(0);
    D6R_REQUIRE_EQ(std::string("corrected-password"), menu.password);
    D6R_REQUIRE_EQ(originalAddress, menu.address); D6R_REQUIRE_EQ(originalPort, menu.port);
    D6R_REQUIRE_EQ(std::size_t(1), menu.localPlayers.size());
    D6R_REQUIRE_EQ(std::string("Guest"), menu.localPlayers.front().name);
    D6R_REQUIRE(menu.localPlayers.front().controls == controls.get());
    D6R_REQUIRE(menu.browserSelection && menu.browserSelection->id == row.id);
    D6R_REQUIRE(texts().find("Password required") != std::string::npos);
    menu.back(); menu.browser.page.available = false;
    D6R_REQUIRE(texts().find("Directory unavailable") != std::string::npos);
    menu.focus = 3; menu.activate();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Join);
    D6R_REQUIRE(!menu.browserSelection);
}

D6R_TEST_CASE("UX-NET native frames preserve scaled press activation fields selection and modal protection") {
    char name[] = "duel6r-network-style-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot; std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recording = std::make_unique<Test::RecordingRenderer>();
    auto &recorder = *recording; video.renderer = std::move(recording);
    Font font(recorder); font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), video,
        original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    const auto clear = [&] {
        recorder.quads.clear(); recorder.draws.clear(); recorder.frames.clear(); recorder.lines.clear();
    };
    const auto fill = [&](int x, int y, int w, int h, Color color) {
        return std::any_of(recorder.quads.begin(), recorder.quads.end(), [&](const auto &q) {
            return q.material.getTexture() == Texture{} && q.material.getColor() == color
                && q.vertices[0].x == x && q.vertices[0].y == y
                && q.vertices[2].x == x + w && q.vertices[2].y == y + h;
        });
    };
    const auto textPosition = [&](const std::string &text) {
        for (const auto &q: recorder.quads) {
            const auto found = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                [&](const auto &entry) { return entry.texture == q.material.getTexture() && entry.text == text; });
            if (found != font.fontCache.entryList.end()) return q.vertices[0];
        }
        Test::fail("caption rendered", __FILE__, __LINE__, text); return Vector();
    };
    const auto click = [&](int x, int y, bool pressed) {
        const auto &s = video.getScreen();
        const float scale = std::min(1.35f, std::min(float(s.getClientWidth()) / 850, float(s.getClientHeight()) / 700));
        const int tx = (s.getClientWidth() - int(850 * scale)) / 2;
        const int ty = (s.getClientHeight() - int(700 * scale)) / 2;
        menu.mouseButtonEvent(MouseButtonEvent(tx + int(x * scale), ty + int(y * scale),
            SysEvent::MouseButton::LEFT, pressed ? SysEvent::ButtonState::PRESSED : SysEvent::ButtonState::RELEASED, false));
    };
    // Recording geometry checks cover the floor, modern minimum and scale cap;
    // these are not raster screenshots or physical-display claims.
    for (const auto size: {std::pair{850, 700}, std::pair{1280, 720}, std::pair{1920, 1080}}) {
        video.screen = ScreenParameters(size.first, size.second, 24, 24, 0, false);
        menu.runtime.current = {}; menu.setupScreen = NetworkMenu::SetupScreen::Entry; menu.focus = 0;
        menu.pointerHeld = false;
        clear(); menu.render();
        D6R_REQUIRE(fill(26, 548, 798, 18, Color(0, 0, 200)));
        for (int i = 0; i < 4; ++i) D6R_REQUIRE(fill(275, 325 - i * 45, 300, 32, Color(192)));
        D6R_REQUIRE(!recorder.lines.empty());
        D6R_REQUIRE(recorder.lines.front().color == Color(235));
        click(274, 336, true); // Immediately outside the displayed control.
        D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Entry);
        click(425, 336, false); // Release alone never activates.
        D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Entry);
        click(425, 336, true);
        D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Host); // On press, not release.
        clear(); menu.render();
        D6R_REQUIRE(fill(224, 494, 586, 24, Color::WHITE));
        D6R_REQUIRE(fill(224, 462, 586, 24, Color::WHITE));
        D6R_REQUIRE(fill(224, 430, 586, 24, Color::WHITE));
        D6R_REQUIRE(fill(48, 206, 354, 176, Color::WHITE));
        D6R_REQUIRE(fill(428, 206, 394, 176, Color::WHITE));
        click(223, 440, true); D6R_REQUIRE_EQ(0, menu.focus);
        click(225, 440, true); D6R_REQUIRE_EQ(2, menu.focus);
        menu.textInputEvent(TextInputEvent("disposable"));
        clear(); menu.render();
        textPosition("**********");
        D6R_REQUIRE(fill(275, 82, 300, 32, Color(192))); // Disabled Start still has a surface and boundary.
        D6R_REQUIRE(std::any_of(recorder.frames.begin(), recorder.frames.end(), [](const auto &frame) {
            return frame.position.x == 275 && frame.position.y == 82 && frame.width == 1;
        }));
        menu.password.clear();
    }
    menu.pointerHeld = false; menu.focus = 0;
    clear(); menu.drawButton(100, 100, 160, 32, "Action", true);
    const auto normal = textPosition("Action");
    application.input.setPressed(SDLK_RETURN, true);
    clear(); menu.drawButton(100, 100, 160, 32, "Action", true);
    const auto pressed = textPosition("Action");
    D6R_REQUIRE_EQ(normal.x + 1, pressed.x); D6R_REQUIRE_EQ(normal.y - 1, pressed.y);
    D6R_REQUIRE(recorder.lines.front().color == Color::BLACK);
    application.input.setPressed(SDLK_RETURN, false);
    menu.setupScreen = NetworkMenu::SetupScreen::Browser;
    Client::DirectoryListing row;
    row.id = std::string(32, 'a'); row.sessionId = std::string(32, 'b'); row.endpoint = {"127.0.0.1", 26660};
    row.phase = "lobby"; row.mode = "deathmatch"; row.players = 1; row.capacity = 15;
    row.expiresAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    menu.browser.page = {true, {row}, {}}; menu.browser.received = std::chrono::steady_clock::now();
    menu.selectedListing = row.id; menu.focus = 0;
    clear(); menu.render();
    D6R_REQUIRE(fill(30, 466, 790, 22, Color(0, 0, 200)));
    D6R_REQUIRE(fill(28, 490, 794, 20, Color(170)));
    D6R_REQUIRE(std::any_of(recorder.frames.begin(), recorder.frames.end(), [](const auto &f) {
        return f.position.x == 28 && f.position.y == 464 && f.width == 2;
    }));
    menu.browser.received -= std::chrono::seconds(31);
    click(100, 80, true); D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    menu.runtime.current.journey = Client::NetworkJourney::Match;
    menu.runtime.current.canonical = canonical(91, Network::Replication::Phase::ActiveRound);
    menu.showConfirmation(NetworkMenu::Confirmation::Leave);
    application.input.setPressed(SDLK_RETURN, true);
    clear(); menu.drawConfirmation();
    D6R_REQUIRE(!menu.confirmationInputArmed);
    menu.keyEvent(KeyPressEvent(SDLK_RETURN, SysEvent::ButtonState::PRESSED, 0));
    D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::Leave);
    D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
    application.input.setPressed(SDLK_RETURN, false);
    menu.back(); D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
    D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
}

D6R_TEST_CASE("UX-NET round result wraps complete outcomes above countdown and keeps ranking scroll reachable") {
    char name[] = "duel6r-round-style-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot; std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recording = std::make_unique<Test::RecordingRenderer>();
    auto &recorder = *recording; video.renderer = std::move(recording);
    Font font(recorder); font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), video,
        original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    auto state = canonical(91, Network::Replication::Phase::RoundSummary);
    state.players.clear(); state.score.ranking.clear(); state.score.winner = {};
    state.settings.roundLimit = 3; state.currentRoundNumber = 1;
    state.completedRounds = 0; state.roundEndCountdown = 240;
    for (unsigned i = 0; i < 15; ++i) {
        Network::Replication::PlayerState player{};
        player.playerId = i + 1; player.displayName = std::string(63, char('A' + i)) + char('a' + i);
        state.players.push_back(player); state.score.ranking.push_back(player.playerId);
        if (i < 14) state.score.winner.winnerPlayerIds.push_back(player.playerId);
    }
    menu.runtime.current.journey = Client::NetworkJourney::Match;
    menu.runtime.current.canonical = state;
    video.screen = ScreenParameters(1280, 720, 24, 24, 0, false);
    std::set<std::string> rankings;
    for (int step = 0; step < 20; ++step) {
        recorder.quads.clear(); menu.drawRoundSummary(menu.runtime.snapshot(), 1280, 720);
        std::string outcomeText; float lowestOutcome = 720, phaseTop = 0;
        for (const auto &q: recorder.quads) {
            const auto entry = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                [&](const auto &item) { return item.texture == q.material.getTexture(); });
            if (entry == font.fontCache.entryList.end()) continue;
            if (entry->text.find("Round outcome:") == 0 || entry->text.size() >= 63) {
                outcomeText += entry->text; lowestOutcome = std::min(lowestOutcome, q.vertices[0].y);
            }
            if (entry->text.find("Round frozen") == 0) phaseTop = q.vertices[2].y;
            for (unsigned i = 0; i < 15; ++i)
                if (entry->text.find(std::to_string(i + 1) + ". ") == 0) rankings.insert(std::to_string(i + 1));
        }
        D6R_REQUIRE(phaseTop > 0 && phaseTop < lowestOutcome);
        for (unsigned i = 0; i < 14; ++i) D6R_REQUIRE(outcomeText.find(state.players[i].displayName) != std::string::npos);
        menu.mouseWheelEvent(MouseWheelEvent(0, 0, 0, -1));
    }
    D6R_REQUIRE(menu.rankingScroll > 0);
    D6R_REQUIRE_EQ(std::size_t(15), rankings.size());
}

namespace {
    // Draw-submission/input diagnostics, not native capture or live admission evidence.
    struct ReviewMenuFixture {
        char name[32] = "network-review-tests";
        char *arguments[1] = {name};
        Application application{1, arguments};
        Video &video = application.service->getVideo();
        struct RestoreRenderer {
            std::unique_ptr<Renderer> &slot; std::unique_ptr<Renderer> original;
            ~RestoreRenderer() { slot = std::move(original); }
        } restore{video.renderer, std::move(video.renderer)};
        static Test::RecordingRenderer &install(Video &video) {
            auto renderer = std::make_unique<Test::RecordingRenderer>();
            auto &result = *renderer; video.renderer = std::move(renderer); return result;
        }
        Test::RecordingRenderer &recorder = install(video);
        Font font{recorder};
        AppService &original = *application.service;
        AppService service{font, original.getConsole(), original.getTextureManager(), video,
            original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager()};
        NetworkMenu menu{service, application.gameResources, {}, [] {}};
        ReviewMenuFixture() {
            font.load("data/font.ttf", application.console);
            video.screen = ScreenParameters(850, 700, 24, 24, 0, false);
            const auto &controls = service.getControlsManager().get(0);
            menu.localPlayers = {{"Ada", &controls, controls.getDescription()}};
            menu.runtime.players = menu.localPlayers; menu.runtime.sampledActions.resize(1);
        }
        void draw() {
            recorder.frames.clear(); recorder.lines.clear(); recorder.draws.clear(); recorder.quads.clear();
            menu.render();
        }
        bool text(const std::string &value) const {
            return std::any_of(recorder.draws.begin(), recorder.draws.end(), [&](const auto &draw) {
                return std::any_of(font.fontCache.entryList.begin(), font.fontCache.entryList.end(), [&](const auto &entry) {
                    return entry.texture == draw.material.getTexture() && entry.text == value;
                });
            });
        }
        bool frame(int x, int y, int width, int height, int thickness) const {
            return std::any_of(recorder.frames.begin(), recorder.frames.end(), [&](const auto &f) {
                return f.position.x == x && f.position.y == y && f.size.x == width && f.size.y == height
                    && f.width == thickness && f.color == Color::BLACK;
            });
        }
        void key(SDL_Keycode key) { menu.keyEvent(KeyPressEvent(key, SysEvent::ButtonState::PRESSED, 0)); }
        void click(int x, int y, bool down = true) {
            menu.mouseButtonEvent(MouseButtonEvent(x, y, SysEvent::MouseButton::LEFT,
                down ? SysEvent::ButtonState::PRESSED : SysEvent::ButtonState::RELEASED, false));
        }
        void lobby(bool host = true, bool retained = false) {
            auto state = canonical(91, Network::Replication::Phase::Lobby);
            state.hostParticipantId = 1; state.settings.mode = "Deathmatch";
            state.participants = {{1, true, Network::Replication::ConnectionState::Connected, true, {101}},
                                  {2, false, Network::Replication::ConnectionState::Connected, false, {102}}};
            for (unsigned i = 0; i < 2; ++i) {
                Network::Replication::PlayerState player;
                player.playerId = 101 + i; player.ownerParticipantId = 1 + i;
                player.rosterPosition = i; player.displayName = i ? "Lin" : "Ada";
                state.players.push_back(player);
            }
            state.result.available = retained; state.result.state = retained ? "Completed" : "";
            menu.runtime.current.host = host; menu.runtime.current.localParticipantId = host ? 1 : 2;
            menu.runtime.current.journey = Client::NetworkJourney::Lobby; menu.runtime.current.canonical = state;
            menu.lastJourney = menu.lastStableJourney = Client::NetworkJourney::Lobby;
            menu.focus = 0;
        }
    };

    struct ReviewController {
        ReviewMenuFixture &fixture;
        int device;
        SDL_Joystick *joystick;
        explicit ReviewController(ReviewMenuFixture &fixture) : fixture(fixture) {
            SDL_VirtualJoystickDesc descriptor{};
            descriptor.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
            descriptor.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
            descriptor.naxes = SDL_CONTROLLER_AXIS_MAX; descriptor.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
            descriptor.name = "Review controller";
            device = SDL_JoystickAttachVirtualEx(&descriptor); D6R_REQUIRE(device >= 0);
            joystick = SDL_JoystickOpen(device); D6R_REQUIRE(joystick != nullptr);
            fixture.application.processEvents(fixture.menu); fixture.menu.update(0);
            D6R_REQUIRE(!fixture.application.input.getJoys().empty());
        }
        ~ReviewController() { SDL_JoystickClose(joystick); SDL_JoystickDetachVirtual(device); }
        void pulse(SDL_GameControllerButton button) {
            for (bool down: {true, false}) {
                D6R_REQUIRE_EQ(0, SDL_JoystickSetVirtualButton(joystick, button, down));
                SDL_JoystickUpdate(); fixture.application.processEvents(fixture.menu); fixture.menu.update(0);
            }
        }
    };
}

D6R_TEST_CASE("UX-NET geometry keeps endpoint gaps external password help and Ready hit bounds aligned") {
    ReviewMenuFixture f;
    auto &menu = f.menu;
    const auto textPosition = [&](const std::string &text) {
        for (const auto &quad: f.recorder.quads) {
            const auto entry = std::find_if(f.font.fontCache.entryList.begin(), f.font.fontCache.entryList.end(),
                [&](const auto &entry) { return entry.texture == quad.material.getTexture() && entry.text == text; });
            if (entry != f.font.fontCache.entryList.end()) return quad.vertices[0];
        }
        Test::fail("text rendered", __FILE__, __LINE__, text); return Vector();
    };
    const auto fill = [&](int x, int y, int w, int h, Color color) {
        return std::any_of(f.recorder.quads.begin(), f.recorder.quads.end(), [&](const auto &quad) {
            return quad.material.getTexture() == Texture{} && quad.material.getColor() == color
                && quad.vertices[0].x == x && quad.vertices[0].y == y
                && quad.vertices[2].x == x+w && quad.vertices[2].y == y+h;
        });
    };
    const auto click = [&](int x, int y, bool down = true) {
        const auto &screen = f.video.getScreen();
        const float scale = std::min(1.35f, std::min(float(screen.getClientWidth())/850, float(screen.getClientHeight())/700));
        const int tx = (screen.getClientWidth()-int(850*scale))/2;
        const int ty = (screen.getClientHeight()-int(700*scale))/2;
        menu.mouseButtonEvent(MouseButtonEvent(tx+int(x*scale), ty+int(y*scale), SysEvent::MouseButton::LEFT,
            down ? SysEvent::ButtonState::PRESSED : SysEvent::ButtonState::RELEASED, false));
    };
    for (const auto &size: {std::pair{850,700}, std::pair{1280,720}, std::pair{1920,1080}}) {
        f.video.screen = ScreenParameters(size.first,size.second,24,24,0,false);
        for (auto setup: {NetworkMenu::SetupScreen::Host, NetworkMenu::SetupScreen::Join}) {
            menu.runtime.current = {}; menu.setupScreen = setup;
            menu.hostAddress = menu.address = "127.0.0.1"; menu.port = "26660";
            menu.hostAddresses.clear(); menu.password = "masked";
            menu.browserSelection = Client::DirectoryListing{};
            menu.browserSelection->passwordRequired = true;
            f.draw();
            for (int y: {494,462,430}) D6R_REQUIRE(fill(224,y,586,24,Color::WHITE));
            D6R_REQUIRE_EQ(8,494-(462+24)); D6R_REQUIRE_EQ(8,462-(430+24));
            D6R_REQUIRE(f.text("******"));
            D6R_REQUIRE_EQ(setup == NetworkMenu::SetupScreen::Join, f.text("Password required"));
            if (setup == NetworkMenu::SetupScreen::Join) {
                const auto cue = textPosition("Password required");
                D6R_REQUIRE_EQ(600.0f,cue.x); D6R_REQUIRE(cue.y+16 <= 430);
                D6R_REQUIRE(cue.x+f.font.getTextWidth("Password required",16) < 810);
                menu.browserSelection->passwordRequired = false; f.draw();
                D6R_REQUIRE(!f.text("Password required")); // Direct/optional setup is unchanged.
            }
            menu.focus = 2; click(300,506,false); D6R_REQUIRE_EQ(2,menu.focus);
            click(300,506); D6R_REQUIRE_EQ(0,menu.focus);
            click(300,474); D6R_REQUIRE_EQ(1,menu.focus);
            click(300,442); D6R_REQUIRE_EQ(2,menu.focus);
            click(300,490); D6R_REQUIRE_EQ(2,menu.focus); // Eight-pixel gaps are not controls.
            click(300,458); D6R_REQUIRE_EQ(2,menu.focus);
            if (setup == NetworkMenu::SetupScreen::Host) {
                menu.hostAddresses = {"127.0.0.1","192.168.0.2"}; menu.hostAddressHighlight = 0;
                menu.focus = 1; f.key(SDLK_RETURN); D6R_REQUIRE(menu.hostAddressSelectorOpen);
                f.draw(); D6R_REQUIRE(fill(224,438,586,22,Color::WHITE));
                click(300,426); D6R_REQUIRE(!menu.hostAddressSelectorOpen);
                D6R_REQUIRE_EQ(std::string("192.168.0.2"),menu.hostAddress);
                D6R_REQUIRE_EQ(1,menu.focus);
            }
        }
        f.lobby(false); menu.runtime.pendingGuestCommands.clear(); f.draw();
        const auto order = textPosition("Order");
        D6R_REQUIRE_EQ(520.0f,order.x);
        D6R_REQUIRE(order.x+f.font.getTextWidth("Order",16) <= 564-4); // Inner heading right padding.
        D6R_REQUIRE(fill(40,164,220,24,Color(192)));
        D6R_REQUIRE_EQ(8,196-(164+24)); // Panel, not only inset body, clears Ready.
        click(150,190); D6R_REQUIRE_EQ(0,menu.focus); D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
        click(150,166); D6R_REQUIRE_EQ(2,menu.focus);
        D6R_REQUIRE_EQ(std::size_t(1),menu.runtime.pendingGuestCommands.size());
        click(150,166,false); D6R_REQUIRE_EQ(std::size_t(1),menu.runtime.pendingGuestCommands.size());
        const auto ready = Network::Lifecycle::deserializeParticipantAction(menu.runtime.pendingGuestCommands.front());
        D6R_REQUIRE(ready && ready->kind == Network::Lifecycle::ParticipantActionKind::Ready);
        f.lobby(false,true); f.draw(); D6R_REQUIRE(fill(40,54,220,24,Color(192))); // Accepted retained layout unchanged.
    }
}

D6R_TEST_CASE("UX-NET round progress uses current authoritative round during active and frozen end delay") {
    ReviewMenuFixture f;
    auto &menu = f.menu;
    struct Case { unsigned current, completed, limit, countdown; };
    for (const auto &test: {Case{1,0,2,360},Case{1,0,2,240},Case{2,1,3,360},Case{2,1,3,240},Case{2,1,2,1}}) {
        auto state = canonical(91,Network::Replication::Phase::RoundSummary);
        state.currentRoundNumber = test.current; state.completedRounds = test.completed;
        state.settings.roundLimit = test.limit; state.roundEndCountdown = test.countdown;
        state.score.winner.noWinner = true;
        menu.runtime.current.journey = Client::NetworkJourney::Match;
        menu.runtime.current.canonical = state;
        f.draw(); // Real drawMatch dispatch, including its top-HUD suppression.
        const std::string expected = "Rounds: "+std::to_string(test.current)+"|"+std::to_string(test.limit);
        std::size_t progress = 0, topHudProgress = 0;
        for (const auto &draw: f.recorder.draws) {
            const auto entry = std::find_if(f.font.fontCache.entryList.begin(),f.font.fontCache.entryList.end(),
                [&](const auto &entry) { return entry.texture == draw.material.getTexture(); });
            if (entry == f.font.fontCache.entryList.end()) continue;
            if (entry->text.find("Rounds: ") == 0) { ++progress; D6R_REQUIRE_EQ(expected,entry->text); }
            if (entry->text.find("Round "+std::to_string(test.current)+"/") == 0) ++topHudProgress;
        }
        D6R_REQUIRE_EQ(std::size_t(1),progress); D6R_REQUIRE_EQ(std::size_t(0),topHudProgress);
        D6R_REQUIRE_EQ(test.current,unsigned(menu.runtime.current.canonical->currentRoundNumber));
        D6R_REQUIRE_EQ(test.completed,unsigned(menu.runtime.current.canonical->completedRounds));
        D6R_REQUIRE_EQ(test.countdown,unsigned(menu.runtime.current.canonical->roundEndCountdown));
        D6R_REQUIRE(f.text(test.countdown > 300 ? "World active • 1s"
            : "Round frozen • Next round in "+std::to_string((test.countdown+59)/60)+"s"));
    }
    // Other result consumers retain completed-round semantics, not the new numerator.
    auto state = canonical(91,Network::Replication::Phase::FinalSummary);
    state.currentRoundNumber = 2; state.completedRounds = 1; state.settings.roundLimit = 3;
    state.round = Network::Replication::RoundState{}; state.result.available = true;
    for (auto phase: {Network::Replication::Phase::FinalSummary, Network::Replication::Phase::Lobby,
                     Network::Replication::Phase::ActiveRound}) {
        state.phase = phase; menu.runtime.current.canonical = state;
        menu.runtime.current.journey = phase == Network::Replication::Phase::FinalSummary ? Client::NetworkJourney::Summary
            : phase == Network::Replication::Phase::Lobby ? Client::NetworkJourney::Lobby : Client::NetworkJourney::Match;
        menu.scoreOverlay = phase == Network::Replication::Phase::ActiveRound;
        f.draw();
        D6R_REQUIRE(f.text("Last completed round 1: Pending"));
        D6R_REQUIRE(!f.text("Rounds: 2|3"));
    }
    state.phase = Network::Replication::Phase::RoundSummary; state.settings.roundLimit = 0;
    menu.runtime.current.canonical = state; menu.scoreOverlay = false; f.draw();
    D6R_REQUIRE(!f.text("Rounds: 2|0"));
}

D6R_TEST_CASE("UX-NET review disabled focused setup start and missing-controller Ready stay visible and blocked") {
    ReviewMenuFixture f;
    ReviewController controller(f);
    auto &menu = f.menu;
    for (auto screen: {NetworkMenu::SetupScreen::Host, NetworkMenu::SetupScreen::Join}) {
        menu.setupScreen = screen; menu.port.clear(); menu.focus = 4;
        controller.pulse(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        D6R_REQUIRE_EQ(5, menu.focus); // Baseline traversal still reaches disabled Start/Connect.
        f.draw(); D6R_REQUIRE(f.frame(275, 82, 300, 32, 1)); D6R_REQUIRE(f.frame(273, 80, 304, 36, 2));
        f.key(SDLK_RETURN); f.key(SDLK_SPACE); f.click(425, 98);
        controller.pulse(SDL_CONTROLLER_BUTTON_A);
        D6R_REQUIRE(menu.runtime.snapshot().journey == Client::NetworkJourney::Inactive);
        D6R_REQUIRE_EQ(5, menu.focus); D6R_REQUIRE(!menu.runtime.supervisor);
        f.key(SDLK_UP); D6R_REQUIRE_EQ(4, menu.focus);
        f.key(SDLK_TAB); D6R_REQUIRE_EQ(5, menu.focus);
    }
    f.lobby(); menu.runtime.supervisor = std::make_unique<Client::HostServiceSupervisor>();
    menu.focus = 13; f.key(SDLK_TAB); D6R_REQUIRE_EQ(14, menu.focus);
    f.draw(); D6R_REQUIRE(f.frame(408, 54, 210, 24, 1)); D6R_REQUIRE(f.frame(406, 52, 214, 28, 2));
    D6R_REQUIRE(f.text("Waiting for Guest 2 to be ready."));
    f.key(SDLK_RETURN); f.key(SDLK_SPACE); f.click(500, 66); controller.pulse(SDL_CONTROLLER_BUTTON_A);
    D6R_REQUIRE(menu.runtime.pendingHostCommands.empty());
    D6R_REQUIRE(menu.runtime.snapshot().journey == Client::NetworkJourney::Lobby);

    // Model a removed binding absent from the current control registry, through
    // the existing removal/rescan hook. This is not physical hotplug evidence.
    f.lobby(false); menu.runtime.supervisor.reset(); menu.focus = 2;
    menu.localPlayers[0].controlDescription = "Removed controller binding";
    menu.joyDeviceRemovedEvent(JoyDeviceRemovedEvent(-1));
    D6R_REQUIRE(menu.localPlayers[0].controls == nullptr); D6R_REQUIRE_EQ(2, menu.focus);
    f.draw(); D6R_REQUIRE(f.frame(40, 164, 220, 24, 1)); D6R_REQUIRE(f.frame(38, 162, 224, 28, 2));
    f.key(SDLK_RETURN); f.key(SDLK_SPACE); f.click(150, 180); controller.pulse(SDL_CONTROLLER_BUTTON_A);
    D6R_REQUIRE_EQ(2, menu.focus); D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
    D6R_REQUIRE(!menu.runtime.current.canonical->participants[1].ready);
}

D6R_TEST_CASE("UX-NET review retained host reorder help is inert until baseline keyboard or controller focus") {
    ReviewMenuFixture f;
    ReviewController controller(f);
    auto &menu = f.menu;
    f.lobby(true, true); menu.runtime.supervisor = std::make_unique<Client::HostServiceSupervisor>();
    const std::string help = "Reorder: Tab/↑↓/pad ↑↓; Enter/Space/Confirm";
    const auto unchanged = lobbyConfigurationFingerprint(menu.runtime.snapshot());
    f.draw(); D6R_REQUIRE(f.text(help)); D6R_REQUIRE(!f.frame(40, 386, 360, 24, 2));
    f.click(100, 398); f.click(100, 398, false);
    D6R_REQUIRE_EQ(0, menu.focus); D6R_REQUIRE(menu.runtime.pendingHostCommands.empty());
    for (int i = 0; i < 12 && menu.focus != 12; ++i) f.key(SDLK_TAB);
    D6R_REQUIRE_EQ(12, menu.focus); f.draw();
    D6R_REQUIRE(f.text("> Reorder 1. Ada")); D6R_REQUIRE(f.frame(40, 386, 360, 24, 2));
    f.key(SDLK_SPACE);
    D6R_REQUIRE_EQ(std::size_t(1), menu.runtime.pendingHostCommands.size());
    D6R_REQUIRE(menu.runtime.pendingHostCommands.front() == Network::HostComposition::serializeRosterMove(101, 1));
    menu.runtime.pendingHostCommands.clear();
    f.key(SDLK_UP); D6R_REQUIRE_EQ(11, menu.focus);
    controller.pulse(SDL_CONTROLLER_BUTTON_DPAD_DOWN); D6R_REQUIRE_EQ(12, menu.focus);
    controller.pulse(SDL_CONTROLLER_BUTTON_A);
    D6R_REQUIRE_EQ(std::size_t(1), menu.runtime.pendingHostCommands.size());
    D6R_REQUIRE(menu.runtime.pendingHostCommands.front() == Network::HostComposition::serializeRosterMove(101, 1));
    menu.runtime.pendingHostCommands.clear();
    f.click(100, 398); D6R_REQUIRE_EQ(std::size_t(1), menu.runtime.pendingHostCommands.size());
    f.click(100, 398, false); D6R_REQUIRE_EQ(std::size_t(1), menu.runtime.pendingHostCommands.size());
    D6R_REQUIRE(menu.runtime.pendingHostCommands.front() == Network::HostComposition::serializeRosterMove(101, 1));
    D6R_REQUIRE_EQ(unchanged, lobbyConfigurationFingerprint(menu.runtime.snapshot()));
    menu.runtime.pendingHostCommands.clear();
    f.lobby(false, true); f.draw(); D6R_REQUIRE(!f.text(help));
    D6R_REQUIRE(f.text("Host settings and authoritative roster order are read-only."));
    f.click(100, 398); D6R_REQUIRE_EQ(0, menu.focus);
    D6R_REQUIRE(menu.runtime.pendingHostCommands.empty());
}

D6R_TEST_CASE("UX-NET review active Tab score is informational while retained and final results retain controls") {
    ReviewMenuFixture f;
    auto &menu = f.menu;
    menu.runtime.current.canonical = canonical(91, Network::Replication::Phase::ActiveRound);
    menu.runtime.current.journey = Client::NetworkJourney::Match;
    menu.lastJourney = menu.lastStableJourney = Client::NetworkJourney::Match;
    menu.scoreOverlay = true;
    // Exercise the actual result drawing without depending on world fixture assets.
    menu.drawResult(*menu.runtime.current.canonical, false);
    D6R_REQUIRE(!f.text("<") && !f.text(">") && !f.text("PgUp/PgDn • ←/→"));
    D6R_REQUIRE(f.text("Columns 0–0/0 • Rows 0–0/0"));
    D6R_REQUIRE(!f.frame(48, 64, 560, 26, 2));
    D6R_REQUIRE(std::none_of(f.recorder.lines.begin(), f.recorder.lines.end(), [](const auto &line) {
        return line.start.y >= 66 && line.start.y <= 88;
    }));
    for (auto key: {SDLK_LEFT, SDLK_RIGHT, SDLK_PAGEUP, SDLK_PAGEDOWN}) f.key(key);
    f.click(62, 76); f.click(586, 76); menu.mouseWheelEvent(MouseWheelEvent(200, 150, 1, -1));
    D6R_REQUIRE_EQ(0, menu.summaryScroll); D6R_REQUIRE_EQ(0, menu.summaryHorizontal);
    D6R_REQUIRE_EQ(0, menu.focus); D6R_REQUIRE_EQ(0u, menu.consumedKeyboardActions);
    f.application.input.setPressed(SDLK_RIGHT, true); menu.update(0);
    D6R_REQUIRE((menu.runtime.sampledActions[0] & Network::Input::MoveRight) != 0);
    D6R_REQUIRE(!menu.runtime.gameplayInputSuppressed);
    f.application.input.setPressed(SDLK_RIGHT, false); menu.update(0);
    f.key(SDLK_TAB); D6R_REQUIRE(!menu.scoreOverlay);
    menu.keyEvent(KeyPressEvent(SDLK_TAB, SysEvent::ButtonState::PRESSED, 0, true));
    D6R_REQUIRE(!menu.scoreOverlay); f.key(SDLK_TAB); D6R_REQUIRE(menu.scoreOverlay);
    for (bool retained: {true, false}) {
        menu.runtime.current.journey = retained ? Client::NetworkJourney::Lobby : Client::NetworkJourney::Summary;
        menu.runtime.current.canonical->phase = retained ? Network::Replication::Phase::Lobby : Network::Replication::Phase::FinalSummary;
        menu.runtime.current.canonical->result.available = true;
        menu.focus = retained ? 4 : 1;
        f.recorder.draws.clear(); f.recorder.frames.clear(); f.recorder.lines.clear();
        menu.drawResult(*menu.runtime.current.canonical, retained);
        D6R_REQUIRE(f.text("<") && f.text(">") && f.text("PgUp/PgDn • ←/→"));
        D6R_REQUIRE(f.frame(48, retained ? 137 : 122, 560, 26, 2));
    }
}

D6R_TEST_CASE("NET-DIR refresh shrink normalizes visible rows selection and input through update and render") {
    char name[] = "duel6r-browser-shrink-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot; std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recording = std::make_unique<Test::RecordingRenderer>();
    auto &recorder = *recording; video.renderer = std::move(recording);
    Font font(recorder); font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), video,
        original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    menu.setupScreen = NetworkMenu::SetupScreen::Browser;
    const auto texts = [&] {
        recorder.draws.clear(); recorder.quads.clear(); menu.render(); std::string result;
        for (const auto &draw: recorder.draws) {
            const auto found = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                [&](const auto &entry) { return entry.texture == draw.material.getTexture(); });
            if (found != font.fontCache.entryList.end()) result += found->text + "\n";
        }
        return result;
    };
    std::vector<Client::DirectoryListing> rows;
    for (int index = 0; index < 25; ++index) {
        Client::DirectoryListing row;
        row.id = std::string(30, 'a') + std::to_string(10 + index); row.sessionId = row.id;
        row.endpoint = {"127.0.0.1", static_cast<std::uint16_t>(25000 + index)};
        row.mode = "deathmatch"; row.phase = "lobby"; row.players = 1; row.capacity = 15;
        row.expiresAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
        rows.push_back(row);
    }
    const auto deliver = [&](Client::DirectoryPage page) {
        std::promise<Client::DirectoryPage> response;
        menu.browser.pending = response.get_future();
        response.set_value(std::move(page)); menu.update(0);
    };
    const auto scrollToLast = [&] {
        menu.browser.requested = std::chrono::steady_clock::now();
        deliver({true, rows, "next-page"}); menu.focus = 0;
        for (int index = 0; index < 25; ++index)
            menu.keyEvent(KeyPressEvent(SDLK_DOWN, SysEvent::ButtonState::PRESSED, 0));
        D6R_REQUIRE_EQ(rows.back().id, menu.selectedListing);
        D6R_REQUIRE(menu.browserScroll > 0);
    };
    const auto requestRefresh = [&](bool automatic) {
        if (automatic) {
            menu.browser.requested -= std::chrono::seconds(21); menu.update(0);
        } else { menu.focus = 2; menu.activate(); }
        D6R_REQUIRE(menu.browser.loading());
    };
    for (bool automatic: {false, true}) {
        for (bool retainSelected: {false, true}) {
            scrollToLast();
            requestRefresh(automatic);
            const auto survivor = retainSelected ? rows.back() : rows.front();
            deliver({true, {survivor}, {}});
            D6R_REQUIRE_EQ(0, menu.browserScroll);
            D6R_REQUIRE_EQ(rows.back().id, menu.selectedListing);
            if (!retainSelected) {
                D6R_REQUIRE(!menu.browserFocusEnabled(1));
                D6R_REQUIRE(texts().find("Session is no longer listed.") != std::string::npos);
            }
            D6R_REQUIRE(texts().find("127.0.0.1:" + std::to_string(survivor.endpoint.port)
                + " deathmatch " + survivor.sessionId.substr(28)) != std::string::npos);
            menu.focus = 0; menu.keyEvent(KeyPressEvent(SDLK_DOWN, SysEvent::ButtonState::PRESSED, 0));
            D6R_REQUIRE_EQ(survivor.id, menu.selectedListing);
            D6R_REQUIRE(menu.browserFocusEnabled(1));
        }
        scrollToLast(); requestRefresh(automatic); deliver({true, {}, {}});
        D6R_REQUIRE_EQ(0, menu.browserScroll); D6R_REQUIRE_EQ(rows.back().id, menu.selectedListing);
        D6R_REQUIRE(texts().find("No active sessions listed") != std::string::npos);
        requestRefresh(automatic); deliver({true, {rows.front()}, {}});
        D6R_REQUIRE(texts().find("127.0.0.1:25000 deathmatch aa10") != std::string::npos);
    }
    scrollToLast();
    auto reordered = rows;
    std::rotate(reordered.begin(), reordered.end() - 1, reordered.end());
    deliver({true, reordered, {}});
    D6R_REQUIRE_EQ(rows.back().id, menu.selectedListing);
    D6R_REQUIRE_EQ(0, menu.browserScroll);
    D6R_REQUIRE(texts().find("127.0.0.1:25024 deathmatch aa34") != std::string::npos);
    scrollToLast(); deliver({}); // Failed refresh retains data but disables joining.
    D6R_REQUIRE_EQ(rows.back().id, menu.selectedListing);
    D6R_REQUIRE(!menu.browserFocusEnabled(1));
    D6R_REQUIRE(texts().find("Directory unavailable") != std::string::npos);
    deliver({true, {rows.front()}, {}});
    const auto &screen = video.getScreen();
    const auto scale = std::min(1.35f, std::min(float(screen.getClientWidth()) / 850, float(screen.getClientHeight()) / 700));
    const auto tx = (screen.getClientWidth() - int(850 * scale)) / 2;
    const auto ty = (screen.getClientHeight() - int(700 * scale)) / 2;
    menu.mouseButtonEvent(MouseButtonEvent(tx + int(100 * scale), ty + int(474 * scale),
        SysEvent::MouseButton::LEFT, SysEvent::ButtonState::PRESSED, false));
    D6R_REQUIRE_EQ(rows.front().id, menu.selectedListing);
    D6R_REQUIRE(menu.browserFocusEnabled(1));
    // Page position is rendered from real cursor navigation, with no invented total.
    // Traverse paging before the footer, skipping disabled Previous on page one.
    scrollToLast();
    auto rendered = texts();
    D6R_REQUIRE(rendered.find("Page 1\n") != std::string::npos);
    D6R_REQUIRE(rendered.find("Previous page") < rendered.find("Join selected"));
    D6R_REQUIRE(rendered.find("Next page") < rendered.find("Join selected"));
    for (int expected: {5, 1, 2, 3, 6, 0}) {
        menu.keyEvent(KeyPressEvent(SDLK_TAB, SysEvent::ButtonState::PRESSED, 0));
        menu.update(0); D6R_REQUIRE_EQ(expected, menu.focus);
    }
    for (int expected: {6, 3, 2, 1, 5, 0}) {
        menu.keyEvent(KeyPressEvent(SDLK_TAB, SysEvent::ButtonState::PRESSED, KMOD_SHIFT));
        menu.update(0); D6R_REQUIRE_EQ(expected, menu.focus);
    }
    // The new pointer regions use the same scaled layout as rendering.
    menu.mouseButtonEvent(MouseButtonEvent(tx + int(600 * scale), ty + int(120 * scale),
        SysEvent::MouseButton::LEFT, SysEvent::ButtonState::PRESSED, false));
    menu.update(0);
    D6R_REQUIRE_EQ(0, menu.browserScroll); D6R_REQUIRE(menu.selectedListing.empty());
    deliver({true, {rows.front()}, "third-page"});
    rendered = texts();
    D6R_REQUIRE(rendered.find("Page 2\n") != std::string::npos);
    D6R_REQUIRE(rendered.find("127.0.0.1:25000") != std::string::npos);
    menu.focus = 0; menu.keyEvent(KeyPressEvent(SDLK_DOWN, SysEvent::ButtonState::PRESSED, 0));
    for (int expected: {4, 5, 1, 2, 3, 6, 0}) {
        menu.keyEvent(KeyPressEvent(SDLK_TAB, SysEvent::ButtonState::PRESSED, 0));
        menu.update(0); D6R_REQUIRE_EQ(expected, menu.focus);
    }
    // Directional/controller traversal shares the same row -> paging -> footer order.
    for (int expected: {4, 5, 1, 2, 3, 6, 0}) {
        menu.moveFocus(1); D6R_REQUIRE_EQ(expected, menu.focus);
    }
    menu.browser.received -= std::chrono::seconds(31);
    for (int expected: {4, 2, 3, 6, 0}) {
        menu.keyEvent(KeyPressEvent(SDLK_TAB, SysEvent::ButtonState::PRESSED, 0));
        D6R_REQUIRE_EQ(expected, menu.focus); // stale Next and Join stay excluded
    }
    menu.mouseButtonEvent(MouseButtonEvent(tx + int(100 * scale), ty + int(120 * scale),
        SysEvent::MouseButton::LEFT, SysEvent::ButtonState::PRESSED, false));
    menu.update(0);
    deliver({true, rows, {}});
    D6R_REQUIRE_EQ(0, menu.browserScroll);
    rendered = texts();
    D6R_REQUIRE(rendered.find("Page 1\n") != std::string::npos);
    D6R_REQUIRE(rendered.find("127.0.0.1:25000") != std::string::npos);
    const auto textBounds = [&](const std::string &prefix) {
        for (const auto &quad: recorder.quads) {
            const auto entry = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                [&](const auto &item) { return item.texture == quad.material.getTexture(); });
            if (entry == font.fontCache.entryList.end() || entry->text.find(prefix) != 0) continue;
            // Renderer::quad submits position/UV pairs; these are the first two
            // position arguments retained by the existing recording helper.
            return std::make_pair(std::min(quad.vertices[0].y, quad.vertices[2].y),
                                  std::max(quad.vertices[0].y, quad.vertices[2].y));
        }
        Duel6::Test::fail("rendered text bounds", __FILE__, __LINE__, prefix);
        return std::make_pair(0.0f, 0.0f);
    };
    D6R_REQUIRE(textBounds("Join selected").second < textBounds("Previous page").first);
    D6R_REQUIRE(textBounds("Join selected").second < textBounds("Next page").first);
    D6R_REQUIRE(textBounds("Page 1").second < textBounds("LAN-first.").first);
    D6R_REQUIRE(textBounds("Back").second < textBounds("Join selected").first);
}

D6R_TEST_CASE("NET-ADM reviewed presenter uses authoritative per-player arrival age for every observer") {
    char name[] = "duel6r-arrival-render-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot; std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recorder = std::make_unique<Test::RecordingRenderer>();
    auto &recording = *recorder; video.renderer = std::move(recorder);
    auto state = canonical(91, Network::Replication::Phase::ActiveRound);
    Network::Replication::PlayerState existing, arrival;
    existing.playerId = 101; arrival.playerId = 103;
    state.players = {existing, arrival}; state.phaseTime = 500;
    Network::Replication::ContinuingEffectState effect;
    effect.effectId = 1; effect.playerId = 103; effect.type = "player-arrival"; effect.remaining = 120;
    state.effects = {effect};
    // Host/existing/new observers all receive the same authoritative effect; no local first-snapshot timer.
    for (int observer = 0; observer < 3; ++observer) {
        CanonicalWorldPresenter presenter(*application.service, application.gameResources);
        presenter.update(0, &state, {});
        recording.points.clear(); presenter.renderPlayerEffects(state, existing, 0, 0);
        D6R_REQUIRE(recording.points.empty());
        presenter.renderPlayerEffects(state, arrival, 0, 0);
        D6R_REQUIRE_EQ(std::size_t(15), recording.points.size());
        const auto initial = recording.points.front().position;
        D6R_REQUIRE(std::abs(std::sqrt(initial.x * initial.x + initial.y * initial.y) - 0.15f) < 0.001f);
    }
    state.effects[0].remaining = 60; state.phaseTime += 60;
    CanonicalWorldPresenter reconnect(*application.service, application.gameResources);
    reconnect.update(0, &state, {});
    recording.points.clear(); reconnect.renderPlayerEffects(state, arrival, 0, 0);
    D6R_REQUIRE_EQ(std::size_t(15), recording.points.size());
    const auto point = recording.points.front().position;
    D6R_REQUIRE(std::abs(std::sqrt(point.x * point.x + point.y * point.y) - 0.525f) < 0.001f);
    state.effects.clear(); state.phaseTime += 60;
    recording.points.clear(); reconnect.renderPlayerEffects(state, arrival, 0, 0);
    D6R_REQUIRE(recording.points.empty());
}

D6R_TEST_CASE("NET-DIR NET-PASS password setup preserves complete UTF8 input and bounded editable correction") {
    char name[] = "duel6r-password-editor-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    menu.setupScreen = NetworkMenu::SetupScreen::Host; menu.focus = 2;
    menu.textInputEvent(TextInputEvent(u8"së"));
    D6R_REQUIRE_EQ(std::string(u8"së"), menu.password);
    menu.keyEvent(KeyPressEvent(SDLK_BACKSPACE, SysEvent::ButtonState::PRESSED, 0));
    D6R_REQUIRE_EQ(std::string("s"), menu.password);
    menu.password = std::string(127, 'x');
    menu.textInputEvent(TextInputEvent(u8"ë"));
    D6R_REQUIRE_EQ(std::size_t(127), menu.password.size());
    menu.password = u8"a password ë";
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Host"};
    setup.password = std::make_shared<Network::SessionPassword>(menu.password);
    const auto decoded = Network::HostComposition::deserialize(Network::HostComposition::serializeSetup(setup));
    D6R_REQUIRE(decoded && decoded->setup && decoded->setup->password);
    D6R_REQUIRE_EQ(menu.password, std::string(decoded->setup->password->value()));
}

D6R_TEST_CASE("NET-DIR browser selection blocks stale full closed and removed sessions") {
    char name[] = "duel6r-browser-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    Client::DirectoryListing row;
    row.id = std::string(32, 'a'); row.sessionId = std::string(32, 'b');
    row.endpoint = {"127.0.0.1", 25000}; row.mode = "predator"; row.phase = "first-round";
    row.players = 2; row.capacity = 15; row.passwordRequired = true;
    row.expiresAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60000;
    menu.browser.page = {true, {row}, {}};
    menu.browser.received = std::chrono::steady_clock::now();
    menu.setupScreen = NetworkMenu::SetupScreen::Browser; menu.selectedListing = row.id;
    menu.browser.page.available = false; menu.joinSelected();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    menu.browser.page.available = true; menu.browser.page.listings[0].players = 15; menu.joinSelected();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    menu.browser.page.listings[0] = row; menu.browser.page.listings[0].phase = "closed"; menu.joinSelected();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    menu.browser.page.listings[0] = row; menu.joinSelected();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Join);
    D6R_REQUIRE_EQ(2, menu.focus);
    D6R_REQUIRE(menu.browserSelection && menu.browserSelection->id == row.id);
    menu.focus = 0; menu.textInputEvent(TextInputEvent("1"));
    D6R_REQUIRE(!menu.browserSelection && menu.joinFromBrowser);
    menu.back(); D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    menu.browser.page.listings.clear(); menu.joinSelected();
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Browser);
    D6R_REQUIRE_EQ(row.id, menu.selectedListing);
}

D6R_TEST_CASE("NET-DIR NET-PASS setup rosters retain scrolling and pointer removal below password help") {
    char name[] = "duel6r-setup-roster-tests"; char *arguments[] = {name};
    Application application(1, arguments);
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    menu.setupScreen = NetworkMenu::SetupScreen::Host;
    for (int index = 0; index < 15; ++index) {
        const auto name = "Player " + std::to_string(index);
        menu.availablePersons.push_back(name); menu.localPlayers.push_back(player(name));
    }
    menu.focus = 3 + 15 + 14 * 2;
    menu.syncSetupScroll(); D6R_REQUIRE_EQ(7, menu.setupPlayersScroll);
    menu.focus = 3 + 14;
    menu.syncSetupScroll(); D6R_REQUIRE_EQ(7, menu.setupPersonsScroll);
    const auto &screen = application.service->getVideo().getScreen();
    const float scale = std::min(1.35f, std::min(float(screen.getClientWidth()) / 850, float(screen.getClientHeight()) / 700));
    const int tx = (screen.getClientWidth() - int(850 * scale)) / 2;
    const int ty = (screen.getClientHeight() - int(700 * scale)) / 2;
    menu.mouseWheelEvent(MouseWheelEvent(tx + int(200 * scale), ty + int(300 * scale), 0, 3));
    D6R_REQUIRE_EQ(4, menu.setupPersonsScroll);
    D6R_REQUIRE_EQ(std::size_t(15), menu.localPlayers.size());
    menu.mouseButtonEvent(MouseButtonEvent(tx + int(760 * scale), ty + int(218 * scale),
        SysEvent::MouseButton::LEFT, SysEvent::ButtonState::PRESSED, false));
    D6R_REQUIRE_EQ(std::size_t(14), menu.localPlayers.size());
    D6R_REQUIRE_EQ(std::size_t(15), menu.availablePersons.size());
    D6R_REQUIRE(menu.runtime.snapshot().journey == Client::NetworkJourney::Inactive);
}

D6R_TEST_CASE("PR83 capture flat bundle menu Host Join Retry and effective level plans start real matches") {
    using namespace std::chrono_literals;
    D6R_REQUIRE(!std::filesystem::exists("resources"));
    D6R_REQUIRE(std::filesystem::is_regular_file("duel6r-server"));
    char name[] = "duel6r-flat-menu-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto k1 = PlayerControls::keyboardControls("K1", application.input,
            SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
    auto k2 = PlayerControls::keyboardControls("K2", application.input,
            SDLK_a, SDLK_d, SDLK_w, SDLK_s, SDLK_q, SDLK_1, SDLK_2);
    NetworkMenu host(*application.service, application.gameResources, {}, [] {});
    NetworkMenu guest(*application.service, application.gameResources, {}, [] {});
    const auto enter = [&](NetworkMenu &menu) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN; event.key.type = event.type; event.key.keysym.sym = SDLK_RETURN;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event)); application.processEvents(menu);
        event.type = SDL_KEYUP; event.key.type = event.type;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event)); application.processEvents(menu);
    };
    const auto pump = [&](auto predicate, std::chrono::milliseconds timeout = 5s) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            host.update(0); guest.update(0);
            if (predicate()) return true;
            std::this_thread::sleep_for(2ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return predicate();
    };
    const auto prepare = [&](NetworkMenu &menu, bool isHost, std::uint16_t port) {
        menu.runtime.reset();
        menu.setupScreen = isHost ? NetworkMenu::SetupScreen::Host : NetworkMenu::SetupScreen::Join;
        menu.hostAddress = menu.address = "127.0.0.1"; menu.port = std::to_string(port);
        menu.localPlayers = {{isHost ? "Host" : "Guest", isHost ? k1.get() : k2.get(), isHost ? "K1" : "K2"}};
        menu.availablePersons = {menu.localPlayers[0].name};
        menu.availableLevels = {"levels/duel_01.json", "levels/duel_02.json"};
        menu.hostSetup = {};
        // MENU-01's normal network handoff retains a fixed-map preference even
        // when Random is effective. The service must normalize that preference.
        menu.hostSetup.levelPlan = "Random level";
        menu.hostSetup.fixedLevel = menu.availableLevels.front();
        menu.lastJourney = Client::NetworkJourney::Inactive;
        menu.confirmation = NetworkMenu::Confirmation::None;
        menu.update(0);
        menu.focus = 6; // Endpoint and password fields, one person, one control/remove pair, then Start/Connect.
    };
    for (unsigned transitions = 0; transitions < 4; ++transitions) {
        const auto port = unusedLoopbackPort();
        prepare(host, true, port); prepare(guest, false, port);
        if (transitions == 0) {
            enter(guest); // Real failed Connect, then Retry after the host exists.
            D6R_REQUIRE(pump([&] { return guest.runtime.snapshot().journey == Client::NetworkJourney::Failure
                                               && guest.runtime.snapshot().retryAllowed; }));
            D6R_REQUIRE_EQ(std::string("Host unreachable."), guest.runtime.snapshot().failure);
            const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
            D6R_REQUIRE(descriptor >= 0);
            struct Close { int descriptor; ~Close() { if (descriptor >= 0) ::close(descriptor); } } close{descriptor};
            sockaddr_in address{}; address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(port);
            D6R_REQUIRE_EQ(0, ::bind(descriptor, reinterpret_cast<sockaddr *>(&address), sizeof(address)));
            D6R_REQUIRE_EQ(0, ::listen(descriptor, 1));
            enter(host); // Real port collision and completed cleanup, not a fabricated Failure snapshot.
            D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Failure
                                               && host.runtime.snapshot().retryAllowed; }));
            D6R_REQUIRE_EQ(std::string("The selected port is unavailable. Choose another port and try again."),
                           host.runtime.snapshot().failure);
            ::close(descriptor); close.descriptor = -1;
            host.focus = 0; enter(host);
        } else enter(host);
        D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Lobby; }, 10s));
        if (transitions == 0) guest.focus = 0; // Retry, rather than re-entering setup.
        enter(guest);
        D6R_REQUIRE(pump([&] { return guest.runtime.snapshot().journey == Client::NetworkJourney::Lobby; }, 10s));
        const auto ready = [&] {
            host.focus = guest.focus = 2; enter(host); enter(guest);
            D6R_REQUIRE(pump([&] { return everyParticipantReady(host.runtime.snapshot(), true)
                                               && everyParticipantReady(guest.runtime.snapshot(), true); }));
        };
        D6R_REQUIRE(host.runtime.snapshot().canonical->settings.fixedLevel.empty());
        D6R_REQUIRE(guest.runtime.snapshot().canonical->settings.fixedLevel.empty());
        D6R_REQUIRE_EQ(std::string("Random level"), guest.runtime.snapshot().canonical->settings.levelPlan);
        std::string expected = "Random level";
        for (unsigned step = 0; step < transitions; ++step) {
            ready();
            host.focus = 6; enter(host); // UI Level plan: Random -> Fixed -> Shuffle -> Random.
            expected = expected == "Random level" ? "Fixed level"
                       : expected == "Fixed level" ? "Shuffle all levels" : "Random level";
            D6R_REQUIRE(pump([&] {
                for (auto *menu : {&host, &guest}) {
                    const auto snap = menu->runtime.snapshot();
                    if (!snap.canonical || snap.canonical->settings.levelPlan != expected
                        || snap.canonical->settings.fixedLevel != (expected == "Fixed level" ? "levels/duel_01.json" : "")
                        || !everyParticipantReady(snap, false)) return false;
                }
                return true;
            }));
        }
        ready();
        const auto oldRounds = host.runtime.snapshot().canonical->settings.roundLimit;
        host.focus = 8; enter(host); // An unrelated edit must still work after normalization.
        D6R_REQUIRE(pump([&] { return guest.runtime.snapshot().canonical->settings.roundLimit == oldRounds + 1
                                           && everyParticipantReady(guest.runtime.snapshot(), false); }));
        ready();
        host.focus = 14; enter(host); // Start with two connected/ready participants.
        D6R_REQUIRE(pump([&] {
            for (auto *menu : {&host, &guest}) {
                const auto snap = menu->runtime.snapshot();
                if (snap.journey != Client::NetworkJourney::Match || !snap.canonical || !snap.canonical->round
                    || snap.canonical->settings.levelPlan != expected
                    || snap.canonical->settings.fixedLevel != (expected == "Fixed level" ? "levels/duel_01.json" : "")
                    || std::find(snap.canonical->settings.levels.begin(), snap.canonical->settings.levels.end(),
                                 snap.canonical->round->level) == snap.canonical->settings.levels.end()) return false;
            }
            return true;
        }, 10s));
        D6R_REQUIRE_EQ(host.runtime.snapshot().canonical->round->level, guest.runtime.snapshot().canonical->round->level);
        if (expected == "Fixed level")
            D6R_REQUIRE_EQ(std::string("levels/duel_01.json"), guest.runtime.snapshot().canonical->round->level);
        host.runtime.endSession();
        D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Inactive
                                           && guest.runtime.snapshot().journey == Client::NetworkJourney::HostEnded; }));
    }
}

D6R_TEST_CASE("PR83 capture Application K2 endpoint text holds do not invoke Back and mapped controls recover") {
    char name[] = "duel6r-endpoint-input-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    auto k2 = PlayerControls::keyboardControls("K2: WSAD", application.input,
            SDLK_a, SDLK_d, SDLK_w, SDLK_s, SDLK_q, SDLK_1, SDLK_2);
    menu.localPlayers = {{"Alice", k2.get(), "K2: WSAD"}};
    menu.availablePersons = {"Alice", "Bob"};
    const auto key = [&](SDL_Keycode code, bool down, bool repeat = false) {
        SDL_Event event{}; event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.type = event.type; event.key.keysym.sym = code; event.key.repeat = repeat;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event)); application.processEvents(menu);
    };
    const auto text = [&](char value) {
        SDL_Event event{}; event.type = SDL_TEXTINPUT; event.text.text[0] = value; event.text.text[1] = 0;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event)); application.processEvents(menu);
    };
    for (const auto screen : {NetworkMenu::SetupScreen::Host, NetworkMenu::SetupScreen::Join}) {
        menu.setupScreen = screen; menu.runtime.current = {}; menu.lastJourney = Client::NetworkJourney::Inactive;
        menu.focus = screen == NetworkMenu::SetupScreen::Host ? 0 : 1;
        menu.port.clear(); menu.update(0);
        for (const char value : std::string("31113")) {
            key(value, true); text(value);
            for (unsigned frame = 0; frame < 8; ++frame) menu.update(1.0f / 60.0f);
            key(value, true, true); menu.update(0);
            D6R_REQUIRE(menu.setupScreen == screen);
            D6R_REQUIRE(menu.runtime.snapshot().journey == Client::NetworkJourney::Inactive);
            D6R_REQUIRE_EQ(screen == NetworkMenu::SetupScreen::Host ? 0 : 1, menu.focus);
            key(value, false); menu.update(0);
        }
        D6R_REQUIRE_EQ(std::string("31113"), menu.port);
        // Holding Back's physical key while Tab moves focus must not acquire
        // navigation meaning on the next poll. Only a fresh press may act.
        key(SDLK_1, true); key(SDLK_TAB, true); key(SDLK_TAB, false); menu.update(0);
        for (unsigned frame = 0; frame < 8; ++frame) menu.update(0);
        D6R_REQUIRE(menu.setupScreen == screen);
        key(SDLK_1, false); menu.update(0);
        if (screen == NetworkMenu::SetupScreen::Host) {
            key(SDLK_TAB, true); key(SDLK_TAB, false); menu.update(0);
        }
        key(SDLK_TAB, true); key(SDLK_TAB, false); menu.update(0); // Leave the new password editor.
        key(SDLK_1, true); menu.update(0);
        D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Entry);
        key(SDLK_1, false); menu.update(0);
    }
    menu.setupScreen = NetworkMenu::SetupScreen::Join; menu.focus = 0; menu.address.clear(); menu.update(0);
    for (char value : std::string("wsq1.example")) {
        key(value, true); text(value); menu.update(0);
        key(value, false); menu.update(0);
    }
    D6R_REQUIRE_EQ(std::string("wsq1.example"), menu.address);
    D6R_REQUIRE_EQ(0, menu.focus);
    D6R_REQUIRE(menu.setupScreen == NetworkMenu::SetupScreen::Join);
    menu.runtime.current.journey = Client::NetworkJourney::Match;
    menu.runtime.current.canonical = canonical(91, Network::Replication::Phase::ActiveRound);
    menu.runtime.players = menu.localPlayers; menu.runtime.sampledActions.resize(1); menu.update(0);
    key(SDLK_q, true); key(SDLK_1, true); menu.update(0);
    D6R_REQUIRE_EQ(Network::Input::Shoot | Network::Input::PickOrSwapWeapon, menu.runtime.sampledActions[0]);
    D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
    key(SDLK_q, false); key(SDLK_1, false); menu.update(0);
    D6R_REQUIRE_EQ(0u, menu.runtime.sampledActions[0]);
}

D6R_TEST_CASE("PR83 latest Application mapped keyboard independent controller and retained last-owned-slot focus") {
    char name[] = "duel6r-pr83-mapped-controls-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recording = std::make_unique<Test::RecordingRenderer>();
    auto &recorder = *recording;
    video.renderer = std::move(recording);
    Font font(recorder);
    font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), video,
            original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    auto k1 = PlayerControls::keyboardControls("K1: Arrows", application.input,
            SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
    auto k2 = PlayerControls::keyboardControls("K2: WSAD", application.input,
            SDLK_a, SDLK_d, SDLK_w, SDLK_s, SDLK_q, SDLK_1, SDLK_2);
    menu.availablePersons = {"Alice", "Bob", "Charlie"};
    menu.localPlayers = {{"Alice", k2.get(), "K2: WSAD"}};
    menu.runtime.players = menu.localPlayers;
    menu.runtime.sampledActions.resize(1);
    menu.runtime.current.journey = Client::NetworkJourney::Lobby;
    menu.runtime.current.localParticipantId = 2;
    menu.runtime.current.canonical = canonical(91, Network::Replication::Phase::Lobby);
    menu.update(0); // Observe the stable lobby before supplying UI edges.
    const auto event = [&](SDL_Keycode code, bool down, bool repeat = false) {
        SDL_Event value{};
        value.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        value.key.type = value.type; value.key.keysym.sym = code; value.key.repeat = repeat;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&value));
        application.processEvents(menu);
    };
    const auto pulse = [&](SDL_Keycode code) {
        event(code, true); menu.update(0);
        event(code, false); menu.update(0);
    };
    // K2 navigation was previously erased by the broad keyboard-handled latch.
    pulse(SDLK_s); D6R_REQUIRE_EQ(1, menu.focus);
    pulse(SDLK_w); D6R_REQUIRE_EQ(0, menu.focus);
    event(SDLK_q, true); menu.update(0);
    D6R_REQUIRE_EQ(std::string("Bob"), menu.localPlayers[0].name);
    event(SDLK_q, true, true); menu.update(0);
    D6R_REQUIRE_EQ(std::string("Bob"), menu.localPlayers[0].name);
    event(SDLK_q, false); menu.update(0);
    pulse(SDLK_1); D6R_REQUIRE_EQ(3, menu.focus); // Leave action, not a session departure.
    D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
    menu.localPlayers[0].controls = k1.get();
    menu.runtime.rebindLocalPlayers(menu.localPlayers);
    menu.focus = 0;
    pulse(SDLK_RCTRL);
    D6R_REQUIRE_EQ(std::string("Charlie"), menu.localPlayers[0].name);
    pulse(SDLK_RSHIFT); D6R_REQUIRE_EQ(3, menu.focus);
    menu.focus = 0;
    pulse(SDLK_DOWN); D6R_REQUIRE_EQ(1, menu.focus); // Direct and mapped handling must not double step.
    pulse(SDLK_UP); D6R_REQUIRE_EQ(0, menu.focus);

    SDL_VirtualJoystickDesc descriptor{};
    descriptor.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    descriptor.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    descriptor.naxes = SDL_CONTROLLER_AXIS_MAX;
    descriptor.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    descriptor.name = "PR83 independent controller";
    const int device = SDL_JoystickAttachVirtualEx(&descriptor);
    D6R_REQUIRE(device >= 0);
    SDL_Joystick *joystick = SDL_JoystickOpen(device);
    struct Detach {
        SDL_Joystick *joystick; int device;
        ~Detach() { if (joystick) SDL_JoystickClose(joystick); SDL_JoystickDetachVirtual(device); }
    } detach{joystick, device};
    D6R_REQUIRE(joystick != nullptr);
    application.processEvents(menu);
    menu.update(0);
    D6R_REQUIRE(!application.input.getJoys().empty());
    const auto button = [&](SDL_GameControllerButton value, bool down) {
        D6R_REQUIRE_EQ(0, SDL_JoystickSetVirtualButton(joystick, value, down));
        SDL_JoystickUpdate();
        application.processEvents(menu);
    };
    menu.focus = 0;
    event(SDLK_z, true); // Unrelated keyboard input must not mask a controller edge.
    button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true); menu.update(0);
    D6R_REQUIRE_EQ(1, menu.focus);
    event(SDLK_z, false); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false); menu.update(0);
    menu.focus = 0;
    event(SDLK_RETURN, true); // Keyboard activates Person; independent controller moves focus.
    button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, true); menu.update(0);
    D6R_REQUIRE_EQ(std::string("Alice"), menu.localPlayers[0].name);
    D6R_REQUIRE_EQ(1, menu.focus);
    event(SDLK_RETURN, false); button(SDL_CONTROLLER_BUTTON_DPAD_DOWN, false); menu.update(0);
    menu.focus = 0;
    event(SDLK_z, true); button(SDL_CONTROLLER_BUTTON_A, true); menu.update(0);
    D6R_REQUIRE_EQ(std::string("Bob"), menu.localPlayers[0].name);
    event(SDLK_z, false); button(SDL_CONTROLLER_BUTTON_A, false); menu.update(0);

    // A retained-result UI fixture keeps fourteen admitted local slots. Check
    // rendered labels and focus rectangles, not just the internal focus index.
    menu.localPlayers.clear();
    for (unsigned index = 0; index < 14; ++index)
        menu.localPlayers.push_back({"Owned " + std::to_string(index + 1), k2.get(), "K2: WSAD"});
    menu.runtime.players = menu.localPlayers;
    menu.runtime.sampledActions.resize(14);
    auto &state = *menu.runtime.current.canonical;
    state.result.available = true; state.result.state = "Completed";
    state.participants = {{2, false, Network::Replication::ConnectionState::Connected, false, {}}};
    state.players.clear();
    for (unsigned index = 0; index < 14; ++index) {
        Network::Replication::PlayerState slot;
        slot.playerId = index + 101; slot.ownerParticipantId = 2;
        slot.rosterPosition = index; slot.displayName = menu.localPlayers[index].name;
        state.players.push_back(slot); state.participants[0].ownedPlayerIds.push_back(slot.playerId);
    }
    const auto visibleTexts = [&] {
        std::vector<std::string> result;
        for (const auto &draw : recorder.draws) {
            const auto found = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                    [&](const auto &entry) { return entry.texture == draw.material.getTexture(); });
            if (found != font.fontCache.entryList.end()) result.push_back(found->text);
        }
        return result;
    };
    for (const bool host : {false, true}) {
        menu.runtime.current.host = host;
        menu.focus = 0;
        for (unsigned index = 0; index < 27; ++index) pulse(SDLK_s);
        D6R_REQUIRE_EQ(27, menu.focus); // Control of the last owned slot.
        pulse(SDLK_w); D6R_REQUIRE_EQ(26, menu.focus);
        recorder.draws.clear(); recorder.frames.clear();
        menu.drawLobby(menu.runtime.snapshot());
        const auto personTexts = visibleTexts();
        D6R_REQUIRE(std::find(personTexts.begin(), personTexts.end(), "Person: Owned 14") != personTexts.end());
        const auto selectedPerson = std::find_if(recorder.frames.begin(), recorder.frames.end(),
                [](const auto &frame) { return frame.width == 2 && frame.size.y < 40; });
        D6R_REQUIRE(selectedPerson != recorder.frames.end());
        const auto personX = selectedPerson->position.x;
        pulse(SDLK_s); D6R_REQUIRE_EQ(27, menu.focus);
        recorder.draws.clear(); recorder.frames.clear();
        menu.drawLobby(menu.runtime.snapshot());
        const auto controlTexts = visibleTexts();
        D6R_REQUIRE(std::find(controlTexts.begin(), controlTexts.end(), "Person: Owned 14") != controlTexts.end());
        D6R_REQUIRE(std::find(controlTexts.begin(), controlTexts.end(), "Control: K2: WSAD") != controlTexts.end());
        const auto selectedControl = std::find_if(recorder.frames.begin(), recorder.frames.end(),
                [](const auto &frame) { return frame.width == 2 && frame.size.y < 40; });
        D6R_REQUIRE(selectedControl != recorder.frames.end());
        D6R_REQUIRE(selectedControl->position.x > personX);
    }
}

D6R_TEST_CASE("PR83 Application Return repeat Tab and modal input isolation for host and guest") {
    char name[] = "duel6r-pr83-input-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    auto controls = PlayerControls::keyboardControls("QA K1", application.input,
            SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
    menu.localPlayers = {{"QA", controls.get(), "QA K1"}};
    menu.runtime.players = menu.localPlayers;
    menu.runtime.sampledActions.resize(1);
    // Use SDL's queue and Application's production dispatcher, not a fabricated
    // context key-up: Application intentionally dispatches only pressed keys.
    const auto key = [&](SDL_Keycode code, bool pressed, bool repeat = false) {
        SDL_Event event{};
        event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.type = event.type;
        event.key.keysym.sym = code;
        event.key.repeat = repeat;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event));
        application.processEvents(menu);
    };
    for (const bool host : {false, true}) {
        menu.runtime.current = {};
        menu.runtime.current.host = host;
        menu.runtime.current.localParticipantId = host ? 1 : 2;
        menu.runtime.current.journey = Client::NetworkJourney::Match;
        menu.runtime.current.canonical = canonical(91, Network::Replication::Phase::ActiveRound);
        menu.confirmation = NetworkMenu::Confirmation::None;
        menu.scoreOverlay = false;
        menu.update(0); // Observe entry before dispatching active-match events.
        key(SDLK_RETURN, true);
        D6R_REQUIRE(controls->getStatus().isPressed());
        for (int repeat = 0; repeat < 4; ++repeat) key(SDLK_RETURN, true, true);
        D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
        D6R_REQUIRE(!menu.runtime.pendingHostEnd);
        D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
        key(SDLK_RETURN, false);
        D6R_REQUIRE(!controls->getStatus().isPressed());
        for (const bool expected : {true, false, true}) {
            key(SDLK_TAB, true);
            D6R_REQUIRE_EQ(expected, menu.scoreOverlay);
            key(SDLK_TAB, true, true);
            D6R_REQUIRE_EQ(expected, menu.scoreOverlay);
            key(SDLK_TAB, false);
            D6R_REQUIRE_EQ(expected, menu.scoreOverlay);
        }
        // A held fire/status key must not gain destructive meaning when Escape
        // explicitly opens a dialog. Releasing all confirms arms a fresh action.
        key(SDLK_RCTRL, true);
        key(SDLK_RETURN, true);
        key(SDLK_ESCAPE, true);
        key(SDLK_ESCAPE, false);
        menu.update(0);
        D6R_REQUIRE(menu.confirmation != NetworkMenu::Confirmation::None);
        D6R_REQUIRE(!menu.confirmationInputArmed);
        D6R_REQUIRE_EQ(0u, menu.runtime.sampledActions[0]);
        key(SDLK_RETURN, true, true);
        D6R_REQUIRE(menu.confirmation != NetworkMenu::Confirmation::None);
        key(SDLK_RETURN, false);
        key(SDLK_RCTRL, false);
        menu.update(0);
        D6R_REQUIRE(menu.confirmationInputArmed);
        key(SDLK_ESCAPE, true);
        key(SDLK_ESCAPE, false);
        D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
        D6R_REQUIRE(menu.runtime.pendingGuestCommands.empty());
        key(SDLK_ESCAPE, true);
        key(SDLK_ESCAPE, false);
        menu.update(0);
        D6R_REQUIRE(menu.confirmationInputArmed);
        key(SDLK_RETURN, true);
        key(SDLK_RETURN, false);
        D6R_REQUIRE(menu.confirmation == NetworkMenu::Confirmation::None);
        D6R_REQUIRE(menu.runtime.snapshot().journey == Client::NetworkJourney::Cancelling);
        if (!host) {
            D6R_REQUIRE_EQ(1u, menu.runtime.pendingGuestCommands.size());
            const auto leave = Network::Lifecycle::deserializeParticipantAction(menu.runtime.pendingGuestCommands.front());
            D6R_REQUIRE(leave && leave->kind == Network::Lifecycle::ParticipantActionKind::Leave);
        }
        menu.runtime.pendingGuestCommands.clear();
    }
}

D6R_TEST_CASE("PR83 real runtime End drains update from lobby and active match and releases listener") {
    using namespace std::chrono_literals;
    for (const bool active : {false, true}) {
        Client::NetworkSessionRuntime host, guest, unused;
        const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
        Network::HostComposition::Setup setup;
        setup.localPlayerNames = {"Host"};
        setup.fixedLevel = "levels/duel_01.json";
        D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR,
                                  setup, {player("Host")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
            return host.snapshot().journey == Client::NetworkJourney::Lobby;
        }));
        D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
            return guest.snapshot().journey == Client::NetworkJourney::Lobby;
        }));
        if (active) {
            host.setReady(true); guest.setReady(true);
            D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
                return everyParticipantReady(host.snapshot(), true);
            }));
            host.startMatch();
            D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
                return host.snapshot().journey == Client::NetworkJourney::Match
                       && guest.snapshot().journey == Client::NetworkJourney::Match;
            }));
        }
        host.endSession();
        // This update, not the destructor, exercises the former synchronous
        // observer/mutex deadlock. The enclosing test process timeout is its guard.
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            return host.snapshot().journey == Client::NetworkJourney::Inactive
                   && guest.snapshot().journey == Client::NetworkJourney::HostEnded;
        }));
        D6R_REQUIRE(!host.snapshot().canonical);
        D6R_REQUIRE(host.supervisor->snapshot().cleanupComplete);
        // Restart the real service on the same endpoint, proving listener cleanup.
        host.reset(); guest.reset();
        D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR,
                                  setup, {player("Host")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
            return host.snapshot().journey == Client::NetworkJourney::Lobby;
        }));
        host.endSession();
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            return host.snapshot().journey == Client::NetworkJourney::Inactive;
        }));
    }
}

D6R_TEST_CASE("NET-PASS NET-ADM real protected direct and selected-session joins enforce password and enter live round") {
    using namespace std::chrono_literals;
    for (const std::string mode: {"Deathmatch", "Predator", "Team deathmatch"}) {
        Client::NetworkSessionRuntime host, guest, arrival;
        const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
        Network::HostComposition::Setup setup;
        setup.localPlayerNames = {"Host"}; setup.fixedLevel = "levels/duel_01.json";
        setup.mode = mode; setup.teamCount = mode == "Team deathmatch" ? 2 : 0;
        setup.quickLiquid = false; setup.roundLimit = 2;
        setup.password = std::make_shared<Network::SessionPassword>("test-session-password");
        D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR, setup, {player("Host")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 10s, [&] { return host.snapshot().journey == Client::NetworkJourney::Lobby; }));
        for (const std::string password: {"", "incorrect"}) {
            D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}, std::make_shared<Network::SessionPassword>(password)));
            D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Failure; }));
            D6R_REQUIRE_EQ(std::string("Connection not authorized."), guest.snapshot().failure);
            D6R_REQUIRE_EQ(std::size_t(1), host.snapshot().canonical->players.size());
            guest.reset();
        }
        D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}, setup.password));
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Lobby; }));
        host.setReady(true); guest.setReady(true);
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
        host.startMatch();
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Match; }));
        const auto before = *host.snapshot().canonical;
        D6R_REQUIRE(before.phase == Network::Replication::Phase::ActiveRound && before.round);
        D6R_REQUIRE(arrival.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Arrival one"), player("Arrival two")},
            setup.password, Client::directorySessionId(before.sessionId)));
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 10s, [&] {
            const auto state = arrival.snapshot();
            return state.journey == Client::NetworkJourney::Match && state.canonical && state.canonical->players.size() == 4;
        }));
        const auto after = *arrival.snapshot().canonical;
        D6R_REQUIRE_EQ(before.matchId, after.matchId);
        D6R_REQUIRE(after.round && after.round->roundId == before.round->roundId);
        D6R_REQUIRE(after.phaseTime >= before.phaseTime);
        D6R_REQUIRE_EQ(std::uint8_t(1), after.currentRoundNumber);
        for (const auto &existing: before.players) {
            const auto found = std::find_if(after.players.begin(), after.players.end(), [&](const auto &p) { return p.playerId == existing.playerId; });
            D6R_REQUIRE(found != after.players.end());
            D6R_REQUIRE_EQ(existing.ownerParticipantId, found->ownerParticipantId);
            D6R_REQUIRE_EQ(existing.team, found->team);
            if (mode == "Predator") D6R_REQUIRE_EQ(existing.presentationAlpha, found->presentationAlpha);
        }
        const auto owner = std::find_if(after.participants.begin(), after.participants.end(), [&](const auto &participant) {
            return participant.participantId == arrival.snapshot().localParticipantId;
        });
        D6R_REQUIRE(owner != after.participants.end() && owner->ownedPlayerIds.size() == 2);
        host.endSession();
        D6R_REQUIRE(pumpRuntimes(host, guest, arrival, 5s, [&] { return host.snapshot().journey == Client::NetworkJourney::Inactive; }));
    }
}

D6R_TEST_CASE("NET-PASS real wrong selected session never allocates or admits a player") {
    using namespace std::chrono_literals;
    Client::NetworkSessionRuntime host, guest, unused;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Host"}; setup.fixedLevel = "levels/duel_01.json";
    setup.password = std::make_shared<Network::SessionPassword>("scope-test-password");
    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR, setup, {player("Host")}));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return host.snapshot().journey == Client::NetworkJourney::Lobby; }));
    const auto id = host.snapshot().canonical->sessionId;
    D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}, setup.password,
        Client::directorySessionId(id == 1 ? 2 : id ^ 1)));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Failure; }));
    D6R_REQUIRE_EQ(std::string("Connection not authorized."), guest.snapshot().failure);
    D6R_REQUIRE_EQ(std::size_t(1), host.snapshot().canonical->players.size());
    D6R_REQUIRE_EQ(std::size_t(1), host.snapshot().canonical->participants.size());
    guest.reset();
    D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}, setup.password, Client::directorySessionId(id)));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Lobby; }));
    host.endSession();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return host.snapshot().journey == Client::NetworkJourney::Inactive; }));
}

D6R_TEST_CASE("NET-PASS key lifetime exhaustion restores reserved protected players without respawn") {
    using namespace std::chrono_literals;
    Client::NetworkSessionRuntime host, guest, unused;
    guest.sessionLimits.lifetime = 4s;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Host"}; setup.fixedLevel = "levels/duel_01.json";
    setup.quickLiquid = false; setup.roundLimit = 2;
    setup.password = std::make_shared<Network::SessionPassword>("rotation-fixture");
    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR, setup, {player("Host")}));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return host.snapshot().journey == Client::NetworkJourney::Lobby; }));
    D6R_REQUIRE(guest.join(endpoint, D6R_TEST_RESOURCE_DIR, {player("Guest")}, setup.password));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Lobby; }));
    host.setReady(true); guest.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
    host.startMatch();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Match; }));
    const auto participant = guest.snapshot().localParticipantId;
    const auto before = *guest.snapshot().canonical;
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 8s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Reconnecting; }));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 3s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Match; }));
    const auto after = *guest.snapshot().canonical;
    D6R_REQUIRE_EQ(participant, guest.snapshot().localParticipantId);
    D6R_REQUIRE_EQ(before.sessionId, after.sessionId);
    D6R_REQUIRE_EQ(before.matchId, after.matchId);
    D6R_REQUIRE(before.round && after.round && before.round->roundId == after.round->roundId);
    D6R_REQUIRE(after.phaseTime > before.phaseTime);
    D6R_REQUIRE_EQ(before.players.size(), after.players.size());
    for (std::size_t index = 0; index < before.players.size(); ++index) {
        D6R_REQUIRE_EQ(before.players[index].playerId, after.players[index].playerId);
        D6R_REQUIRE_EQ(before.players[index].ownerParticipantId, after.players[index].ownerParticipantId);
        D6R_REQUIRE(!after.players[index].invulnerable);
    }
    host.endSession();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return host.snapshot().journey == Client::NetworkJourney::Inactive; }));
}

D6R_TEST_CASE("PR83 canonical held weapon draw alpha distinguishes invisibility from Predator") {
    char name[] = "duel6r-pr83-weapon-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recorder = std::make_unique<Test::RecordingRenderer>();
    auto &draws = recorder->draws;
    video.renderer = std::move(recorder);
    CanonicalWorldPresenter presenter(*application.service, application.gameResources);
    Network::Replication::PlayerState player;
    player.heldWeapon = "pistol";
    for (const bool predator : {false, true}) {
        for (const bool invisible : {false, true}) {
            player.presentationAlpha = invisible ? 51 : predator ? 25 : 255;
            player.activeBonus = invisible ? "invisibility" : "";
            player.bonusRemaining = invisible ? 60 : 0;
            draws.clear();
            presenter.renderHeldWeapon(player, 0, 0);
            D6R_REQUIRE_EQ(1u, draws.size());
            D6R_REQUIRE(draws[0].material.getColor() == Color(255, 255, 255, invisible ? 51 : 255));
            D6R_REQUIRE_EQ(!invisible, draws[0].material.isMasked());
            D6R_REQUIRE(draws[0].blend == (invisible ? BlendFunc::SrcAlpha : BlendFunc::None));
        }
    }
    player.bonusRemaining = 0; // Expired bonus name must not retain weapon opacity.
    draws.clear();
    presenter.renderHeldWeapon(player, 0, 0);
    D6R_REQUIRE_EQ(1u, draws.size());
    D6R_REQUIRE(draws[0].material.getColor() == Color::WHITE);
    player.lifeState = Network::Replication::LifeState::Dead;
    draws.clear();
    presenter.renderHeldWeapon(player, 0, 0);
    D6R_REQUIRE(draws.empty());
}

D6R_TEST_CASE("PR83 capture complete presenter draws translucent body and weapon including combined bonus and expiry") {
    char name[] = "duel6r-invisibility-render-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recorder = std::make_unique<Test::RecordingRenderer>();
    auto &draws = recorder->draws;
    video.renderer = std::move(recorder);
    CanonicalWorldPresenter presenter(*application.service, application.gameResources);
    auto state = canonical(91, Network::Replication::Phase::ActiveRound);
    state.matchId = 1; state.settings.levels = {"levels/duel_01.json"};
    state.round = Network::Replication::RoundState{1, 1, "levels/duel_01.json"};
    Network::Replication::PlayerState player;
    player.playerId = 101; player.ownerParticipantId = 1; player.life = 100;
    player.positionX = 2 * 65536; player.positionY = 2 * 65536;
    player.heldWeapon = "pistol"; player.visible = true;
    state.players = {player};
    presenter.setCanonicalLevels(state.settings.levels);
    presenter.update(0, &state, {});
    const auto bodyTexture = presenter.skinFor(player).getTexture();
    const auto weaponTexture = presenter.weaponFor("pistol")->getNetworkWeaponTexture();
    D6R_REQUIRE(bodyTexture && weaponTexture && bodyTexture != weaponTexture);
    for (const bool predator : {false, true}) {
        for (const bool invisible : {false, true, false}) { // Last entry models expired Invisibility.
            auto &slot = state.players.front();
            slot.presentationAlpha = invisible ? 51 : predator ? 25 : 255;
            slot.activeBonus = invisible ? "invisibility" : "";
            slot.bonusRemaining = invisible ? 30 : 0;
            draws.clear();
            D6R_REQUIRE(presenter.render(state, {}, {}, 1280, 900));
            for (const auto &[texture, alpha] : std::vector<std::pair<Texture, std::uint8_t>>{
                    {bodyTexture, slot.presentationAlpha}, {weaponTexture, invisible ? 51 : 255}}) {
                const auto draw = std::find_if(draws.begin(), draws.end(),
                        [texture](const auto &item) { return item.material.getTexture() == texture; });
                D6R_REQUIRE(draw != draws.end());
                D6R_REQUIRE(draw->material.getColor() == Color(255, 255, 255, alpha));
                if (alpha < 255) D6R_REQUIRE(draw->blend == BlendFunc::SrcAlpha);
            }
        }
    }
}

D6R_TEST_CASE("PR87 submitted arena vertices retain Local Play basis through mirrored and retained snapshots") {
    char name[] = "duel6r-orientation-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recorder = std::make_unique<Test::RecordingRenderer>();
    auto &recording = *recorder;
    video.renderer = std::move(recorder);
    CanonicalWorldPresenter presenter(*application.service, application.gameResources);
    auto state = canonical(87, Network::Replication::Phase::ActiveRound);
    state.matchId = 1;
    state.settings.levels = {"levels/duel_16.json"};
    state.round = Network::Replication::RoundState{1, 1, state.settings.levels.front()};
    Network::Replication::PlayerState actor;
    actor.playerId = 101; actor.ownerParticipantId = 1; actor.life = 100;
    actor.positionX = 4 * 65536; actor.positionY = 3 * 65536;
    actor.heldWeapon = "pistol"; actor.visible = true; actor.displayName = "Upright";
    state.players = {actor};
    state.messages.currentPlayerIndicators = {actor.playerId};
    presenter.setCanonicalLevels(state.settings.levels);
    Camera localCamera;
    localCamera.rotate(180.0f, 0, 0); // Player constructors establish this Local Play camera.
    const auto localView = Matrix::lookAt(localCamera.getPosition(), localCamera.getFront(), localCamera.getUp());
    const auto localOrigin = localView * Vector::ZERO;
    D6R_REQUIRE((localView * Vector::UNIT_X).x > localOrigin.x);
    D6R_REQUIRE((localView * Vector::UNIT_Y).y > localOrigin.y);
    std::vector<bool> normalWalls;
    for (const bool mirrored : {false, true}) {
        state.round->mirrored = mirrored;
        ++state.round->roundNumber;
        presenter.update(0, &state, {});
        D6R_REQUIRE(presenter.level);
        const auto &level = *presenter.level;
        unsigned asymmetricCells = 0;
        for (int y = 0; y < level.getHeight(); ++y) {
            for (int x = 0; x < level.getWidth(); ++x) {
                if (!mirrored) normalWalls.push_back(level.isWall(x, y, false));
                else {
                    D6R_REQUIRE_EQ(normalWalls[y * level.getWidth() + level.getWidth() - 1 - x],
                                   level.isWall(x, y, false));
                    asymmetricCells += normalWalls[y * level.getWidth() + x] != level.isWall(x, y, false);
                }
            }
        }
        if (mirrored) D6R_REQUIRE(asymmetricCells > 0); // A real X-mirror, not a flag-only fixture.
        const auto texture = presenter.skinFor(actor).getTexture();
        D6R_REQUIRE(texture);
        for (const auto phase : {Network::Replication::Phase::ActiveRound,
                                 Network::Replication::Phase::FinalSummary}) {
            state.phase = phase;
            if (phase == Network::Replication::Phase::FinalSummary)
                presenter.update(0, nullptr, {}); // Retain loaded arena while no fresh canonical state is supplied.
            for (const auto &size : {std::pair<int, int>{1280, 900}, {900, 1280}}) {
                recording.setProjectionMatrix(Matrix::orthographic(0, size.first, 0, size.second, -1, 1));
                recording.setModelMatrix(Matrix::IDENTITY);
                recording.quads.clear();
                recording.buffers.clear();
                D6R_REQUIRE(presenter.render(state, {}, {}, size.first, size.second));
                const auto body = std::find_if(recording.quads.begin(), recording.quads.end(),
                        [texture](const auto &draw) { return draw.material.getTexture() == texture; });
                D6R_REQUIRE(body != recording.quads.end());
                const auto matrix = body->projection * body->view * body->model;
                const auto origin = matrix * Vector::ZERO;
                const auto right = matrix * Vector::UNIT_X;
                const auto up = matrix * Vector::UNIT_Y;
                D6R_REQUIRE(right.x > origin.x);
                D6R_REQUIRE(up.y > origin.y);
                D6R_REQUIRE(std::abs(right.y - origin.y) < 0.00001f);
                D6R_REQUIRE(std::abs(up.x - origin.x) < 0.00001f);
                auto signedArea = [](const auto &points) {
                    return (points[1].x - points[0].x) * (points[2].y - points[0].y)
                         - (points[1].y - points[0].y) * (points[2].x - points[0].x);
                };
                D6R_REQUIRE(signedArea(body->vertices) * signedArea(body->projected) > 0);
                D6R_REQUIRE(body->material.getColor() == Color::WHITE);
                D6R_REQUIRE(std::abs(signedArea(body->uv)) > 0);
                D6R_REQUIRE(!recording.buffers.empty());
                unsigned nonemptyBuffers = 0;
                for (const auto &buffer : recording.buffers) {
                    if (buffer.vertices.empty()) continue;
                    ++nonemptyBuffers;
                    D6R_REQUIRE_EQ(buffer.vertices.size(), buffer.projected.size());
                    for (unsigned i = 0; i < buffer.vertices.size(); ++i) {
                        const auto &vertex = buffer.vertices[i];
                        const auto expected = matrix * Vector(vertex.x, vertex.y, vertex.z);
                        D6R_REQUIRE(std::abs(expected.x - buffer.projected[i].x) < 0.00001f);
                        D6R_REQUIRE(std::abs(expected.y - buffer.projected[i].y) < 0.00001f);
                    }
                }
                D6R_REQUIRE(nonemptyBuffers > 0);
                for (unsigned i = 0; i < 4; ++i) {
                    for (unsigned j = 0; j < 4; ++j) {
                        if (body->vertices[i].y > body->vertices[j].y)
                            D6R_REQUIRE(body->projected[i].y > body->projected[j].y);
                        if (body->vertices[i].x > body->vertices[j].x)
                            D6R_REQUIRE(body->projected[i].x > body->projected[j].x);
                    }
                }
                // Name/ammunition backing is submitted above the actor, not beneath its feet.
                const auto label = std::find_if(recording.quads.begin(), recording.quads.end(), [](const auto &draw) {
                    return draw.material.getColor() == Color(0, 0, 200, 220);
                });
                D6R_REQUIRE(label != recording.quads.end());
                const auto feet = matrix * Vector(4, 3, 0);
                for (const auto &vertex : label->projected) D6R_REQUIRE(vertex.y > feet.y);
                D6R_REQUIRE_EQ(actor.positionX, state.players.front().positionX);
                D6R_REQUIRE_EQ(actor.positionY, state.players.front().positionY);
                D6R_REQUIRE_EQ(mirrored, state.round->mirrored);
            }
        }
        // Canonical water heights are absolute world levels. Check the submitted
        // filled region stays rooted at world zero as its surface advances.
        Network::Replication::WorldEntityState water;
        water.kind = Network::Replication::EntityKind::Water;
        state.entities = {water};
        Float32 previousTop = -2;
        Float32 bottom = -2;
        for (const int height : {0, 1, 2}) {
            state.entities.front().primaryValue = height;
            recording.quads.clear();
            recording.buffers.clear();
            D6R_REQUIRE(presenter.render(state, {}, {}, 900, 1280));
            const auto draw = std::find_if(recording.quads.begin(), recording.quads.end(), [](const auto &quad) {
                return quad.material.getColor() == Color(32, 96, 224, 128);
            });
            D6R_REQUIRE(draw != recording.quads.end());
            const auto bounds = std::minmax_element(draw->projected.begin(), draw->projected.end(),
                    [](const auto &a, const auto &b) { return a.y < b.y; });
            if (height == 0) bottom = bounds.first->y;
            D6R_REQUIRE(std::abs(bottom - bounds.first->y) < 0.00001f);
            D6R_REQUIRE(bounds.second->y > previousTop);
            previousTop = bounds.second->y;
            D6R_REQUIRE(draw->blend == BlendFunc::SrcAlpha);
            D6R_REQUIRE_EQ(height, state.entities.front().primaryValue);
        }
        state.entities.clear();
    }
}

D6R_TEST_CASE("PR87 real canonical motion and timed water project through submitted presenter matrices") {
    using namespace std::chrono_literals;
    // The test owns this temporary trace and removes it even on assertion failure.
    const auto root = std::filesystem::temp_directory_path()
            / ("duel6r-pr87-motion-" + std::to_string(::getpid()));
    D6R_REQUIRE(std::filesystem::create_directory(root));
    struct Cleanup {
        std::filesystem::path root;
        pid_t child = -1;
        ~Cleanup() {
            if (child > 0) {
                ::kill(child, SIGKILL);
                while (::waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
            }
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        }
    } cleanup{root};
    const auto path = (root / "motion.trace").string();
    // No shell or environment mutation in the parent; env replaces only these
    // child settings. The headless producer never shares its Player ABI with GL4.
    std::vector<std::string> arguments{"env", std::string("D6R_TEST_FILTER=") + Test::CanonicalMotionProducer,
            "D6R_TEST_EXACT=1", "D6R_CANONICAL_MOTION_TRACE=" + path, D6R_CANONICAL_MOTION_TEST_PRODUCER};
    std::vector<char *> argv;
    for (auto &argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    D6R_REQUIRE_EQ(0, ::posix_spawnp(&cleanup.child, "env", nullptr, nullptr, argv.data(), environ));
    int status = 0;
    bool exited = false;
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto result = ::waitpid(cleanup.child, &status, WNOHANG);
        if (result == cleanup.child) { cleanup.child = -1; exited = true; break; }
        D6R_REQUIRE(result == 0 || (result < 0 && errno == EINTR));
        std::this_thread::sleep_for(10ms);
    }
    D6R_REQUIRE(exited && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    const auto samples = Test::readCanonicalMotionTrace(path);

    char name[] = "duel6r-pr87-motion-presenter-tests";
    char *appArguments[] = {name};
    Application application(1, appArguments);
    auto &video = application.service->getVideo();
    struct RestoreRenderer {
        std::unique_ptr<Renderer> &slot;
        std::unique_ptr<Renderer> original;
        ~RestoreRenderer() { slot = std::move(original); }
    } restore{video.renderer, std::move(video.renderer)};
    auto recorder = std::make_unique<Test::RecordingRenderer>();
    auto &recording = *recorder;
    video.renderer = std::move(recorder);
    CanonicalWorldPresenter presenter(*application.service, application.gameResources);
    presenter.setCanonicalLevels(samples.front().snapshot.state.settings.levels);
    recording.setProjectionMatrix(Matrix::orthographic(0, 1280, 0, 900, -1, 1));
    recording.setModelMatrix(Matrix::IDENTITY);
    struct Projection {
        Vector root;
        Float32 waterBottom, waterTop;
        Network::Replication::PlayerState actor;
        std::int64_t waterHeight;
    };
    std::map<std::string, Projection> projected;
    for (const auto &sample : samples) {
        const auto &state = sample.snapshot.state;
        const auto serialized = Network::Replication::serializeReplicationSnapshot(sample.snapshot);
        const auto actor = std::find_if(state.players.begin(), state.players.end(),
                [](const auto &value) { return value.playerId == 101; });
        D6R_REQUIRE(actor != state.players.end());
        presenter.update(0, &state, {});
        recording.quads.clear(); recording.buffers.clear();
        D6R_REQUIRE(presenter.render(state, {}, {}, 1280, 900));
        const auto texture = presenter.skinFor(*actor).getTexture();
        const auto x = static_cast<Float32>(actor->positionX) / 65536;
        const auto y = static_cast<Float32>(actor->positionY) / 65536;
        const auto body = std::find_if(recording.quads.begin(), recording.quads.end(), [&](const auto &draw) {
            return draw.material.getTexture() == texture
                && std::any_of(draw.vertices.begin(), draw.vertices.end(), [&](const auto &vertex) {
                    return vertex.x == x && vertex.y == y;
                });
        });
        D6R_REQUIRE(body != recording.quads.end());
        const auto corner = std::find_if(body->vertices.begin(), body->vertices.end(), [&](const auto &vertex) {
            return vertex.x == x && vertex.y == y;
        });
        const auto bodyRoot = body->projected[std::distance(body->vertices.begin(), corner)];
        const auto matrix = body->projection * body->view * body->model;
        const auto expected = matrix * Vector(x, y, corner->z);
        D6R_REQUIRE_NEAR(expected.x, bodyRoot.x, 0.00001f);
        D6R_REQUIRE_NEAR(expected.y, bodyRoot.y, 0.00001f);
        const auto water = std::find_if(state.entities.begin(), state.entities.end(), [](const auto &entity) {
            return entity.kind == Network::Replication::EntityKind::Water;
        });
        D6R_REQUIRE(water != state.entities.end());
        const auto waterDraw = std::find_if(recording.quads.begin(), recording.quads.end(), [](const auto &draw) {
            return draw.material.getColor() == Color(32, 96, 224, 128);
        });
        D6R_REQUIRE(waterDraw != recording.quads.end());
        const auto bounds = std::minmax_element(waterDraw->projected.begin(), waterDraw->projected.end(),
                [](const auto &a, const auto &b) { return a.y < b.y; });
        D6R_REQUIRE_NEAR((matrix * Vector::ZERO).y, bounds.first->y, 0.00001f);
        D6R_REQUIRE_NEAR((matrix * Vector(0, water->primaryValue + 1, 0)).y, bounds.second->y, 0.00001f);
        D6R_REQUIRE(projected.emplace(sample.name, Projection{bodyRoot, bounds.first->y, bounds.second->y,
                                                             *actor, water->primaryValue}).second);
        D6R_REQUIRE(serialized == Network::Replication::serializeReplicationSnapshot(sample.snapshot));
    }
    const auto &rightBefore = projected.at("right-before"), &rightAfter = projected.at("right-after");
    D6R_REQUIRE(rightAfter.actor.positionX > rightBefore.actor.positionX);
    D6R_REQUIRE(rightAfter.root.x > rightBefore.root.x);
    const auto &jumpBefore = projected.at("jump-before"), &jumpAfter = projected.at("jump-after");
    D6R_REQUIRE(jumpAfter.actor.positionY > jumpBefore.actor.positionY);
    D6R_REQUIRE(jumpAfter.root.y > jumpBefore.root.y);
    const auto &fallBefore = projected.at("fall-before"), &fallAfter = projected.at("fall-after");
    D6R_REQUIRE(fallAfter.actor.positionY < fallBefore.actor.positionY);
    D6R_REQUIRE(fallAfter.root.y < fallBefore.root.y);
    const auto &waterBefore = projected.at("water-before"), &waterAfter = projected.at("water-after");
    D6R_REQUIRE_EQ(waterBefore.waterHeight + 1, waterAfter.waterHeight);
    D6R_REQUIRE(waterAfter.waterTop > waterBefore.waterTop);
    D6R_REQUIRE_NEAR(waterBefore.waterBottom, waterAfter.waterBottom, 0.00001f);
}

D6R_TEST_CASE("PR83 real runtime End from naturally completed final summary drains and discards result") {
    using namespace std::chrono_literals;
    // Independent temporary gameplay content, never changes a supplied bundle.
    // The tester owns cleanup, including assertion-failure unwinding.
    const auto root = std::filesystem::temp_directory_path()
            / ("duel6r-pr83-final-" + std::to_string(::getpid()));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    };
    D6R_REQUIRE(std::filesystem::create_directory(root));
    Cleanup cleanup{root};
    std::filesystem::create_directory(root / "data");
    std::filesystem::create_directory(root / "levels");
    for (const auto *file : {"blocks.json", "config.script"})
        std::filesystem::copy_file(std::filesystem::path(D6R_TEST_RESOURCE_DIR) / "data" / file,
                                   root / "data" / file);
    {
        std::ofstream level(root / "levels/end.json");
        level << R"({"width":6,"height":4,"blocks":[1,1,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,1,1,1,1,1,1,1],"elevators":[]})";
        D6R_REQUIRE(level.good());
    }
    Client::NetworkSessionRuntime host, guest, unused;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Host"}; setup.fixedLevel = "levels/end.json";
    setup.roundLimit = 1; setup.quickLiquid = true;
    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, root.string(), setup, {player("Host")}));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    D6R_REQUIRE(guest.join(endpoint, root.string(), {player("Guest")}));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
        return guest.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    host.setReady(true); guest.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
    host.startMatch();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 90s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Summary
               && guest.snapshot().journey == Client::NetworkJourney::Summary;
    }));
    D6R_REQUIRE(host.snapshot().canonical->result.available);
    D6R_REQUIRE_EQ(std::string("Completed"), host.snapshot().canonical->result.state);
    host.endSession();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Inactive
               && guest.snapshot().journey == Client::NetworkJourney::HostEnded;
    }));
    D6R_REQUIRE(!host.snapshot().canonical);
    D6R_REQUIRE(host.supervisor->snapshot().cleanupComplete);
}

D6R_TEST_CASE("PR83 summary and retained result draw bounded headers and scroll every long historical winner") {
    namespace A = Server::Authoritative;
    namespace R = Network::Replication;
    char name[] = "duel6r-pr83-outcome-layout-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    Test::RecordingRenderer recorder;
    Font font(recorder);
    font.load("data/font.ttf", application.console);
    auto &original = *application.service;
    AppService service(font, original.getConsole(), original.getTextureManager(), original.getVideo(),
            original.getInput(), original.getControlsManager(), original.getSound(), original.getScriptManager());
    NetworkMenu menu(service, application.gameResources, {}, [] {});
    A::SessionResult result;
    result.config.mode = A::Mode::Predator;
    result.config.hostParticipantId = 1;
    result.config.seed = 1234;
    result.config.roundLimit = 1;
    result.config.fixedLevel = "levels/duel_01.json";
    result.config.playableLevels = {result.config.fixedLevel};
    result.config.enabledWeapons = {"pistol"};
    result.completedRounds = 1;
    A::RoundResult round;
    round.roundNumber = 1; round.level = result.config.fixedLevel;
    for (unsigned index = 0; index < 15; ++index) {
        A::PlayerResultRow row;
        row.playerId = (std::numeric_limits<A::Identity>::max)() - index;
        row.participantId = index + 1;
        row.rosterOrder = index;
        row.displayName = std::string(63, static_cast<char>('A' + index)) + static_cast<char>('a' + index);
        row.departed = true;
        row.statistics.roundsPlayed = 1;
        row.rounds.resize(1); row.rounds[0].roundsPlayed = 1;
        round.rosterOrder.push_back(row.playerId);
        if (index < 14) round.winnerPlayerIds.push_back(row.playerId);
        result.players.push_back(row);
    }
    result.rounds = {round}; result.finalWinnerPlayerIds = round.winnerPlayerIds;
    const auto serialized = A::serializeSessionResult(result);
    D6R_REQUIRE(serialized.has_value());
    R::CanonicalState state;
    state.completedRounds = 1;
    state.settings.mode = "Predator";
    state.score.winner.winnerPlayerIds = result.finalWinnerPlayerIds;
    state.round = R::RoundState{3, 1, round.level, false, round.rosterOrder, state.score.winner};
    state.result = {true, true, "Completed", *serialized};
    D6R_REQUIRE(R::retainedOutcomeRows(state.result).has_value());
    for (const bool retained : {false, true}) {
        state.phase = retained ? R::Phase::Lobby : R::Phase::FinalSummary;
        menu.runtime.current.journey = retained ? Client::NetworkJourney::Lobby : Client::NetworkJourney::Summary;
        menu.runtime.current.canonical = state;
        std::set<std::string> visible;
        bool matchHeading = false, lastRoundHeading = false;
        menu.summaryScroll = 0;
        menu.focus = retained ? 2 : 1; // Guest result-table focus, with no local slots in this historical fixture.
        for (unsigned page = 0; page < 40; ++page) {
            menu.summaryHorizontal = 0;
            for (unsigned horizontal = 0; horizontal < 5; ++horizontal) {
                recorder.draws.clear();
                if (retained) menu.drawResult(state, true);
                else menu.drawSummary(menu.runtime.snapshot());
                for (const auto &draw : recorder.draws) {
                    const auto entry = std::find_if(font.fontCache.entryList.begin(), font.fontCache.entryList.end(),
                            [&](const auto &item) { return item.texture == draw.material.getTexture(); });
                    D6R_REQUIRE(entry != font.fontCache.entryList.end());
                    const auto &text = entry->text;
                    const bool match = text.find("Match outcome:") == 0;
                    const bool last = text.find("Last completed round") == 0;
                    if (match || last) {
                        D6R_REQUIRE(draw.right <= 818); // Inside the fixed 850-unit menu canvas.
                        matchHeading = matchHeading || match; lastRoundHeading = lastRoundHeading || last;
                    }
                    visible.insert(text);
                }
                application.keyEvent(menu, KeyPressEvent(SDLK_RIGHT, SysEvent::ButtonState::PRESSED, 0));
            }
            menu.mouseWheelEvent(MouseWheelEvent(0, 0, 0, -1));
        }
        D6R_REQUIRE(matchHeading && lastRoundHeading);
        for (unsigned index = 0; index < 14; ++index) {
            const auto &row = result.players[index];
            const bool nameVisible = std::any_of(visible.begin(), visible.end(), [&](const auto &text) {
                return text.find(row.displayName) != std::string::npos;
            });
            if (!nameVisible) {
                std::string detail = "retained=" + std::to_string(retained) + ";winner=" + std::to_string(index)
                        + ";scroll=" + std::to_string(menu.summaryScroll) + ";visible=";
                for (const auto &text : visible) if (detail.size() < 4000) detail += "[" + text + "]";
                Test::fail("complete winner name is reachable", __FILE__, __LINE__, detail);
            }
            D6R_REQUIRE(std::any_of(visible.begin(), visible.end(), [&](const auto &text) {
                return text.find(std::to_string(row.playerId)) != std::string::npos
                       && text.find(std::string(8, static_cast<char>('A' + index))) != std::string::npos;
            }));
        }
    }
}

D6R_TEST_CASE("PR83 host emits movement and fire-release masks until exact round-end freeze boundary") {
    char name[] = "duel6r-pr83-round-input-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    auto controls = PlayerControls::keyboardControls("QA", application.input,
            SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
    Client::NetworkSessionRuntime host;
    host.players = {{"Host", controls.get(), "QA"}};
    host.sampledActions.resize(1);
    host.current.host = true;
    host.current.canonical = canonical(91, Network::Replication::Phase::ActiveRound);
    auto &state = *host.current.canonical;
    state.hostParticipantId = 1;
    state.participants = {{1, true, Network::Replication::ConnectionState::Connected, true, {101}}};
    std::vector<Network::Input::Command> commands;
    host.hostInput = std::make_unique<Network::Input::ClientCommandSession>(1,
            std::vector<Network::Input::Identity>{101}, [&](auto payload) {
                const auto decoded = Network::Input::deserializeFrame(payload);
                D6R_REQUIRE(decoded && decoded->command);
                commands.push_back(*decoded->command);
                return Network::SendResult::Accepted;
            });
    application.input.setPressed(SDLK_LEFT, true);
    application.input.setPressed(SDLK_RCTRL, true);
    host.update();
    D6R_REQUIRE_EQ(1u, commands.size());
    D6R_REQUIRE_EQ(Network::Input::MoveLeft | Network::Input::Shoot, commands.back().actions);
    state.phase = Network::Replication::Phase::RoundSummary;
    state.roundEndCountdown = 360;
    ++state.phaseTime;
    application.input.setPressed(SDLK_LEFT, false);
    application.input.setPressed(SDLK_RCTRL, false); // Charged-fire release.
    host.update();
    D6R_REQUIRE_EQ(2u, commands.size());
    D6R_REQUIRE_EQ(0u, commands.back().actions);
    state.roundEndCountdown = 301;
    ++state.phaseTime;
    application.input.setPressed(SDLK_RIGHT, true);
    host.update();
    D6R_REQUIRE_EQ(3u, commands.size());
    D6R_REQUIRE_EQ(Network::Input::MoveRight, commands.back().actions);
    for (const unsigned countdown : {300u, 299u, 1u, 0u}) {
        state.roundEndCountdown = countdown;
        ++state.phaseTime;
        host.update();
        D6R_REQUIRE_EQ(3u, commands.size());
    }
}

D6R_TEST_CASE("PR83 latest host and guest charged bow releases fire during active round-end second") {
    using namespace std::chrono_literals;
    char name[] = "duel6r-pr83-charged-release-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    Input input(application.console);
    auto hostControls = PlayerControls::keyboardControls("Host bow", input,
            SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_q, SDLK_RSHIFT, SDLK_RETURN);
    auto guestControls = PlayerControls::keyboardControls("Guest bow", input,
            SDLK_a, SDLK_d, SDLK_w, SDLK_s, SDLK_u, SDLK_1, SDLK_2);
    const auto root = std::filesystem::temp_directory_path()
            / ("duel6r-pr83-bow-" + std::to_string(::getpid()));
    D6R_REQUIRE(std::filesystem::create_directory(root));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    std::filesystem::create_directory(root / "data");
    std::filesystem::create_directory(root / "levels");
    std::filesystem::copy_file(std::filesystem::path(D6R_TEST_RESOURCE_DIR) / "data/blocks.json", root / "data/blocks.json");
    {
        std::ofstream config(root / "data/config.script");
        config << "volume 128\nmusic on\n";
        for (unsigned weapon = 0; weapon < 17; ++weapon)
            config << "gun " << weapon << (weapon == 9 ? " true\n" : " false\n");
        D6R_REQUIRE(config.good());
        std::ofstream level(root / "levels/bow.json");
        level << R"({"width":10,"height":4,"blocks":[1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1],"elevators":[]})";
        D6R_REQUIRE(level.good());
    }
    Client::NetworkSessionRuntime host, opponent, ally;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {"Host archer"};
    setup.mode = "Team deathmatch"; setup.teamCount = 2;
    setup.fixedLevel = "levels/bow.json"; setup.roundLimit = 1; setup.quickLiquid = false;
    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, root.string(), setup,
                              {{"Host archer", hostControls.get(), "Host bow"}}));
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 10s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    D6R_REQUIRE(opponent.join(endpoint, root.string(), {player("Departing Bravo")}));
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 10s, [&] {
        return opponent.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    D6R_REQUIRE(ally.join(endpoint, root.string(), {{"Guest archer", guestControls.get(), "Guest bow"}}));
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 10s, [&] {
        return ally.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    host.setReady(true); opponent.setReady(true); ally.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
    host.startMatch();
    const auto chargedArchers = [](const Client::NetworkRuntimeSnapshot &snapshot) {
        return snapshot.canonical && std::count_if(snapshot.canonical->players.begin(), snapshot.canonical->players.end(),
                [](const auto &slot) { return slot.team == 1 && slot.heldWeapon == "bow" && slot.charge >= 49152
                    && (slot.actionMask & Network::Input::Shoot) != 0; }) == 2;
    };
    input.setPressed(SDLK_q, true); input.setPressed(SDLK_u, true);
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 5s, [&] {
        return chargedArchers(host.snapshot()) && chargedArchers(ally.snapshot());
    }));
    const auto before = *host.snapshot().canonical;
    std::map<Network::Replication::Identity, int> ammo;
    for (const auto &slot : before.players) if (slot.team == 1) ammo.emplace(slot.playerId, slot.ammunition);
    D6R_REQUIRE_EQ(2u, ammo.size());
    // Real intentional removal of the sole opposing team produces a normal
    // round win while both the host and remote archer remain alive and charged.
    opponent.leave();
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 3s, [&] {
        for (const auto *runtime : {&host, &ally}) {
            const auto snapshot = runtime->snapshot();
            if (!snapshot.canonical || snapshot.canonical->phase != Network::Replication::Phase::RoundSummary
                || snapshot.canonical->roundEndCountdown <= 300 || !chargedArchers(snapshot)) return false;
        }
        return true;
    }));
    input.setPressed(SDLK_q, false); input.setPressed(SDLK_u, false);
    const bool firedOnRelease = pumpRuntimes(host, opponent, ally, 500ms, [&] {
        for (const auto *runtime : {&host, &ally}) {
            const auto snapshot = runtime->snapshot();
            if (!snapshot.canonical || snapshot.canonical->roundEndCountdown <= 300) return false;
            for (const auto &[id, previousAmmo] : ammo) {
                const auto slot = std::find_if(snapshot.canonical->players.begin(), snapshot.canonical->players.end(),
                        [id](const auto &player) { return player.playerId == id; });
                if (slot == snapshot.canonical->players.end() || slot->charge >= 32768 || slot->actionMask != 0
                    || slot->ammunition != previousAmmo - 1) return false;
                const auto score = std::find_if(snapshot.canonical->score.players.begin(), snapshot.canonical->score.players.end(),
                        [id](const auto &row) { return row.playerId == id; });
                if (score == snapshot.canonical->score.players.end() || score->shots != 1) return false;
                if (!snapshot.host && std::none_of(snapshot.presentationEvents.begin(), snapshot.presentationEvents.end(),
                        [id](const auto &event) { return event.playerId == id && event.type == "shot-fired"; })) return false;
            }
        }
        return true;
    });
    if (!firedOnRelease) {
        std::ostringstream detail;
        for (const auto *runtime : {&host, &ally}) {
            const auto snapshot = runtime->snapshot();
            detail << "host=" << snapshot.host << ";journey=" << static_cast<unsigned>(snapshot.journey)
                   << ";status=" << snapshot.status << ";failure=" << snapshot.failure;
            if (snapshot.canonical) {
                detail << ";countdown=" << snapshot.canonical->roundEndCountdown;
                for (const auto &slot : snapshot.canonical->players)
                    detail << ";player=" << slot.playerId << ":ammo=" << slot.ammunition
                           << ":charge=" << slot.charge << ":mask=" << slot.actionMask;
            }
            for (const auto &event : snapshot.presentationEvents)
                detail << ";event=" << event.type << ":player=" << event.playerId;
            detail << '\n';
        }
        Test::fail("host and guest fire charged bows before freezing", __FILE__, __LINE__, detail.str());
    }
    host.endSession();
    D6R_REQUIRE(pumpRuntimes(host, opponent, ally, 5s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Inactive
               && ally.snapshot().journey == Client::NetworkJourney::HostEnded;
    }));
}

D6R_TEST_CASE("PR83 menu Team preferences converge then fourteen host slots and guest sustain release edges") {
    using namespace std::chrono_literals;
    char name[] = "duel6r-pr83-capacity-tests";
    char *arguments[] = {name};
    Application application(1, arguments);
    const auto root = std::filesystem::temp_directory_path()
            / ("duel6r-pr83-capacity-" + std::to_string(::getpid()));
    D6R_REQUIRE(std::filesystem::create_directory(root));
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    std::filesystem::create_directory(root / "data");
    std::filesystem::create_directory(root / "levels");
    for (const auto *file : {"blocks.json", "config.script"})
        std::filesystem::copy_file(std::filesystem::path(D6R_TEST_RESOURCE_DIR) / "data" / file, root / "data" / file);
    {
        // A long input-rate test must not mistake an arena hazard killing an
        // idle player for rejected release input. Keep all fifteen slots alive.
        std::ofstream level(root / "levels/capacity.json");
        level << "{\"width\":40,\"height\":6,\"blocks\":[";
        for (unsigned y = 0; y < 6; ++y) for (unsigned x = 0; x < 40; ++x) {
            if (y || x) level << ',';
            level << ((x == 0 || x == 39 || y == 0 || y == 5) ? 1 : 0);
        }
        level << "],\"elevators\":[]}";
        D6R_REQUIRE(level.good());
    }
    NetworkMenu menu(*application.service, application.gameResources, {}, [] {});
    Client::NetworkSessionRuntime guest, unused;
    auto &host = menu.runtime;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
    std::vector<std::unique_ptr<PlayerControls>> controls;
    Input input(application.console);
    for (unsigned index = 0; index < 15; ++index) {
        // Independent real keyboard controls; crouch is harmless on the arena
        // floor and its accepted mask exposes both press and release per slot.
        controls.push_back(PlayerControls::keyboardControls("QA", input,
                SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_a + index,
                SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN));
        if (index < 14) menu.localPlayers.push_back({"Host " + std::to_string(index), controls.back().get(), "QA"});
    }
    menu.hostSetup.localPlayerNames.clear();
    for (const auto &slot : menu.localPlayers) menu.hostSetup.localPlayerNames.push_back(slot.name);
    menu.hostSetup.mode = "Team deathmatch";
    menu.hostSetup.teamCount = 4;
    menu.hostSetup.friendlyFire = true;
    menu.hostSetup.fixedLevel = "levels/capacity.json";
    menu.hostSetup.quickLiquid = false;
    menu.preferredTeamCount = 4;
    menu.preferredFriendlyFire = true;
    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, root.string(),
                              menu.hostSetup, menu.localPlayers));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    D6R_REQUIRE(guest.join(endpoint, root.string(), {{"Guest", controls.back().get(), "QA"}}));
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
        return guest.snapshot().journey == Client::NetworkJourney::Lobby;
    }));
    for (const std::string mode : {"Deathmatch", "Predator", "Team deathmatch"}) {
        host.setReady(true); guest.setReady(true);
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            return everyParticipantReady(host.snapshot(), true);
        }));
        menu.focus = 2 * 14 + 1; // Mode, after fourteen person/control pairs and Ready.
        application.keyEvent(menu, KeyPressEvent(SDLK_RETURN, SysEvent::ButtonState::PRESSED, 0));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            for (auto *runtime : {&host, &guest}) {
                const auto snapshot = runtime->snapshot();
                if (!snapshot.canonical || snapshot.canonical->settings.mode != mode
                    || snapshot.canonical->settings.teamCount != (mode == "Team deathmatch" ? 4 : 0)
                    || snapshot.canonical->settings.friendlyFire != (mode == "Team deathmatch")
                    || !everyParticipantReady(snapshot, false)) return false;
            }
            return true;
        }));
        // A subsequent unrelated edit must not disappear into an invalid draft.
        const auto previousRounds = menu.hostSetup.roundLimit;
        menu.focus = 2 * 14 + 1 + 5;
        application.keyEvent(menu, KeyPressEvent(SDLK_RETURN, SysEvent::ButtonState::PRESSED, 0));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            return guest.snapshot().canonical->settings.roundLimit == previousRounds + 1;
        }));
    }
    host.setReady(true); guest.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
    host.startMatch();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Match
               && guest.snapshot().journey == Client::NetworkJourney::Match;
    }));
    const auto initialTick = host.snapshot().canonical->phaseTime;
    const auto ids = host.snapshot().canonical->participants.front().ownedPlayerIds;
    D6R_REQUIRE_EQ(14u, ids.size());
    std::size_t edges = 0;
    for (unsigned step = 0; step < 120; ++step) {
        for (unsigned index = 0; index < 15; ++index)
            input.setPressed(SDLK_a + index, (step + index) % 2 == 0);
        const auto until = std::chrono::steady_clock::now() + 300ms;
        const bool converged = pumpRuntimes(host, guest, unused, 250ms, [&] {
            const auto snapshot = guest.snapshot();
            if (!snapshot.canonical || snapshot.canonical->players.size() != 15) return false;
            for (const auto &slot : snapshot.canonical->players) {
                if (slot.lifeState != Network::Replication::LifeState::Alive) return false;
                const auto found = std::find(ids.begin(), ids.end(), slot.playerId);
                const auto index = found == ids.end() ? 14u : static_cast<unsigned>(found - ids.begin());
                const auto expected = (step + index) % 2 == 0 ? Network::Input::Crouch : 0u;
                if (slot.actionMask != expected) return false;
            }
            return true;
        });
        if (!converged) {
            std::ostringstream detail;
            detail << "step=" << step;
            for (const auto *runtime : {&host, &guest}) {
                const auto snapshot = runtime->snapshot();
                detail << ";host=" << snapshot.host << ";journey=" << static_cast<unsigned>(snapshot.journey)
                       << ";failure=" << snapshot.failure;
                if (snapshot.canonical) {
                    detail << ";tick=" << snapshot.canonical->phaseTime;
                    for (const auto &slot : snapshot.canonical->players)
                        detail << ";player=" << slot.playerId << ":life=" << static_cast<unsigned>(slot.lifeState)
                               << ":mask=" << slot.actionMask;
                }
            }
            Test::fail("all fifteen slots converge under sustained input", __FILE__, __LINE__, detail.str());
        }
        ++edges;
        // Continue pumping at normal frame cadence after convergence; a test
        // that stops the producer on each early success hides growing backlog.
        while (std::chrono::steady_clock::now() < until) {
            host.update(); guest.update();
            std::this_thread::sleep_for(5ms);
        }
        D6R_REQUIRE(host.snapshot().journey == Client::NetworkJourney::Match);
        D6R_REQUIRE(guest.snapshot().journey == Client::NetworkJourney::Match);
    }
    D6R_REQUIRE_EQ(120u, edges);
    D6R_REQUIRE(host.snapshot().canonical->phaseTime > initialTick + 1800);
    D6R_REQUIRE(guest.snapshot().canonical->phaseTime > initialTick + 1800);
    D6R_REQUIRE(host.hostInput && !host.hostInput->policyViolation());
    host.endSession();
    D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
        return host.snapshot().journey == Client::NetworkJourney::Inactive
               && guest.snapshot().journey == Client::NetworkJourney::HostEnded;
    }));
}

D6R_TEST_CASE("NET-AC-004 NET-AC-006 NET-AC-009 NET-AC-017 three NetworkSessionRuntime participants preserve team lobby composition and survive authenticated reconnect probes") {
    using namespace std::chrono_literals;
    Client::NetworkSessionRuntime host;
    Client::NetworkSessionRuntime first;
    Client::NetworkSessionRuntime second;
    const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};

    const std::vector<Client::NetworkLocalPlayer> hostPlayers{player("Håkon"), player("Zoë")};
    const std::vector<Client::NetworkLocalPlayer> firstPlayers{player("Jiří"), player("Renée")};
    const std::vector<Client::NetworkLocalPlayer> secondPlayers{player("Søren"), player("Łukasz")};
    Network::HostComposition::Setup setup;
    setup.localPlayerNames = {hostPlayers[0].name, hostPlayers[1].name};
    setup.mode = "Team deathmatch";
    setup.teamCount = 2;
    setup.fixedLevel = "levels/duel_01.json";
    setup.roundLimit = 3;

    D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, D6R_TEST_RESOURCE_DIR,
                               setup, hostPlayers));
    D6R_REQUIRE(pumpRuntimes(host, first, second, 10s, [&] {
        const auto snapshot = host.snapshot();
        return snapshot.journey == Client::NetworkJourney::Lobby && snapshot.canonical
               && snapshot.canonical->players.size() == 2;
    }));
    D6R_REQUIRE(first.join(endpoint, D6R_TEST_RESOURCE_DIR, firstPlayers));
    D6R_REQUIRE(second.join(endpoint, D6R_TEST_RESOURCE_DIR, secondPlayers));
    D6R_REQUIRE(pumpRuntimes(host, first, second, 10s, [&] {
        return sixPlayerLobby(host.snapshot()) && sixPlayerLobby(first.snapshot())
               && sixPlayerLobby(second.snapshot());
    }));

    const auto hostLobby = host.snapshot();
    const auto firstLobby = first.snapshot();
    const auto secondLobby = second.snapshot();
    D6R_REQUIRE(hostLobby.canonical->sessionId != 0);
    D6R_REQUIRE_EQ(hostLobby.canonical->sessionId, firstLobby.canonical->sessionId);
    D6R_REQUIRE_EQ(hostLobby.canonical->sessionId, secondLobby.canonical->sessionId);
    D6R_REQUIRE(firstLobby.localParticipantId != 0);
    D6R_REQUIRE(secondLobby.localParticipantId != 0);
    D6R_REQUIRE(firstLobby.localParticipantId != secondLobby.localParticipantId);
    D6R_REQUIRE_EQ(std::string("Team deathmatch"), hostLobby.canonical->settings.mode);
    D6R_REQUIRE_EQ(2u, hostLobby.canonical->settings.teamCount);
    D6R_REQUIRE(!hostLobby.canonical->settings.friendlyFire);

    std::vector<std::pair<Network::Replication::Identity, Network::Replication::Identity>> immutableSlots;
    for (const auto &slot: hostLobby.canonical->players)
        immutableSlots.emplace_back(slot.playerId, slot.ownerParticipantId);
    std::sort(immutableSlots.begin(), immutableSlots.end());
    const auto requireImmutableSixSlots = [&](const Client::NetworkRuntimeSnapshot &snapshot) {
        D6R_REQUIRE(sixPlayerLobby(snapshot));
        std::vector<std::pair<Network::Replication::Identity, Network::Replication::Identity>> slots;
        for (const auto &slot: snapshot.canonical->players)
            slots.emplace_back(slot.playerId, slot.ownerParticipantId);
        std::sort(slots.begin(), slots.end());
        D6R_REQUIRE(slots == immutableSlots);
    };
    const auto ownedPlayerIds = [](const Client::NetworkRuntimeSnapshot &snapshot,
                                   Network::Replication::Identity participantId) {
        const auto participant = std::find_if(snapshot.canonical->participants.begin(),
                snapshot.canonical->participants.end(), [participantId](const auto &value) {
                    return value.participantId == participantId;
                });
        D6R_REQUIRE(participant != snapshot.canonical->participants.end());
        return participant->ownedPlayerIds;
    };
    const auto hostPlayerIds = ownedPlayerIds(hostLobby, hostLobby.localParticipantId);
    const auto firstPlayerIds = ownedPlayerIds(firstLobby, firstLobby.localParticipantId);
    D6R_REQUIRE_EQ(2u, hostPlayerIds.size());
    D6R_REQUIRE_EQ(2u, firstPlayerIds.size());

    const auto readyEveryone = [&] {
        host.setReady(true); first.setReady(true); second.setReady(true);
        D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
            return everyParticipantReady(host.snapshot(), true)
                   && everyParticipantReady(first.snapshot(), true)
                   && everyParticipantReady(second.snapshot(), true);
        }));
    };
    const auto hostTransition = [&](const std::string &mode, std::uint8_t teamCount,
                                    bool friendlyFire) {
        readyEveryone();
        setup.mode = mode;
        setup.teamCount = teamCount;
        setup.friendlyFire = friendlyFire;
        host.updateHostSetup(setup);
        D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
            for (const auto *runtime: {&host, &first, &second}) {
                const auto snapshot = runtime->snapshot();
                if (!sixPlayerLobby(snapshot) || snapshot.canonical->settings.mode != mode
                    || snapshot.canonical->settings.teamCount != teamCount
                    || snapshot.canonical->settings.friendlyFire != friendlyFire
                    || !everyParticipantReady(snapshot, false)) return false;
                if (snapshot.canonical->score.teamTotals.size() != teamCount
                    || snapshot.canonical->score.teamRanking.size() != teamCount) return false;
                for (std::size_t team = 0; team < teamCount; ++team)
                    if (snapshot.canonical->score.teamRanking[team] != team + 1) return false;
            }
            return true;
        }));
        requireImmutableSixSlots(host.snapshot());
        requireImmutableSixSlots(first.snapshot());
        requireImmutableSixSlots(second.snapshot());
    };

    hostTransition("Team deathmatch", 3, true);
    hostTransition("Team deathmatch", 4, false);
    hostTransition("Predator", 0, false);
    hostTransition("Deathmatch", 0, false);
    hostTransition("Team deathmatch", 2, true);

    readyEveryone();
    host.setReady(false);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return !participantReady(host.snapshot(), hostLobby.localParticipantId)
               && participantReady(host.snapshot(), firstLobby.localParticipantId)
               && participantReady(host.snapshot(), secondLobby.localParticipantId);
    }));
    host.setReady(true);
    first.setReady(false);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return participantReady(host.snapshot(), hostLobby.localParticipantId)
               && !participantReady(host.snapshot(), firstLobby.localParticipantId)
               && participantReady(host.snapshot(), secondLobby.localParticipantId);
    }));
    first.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return everyParticipantReady(host.snapshot(), true);
    }));

    first.localConfigurationChanged();
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return everyParticipantReady(host.snapshot(), false)
               && everyParticipantReady(first.snapshot(), false)
               && everyParticipantReady(second.snapshot(), false);
    }));
    readyEveryone();
    host.localConfigurationChanged();
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return everyParticipantReady(host.snapshot(), false)
               && everyParticipantReady(first.snapshot(), false)
               && everyParticipantReady(second.snapshot(), false);
    }));

    readyEveryone();
    first.rebindLocalPlayers({player("Guest Renamed One"), player("Guest Renamed Two")});
    first.ownedPersonsChanged();
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        const auto snapshot = host.snapshot();
        if (!snapshot.canonical || !everyParticipantReady(snapshot, false)) return false;
        std::map<Network::Replication::Identity, std::string> names;
        for (const auto &slot: snapshot.canonical->players) names[slot.playerId] = slot.displayName;
        return names[firstPlayerIds[0]] == "Guest Renamed One"
               && names[firstPlayerIds[1]] == "Guest Renamed Two";
    }));
    requireImmutableSixSlots(host.snapshot());

    readyEveryone();
    setup.localPlayerNames = {"Host Renamed One", "Host Renamed Two"};
    host.updateHostSetup(setup);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        const auto snapshot = host.snapshot();
        if (!snapshot.canonical || !everyParticipantReady(snapshot, false)) return false;
        std::map<Network::Replication::Identity, std::string> names;
        for (const auto &slot: snapshot.canonical->players) names[slot.playerId] = slot.displayName;
        return names[hostPlayerIds[0]] == "Host Renamed One"
               && names[hostPlayerIds[1]] == "Host Renamed Two";
    }));
    requireImmutableSixSlots(host.snapshot());

    readyEveryone();
    std::map<Network::Replication::Identity, std::uint8_t> positionsBefore;
    const auto beforeRosterMove = host.snapshot();
    D6R_REQUIRE(beforeRosterMove.canonical.has_value());
    for (const auto &slot: beforeRosterMove.canonical->players)
        positionsBefore[slot.playerId] = slot.rosterPosition;
    const auto movedPlayer = beforeRosterMove.canonical->players.front().playerId;
    host.moveRosterPlayer(movedPlayer, 1);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        const auto snapshot = host.snapshot();
        if (!snapshot.canonical || !everyParticipantReady(snapshot, false)) return false;
        const auto moved = std::find_if(snapshot.canonical->players.begin(),
                snapshot.canonical->players.end(), [&](const auto &slot) {
                    return slot.playerId == movedPlayer;
                });
        return moved != snapshot.canonical->players.end()
               && moved->rosterPosition != positionsBefore.at(movedPlayer);
    }));
    requireImmutableSixSlots(host.snapshot());
    requireImmutableSixSlots(first.snapshot());
    requireImmutableSixSlots(second.snapshot());
    const bool configurationConverged = pumpRuntimes(host, first, second, 5s, [&] {
        const auto hostNow = host.snapshot();
        const auto firstNow = first.snapshot();
        const auto secondNow = second.snapshot();
        return everyParticipantReady(hostNow, false)
               && lobbyConfigurationFingerprint(hostNow) == lobbyConfigurationFingerprint(firstNow)
               && lobbyConfigurationFingerprint(hostNow) == lobbyConfigurationFingerprint(secondNow);
    });
    if (!configurationConverged) {
        const auto hostNow = host.snapshot();
        const auto firstNow = first.snapshot();
        const auto secondNow = second.snapshot();
        Duel6::Test::fail("lobby configuration converged", __FILE__, __LINE__,
                "host=" + lobbyConfigurationFingerprint(hostNow)
                + "\nfirst=" + lobbyConfigurationFingerprint(firstNow)
                + "\nsecond=" + lobbyConfigurationFingerprint(secondNow));
    }

    const auto guestBefore = canonicalFingerprint(first.snapshot());
    auto unauthorizedGuestSetup = setup;
    unauthorizedGuestSetup.teamCount = 3;
    first.updateHostSetup(unauthorizedGuestSetup);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 250ms, [&] { return false; }) == false);
    D6R_REQUIRE_EQ(guestBefore, canonicalFingerprint(first.snapshot()));

    const auto rejectHostCommandWithoutMutation = [&](std::vector<std::uint8_t> command) {
        const auto hostBefore = canonicalFingerprint(host.snapshot());
        const auto firstBefore = canonicalFingerprint(first.snapshot());
        const auto secondBefore = canonicalFingerprint(second.snapshot());
        {
            std::lock_guard<std::mutex> lock(host.mutex);
            host.pendingHostCommands.push_back(std::move(command));
        }
        D6R_REQUIRE(pumpRuntimes(host, first, second, 250ms, [&] { return false; }) == false);
        D6R_REQUIRE(host.snapshot().journey == Client::NetworkJourney::Lobby);
        D6R_REQUIRE_EQ(hostBefore, canonicalFingerprint(host.snapshot()));
        D6R_REQUIRE_EQ(firstBefore, canonicalFingerprint(first.snapshot()));
        D6R_REQUIRE_EQ(secondBefore, canonicalFingerprint(second.snapshot()));
    };

    rejectHostCommandWithoutMutation({0x44, 0x36, 0x48});

    auto invalidNonTeam = setup;
    invalidNonTeam.mode = "Deathmatch";
    invalidNonTeam.teamCount = 0;
    invalidNonTeam.friendlyFire = false;
    auto invalidNonTeamPayload = Network::HostComposition::serializeSetupUpdate(invalidNonTeam);
    std::size_t teamOffset = 9;
    for (const auto &name: invalidNonTeam.localPlayerNames) teamOffset += 2 + name.size();
    teamOffset += 2 + invalidNonTeam.mode.size();
    D6R_REQUIRE(teamOffset < invalidNonTeamPayload.size());
    invalidNonTeamPayload[teamOffset] = 2;
    rejectHostCommandWithoutMutation(std::move(invalidNonTeamPayload));

    auto slotChanging = setup;
    slotChanging.localPlayerNames.resize(1);
    rejectHostCommandWithoutMutation(Network::HostComposition::serializeSetupUpdate(slotChanging));
    rejectHostCommandWithoutMutation(Network::HostComposition::serializeRosterMove(
            (std::numeric_limits<Network::Replication::Identity>::max)(), 1));

    // A valid command after rejected inputs proves the host session remains usable.
    hostTransition("Team deathmatch", 3, false);

    first.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        return participantReady(host.snapshot(), firstLobby.localParticipantId)
               && participantReady(first.snapshot(), firstLobby.localParticipantId)
               && participantReady(second.snapshot(), firstLobby.localParticipantId);
    }));
    D6R_REQUIRE(first.snapshot().journey == Client::NetworkJourney::Lobby);
    D6R_REQUIRE(second.snapshot().journey == Client::NetworkJourney::Lobby);

    second.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        for (const auto *runtime: {&host, &first, &second}) {
            const auto snapshot = runtime->snapshot();
            if (!sixPlayerLobby(snapshot)
                || !participantReady(snapshot, firstLobby.localParticipantId)
                || !participantReady(snapshot, secondLobby.localParticipantId)) return false;
        }
        return true;
    }));

    const auto readyProbeDeadline = std::chrono::steady_clock::now() + 1200ms;
    while (std::chrono::steady_clock::now() < readyProbeDeadline) {
        host.update(); first.update(); second.update();
        D6R_REQUIRE(sixPlayerLobby(first.snapshot()));
        D6R_REQUIRE(sixPlayerLobby(second.snapshot()));
        std::this_thread::sleep_for(5ms);
    }

    // A correctly framed action authenticated to the wrong session must close only
    // the offender. The runtime must consume its reconnect grant, restore the same
    // participant and canonical session, and continue beyond several 250 ms probes.
    const auto wrongSession = hostLobby.canonical->sessionId
                              == (std::numeric_limits<std::uint64_t>::max)()
                              ? hostLobby.canonical->sessionId - 1
                              : hostLobby.canonical->sessionId + 1;
    {
        std::lock_guard<std::mutex> lock(first.mutex);
        first.pendingGuestCommands.push_back(Network::Lifecycle::serializeParticipantAction({
                wrongSession, firstLobby.localParticipantId,
                Network::Lifecycle::ParticipantActionKind::Ready}));
    }
    bool reconnectObserved = false;
    bool hostObservedReservedParticipant = false;
    std::vector<unsigned> countdown;
    bool countdownMonotonic = true;
    std::ostringstream reconnectTrace;
    std::optional<Client::NetworkJourney> lastJourney;
    const bool restored = pumpRuntimes(host, first, second, 5s, [&] {
        const auto firstNow = first.snapshot();
        const auto hostNow = host.snapshot();
        const auto secondNow = second.snapshot();
        if (!lastJourney || *lastJourney != firstNow.journey) {
            reconnectTrace << "journey=" << static_cast<unsigned>(firstNow.journey);
            if (firstNow.reconnectSeconds) reconnectTrace << ":seconds=" << *firstNow.reconnectSeconds;
            reconnectTrace << ';';
            lastJourney = firstNow.journey;
        }
        if (firstNow.journey == Client::NetworkJourney::Reconnecting) {
            reconnectObserved = true;
            if (firstNow.reconnectSeconds) {
                if (!countdown.empty() && *firstNow.reconnectSeconds > countdown.back()) {
                    countdownMonotonic = false;
                    reconnectTrace << "increase=" << countdown.back() << "->"
                                   << *firstNow.reconnectSeconds << ';';
                }
                countdown.push_back(*firstNow.reconnectSeconds);
            }
        }
        if (hostNow.canonical) {
            const auto participant = std::find_if(hostNow.canonical->participants.begin(),
                    hostNow.canonical->participants.end(), [&](const auto &value) {
                        return value.participantId == firstLobby.localParticipantId;
                    });
            hostObservedReservedParticipant = hostObservedReservedParticipant
                    || (participant != hostNow.canonical->participants.end()
                        && participant->connection == Network::Replication::ConnectionState::Reconnecting);
        }
        if (secondNow.journey != Client::NetworkJourney::Lobby)
            reconnectTrace << "peer-journey=" << static_cast<unsigned>(secondNow.journey) << ';';
        return reconnectObserved && sixPlayerLobby(firstNow)
               && firstNow.localParticipantId == firstLobby.localParticipantId
               && firstNow.canonical->sessionId == hostLobby.canonical->sessionId;
    });
    if (!restored) {
        const auto failed = first.snapshot();
        reconnectTrace << "final-journey=" << static_cast<unsigned>(failed.journey);
        if (failed.reconnectSeconds) reconnectTrace << ":seconds=" << *failed.reconnectSeconds;
        reconnectTrace << ":failure=" << failed.failure;
        if (failed.canonical)
            reconnectTrace << ":participants=" << failed.canonical->participants.size()
                           << ":players=" << failed.canonical->players.size();
        Duel6::Test::fail("restored", __FILE__, __LINE__, reconnectTrace.str());
    }
    D6R_REQUIRE(reconnectObserved);
    D6R_REQUIRE(hostObservedReservedParticipant);
    D6R_REQUIRE(!countdown.empty());
    if (!countdownMonotonic)
        Duel6::Test::fail("countdownMonotonic", __FILE__, __LINE__, reconnectTrace.str());

    const auto probeDeadline = std::chrono::steady_clock::now() + 1200ms;
    const auto probeStarted = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < probeDeadline) {
        host.update(); first.update(); second.update();
        const auto restored = first.snapshot();
        if (!sixPlayerLobby(restored)) {
            std::ostringstream details;
            details << "elapsed-ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - probeStarted).count()
                    << ";journey=" << static_cast<unsigned>(restored.journey)
                    << ";seconds=";
            if (restored.reconnectSeconds) details << *restored.reconnectSeconds;
            else details << "none";
            details << ";participants="
                    << (restored.canonical ? restored.canonical->participants.size() : 0)
                    << ";players=" << (restored.canonical ? restored.canonical->players.size() : 0)
                    << ";failure=" << restored.failure;
            Duel6::Test::fail("restored six-player lobby survives quality probes",
                              __FILE__, __LINE__, details.str());
        }
        D6R_REQUIRE(sixPlayerLobby(second.snapshot()));
        D6R_REQUIRE(participantReady(first.snapshot(), firstLobby.localParticipantId));
        std::this_thread::sleep_for(5ms);
    }

    host.setReady(true);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 5s, [&] {
        for (const auto *runtime: {&host, &first, &second}) {
            const auto snapshot = runtime->snapshot();
            if (!snapshot.canonical || snapshot.canonical->participants.size() != 3
                || snapshot.canonical->players.size() != 6
                || !std::all_of(snapshot.canonical->participants.begin(),
                                snapshot.canonical->participants.end(),
                                [](const auto &participant) { return participant.ready; })) return false;
        }
        return true;
    }));

    host.startMatch();
    D6R_REQUIRE(pumpRuntimes(host, first, second, 10s, [&] {
        for (const auto *runtime: {&host, &first, &second}) {
            const auto snapshot = runtime->snapshot();
            if (snapshot.journey != Client::NetworkJourney::Match || !snapshot.canonical
                || snapshot.canonical->phase != Network::Replication::Phase::ActiveRound
                || !snapshot.canonical->round || snapshot.canonical->round->roundNumber != 1
                || snapshot.canonical->round->level != "levels/duel_01.json"
                || snapshot.canonical->settings.roundLimit != 3
                || snapshot.canonical->players.size() != 6) return false;
        }
        return true;
    }));

    const auto activeBeforeRejectedSetup = host.snapshot();
    D6R_REQUIRE(activeBeforeRejectedSetup.canonical.has_value());
    host.updateHostSetup(setup);
    D6R_REQUIRE(pumpRuntimes(host, first, second, 250ms, [&] { return false; }) == false);
    for (const auto *runtime: {&host, &first, &second}) {
        const auto current = runtime->snapshot();
        D6R_REQUIRE(current.journey == Client::NetworkJourney::Match);
        D6R_REQUIRE(current.canonical.has_value());
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->sessionId, current.canonical->sessionId);
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->matchId, current.canonical->matchId);
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->round->roundId,
                       current.canonical->round->roundId);
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->settings.mode,
                       current.canonical->settings.mode);
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->settings.teamCount,
                       current.canonical->settings.teamCount);
        D6R_REQUIRE_EQ(activeBeforeRejectedSetup.canonical->settings.friendlyFire,
                       current.canonical->settings.friendlyFire);
        D6R_REQUIRE_EQ(6u, current.canonical->players.size());
    }

    char applicationName[] = "duel6r-network-session-runtime-tests";
    char *arguments[] = {applicationName};
    Application application(1, arguments);
    ScopedLevelVariant levelVariant(std::string(D6R_TEST_RESOURCE_DIR) + "/levels/duel_01.json");
    const auto &backgroundMap = application.gameResources.getBcgTextures().getTextures();
    std::vector<std::string> eligibleBackgrounds;
    for (const auto &entry: backgroundMap) eligibleBackgrounds.push_back(entry.first);
    std::sort(eligibleBackgrounds.begin(), eligibleBackgrounds.end());
    D6R_REQUIRE(!eligibleBackgrounds.empty());

    std::vector<std::unique_ptr<CanonicalWorldPresenter>> presenters;
    for (unsigned index = 0; index < 3; ++index)
        presenters.emplace_back(std::make_unique<CanonicalWorldPresenter>(
                *application.service, application.gameResources));
    const std::array<Client::NetworkRuntimeSnapshot, 3> active{
            host.snapshot(), first.snapshot(), second.snapshot()};
    for (std::size_t index = 1; index < active.size(); ++index) {
        D6R_REQUIRE_EQ(active[0].canonical->sessionId, active[index].canonical->sessionId);
        D6R_REQUIRE_EQ(active[0].canonical->matchId, active[index].canonical->matchId);
        D6R_REQUIRE_EQ(active[0].canonical->round->roundId, active[index].canonical->round->roundId);
        D6R_REQUIRE_EQ(active[0].canonical->round->level, active[index].canonical->round->level);
        D6R_REQUIRE(active[0].canonical->settings.levels == active[index].canonical->settings.levels);
    }

    const auto canonicalBeforePresentation = Network::Replication::serializeReplicationSnapshot(
            {1, *active[0].canonical, active[0].canonical->phaseTime});
    for (std::size_t index = 0; index < active.size(); ++index) {
        const auto &snapshot = active[index];
        D6R_REQUIRE(snapshot.canonical.has_value());
        presenters[index]->setCanonicalLevels(snapshot.canonical->settings.levels);
        presenters[index]->update(1.0f / 60.0f, &*snapshot.canonical, snapshot.presentationEvents);
        D6R_REQUIRE(presenters[index]->render(*snapshot.canonical, snapshot.presentation,
                                              snapshot.presentedPlayers, 1024, 768));
        D6R_REQUIRE(std::binary_search(eligibleBackgrounds.begin(), eligibleBackgrounds.end(),
                                      presenters[index]->backgroundIdentity()));
        D6R_REQUIRE_EQ(presenters[0]->backgroundIdentity(), presenters[index]->backgroundIdentity());
    }
    D6R_REQUIRE(canonicalBeforePresentation == Network::Replication::serializeReplicationSnapshot(
            {1, *active[0].canonical, active[0].canonical->phaseTime}));

    auto &mutableBackgrounds = const_cast<GameResources::BackgroundList::Container &>(backgroundMap);
    std::vector<std::pair<std::string, Texture>> backgroundEntries(
            mutableBackgrounds.begin(), mutableBackgrounds.end());
    mutableBackgrounds.clear();
    for (auto entry = backgroundEntries.rbegin(); entry != backgroundEntries.rend(); ++entry)
        mutableBackgrounds.emplace(entry->first, entry->second);
    CanonicalWorldPresenter reordered(*application.service, application.gameResources);
    reordered.setCanonicalLevels(active[0].canonical->settings.levels);
    reordered.update(1.0f / 60.0f, &*active[0].canonical, {});
    D6R_REQUIRE_EQ(presenters[0]->backgroundIdentity(), reordered.backgroundIdentity());

    Network::Replication::AuthoritativeStateReplicator publisher;
    D6R_REQUIRE(publisher.initialize(*active[0].canonical));
    const auto full = publisher.fullSnapshot();
    D6R_REQUIRE(full.has_value());
    Network::Replication::ReplicatedState replicated;
    D6R_REQUIRE(replicated.apply(*full) == Network::Replication::ApplyResult::Applied);
    auto incrementalState = *active[0].canonical;
    ++incrementalState.phaseTime;
    const auto incremental = publisher.publish(incrementalState);
    D6R_REQUIRE(incremental.has_value());
    D6R_REQUIRE(replicated.apply(*incremental) == Network::Replication::ApplyResult::Applied);
    replicated.requireResynchronization();
    D6R_REQUIRE(replicated.apply(*publisher.fullSnapshot()) == Network::Replication::ApplyResult::Applied);
    CanonicalWorldPresenter resynchronized(*application.service, application.gameResources);
    resynchronized.setCanonicalLevels(replicated.state()->settings.levels);
    resynchronized.update(1.0f / 60.0f, replicated.state(), {});
    D6R_REQUIRE_EQ(presenters[0]->backgroundIdentity(), resynchronized.backgroundIdentity());

    std::set<std::string> roundSelections;
    for (std::size_t index = 0; index < eligibleBackgrounds.size(); ++index) {
        auto varied = *active[0].canonical;
        varied.round->roundId = static_cast<Network::Replication::Identity>(index);
        CanonicalWorldPresenter variedPresenter(*application.service, application.gameResources);
        variedPresenter.setCanonicalLevels(varied.settings.levels);
        variedPresenter.update(1.0f / 60.0f, &varied, {});
        roundSelections.insert(variedPresenter.backgroundIdentity());
    }
    D6R_REQUIRE_EQ(eligibleBackgrounds.size(), roundSelections.size());

    levelVariant.write(eligibleBackgrounds.back());
    auto namedState = *active[0].canonical;
    ++namedState.round->roundId;
    CanonicalWorldPresenter named(*application.service, application.gameResources);
    named.setCanonicalLevels(namedState.settings.levels);
    named.update(1.0f / 60.0f, &namedState, {});
    D6R_REQUIRE_EQ(eligibleBackgrounds.back(), named.backgroundIdentity());
    D6R_REQUIRE(named.render(namedState, active[0].presentation, active[0].presentedPlayers, 1024, 768));

    levelVariant.write("not-a-local-background");
    ++namedState.round->roundId;
    CanonicalWorldPresenter unavailable(*application.service, application.gameResources);
    unavailable.setCanonicalLevels(namedState.settings.levels);
    unavailable.update(1.0f / 60.0f, &namedState, {});
    D6R_REQUIRE(std::binary_search(eligibleBackgrounds.begin(), eligibleBackgrounds.end(),
                                  unavailable.backgroundIdentity()));
    D6R_REQUIRE(unavailable.render(namedState, active[0].presentation,
                                   active[0].presentedPlayers, 1024, 768));

    const std::string removedIdentity = unavailable.backgroundIdentity();
    const Texture removedTexture = mutableBackgrounds.at(removedIdentity);
    mutableBackgrounds.erase(removedIdentity);
    D6R_REQUIRE_THROW(unavailable.backgroundTexture(), DataException);
    mutableBackgrounds.emplace(removedIdentity, removedTexture);

    first.leave(); second.leave();
    (void) pumpRuntimes(host, first, second, 3s, [&] {
        return first.snapshot().journey == Client::NetworkJourney::Inactive
               && second.snapshot().journey == Client::NetworkJourney::Inactive;
    });
    host.endSession();
}
#endif
