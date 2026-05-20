#include <cstring>
#include <stdexcept>
#include <sstream>
#include <string>

#include "source/client/ConnectionPlan.h"
#include "source/client/LocalServerLauncher.h"
#include "source/client/LoopbackSession.h"
#include "source/network/Protocol.h"
#include "source/network/ProtocolSerialization.h"
#include "source/server/HeadlessServer.h"
#include "source/server/ServerConfig.h"
#include "tests/TestHarness.h"

namespace {
    char *mutableArg(const char *value) {
        return const_cast<char *>(value);
    }
}

D6R_TEST_CASE("Protocol handshake request round-trips through escaped text serialization") {
    Duel6::Network::HandshakeRequest request;
    request.protocolVersion = Duel6::Network::ProtocolVersion;
    request.buildVersion = "6.0.0 test";
    request.clientName = "Player One=Ready";
    request.authToken = "token with spaces";
    request.resources.push_back({"levels/duel_24.json", "abc123"});
    request.resources.push_back({"data/config.script", "def456"});

    std::string payload = Duel6::Network::serializeHandshakeRequest(request);
    Duel6::Network::HandshakeRequest parsed = Duel6::Network::deserializeHandshakeRequest(payload);

    D6R_REQUIRE_EQ(request.protocolVersion, parsed.protocolVersion);
    D6R_REQUIRE_EQ(request.buildVersion, parsed.buildVersion);
    D6R_REQUIRE_EQ(request.clientName, parsed.clientName);
    D6R_REQUIRE_EQ(request.authToken, parsed.authToken);
    D6R_REQUIRE_EQ(2u, parsed.resources.size());
    D6R_REQUIRE_EQ(std::string("levels/duel_24.json"), parsed.resources[0].path);
    D6R_REQUIRE_EQ(std::string("def456"), parsed.resources[1].hash);
}

D6R_TEST_CASE("Input command actions use authoritative action bitset") {
    Duel6::Network::InputCommand command;
    command.clientId = 7;
    command.playerId = 3;
    command.sequence = 42;
    command.targetTick = 9001;
    command.actions = Duel6::Network::MoveLeft | Duel6::Network::Jump | Duel6::Network::Shoot;

    std::string payload = Duel6::Network::serializeInputCommand(command);
    Duel6::Network::InputCommand parsed = Duel6::Network::deserializeInputCommand(payload);

    D6R_REQUIRE_EQ(command.clientId, parsed.clientId);
    D6R_REQUIRE_EQ(command.playerId, parsed.playerId);
    D6R_REQUIRE_EQ(command.sequence, parsed.sequence);
    D6R_REQUIRE_EQ(command.targetTick, parsed.targetTick);
    D6R_REQUIRE(Duel6::Network::hasAction(parsed, Duel6::Network::MoveLeft));
    D6R_REQUIRE(Duel6::Network::hasAction(parsed, Duel6::Network::Jump));
    D6R_REQUIRE(Duel6::Network::hasAction(parsed, Duel6::Network::Shoot));
    D6R_REQUIRE(!Duel6::Network::hasAction(parsed, Duel6::Network::Crouch));
}

D6R_TEST_CASE("Protocol deserialization rejects out-of-range unsigned integer fields") {
    D6R_REQUIRE_THROW(Duel6::Network::deserializeEndpoint("endpoint.host=127.0.0.1\nendpoint.port=65536\n"),
                      std::out_of_range);
    D6R_REQUIRE_THROW(Duel6::Network::deserializeInputCommand("clientId=4294967296\nplayerId=3\nsequence=42\ntargetTick=9001\nactions=20\n"),
                      std::out_of_range);
    D6R_REQUIRE_THROW(Duel6::Network::deserializeInputCommand("clientId=7x\nplayerId=3\nsequence=42\ntargetTick=9001\nactions=20\n"),
                      std::invalid_argument);
    D6R_REQUIRE_THROW(Duel6::Network::deserializeInputCommand("clientId=-1\nplayerId=3\nsequence=42\ntargetTick=9001\nactions=20\n"),
                      std::invalid_argument);
}

D6R_TEST_CASE("Protocol lobby state round-trips selected levels and player readiness") {
    Duel6::Network::LobbyState state;
    state.gameMode = "deathmatch";
    state.maxRounds = 10;
    state.assistance = true;
    state.quickLiquid = false;
    state.selectedLevels.push_back("levels/duel_24.json");
    state.selectedLevels.push_back("levels/waterworks.json");
    state.players.push_back({1, 7, "Player One", 2, true});
    state.players.push_back({2, 8, "Player Two", 3, false});

    Duel6::Network::LobbyState parsed = Duel6::Network::deserializeLobbyState(Duel6::Network::serializeLobbyState(state));

    D6R_REQUIRE_EQ(state.gameMode, parsed.gameMode);
    D6R_REQUIRE_EQ(state.maxRounds, parsed.maxRounds);
    D6R_REQUIRE(parsed.assistance);
    D6R_REQUIRE(!parsed.quickLiquid);
    D6R_REQUIRE_EQ(2u, parsed.selectedLevels.size());
    D6R_REQUIRE_EQ(std::string("levels/waterworks.json"), parsed.selectedLevels[1]);
    D6R_REQUIRE_EQ(2u, parsed.players.size());
    D6R_REQUIRE_EQ(std::string("Player One"), parsed.players[0].displayName);
    D6R_REQUIRE_EQ(3, parsed.players[1].team);
    D6R_REQUIRE(!parsed.players[1].ready);
}

