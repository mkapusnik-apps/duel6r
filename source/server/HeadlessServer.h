#ifndef DUEL6_SERVER_HEADLESSSERVER_H
#define DUEL6_SERVER_HEADLESSSERVER_H

#include <iosfwd>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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
        virtual bool receive(Network::TransportFrame &frame) = 0;
        virtual Network::ClientState state() const = 0;
        virtual Network::TransportTimePoint acceptedAt() const = 0;
        virtual Network::TransportTimePoint terminalAt() const = 0;
        virtual bool permitAdmissionAcceptance() = 0;
        virtual void revokeAdmissionAcceptance() = 0;
        virtual void markAdmissionSucceeded() = 0;
        virtual void requestClose() = 0;
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
