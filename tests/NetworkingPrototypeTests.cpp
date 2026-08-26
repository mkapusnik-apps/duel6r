#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "tests/TestHarness.h"
#include "source/client/ConnectionPlan.h"
#include "source/client/LocalServerLauncher.h"
#include "source/client/LoopbackSession.h"
#include "source/network/ProtocolSerialization.h"
#include "source/server/HeadlessServer.h"
#include "source/server/ServerConfig.h"

namespace {
    using namespace Duel6;

    template<typename Function>
    bool rejects(Function function) {
        try {
            function();
        } catch (const std::exception &) {
            return true;
        }
        return false;
    }

    template<typename Function>
    std::string rejectionMessage(Function function) {
        try {
            function();
        } catch (const std::exception &exception) {
            return exception.what();
        }
        return {};
    }

    Server::ServerConfig parseConfig(std::vector<std::string> arguments) {
        std::vector<char *> argv;
        for (std::string &argument: arguments) {
            argv.push_back(argument.data());
        }
        return Server::parseServerConfig(static_cast<int>(argv.size()), argv.data());
    }

    Network::HandshakeRequest validHandshake() {
        Network::HandshakeRequest request;
        request.clientName = "Local Player";
        request.resources = {{"levels/arena.json", "sha256:1234"}};
        return request;
    }
}

D6R_TEST_CASE("network endpoint and connection config round trip deterministically") {
    Network::Endpoint endpoint{"local host", 32123};
    const std::string encodedEndpoint = Network::serializeEndpoint(endpoint);
    D6R_REQUIRE_EQ("endpoint.host=local%20host\nendpoint.port=32123\n", encodedEndpoint);
    const auto decodedEndpoint = Network::deserializeEndpoint(encodedEndpoint);
    D6R_REQUIRE_EQ(endpoint.host, decodedEndpoint.host);
    D6R_REQUIRE_EQ(endpoint.port, decodedEndpoint.port);
    D6R_REQUIRE_EQ(encodedEndpoint, Network::serializeEndpoint(decodedEndpoint));

    Network::ClientConnectionConfig config;
    config.mode = Network::ConnectionMode::RemoteServer;
    config.remoteEndpoint = {"game.example", 26661};
    config.localEndpoint = {"127.0.0.1", 26660};
    config.localServerExecutable = "duel server";
    const std::string encodedConfig = Network::serializeClientConnectionConfig(config);
    const auto decodedConfig = Network::deserializeClientConnectionConfig(encodedConfig);
    D6R_REQUIRE(decodedConfig.mode == config.mode);
    D6R_REQUIRE_EQ(config.remoteEndpoint.host, decodedConfig.remoteEndpoint.host);
    D6R_REQUIRE_EQ(config.remoteEndpoint.port, decodedConfig.remoteEndpoint.port);
    D6R_REQUIRE_EQ(config.localEndpoint.host, decodedConfig.localEndpoint.host);
    D6R_REQUIRE_EQ(config.localEndpoint.port, decodedConfig.localEndpoint.port);
    D6R_REQUIRE_EQ(config.localServerExecutable, decodedConfig.localServerExecutable);
    D6R_REQUIRE_EQ(encodedConfig, Network::serializeClientConnectionConfig(decodedConfig));
}

