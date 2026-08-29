#ifndef DUEL6_SERVER_HEADLESSSERVER_H
#define DUEL6_SERVER_HEADLESSSERVER_H

#include <iosfwd>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "ServerConfig.h"
#include "AdmissionSession.h"
#include "../network/CompatibilityManifest.h"
#include "../network/Protocol.h"
#include "../network/SessionTransport.h"

namespace Duel6::Server {
    struct HandshakeResult {
        bool accepted = false;
        Network::HandshakeAccept accept;
        Network::HandshakeReject reject;
    };

    enum class AdmissionLifecycleStage {
        ManifestBuildStarted,
        ManifestBuildFailed,
        HostInitialized,
        ListenerStarting,
        ListenerReady,
        ConnectionAccepted,
        RequestReceived,
        OfferPrepared,
        OfferQueued,
        AcceptanceReceived,
        TransactionCommitted,
        ConfirmationQueued,
        TransactionRolledBack,
        ConnectionClosed,
        ListenerCleanupStarted,
        ListenerCleanupCompleted
    };

    struct AdmissionLifecycleEvent {
        AdmissionLifecycleStage stage;
        Network::Trust::ConnectionId connectionId = 0;
        std::uint64_t transactionId = 0;
    };

    class AdmissionRuntimeConnection {
    public:
        virtual ~AdmissionRuntimeConnection() = default;
        virtual Network::SendResult send(std::vector<std::uint8_t> payload) = 0;
        virtual Network::AdmissionAcceptanceEnqueueResult enqueueAdmissionAcceptance(
                std::vector<std::uint8_t> payload,
                Network::AdmissionAttemptGate &attempt,
                const std::function<bool()> &cancelled,
                const Network::Trust::Clock &now,
                Network::TransportTimePoint deadline) {
            return attempt.enqueueAcceptance(
                    [this, &cancelled, &now, deadline, payload = std::move(payload)]() mutable {
                        bool cancellationRequested = true;
                        try { cancellationRequested = !cancelled || cancelled(); } catch (...) {}
                        if (cancellationRequested)
                            return Network::AdmissionAcceptanceEnqueueResult::Cancelled;
                        Network::TransportTimePoint current = Network::TransportTimePoint::max();
                        try { if (now) current = now(); } catch (...) {}
                        if (current >= deadline)
                            return Network::AdmissionAcceptanceEnqueueResult::DeadlineExceeded;
                        const auto sent = send(std::move(payload));
                        // Deterministic fake seams commonly invoke their pre-insertion hook from send().
                        // Production overrides this method and checks at the real queue insertion point.
                        try {
                            if (sent == Network::SendResult::Accepted && cancelled && cancelled())
                                return Network::AdmissionAcceptanceEnqueueResult::Cancelled;
                        } catch (...) { return Network::AdmissionAcceptanceEnqueueResult::Cancelled; }
                        return sent == Network::SendResult::Accepted
                               ? Network::AdmissionAcceptanceEnqueueResult::Accepted
                               : Network::AdmissionAcceptanceEnqueueResult::NotQueued;
                    });
        }
        virtual bool receive(Network::TransportFrame &frame) = 0;
        virtual Network::TransportInputSnapshot sealAndDrainInput() {
            Network::TransportInputSnapshot snapshot;
            Network::TransportFrame frame;
            while (receive(frame)) snapshot.frames.push_back(std::move(frame));
            snapshot.state = state();
            snapshot.terminalAt = terminalAt();
            return snapshot;
        }
        virtual Network::ClientState state() const = 0;
        virtual Network::TransportTimePoint acceptedAt() const = 0;
        virtual Network::TransportTimePoint terminalAt() const = 0;
        virtual bool permitAdmissionAcceptance() = 0;
        virtual void revokeAdmissionAcceptance() = 0;
        virtual void markAdmissionSucceeded() = 0;
        virtual void requestClose() = 0;
    };

    class AdmissionRuntimeClient {
    public:
        virtual ~AdmissionRuntimeClient() = default;
        virtual bool start(const Network::Endpoint &endpoint) = 0;
        virtual bool waitForConnected(std::chrono::milliseconds timeout) = 0;
        virtual Network::ClientState state() const = 0;
        virtual Network::TransportFailure failure() const = 0;
        virtual std::shared_ptr<AdmissionRuntimeConnection> connection() const = 0;
        virtual void cancel() = 0;
        virtual void close() = 0;
    };

    class AdmissionRuntimeListener {
    public:
        virtual ~AdmissionRuntimeListener() = default;
        virtual bool start(const Network::Endpoint &endpoint) = 0;
        virtual bool waitForReady(std::chrono::milliseconds timeout) = 0;
        virtual Network::ListenerState state() const = 0;
        virtual Network::TransportFailure failure() const = 0;
        virtual std::shared_ptr<AdmissionRuntimeConnection> acceptConnection() = 0;
        virtual void shutdown() = 0;
    };

    struct AdmissionRuntimeDependencies {
        Network::Trust::Clock now;
        std::function<bool()> cancelled;
        std::function<void(std::chrono::milliseconds)> wait;
        std::function<std::unique_ptr<AdmissionRuntimeClient>()> clientFactory;
        std::function<std::unique_ptr<AdmissionRuntimeListener>(std::size_t, bool)> listenerFactory;
        std::function<Network::SendResult(AdmissionRuntimeConnection &, std::vector<std::uint8_t>)> outboundWriter;
        std::function<bool(const AdmissionLifecycleEvent &)> lifecycleObserver;
        std::shared_ptr<const Network::ManifestSource> manifestSource;
        Network::ManifestFilesystemObserver filesystemObserver;
        IdentitySource identitySource;
        std::shared_ptr<Network::Trust::ConcurrentWorkLimiter> validationWorkLimiter;
        ValidationWorkGate validationWorkGate;
    };

    class HeadlessServer {
    private:
        ServerConfig config;
        std::uint32_t nextClientId;
        AdmissionRuntimeDependencies runtimeDependencies;

    public:
        explicit HeadlessServer(ServerConfig config, AdmissionRuntimeDependencies runtimeDependencies = {});

        const ServerConfig &getConfig() const;

        HandshakeResult validateHandshake(const Network::HandshakeRequest &request) const;
        Network::HandshakeAccept acceptHandshake(const Network::HandshakeRequest &request);

        int run(std::ostream &output);
    };
}

#endif
