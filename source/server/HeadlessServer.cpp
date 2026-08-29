#include "HeadlessServer.h"

#include <algorithm>
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

    Duel6::Network::Trust::TimePoint realNow() { return std::chrono::steady_clock::now(); }

    Duel6::Network::Trust::TimePoint runtimeNow(
            const Duel6::Server::AdmissionRuntimeDependencies &dependencies) noexcept {
        try { return dependencies.now ? dependencies.now() : realNow(); }
        catch (...) { return Duel6::Network::Trust::TimePoint::max(); }
    }

    template<typename Duration>
    Duel6::Network::Trust::TimePoint deadlineAfter(Duel6::Network::Trust::TimePoint start,
                                                    Duration duration) noexcept {
        const auto converted = std::chrono::duration_cast<Duel6::Network::Trust::TimePoint::duration>(duration);
        const auto latestStart = Duel6::Network::Trust::TimePoint::max() - converted;
        return start >= latestStart ? Duel6::Network::Trust::TimePoint::max() : start + converted;
    }

    class ProductionAdmissionConnection final : public Duel6::Server::AdmissionRuntimeConnection {
    public:
        explicit ProductionAdmissionConnection(std::shared_ptr<Duel6::Network::TcpConnection> connection)
                : connection(std::move(connection)) {}
        Duel6::Network::SendResult send(std::vector<std::uint8_t> payload) override {
            return connection->send(std::move(payload));
        }
        Duel6::Network::AdmissionAcceptanceEnqueueResult enqueueAdmissionAcceptance(
                std::vector<std::uint8_t> payload,
                Duel6::Network::AdmissionAttemptGate &attempt,
                const std::function<bool()> &cancelled,
                const Duel6::Network::Trust::Clock &now,
                Duel6::Network::TransportTimePoint deadline) override {
            return connection->enqueueAdmissionAcceptance(
                    std::move(payload), attempt, cancelled, now, deadline);
        }
        bool receive(Duel6::Network::TransportFrame &frame) override { return connection->receive(frame); }
        Duel6::Network::TransportInputSnapshot sealAndDrainInput() override {
            return connection->sealAndDrainInput();
        }
        Duel6::Network::ClientState state() const override { return connection->state(); }
        Duel6::Network::TransportTimePoint acceptedAt() const override { return connection->acceptedAt(); }
        Duel6::Network::TransportTimePoint terminalAt() const override { return connection->terminalAt(); }
        bool permitAdmissionAcceptance() override { return connection->permitAdmissionAcceptance(); }
        void revokeAdmissionAcceptance() override { connection->revokeAdmissionAcceptance(); }
        void markAdmissionSucceeded() override { connection->markAdmissionSucceeded(); }
        void requestClose() override { connection->requestClose(); }
    private:
        std::shared_ptr<Duel6::Network::TcpConnection> connection;
    };

    class ProductionAdmissionClient final : public Duel6::Server::AdmissionRuntimeClient {
    public:
        explicit ProductionAdmissionClient(Duel6::Network::SessionTransportDependencies dependencies)
                : client(std::move(dependencies)) {}
        bool start(const Duel6::Network::Endpoint &endpoint) override { return client.start(endpoint); }
        bool waitForConnected(std::chrono::milliseconds timeout) override {
            return client.waitForConnected(timeout);
        }
        Duel6::Network::ClientState state() const override { return client.state(); }
        Duel6::Network::TransportFailure failure() const override { return client.failure(); }
        std::shared_ptr<Duel6::Server::AdmissionRuntimeConnection> connection() const override {
            auto connected = client.connection();
            return connected ? std::make_shared<ProductionAdmissionConnection>(std::move(connected)) : nullptr;
        }
        void cancel() override { client.cancel(); }
        void close() override { client.close(); }
    private:
        Duel6::Network::TcpClient client;
    };

    class ProductionAdmissionListener final : public Duel6::Server::AdmissionRuntimeListener {
    public:
        ProductionAdmissionListener(std::size_t maxConnections, bool preAdmission,
                                    Duel6::Network::Trust::Clock now) {
            Duel6::Network::SessionTransportDependencies dependencies;
            dependencies.now = std::move(now);
            dependencies.enforceNetworkSessionPolicy = true;
            dependencies.enforcePreAdmissionPolicy = preAdmission;
            listener = std::make_unique<Duel6::Network::TcpListener>(maxConnections, std::move(dependencies));
        }
        bool start(const Duel6::Network::Endpoint &endpoint) override { return listener->start(endpoint); }
        bool waitForReady(std::chrono::milliseconds timeout) override { return listener->waitForReady(timeout); }
        Duel6::Network::ListenerState state() const override { return listener->state(); }
        Duel6::Network::TransportFailure failure() const override { return listener->failure(); }
        std::shared_ptr<Duel6::Server::AdmissionRuntimeConnection> acceptConnection() override {
            auto accepted = listener->acceptConnection();
            return accepted ? std::make_shared<ProductionAdmissionConnection>(std::move(accepted)) : nullptr;
        }
        void shutdown() override { listener->shutdown(); }
    private:
        std::unique_ptr<Duel6::Network::TcpListener> listener;
    };

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

    void printInvalidHostAdmissionMessage(std::ostream &output) {
        output << Duel6::Network::InvalidHostAdmissionMessageIdentifier << '\n'
               << Duel6::Network::InvalidHostAdmissionMessageCopy << '\n';
        output.flush();
    }

    int runAdmissionClient(const Duel6::Server::ServerConfig &config, std::ostream &output,
                           Duel6::Network::GameplayManifest manifest,
                           std::chrono::steady_clock::time_point deadline,
                           const Duel6::Server::AdmissionRuntimeDependencies &runtimeDependencies) {
        const auto cancelled = [&runtimeDependencies] {
            try { return runtimeDependencies.cancelled && runtimeDependencies.cancelled(); }
            catch (...) { return true; }
        };
        std::unique_ptr<Duel6::Server::AdmissionRuntimeClient> client;
        try { client = runtimeDependencies.clientFactory ? runtimeDependencies.clientFactory() : nullptr; }
        catch (...) {}
        if (!client) return 2;
        const auto closeClient = [&] { try { client->close(); } catch (...) {} };
        const auto cancelClient = [&] {
            try { client->cancel(); } catch (...) {}
            closeClient();
        };
        Duel6::Network::AdmissionAttemptGate attempt;
        const auto cancelAttempt = [&] {
            if (!cancelled() || !attempt.cancel()) return false;
            cancelClient();
            return true;
        };
        if (cancelAttempt()) return 2;
        bool started = false;
        try { started = client->start(config.listenEndpoint); } catch (...) {}
        if (!started) {
            if (cancelAttempt()) return 2;
            output << "Host unreachable.\n";
            closeClient();
            return 2;
        }
        bool connected = false;
        for (;;) {
            if (cancelAttempt()) return 2;
            const auto now = runtimeNow(runtimeDependencies);
            if (now >= deadline) break;
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const auto slice = std::min(remaining, std::chrono::milliseconds(5));
            if (slice <= std::chrono::milliseconds::zero()) break;
            try { connected = client->waitForConnected(slice); } catch (...) { break; }
            if (connected) break;
            Duel6::Network::ClientState clientState = Duel6::Network::ClientState::Failed;
            try { clientState = client->state(); } catch (...) { break; }
            if (clientState == Duel6::Network::ClientState::Failed
                || clientState == Duel6::Network::ClientState::Cancelled
                || clientState == Duel6::Network::ClientState::TimedOut
                || clientState == Duel6::Network::ClientState::Closed) break;
        }
        if (cancelAttempt()) return 2;
        if (!connected) {
            std::shared_ptr<Duel6::Server::AdmissionRuntimeConnection> interrupted;
            try { interrupted = client->connection(); } catch (...) {}
            Duel6::Network::TransportTimePoint terminalAt{};
            try { if (interrupted) terminalAt = interrupted->terminalAt(); } catch (...) {}
            if (terminalAt != Duel6::Network::TransportTimePoint{} && terminalAt < deadline) {
                output << "Connection ended before admission completed.\n";
                closeClient();
                return 2;
            }
            Duel6::Network::TransportFailure failure = Duel6::Network::TransportFailure::SystemError;
            try { failure = client->failure(); } catch (...) {}
            if (failure == Duel6::Network::TransportFailure::ResolveFailed)
                output << "Host name could not be resolved.\n";
            else if (failure == Duel6::Network::TransportFailure::ConnectionRefused
                     || failure == Duel6::Network::TransportFailure::Unreachable)
                output << "Host unreachable.\n";
            else
                output << "Connection timed out.\n";
            closeClient();
            return 2;
        }

        std::shared_ptr<Duel6::Server::AdmissionRuntimeConnection> connection;
        try { connection = client->connection(); } catch (...) {}
        if (cancelAttempt()) return 2;
        if (!connection) {
            output << "Connection ended before admission completed.\n";
            closeClient();
            return 2;
        }
        const auto request = Duel6::Network::makeLocalAdmissionRequest(config.localPlayers, std::move(manifest));
        if (runtimeNow(runtimeDependencies) >= deadline) {
            if (cancelAttempt()) return 2;
            output << "Connection timed out.\n";
            closeClient();
            return 2;
        }
        if (cancelAttempt()) return 2;
        Duel6::Network::SendResult requestSent = Duel6::Network::SendResult::NotConnected;
        try { requestSent = connection->send(Duel6::Network::serializeAdmissionRequest(request)); }
        catch (...) {}
        if (requestSent != Duel6::Network::SendResult::Accepted) {
            if (cancelAttempt()) return 2;
            output << "Connection ended before admission completed.\n";
            closeClient();
            return 2;
        }

        enum class GuestDecision { None, Cancelled, Rejected, Admitted, InvalidHost, Ended };
        struct GuestFrameDecision {
            GuestFrameDecision() : decision(GuestDecision::None), result() {}
            explicit GuestFrameDecision(GuestDecision decision) : decision(decision), result() {}
            GuestFrameDecision(GuestDecision decision, Duel6::Network::AdmissionResult result)
                    : decision(decision), result(std::move(result)) {}

            GuestDecision decision;
            Duel6::Network::AdmissionResult result;
        };
        std::optional<Duel6::Network::AdmissionOfferPayload> acceptedOffer;
        const auto processFrame = [&](const Duel6::Network::TransportFrame &frame) -> GuestFrameDecision {
            if (cancelled()) return GuestFrameDecision(GuestDecision::Cancelled);
            const bool beforeDeadline = frame.receivedAt < deadline;
            try {
                if (!acceptedOffer) {
                    try {
                        Duel6::Network::AdmissionOfferPayload offer =
                                Duel6::Network::deserializeAdmissionOffer(frame.payload);
                        if (!Duel6::Network::validAdmissionIdentitySet(offer, request.localPlayerCount))
                            throw std::invalid_argument("Admission offer does not match the request");
                        if (!beforeDeadline) return GuestFrameDecision();
                        Duel6::Network::AdmissionAcceptanceEnqueueResult queued =
                                Duel6::Network::AdmissionAcceptanceEnqueueResult::NotQueued;
                        try {
                            queued = connection->enqueueAdmissionAcceptance(
                                    Duel6::Network::serializeAdmissionAcceptance(offer), attempt,
                                    cancelled, runtimeDependencies.now, deadline);
                        } catch (...) {}
                        if (queued == Duel6::Network::AdmissionAcceptanceEnqueueResult::Cancelled)
                            return GuestFrameDecision(GuestDecision::Cancelled);
                        if (queued == Duel6::Network::AdmissionAcceptanceEnqueueResult::DeadlineExceeded)
                            return GuestFrameDecision();
                        if (queued != Duel6::Network::AdmissionAcceptanceEnqueueResult::Accepted)
                            return GuestFrameDecision(GuestDecision::Ended);
                        acceptedOffer = std::move(offer);
                        return GuestFrameDecision();
                    } catch (...) {
                        const Duel6::Network::AdmissionResult rejection =
                                Duel6::Network::deserializeAdmissionResult(frame.payload);
                        if (!beforeDeadline) return {};
                        return GuestFrameDecision(GuestDecision::Rejected, rejection);
                    }
                }

                const Duel6::Network::AdmissionConfirmation confirmation =
                        Duel6::Network::deserializeAdmissionConfirmation(frame.payload);
                if (!Duel6::Network::sameAdmissionIdentitySet(*acceptedOffer, confirmation))
                    throw std::invalid_argument("Admission confirmation does not match the offer");
                if (!beforeDeadline) return GuestFrameDecision();
                Duel6::Network::AdmissionResult result;
                result.code = Duel6::Network::AdmissionResultCode::Admitted;
                result.participantId = confirmation.participantId;
                result.playerIds = confirmation.playerIds;
                return GuestFrameDecision(GuestDecision::Admitted, std::move(result));
            } catch (...) {
                return GuestFrameDecision(GuestDecision::InvalidHost);
            }
        };
        const auto publish = [&](GuestFrameDecision decision) -> std::optional<int> {
            if (decision.decision == GuestDecision::None) return std::nullopt;
            if (decision.decision == GuestDecision::Cancelled) {
                if (attempt.cancel()) {
                    cancelClient();
                    return 2;
                }
                return std::nullopt;
            }
            if (cancelled()) {
                if (attempt.cancel()) {
                    cancelClient();
                    return 2;
                }
            }
            if (!attempt.finish()) return std::nullopt;
            switch (decision.decision) {
                case GuestDecision::Rejected:
                case GuestDecision::Admitted:
                    printAdmissionResult(output, decision.result);
                    closeClient();
                    return decision.decision == GuestDecision::Admitted ? 0 : 2;
                case GuestDecision::InvalidHost:
                    printInvalidHostAdmissionMessage(output);
                    closeClient();
                    return 2;
                case GuestDecision::Ended:
                    output << "Connection ended before admission completed.\n";
                    closeClient();
                    return 2;
                case GuestDecision::None:
                case GuestDecision::Cancelled:
                    break;
            }
            return std::nullopt;
        };
        const auto isTerminal = [](Duel6::Network::ClientState state) {
            return state == Duel6::Network::ClientState::Closed
                   || state == Duel6::Network::ClientState::Failed
                   || state == Duel6::Network::ClientState::Cancelled
                   || state == Duel6::Network::ClientState::TimedOut;
        };
        const auto sealAndFinish = [&]() -> int {
            Duel6::Network::TransportInputSnapshot snapshot;
            try { snapshot = connection->sealAndDrainInput(); }
            catch (...) {
                if (cancelAttempt()) return 2;
                output << "Connection ended before admission completed.\n";
                closeClient();
                return 2;
            }
            if (cancelAttempt()) return 2;
            for (const auto &queued: snapshot.frames) {
                if (const auto finished = publish(processFrame(queued))) return *finished;
            }
            if (cancelAttempt()) return 2;
            attempt.finish();
            if (snapshot.terminalAt != Duel6::Network::TransportTimePoint{}
                && snapshot.terminalAt < deadline) {
                output << "Connection ended before admission completed.\n";
            } else {
                output << "Connection timed out.\n";
            }
            closeClient();
            return 2;
        };

        for (;;) {
            if (cancelAttempt()) return 2;
            Duel6::Network::ClientState state = Duel6::Network::ClientState::Failed;
            try { state = connection->state(); } catch (...) {}
            if (runtimeNow(runtimeDependencies) >= deadline || isTerminal(state)) return sealAndFinish();

            Duel6::Network::TransportFrame frame;
            bool received = false;
            try { received = connection->receive(frame); }
            catch (...) {
                if (cancelAttempt()) return 2;
                output << "Connection ended before admission completed.\n";
                closeClient();
                return 2;
            }
            if (received) {
                if (const auto finished = publish(processFrame(frame))) return *finished;
                if (runtimeNow(runtimeDependencies) >= deadline) return sealAndFinish();
                continue;
            }

            state = Duel6::Network::ClientState::Failed;
            try { state = connection->state(); } catch (...) {}
            if (runtimeNow(runtimeDependencies) >= deadline || isTerminal(state)) return sealAndFinish();
            try {
                runtimeDependencies.wait(std::chrono::milliseconds(5));
            } catch (...) {
                cancelClient();
                return 2;
            }
        }
    }
}