D6R_TEST_CASE("Protocol snapshot, event, and disconnect messages round-trip") {
    Duel6::Network::Snapshot snapshot;
    snapshot.snapshotId = 12;
    snapshot.baselineId = 10;
    snapshot.tick = 420;
    snapshot.roundNumber = 3;
    snapshot.players.push_back({5, 12.5f, 99.25f, 80.0f, true});

    Duel6::Network::Snapshot parsedSnapshot = Duel6::Network::deserializeSnapshot(Duel6::Network::serializeSnapshot(snapshot));
    D6R_REQUIRE_EQ(12u, parsedSnapshot.snapshotId);
    D6R_REQUIRE_EQ(420u, parsedSnapshot.tick);
    D6R_REQUIRE_EQ(1u, parsedSnapshot.players.size());
    D6R_REQUIRE_NEAR(12.5f, parsedSnapshot.players[0].x, 0.001f);
    D6R_REQUIRE(parsedSnapshot.players[0].alive);

    Duel6::Network::Event event;
    event.eventId = 77;
    event.type = "sound";
    event.payload = "weapon/fire=plasma";
    Duel6::Network::Event parsedEvent = Duel6::Network::deserializeEvent(Duel6::Network::serializeEvent(event));
    D6R_REQUIRE_EQ(77u, parsedEvent.eventId);
    D6R_REQUIRE_EQ(std::string("weapon/fire=plasma"), parsedEvent.payload);

    Duel6::Network::Disconnect disconnect;
    disconnect.reason = "server-shutdown";
    disconnect.canReconnect = true;
    Duel6::Network::Disconnect parsedDisconnect = Duel6::Network::deserializeDisconnect(Duel6::Network::serializeDisconnect(disconnect));
    D6R_REQUIRE_EQ(std::string("server-shutdown"), parsedDisconnect.reason);
    D6R_REQUIRE(parsedDisconnect.canReconnect);
}

D6R_TEST_CASE("Server command line parser supports local authoritative instance options") {
    char *argv[] = {
            mutableArg("duel6r-server"),
            mutableArg("--local-only"),
            mutableArg("--port=27777"),
            mutableArg("--name=Local Duel"),
            mutableArg("--resources=resources-test"),
            mutableArg("--token=secret"),
            mutableArg("--tick-rate=120"),
            mutableArg("--max-clients=8"),
            mutableArg("--once")
    };

    Duel6::Server::ServerConfig config = Duel6::Server::parseServerConfig(9, argv);

    D6R_REQUIRE(config.localOnly);
    D6R_REQUIRE(config.once);
    D6R_REQUIRE_EQ(std::string("127.0.0.1"), config.listenEndpoint.host);
    D6R_REQUIRE_EQ(27777, config.listenEndpoint.port);
    D6R_REQUIRE_EQ(std::string("Local Duel"), config.serverName);
    D6R_REQUIRE_EQ(std::string("resources-test"), config.resourcePath);
    D6R_REQUIRE_EQ(std::string("secret"), config.authToken);
    D6R_REQUIRE_EQ(120u, config.tickRate);
    D6R_REQUIRE_EQ(8u, config.maxClients);
}

D6R_TEST_CASE("Headless server accepts compatible authenticated handshakes") {
    Duel6::Server::ServerConfig config;
    config.serverName = "Test Server";
    config.authToken = "secret";
    config.tickRate = 75;
    Duel6::Server::HeadlessServer server(config);

    Duel6::Network::HandshakeRequest request;
    request.protocolVersion = Duel6::Network::ProtocolVersion;
    request.authToken = "secret";
    request.clientName = "client";
    request.buildVersion = "test";

    Duel6::Network::HandshakeAccept accept = server.acceptHandshake(request);

    D6R_REQUIRE_EQ(1u, accept.clientId);
    D6R_REQUIRE_EQ(75u, accept.serverTickRate);
    D6R_REQUIRE_EQ(std::string("Test Server"), accept.serverName);
}

D6R_TEST_CASE("Headless server reports structured handshake rejection reasons") {
    Duel6::Server::ServerConfig config;
    config.authToken = "secret";
    Duel6::Server::HeadlessServer server(config);

    Duel6::Network::HandshakeRequest request;
    request.protocolVersion = Duel6::Network::ProtocolVersion + 1;
    request.authToken = "secret";

    Duel6::Server::HandshakeResult result = server.validateHandshake(request);
    D6R_REQUIRE(!result.accepted);
    D6R_REQUIRE_EQ(static_cast<int>(Duel6::Network::RejectReason::IncompatibleProtocol),
                   static_cast<int>(result.reject.reason));

    request.protocolVersion = Duel6::Network::ProtocolVersion;
    request.authToken = "wrong";
    result = server.validateHandshake(request);
    D6R_REQUIRE(!result.accepted);
    D6R_REQUIRE_EQ(static_cast<int>(Duel6::Network::RejectReason::AuthenticationFailed),
                   static_cast<int>(result.reject.reason));
}