D6R_TEST_CASE("all handshake DTOs preserve meaning and deterministic text") {
    Network::HandshakeRequest request = validHandshake();
    request.resources.push_back({"profiles/A&B", "hash with spaces"});
    const std::string requestText = Network::serializeHandshakeRequest(request);
    const auto decodedRequest = Network::deserializeHandshakeRequest(requestText);
    D6R_REQUIRE_EQ(request.protocolVersion, decodedRequest.protocolVersion);
    D6R_REQUIRE_EQ(request.buildVersion, decodedRequest.buildVersion);
    D6R_REQUIRE_EQ(request.clientName, decodedRequest.clientName);
    D6R_REQUIRE_EQ(request.resources.size(), decodedRequest.resources.size());
    D6R_REQUIRE_EQ(request.resources[1].path, decodedRequest.resources[1].path);
    D6R_REQUIRE_EQ(request.resources[1].hash, decodedRequest.resources[1].hash);
    D6R_REQUIRE_EQ(requestText, Network::serializeHandshakeRequest(decodedRequest));

    Network::HandshakeAccept accept{42, 120, "Prototype Server"};
    const std::string acceptText = Network::serializeHandshakeAccept(accept);
    const auto decodedAccept = Network::deserializeHandshakeAccept(acceptText);
    D6R_REQUIRE_EQ(accept.clientId, decodedAccept.clientId);
    D6R_REQUIRE_EQ(accept.serverTickRate, decodedAccept.serverTickRate);
    D6R_REQUIRE_EQ(accept.serverName, decodedAccept.serverName);
    D6R_REQUIRE_EQ(acceptText, Network::serializeHandshakeAccept(decodedAccept));

    Network::HandshakeReject reject{Network::RejectReason::ContentMismatch, "different content"};
    const std::string rejectText = Network::serializeHandshakeReject(reject);
    const auto decodedReject = Network::deserializeHandshakeReject(rejectText);
    D6R_REQUIRE(decodedReject.reason == reject.reason);
    D6R_REQUIRE_EQ(reject.detail, decodedReject.detail);
    D6R_REQUIRE_EQ(rejectText, Network::serializeHandshakeReject(decodedReject));
}

D6R_TEST_CASE("lobby input snapshot event and disconnect DTOs round trip") {
    Network::LobbyState lobby;
    lobby.gameMode = "Team Deathmatch";
    lobby.maxRounds = 9;
    lobby.assistance = true;
    lobby.quickLiquid = false;
    lobby.selectedLevels = {"Arena One", "Arena/Two"};
    lobby.players = {{7, 3, "Alice & Bob", 2, true}, {8, 4, "Eve", 1, false}};
    const std::string lobbyText = Network::serializeLobbyState(lobby);
    const auto decodedLobby = Network::deserializeLobbyState(lobbyText);
    D6R_REQUIRE_EQ(lobby.gameMode, decodedLobby.gameMode);
    D6R_REQUIRE_EQ(lobby.maxRounds, decodedLobby.maxRounds);
    D6R_REQUIRE_EQ(lobby.assistance, decodedLobby.assistance);
    D6R_REQUIRE_EQ(lobby.quickLiquid, decodedLobby.quickLiquid);
    D6R_REQUIRE_EQ(lobby.selectedLevels[1], decodedLobby.selectedLevels[1]);
    D6R_REQUIRE_EQ(lobby.players[0].displayName, decodedLobby.players[0].displayName);
    D6R_REQUIRE_EQ(lobby.players[1].ready, decodedLobby.players[1].ready);
    D6R_REQUIRE_EQ(lobbyText, Network::serializeLobbyState(decodedLobby));

    Network::InputCommand input{3, 7, 99, 101, Network::MoveLeft | Network::Jump | Network::Shoot};
    const std::string inputText = Network::serializeInputCommand(input);
    const auto decodedInput = Network::deserializeInputCommand(inputText);
    D6R_REQUIRE_EQ(input.clientId, decodedInput.clientId);
    D6R_REQUIRE_EQ(input.playerId, decodedInput.playerId);
    D6R_REQUIRE_EQ(input.sequence, decodedInput.sequence);
    D6R_REQUIRE_EQ(input.targetTick, decodedInput.targetTick);
    D6R_REQUIRE_EQ(input.actions, decodedInput.actions);
    D6R_REQUIRE(Network::hasAction(decodedInput, Network::Jump));
    D6R_REQUIRE(!Network::hasAction(decodedInput, Network::Crouch));
    D6R_REQUIRE_EQ(inputText, Network::serializeInputCommand(decodedInput));

    Network::Snapshot snapshot;
    snapshot.snapshotId = 55;
    snapshot.baselineId = 50;
    snapshot.tick = 1000;
    snapshot.roundNumber = 4;
    snapshot.players = {{7, -12.5f, 0.25f, 99.75f, true}, {8, 1.0f, 2.0f, 0.0f, false}};
    const std::string snapshotText = Network::serializeSnapshot(snapshot);
    const auto decodedSnapshot = Network::deserializeSnapshot(snapshotText);
    D6R_REQUIRE_EQ(snapshot.snapshotId, decodedSnapshot.snapshotId);
    D6R_REQUIRE_EQ(snapshot.players.size(), decodedSnapshot.players.size());
    D6R_REQUIRE_NEAR(snapshot.players[0].x, decodedSnapshot.players[0].x, 0.00001f);
    D6R_REQUIRE_NEAR(snapshot.players[0].life, decodedSnapshot.players[0].life, 0.00001f);
    D6R_REQUIRE_EQ(snapshot.players[1].alive, decodedSnapshot.players[1].alive);
    D6R_REQUIRE_EQ(snapshotText, Network::serializeSnapshot(decodedSnapshot));

    Network::Event event{77, "round/start", "level=One\nseed=42"};
    const std::string eventText = Network::serializeEvent(event);
    const auto decodedEvent = Network::deserializeEvent(eventText);
    D6R_REQUIRE_EQ(event.eventId, decodedEvent.eventId);
    D6R_REQUIRE_EQ(event.type, decodedEvent.type);
    D6R_REQUIRE_EQ(event.payload, decodedEvent.payload);
    D6R_REQUIRE_EQ(eventText, Network::serializeEvent(decodedEvent));

    Network::Disconnect disconnect{"server stopped", true};
    const std::string disconnectText = Network::serializeDisconnect(disconnect);
    const auto decodedDisconnect = Network::deserializeDisconnect(disconnectText);
    D6R_REQUIRE_EQ(disconnect.reason, decodedDisconnect.reason);
    D6R_REQUIRE_EQ(disconnect.canReconnect, decodedDisconnect.canReconnect);
    D6R_REQUIRE_EQ(disconnectText, Network::serializeDisconnect(decodedDisconnect));
}