namespace Duel6::Server {
    HeadlessServer::HeadlessServer(ServerConfig config, AdmissionRuntimeDependencies runtimeDependencies)
            : config(std::move(config)), nextClientId(1), runtimeDependencies(std::move(runtimeDependencies)) {
        if (this->config.serverName.empty() || this->config.buildVersion.empty()
            || this->config.listenEndpoint.host.empty() || this->config.listenEndpoint.port == 0
            || this->config.tickRate == 0 || this->config.maxClients == 0
            || this->config.maxClients > Network::MaxNetworkPlayers) {
            throw std::invalid_argument("Server configuration is invalid");
        }
        if (!this->config.authToken.empty()) {
            throw std::invalid_argument("Authentication is unsupported by the networking scaffold");
        }
        if (!this->runtimeDependencies.now) this->runtimeDependencies.now = realNow;
        if (!this->runtimeDependencies.cancelled) this->runtimeDependencies.cancelled = [] { return false; };
        if (!this->runtimeDependencies.wait) {
            this->runtimeDependencies.wait = [](std::chrono::milliseconds duration) {
                std::this_thread::sleep_for(duration);
            };
        }
        const Network::Trust::Clock clock = this->runtimeDependencies.now;
        const Network::Trust::Clock safeClock = [clock] {
            try { return clock(); }
            catch (...) { return Network::Trust::TimePoint::max(); }
        };
        if (!this->runtimeDependencies.clientFactory) {
            this->runtimeDependencies.clientFactory = [safeClock] {
                Network::SessionTransportDependencies dependencies;
                dependencies.now = safeClock;
                dependencies.enforceNetworkSessionPolicy = true;
                return std::make_unique<ProductionAdmissionClient>(std::move(dependencies));
            };
        }
        if (!this->runtimeDependencies.listenerFactory) {
            this->runtimeDependencies.listenerFactory = [safeClock](std::size_t maximum, bool preAdmission) {
                return std::make_unique<ProductionAdmissionListener>(maximum, preAdmission, safeClock);
            };
        }
        if (!this->runtimeDependencies.outboundWriter) {
            this->runtimeDependencies.outboundWriter = [](AdmissionRuntimeConnection &connection,
                                                          std::vector<std::uint8_t> payload) {
                return connection.send(std::move(payload));
            };
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
        const auto observe = [this](AdmissionLifecycleStage stage, Network::Trust::ConnectionId connection = 0,
                                    std::uint64_t transaction = 0) {
            if (!runtimeDependencies.lifecycleObserver) return true;
            try { return runtimeDependencies.lifecycleObserver({stage, connection, transaction}); }
            catch (...) { return false; }
        };
        const auto write = [this](AdmissionRuntimeConnection &connection, std::vector<std::uint8_t> payload) {
            try { return runtimeDependencies.outboundWriter(connection, std::move(payload)); }
            catch (...) { return Network::SendResult::NotConnected; }
        };
        const auto startupBegan = runtimeNow(runtimeDependencies);
        const auto startupDeadline = deadlineAfter(startupBegan, AdmissionAttemptDeadline);
        Network::ManifestBuildResult manifest;
        if (!config.transportEcho && (config.transportEnabled || config.admissionClient)) {
            if (!observe(AdmissionLifecycleStage::ManifestBuildStarted)) return 2;
            manifest = Network::CompatibilityManifestBuilder(
                    config.resourcePath, config.enabledGameplayScripts, runtimeDependencies.manifestSource,
                    runtimeDependencies.filesystemObserver).build();
            if (!manifest.valid()) {
                observe(AdmissionLifecycleStage::ManifestBuildFailed);
                if (config.admissionClient) {
                    try { if (runtimeDependencies.cancelled()) return 2; }
                    catch (...) { return 2; }
                }
                if (runtimeNow(runtimeDependencies) >= startupDeadline) {
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
            return runAdmissionClient(config, output, std::move(manifest.manifest), startupDeadline,
                                      runtimeDependencies);

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
        if (!config.transportEcho) {
            try {
                admissionPolicy = std::make_unique<AdmissionPolicy>(
                        std::move(manifest.manifest), config.localPlayers, runtimeDependencies.identitySource,
                        runtimeDependencies.validationWorkLimiter, runtimeDependencies.validationWorkGate);
            } catch (...) { return 2; }
            if (!observe(AdmissionLifecycleStage::HostInitialized)) return 2;
        }

        std::unique_ptr<AdmissionRuntimeListener> listener;
        try { listener = runtimeDependencies.listenerFactory(config.maxClients, !config.transportEcho); }
        catch (...) {}
        if (!listener) return 2;
        const auto cleanupListener = [&] {
            observe(AdmissionLifecycleStage::ListenerCleanupStarted);
            try { listener->shutdown(); } catch (...) {}
            observe(AdmissionLifecycleStage::ListenerCleanupCompleted);
        };
        if (!observe(AdmissionLifecycleStage::ListenerStarting)) {
            cleanupListener();
            return 2;
        }
        const auto startupRemaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                startupDeadline - runtimeNow(runtimeDependencies));
        if (startupRemaining <= std::chrono::milliseconds::zero()) {
            output << "duel6r-server transport startup failed (deadline expired).\n";
            cleanupListener();
            return 2;
        }
        bool listenerReady = false;
        try { listenerReady = listener->start(config.listenEndpoint)
                              && listener->waitForReady(startupRemaining); }
        catch (...) {}
        if (!listenerReady) {
            Network::ListenerState listenerState = Network::ListenerState::Failed;
            Network::TransportFailure listenerFailure = Network::TransportFailure::SystemError;
            try { listenerState = listener->state(); listenerFailure = listener->failure(); } catch (...) {}
            output << "duel6r-server transport startup failed (state="
                   << static_cast<int>(listenerState) << ", reason="
                   << static_cast<int>(listenerFailure) << ").\n";
            cleanupListener();
            return 2;
        }
        if (!observe(AdmissionLifecycleStage::ListenerReady)) {
            cleanupListener();
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
            std::shared_ptr<AdmissionRuntimeConnection> transport;
            std::chrono::steady_clock::time_point requestDeadline;
            std::chrono::steady_clock::time_point attemptDeadline;
            Network::AdmissionOfferPayload offer;
            std::uint64_t transactionId = 0;
            Network::Trust::ConnectionId connectionId = 0;
            bool requestReceived = false;
            bool admitted = false;
            RuntimeConnection(std::shared_ptr<AdmissionRuntimeConnection> transport,
                              Network::Trust::ConnectionId connectionId,
                              std::chrono::steady_clock::time_point acceptedAt)
                    : transport(std::move(transport)),
                      requestDeadline(deadlineAfter(acceptedAt, Network::Trust::FirstAdmissionRequestDeadline)),
                      attemptDeadline(deadlineAfter(acceptedAt, AdmissionAttemptDeadline)),
                      connectionId(connectionId) {}
        };
        std::vector<RuntimeConnection> connections;
        Network::Trust::ConnectionId nextConnectionId = 1;
        bool runtimeFailed = false;
        const auto cancelled = [this] {
            try { return runtimeDependencies.cancelled(); } catch (...) { return true; }
        };
        while (!stopRequested && !cancelled()) {
            while (true) {
                std::shared_ptr<AdmissionRuntimeConnection> connection;
                try { connection = listener->acceptConnection(); }
                catch (...) { runtimeFailed = true; break; }
                if (!connection) break;
                if (nextConnectionId == 0) {
                    try { connection->requestClose(); } catch (...) {}
                    continue;
                }
                const Network::Trust::ConnectionId connectionId = nextConnectionId++;
                if (!observe(AdmissionLifecycleStage::ConnectionAccepted, connectionId)) {
                    try { connection->requestClose(); } catch (...) {}
                    continue;
                }
                try {
                    const auto acceptedAt = connection->acceptedAt();
                    connections.emplace_back(std::move(connection), connectionId, acceptedAt);
                } catch (...) {
                    try { connection->requestClose(); } catch (...) {}
                }
            }
            if (runtimeFailed) break;
            for (auto iterator = connections.begin(); iterator != connections.end();) {
                auto &runtime = *iterator;
                auto &connection = runtime.transport;
                const auto rollback = [&] {
                    if (runtime.transactionId == 0 || !admissionPolicy) return;
                    const std::uint64_t transaction = runtime.transactionId;
                    runtime.transactionId = 0;
                    try { connection->revokeAdmissionAcceptance(); } catch (...) {}
                    try { admissionPolicy->rollback(transaction); } catch (...) {}
                    observe(AdmissionLifecycleStage::TransactionRolledBack, runtime.connectionId, transaction);
                };
                try {
                if (!runtime.requestReceived && runtimeNow(runtimeDependencies) >= runtime.requestDeadline) {
                    connection->requestClose();
                }
                if (!runtime.requestReceived) {
                    Network::TransportFrame frame;
                    if (connection->receive(frame)) {
                        runtime.requestReceived = true;
                        if (frame.receivedAt >= runtime.requestDeadline) {
                            connection->requestClose();
                        } else if (!observe(AdmissionLifecycleStage::RequestReceived, runtime.connectionId)) {
                            connection->requestClose();
                        } else if (config.transportEcho) {
                            runtime.admitted = true;
                            connection->markAdmissionSucceeded();
                            if (write(*connection, std::move(frame.payload)) != Network::SendResult::Accepted)
                                connection->requestClose();
                        } else {
                            AdmissionOffer offer = admissionPolicy->offerPayload(frame.payload);
                            if (offer.pending()) {
                                runtime.transactionId = offer.transactionId;
                                runtime.offer = {offer.result.participantId, offer.result.playerIds};
                                if (!observe(AdmissionLifecycleStage::OfferPrepared, runtime.connectionId,
                                             runtime.transactionId)
                                    || !connection->permitAdmissionAcceptance()) {
                                    rollback();
                                    connection->requestClose();
                                } else {
                                    const Network::SendResult sent = write(
                                            *connection, Network::serializeAdmissionOffer(runtime.offer));
                                    if (sent != Network::SendResult::Accepted
                                        || !observe(AdmissionLifecycleStage::OfferQueued, runtime.connectionId,
                                                    runtime.transactionId)) {
                                        rollback();
                                        connection->requestClose();
                                    }
                                }
                            } else {
                                write(*connection, Network::serializeAdmissionResult(offer.result));
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
                            accepted = frame.receivedAt < runtime.attemptDeadline
                                       && Network::sameAdmissionIdentitySet(acceptance, runtime.offer)
                                       && observe(AdmissionLifecycleStage::AcceptanceReceived,
                                                  runtime.connectionId, runtime.transactionId)
                                       && admissionPolicy->commit(runtime.transactionId, runtime.connectionId);
                        } catch (...) {}
                        if (accepted) {
                            runtime.admitted = true;
                            const std::uint64_t committedTransaction = runtime.transactionId;
                            runtime.transactionId = 0;
                            connection->markAdmissionSucceeded();
                            const bool observed = observe(AdmissionLifecycleStage::TransactionCommitted,
                                                          runtime.connectionId, committedTransaction);
                            const bool confirmationObserved = observed
                                    && observe(AdmissionLifecycleStage::ConfirmationQueued, runtime.connectionId,
                                               committedTransaction);
                            const Network::SendResult confirmed = confirmationObserved
                                    ? write(*connection, Network::serializeAdmissionConfirmation(runtime.offer))
                                    : Network::SendResult::NotConnected;
                            if (confirmed == Network::SendResult::Accepted) {
                                Network::AdmissionResult result;
                                result.code = Network::AdmissionResultCode::Admitted;
                                result.participantId = runtime.offer.participantId;
                                result.playerIds = runtime.offer.playerIds;
                                printAdmissionResult(output, result);
                            } else {
                                connection->requestClose();
                            }
                        } else {
                            rollback();
                            connection->requestClose();
                        }
                    } else if (runtimeNow(runtimeDependencies) >= runtime.attemptDeadline) {
                        rollback();
                        connection->requestClose();
                    }
                } else if (config.transportEcho && runtime.admitted) {
                    Network::TransportFrame frame;
                    while (connection->receive(frame)) {
                        if (write(*connection, std::move(frame.payload)) != Network::SendResult::Accepted) {
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
                    rollback();
                    if (runtime.admitted && !config.transportEcho)
                        admissionPolicy->disconnect(runtime.connectionId);
                    observe(AdmissionLifecycleStage::ConnectionClosed, runtime.connectionId);
                    iterator = connections.erase(iterator);
                } else {
                    ++iterator;
                }
                } catch (...) {
                    rollback();
                    try { connection->requestClose(); } catch (...) {}
                    if (runtime.admitted && !config.transportEcho) {
                        try { admissionPolicy->disconnect(runtime.connectionId); } catch (...) {}
                    }
                    observe(AdmissionLifecycleStage::ConnectionClosed, runtime.connectionId);
                    iterator = connections.erase(iterator);
                }
            }
            try { runtimeDependencies.wait(std::chrono::milliseconds(5)); }
            catch (...) { break; }
        }

        if (admissionPolicy) {
            for (auto &runtime: connections) {
                if (runtime.transactionId != 0) {
                    const std::uint64_t transaction = runtime.transactionId;
                    try { runtime.transport->revokeAdmissionAcceptance(); } catch (...) {}
                    try { admissionPolicy->rollback(transaction); } catch (...) {}
                    observe(AdmissionLifecycleStage::TransactionRolledBack, runtime.connectionId, transaction);
                }
                if (runtime.admitted) {
                    try { admissionPolicy->disconnect(runtime.connectionId); } catch (...) {}
                }
            }
        }

        cleanupListener();
        output << "duel6r-server transport stopped.\n";
        return 0;
    }
}
