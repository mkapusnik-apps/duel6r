#include "NetworkSessionRuntime.h"

#include <algorithm>
#include <sstream>

#include "../network/StateReplicationProtocol.h"
#include "../network/PlayerInputProtocol.h"
#include "../server/HeadlessServer.h"
#include "../server/ServerConfig.h"

namespace Duel6::Client {
    namespace {
        NetworkJourney journeyFor(const Network::Replication::CanonicalState &state) {
            switch (state.phase) {
                case Network::Replication::Phase::Lobby: return NetworkJourney::Lobby;
                case Network::Replication::Phase::ActiveRound:
                case Network::Replication::Phase::RoundSummary: return NetworkJourney::Match;
                case Network::Replication::Phase::FinalSummary: return NetworkJourney::Summary;
                case Network::Replication::Phase::Ended: return NetworkJourney::HostEnded;
            }
            return NetworkJourney::Failure;
        }

        std::string finalLine(const std::string &value) {
            std::istringstream input(value); std::string line, result;
            while (std::getline(input, line)) if (!line.empty()) result = line;
            if (result.find('=') != std::string::npos || result.find("-id") != std::string::npos
                || (result.find('.') == std::string::npos && result.find('!') == std::string::npos)) return {};
            return result;
        }
    }

    NetworkSessionRuntime::NetworkSessionRuntime() = default;
    NetworkSessionRuntime::~NetworkSessionRuntime() { reset(); }

    bool NetworkSessionRuntime::startHost(
            const Network::Endpoint &endpoint, const std::string &serverExecutable,
            const std::string &resourcePath, const Network::HostComposition::Setup &setup,
            std::vector<NetworkLocalPlayer> localPlayers) {
        reset();
        HostServiceDependencies dependencies;
        dependencies.lifecycleObserver = [this](const auto &value) { observeHostLifecycle(value); };
        dependencies.sessionPayloadObserver = [this](const auto &payload) { receiveHostPayload(payload); };
        supervisor = std::make_unique<HostServiceSupervisor>(std::move(dependencies));
        {
            std::lock_guard<std::mutex> lock(mutex);
            players = std::move(localPlayers);
            sampledActions.assign(players.size(), 0);
            current = {}; current.host = true; current.endpoint = endpoint;
            current.journey = NetworkJourney::Starting;
            current.status = "Starting session…";
        }
        HostServiceStartConfig config;
        config.serverExecutable = serverExecutable; config.endpoint = endpoint;
        config.resourcePath = resourcePath; config.localPlayers = static_cast<std::uint8_t>(players.size());
        config.graphicalComposition = true;
        if (!supervisor->start(config)) { reset(); return false; }
        try {
            if (!supervisor->sendSessionPayload(Network::HostComposition::serializeSetup(setup))) {
                supervisor->cancelStartup(); return false;
            }
        } catch (...) { supervisor->cancelStartup(); return false; }
        return true;
    }