D6R_TEST_CASE("protocol parser rejects malformed truncated oversized invalid and trailing input") {
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host\nendpoint.port=26660\n"); }));
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host=abc%2\nendpoint.port=26660\n"); }));
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host=abc%XZ\nendpoint.port=26660\n"); }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeEndpoint(std::string(Network::MaxPayloadBytes + 1, 'x'));
    }));
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host=x\nendpoint.port=-1\n"); }));
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host=x\nendpoint.port=12x\n"); }));
    D6R_REQUIRE(rejects([] { Network::deserializeEndpoint("endpoint.host=x\nendpoint.port=0\n"); }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeInputCommand("actions=128\nclientId=1\nplayerId=1\nsequence=1\ntargetTick=1\n");
    }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeLobbyState(
                "assistance=false\ngameMode=dm\nmaxRounds=1\nplayer.count=16\nquickLiquid=false\n");
    }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeSnapshot(
                "baselineId=0\nplayer.count=1\nroundNumber=1\nsnapshotId=1\ntick=1\n");
    }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeSnapshot(
                "baselineId=0\nplayer.count=0\nroundNumber=1\nsnapshotId=1\ntick=nan\n");
    }));
    D6R_REQUIRE(rejects([] {
        Network::deserializeDisconnect("canReconnect=false\nreason=bye\ntrailing garbage");
    }));
    D6R_REQUIRE(rejects([] {
        Network::Snapshot snapshot;
        snapshot.players.push_back({1, std::numeric_limits<float>::infinity(), 0, 1, true});
        Network::serializeSnapshot(snapshot);
    }));
}

