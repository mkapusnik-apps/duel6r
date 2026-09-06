#ifndef DUEL6_NETWORK_PROTOCOLSERIALIZATION_H
#define DUEL6_NETWORK_PROTOCOLSERIALIZATION_H

#include <string>

#include "Protocol.h"

namespace Duel6::Network {
    std::string toString(ConnectionMode mode);
    ConnectionMode connectionModeFromString(const std::string &value);

    std::string toString(RejectReason reason);
    RejectReason rejectReasonFromString(const std::string &value);

    std::string serializeEndpoint(const Endpoint &endpoint);
    Endpoint deserializeEndpoint(const std::string &payload);

    std::string serializeHandshakeRequest(const HandshakeRequest &request);
    HandshakeRequest deserializeHandshakeRequest(const std::string &payload);

    std::string serializeHandshakeAccept(const HandshakeAccept &accept);
    HandshakeAccept deserializeHandshakeAccept(const std::string &payload);

    std::string serializeHandshakeReject(const HandshakeReject &reject);
    HandshakeReject deserializeHandshakeReject(const std::string &payload);

    std::string serializeLobbyState(const LobbyState &state);
    LobbyState deserializeLobbyState(const std::string &payload);

    std::string serializeInputCommand(const InputCommand &command);
    InputCommand deserializeInputCommand(const std::string &payload);

    std::string serializeSnapshot(const Snapshot &snapshot);
    Snapshot deserializeSnapshot(const std::string &payload);

    std::string serializeEvent(const Event &event);
    Event deserializeEvent(const std::string &payload);

    std::string serializeDisconnect(const Disconnect &disconnect);
    Disconnect deserializeDisconnect(const std::string &payload);

    std::string serializeClientConnectionConfig(const ClientConnectionConfig &config);
    ClientConnectionConfig deserializeClientConnectionConfig(const std::string &payload);
}

#endif
