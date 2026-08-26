#ifndef DUEL6_NETWORK_PROTOCOL_H
#define DUEL6_NETWORK_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Duel6::Network {
    static constexpr std::uint16_t ProtocolVersion = 1;
    static constexpr std::uint16_t DefaultServerPort = 26660;
    static constexpr const char *PrototypeBuildVersion = "network-scaffold-1";
    static constexpr std::size_t MaxPayloadBytes = 1024 * 1024;
    static constexpr std::size_t MaxProtocolProperties = 4096;
    static constexpr std::size_t MaxProtocolKeyBytes = 128;
    static constexpr std::size_t MaxProtocolStringBytes = 4096;
    static constexpr std::size_t MaxProtocolCollectionEntries = 256;
    static constexpr std::size_t MaxNetworkPlayers = 15;

    enum class ConnectionMode {
        LocalGame,
        RemoteServer
    };

    enum class MessageType : std::uint16_t {
        HandshakeRequest = 1,
        HandshakeAccept = 2,
        HandshakeReject = 3,
        LobbyState = 10,
        InputCommand = 20,
        Snapshot = 30,
        Event = 40,
        Disconnect = 50
    };

    enum class RejectReason : std::uint16_t {
        None = 0,
        IncompatibleProtocol = 1,
        IncompatibleBuild = 2,
        ContentMismatch = 3,
        ServerFull = 4,
        AuthenticationFailed = 5,
        MatchInProgress = 6,
        InvalidRequest = 7
    };

    enum InputAction : std::uint32_t {
        MoveLeft = 1u << 0u,
        MoveRight = 1u << 1u,
        Jump = 1u << 2u,
        Crouch = 1u << 3u,
        Shoot = 1u << 4u,
        PickOrSwapWeapon = 1u << 5u,
        ShowStatus = 1u << 6u
    };

    struct ResourceHash {
        std::string path;
        std::string hash;
    };

    struct Endpoint {
        std::string host = "127.0.0.1";
        std::uint16_t port = DefaultServerPort;
    };

    struct HandshakeRequest {
        std::uint16_t protocolVersion = ProtocolVersion;
        std::string buildVersion = PrototypeBuildVersion;
        std::string clientName;
        std::string authToken;
        std::vector<ResourceHash> resources;
    };

    struct HandshakeAccept {
        std::uint32_t clientId = 0;
        std::uint32_t serverTickRate = 60;
        std::string serverName;
    };

    struct HandshakeReject {
        RejectReason reason = RejectReason::InvalidRequest;
        std::string detail;
    };

    struct LobbyPlayer {
        std::uint32_t playerId = 0;
        std::uint32_t clientId = 0;
        std::string displayName;
        std::uint8_t team = 0;
        bool ready = false;
    };

    struct LobbyState {
        std::string gameMode;
        std::uint32_t maxRounds = 0;
        bool assistance = false;
        bool quickLiquid = false;
        std::vector<std::string> selectedLevels;
        std::vector<LobbyPlayer> players;
    };

    struct InputCommand {
        std::uint32_t clientId = 0;
        std::uint32_t playerId = 0;
        std::uint32_t sequence = 0;
        std::uint32_t targetTick = 0;
        std::uint32_t actions = 0;
    };

    struct PlayerSnapshot {
        std::uint32_t playerId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float life = 0.0f;
        bool alive = true;
    };

    struct Snapshot {
        std::uint32_t snapshotId = 0;
        std::uint32_t baselineId = 0;
        std::uint32_t tick = 0;
        std::uint32_t roundNumber = 0;
        std::vector<PlayerSnapshot> players;
    };

    struct Event {
        std::uint32_t eventId = 0;
        std::string type;
        std::string payload;
    };

    struct Disconnect {
        std::string reason;
        bool canReconnect = false;
    };

    struct ClientConnectionConfig {
        ConnectionMode mode = ConnectionMode::LocalGame;
        Endpoint remoteEndpoint;
        Endpoint localEndpoint;
        std::string localServerExecutable = "duel6r-server";
        std::string authToken;
    };

    bool hasAction(const InputCommand &command, InputAction action);
}

#endif