D6R_TEST_CASE("handshake policy rejects invalid requests and assigns bounded client IDs") {
    Server::ServerConfig config;
    config.maxClients = 2;
    Server::HeadlessServer server(config);

    auto request = validHandshake();
    auto result = server.validateHandshake(request);
    D6R_REQUIRE(result.accepted);
    D6R_REQUIRE_EQ(0u, result.accept.clientId);

    auto wrongProtocol = request;
    ++wrongProtocol.protocolVersion;
    result = server.validateHandshake(wrongProtocol);
    D6R_REQUIRE(!result.accepted);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::IncompatibleProtocol);

    auto wrongBuild = request;
    wrongBuild.buildVersion = "another-build";
    result = server.validateHandshake(wrongBuild);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::IncompatibleBuild);

    auto unnamed = request;
    unnamed.clientName.clear();
    result = server.validateHandshake(unnamed);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::InvalidRequest);
    D6R_REQUIRE(rejects([&] { Network::serializeHandshakeRequest(unnamed); }));

    auto noBuild = request;
    noBuild.buildVersion.clear();
    result = server.validateHandshake(noBuild);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::InvalidRequest);
    D6R_REQUIRE(rejects([&] { Network::serializeHandshakeRequest(noBuild); }));

    auto malformedResource = request;
    malformedResource.resources[0].hash.clear();
    result = server.validateHandshake(malformedResource);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::InvalidRequest);

    auto tokenRequest = request;
    tokenRequest.authToken = "do-not-print-this-secret";
    result = server.validateHandshake(tokenRequest);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::AuthenticationFailed);
    D6R_REQUIRE(result.reject.detail.find(tokenRequest.authToken) == std::string::npos);
    const std::string tokenSerializationError = rejectionMessage([&] {
        Network::serializeHandshakeRequest(tokenRequest);
    });
    D6R_REQUIRE(!tokenSerializationError.empty());
    D6R_REQUIRE(tokenSerializationError.find(tokenRequest.authToken) == std::string::npos);

    const auto first = server.acceptHandshake(request);
    const auto second = server.acceptHandshake(request);
    D6R_REQUIRE(first.clientId != 0);
    D6R_REQUIRE(second.clientId != 0);
    D6R_REQUIRE(first.clientId != second.clientId);
    result = server.validateHandshake(request);
    D6R_REQUIRE(result.reject.reason == Network::RejectReason::ServerFull);
}

D6R_TEST_CASE("loopback is explicitly in process and never reports a launched transport") {
    Server::HeadlessServer server(Server::ServerConfig{});
    Network::ClientConnectionConfig config;
    config.mode = Network::ConnectionMode::LocalGame;
    config.localEndpoint = {"127.0.0.1", 27770};
    const auto plan = Client::createConnectionPlan(config);
    Client::LoopbackSession session(server);
    const auto result = session.connect(plan, validHandshake());
    D6R_REQUIRE(result.connected);
    D6R_REQUIRE(!result.localServerLaunched);
    D6R_REQUIRE_EQ(plan.endpoint.host, result.endpoint.host);
    D6R_REQUIRE_EQ(plan.endpoint.port, result.endpoint.port);
    D6R_REQUIRE(result.accept.clientId != 0);

    auto rejectedRequest = validHandshake();
    rejectedRequest.buildVersion = "incompatible";
    const auto rejected = session.connect(plan, rejectedRequest);
    D6R_REQUIRE(!rejected.connected);
    D6R_REQUIRE(!rejected.localServerLaunched);
    D6R_REQUIRE(rejected.reject.reason == Network::RejectReason::IncompatibleBuild);

    config.mode = Network::ConnectionMode::RemoteServer;
    const auto remotePlan = Client::createConnectionPlan(config);
    D6R_REQUIRE(rejects([&] { session.connect(remotePlan, validHandshake()); }));
}

