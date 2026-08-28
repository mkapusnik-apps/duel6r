#include "HeadlessServer.h"

#include <array>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "../network/SessionTransport.h"
#include "../network/NetworkTrustPolicy.h"
#include "../network/AdmissionProtocol.h"
#include "../network/CompatibilityManifest.h"
#include "AdmissionSession.h"

namespace {
    volatile std::sig_atomic_t stopRequested = 0;

    void requestStop(int) {
        stopRequested = 1;
    }

    constexpr auto AdmissionAttemptDeadline = std::chrono::seconds(10);

    void printAdmissionResult(std::ostream &output, const Duel6::Network::AdmissionResult &result) {
        output << Duel6::Network::admissionResultIdentifier(result.code) << '\n';
        const std::string_view copy = Duel6::Network::admissionResultUserCopy(result.code);
        if (!copy.empty()) output << copy << '\n';
        if (result.admitted()) {
            output << "participant-id=" << result.participantId << " player-ids=";
            for (std::size_t index = 0; index < result.playerIds.size(); ++index) {
                if (index) output << ',';
                output << result.playerIds[index];
            }
            output << '\n';
        }
        output.flush();
    }

    int runAdmissionClient(const Duel6::Server::ServerConfig &config, std::ostream &output,
                           Duel6::Network::GameplayManifest manifest,
                           std::chrono::steady_clock::time_point deadline) {
        Duel6::Network::SessionTransportDependencies dependencies;
        dependencies.enforceNetworkSessionPolicy = true;
        Duel6::Network::TcpClient client(std::move(dependencies));
        if (!client.start(config.listenEndpoint)) {
            output << "Host unreachable.\n";
            return 2;
        }
        const auto connectRemaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
        if (connectRemaining <= std::chrono::milliseconds::zero()
            || !client.waitForConnected(connectRemaining)) {
            const auto interrupted = client.connection();
            if (interrupted && interrupted->terminalAt() != Duel6::Network::TransportTimePoint{}
                && interrupted->terminalAt() < deadline) {
                output << "Connection ended before admission completed.\n";
                client.close();
                return 2;
            }
            const auto failure = client.failure();
            if (failure == Duel6::Network::TransportFailure::ResolveFailed)
                output << "Host name could not be resolved.\n";
            else if (failure == Duel6::Network::TransportFailure::ConnectionRefused
                     || failure == Duel6::Network::TransportFailure::Unreachable)
                output << "Host unreachable.\n";
            else
                output << "Connection timed out.\n";
            client.close();
            return 2;
        }

        auto connection = client.connection();
        if (!connection) {
            output << "Connection ended before admission completed.\n";
            client.close();
            return 2;
        }
        const auto request = Duel6::Network::makeLocalAdmissionRequest(config.localPlayers, std::move(manifest));
        if (connection->send(Duel6::Network::serializeAdmissionRequest(request))
            != Duel6::Network::SendResult::Accepted) {
            output << "Connection ended before admission completed.\n";
            client.close();
            return 2;
        }

        while (std::chrono::steady_clock::now() < deadline) {
            Duel6::Network::TransportFrame frame;
            if (connection->receive(frame)) {
                if (frame.receivedAt >= deadline) {
                    output << "Connection timed out.\n";
                    client.close();
                    return 2;
                }
                try {
                    const auto result = Duel6::Network::deserializeAdmissionResult(frame.payload);
                    if (result.admitted()
                        && connection->send(Duel6::Network::serializeAdmissionAcceptance({result.participantId}))
                           != Duel6::Network::SendResult::Accepted) {
                        output << "Connection ended before admission completed.\n";
                        client.close();
                        return 2;
                    }
                    printAdmissionResult(output, result);
                    client.close();
                    return result.admitted() ? 0 : 2;
                } catch (...) {
                    output << "Connection request rejected.\n";
                    client.close();
                    return 2;
                }
            }
            const auto state = connection->state();
            if (state == Duel6::Network::ClientState::Closed || state == Duel6::Network::ClientState::Failed
                || state == Duel6::Network::ClientState::Cancelled || state == Duel6::Network::ClientState::TimedOut) {
                output << "Connection ended before admission completed.\n";
                client.close();
                return 2;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const auto terminalAt = connection->terminalAt();
        if (terminalAt != Duel6::Network::TransportTimePoint{} && terminalAt < deadline) {
            output << "Connection ended before admission completed.\n";
            client.close();
            return 2;
        }
        output << "Connection timed out.\n";
        client.close();
        return 2;
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
        const auto startupBegan = std::chrono::steady_clock::now();
        const auto startupDeadline = startupBegan + AdmissionAttemptDeadline;
        Network::ManifestBuildResult manifest;
        if (!config.transportEcho && (config.transportEnabled || config.admissionClient)) {
            manifest = Network::CompatibilityManifestBuilder(
                    config.resourcePath, config.enabledGameplayScripts).build();
            if (!manifest.valid()) {
                if (std::chrono::steady_clock::now() >= startupDeadline) {
                    output << (config.admissionClient ? "Connection timed out.\n"
                                                      : "duel6r-server transport startup failed (deadline expired).\n");
                } else if (config.admissionClient) {
                    output << "guest-gameplay-content-manifest-invalid\n"
                           << "Local gameplay content is invalid. Restore the supported gameplay content and restart the application.\n";
                } else {
                    output << "host-gameplay-content-manifest-invalid\n"
                           << "Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.\n";
                }
                return 2;
            }
        }

        if (config.admissionClient)
            return runAdmissionClient(config, output, std::move(manifest.manifest), startupDeadline);

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

        std::unique_ptr<AdmissionPolicy> admissionPolicy;
        if (!config.transportEcho)
            admissionPolicy = std::make_unique<AdmissionPolicy>(std::move(manifest.manifest), config.localPlayers);

        Network::SessionTransportDependencies transportDependencies;
        transportDependencies.enforceNetworkSessionPolicy = true;
        transportDependencies.enforcePreAdmissionPolicy = !config.transportEcho;
        Network::TcpListener listener(config.maxClients, std::move(transportDependencies));
        const auto startupRemaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                startupDeadline - std::chrono::steady_clock::now());
        if (startupRemaining <= std::chrono::milliseconds::zero()) {
            output << "duel6r-server transport startup failed (deadline expired).\n";
            return 2;
        }
        if (!listener.start(config.listenEndpoint)
            || !listener.waitForReady(startupRemaining)) {
            output << "duel6r-server transport startup failed (state="
                   << static_cast<int>(listener.state()) << ", reason="
                   << static_cast<int>(listener.failure()) << ").\n";
            listener.shutdown();
            return 2;
        }

        output << "duel6r-server transport ready on " << config.listenEndpoint.host << ':'
               << config.listenEndpoint.port << ".\n";
        if (config.transportEcho) {
            output << "scaffold warning: transport is active, but no lobby, admission, simulation, or playable "
                   << "network session is implemented.\n"
                   << "diagnostic echo is active; received opaque application frames are returned only "
                   << "to their originating connection.\n";
        } else {
            output << "scaffold warning: transport and compatibility admission are active, but no lobby, "
                   << "simulation, or playable network session is implemented.\n";
        }
        output.flush();

        struct RuntimeConnection {
            std::shared_ptr<Network::TcpConnection> transport;
            std::chrono::steady_clock::time_point requestDeadline;
            std::chrono::steady_clock::time_point acceptanceDeadline{};
            Network::AdmissionResult offer;
            std::uint64_t transactionId = 0;
            Network::Trust::ConnectionId connectionId = 0;
            bool requestReceived = false;
            bool admitted = false;
            RuntimeConnection(std::shared_ptr<Network::TcpConnection> transport,
                              Network::Trust::ConnectionId connectionId)
                    : transport(std::move(transport)),
                      requestDeadline(this->transport->acceptedAt() + Network::Trust::FirstAdmissionRequestDeadline),
                      connectionId(connectionId) {}
        };
        std::vector<RuntimeConnection> connections;
        Network::Trust::ConnectionId nextConnectionId = 1;
        while (!stopRequested) {
            while (auto connection = listener.acceptConnection()) {
                if (nextConnectionId == 0) {
                    connection->requestClose();
                    continue;
                }
                connections.emplace_back(std::move(connection), nextConnectionId++);
            }
            for (auto iterator = connections.begin(); iterator != connections.end();) {
                auto &runtime = *iterator;
                auto &connection = runtime.transport;
                if (!runtime.requestReceived && std::chrono::steady_clock::now() >= runtime.requestDeadline) {
                    connection->requestClose();
                }
                if (!runtime.requestReceived) {
                    Network::TransportFrame frame;
                    if (connection->receive(frame)) {
                        runtime.requestReceived = true;
                        if (frame.receivedAt >= runtime.requestDeadline) {
                            connection->requestClose();
                        } else if (config.transportEcho) {
                            runtime.admitted = true;
                            connection->markAdmissionSucceeded();
                            if (connection->send(std::move(frame.payload)) != Network::SendResult::Accepted)
                                connection->requestClose();
                        } else {
                            AdmissionOffer offer = admissionPolicy->offerPayload(frame.payload);
                            const Network::SendResult sent = connection->send(
                                    Network::serializeAdmissionResult(offer.result));
                            if (offer.pending() && sent == Network::SendResult::Accepted) {
                                runtime.offer = std::move(offer.result);
                                runtime.transactionId = offer.transactionId;
                                runtime.acceptanceDeadline = frame.receivedAt
                                        + Network::Trust::AdmissionAcceptanceDeadline;
                                connection->permitAdmissionAcceptance();
                            } else {
                                if (offer.pending()) admissionPolicy->rollback(offer.transactionId);
                                connection->requestClose();
                            }
                        }
                    }
                } else if (!config.transportEcho && runtime.transactionId != 0 && !runtime.admitted) {
                    Network::TransportFrame frame;
                    if (connection->receive(frame)) {
                        bool accepted = false;
                        try {
                            const Network::AdmissionAcceptance acceptance =
                                    Network::deserializeAdmissionAcceptance(frame.payload);
                            accepted = frame.receivedAt < runtime.acceptanceDeadline
                                       && acceptance.participantId == runtime.offer.participantId
                                       && admissionPolicy->commit(runtime.transactionId, runtime.connectionId);
                        } catch (...) {}
                        if (accepted) {
                            runtime.admitted = true;
                            runtime.transactionId = 0;
                            connection->markAdmissionSucceeded();
                            printAdmissionResult(output, runtime.offer);
                        } else {
                            admissionPolicy->rollback(runtime.transactionId);
                            runtime.transactionId = 0;
                            connection->requestClose();
                        }
                    } else if (std::chrono::steady_clock::now() >= runtime.acceptanceDeadline) {
                        admissionPolicy->rollback(runtime.transactionId);
                        runtime.transactionId = 0;
                        connection->requestClose();
                    }
                } else if (config.transportEcho && runtime.admitted) {
                    Network::TransportFrame frame;
                    while (connection->receive(frame)) {
                        if (connection->send(std::move(frame.payload)) != Network::SendResult::Accepted) {
                            connection->requestClose();
                            break;
                        }
                    }
                } else if (runtime.admitted) {
                    Network::TransportFrame unexpected;
                    if (connection->receive(unexpected)) connection->requestClose();
                }
                Network::ClientState state = connection->state();
                if (state == Network::ClientState::Closed || state == Network::ClientState::Failed
                    || state == Network::ClientState::Cancelled || state == Network::ClientState::TimedOut) {
                    if (runtime.transactionId != 0) admissionPolicy->rollback(runtime.transactionId);
                    if (runtime.admitted && !config.transportEcho)
                        admissionPolicy->disconnect(runtime.connectionId);
                    iterator = connections.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        if (admissionPolicy) {
            for (auto &runtime: connections) {
                if (runtime.transactionId != 0) admissionPolicy->rollback(runtime.transactionId);
                if (runtime.admitted) admissionPolicy->disconnect(runtime.connectionId);
            }
        }

        listener.shutdown();
        output << "duel6r-server transport stopped.\n";
        return 0;
    }
}
