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
#undef private

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
D6R_TEST_CASE("NET-AC-006 NET-AC-009 NET-AC-017 three NetworkSessionRuntime participants converge Ready and survive authenticated reconnect probes") {
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