D6R_TEST_CASE("server config and nominal runtime clearly reject unsupported behavior and secrets") {
    const auto config = parseConfig({"duel6r-server", "--host=0.0.0.0", "--port=34567", "--name=QA Server",
                                     "--build-version=qa", "--resources=test data", "--tick-rate=120",
                                     "--max-clients=8", "--local-only"});
    D6R_REQUIRE_EQ("127.0.0.1", config.listenEndpoint.host);
    D6R_REQUIRE_EQ(34567, config.listenEndpoint.port);
    D6R_REQUIRE_EQ("QA Server", config.serverName);
    D6R_REQUIRE_EQ(120u, config.tickRate);
    D6R_REQUIRE_EQ(8u, config.maxClients);
    D6R_REQUIRE(config.localOnly);

    D6R_REQUIRE(rejects([] { parseConfig({"duel6r-server", "--port=0"}); }));
    D6R_REQUIRE(rejects([] { parseConfig({"duel6r-server", "--port=12x"}); }));
    D6R_REQUIRE(rejects([] { parseConfig({"duel6r-server", "--tick-rate=1001"}); }));
    D6R_REQUIRE(rejects([] { parseConfig({"duel6r-server", "--max-clients=16"}); }));
    D6R_REQUIRE(rejects([] { parseConfig({"duel6r-server", "--unknown"}); }));

    const std::string secret = "do-not-print-this-secret";
    const std::string error = rejectionMessage([&] {
        parseConfig({"duel6r-server", "--token=" + secret});
    });
    D6R_REQUIRE(!error.empty());
    D6R_REQUIRE(error.find(secret) == std::string::npos);

    Server::HeadlessServer server(Server::ServerConfig{});
    std::ostringstream output;
    D6R_REQUIRE_EQ(2, server.run(output));
    D6R_REQUIRE(output.str().find("unsupported") != std::string::npos);
    D6R_REQUIRE(output.str().find("no network transport") != std::string::npos);
    D6R_REQUIRE(output.str().find("did not listen") != std::string::npos);
}

D6R_TEST_CASE("local command planning quotes arguments without exposing authentication tokens") {
    Network::ClientConnectionConfig config;
    config.mode = Network::ConnectionMode::LocalGame;
    config.localServerExecutable = "duel server's bin";
    config.localEndpoint = {"local host", 26660};
    const auto plan = Client::createConnectionPlan(config);
    D6R_REQUIRE(plan.launchesLocalServer);
    D6R_REQUIRE_EQ(4u, plan.localServerArguments.size());
    for (const auto &argument: plan.localServerArguments) {
        D6R_REQUIRE(argument.find("token") == std::string::npos);
    }

    Client::LocalServerLauncher launcher;
    D6R_REQUIRE(plan.localServerArguments == launcher.buildCommand(plan));
#ifdef _WIN32
    D6R_REQUIRE_EQ("\"duel server's bin\" --local-only \"--host=local host\" --port=26660",
                   launcher.buildCommandLine(plan));
#else
    D6R_REQUIRE_EQ("'duel server'\\''s bin' --local-only '--host=local host' --port=26660",
                   launcher.buildCommandLine(plan));
#endif

    config.authToken = "do-not-print-this-secret";
    const std::string configSerializationError = rejectionMessage([&] {
        Network::serializeClientConnectionConfig(config);
    });
    D6R_REQUIRE(!configSerializationError.empty());
    D6R_REQUIRE(configSerializationError.find(config.authToken) == std::string::npos);
    const std::string planningError = rejectionMessage([&] { Client::createConnectionPlan(config); });
    D6R_REQUIRE(!planningError.empty());
    D6R_REQUIRE(planningError.find(config.authToken) == std::string::npos);

    Client::ConnectionPlan injected = plan;
    injected.localServerArguments.push_back("--token=do-not-print-this-secret");
    const std::string launcherError = rejectionMessage([&] { launcher.buildCommand(injected); });
    D6R_REQUIRE(!launcherError.empty());
    D6R_REQUIRE(launcherError.find("do-not-print-this-secret") == std::string::npos);
}
