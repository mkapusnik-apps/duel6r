#include <algorithm>
#include <deque>
#include <list>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "tests/HostEndBoundaryControl.h"
#include "source/network/SessionLifecycle.h"
#include "source/network/StateReplicationProtocol.h"
#include "source/client/NetworkSessionRuntime.h"

// Use the existing test-only access convention, not a shipped diagnostic API.
#define private public
#include "source/server/HeadlessServer.h"
#undef private

namespace Duel6::Test {
    namespace {
        std::mutex controlMutex;
        std::shared_ptr<HostEndBoundaryControl> installedControl;

        class BoundaryConnection final : public Server::AdmissionRuntimeConnection {
            std::shared_ptr<Server::AdmissionRuntimeConnection> connection;
            std::shared_ptr<HostEndBoundaryControl> control;
            std::vector<Network::TransportFrame> held;
            std::size_t heldBytes = 0;

        public:
            BoundaryConnection(std::shared_ptr<Server::AdmissionRuntimeConnection> connection,
                               std::shared_ptr<HostEndBoundaryControl> control)
                    : connection(std::move(connection)), control(std::move(control)) {}
            ~BoundaryConnection() override {
                for (auto &frame: held) Network::Trust::secureEraseMemory(frame.payload.data(), frame.payload.size());
            }
            Network::SendResult send(std::vector<std::uint8_t> payload) override {
                const auto frame = Network::Replication::deserializeReplicationFrame(payload);
                const auto result = connection->send(std::move(payload));
                if (control->armed && frame && frame->kind == Network::Replication::ReplicationFrameKind::QualityProbe
                    && (result == Network::SendResult::NotConnected || result == Network::SendResult::Closing))
                    ++control->rejectedProbes;
                return result; // Never fabricate an outbound failure.
            }
            Network::SendResult sendSensitive(std::vector<std::uint8_t> payload) override {
                return connection->sendSensitive(std::move(payload));
            }
            Network::AdmissionAcceptanceEnqueueResult enqueueAdmissionAcceptance(
                    std::vector<std::uint8_t> payload, Network::AdmissionAttemptGate &attempt,
                    const std::function<bool()> &cancelled, const Network::Trust::Clock &now,
                    Network::TransportTimePoint deadline) override {
                return connection->enqueueAdmissionAcceptance(std::move(payload), attempt, cancelled, now, deadline);
            }
            bool receive(Network::TransportFrame &frame) override {
                if (!control->armed) return connection->receive(frame);
                // Retain real post-arm frames in original order. This delays
                // consumption, not their transport timestamps or canonical state.
                Network::TransportFrame received;
                while (held.size() < Network::MaxQueuedTransportFrames && connection->receive(received)) {
                    heldBytes += received.payload.size();
                    if (heldBytes > Network::MaxQueuedTransportPayloadBytes) {
                        Network::Trust::secureEraseMemory(received.payload.data(), received.payload.size());
                        throw std::runtime_error("Close-boundary harness buffer limit reached");
                    }
                    if (Network::Lifecycle::deserializeIntentionalHostEnd(received.payload)) ++control->notices;
                    held.push_back(std::move(received));
                }
                return false;
            }
            Network::TransportInputSnapshot sealAndDrainInput(
                    std::size_t maximumFrames = Network::MaxQueuedTransportFrames) override {
                maximumFrames = std::min(maximumFrames, Network::MaxQueuedTransportFrames);
                auto snapshot = connection->sealAndDrainInput(
                        maximumFrames > held.size() ? maximumFrames - held.size() : 0);
                std::size_t bytes = heldBytes;
                for (const auto &frame: snapshot.frames) bytes += frame.payload.size();
                if (held.size() + snapshot.frames.size() > Network::MaxQueuedTransportFrames
                    || bytes > Network::MaxQueuedTransportPayloadBytes) {
                    for (auto &frame: snapshot.frames)
                        Network::Trust::secureEraseMemory(frame.payload.data(), frame.payload.size());
                    throw std::runtime_error("Close-boundary harness sealed buffer limit reached");
                }
                held.insert(held.end(), std::make_move_iterator(snapshot.frames.begin()),
                            std::make_move_iterator(snapshot.frames.end()));
                snapshot.frames.clear();
                const auto count = std::min(maximumFrames, held.size());
                snapshot.frames.insert(snapshot.frames.end(), std::make_move_iterator(held.begin()),
                                       std::make_move_iterator(held.begin() + count));
                held.erase(held.begin(), held.begin() + count);
                heldBytes = 0;
                for (const auto &frame: held) heldBytes += frame.payload.size();
                for (const auto &frame: snapshot.frames) {
                    if (Network::Lifecycle::deserializeIntentionalHostEnd(frame.payload)) {
                        ++control->sealedNotices;
                        control->eligibleReceipt = snapshot.terminalAt != Network::TransportTimePoint{}
                                && frame.receivedAt <= snapshot.terminalAt;
                    }
                }
                return snapshot;
            }
            Network::ClientState state() const override {
                // Model delayed terminal observation until the due probe sees
                // the real closed socket. UI/session state is never assigned.
                if (control->armed && control->rejectedProbes == 0) return Network::ClientState::Connected;
                return connection->state();
            }
            Network::TransportTimePoint acceptedAt() const override { return connection->acceptedAt(); }
            Network::TransportTimePoint terminalAt() const override { return connection->terminalAt(); }
            bool permitAdmissionAcceptance() override { return connection->permitAdmissionAcceptance(); }
            void revokeAdmissionAcceptance() override { connection->revokeAdmissionAcceptance(); }
            void markAdmissionSucceeded() override { connection->markAdmissionSucceeded(); }
            void requestClose() override { connection->requestClose(); }
        };