    bool NetworkSessionRuntime::join(const Network::Endpoint &endpoint, const std::string &resourcePath,
                                     std::vector<NetworkLocalPlayer> localPlayers) {
        reset();
        if (localPlayers.empty() || localPlayers.size() > Network::MaxNetworkPlayers) return false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            players = std::move(localPlayers); current = {}; current.endpoint = endpoint;
            sampledActions.assign(players.size(), 0);
            current.status = "Connecting to " + endpoint.host + ':' + std::to_string(endpoint.port) + "…";
            current.journey = NetworkJourney::Starting;
        }
        guestCancelled = false;
        guestWorker = std::thread([this, endpoint, resourcePath] {
            Server::ServerConfig config; config.admissionClient = true; config.listenEndpoint = endpoint;
            config.resourcePath = resourcePath; config.localPlayers = static_cast<std::uint8_t>(players.size());
            for (const auto &player: players) config.localPlayerNames.push_back(player.name);
            Server::AdmissionRuntimeDependencies dependencies;
            dependencies.cancelled = [this] { return guestCancelled.load(); };
            dependencies.guestAdmission = [this](auto participant, const auto &playerIds) {
                std::lock_guard<std::mutex> lock(mutex);
                current.localParticipantId = participant;
                ownedPlayerBindings.clear();
                for (std::size_t index = 0; index < playerIds.size() && index < players.size(); ++index)
                    ownedPlayerBindings[playerIds[index]] = index;
                (void) participant;
            };
            dependencies.localPlayerActions = [this](std::uint64_t playerId) {
                std::lock_guard<std::mutex> lock(mutex);
                const auto found = ownedPlayerBindings.find(playerId);
                return found == ownedPlayerBindings.end() || found->second >= sampledActions.size()
                       ? 0u : sampledActions[found->second];
            };
            dependencies.localParticipantAction = [this]() {
                std::lock_guard<std::mutex> lock(mutex);
                auto value = pendingGuestAction; pendingGuestAction.reset(); return value;
            };
            dependencies.guestPresentation = [this](const auto &state, const auto &presentation,
                                                     const auto &poses, const auto &events) {
                applyCanonical(state, presentation, poses, events);
            };
            dependencies.guestRecoveryPresentation = [this](auto journey, auto seconds, std::string_view failure) {
                std::lock_guard<std::mutex> lock(mutex);
                current.reconnectSeconds = seconds;
                if (journey == Network::Lifecycle::GuestJourney::HostEnded)
                    current.journey = NetworkJourney::HostEnded;
                else if (journey == Network::Lifecycle::GuestJourney::ConnectionFailure) {
                    current.journey = NetworkJourney::Failure; current.failure = std::string(failure);
                } else if (journey == Network::Lifecycle::GuestJourney::Reconnecting)
                    current.journey = NetworkJourney::Reconnecting;
            };
            std::ostringstream output;
            const int result = Server::HeadlessServer(config, std::move(dependencies)).run(output);
            std::lock_guard<std::mutex> lock(mutex);
            if (guestCancelled.load() || current.journey == NetworkJourney::Cancelling) {
                current = {}; return;
            }
            if (current.journey != NetworkJourney::HostEnded && current.journey != NetworkJourney::Inactive) {
                current.journey = NetworkJourney::Failure;
                if (current.failure.empty()) current.failure = finalLine(output.str());
                if (current.failure.empty()) current.failure = result == 0 ? "Connection ended." : "Connection timed out.";
                current.retryAllowed = current.reconnectSeconds.has_value() ? false : true;
            }
        });
        return true;
    }

    void NetworkSessionRuntime::applyCanonical(
            const Network::Replication::CanonicalState &state,
            const Network::Responsiveness::ConnectionPresentationState &presentation,
            std::vector<Network::Responsiveness::PresentedPlayerPose> presentedPlayers,
            std::vector<Network::Replication::PresentationEvent> events) {
        std::lock_guard<std::mutex> lock(mutex);
        current.canonical = state; current.presentation = presentation;
        current.presentedPlayers = std::move(presentedPlayers);
        current.presentationEvents = std::move(events);
        if (current.host) current.localParticipantId = state.hostParticipantId;
        current.journey = journeyFor(state); current.status = "Connected";
    }

    void NetworkSessionRuntime::receiveHostPayload(const std::vector<std::uint8_t> &payload) {
        const auto message = Network::HostComposition::deserialize(payload);
        if (!message) return;
        if (message->kind == Network::HostComposition::Kind::CanonicalSnapshot) {
            const auto frame = Network::Replication::deserializeReplicationFrame(message->payload);
            if (frame && frame->snapshot) applyCanonical(frame->snapshot->state);
        } else if (message->kind == Network::HostComposition::Kind::PlayerInputOutcome) {
            std::lock_guard<std::mutex> lock(mutex);
            if (hostInput) (void) hostInput->receive(message->payload);
        }
    }

    void NetworkSessionRuntime::observeHostLifecycle(const HostServiceSnapshot &value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (value.state == HostServiceState::Stopping) {
            current.journey = NetworkJourney::Cancelling; current.status = "Cancelling session…";
        } else if (value.state == HostServiceState::StartupFailed || value.state == HostServiceState::SessionFailed) {
            current.journey = NetworkJourney::Failure; current.failure = hostServiceOutcomeCopy(value.outcome);
            current.retryAllowed = value.retryAllowed;
        } else if (value.state == HostServiceState::NoService
                   && current.journey == NetworkJourney::Cancelling) {
            current = {};
        }
    }

    std::uint32_t NetworkSessionRuntime::sampleActionsOnInputThread(std::size_t binding) const {
        if (binding >= players.size() || !players[binding].controls) return 0;
        const auto &c = *players[binding].controls;
        return (c.getLeft().isPressed() ? Network::Input::MoveLeft : 0u)
               | (c.getRight().isPressed() ? Network::Input::MoveRight : 0u)
               | (c.getUp().isPressed() ? Network::Input::Jump : 0u)
               | (c.getDown().isPressed() ? Network::Input::Crouch : 0u)
               | (c.getShoot().isPressed() ? Network::Input::Shoot : 0u)
               | (c.getPick().isPressed() ? Network::Input::PickOrSwapWeapon : 0u)
               | (c.getStatus().isPressed() ? Network::Input::ShowStatus : 0u);
    }

    void NetworkSessionRuntime::update() {
        std::vector<std::pair<Network::Replication::Identity, std::size_t>> bindings;
        std::uint64_t tick = 0; Network::Replication::Identity participant = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (std::size_t index = 0; index < players.size(); ++index)
                sampledActions[index] = sampleActionsOnInputThread(index);
            if (!current.host || !current.canonical
                || current.canonical->phase != Network::Replication::Phase::ActiveRound
                || (submittedHostTick && *submittedHostTick == current.canonical->phaseTime)) return;
            tick = current.canonical->phaseTime; submittedHostTick = tick;
            participant = current.canonical->hostParticipantId;
            const auto found = std::find_if(current.canonical->participants.begin(), current.canonical->participants.end(),
                                            [participant](const auto &p) { return p.participantId == participant; });
            if (found == current.canonical->participants.end()) return;
            for (std::size_t index = 0; index < found->ownedPlayerIds.size() && index < players.size(); ++index)
                bindings.emplace_back(found->ownedPlayerIds[index], index);
            if (!hostInput) {
                std::vector<Network::Input::Identity> ids;
                for (const auto &binding: bindings) ids.push_back(binding.first);
                hostInput = std::make_unique<Network::Input::ClientCommandSession>(participant, std::move(ids),
                        [this](std::vector<std::uint8_t> payload) {
                            try {
                                return supervisor && supervisor->sendSessionPayload(
                                        Network::HostComposition::serializePayload(
                                                Network::HostComposition::Kind::PlayerInput, payload))
                                       ? Network::SendResult::Accepted : Network::SendResult::NotConnected;
                            } catch (...) { return Network::SendResult::NotConnected; }
                        });
            }
        }
        for (const auto &binding: bindings) {
            std::lock_guard<std::mutex> lock(mutex);
            const auto actions = binding.second < sampledActions.size() ? sampledActions[binding.second] : 0u;
            if (hostInput) (void) hostInput->submit(binding.first, tick, actions);
        }
    }

    void NetworkSessionRuntime::sendHostAction(Network::HostComposition::Kind kind) {
        try { if (supervisor) (void) supervisor->sendSessionPayload(Network::HostComposition::serializeAction(kind)); }
        catch (...) {}
    }
    void NetworkSessionRuntime::setReady(bool ready) {
        if (supervisor) (void) supervisor->setSessionReady(ready);
        else { std::lock_guard<std::mutex> lock(mutex); pendingGuestAction = ready
                ? Network::Lifecycle::ParticipantActionKind::Ready
                : Network::Lifecycle::ParticipantActionKind::NotReady; }
    }
    void NetworkSessionRuntime::startMatch() { sendHostAction(Network::HostComposition::Kind::StartMatch); }
    void NetworkSessionRuntime::returnToLobby() { sendHostAction(Network::HostComposition::Kind::ReturnToLobby); }
    void NetworkSessionRuntime::advanceRound() { sendHostAction(Network::HostComposition::Kind::AdvanceRound); }
    void NetworkSessionRuntime::updateHostSetup(const Network::HostComposition::Setup &setup) {
        try { if (supervisor) (void) supervisor->sendSessionPayload(
                Network::HostComposition::serializeSetupUpdate(setup)); } catch (...) {}
    }
    void NetworkSessionRuntime::rebindLocalPlayers(std::vector<NetworkLocalPlayer> localPlayers) {
        std::lock_guard<std::mutex> lock(mutex);
        if (localPlayers.size() != players.size()) return;
        players = std::move(localPlayers);
        sampledActions.assign(players.size(), 0);
    }
    void NetworkSessionRuntime::localConfigurationChanged() {
        if (supervisor) sendHostAction(Network::HostComposition::Kind::ConfigurationChanged);
        else {
            std::lock_guard<std::mutex> lock(mutex);
            pendingGuestAction = Network::Lifecycle::ParticipantActionKind::ConfigurationChanged;
        }
    }
    void NetworkSessionRuntime::moveRosterPlayer(Network::Replication::Identity playerId, int direction) {
        try {
            if (supervisor) (void) supervisor->sendSessionPayload(
                    Network::HostComposition::serializeRosterMove(
                            playerId, static_cast<std::int8_t>(direction)));
        } catch (...) {}
    }
    void NetworkSessionRuntime::cancel() {
        { std::lock_guard<std::mutex> lock(mutex); current.journey = NetworkJourney::Cancelling;
          current.status = "Cancelling session…"; current.failure.clear(); }
        if (supervisor) (void) supervisor->cancelStartup(); else guestCancelled = true;
    }
    void NetworkSessionRuntime::leave() {
        if (supervisor) return;
        std::lock_guard<std::mutex> lock(mutex);
        pendingGuestAction = Network::Lifecycle::ParticipantActionKind::Leave;
        current.journey = NetworkJourney::Cancelling; current.status = "Leaving session…";
    }
    void NetworkSessionRuntime::endSession() {
        { std::lock_guard<std::mutex> lock(mutex); current.journey = NetworkJourney::Cancelling;
          current.status = "Ending session…"; }
        if (supervisor) (void) supervisor->endSession();
    }
    NetworkRuntimeSnapshot NetworkSessionRuntime::snapshot() const {
        std::lock_guard<std::mutex> lock(mutex); return current;
    }
    void NetworkSessionRuntime::stopGuest() {
        guestCancelled = true;
        if (guestWorker.joinable()) guestWorker.join();
    }
    void NetworkSessionRuntime::reset() {
        stopGuest();
        if (supervisor) { supervisor->applicationExit(); supervisor.reset(); }
        std::lock_guard<std::mutex> lock(mutex);
        current = {}; players.clear(); sampledActions.clear(); ownedPlayerBindings.clear(); pendingGuestAction.reset();
        hostInput.reset(); submittedHostTick.reset();
    }
}
