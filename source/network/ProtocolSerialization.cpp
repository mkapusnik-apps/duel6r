#include "ProtocolSerialization.h"
#include "NetworkTrustPolicy.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Duel6::Network {
    namespace {
        using Properties = std::map<std::string, std::vector<std::string>>;

        void requireBoundedSize(const std::string &name, std::size_t size, std::size_t maximum) {
            if (size > maximum) {
                throw std::length_error(name + " exceeds the networking scaffold limit");
            }
        }

        void requireCollectionSize(const std::string &name, std::size_t size, std::size_t maximum) {
            requireBoundedSize(name, size, maximum);
        }

        bool isUnescaped(unsigned char chr) {
            return (chr >= '0' && chr <= '9')
                   || (chr >= 'A' && chr <= 'Z')
                   || (chr >= 'a' && chr <= 'z')
                   || chr == '-' || chr == '_' || chr == '.' || chr == '~';
        }

        std::string escape(const std::string &value) {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::uppercase << std::hex;
            for (unsigned char chr: value) {
                if (isUnescaped(chr)) {
                    stream << static_cast<char>(chr);
                } else {
                    stream << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(chr);
                }
            }
            return stream.str();
        }

        int hexValue(char chr) {
            if (chr >= '0' && chr <= '9') {
                return chr - '0';
            }
            if (chr >= 'A' && chr <= 'F') {
                return chr - 'A' + 10;
            }
            if (chr >= 'a' && chr <= 'f') {
                return chr - 'a' + 10;
            }
            throw std::invalid_argument("Invalid escape sequence");
        }

        std::string unescape(const std::string &value) {
            requireBoundedSize("Encoded protocol value", value.size(), MaxPayloadBytes);
            std::string result;
            result.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '%') {
                    if (i + 2 >= value.size()) {
                        throw std::invalid_argument("Truncated escape sequence");
                    }
                    int high = hexValue(value[i + 1]);
                    int low = hexValue(value[i + 2]);
                    result += static_cast<char>((high << 4) | low);
                    i += 2;
                } else {
                    result += value[i];
                }
            }
            return result;
        }

        std::string serializeProperties(const Properties &properties) {
            std::size_t propertyCount = 0;
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            for (const auto &property: properties) {
                if (!Trust::validPropertyKey(property.first))
                    throw std::invalid_argument("Protocol property key is invalid");
                for (const auto &value: property.second) {
                    if (!Trust::validGeneralString(value))
                        throw std::length_error("Protocol property value exceeds the networking scaffold limit");
                    if (!Trust::validPropertyCount(++propertyCount)) {
                        throw std::length_error("Too many protocol properties");
                    }
                    stream << escape(property.first) << '=' << escape(value) << '\n';
                    if (static_cast<std::size_t>(stream.tellp()) > MaxPayloadBytes) {
                        throw std::length_error("Serialized protocol payload is too large");
                    }
                }
            }
            return stream.str();
        }

        Properties parseProperties(const std::string &payload) {
            requireBoundedSize("Protocol payload", payload.size(), MaxPayloadBytes);
            Properties properties;
            std::istringstream stream(payload);
            stream.imbue(std::locale::classic());
            std::string line;
            std::size_t propertyCount = 0;
            while (std::getline(stream, line)) {
                if (line.empty()) {
                    continue;
                }
                std::size_t separator = line.find('=');
                if (separator == std::string::npos) {
                    throw std::invalid_argument("Protocol property is missing '=' separator");
                }
                if (!Trust::validPropertyCount(++propertyCount)) {
                    throw std::length_error("Too many protocol properties");
                }
                if (separator > Trust::MaxKeyBytes * 3
                    || line.size() - separator - 1 > Trust::MaxStringBytes * 3)
                    throw std::length_error("Encoded protocol property exceeds the networking scaffold limit");
                std::string key = unescape(line.substr(0, separator));
                std::string value = unescape(line.substr(separator + 1));
                if (!Trust::validPropertyKey(key)) throw std::invalid_argument("Protocol property key is invalid");
                if (!Trust::validGeneralString(value))
                    throw std::length_error("Protocol property value exceeds the networking scaffold limit");
                properties[std::move(key)].push_back(std::move(value));
            }
            return properties;
        }

        void set(Properties &properties, const std::string &key, const std::string &value) {
            if (!Trust::validPropertyKey(key)) throw std::invalid_argument("Protocol property key is invalid");
            if (!Trust::validGeneralString(value))
                throw std::length_error("Protocol property value exceeds the networking scaffold limit");
            properties[key].push_back(value);
        }

        std::string requiredString(const Properties &properties, const std::string &key) {
            auto it = properties.find(key);
            if (it == properties.end() || it->second.empty()) {
                throw std::invalid_argument("Missing protocol property: " + key);
            }
            if (it->second.size() != 1) {
                throw std::invalid_argument("Duplicate protocol property: " + key);
            }
            return it->second.front();
        }

        unsigned long requiredUnsignedLong(const Properties &properties, const std::string &key, unsigned long maxValue) {
            const std::string value = requiredString(properties, key);
            if (value.empty() || value[0] == '+' || value[0] == '-') {
                throw std::invalid_argument("Invalid unsigned protocol property: " + key);
            }

            std::size_t consumed = 0;
            unsigned long parsed = std::stoul(value, &consumed);
            if (consumed != value.size()) {
                throw std::invalid_argument("Invalid unsigned protocol property: " + key);
            }
            if (parsed > maxValue) {
                throw std::out_of_range("Protocol property is out of range: " + key);
            }
            return parsed;
        }

        std::uint32_t requiredUint32(const Properties &properties, const std::string &key) {
            return static_cast<std::uint32_t>(requiredUnsignedLong(properties, key,
                                                                   std::numeric_limits<std::uint32_t>::max()));
        }

        std::uint16_t requiredUint16(const Properties &properties, const std::string &key) {
            return static_cast<std::uint16_t>(requiredUnsignedLong(properties, key,
                                                                   std::numeric_limits<std::uint16_t>::max()));
        }

        std::uint8_t requiredUint8(const Properties &properties, const std::string &key) {
            return static_cast<std::uint8_t>(requiredUnsignedLong(properties, key,
                                                                  std::numeric_limits<std::uint8_t>::max()));
        }

        bool requiredBool(const Properties &properties, const std::string &key) {
            const std::string value = requiredString(properties, key);
            if (value == "true") {
                return true;
            }
            if (value == "false") {
                return false;
            }
            throw std::invalid_argument("Invalid boolean protocol property: " + key);
        }

        float requiredFloat(const Properties &properties, const std::string &key) {
            const std::string value = requiredString(properties, key);
            std::istringstream stream(value);
            stream.imbue(std::locale::classic());
            stream >> std::noskipws;
            float parsed = 0.0f;
            stream >> parsed;
            if (!stream || !stream.eof() || !std::isfinite(parsed)) {
                throw std::invalid_argument("Invalid float protocol property: " + key);
            }
            return parsed;
        }

        std::string floatToString(float value) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("Protocol float must be finite");
            }
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
            return stream.str();
        }

        void requireNonEmpty(const std::string &name, const std::string &value) {
            if (value.empty()) {
                throw std::invalid_argument(name + " must not be empty");
            }
        }

        std::string indexedKey(const std::string &prefix, std::uint32_t index, const std::string &field) {
            return prefix + "." + std::to_string(index) + "." + field;
        }

        Endpoint endpointFromProperties(const Properties &properties, const std::string &prefix) {
            Endpoint endpoint;
            endpoint.host = requiredString(properties, prefix + ".host");
            endpoint.port = requiredUint16(properties, prefix + ".port");
            requireNonEmpty("Endpoint host", endpoint.host);
            if (endpoint.port == 0) {
                throw std::invalid_argument("Endpoint port must not be zero");
            }
            return endpoint;
        }

        void endpointToProperties(Properties &properties, const std::string &prefix, const Endpoint &endpoint) {
            requireNonEmpty("Endpoint host", endpoint.host);
            if (endpoint.port == 0) {
                throw std::invalid_argument("Endpoint port must not be zero");
            }
            set(properties, prefix + ".host", endpoint.host);
            set(properties, prefix + ".port", std::to_string(endpoint.port));
        }
    }

    std::string toString(ConnectionMode mode) {
        switch (mode) {
            case ConnectionMode::LocalGame:
                return "local-game";
            case ConnectionMode::RemoteServer:
                return "remote-server";
        }
        throw std::invalid_argument("Unknown connection mode value");
    }

    ConnectionMode connectionModeFromString(const std::string &value) {
        if (value == "local-game") {
            return ConnectionMode::LocalGame;
        }
        if (value == "remote-server") {
            return ConnectionMode::RemoteServer;
        }
        throw std::invalid_argument("Unknown connection mode: " + value);
    }

    std::string toString(RejectReason reason) {
        switch (reason) {
            case RejectReason::None:
                return "none";
            case RejectReason::IncompatibleProtocol:
                return "incompatible-protocol";
            case RejectReason::IncompatibleBuild:
                return "incompatible-build";
            case RejectReason::ContentMismatch:
                return "content-mismatch";
            case RejectReason::ServerFull:
                return "server-full";
            case RejectReason::AuthenticationFailed:
                return "authentication-failed";
            case RejectReason::MatchInProgress:
                return "match-in-progress";
            case RejectReason::InvalidRequest:
                return "invalid-request";
        }
        throw std::invalid_argument("Unknown reject reason value");
    }

    RejectReason rejectReasonFromString(const std::string &value) {
        if (value == "none") {
            return RejectReason::None;
        }
        if (value == "incompatible-protocol") {
            return RejectReason::IncompatibleProtocol;
        }
        if (value == "incompatible-build") {
            return RejectReason::IncompatibleBuild;
        }
        if (value == "content-mismatch") {
            return RejectReason::ContentMismatch;
        }
        if (value == "server-full") {
            return RejectReason::ServerFull;
        }
        if (value == "authentication-failed") {
            return RejectReason::AuthenticationFailed;
        }
        if (value == "match-in-progress") {
            return RejectReason::MatchInProgress;
        }
        if (value == "invalid-request") {
            return RejectReason::InvalidRequest;
        }
        throw std::invalid_argument("Unknown reject reason: " + value);
    }

    std::string serializeEndpoint(const Endpoint &endpoint) {
        Properties properties;
        endpointToProperties(properties, "endpoint", endpoint);
        return serializeProperties(properties);
    }

    Endpoint deserializeEndpoint(const std::string &payload) {
        return endpointFromProperties(parseProperties(payload), "endpoint");
    }

    std::string serializeHandshakeRequest(const HandshakeRequest &request) {
        requireNonEmpty("Handshake build version", request.buildVersion);
        requireNonEmpty("Handshake client name", request.clientName);
        if (!request.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
        requireCollectionSize("Handshake resource list", request.resources.size(), MaxProtocolCollectionEntries);
        Properties properties;
        set(properties, "protocolVersion", std::to_string(request.protocolVersion));
        set(properties, "buildVersion", request.buildVersion);
        set(properties, "clientName", request.clientName);
        set(properties, "authToken", request.authToken);
        set(properties, "resource.count", std::to_string(request.resources.size()));
        for (std::uint32_t i = 0; i < request.resources.size(); ++i) {
            const ResourceHash &resource = request.resources[i];
            requireNonEmpty("Resource path", resource.path);
            requireNonEmpty("Resource hash", resource.hash);
            set(properties, indexedKey("resource", i, "path"), resource.path);
            set(properties, indexedKey("resource", i, "hash"), resource.hash);
        }
        return serializeProperties(properties);
    }

    HandshakeRequest deserializeHandshakeRequest(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        HandshakeRequest request;
        request.protocolVersion = requiredUint16(properties, "protocolVersion");
        request.buildVersion = requiredString(properties, "buildVersion");
        request.clientName = requiredString(properties, "clientName");
        request.authToken = requiredString(properties, "authToken");
        requireNonEmpty("Handshake build version", request.buildVersion);
        requireNonEmpty("Handshake client name", request.clientName);
        if (!request.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
        const std::uint32_t resourceCount = requiredUint32(properties, "resource.count");
        requireCollectionSize("Handshake resource list", resourceCount, MaxProtocolCollectionEntries);
        for (std::uint32_t i = 0; i < resourceCount; ++i) {
            ResourceHash resource{
                    requiredString(properties, indexedKey("resource", i, "path")),
                    requiredString(properties, indexedKey("resource", i, "hash"))};
            requireNonEmpty("Resource path", resource.path);
            requireNonEmpty("Resource hash", resource.hash);
            request.resources.push_back(std::move(resource));
        }
        return request;
    }

    std::string serializeHandshakeAccept(const HandshakeAccept &accept) {
        if (accept.clientId == 0 || accept.serverTickRate == 0) {
            throw std::invalid_argument("Accepted handshake requires nonzero client ID and tick rate");
        }
        requireNonEmpty("Handshake server name", accept.serverName);
        Properties properties;
        set(properties, "clientId", std::to_string(accept.clientId));
        set(properties, "serverTickRate", std::to_string(accept.serverTickRate));
        set(properties, "serverName", accept.serverName);
        return serializeProperties(properties);
    }

    HandshakeAccept deserializeHandshakeAccept(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        HandshakeAccept accept;
        accept.clientId = requiredUint32(properties, "clientId");
        accept.serverTickRate = requiredUint32(properties, "serverTickRate");
        accept.serverName = requiredString(properties, "serverName");
        if (accept.clientId == 0 || accept.serverTickRate == 0) {
            throw std::invalid_argument("Accepted handshake requires nonzero client ID and tick rate");
        }
        requireNonEmpty("Handshake server name", accept.serverName);
        return accept;
    }

    std::string serializeHandshakeReject(const HandshakeReject &reject) {
        if (reject.reason == RejectReason::None) {
            throw std::invalid_argument("Rejected handshake requires a rejection reason");
        }
        requireNonEmpty("Handshake rejection detail", reject.detail);
        Properties properties;
        set(properties, "reason", toString(reject.reason));
        set(properties, "detail", reject.detail);
        return serializeProperties(properties);
    }

    HandshakeReject deserializeHandshakeReject(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        HandshakeReject reject;
        reject.reason = rejectReasonFromString(requiredString(properties, "reason"));
        reject.detail = requiredString(properties, "detail");
        if (reject.reason == RejectReason::None) {
            throw std::invalid_argument("Rejected handshake requires a rejection reason");
        }
        requireNonEmpty("Handshake rejection detail", reject.detail);
        return reject;
    }

    std::string serializeLobbyState(const LobbyState &state) {
        requireNonEmpty("Lobby game mode", state.gameMode);
        requireCollectionSize("Selected level list", state.selectedLevels.size(), MaxProtocolCollectionEntries);
        requireCollectionSize("Lobby player list", state.players.size(), MaxNetworkPlayers);
        Properties properties;
        set(properties, "gameMode", state.gameMode);
        set(properties, "maxRounds", std::to_string(state.maxRounds));
        set(properties, "assistance", state.assistance ? "true" : "false");
        set(properties, "quickLiquid", state.quickLiquid ? "true" : "false");
        for (const auto &level: state.selectedLevels) {
            requireNonEmpty("Selected level", level);
            set(properties, "selectedLevel", level);
        }
        set(properties, "player.count", std::to_string(state.players.size()));
        for (std::uint32_t i = 0; i < state.players.size(); ++i) {
            const LobbyPlayer &player = state.players[i];
            requireNonEmpty("Lobby player display name", player.displayName);
            set(properties, indexedKey("player", i, "playerId"), std::to_string(player.playerId));
            set(properties, indexedKey("player", i, "clientId"), std::to_string(player.clientId));
            set(properties, indexedKey("player", i, "displayName"), player.displayName);
            set(properties, indexedKey("player", i, "team"), std::to_string(player.team));
            set(properties, indexedKey("player", i, "ready"), player.ready ? "true" : "false");
        }
        return serializeProperties(properties);
    }

    LobbyState deserializeLobbyState(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        LobbyState state;
        state.gameMode = requiredString(properties, "gameMode");
        requireNonEmpty("Lobby game mode", state.gameMode);
        state.maxRounds = requiredUint32(properties, "maxRounds");
        state.assistance = requiredBool(properties, "assistance");
        state.quickLiquid = requiredBool(properties, "quickLiquid");
        auto selectedLevels = properties.find("selectedLevel");
        if (selectedLevels != properties.end()) {
            requireCollectionSize("Selected level list", selectedLevels->second.size(), MaxProtocolCollectionEntries);
            for (const std::string &level: selectedLevels->second) {
                requireNonEmpty("Selected level", level);
            }
            state.selectedLevels = selectedLevels->second;
        }
        std::uint32_t playerCount = requiredUint32(properties, "player.count");
        requireCollectionSize("Lobby player list", playerCount, MaxNetworkPlayers);
        for (std::uint32_t i = 0; i < playerCount; ++i) {
            LobbyPlayer player;
            player.playerId = requiredUint32(properties, indexedKey("player", i, "playerId"));
            player.clientId = requiredUint32(properties, indexedKey("player", i, "clientId"));
            player.displayName = requiredString(properties, indexedKey("player", i, "displayName"));
            requireNonEmpty("Lobby player display name", player.displayName);
            player.team = requiredUint8(properties, indexedKey("player", i, "team"));
            player.ready = requiredBool(properties, indexedKey("player", i, "ready"));
            state.players.push_back(player);
        }
        return state;
    }

    std::string serializeInputCommand(const InputCommand &command) {
        constexpr std::uint32_t supportedActions = MoveLeft | MoveRight | Jump | Crouch | Shoot
                                                   | PickOrSwapWeapon | ShowStatus;
        if ((command.actions & ~supportedActions) != 0) {
            throw std::invalid_argument("Input command contains unsupported action bits");
        }
        Properties properties;
        set(properties, "clientId", std::to_string(command.clientId));
        set(properties, "playerId", std::to_string(command.playerId));
        set(properties, "sequence", std::to_string(command.sequence));
        set(properties, "targetTick", std::to_string(command.targetTick));
        set(properties, "actions", std::to_string(command.actions));
        return serializeProperties(properties);
    }

    InputCommand deserializeInputCommand(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        InputCommand command;
        command.clientId = requiredUint32(properties, "clientId");
        command.playerId = requiredUint32(properties, "playerId");
        command.sequence = requiredUint32(properties, "sequence");
        command.targetTick = requiredUint32(properties, "targetTick");
        command.actions = requiredUint32(properties, "actions");
        constexpr std::uint32_t supportedActions = MoveLeft | MoveRight | Jump | Crouch | Shoot
                                                   | PickOrSwapWeapon | ShowStatus;
        if ((command.actions & ~supportedActions) != 0) {
            throw std::invalid_argument("Input command contains unsupported action bits");
        }
        return command;
    }

    std::string serializeSnapshot(const Snapshot &snapshot) {
        requireCollectionSize("Snapshot player list", snapshot.players.size(), MaxNetworkPlayers);
        Properties properties;
        set(properties, "snapshotId", std::to_string(snapshot.snapshotId));
        set(properties, "baselineId", std::to_string(snapshot.baselineId));
        set(properties, "tick", std::to_string(snapshot.tick));
        set(properties, "roundNumber", std::to_string(snapshot.roundNumber));
        set(properties, "player.count", std::to_string(snapshot.players.size()));
        for (std::uint32_t i = 0; i < snapshot.players.size(); ++i) {
            const PlayerSnapshot &player = snapshot.players[i];
            set(properties, indexedKey("player", i, "playerId"), std::to_string(player.playerId));
            set(properties, indexedKey("player", i, "x"), floatToString(player.x));
            set(properties, indexedKey("player", i, "y"), floatToString(player.y));
            set(properties, indexedKey("player", i, "life"), floatToString(player.life));
            set(properties, indexedKey("player", i, "alive"), player.alive ? "true" : "false");
        }
        return serializeProperties(properties);
    }

    Snapshot deserializeSnapshot(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        Snapshot snapshot;
        snapshot.snapshotId = requiredUint32(properties, "snapshotId");
        snapshot.baselineId = requiredUint32(properties, "baselineId");
        snapshot.tick = requiredUint32(properties, "tick");
        snapshot.roundNumber = requiredUint32(properties, "roundNumber");
        std::uint32_t playerCount = requiredUint32(properties, "player.count");
        requireCollectionSize("Snapshot player list", playerCount, MaxNetworkPlayers);
        for (std::uint32_t i = 0; i < playerCount; ++i) {
            PlayerSnapshot player;
            player.playerId = requiredUint32(properties, indexedKey("player", i, "playerId"));
            player.x = requiredFloat(properties, indexedKey("player", i, "x"));
            player.y = requiredFloat(properties, indexedKey("player", i, "y"));
            player.life = requiredFloat(properties, indexedKey("player", i, "life"));
            player.alive = requiredBool(properties, indexedKey("player", i, "alive"));
            snapshot.players.push_back(player);
        }
        return snapshot;
    }

    std::string serializeEvent(const Event &event) {
        requireNonEmpty("Event type", event.type);
        Properties properties;
        set(properties, "eventId", std::to_string(event.eventId));
        set(properties, "type", event.type);
        set(properties, "payload", event.payload);
        return serializeProperties(properties);
    }

    Event deserializeEvent(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        Event event;
        event.eventId = requiredUint32(properties, "eventId");
        event.type = requiredString(properties, "type");
        event.payload = requiredString(properties, "payload");
        requireNonEmpty("Event type", event.type);
        return event;
    }

    std::string serializeDisconnect(const Disconnect &disconnect) {
        requireNonEmpty("Disconnect reason", disconnect.reason);
        Properties properties;
        set(properties, "reason", disconnect.reason);
        set(properties, "canReconnect", disconnect.canReconnect ? "true" : "false");
        return serializeProperties(properties);
    }

    Disconnect deserializeDisconnect(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        Disconnect disconnect;
        disconnect.reason = requiredString(properties, "reason");
        requireNonEmpty("Disconnect reason", disconnect.reason);
        disconnect.canReconnect = requiredBool(properties, "canReconnect");
        return disconnect;
    }

    std::string serializeClientConnectionConfig(const ClientConnectionConfig &config) {
        if (!config.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
        requireNonEmpty("Local server executable", config.localServerExecutable);
        Properties properties;
        set(properties, "mode", toString(config.mode));
        endpointToProperties(properties, "remoteEndpoint", config.remoteEndpoint);
        endpointToProperties(properties, "localEndpoint", config.localEndpoint);
        set(properties, "localServerExecutable", config.localServerExecutable);
        set(properties, "authToken", config.authToken);
        return serializeProperties(properties);
    }

    ClientConnectionConfig deserializeClientConnectionConfig(const std::string &payload) {
        const Properties properties = parseProperties(payload);
        ClientConnectionConfig config;
        config.mode = connectionModeFromString(requiredString(properties, "mode"));
        config.remoteEndpoint = endpointFromProperties(properties, "remoteEndpoint");
        config.localEndpoint = endpointFromProperties(properties, "localEndpoint");
        config.localServerExecutable = requiredString(properties, "localServerExecutable");
        config.authToken = requiredString(properties, "authToken");
        requireNonEmpty("Local server executable", config.localServerExecutable);
        if (!config.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
        return config;
    }
}