        class BoundaryClient final : public Server::AdmissionRuntimeClient {
            std::unique_ptr<Server::AdmissionRuntimeClient> client;
            std::shared_ptr<HostEndBoundaryControl> control;
        public:
            BoundaryClient(std::unique_ptr<Server::AdmissionRuntimeClient> client,
                           std::shared_ptr<HostEndBoundaryControl> control)
                    : client(std::move(client)), control(std::move(control)) {}
            bool start(const Network::Endpoint &endpoint) override { return client->start(endpoint); }
            bool waitForConnected(std::chrono::milliseconds timeout) override { return client->waitForConnected(timeout); }
            Network::ClientState state() const override { return client->state(); }
            Network::TransportFailure failure() const override { return client->failure(); }
            std::shared_ptr<Server::AdmissionRuntimeConnection> connection() const override {
                auto connected = client->connection();
                return connected ? std::make_shared<BoundaryConnection>(std::move(connected), control) : nullptr;
            }
            void cancel() override { client->cancel(); }
            void close() override { client->close(); }
        };
    }

    void installHostEndBoundaryControl(std::shared_ptr<HostEndBoundaryControl> control) {
        std::lock_guard<std::mutex> lock(controlMutex);
        installedControl = std::move(control);
    }
    std::shared_ptr<HostEndBoundaryControl> hostEndBoundaryControl() {
        std::lock_guard<std::mutex> lock(controlMutex);
        return installedControl;
    }
    std::unique_ptr<Server::AdmissionRuntimeClient> decorateBoundaryClient(
            std::unique_ptr<Server::AdmissionRuntimeClient> client,
            std::shared_ptr<HostEndBoundaryControl> control) {
        return std::make_unique<BoundaryClient>(std::move(client), std::move(control));
    }
}

namespace Duel6::Server {
    class BoundaryHeadlessServer final : public HeadlessServer {
    public:
        BoundaryHeadlessServer(ServerConfig config, AdmissionRuntimeDependencies dependencies)
                : HeadlessServer(std::move(config), std::move(dependencies)) {
            const auto control = Test::hostEndBoundaryControl();
            if (!control || !this->config.admissionClient) return;
            if (!runtimeDependencies.productionReplicationProtocol)
                throw std::runtime_error("Close-boundary harness requires the production transport");
            auto factory = std::move(runtimeDependencies.clientFactory);
            runtimeDependencies.clientFactory = [factory = std::move(factory), control] {
                return Test::decorateBoundaryClient(factory(), control);
            };
        }
    };
}

// Compile the unchanged runtime in this executable alone. Only the local guest
// server construction is substituted; all join/recovery/UI callbacks are real.
// The shipped game and existing test binaries keep their normal runtime object.
#define HeadlessServer BoundaryHeadlessServer
#include "source/client/NetworkSessionRuntime.cpp"
#undef HeadlessServer
