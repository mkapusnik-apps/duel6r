#include "HeadlessServer.h"

#include <array>
#include <chrono>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "../network/SessionTransport.h"
#include "../network/NetworkTrustPolicy.h"

namespace {
    volatile std::sig_atomic_t stopRequested = 0;

    void requestStop(int) {
        stopRequested = 1;
    }
}

namespace Duel6::Server {
    HeadlessServer::HeadlessServer(ServerConfig config)
            : config(std::move(config)), nextClientId(1) {
        if (this->config.serverName.empty() || this->config.buildVersion.empty()
            || this->config.listenEndpoint.host.empty() || this->config.listenEndpoint.port == 0
            || this->config.tickRate == 0 || this->config.maxClients == 0
            || this->config.maxClients > Network::MaxNetworkPlayers) {
            throw std::invalid_argument("Server configuration is invalid");
        }
        if (!this->config.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
    }

    const ServerConfig &HeadlessServer::getConfig() const {
        return config;
    }

    HandshakeResult HeadlessServer::validateHandshake(const Network::HandshakeRequest &request) const {
        HandshakeResult result;
        if (request.protocolVersion != Network::ProtocolVersion) {
            result.reject.reason = Network::RejectReason::IncompatibleProtocol;
            result.reject.detail = "Client protocol version is incompatible";
            return result;
        }

        if (request.buildVersion.empty()) {
            result.reject.reason = Network::RejectReason::InvalidRequest;
            result.reject.detail = "Client build version is required";
            return result;
        }

        if (request.buildVersion != config.buildVersion) {
            result.reject.reason = Network::RejectReason::IncompatibleBuild;
            result.reject.detail = "Client build version is incompatible";
            return result;
        }

        if (request.clientName.empty()) {
            result.reject.reason = Network::RejectReason::InvalidRequest;
            result.reject.detail = "Client name is required";
            return result;
        }

        if (request.clientName.size() > Network::MaxProtocolStringBytes
            || request.resources.size() > Network::MaxProtocolCollectionEntries) {
            result.reject.reason = Network::RejectReason::InvalidRequest;
            result.reject.detail = "Client handshake exceeds scaffold limits";
            return result;
        }

        for (const Network::ResourceHash &resource: request.resources) {
            if (resource.path.empty() || resource.hash.empty()
                || resource.path.size() > Network::MaxProtocolStringBytes
                || resource.hash.size() > Network::MaxProtocolStringBytes) {
                result.reject.reason = Network::RejectReason::InvalidRequest;
                result.reject.detail = "Client resource hash entries must contain bounded path and hash values";
                return result;
            }
        }

        if (!request.authToken.empty()) {
            result.reject.reason = Network::RejectReason::AuthenticationFailed;
            result.reject.detail = "Authentication is unsupported by the networking scaffold";
            return result;
        }

        if (nextClientId > config.maxClients) {
            result.reject.reason = Network::RejectReason::ServerFull;
            result.reject.detail = "The scaffold has no remaining client IDs";
            return result;
        }

        result.accepted = true;
        result.accept.serverTickRate = config.tickRate;
        result.accept.serverName = config.serverName;
        return result;
    }

    Network::HandshakeAccept HeadlessServer::acceptHandshake(const Network::HandshakeRequest &request) {
        HandshakeResult result = validateHandshake(request);
        if (!result.accepted) {
            throw std::invalid_argument(result.reject.detail);
        }

        result.accept.clientId = nextClientId++;
        if (result.accept.clientId == 0 || nextClientId == 0) {
            throw std::overflow_error("Client ID space exhausted");
        }
        return result.accept;
    }

    int HeadlessServer::run(std::ostream &output) {
        if (!config.transportEnabled) {
            output << "duel6r-server configuration valid for " << config.listenEndpoint.host << ':'
                   << config.listenEndpoint.port << ".\n"
                   << "unsupported: no network transport or playable remote-session runtime is implemented; "
                   << "the server did not listen for clients.\n";
            return 2;
        }

        std::array<std::uint8_t, 4> listenAddress{};
        auto endpointScope = Network::Trust::classifyIpv4Literal(config.listenEndpoint.host, &listenAddress);
        const bool loopbackHostname = config.listenEndpoint.host == "localhost";
        if (loopbackHostname) endpointScope = Network::Trust::EndpointScope::Loopback;
        if ((endpointScope != Network::Trust::EndpointScope::Loopback
             && endpointScope != Network::Trust::EndpointScope::PrivateLan)
            || (!loopbackHostname
                && Network::Trust::localListenerBindDecision(listenAddress)
                   != Network::Trust::LocalListenerBindDecision::Allowed)) {
            output << Network::Trust::UnsupportedAddressCopy << '\n';
            return 2;
        }

        output << (endpointScope == Network::Trust::EndpointScope::Loopback
                   ? Network::Trust::LoopbackExposureCopy : Network::Trust::PrivateLanExposureCopy) << '\n';
        output.flush();

        stopRequested = 0;
        std::signal(SIGINT, requestStop);
        std::signal(SIGTERM, requestStop);

        Network::SessionTransportDependencies transportDependencies;
        transportDependencies.enforceNetworkSessionPolicy = true;
        transportDependencies.enforcePreAdmissionPolicy = !config.transportEcho;
        Network::TcpListener listener(config.maxClients, std::move(transportDependencies));
        if (!listener.start(config.listenEndpoint)
            || !listener.waitForReady(std::chrono::seconds(10))) {
            output << "duel6r-server transport startup failed (state="
                   << static_cast<int>(listener.state()) << ", reason="
                   << static_cast<int>(listener.failure()) << ").\n";
            listener.shutdown();
            return 2;
        }

        output << "duel6r-server transport ready on " << config.listenEndpoint.host << ':'
               << config.listenEndpoint.port << ".\n"
               << "scaffold warning: transport is active, but no lobby, admission, simulation, or playable "
               << "network session is implemented.\n";
        if (config.transportEcho) {
            output << "diagnostic echo is active; received opaque application frames are returned only "
                   << "to their originating connection.\n";
        }
        output.flush();

        struct RuntimeConnection {
            std::shared_ptr<Network::TcpConnection> transport;
            Network::Trust::AdmissionGate admission;
            RuntimeConnection(std::shared_ptr<Network::TcpConnection> transport,
                              Network::Trust::AdmissionHook hook)
                    : transport(std::move(transport)), admission(std::move(hook)) {}
        };
        std::vector<RuntimeConnection> connections;
        while (!stopRequested) {
            while (auto connection = listener.acceptConnection()) {
                const bool echo = config.transportEcho;
                connections.emplace_back(std::move(connection), [echo](const std::vector<std::uint8_t> &) {
                    return echo ? Network::Trust::AdmissionOutcome::Accepted
                                : Network::Trust::AdmissionOutcome::HostPolicyRejected;
                });
            }
            for (auto iterator = connections.begin(); iterator != connections.end();) {
                auto &runtime = *iterator;
                auto &connection = runtime.transport;
                if (runtime.admission.state() == Network::Trust::AdmissionGate::State::AwaitingRequest
                    && runtime.admission.expireIfDue()) {
                    const auto copy = Network::Trust::outcomeUserCopy(runtime.admission.outcome());
                    connection->send(std::vector<std::uint8_t>(copy.begin(), copy.end()));
                    connection->requestClose();
                }
                if (runtime.admission.state() == Network::Trust::AdmissionGate::State::AwaitingRequest) {
                    Network::TransportFrame frame;
                    if (connection->receive(frame)) {
                        const auto outcome = runtime.admission.submit(frame.payload);
                        if (outcome == Network::Trust::AdmissionOutcome::Accepted) {
                            connection->markAdmissionSucceeded();
                            if (config.transportEcho
                                && connection->send(std::move(frame.payload)) != Network::SendResult::Accepted)
                                connection->requestClose();
                        } else {
                            const auto copy = Network::Trust::outcomeUserCopy(outcome);
                            connection->send(std::vector<std::uint8_t>(copy.begin(), copy.end()));
                            connection->requestClose();
                        }
                    }
                } else if (config.transportEcho
                           && runtime.admission.state() == Network::Trust::AdmissionGate::State::Accepted) {
                    Network::TransportFrame frame;
                    while (connection->receive(frame)) {
                        if (connection->send(std::move(frame.payload)) != Network::SendResult::Accepted) {
                            connection->requestClose();
                            break;
                        }
                    }
                }
                Network::ClientState state = connection->state();
                if (state == Network::ClientState::Closed || state == Network::ClientState::Failed
                    || state == Network::ClientState::Cancelled || state == Network::ClientState::TimedOut) {
                    iterator = connections.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        listener.shutdown();
        output << "duel6r-server transport stopped.\n";
        return 0;
    }
}
