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
#include "source/server/AuthoritativeMatchSerialization.h"

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

#ifndef _WIN32
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
            player.presentationAlpha = invisible ? 51 : predator ? 0 : 255;
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
