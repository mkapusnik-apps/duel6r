#include "NetworkSessionRuntime.h"

#include <algorithm>
#include <limits>
#include <sstream>

#include "../network/StateReplicationProtocol.h"
#include "../network/PlayerInputProtocol.h"
#include "../network/NetworkTrustPolicy.h"
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

        std::int64_t saturatedAdd(std::int64_t left, std::int64_t right) noexcept {
            if (right > 0 && left > (std::numeric_limits<std::int64_t>::max)() - right)
                return (std::numeric_limits<std::int64_t>::max)();
            if (right < 0 && left < (std::numeric_limits<std::int64_t>::min)() - right)
                return (std::numeric_limits<std::int64_t>::min)();
            return left + right;
        }
    }

    NetworkSessionRuntime::NetworkSessionRuntime() = default;
    NetworkSessionRuntime::~NetworkSessionRuntime() { reset(); }

    bool NetworkSessionRuntime::startHost(
            const Network::Endpoint &endpoint, const std::string &serverExecutable,
            const std::string &resourcePath, const Network::HostComposition::Setup &setup,
            std::vector<NetworkLocalPlayer> localPlayers) {
        std::vector<std::uint8_t> setupPayload;
        try { setupPayload = Network::HostComposition::serializeSetup(setup); }
        catch (...) { return false; }
        if (localPlayers.size() != setup.localPlayerNames.size()
            || !std::all_of(localPlayers.begin(), localPlayers.end(), [](const auto &player) {
                return Network::Trust::validParticipantName(player.name);
            })) return false;
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
        if (!supervisor->sendSessionPayload(std::move(setupPayload))) {
            supervisor->cancelStartup(); return false;
        }
        return true;
    }

    bool NetworkSessionRuntime::join(const Network::Endpoint &endpoint, const std::string &resourcePath,
                                     std::vector<NetworkLocalPlayer> localPlayers) {
        if (localPlayers.empty() || localPlayers.size() > Network::MaxNetworkPlayers
            || !std::all_of(localPlayers.begin(), localPlayers.end(), [](const auto &player) {
                return Network::Trust::validParticipantName(player.name);
            })) return false;
        reset();
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
            dependencies.localParticipantCommand = [this]() {
                std::lock_guard<std::mutex> lock(mutex);
                return pendingGuestCommands.empty()
                       ? std::optional<std::vector<std::uint8_t>>{}
                       : std::optional<std::vector<std::uint8_t>>{pendingGuestCommands.front()};
            };
            dependencies.localParticipantCommandAccepted = [this]() {
                std::lock_guard<std::mutex> lock(mutex);
                if (!pendingGuestCommands.empty()) pendingGuestCommands.pop_front();
            };
            dependencies.guestAdmissionOutcome = [this](auto outcome, bool local) {
                std::lock_guard<std::mutex> lock(mutex);
                if (local && outcome == Network::AdmissionResultCode::GameplayContentManifestInvalid) {
                    current.retryAllowed = false;
                    current.retryBlockReason = NetworkRetryBlockReason::RestartRequired;
                }
            };
            dependencies.guestPresentation = [this](const auto &state, const auto &presentation,
                                                     const auto &poses, const auto &events) {
                applyCanonical(state, presentation, poses, events);
            };
            dependencies.guestRecoveryPresentation = [this](auto journey, auto seconds, std::string_view failure) {
                std::lock_guard<std::mutex> lock(mutex);
                current.reconnectSeconds = seconds;
                if (journey == Network::Lifecycle::GuestJourney::HostEnded) {
                    current.retainReconnectContext = current.journey == NetworkJourney::Reconnecting;
                    current.journey = NetworkJourney::HostEnded;
                } else if (journey == Network::Lifecycle::GuestJourney::ConnectionFailure) {
                    current.journey = NetworkJourney::Failure; current.failure = std::string(failure);
                    current.retryAllowed = false;
                    current.retryBlockReason = NetworkRetryBlockReason::TerminalReconnect;
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
                if (current.retryBlockReason == NetworkRetryBlockReason::TerminalReconnect) {
                    current.retryAllowed = false;
                } else if (current.retryBlockReason != NetworkRetryBlockReason::RestartRequired) {
                    current.retryAllowed = true;
                    current.retryBlockReason = NetworkRetryBlockReason::None;
                }
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
        applyCanonicalLocked(state, presentation, std::move(presentedPlayers), std::move(events));
    }

    void NetworkSessionRuntime::applyCanonicalLocked(
            const Network::Replication::CanonicalState &state,
            const Network::Responsiveness::ConnectionPresentationState &presentation,
            std::vector<Network::Responsiveness::PresentedPlayerPose> presentedPlayers,
            std::vector<Network::Replication::PresentationEvent> events) {
        current.canonical = state; current.presentation = presentation;
        current.presentedPlayers = std::move(presentedPlayers);
        current.presentationEvents.insert(current.presentationEvents.end(), events.begin(), events.end());
        if (current.presentationEvents.size() > Network::Replication::MaxReplicatedEvents) {
            current.presentationEvents.erase(current.presentationEvents.begin(),
                    current.presentationEvents.end() - Network::Replication::MaxReplicatedEvents);
        }
        if (current.host) current.localParticipantId = state.hostParticipantId;
        current.journey = journeyFor(state); current.status = "Connected";
    }

    void NetworkSessionRuntime::receiveHostPayload(const std::vector<std::uint8_t> &payload) {
        const auto message = Network::HostComposition::deserialize(payload);
        if (!message) return;
        if (message->kind == Network::HostComposition::Kind::CanonicalSnapshot) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!hostPresentation) {
                hostPresentation = std::make_unique<Network::Replication::ClientReplicationConnection>(
                        [](std::vector<std::uint8_t>) { return Network::SendResult::Accepted; },
                        Network::Responsiveness::Environment::SameMachine);
            }
            const auto acceptedAt = Network::Responsiveness::Clock::now();
            const auto result = hostPresentation->receive(message->payload, acceptedAt);
            if (result != Network::Replication::ClientReplicationResult::Applied) return;
            (void) hostPresentation->observeNetworkSample({std::chrono::milliseconds::zero(), 1, 0}, acceptedAt);
            const auto *state = hostPresentation->replicatedState().retainedState();
            if (!state) return;
            const auto host = std::find_if(state->participants.begin(), state->participants.end(),
                    [state](const auto &participant) { return participant.participantId == state->hostParticipantId; });
            if (host != state->participants.end()) {
                hostPresentation->setLocallyControlledPlayers(
                        std::set<Network::Replication::Identity>(host->ownedPlayerIds.begin(), host->ownedPlayerIds.end()));
            }
            applyCanonicalLocked(*state, hostPresentation->presentationState(acceptedAt),
                    hostPresentation->presentedPlayers(acceptedAt), hostPresentation->takePresentationEvents());
        } else if (message->kind == Network::HostComposition::Kind::PlayerInputOutcome) {
            std::lock_guard<std::mutex> lock(mutex);
            if (hostInput) (void) hostInput->receive(message->payload);
        }
    }

    void NetworkSessionRuntime::observeHostLifecycle(const HostServiceSnapshot &value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (value.state == HostServiceState::Stopping) {
            if (value.stopReason == HostServiceStopReason::StartupFailure
                || value.stopReason == HostServiceStopReason::SessionFailure) {
                current.journey = NetworkJourney::Failure;
                current.failure = value.stopReason == HostServiceStopReason::SessionFailure
                                  ? "Hosted session stopped unexpectedly."
                                  : "Hosted session could not start.";
                current.retryAllowed = false;
                current.retryBlockReason = NetworkRetryBlockReason::CleanupInProgress;
            } else {
                current.journey = NetworkJourney::Cancelling; current.status = "Cancelling session…";
            }
        } else if (value.state == HostServiceState::StartupFailed || value.state == HostServiceState::SessionFailed) {
            current.journey = NetworkJourney::Failure; current.failure = hostServiceOutcomeCopy(value.outcome);
            current.retryAllowed = value.retryAllowed;
            current.retryBlockReason = value.retryAllowed ? NetworkRetryBlockReason::None
                    : value.outcome == HostServiceOutcome::HostManifestInvalid
                      ? NetworkRetryBlockReason::RestartRequired
                      : value.state == HostServiceState::SessionFailed
                        ? NetworkRetryBlockReason::EndedSession
                        : NetworkRetryBlockReason::InvalidSetup;
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
        drainHostCommands();
        std::vector<std::pair<Network::Replication::Identity, std::size_t>> bindings;
        std::uint64_t tick = 0; Network::Replication::Identity participant = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (std::size_t index = 0; index < players.size(); ++index)
                sampledActions[index] = sampleActionsOnInputThread(index);
            if (current.host && hostPresentation && current.canonical) {
                const auto now = Network::Responsiveness::Clock::now();
                current.presentation = hostPresentation->presentationState(now);
                current.presentedPlayers = hostPresentation->presentedPlayers(now);
            }
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
            if (hostPresentation && current.canonical) {
                const auto player = std::find_if(current.canonical->players.begin(), current.canonical->players.end(),
                        [&](const auto &value) { return value.playerId == binding.first; });
                if (player != current.canonical->players.end()) {
                    Network::Responsiveness::PresentedPlayerPose predicted{
                            player->playerId, saturatedAdd(player->positionX, player->velocityX),
                            saturatedAdd(player->positionY, player->velocityY), player->facingLeft, player->crouching};
                    const bool left = (actions & Network::Input::MoveLeft) != 0;
                    const bool right = (actions & Network::Input::MoveRight) != 0;
                    if (left != right) predicted.facingLeft = left;
                    predicted.crouching = (actions & Network::Input::Crouch) != 0;
                    (void) hostPresentation->predictLocalMovement(
                            predicted, Network::Responsiveness::Clock::now());
                }
            }
            if (hostInput) (void) hostInput->submit(binding.first, tick, actions);
        }
    }

    void NetworkSessionRuntime::sendHostAction(Network::HostComposition::Kind kind) {
        try { if (supervisor) enqueueHostCommand(Network::HostComposition::serializeAction(kind)); }
        catch (...) {}
    }
    void NetworkSessionRuntime::enqueueGuestAction(Network::Lifecycle::ParticipantActionKind kind) {
        std::lock_guard<std::mutex> lock(mutex);
        pendingGuestCommands.push_back(Network::Lifecycle::serializeParticipantAction({
                current.canonical ? current.canonical->sessionId : 0, current.localParticipantId, kind}));
    }
    void NetworkSessionRuntime::enqueueHostCommand(std::vector<std::uint8_t> payload) {
        std::lock_guard<std::mutex> lock(mutex);
        pendingHostCommands.push_back(std::move(payload));
    }
    void NetworkSessionRuntime::drainHostCommands() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!supervisor) return;
        if (!pendingHostCommands.empty()) {
            if (supervisor->sendSessionPayload(pendingHostCommands.front())) pendingHostCommands.pop_front();
            return;
        }
        if (pendingHostEnd && supervisor->endSession()) pendingHostEnd = false;
    }
    void NetworkSessionRuntime::setReady(bool ready) {
        if (supervisor) sendHostAction(ready ? Network::HostComposition::Kind::Ready
                                            : Network::HostComposition::Kind::NotReady);
        else enqueueGuestAction(ready ? Network::Lifecycle::ParticipantActionKind::Ready
                                      : Network::Lifecycle::ParticipantActionKind::NotReady);
    }
    void NetworkSessionRuntime::startMatch() { sendHostAction(Network::HostComposition::Kind::StartMatch); }
    void NetworkSessionRuntime::returnToLobby() { sendHostAction(Network::HostComposition::Kind::ReturnToLobby); }
    void NetworkSessionRuntime::advanceRound() { sendHostAction(Network::HostComposition::Kind::AdvanceRound); }
    void NetworkSessionRuntime::updateHostSetup(const Network::HostComposition::Setup &setup) {
        try { if (supervisor) enqueueHostCommand(Network::HostComposition::serializeSetupUpdate(setup)); }
        catch (...) {}
    }
    void NetworkSessionRuntime::rebindLocalPlayers(std::vector<NetworkLocalPlayer> localPlayers) {
        std::lock_guard<std::mutex> lock(mutex);
        if (localPlayers.size() != players.size()
            || !std::all_of(localPlayers.begin(), localPlayers.end(), [](const auto &player) {
                return Network::Trust::validParticipantName(player.name);
            })) return;
        players = std::move(localPlayers);
        sampledActions.assign(players.size(), 0);
    }
    void NetworkSessionRuntime::ownedPersonsChanged() {
        std::vector<std::string> names;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto &player: players) {
                if (!Network::Trust::validParticipantName(player.name)) return;
                names.push_back(player.name);
            }
            if (!supervisor) {
                pendingGuestCommands.push_back(Network::HostComposition::serializeOwnedPersons(names));
                return;
            }
        }
        try { if (supervisor) enqueueHostCommand(Network::HostComposition::serializeOwnedPersons(names)); }
        catch (...) {}
    }
    void NetworkSessionRuntime::localConfigurationChanged() {
        if (supervisor) sendHostAction(Network::HostComposition::Kind::ConfigurationChanged);
        else enqueueGuestAction(Network::Lifecycle::ParticipantActionKind::ConfigurationChanged);
    }
    void NetworkSessionRuntime::moveRosterPlayer(Network::Replication::Identity playerId, int direction) {
        try {
            if (supervisor) enqueueHostCommand(Network::HostComposition::serializeRosterMove(
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
        enqueueGuestAction(Network::Lifecycle::ParticipantActionKind::Leave);
        std::lock_guard<std::mutex> lock(mutex);
        current.journey = NetworkJourney::Cancelling; current.status = "Leaving session…";
    }
    void NetworkSessionRuntime::endSession() {
        { std::lock_guard<std::mutex> lock(mutex); current.journey = NetworkJourney::Cancelling;
          current.status = "Ending session…"; pendingHostEnd = supervisor != nullptr; }
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
        current = {}; players.clear(); sampledActions.clear(); ownedPlayerBindings.clear();
        pendingGuestCommands.clear(); pendingHostCommands.clear();
        pendingHostEnd = false;
        hostInput.reset(); hostPresentation.reset(); submittedHostTick.reset();
    }
}
