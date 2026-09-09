#ifndef DUEL6_CLIENT_NETWORKSESSIONRUNTIME_H
#define DUEL6_CLIENT_NETWORKSESSIONRUNTIME_H

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "HostServiceSupervisor.h"
#include "../input/PlayerControls.h"
#include "../network/HostCompositionProtocol.h"
#include "../network/NetworkResponsiveness.h"
#include "../network/PlayerInputProtocol.h"
#include "../network/Protocol.h"
#include "../network/SessionLifecycle.h"
#include "../network/StateReplication.h"
#include "../network/StateReplicationProtocol.h"

namespace Duel6::Client {
    enum class NetworkJourney { Inactive, Starting, Cancelling, Lobby, Match, Summary,
                                Reconnecting, Failure, HostEnded };

    enum class NetworkRetryBlockReason {
        None,
        CleanupInProgress,
        InvalidSetup,
        RestartRequired,
        EndedSession,
        TerminalReconnect
    };

    struct NetworkLocalPlayer {
        std::string name;
        const PlayerControls *controls = nullptr;
        std::string controlDescription;
    };

    struct NetworkRuntimeSnapshot {
        NetworkJourney journey = NetworkJourney::Inactive;
        bool host = false;
        Network::Replication::Identity localParticipantId = 0;
        Network::Endpoint endpoint;
        std::optional<Network::Replication::CanonicalState> canonical;
        Network::Responsiveness::ConnectionPresentationState presentation;
        std::vector<Network::Responsiveness::PresentedPlayerPose> presentedPlayers;
        std::vector<Network::Replication::PresentationEvent> presentationEvents;
        std::optional<unsigned> reconnectSeconds;
        std::string status;
        std::string failure;
        bool retryAllowed = false;
        NetworkRetryBlockReason retryBlockReason = NetworkRetryBlockReason::None;
    };

    class NetworkSessionRuntime final {
    public:
        NetworkSessionRuntime();
        ~NetworkSessionRuntime();
        bool startHost(const Network::Endpoint &endpoint, const std::string &serverExecutable,
                       const std::string &resourcePath,
                       const Network::HostComposition::Setup &setup,
                       std::vector<NetworkLocalPlayer> players);
        bool join(const Network::Endpoint &endpoint, const std::string &resourcePath,
                  std::vector<NetworkLocalPlayer> players);
        void cancel();
        void leave();
        void endSession();
        void setReady(bool ready);
        void startMatch();
        void returnToLobby();
        void advanceRound();
        void updateHostSetup(const Network::HostComposition::Setup &setup);
        void rebindLocalPlayers(std::vector<NetworkLocalPlayer> players);
        void ownedPersonsChanged();
        void localConfigurationChanged();
        void moveRosterPlayer(Network::Replication::Identity playerId, int direction);
        void update();
        NetworkRuntimeSnapshot snapshot() const;
        void reset();

    private:
        mutable std::mutex mutex;
        NetworkRuntimeSnapshot current;
        std::vector<NetworkLocalPlayer> players;
        std::vector<std::uint32_t> sampledActions;
        std::map<Network::Replication::Identity, std::size_t> ownedPlayerBindings;
        std::deque<std::vector<std::uint8_t>> pendingGuestCommands;
        std::deque<std::vector<std::uint8_t>> pendingHostCommands;
        bool pendingHostEnd = false;
        std::unique_ptr<Network::Input::ClientCommandSession> hostInput;
        std::unique_ptr<Network::Replication::ClientReplicationConnection> hostPresentation;
        std::optional<std::uint64_t> submittedHostTick;
        std::unique_ptr<HostServiceSupervisor> supervisor;
        std::thread guestWorker;
        std::atomic<bool> guestCancelled{false};

        void receiveHostPayload(const std::vector<std::uint8_t> &payload);
        void observeHostLifecycle(const HostServiceSnapshot &snapshot);
        void applyCanonical(const Network::Replication::CanonicalState &state,
                            const Network::Responsiveness::ConnectionPresentationState &presentation = {},
                            std::vector<Network::Responsiveness::PresentedPlayerPose> presentedPlayers = {},
                            std::vector<Network::Replication::PresentationEvent> events = {});
        void applyCanonicalLocked(const Network::Replication::CanonicalState &state,
                                  const Network::Responsiveness::ConnectionPresentationState &presentation,
                                  std::vector<Network::Responsiveness::PresentedPlayerPose> presentedPlayers,
                                  std::vector<Network::Replication::PresentationEvent> events);
        std::uint32_t sampleActionsOnInputThread(std::size_t binding) const;
        void sendHostAction(Network::HostComposition::Kind kind);
        void enqueueGuestAction(Network::Lifecycle::ParticipantActionKind kind);
        void enqueueHostCommand(std::vector<std::uint8_t> payload);
        void drainHostCommands();
        void stopGuest();
    };
}

#endif