D6R_TEST_CASE("Local game connection plan launches bundled server on loopback endpoint") {
    Duel6::Network::ClientConnectionConfig config;
    config.mode = Duel6::Network::ConnectionMode::LocalGame;
    config.localEndpoint.host = "127.0.0.1";
    config.localEndpoint.port = 29999;
    config.localServerExecutable = "duel6r-server-test";
    config.authToken = "local-token";

    Duel6::Client::ConnectionPlan plan = Duel6::Client::createConnectionPlan(config);
    Duel6::Client::LocalServerLauncher launcher;

    D6R_REQUIRE(plan.launchesLocalServer);
    D6R_REQUIRE_EQ(std::string("127.0.0.1"), plan.endpoint.host);
    D6R_REQUIRE_EQ(29999, plan.endpoint.port);
    D6R_REQUIRE_EQ(std::string("duel6r-server-test"), plan.localServerArguments[0]);
    D6R_REQUIRE_EQ(std::string("--local-only"), plan.localServerArguments[1]);
    D6R_REQUIRE_EQ(std::string("duel6r-server-test --local-only --host=127.0.0.1 --port=29999 --token=local-token"),
                   launcher.buildCommandLine(plan));
}

D6R_TEST_CASE("Local server command line quotes shell metacharacters in configured arguments") {
    Duel6::Network::ClientConnectionConfig config;
    config.mode = Duel6::Network::ConnectionMode::LocalGame;
    config.localEndpoint.host = "127.0.0.1";
    config.localEndpoint.port = 29999;
    config.localServerExecutable = "duel6r-server-test";
    config.authToken = "local&token";

    Duel6::Client::ConnectionPlan plan = Duel6::Client::createConnectionPlan(config);
    Duel6::Client::LocalServerLauncher launcher;

#ifdef _WIN32
    D6R_REQUIRE_EQ(std::string("duel6r-server-test --local-only --host=127.0.0.1 --port=29999 \"--token=local^&token\""),
                   launcher.buildCommandLine(plan));
#else
    D6R_REQUIRE_EQ(std::string("duel6r-server-test --local-only --host=127.0.0.1 --port=29999 '--token=local&token'"),
                   launcher.buildCommandLine(plan));
#endif
}

D6R_TEST_CASE("Loopback session connects local game through server handshake path") {
    Duel6::Network::ClientConnectionConfig clientConfig;
    clientConfig.mode = Duel6::Network::ConnectionMode::LocalGame;
    clientConfig.localEndpoint.host = "127.0.0.1";
    clientConfig.localEndpoint.port = 28888;
    clientConfig.authToken = "loopback-token";

    Duel6::Client::ConnectionPlan plan = Duel6::Client::createConnectionPlan(clientConfig);

    Duel6::Server::ServerConfig serverConfig;
    serverConfig.authToken = "loopback-token";
    serverConfig.tickRate = 90;
    Duel6::Server::HeadlessServer server(serverConfig);
    Duel6::Client::LoopbackSession session(server);

    Duel6::Network::HandshakeRequest request;
    request.protocolVersion = Duel6::Network::ProtocolVersion;
    request.authToken = "loopback-token";
    request.clientName = "local-client";
    request.buildVersion = "test";

    Duel6::Client::LoopbackConnectionResult result = session.connect(plan, request);

    D6R_REQUIRE(result.localServerLaunched);
    D6R_REQUIRE(result.connected);
    D6R_REQUIRE_EQ(std::string("127.0.0.1"), result.endpoint.host);
    D6R_REQUIRE_EQ(28888, result.endpoint.port);
    D6R_REQUIRE_EQ(1u, result.accept.clientId);
    D6R_REQUIRE_EQ(90u, result.accept.serverTickRate);
}

D6R_TEST_CASE("Loopback session returns structured rejection without connecting") {
    Duel6::Network::ClientConnectionConfig clientConfig;
    clientConfig.mode = Duel6::Network::ConnectionMode::LocalGame;
    clientConfig.authToken = "expected-token";
    Duel6::Client::ConnectionPlan plan = Duel6::Client::createConnectionPlan(clientConfig);

    Duel6::Server::ServerConfig serverConfig;
    serverConfig.authToken = "expected-token";
    Duel6::Server::HeadlessServer server(serverConfig);
    Duel6::Client::LoopbackSession session(server);

    Duel6::Network::HandshakeRequest request;
    request.protocolVersion = Duel6::Network::ProtocolVersion;
    request.authToken = "wrong-token";

    Duel6::Client::LoopbackConnectionResult result = session.connect(plan, request);

    D6R_REQUIRE(result.localServerLaunched);
    D6R_REQUIRE(!result.connected);
    D6R_REQUIRE_EQ(static_cast<int>(Duel6::Network::RejectReason::AuthenticationFailed),
                   static_cast<int>(result.reject.reason));
}
