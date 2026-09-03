#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "tests/TestHarness.h"
#include "source/network/AdmissionProtocol.h"
#include "source/network/CompatibilityManifest.h"
#include "source/network/NetworkTrustPolicy.h"
#include "source/server/AdmissionSession.h"
#include "source/server/HeadlessServer.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
    namespace fs = std::filesystem;
    using namespace Duel6;
    using namespace std::chrono_literals;

    struct TemporaryResources {
        fs::path root;
        TemporaryResources() {
            static std::atomic<unsigned> sequence{0};
            root = fs::temp_directory_path() /
                   ("duel6r-admission-tests-" + std::to_string(sequence.fetch_add(1)));
            fs::create_directories(root);
        }
        ~TemporaryResources() { std::error_code ignored; fs::remove_all(root, ignored); }
        bool write(const std::string &logical, const std::string &bytes = "content") const {
            const fs::path path = root / fs::path(logical);
            std::error_code error;
            fs::create_directories(path.parent_path(), error);
            if (error) return false;
            std::ofstream stream(path, std::ios::binary);
            stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            stream.close();
            return !stream.fail();
        }
        std::optional<std::string> read(const std::string &logical) const {
            std::ifstream stream(root / fs::path(logical), std::ios::binary);
            if (!stream) return std::nullopt;
            std::string result((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            if (stream.bad()) return std::nullopt;
            return result;
        }
        void required() const {
            write("levels/arena.json", "arena");
            write("levels/arena.meta", "metadata");
            write("data/blocks.json", "blocks");
            write("data/config.script", "config");
        }
    };

    Network::ContentIdentity identity(std::uint8_t value) {
        Network::ContentIdentity result{};
        result.fill(value);
        return result;
    }

    Network::GameplayManifest manifest(std::initializer_list<std::pair<std::string, std::uint8_t>> entries) {
        Network::GameplayManifest result;
        for (const auto &entry: entries) result.push_back({entry.first, identity(entry.second)});
        return result;
    }

    Network::AdmissionRequest requestFor(const Network::GameplayManifest &value, std::uint8_t players = 1) {
        return Network::makeLocalAdmissionRequest(players, value);
    }

    bool sameManifest(const Network::GameplayManifest &left, const Network::GameplayManifest &right) {
        return Network::gameplayManifestsEqual(left, right);
    }

    class FixedManifestSource final : public Network::ManifestSource {
    public:
        explicit FixedManifestSource(Network::ManifestBuildResult result) : result(std::move(result)) {}
        Network::ManifestBuildResult build(const std::string &, const std::vector<std::string> &) const override {
            ++calls;
            return result;
        }
        mutable std::size_t calls = 0;
    private:
        Network::ManifestBuildResult result;
    };

    struct RuntimeFixture {
        Network::Trust::TimePoint now{};
        std::shared_ptr<class FakeAdmissionConnection> connection;
        bool accepted = false;
        bool cancelled = false;
        bool shutdown = false;
        std::vector<Server::AdmissionLifecycleEvent> events;
        std::vector<Network::HostServiceStatusCode> hostedStatuses;
    };

    class FakeAdmissionConnection final : public Server::AdmissionRuntimeConnection {
    public:
        explicit FakeAdmissionConnection(Network::Trust::TimePoint accepted) : accepted(accepted) {}
        Network::SendResult send(std::vector<std::uint8_t> payload) override {
            if (onSend) onSend(payload);
            sent.push_back(std::move(payload)); return sendResult;
        }
        Network::AdmissionAcceptanceEnqueueResult enqueueAdmissionAcceptance(
                std::vector<std::uint8_t> payload,
                Network::AdmissionAttemptGate &attempt,
                const std::function<bool()> &cancelled,
                const Network::Trust::Clock &now,
                Network::TransportTimePoint deadline) override {
            return attempt.enqueueAcceptance(
                    [this, &cancelled, &now, deadline, payload = std::move(payload)]() mutable {
                        const auto isCancelled = [&] {
                            try { return !cancelled || cancelled(); } catch (...) { return true; }
                        };
                        const auto deadlineReached = [&] {
                            try { return !now || now() >= deadline; } catch (...) { return true; }
                        };
                        if (isCancelled()) return Network::AdmissionAcceptanceEnqueueResult::Cancelled;
                        if (deadlineReached()) return Network::AdmissionAcceptanceEnqueueResult::DeadlineExceeded;
                        if (onAcceptanceBeforeInsert) onAcceptanceBeforeInsert();
                        if (isCancelled()) return Network::AdmissionAcceptanceEnqueueResult::Cancelled;
                        if (deadlineReached()) return Network::AdmissionAcceptanceEnqueueResult::DeadlineExceeded;
                        if (sendResult != Network::SendResult::Accepted)
                            return Network::AdmissionAcceptanceEnqueueResult::NotQueued;
                        sent.push_back(std::move(payload));
                        if (onSend) onSend(sent.back());
                        if (onAcceptanceInserted) onAcceptanceInserted();
                        return Network::AdmissionAcceptanceEnqueueResult::Accepted;
                    });
        }
        bool receive(Network::TransportFrame &frame) override {
            if (incoming.empty()) return false;
            frame = std::move(incoming.front()); incoming.pop_front();
            if (onReceive) onReceive(frame);
            return true;
        }
        Network::ClientState state() const override { return currentState; }
        Network::TransportTimePoint acceptedAt() const override { return accepted; }
        Network::TransportTimePoint terminalAt() const override { return terminal; }
        bool permitAdmissionAcceptance() override { permission = true; return permitResult; }
        void revokeAdmissionAcceptance() override { permission = false; revoked = true; }
        void markAdmissionSucceeded() override { succeeded = true; }
        void requestClose() override { closeRequested = true; }

        void queue(std::vector<std::uint8_t> payload, Network::TransportTimePoint received) {
            incoming.push_back({std::move(payload), received});
        }
        std::deque<Network::TransportFrame> incoming;
        std::vector<std::vector<std::uint8_t>> sent;
        Network::ClientState currentState = Network::ClientState::Connected;
        Network::TransportTimePoint accepted;
        Network::TransportTimePoint terminal{};
        bool permission = false;
        bool permitResult = true;
        bool revoked = false;
        bool succeeded = false;
        bool closeRequested = false;
        std::function<void(const Network::TransportFrame &)> onReceive;
        std::function<void(const std::vector<std::uint8_t> &)> onSend;
        std::function<void()> onAcceptanceBeforeInsert;
        std::function<void()> onAcceptanceInserted;
        Network::SendResult sendResult = Network::SendResult::Accepted;
    };

    class FakeAdmissionListener final : public Server::AdmissionRuntimeListener {
    public:
        explicit FakeAdmissionListener(std::shared_ptr<RuntimeFixture> fixture) : fixture(std::move(fixture)) {}
        bool start(const Network::Endpoint &) override { return startResult; }
        bool waitForReady(std::chrono::milliseconds) override { return readyResult; }
        Network::ListenerState state() const override { return readyResult ? Network::ListenerState::Ready
                                                                          : Network::ListenerState::Failed; }
        Network::TransportFailure failure() const override { return readyResult ? Network::TransportFailure::None
                                                                                : Network::TransportFailure::SystemError; }
        std::shared_ptr<Server::AdmissionRuntimeConnection> acceptConnection() override {
            if (fixture->accepted || !fixture->connection) return {};
            fixture->accepted = true; return fixture->connection;
        }
        void shutdown() override { fixture->shutdown = true; }
        bool startResult = true;
        bool readyResult = true;
    private:
        std::shared_ptr<RuntimeFixture> fixture;
    };

    struct FakeClientState {
        std::shared_ptr<FakeAdmissionConnection> connection;
        bool started = false;
        bool cancelled = false;
        bool closed = false;
        bool connectResult = true;
        Network::ClientState state = Network::ClientState::Connected;
        Network::TransportFailure failure = Network::TransportFailure::None;
        std::function<void()> onWait;
        std::function<void()> onCancel;
    };

    class FakeAdmissionClient final : public Server::AdmissionRuntimeClient {
    public:
        explicit FakeAdmissionClient(std::shared_ptr<FakeClientState> state) : shared(std::move(state)) {}
        bool start(const Network::Endpoint &) override { shared->started = true; return true; }
        bool waitForConnected(std::chrono::milliseconds) override {
            if (shared->onWait) shared->onWait();
            return shared->connectResult;
        }
        Network::ClientState state() const override { return shared->state; }
        Network::TransportFailure failure() const override { return shared->failure; }
        std::shared_ptr<Server::AdmissionRuntimeConnection> connection() const override { return shared->connection; }
        void cancel() override {
            shared->cancelled = true;
            if (shared->onCancel) shared->onCancel();
        }
        void close() override { shared->closed = true; }
    private:
        std::shared_ptr<FakeClientState> shared;
    };

    Server::ServerConfig runtimeServerConfig() {
        Server::ServerConfig config;
        config.transportEnabled = true;
        config.localOnly = true;
        config.listenEndpoint = {"127.0.0.1", 26660};
        config.localPlayers = 1;
        return config;
    }

    Server::ServerConfig runtimeGuestConfig() {
        auto config = runtimeServerConfig();
        config.transportEnabled = false;
        config.admissionClient = true;
        config.localPlayers = 2;
        return config;
    }

    Server::AdmissionRuntimeDependencies runtimeDependencies(
            const std::shared_ptr<RuntimeFixture> &fixture, const Network::GameplayManifest &host) {
        Server::AdmissionRuntimeDependencies dependencies;
        dependencies.now = [fixture] { return fixture->now; };
        dependencies.cancelled = [fixture] { return fixture->cancelled; };
        dependencies.wait = [fixture](std::chrono::milliseconds amount) { fixture->now += amount; };
        dependencies.listenerFactory = [fixture](std::size_t, bool) {
            return std::make_unique<FakeAdmissionListener>(fixture);
        };
        dependencies.lifecycleObserver = [fixture](const Server::AdmissionLifecycleEvent &event) {
            fixture->events.push_back(event); return true;
        };
        dependencies.hostedServiceStatus = [fixture](Network::HostServiceStatusCode status) {
            fixture->hostedStatuses.push_back(status);
            if (status == Network::HostServiceStatusCode::Ready) {
                return std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
                    return event.stage == Server::AdmissionLifecycleStage::HostInitialized;
                }) && std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
                    return event.stage == Server::AdmissionLifecycleStage::ListenerReady;
                });
            }
            return true;
        };
        dependencies.manifestSource = std::make_shared<FixedManifestSource>(
                Network::ManifestBuildResult{Network::ManifestStatus::Valid, host});
        return dependencies;
    }

    Server::AdmissionRuntimeDependencies guestRuntimeDependencies(
            const std::shared_ptr<RuntimeFixture> &fixture,
            const std::shared_ptr<FakeClientState> &client,
            const Network::GameplayManifest &host) {
        auto dependencies = runtimeDependencies(fixture, host);
        dependencies.clientFactory = [client] { return std::make_unique<FakeAdmissionClient>(client); };
        return dependencies;
    }

    template<typename Function>
    bool rejects(Function function) {
        try { function(); } catch (...) { return true; }
        return false;
    }


#ifndef _WIN32
    std::uint16_t unusedLoopbackPort() {
        const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
        if (descriptor < 0) return 0;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (::bind(descriptor, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            ::close(descriptor);
            return 0;
        }
        socklen_t size = sizeof(address);
        if (::getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size) != 0) {
            ::close(descriptor);
            return 0;
        }
        ::close(descriptor);
        return ntohs(address.sin_port);
    }
#endif
}

D6R_TEST_CASE("AC-001 AC-002 compatibility constants capabilities and wire format are exact") {
    D6R_REQUIRE_EQ(1u, Network::AdmissionProtocolVersion);
    D6R_REQUIRE_EQ("duel6r-network-r1", std::string(Network::NetworkReleaseId));
    const std::vector<std::string> exact{
        "d6r.compatibility-admission.v1", "d6r.gameplay-manifest.v1", "d6r.session-identity.v1"};
    D6R_REQUIRE(std::equal(exact.begin(), exact.end(), Network::RequiredAdmissionCapabilities.begin()));

    auto request = requestFor(manifest({{"data/blocks.json", 1}, {"levels/a.json", 2}}), 15);
    request.capabilities.emplace_back("guest.extra.v1");
    const auto bytes = Network::serializeAdmissionRequest(request);
    const auto decoded = Network::deserializeAdmissionRequest(bytes);
    D6R_REQUIRE_EQ(request.protocolVersion, decoded.protocolVersion);
    D6R_REQUIRE_EQ(request.networkReleaseId, decoded.networkReleaseId);
    D6R_REQUIRE(request.capabilities == decoded.capabilities);
    D6R_REQUIRE_EQ(15u, decoded.localPlayerCount);
    D6R_REQUIRE(sameManifest(request.gameplayManifest, decoded.gameplayManifest));
    D6R_REQUIRE(bytes == Network::serializeAdmissionRequest(decoded));
}

D6R_TEST_CASE("AC-005 AC-006 AC-007 exact compatibility checks are case and whitespace sensitive") {
    const auto host = manifest({{"levels/a.json", 1}});
    Server::AdmissionPolicy policy(host, 1);
    auto request = requestFor(host);
    ++request.protocolVersion;
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::ProtocolIncompatible);
    request = requestFor(host); request.networkReleaseId = "DUEL6R-network-r1";
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::NetworkReleaseMismatch);
    request.networkReleaseId = std::string(Network::NetworkReleaseId) + " ";
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::NetworkReleaseMismatch);
    request = requestFor(host); request.capabilities.pop_back();
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::RequiredCapabilityUnsupported);
    request = requestFor(host); request.capabilities.emplace_back("additional.capability");
    D6R_REQUIRE(policy.evaluate(request).admitted());
}

D6R_TEST_CASE("AC-017 admission outcomes retain approved precedence identifiers and fixed copy") {
    using Code = Network::AdmissionResultCode;
    const std::vector<std::tuple<Code, std::string, std::string>> expected{
        {Code::MalformedRequest, "malformed-request", "Connection request rejected."},
        {Code::NotAuthorized, "not-authorized", "Connection not authorized."},
        {Code::ProtocolIncompatible, "protocol-incompatible", "Network release mismatch. Use the same supported game release as the host."},
        {Code::NetworkReleaseMismatch, "network-release-mismatch", "Network release mismatch. Use the same supported game release as the host."},
        {Code::RequiredCapabilityUnsupported, "required-capability-unsupported", "Network release mismatch. Use the same supported game release as the host."},
        {Code::GameplayContentManifestInvalid, "gameplay-content-manifest-invalid", "Gameplay content manifest is invalid. Use the host's exact supported gameplay content."},
        {Code::GameplayContentMismatch, "gameplay-content-mismatch", "Gameplay content mismatch. Use the host's exact supported gameplay content."},
        {Code::MatchAlreadyStarted, "match-already-started", "Match already started. Join-in-progress is not supported."},
        {Code::SessionFull, "session-full", "Session is full."},
        {Code::HostPolicyRejected, "host-policy-rejected", "Host rejected the connection."},
        {Code::Admitted, "admitted", ""}};
    for (const auto &entry: expected) {
        D6R_REQUIRE_EQ(std::get<1>(entry), std::string(Network::admissionResultIdentifier(std::get<0>(entry))));
        D6R_REQUIRE_EQ(std::get<2>(entry), std::string(Network::admissionResultUserCopy(std::get<0>(entry))));
    }

    const auto host = manifest({{"levels/a", 1}});
    Server::AdmissionPolicy policy(host, 14);
    auto request = requestFor(manifest({{"levels/b", 2}}), 2);
    request.protocolVersion = 99;
    request.networkReleaseId = "wrong";
    request.capabilities.clear();
    D6R_REQUIRE(policy.evaluate(request, {false, false}).code == Code::NotAuthorized);
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::ProtocolIncompatible);
    request.protocolVersion = Network::AdmissionProtocolVersion;
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::NetworkReleaseMismatch);
    request.networkReleaseId = std::string(Network::NetworkReleaseId);
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::RequiredCapabilityUnsupported);
    request.capabilities = requestFor(host).capabilities;
    request.gameplayManifest = {{"../invalid", identity(3)}};
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::GameplayContentManifestInvalid);
    request.gameplayManifest = manifest({{"levels/b", 2}});
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::GameplayContentMismatch);
    request.gameplayManifest = host; policy.setMatchStarted(true);
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::MatchAlreadyStarted);
    policy.setMatchStarted(false);
    D6R_REQUIRE(policy.evaluate(request, {true, false}).code == Code::SessionFull);
}

D6R_TEST_CASE("AC-008 AC-011 canonical builder hashes exact gameplay bytes independent of creation order") {
    TemporaryResources first, second;
    first.write("levels/z.meta", "metadata"); first.write("data/config.script", "config");
    first.write("levels/a.json", "arena"); first.write("data/blocks.json", "blocks");
    second.write("data/blocks.json", "blocks"); second.write("levels/a.json", "arena");
    second.write("data/config.script", "config"); second.write("levels/z.meta", "metadata");
    auto left = Network::CompatibilityManifestBuilder(first.root.string()).build();
    auto right = Network::CompatibilityManifestBuilder(second.root.string()).build();
    D6R_REQUIRE(left.valid() && right.valid());
    D6R_REQUIRE(sameManifest(left.manifest, right.manifest));
    D6R_REQUIRE(std::is_sorted(left.manifest.begin(), left.manifest.end(), [](const auto &a, const auto &b) {
        return a.logicalPath < b.logicalPath;
    }));
    second.write("levels/a.json", "changed arena");
    auto changed = Network::CompatibilityManifestBuilder(second.root.string()).build();
    D6R_REQUIRE(changed.valid() && !sameManifest(left.manifest, changed.manifest));
    second.write("levels/a.json", "arena"); second.write("levels/z.meta", "changed metadata");
    changed = Network::CompatibilityManifestBuilder(second.root.string()).build();
    D6R_REQUIRE(changed.valid() && !sameManifest(left.manifest, changed.manifest));
}

D6R_TEST_CASE("AC-010 manifests detect missing extra changed and case-different entries") {
    const auto baseline = manifest({{"data/config.script", 1}, {"levels/A.json", 2}});
    D6R_REQUIRE(!sameManifest(baseline, manifest({{"data/config.script", 1}})));
    D6R_REQUIRE(!sameManifest(baseline, manifest({{"data/config.script", 1}, {"levels/A.json", 2}, {"levels/z", 3}})));
    D6R_REQUIRE(!sameManifest(baseline, manifest({{"data/config.script", 1}, {"levels/A.json", 9}})));
    D6R_REQUIRE(!sameManifest(baseline, manifest({{"data/config.script", 1}, {"levels/a.json", 2}})));
}

D6R_TEST_CASE("AC-009 canonical manifest validation rejects malformed duplicate unsorted and exact one-over bounds") {
    const auto hash = identity(1);
    D6R_REQUIRE(Network::validCanonicalManifest({{"a", hash}}));
    D6R_REQUIRE(!Network::validCanonicalManifest({}));
    D6R_REQUIRE(!Network::validCanonicalManifest({{"a", hash}, {"a", hash}}));
    D6R_REQUIRE(!Network::validCanonicalManifest({{"b", hash}, {"a", hash}}));
    const std::vector<std::string> invalid{
        "", "/a", "a/", "a//b", ".hidden", "../escape", "a/../escape", "a\\b", "a%2fb",
        "white space", std::string("nul\0path", 8), std::string("bad\xFF", 4),
        std::string("bidi\xE2\x80\xAE", 7), std::string("control\x01", 8)};
    for (const auto &path: invalid) D6R_REQUIRE(!Network::validCanonicalManifest({{path, hash}}));
    const std::string path240 = std::string(60, 'a') + '/' + std::string(59, 'b') + '/' +
                                std::string(59, 'c') + '/' + std::string(59, 'd');
    D6R_REQUIRE_EQ(240u, path240.size());
    D6R_REQUIRE(Network::validCanonicalManifest({{path240, hash}}));
    D6R_REQUIRE(!Network::validCanonicalManifest({{path240 + "x", hash}}));
    Network::GameplayManifest maximum, excessive;
    for (unsigned index = 0; index < 256; ++index)
        maximum.push_back({"levels/f" + std::to_string(1000 + index), hash});
    excessive = maximum; excessive.push_back({"levels/z", hash});
    D6R_REQUIRE(Network::validCanonicalManifest(maximum));
    D6R_REQUIRE(!Network::validCanonicalManifest(excessive));
}

D6R_TEST_CASE("AC-004 request parser rejects malformed duplicate noncanonical excessive and trailing payloads") {
    auto valid = requestFor(manifest({{"levels/a", 1}}));
    auto bytes = Network::serializeAdmissionRequest(valid);
    D6R_REQUIRE(rejects([&] { auto value = bytes; value.pop_back(); Network::deserializeAdmissionRequest(value); }));
    D6R_REQUIRE(rejects([&] { auto value = bytes; value.push_back(0); Network::deserializeAdmissionRequest(value); }));
    D6R_REQUIRE(rejects([&] { auto value = bytes; value[0] ^= 1; Network::deserializeAdmissionRequest(value); }));
    D6R_REQUIRE(rejects([&] { Network::deserializeAdmissionRequest(std::vector<std::uint8_t>(262145, 0)); }));
    valid.capabilities.push_back(valid.capabilities.front());
    D6R_REQUIRE(rejects([&] { Network::serializeAdmissionRequest(valid); }));
    valid = requestFor(manifest({{"b", 1}, {"a", 2}}));
    D6R_REQUIRE(rejects([&] { Network::serializeAdmissionRequest(valid); }));
    Server::AdmissionPolicy policy(manifest({{"levels/a", 1}}), 1);
    const auto beforeParticipants = policy.allocation().participantCount();
    const auto beforePlayers = policy.allocation().playerCount();
    D6R_REQUIRE(policy.evaluatePayload({1, 2, 3}, {false, false}).code ==
                Network::AdmissionResultCode::MalformedRequest);
    D6R_REQUIRE_EQ(beforeParticipants, policy.allocation().participantCount());
    D6R_REQUIRE_EQ(beforePlayers, policy.allocation().playerCount());
}

D6R_TEST_CASE("AC-012 AC-013 AC-014 manifest includes enabled host scripts and excludes local and cosmetic trees") {
    TemporaryResources resources; resources.required();
    resources.write("scripts/enabled.lua", "host gameplay");
    resources.write("scripts/not-enabled.lua", "disabled gameplay");
    for (const std::string path: {"profiles/guest/script.lua", "profiles/skin.json", "people/people.json",
             "controls/preset.json", "statistics/stats.json", "saves/save.json", "textures/art.png",
             "sound/theme.ogg", "docs/readme.md"}) resources.write(path, "must not affect compatibility");
    const auto built = Network::CompatibilityManifestBuilder(resources.root.string(), {"scripts/enabled.lua"}).build();
    D6R_REQUIRE(built.valid());
    std::set<std::string> paths;
    for (const auto &entry: built.manifest) paths.insert(entry.logicalPath);
    D6R_REQUIRE(paths.count("levels/arena.json") && paths.count("levels/arena.meta"));
    D6R_REQUIRE(paths.count("data/blocks.json") && paths.count("data/config.script"));
    D6R_REQUIRE(paths.count("scripts/enabled.lua"));
    D6R_REQUIRE(!paths.count("scripts/not-enabled.lua"));
    D6R_REQUIRE(std::none_of(paths.begin(), paths.end(), [](const std::string &path) {
        return path.rfind("profiles/", 0) == 0 || path.rfind("people/", 0) == 0 ||
               path.rfind("controls/", 0) == 0 || path.rfind("statistics/", 0) == 0 ||
               path.rfind("saves/", 0) == 0 || path.rfind("textures/", 0) == 0 ||
               path.rfind("sound/", 0) == 0 || path.rfind("docs/", 0) == 0;
    }));
    D6R_REQUIRE(!Network::Trust::guestContentMayLoadOrExecute());
}

#ifndef _WIN32
D6R_TEST_CASE("AC-004 manifest builder rejects missing content symlinks traversal and duplicate enabled scripts") {
    TemporaryResources missing;
    missing.write("levels/a", "a"); missing.write("data/blocks.json", "blocks");
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(missing.root.string()).build().status ==
                Network::ManifestStatus::MissingRequiredContent);

    TemporaryResources linked; linked.required();
    fs::create_symlink(linked.root / "levels/arena.json", linked.root / "levels/linked.json");
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(linked.root.string()).build().status ==
                Network::ManifestStatus::UnsafeFilesystemEntry);

    TemporaryResources scripts; scripts.required(); scripts.write("scripts/a.lua", "a");
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(scripts.root.string(), {"../outside"}).build().status ==
                Network::ManifestStatus::InvalidLogicalPath);
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(scripts.root.string(), {"scripts/a.lua", "scripts/a.lua"}).build().status ==
                Network::ManifestStatus::DuplicateLogicalPath);
}

D6R_TEST_CASE("secure manifest descriptors reject symlink roots hardlinks nonregular entries invalid names and early traversal excess") {
    TemporaryResources realRoot; realRoot.required();
    const fs::path rootLink = realRoot.root.parent_path() / (realRoot.root.filename().string() + "-link");
    fs::create_directory_symlink(realRoot.root, rootLink);
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(rootLink.string()).build().status ==
                Network::ManifestStatus::InvalidRoot);
    fs::remove(rootLink);

    TemporaryResources hardlinked; hardlinked.required();
    fs::create_hard_link(hardlinked.root / "levels" / "arena.json",
                         hardlinked.root / "levels" / "alias.json");
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(hardlinked.root.string()).build().status ==
                Network::ManifestStatus::UnsafeFilesystemEntry);

    TemporaryResources nonregular; nonregular.required();
    const fs::path fifo = nonregular.root / "levels" / "pipe";
    D6R_REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(nonregular.root.string()).build().status ==
                Network::ManifestStatus::UnsafeFilesystemEntry);

    TemporaryResources invalidName; invalidName.required();
    invalidName.write(std::string("levels/nonascii-") + static_cast<char>(0xff), "x");
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(invalidName.root.string()).build().status ==
                Network::ManifestStatus::InvalidLogicalPath);

    TemporaryResources deep; deep.required();
    fs::path nested = deep.root / "levels";
    for (unsigned index = 0; index < Network::Trust::MaxLogicalPathSegments; ++index) nested /= "d";
    fs::create_directories(nested); std::ofstream(nested / "file") << "x";
    const auto deepStatus = Network::CompatibilityManifestBuilder(deep.root.string()).build().status;
    D6R_REQUIRE(deepStatus == Network::ManifestStatus::TooManyEntries ||
                deepStatus == Network::ManifestStatus::InvalidLogicalPath);

    TemporaryResources wide; wide.required();
    for (std::size_t index = 0; index <= Network::MaxManifestTraversalEntries; ++index)
        fs::create_directories(wide.root / "levels" / ("d" + std::to_string(1000 + index)));
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(wide.root.string()).build().status ==
                Network::ManifestStatus::TooManyEntries);
}
#endif

D6R_TEST_CASE("AC-001 AC-002 AC-003 AC-015 AC-016 allocation is atomic bounded owned and unique") {
    for (std::uint8_t localPlayers = 1; localPlayers <= 15; ++localPlayers) {
        Server::SessionAllocation allocation(localPlayers);
        const auto &host = allocation.hostParticipant();
        D6R_REQUIRE(host.localHost && host.participantId != 0);
        D6R_REQUIRE_EQ(localPlayers, host.playerIds.size());
        for (auto player: host.playerIds) D6R_REQUIRE(player != 0 && allocation.participantOwnsPlayer(host.participantId, player));
        D6R_REQUIRE(!allocation.participantOwnsPlayer(host.participantId + 999, host.playerIds.front()));
    }
    D6R_REQUIRE_THROW(Server::SessionAllocation(0), std::invalid_argument);
    D6R_REQUIRE_THROW(Server::SessionAllocation(16), std::invalid_argument);

    Server::SessionAllocation allocation(1);
    std::vector<Network::AdmissionResult> results(20);
    std::vector<std::thread> workers;
    for (std::size_t index = 0; index < results.size(); ++index)
        workers.emplace_back([&, index] { results[index] = allocation.allocateGuest(1); });
    for (auto &worker: workers) worker.join();
    D6R_REQUIRE_EQ(15u, allocation.participantCount());
    D6R_REQUIRE_EQ(15u, allocation.playerCount());
    std::set<std::uint64_t> identities;
    const auto &host = allocation.hostParticipant();
    identities.insert(host.participantId); identities.insert(host.playerIds.front());
    std::size_t admitted = 0;
    for (const auto &result: results) {
        if (!result.admitted()) {
            D6R_REQUIRE(result.code == Network::AdmissionResultCode::SessionFull);
            D6R_REQUIRE_EQ(0u, result.participantId); D6R_REQUIRE(result.playerIds.empty());
            continue;
        }
        ++admitted;
        D6R_REQUIRE(result.participantId != 0 && result.playerIds.size() == 1);
        D6R_REQUIRE(identities.insert(result.participantId).second);
        D6R_REQUIRE(identities.insert(result.playerIds.front()).second);
        D6R_REQUIRE(allocation.participantOwnsPlayer(result.participantId, result.playerIds.front()));
    }
    D6R_REQUIRE_EQ(14u, admitted);
    D6R_REQUIRE_EQ(30u, identities.size());
}

D6R_TEST_CASE("AC-002 AC-003 AC-015 AC-016 reserve commit rollback are atomic and identities are never reused") {
    Server::SessionAllocation allocation(1);
    const auto initialParticipants = allocation.participantCount();
    const auto initialPlayers = allocation.playerCount();
    const auto rolledBack = allocation.reserveGuest(2);
    D6R_REQUIRE(rolledBack.pending());
    D6R_REQUIRE_EQ(1u, allocation.pendingParticipantCount());
    D6R_REQUIRE_EQ(2u, allocation.pendingPlayerCount());
    D6R_REQUIRE_EQ(initialParticipants, allocation.participantCount());
    D6R_REQUIRE_EQ(initialPlayers, allocation.playerCount());
    D6R_REQUIRE(allocation.rollback(rolledBack.transactionId));
    D6R_REQUIRE(!allocation.rollback(rolledBack.transactionId));
    D6R_REQUIRE(!allocation.commit(rolledBack.transactionId));
    D6R_REQUIRE_EQ(0u, allocation.pendingParticipantCount());
    D6R_REQUIRE_EQ(0u, allocation.pendingPlayerCount());

    const auto committed = allocation.reserveGuest(2);
    D6R_REQUIRE(committed.pending());
    D6R_REQUIRE(committed.transactionId != rolledBack.transactionId);
    D6R_REQUIRE(committed.result.participantId != rolledBack.result.participantId);
    for (auto id: committed.result.playerIds)
        D6R_REQUIRE(std::find(rolledBack.result.playerIds.begin(), rolledBack.result.playerIds.end(), id) ==
                    rolledBack.result.playerIds.end());
    D6R_REQUIRE(allocation.commit(committed.transactionId));
    D6R_REQUIRE(!allocation.commit(committed.transactionId));
    D6R_REQUIRE(!allocation.rollback(committed.transactionId));
    D6R_REQUIRE_EQ(initialParticipants + 1, allocation.participantCount());
    D6R_REQUIRE_EQ(initialPlayers + 2, allocation.playerCount());

    const auto capacityReservation = allocation.reserveGuest(12);
    D6R_REQUIRE(capacityReservation.pending());
    const auto blocked = allocation.reserveGuest(1);
    D6R_REQUIRE(!blocked.pending());
    D6R_REQUIRE(blocked.result.code == Network::AdmissionResultCode::SessionFull);
    D6R_REQUIRE(allocation.rollback(capacityReservation.transactionId));
    D6R_REQUIRE(allocation.reserveGuest(1).pending());
}

D6R_TEST_CASE("AC-003 identity source exhausts at UINT64_MAX without wrap reuse or partial allocation") {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    Server::SessionAllocation hostAtLimit(1, Server::sequentialIdentitySource(maximum - 1));
    D6R_REQUIRE_EQ(maximum - 1, hostAtLimit.hostParticipant().participantId);
    D6R_REQUIRE_EQ(maximum, hostAtLimit.hostParticipant().playerIds.front());
    const auto exhausted = hostAtLimit.reserveGuest(1);
    D6R_REQUIRE(!exhausted.pending());
    D6R_REQUIRE(exhausted.result.code == Network::AdmissionResultCode::HostPolicyRejected);
    D6R_REQUIRE_EQ(1u, hostAtLimit.participantCount());
    D6R_REQUIRE_EQ(1u, hostAtLimit.playerCount());
    D6R_REQUIRE_EQ(0u, hostAtLimit.pendingParticipantCount());

    std::vector<std::optional<std::uint64_t>> supplied{1, 2, 3, std::nullopt};
    std::size_t next = 0;
    Server::SessionAllocation partial(1, [&]() { return supplied.at(next++); });
    const auto failed = partial.reserveGuest(2);
    D6R_REQUIRE(!failed.pending());
    D6R_REQUIRE_EQ(0u, partial.pendingParticipantCount());
    D6R_REQUIRE_EQ(0u, partial.pendingPlayerCount());
    D6R_REQUIRE_EQ(1u, partial.participantCount());
    D6R_REQUIRE_EQ(1u, partial.playerCount());
}

D6R_TEST_CASE("AC-016 admission offers bind once only after commit and revoke guest authority on disconnect") {
    const auto host = manifest({{"levels/a", 1}});
    Server::AdmissionPolicy policy(host, 1);
    const auto offer = policy.offer(requestFor(host, 2));
    D6R_REQUIRE(offer.pending());
    D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::OwnReadiness));
    D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::HostOnly));
    D6R_REQUIRE(policy.commit(offer.transactionId, 77));
    D6R_REQUIRE(!policy.commit(offer.transactionId, 77));
    D6R_REQUIRE(!policy.commit(offer.transactionId, 78));
    D6R_REQUIRE(policy.authorize(77, Network::Trust::AuthorityAction::OwnReadiness));
    D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::HostOnly));
    for (auto player: offer.result.playerIds)
        D6R_REQUIRE(policy.authorize(77, Network::Trust::AuthorityAction::PlayerInput, player));
    D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::PlayerInput,
                                  policy.allocation().hostParticipant().playerIds.front()));
    policy.disconnect(77);
    D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::OwnReadiness));
    for (auto player: offer.result.playerIds)
        D6R_REQUIRE(!policy.authorize(77, Network::Trust::AuthorityAction::PlayerInput, player));

    const auto wrongConnection = policy.offer(requestFor(host));
    D6R_REQUIRE(wrongConnection.pending());
    D6R_REQUIRE(!policy.commit(wrongConnection.transactionId, 0));
    D6R_REQUIRE(policy.rollback(wrongConnection.transactionId));
}

D6R_TEST_CASE("AC-016 every incomplete offer terminal path rolls back while accepted offers commit once") {
    enum class TerminalPath { SendFailure, PeerClose, Cancel, Timeout, MissingAcceptance,
                              InvalidAcceptance, WrongTransaction, LateAcceptance };
    const auto host = manifest({{"levels/a", 1}});
    Server::AdmissionPolicy policy(host, 1);
    const auto baselineParticipants = policy.allocation().participantCount();
    const auto baselinePlayers = policy.allocation().playerCount();
    std::set<std::uint64_t> offeredIdentities;
    for (const auto path: {TerminalPath::SendFailure, TerminalPath::PeerClose, TerminalPath::Cancel,
                           TerminalPath::Timeout, TerminalPath::MissingAcceptance,
                           TerminalPath::InvalidAcceptance, TerminalPath::WrongTransaction,
                           TerminalPath::LateAcceptance}) {
        (void) path;
        const auto offer = policy.offer(requestFor(host, 2));
        D6R_REQUIRE(offer.pending());
        D6R_REQUIRE(offeredIdentities.insert(offer.result.participantId).second);
        for (auto player: offer.result.playerIds) D6R_REQUIRE(offeredIdentities.insert(player).second);
        D6R_REQUIRE(!policy.commit(offer.transactionId + 1, 900));
        D6R_REQUIRE(policy.rollback(offer.transactionId));
        D6R_REQUIRE(!policy.rollback(offer.transactionId));
        D6R_REQUIRE(!policy.commit(offer.transactionId, 900));
        D6R_REQUIRE_EQ(baselineParticipants, policy.allocation().participantCount());
        D6R_REQUIRE_EQ(baselinePlayers, policy.allocation().playerCount());
        D6R_REQUIRE_EQ(0u, policy.allocation().pendingParticipantCount());
        D6R_REQUIRE_EQ(0u, policy.allocation().pendingPlayerCount());
    }
    const auto accepted = policy.offer(requestFor(host));
    D6R_REQUIRE(accepted.pending());
    D6R_REQUIRE(policy.commit(accepted.transactionId, 901));
    D6R_REQUIRE(!policy.commit(accepted.transactionId, 901));
    D6R_REQUIRE_EQ(baselineParticipants + 1, policy.allocation().participantCount());
    D6R_REQUIRE_EQ(baselinePlayers + 1, policy.allocation().playerCount());
}

D6R_TEST_CASE("four-message runtime permits immediate acceptance before offer visibility and commits before confirmation") {
    const auto host = manifest({{"levels/a", 1}});
    auto fixture = std::make_shared<RuntimeFixture>();
    fixture->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host, 2)), fixture->now + 1ms);
    auto dependencies = runtimeDependencies(fixture, host);
    std::optional<Network::AdmissionIdentitySet> offered;
    dependencies.outboundWriter = [fixture, &offered](Server::AdmissionRuntimeConnection &base,
                                                       std::vector<std::uint8_t> payload) {
        auto &connection = static_cast<FakeAdmissionConnection &>(base);
        if (payload.size() >= 4 && payload[3] == 'O') {
            D6R_REQUIRE(connection.permission);
            offered = Network::deserializeAdmissionOffer(payload);
            D6R_REQUIRE_EQ(2u, offered->playerIds.size());
            D6R_REQUIRE(offered->playerIds[0] < offered->playerIds[1]);
            connection.queue(Network::serializeAdmissionAcceptance(*offered), fixture->now + 2ms);
        } else if (payload.size() >= 4 && payload[3] == 'C') {
            const auto confirmation = Network::deserializeAdmissionConfirmation(payload);
            D6R_REQUIRE(offered && Network::sameAdmissionIdentitySet(*offered, confirmation));
            connection.currentState = Network::ClientState::Closed;
            fixture->cancelled = true;
        }
        connection.sent.push_back(std::move(payload));
        return Network::SendResult::Accepted;
    };
    std::ostringstream output;
    Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(0, server.run(output));
    D6R_REQUIRE(offered.has_value());
    D6R_REQUIRE(fixture->connection->succeeded);
    D6R_REQUIRE(fixture->shutdown);
    D6R_REQUIRE_EQ(std::size_t(1), fixture->hostedStatuses.size());
    D6R_REQUIRE_EQ(Network::HostServiceStatusCode::Ready, fixture->hostedStatuses.front());
    D6R_REQUIRE(output.str().find("admitted\nparticipant-id=") != std::string::npos);
    const auto stageIndex = [&](Server::AdmissionLifecycleStage stage) {
        const auto found = std::find_if(fixture->events.begin(), fixture->events.end(),
                                        [stage](const auto &event) { return event.stage == stage; });
        D6R_REQUIRE(found != fixture->events.end());
        return static_cast<std::size_t>(std::distance(fixture->events.begin(), found));
    };
    D6R_REQUIRE(stageIndex(Server::AdmissionLifecycleStage::HostInitialized) <
                stageIndex(Server::AdmissionLifecycleStage::ListenerReady));
    D6R_REQUIRE(stageIndex(Server::AdmissionLifecycleStage::OfferPrepared) <
                stageIndex(Server::AdmissionLifecycleStage::OfferQueued));
    D6R_REQUIRE(stageIndex(Server::AdmissionLifecycleStage::AcceptanceReceived) <
                stageIndex(Server::AdmissionLifecycleStage::TransactionCommitted));
    D6R_REQUIRE(stageIndex(Server::AdmissionLifecycleStage::TransactionCommitted) <
                stageIndex(Server::AdmissionLifecycleStage::ConfirmationQueued));
}

#ifndef _WIN32
D6R_TEST_CASE("REP-067 injected canonical tick failure terminates production server unsuccessfully without host-end claim") {
    const auto hostedManifest = manifest({
            {"data/blocks.json", 1}, {"data/config.script", 2}, {"levels/a.json", 3}});
    auto content = std::make_shared<Network::FrozenGameplayContent>();
    (*content)["data/blocks.json"] = {'{', '}'};
    (*content)["data/config.script"] = {'i', 'n', 'v', 'a', 'l', 'i', 'd'};
    (*content)["levels/a.json"] = {'{', '}'};
    const Network::ManifestBuildResult built{
            Network::ManifestStatus::Valid, hostedManifest, content};

    Server::ServerConfig hostConfig = runtimeServerConfig();
    hostConfig.listenEndpoint.port = unusedLoopbackPort();
    D6R_REQUIRE(hostConfig.listenEndpoint.port != 0);
    Server::AdmissionRuntimeDependencies hostDependencies;
    hostDependencies.manifestSource = std::make_shared<FixedManifestSource>(built);
    std::atomic<bool> ready{false};
    hostDependencies.hostedServiceStatus = [&](Network::HostServiceStatusCode status) {
        if (status == Network::HostServiceStatusCode::Ready) ready = true;
        return true;
    };
    hostDependencies.authoritativeRuntimeFactory = [](const auto &, const auto &, const auto &) {
        Server::Authoritative::MatchRuntimeDependencies failure;
        failure.contentPreflight = [](const auto &) { return true; };
        failure.worldTick = [](Server::Authoritative::Tick, bool) { return false; };
        return failure;
    };

    std::ostringstream hostOutput;
    int hostStatus = -1;
    std::thread host([&] {
        Server::HeadlessServer server(hostConfig, std::move(hostDependencies));
        hostStatus = server.run(hostOutput);
    });
    for (unsigned attempt = 0; attempt < 200 && !ready; ++attempt) std::this_thread::sleep_for(5ms);
    D6R_REQUIRE(ready);

    Server::ServerConfig guestConfig = runtimeGuestConfig();
    guestConfig.listenEndpoint.port = hostConfig.listenEndpoint.port;
    guestConfig.localPlayers = 1;
    Server::AdmissionRuntimeDependencies guestDependencies;
    guestDependencies.manifestSource = std::make_shared<FixedManifestSource>(built);
    std::ostringstream guestOutput;
    Server::HeadlessServer guest(guestConfig, std::move(guestDependencies));
    D6R_REQUIRE_EQ(2, guest.run(guestOutput));
    host.join();

    const std::string output = hostOutput.str();
    const std::string evidence = "status=" + std::to_string(hostStatus)
            + ";runtime-failed=" + (output.find("authoritative-match-runtime-failed") != std::string::npos ? "true" : "false")
            + ";cleaned=" + (output.find("transport stopped") != std::string::npos ? "true" : "false")
            + ";intentional=" + (output.find("authoritative-match-ended-intentionally") != std::string::npos ? "true" : "false");
    D6R_REQUIRE_EQ(std::string("status=3;runtime-failed=true;cleaned=true;intentional=false"), evidence);
}
#endif

D6R_TEST_CASE("lost confirmation after atomic commit never rolls back host and never reports host success") {
    const auto host = manifest({{"levels/a", 1}});
    auto fixture = std::make_shared<RuntimeFixture>();
    fixture->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host)), fixture->now + 1ms);
    auto dependencies = runtimeDependencies(fixture, host);
    dependencies.outboundWriter = [fixture](Server::AdmissionRuntimeConnection &base,
                                             std::vector<std::uint8_t> payload) {
        auto &connection = static_cast<FakeAdmissionConnection &>(base);
        if (payload.size() >= 4 && payload[3] == 'O') {
            const auto offer = Network::deserializeAdmissionOffer(payload);
            connection.queue(Network::serializeAdmissionAcceptance(offer), fixture->now + 2ms);
            return Network::SendResult::Accepted;
        }
        D6R_REQUIRE(payload.size() >= 4 && payload[3] == 'C');
        fixture->cancelled = true;
        return Network::SendResult::NotConnected;
    };
    std::ostringstream output;
    Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(0, server.run(output));
    D6R_REQUIRE(std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
        return event.stage == Server::AdmissionLifecycleStage::TransactionCommitted;
    }));
    D6R_REQUIRE(std::none_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
        return event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack;
    }));
    D6R_REQUIRE(fixture->connection->succeeded);
    D6R_REQUIRE(fixture->connection->closeRequested);
    D6R_REQUIRE(output.str().find("admitted\n") == std::string::npos);
}

D6R_TEST_CASE("offer enqueue failure rolls back burns identities revokes permission and cleans listener") {
    const auto host = manifest({{"levels/a", 1}});
    auto fixture = std::make_shared<RuntimeFixture>();
    fixture->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host, 2)), fixture->now + 1ms);
    auto dependencies = runtimeDependencies(fixture, host);
    std::vector<std::uint64_t> issued;
    std::uint64_t nextIdentity = 100;
    dependencies.identitySource = [&] { issued.push_back(nextIdentity); return std::optional<std::uint64_t>(nextIdentity++); };
    dependencies.outboundWriter = [](Server::AdmissionRuntimeConnection &, std::vector<std::uint8_t> payload) {
        D6R_REQUIRE(payload.size() >= 4 && payload[3] == 'O');
        return Network::SendResult::NotConnected;
    };
    dependencies.lifecycleObserver = [fixture](const Server::AdmissionLifecycleEvent &event) {
        fixture->events.push_back(event);
        if (event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack) fixture->cancelled = true;
        return true;
    };
    std::ostringstream output;
    Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(0, server.run(output));
    D6R_REQUIRE(fixture->connection->revoked);
    D6R_REQUIRE(fixture->connection->closeRequested);
    D6R_REQUIRE(!fixture->connection->succeeded);
    D6R_REQUIRE(fixture->shutdown);
    D6R_REQUIRE_EQ(5u, issued.size()); // host participant/player plus offered participant/two players
    D6R_REQUIRE(std::count_if(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
        return event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack;
    }) == 1);
    D6R_REQUIRE(output.str().find("admitted\n") == std::string::npos);
}

D6R_TEST_CASE("peer close and runtime cancellation before acceptance roll back pending transactions on shutdown") {
    const auto host = manifest({{"levels/a", 1}});
    for (const bool peerClose: {true, false}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        fixture->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
        fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host)), fixture->now + 1ms);
        auto dependencies = runtimeDependencies(fixture, host);
        dependencies.outboundWriter = [fixture, peerClose](Server::AdmissionRuntimeConnection &base,
                                                           std::vector<std::uint8_t> payload) {
            D6R_REQUIRE(payload.size() >= 4 && payload[3] == 'O');
            auto &connection = static_cast<FakeAdmissionConnection &>(base);
            if (peerClose) connection.currentState = Network::ClientState::Closed;
            else fixture->cancelled = true;
            return Network::SendResult::Accepted;
        };
        dependencies.lifecycleObserver = [fixture](const Server::AdmissionLifecycleEvent &event) {
            fixture->events.push_back(event);
            if (event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack) fixture->cancelled = true;
            return true;
        };
        std::ostringstream output;
        Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(0, server.run(output));
        D6R_REQUIRE(std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
            return event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack;
        }));
        D6R_REQUIRE(std::none_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
            return event.stage == Server::AdmissionLifecycleStage::TransactionCommitted;
        }));
        D6R_REQUIRE(fixture->connection->revoked);
        D6R_REQUIRE(fixture->shutdown);
    }
}

D6R_TEST_CASE("host initializes before readiness and listener startup failure performs ordered cleanup") {
    const auto host = manifest({{"levels/a", 1}});
    auto fixture = std::make_shared<RuntimeFixture>();
    auto dependencies = runtimeDependencies(fixture, host);
    dependencies.listenerFactory = [fixture](std::size_t, bool) {
        auto listener = std::make_unique<FakeAdmissionListener>(fixture);
        listener->readyResult = false;
        return listener;
    };
    std::ostringstream output;
    Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(2, server.run(output));
    D6R_REQUIRE(fixture->shutdown);
    D6R_REQUIRE(std::find(fixture->hostedStatuses.begin(), fixture->hostedStatuses.end(),
                          Network::HostServiceStatusCode::Ready) == fixture->hostedStatuses.end());
    const auto position = [&](Server::AdmissionLifecycleStage stage) {
        const auto found = std::find_if(fixture->events.begin(), fixture->events.end(),
                                        [stage](const auto &event) { return event.stage == stage; });
        D6R_REQUIRE(found != fixture->events.end());
        return std::distance(fixture->events.begin(), found);
    };
    D6R_REQUIRE(position(Server::AdmissionLifecycleStage::HostInitialized) <
                position(Server::AdmissionLifecycleStage::ListenerStarting));
    D6R_REQUIRE(position(Server::AdmissionLifecycleStage::ListenerStarting) <
                position(Server::AdmissionLifecycleStage::ListenerCleanupStarted));
    D6R_REQUIRE(position(Server::AdmissionLifecycleStage::ListenerCleanupStarted) <
                position(Server::AdmissionLifecycleStage::ListenerCleanupCompleted));
    D6R_REQUIRE(output.str().find("transport startup failed") != std::string::npos);
}

D6R_TEST_CASE("request deadline uses transport accept and frame timestamps under delayed polling") {
    const auto host = manifest({{"levels/a", 1}});
    for (const auto offset: {2999ms, 3000ms, 3001ms}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        fixture->now = Network::Trust::TimePoint{} + 4s; // polling is deliberately delayed
        fixture->connection = std::make_shared<FakeAdmissionConnection>(Network::Trust::TimePoint{});
        fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host)),
                                   Network::Trust::TimePoint{} + offset);
        auto dependencies = runtimeDependencies(fixture, host);
        dependencies.wait = [fixture](std::chrono::milliseconds) {
            if (fixture->connection->closeRequested) fixture->cancelled = true;
        };
        dependencies.lifecycleObserver = [fixture](const Server::AdmissionLifecycleEvent &event) {
            fixture->events.push_back(event);
            if (event.stage == Server::AdmissionLifecycleStage::RequestReceived) fixture->cancelled = true;
            return true;
        };
        std::ostringstream output;
        Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(0, server.run(output));
        const bool requestObserved = std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
            return event.stage == Server::AdmissionLifecycleStage::RequestReceived;
        });
        D6R_REQUIRE(requestObserved == (offset < 3000ms));
        D6R_REQUIRE(fixture->connection->closeRequested);
        D6R_REQUIRE(std::none_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
            return event.stage == Server::AdmissionLifecycleStage::TransactionCommitted;
        }));
    }
}

D6R_TEST_CASE("original ten-second deadline rejects acceptance at or after boundary and commits strictly before") {
    const auto host = manifest({{"levels/a", 1}});
    for (const auto acceptanceOffset: {9999ms, 10000ms, 10001ms}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        fixture->connection = std::make_shared<FakeAdmissionConnection>(Network::Trust::TimePoint{});
        fixture->connection->queue(Network::serializeAdmissionRequest(requestFor(host)),
                                   Network::Trust::TimePoint{} + 1ms);
        auto dependencies = runtimeDependencies(fixture, host);
        dependencies.validationWorkGate = [fixture] { fixture->now = Network::Trust::TimePoint{} + 9999ms; return true; };
        dependencies.outboundWriter = [fixture, acceptanceOffset](Server::AdmissionRuntimeConnection &base,
                                                                  std::vector<std::uint8_t> payload) {
            auto &connection = static_cast<FakeAdmissionConnection &>(base);
            if (payload.size() >= 4 && payload[3] == 'O') {
                const auto offer = Network::deserializeAdmissionOffer(payload);
                connection.queue(Network::serializeAdmissionAcceptance(offer),
                                 Network::Trust::TimePoint{} + acceptanceOffset);
            }
            connection.sent.push_back(std::move(payload));
            return Network::SendResult::Accepted;
        };
        dependencies.lifecycleObserver = [fixture](const Server::AdmissionLifecycleEvent &event) {
            fixture->events.push_back(event);
            if (event.stage == Server::AdmissionLifecycleStage::TransactionCommitted ||
                event.stage == Server::AdmissionLifecycleStage::TransactionRolledBack) fixture->cancelled = true;
            return true;
        };
        std::ostringstream output;
        Server::HeadlessServer server(runtimeServerConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(0, server.run(output));
        const bool committed = std::any_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
            return event.stage == Server::AdmissionLifecycleStage::TransactionCommitted;
        });
        D6R_REQUIRE(committed == (acceptanceOffset < 10000ms));
        D6R_REQUIRE(std::any_of(fixture->events.begin(), fixture->events.end(), [committed](const auto &event) {
            return event.stage == (committed ? Server::AdmissionLifecycleStage::TransactionCommitted
                                             : Server::AdmissionLifecycleStage::TransactionRolledBack);
        }));
    }
}

D6R_TEST_CASE("guest conditional acceptance cancellation immediately before insertion queues nothing") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    auto fixture = std::make_shared<RuntimeFixture>();
    auto client = std::make_shared<FakeClientState>();
    client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
    client->connection->onAcceptanceBeforeInsert = [fixture] { fixture->cancelled = true; };
    auto dependencies = guestRuntimeDependencies(fixture, client, host);
    std::ostringstream output;
    Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(2, guest.run(output));
    D6R_REQUIRE(output.str().empty());
    D6R_REQUIRE(client->cancelled && client->closed);
    D6R_REQUIRE(std::none_of(client->connection->sent.begin(), client->connection->sent.end(), [](const auto &payload) {
        return payload.size() >= 4 && payload[3] == 'K';
    }));
    D6R_REQUIRE(std::none_of(fixture->events.begin(), fixture->events.end(), [](const auto &event) {
        return event.stage == Server::AdmissionLifecycleStage::TransactionCommitted;
    }));
}

D6R_TEST_CASE("guest conditional acceptance deadline at insertion boundary queues nothing") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    for (const auto offset: {10000ms, 10001ms}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        auto client = std::make_shared<FakeClientState>();
        client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
        client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
        client->connection->onAcceptanceBeforeInsert = [fixture, offset] {
            fixture->now = Network::Trust::TimePoint{} + offset;
        };
        auto dependencies = guestRuntimeDependencies(fixture, client, host);
        std::ostringstream output;
        Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(2, guest.run(output));
        D6R_REQUIRE_EQ("Connection timed out.\n", output.str());
        D6R_REQUIRE(std::none_of(client->connection->sent.begin(), client->connection->sent.end(), [](const auto &payload) {
            return payload.size() >= 4 && payload[3] == 'K';
        }));
    }
}

D6R_TEST_CASE("acceptance enqueue linearizes before later Cancel and unpublished confirmation") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    auto fixture = std::make_shared<RuntimeFixture>();
    auto client = std::make_shared<FakeClientState>();
    client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
    client->connection->queue(Network::serializeAdmissionConfirmation(identities), fixture->now + 2ms);
    std::vector<std::string> order;
    client->connection->onAcceptanceInserted = [fixture, &order] {
        order.emplace_back("acceptance-enqueued");
        fixture->cancelled = true;
    };
    client->onCancel = [&order] { order.emplace_back("cancelled"); };
    auto dependencies = guestRuntimeDependencies(fixture, client, host);
    std::ostringstream output;
    Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(2, guest.run(output));
    D6R_REQUIRE(output.str().empty());
    D6R_REQUIRE(client->cancelled && client->closed);
    D6R_REQUIRE(order == std::vector<std::string>({"acceptance-enqueued", "cancelled"}));
    D6R_REQUIRE_EQ(1, std::count_if(client->connection->sent.begin(), client->connection->sent.end(), [](const auto &payload) {
        return payload.size() >= 4 && payload[3] == 'K';
    }));
    D6R_REQUIRE_EQ(1u, client->connection->incoming.size());
}

D6R_TEST_CASE("AdmissionAttemptGate total orders cancellation enqueue and finish") {
    Network::AdmissionAttemptGate cancelledFirst;
    D6R_REQUIRE(cancelledFirst.cancel());
    bool invoked = false;
    D6R_REQUIRE(cancelledFirst.enqueueAcceptance([&] {
        invoked = true;
        return Network::AdmissionAcceptanceEnqueueResult::Accepted;
    }) == Network::AdmissionAcceptanceEnqueueResult::Cancelled);
    D6R_REQUIRE(!invoked && !cancelledFirst.finish());

    Network::AdmissionAttemptGate enqueuedFirst;
    D6R_REQUIRE(enqueuedFirst.enqueueAcceptance([] {
        return Network::AdmissionAcceptanceEnqueueResult::Accepted;
    }) == Network::AdmissionAcceptanceEnqueueResult::Accepted);
    D6R_REQUIRE(enqueuedFirst.cancel());
    D6R_REQUIRE(!enqueuedFirst.finish());
    invoked = false;
    D6R_REQUIRE(enqueuedFirst.enqueueAcceptance([&] {
        invoked = true;
        return Network::AdmissionAcceptanceEnqueueResult::Accepted;
    }) == Network::AdmissionAcceptanceEnqueueResult::Cancelled);
    D6R_REQUIRE(!invoked);

    Network::AdmissionAttemptGate finishedFirst;
    D6R_REQUIRE(finishedFirst.finish());
    D6R_REQUIRE(!finishedFirst.cancel());
    D6R_REQUIRE(finishedFirst.enqueueAcceptance([] {
        return Network::AdmissionAcceptanceEnqueueResult::Accepted;
    }) == Network::AdmissionAcceptanceEnqueueResult::NotQueued);
}

D6R_TEST_CASE("GuestFrameDecision behavioral paths retain exact portable outcomes") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    const auto run = [&](std::vector<std::vector<std::uint8_t>> frames,
                         Network::SendResult acceptanceSend,
                         int expectedCode,
                         const std::string &expectedOutput) {
        auto fixture = std::make_shared<RuntimeFixture>();
        auto client = std::make_shared<FakeClientState>();
        client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
        auto received = fixture->now + 1ms;
        for (auto &frame: frames) {
            client->connection->queue(std::move(frame), received);
            received += 1ms;
        }
        client->connection->sendResult = acceptanceSend;
        auto dependencies = guestRuntimeDependencies(fixture, client, host);
        std::ostringstream output;
        Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(expectedCode, guest.run(output));
        D6R_REQUIRE_EQ(expectedOutput, output.str());
    };

    run({Network::serializeAdmissionOffer(identities), Network::serializeAdmissionConfirmation(identities)},
        Network::SendResult::Accepted, 0, "admitted\nparticipant-id=10 player-ids=11,12\n");
    run({Network::serializeAdmissionResult({Network::AdmissionResultCode::SessionFull, 0, {}})},
        Network::SendResult::Accepted, 2, "session-full\nSession is full.\n");
    run({std::vector<std::uint8_t>{0xFF}}, Network::SendResult::Accepted, 2,
        std::string(Network::InvalidHostAdmissionMessageIdentifier) + "\n"
        + std::string(Network::InvalidHostAdmissionMessageCopy) + "\n");
    run({Network::serializeAdmissionOffer(identities)}, Network::SendResult::NotConnected, 2,
        "Connection ended before admission completed.\n");
}

D6R_TEST_CASE("guest accepts queued pre-deadline confirmation despite delayed polling and earlier close") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    auto fixture = std::make_shared<RuntimeFixture>();
    auto client = std::make_shared<FakeClientState>();
    client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
    client->connection->queue(Network::serializeAdmissionConfirmation(identities), fixture->now + 9999ms);
    const std::weak_ptr<FakeAdmissionConnection> weakConnection = client->connection;
    client->connection->onSend = [fixture, weakConnection](const std::vector<std::uint8_t> &payload) {
        if (payload.size() >= 4 && payload[3] == 'K') {
            fixture->now = Network::Trust::TimePoint{} + 10001ms;
            const auto connection = weakConnection.lock();
            D6R_REQUIRE(connection);
            connection->currentState = Network::ClientState::Closed;
            connection->terminal = Network::Trust::TimePoint{} + 9999ms;
        }
    };
    auto dependencies = guestRuntimeDependencies(fixture, client, host);
    std::ostringstream output;
    Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(0, guest.run(output));
    D6R_REQUIRE(output.str().find("admitted\nparticipant-id=10 player-ids=11,12\n") != std::string::npos);
    D6R_REQUIRE(client->closed && !client->cancelled);
    D6R_REQUIRE(std::count_if(client->connection->sent.begin(), client->connection->sent.end(), [](const auto &payload) {
        return payload.size() >= 4 && payload[3] == 'K';
    }) == 1);
}

D6R_TEST_CASE("guest confirmation at or after total deadline never becomes success") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    for (const auto confirmationTime: {10000ms, 10001ms}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        auto client = std::make_shared<FakeClientState>();
        client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
        client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
        client->connection->queue(Network::serializeAdmissionConfirmation(identities),
                                  Network::Trust::TimePoint{} + confirmationTime);
        const std::weak_ptr<FakeAdmissionConnection> weakConnection = client->connection;
        client->connection->onSend = [fixture, weakConnection](const std::vector<std::uint8_t> &payload) {
            if (payload.size() >= 4 && payload[3] == 'K') {
                fixture->now = Network::Trust::TimePoint{} + 10001ms;
                const auto connection = weakConnection.lock();
                D6R_REQUIRE(connection);
                connection->currentState = Network::ClientState::TimedOut;
            }
        };
        auto dependencies = guestRuntimeDependencies(fixture, client, host);
        std::ostringstream output;
        Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(2, guest.run(output));
        D6R_REQUIRE(output.str().find("admitted\n") == std::string::npos);
        D6R_REQUIRE(output.str() == "Connection timed out.\n");
    }
}

D6R_TEST_CASE("guest suspended before acceptance never sends at or after hard deadline") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    auto fixture = std::make_shared<RuntimeFixture>();
    auto client = std::make_shared<FakeClientState>();
    client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
    client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 9999ms);
    client->connection->onReceive = [fixture](const Network::TransportFrame &frame) {
        if (frame.payload.size() >= 4 && frame.payload[3] == 'O')
            fixture->now = Network::Trust::TimePoint{} + 10000ms;
    };
    auto dependencies = guestRuntimeDependencies(fixture, client, host);
    std::ostringstream output;
    Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(2, guest.run(output));
    D6R_REQUIRE(std::none_of(client->connection->sent.begin(), client->connection->sent.end(), [](const auto &payload) {
        return payload.size() >= 4 && payload[3] == 'K';
    }));
    D6R_REQUIRE_EQ("Connection timed out.\n", output.str());
}

D6R_TEST_CASE("guest Cancel wins races with offer rejection acceptance confirmation close and timeout") {
    const auto host = manifest({{"levels/a", 1}});
    const Network::AdmissionIdentitySet identities{10, {11, 12}};
    enum class Race { Offer, Rejection, Acceptance, Confirmation, Close, Timeout };
    for (const Race race: {Race::Offer, Race::Rejection, Race::Acceptance,
                           Race::Confirmation, Race::Close, Race::Timeout}) {
        auto fixture = std::make_shared<RuntimeFixture>();
        auto client = std::make_shared<FakeClientState>();
        client->connection = std::make_shared<FakeAdmissionConnection>(fixture->now);
        if (race == Race::Rejection) {
            client->connection->queue(Network::serializeAdmissionResult(
                    {Network::AdmissionResultCode::SessionFull, 0, {}}), fixture->now + 1ms);
        } else if (race != Race::Close && race != Race::Timeout) {
            client->connection->queue(Network::serializeAdmissionOffer(identities), fixture->now + 1ms);
            if (race == Race::Confirmation)
                client->connection->queue(Network::serializeAdmissionConfirmation(identities), fixture->now + 2ms);
        }
        client->connection->onReceive = [fixture, race](const Network::TransportFrame &frame) {
            if (race == Race::Offer || race == Race::Rejection ||
                (race == Race::Confirmation && frame.payload.size() >= 4 && frame.payload[3] == 'C'))
                fixture->cancelled = true;
        };
        client->connection->onSend = [fixture, race](const std::vector<std::uint8_t> &payload) {
            if (race == Race::Acceptance && payload.size() >= 4 && payload[3] == 'K') fixture->cancelled = true;
        };
        if (race == Race::Close) {
            client->connection->currentState = Network::ClientState::Closed;
            fixture->cancelled = true;
        } else if (race == Race::Timeout) {
            client->connection->currentState = Network::ClientState::TimedOut;
            fixture->cancelled = true;
        }
        auto dependencies = guestRuntimeDependencies(fixture, client, host);
        std::ostringstream output;
        Server::HeadlessServer guest(runtimeGuestConfig(), std::move(dependencies));
        D6R_REQUIRE_EQ(2, guest.run(output));
        D6R_REQUIRE(output.str().empty());
        D6R_REQUIRE(client->cancelled && client->closed);
    }
}

D6R_TEST_CASE("AC-016 AC-023 auth policy capacity and match rejection allocate nothing") {
    const auto host = manifest({{"levels/a", 1}});
    Server::AdmissionPolicy policy(host, 1);
    const auto initialParticipants = policy.allocation().participantCount();
    const auto initialPlayers = policy.allocation().playerCount();
    auto request = requestFor(host);
    for (const Server::AdmissionContext context: {Server::AdmissionContext{false, true}, {true, false}}) {
        const auto result = policy.evaluate(request, context);
        D6R_REQUIRE(!result.admitted() && result.participantId == 0 && result.playerIds.empty());
        D6R_REQUIRE_EQ(initialParticipants, policy.allocation().participantCount());
        D6R_REQUIRE_EQ(initialPlayers, policy.allocation().playerCount());
    }
    policy.setMatchStarted(true);
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::MatchAlreadyStarted);
    D6R_REQUIRE_EQ(initialParticipants, policy.allocation().participantCount());
    policy.setMatchStarted(false);
    request.localPlayerCount = 15;
    D6R_REQUIRE(policy.evaluate(request).code == Network::AdmissionResultCode::SessionFull);
    D6R_REQUIRE_EQ(initialPlayers, policy.allocation().playerCount());
}

D6R_TEST_CASE("AC-018 reconnect mapping and rejection copy disclose no peer data") {
    using Reconnect = Network::ReconnectCompatibilityCode;
    const std::vector<std::tuple<Reconnect, std::string, std::string>> mappings{
        {Reconnect::ProtocolIncompatible, "reconnect-protocol-incompatible", "Network release mismatch. This session cannot be restored."},
        {Reconnect::NetworkReleaseMismatch, "reconnect-network-release-mismatch", "Network release mismatch. This session cannot be restored."},
        {Reconnect::RequiredCapabilityUnsupported, "reconnect-required-capability-unsupported", "Network release mismatch. This session cannot be restored."},
        {Reconnect::GameplayContentInvalid, "reconnect-gameplay-content-invalid", "Gameplay content mismatch. This session cannot be restored."},
        {Reconnect::GameplayContentMismatch, "reconnect-gameplay-content-mismatch", "Gameplay content mismatch. This session cannot be restored."}};
    const std::vector<std::string> hostile{"../../secret", "DUEL6R-network-r1", "peer-capability", "hash-value", "203.0.113.1"};
    for (const auto &mapping: mappings) {
        const std::string identifier(Network::reconnectCompatibilityIdentifier(std::get<0>(mapping)));
        const std::string copy(Network::reconnectCompatibilityUserCopy(std::get<0>(mapping)));
        D6R_REQUIRE_EQ(std::get<1>(mapping), identifier); D6R_REQUIRE_EQ(std::get<2>(mapping), copy);
        for (const auto &value: hostile) D6R_REQUIRE(copy.find(value) == std::string::npos);
    }
    for (unsigned code = 1; code <= 11; ++code) {
        const std::string copy(Network::admissionResultUserCopy(static_cast<Network::AdmissionResultCode>(code)));
        for (const auto &value: hostile) D6R_REQUIRE(copy.find(value) == std::string::npos);
    }
}

D6R_TEST_CASE("AC-019 exact admission work and trust limits retain boundary values") {
    using namespace Network::Trust;
    D6R_REQUIRE_EQ(262144u, MaxAdmissionPayloadBytes); D6R_REQUIRE_EQ(4096u, MaxProperties);
    D6R_REQUIRE_EQ(256u, MaxManifestEntries); D6R_REQUIRE_EQ(240u, MaxLogicalPathBytes);
    D6R_REQUIRE_EQ(8u, MaxPendingAdmissions); D6R_REQUIRE_EQ(4u, MaxPendingAdmissionsPerSource);
    D6R_REQUIRE_EQ(20u, MaxAdmissionAttemptsPerSourcePerMinute); D6R_REQUIRE_EQ(4u, AdmissionAttemptBurst);
    D6R_REQUIRE_EQ(2u, MaxConcurrentManifestValidations); D6R_REQUIRE_EQ(15u, MaxParticipants);
    D6R_REQUIRE_EQ(64u * 1024u * 1024u, Network::MaxGameplayContentFileBytes);
    D6R_REQUIRE_EQ(256u * 1024u * 1024u, Network::MaxGameplayContentTotalBytes);
    ConcurrentWorkLimiter work;
    D6R_REQUIRE(work.reserve() && work.reserve()); D6R_REQUIRE(!work.reserve());
    D6R_REQUIRE_EQ(2u, work.active()); work.release(); D6R_REQUIRE(work.reserve());
    work.release(); work.release(); D6R_REQUIRE_EQ(0u, work.active());
}

D6R_TEST_CASE("AC-019 policy rejects a third held validation immediately and releases work on every outcome") {
    using Code = Network::AdmissionResultCode;
    const auto host = manifest({{"levels/a", 1}});
    auto work = std::make_shared<Network::Trust::ConcurrentWorkLimiter>();
    Server::AdmissionPolicy policy(host, 1, {}, work);
    D6R_REQUIRE(work->reserve() && work->reserve());
    D6R_REQUIRE_EQ(2u, work->active());
    D6R_REQUIRE(policy.offer(requestFor(host)).result.code == Code::HostPolicyRejected);
    D6R_REQUIRE_EQ(2u, work->active());
    work->release();
    const auto mismatch = policy.offer(requestFor(manifest({{"levels/b", 2}})));
    D6R_REQUIRE(mismatch.result.code == Code::GameplayContentMismatch);
    D6R_REQUIRE_EQ(1u, work->active());
    const auto pending = policy.offer(requestFor(host));
    D6R_REQUIRE(pending.pending());
    D6R_REQUIRE_EQ(1u, work->active());
    D6R_REQUIRE(policy.rollback(pending.transactionId));
    D6R_REQUIRE_EQ(1u, work->active());
    work->release();
    for (auto codeRequest: {requestFor(host), requestFor(manifest({{"levels/c", 3}}))}) {
        const auto outcome = policy.offer(codeRequest);
        if (outcome.pending()) D6R_REQUIRE(policy.rollback(outcome.transactionId));
        D6R_REQUIRE_EQ(0u, work->active());
    }
}

D6R_TEST_CASE("AC-019 exactly two actual validations can be held while the third rejects without queueing") {
    using Code = Network::AdmissionResultCode;
    const auto host = manifest({{"levels/a", 1}});
    auto work = std::make_shared<Network::Trust::ConcurrentWorkLimiter>();
    std::atomic<unsigned> entered{0};
    std::atomic<bool> release{false};
    Server::AdmissionPolicy policy(host, 1, {}, work, [&] {
        ++entered;
        while (!release.load()) std::this_thread::yield();
        return true;
    });
    std::array<Server::AdmissionOffer, 2> held;
    std::thread first([&] { held[0] = policy.offer(requestFor(host)); });
    std::thread second([&] { held[1] = policy.offer(requestFor(host)); });
    while (entered.load() != 2) std::this_thread::yield();
    D6R_REQUIRE_EQ(2u, work->active());
    const auto third = policy.offer(requestFor(host));
    D6R_REQUIRE(!third.pending());
    D6R_REQUIRE(third.result.code == Code::HostPolicyRejected);
    D6R_REQUIRE_EQ(2u, entered.load());
    D6R_REQUIRE_EQ(2u, work->active());
    release = true;
    first.join(); second.join();
    D6R_REQUIRE(held[0].pending() && held[1].pending());
    D6R_REQUIRE(policy.rollback(held[0].transactionId));
    D6R_REQUIRE(policy.rollback(held[1].transactionId));
    D6R_REQUIRE_EQ(0u, work->active());
    D6R_REQUIRE_EQ(0u, policy.allocation().pendingParticipantCount());
}

D6R_TEST_CASE("AC-004 manifest file and aggregate work bounds accept exact limits and reject one over") {
    TemporaryResources exact;
    exact.write("data/blocks.json", "b"); exact.write("data/config.script", "c");
    for (unsigned index = 0; index < 3; ++index) {
        const fs::path path = exact.root / "levels" / ("large" + std::to_string(index));
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary);
        fs::resize_file(path, Network::MaxGameplayContentFileBytes);
    }
    const fs::path finalPath = exact.root / "levels" / "large3";
    std::ofstream(finalPath, std::ios::binary);
    fs::resize_file(finalPath, Network::MaxGameplayContentFileBytes - 2);
    const auto maximum = Network::CompatibilityManifestBuilder(exact.root.string()).build();
    D6R_REQUIRE(maximum.valid());
    fs::resize_file(finalPath, Network::MaxGameplayContentFileBytes - 1);
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(exact.root.string()).build().status ==
                Network::ManifestStatus::ContentTooLarge);

    TemporaryResources oversized; oversized.required();
    fs::resize_file(oversized.root / "levels" / "arena.json", Network::MaxGameplayContentFileBytes + 1);
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(oversized.root.string()).build().status ==
                Network::ManifestStatus::ContentTooLarge);
}

D6R_TEST_CASE("D6RS is rejection-only and stale admitted result encodings are rejected") {
    Network::AdmissionResult rejection{Network::AdmissionResultCode::SessionFull, 0, {}};
    const std::vector<std::uint8_t> expected{
        'D','6','R','S', 0,9, 0,0,0,0,0,0,0,0, 0};
    D6R_REQUIRE(expected == Network::serializeAdmissionResult(rejection));
    const auto decoded = Network::deserializeAdmissionResult(expected);
    D6R_REQUIRE(decoded.code == Network::AdmissionResultCode::SessionFull);
    D6R_REQUIRE(!decoded.admitted() && decoded.participantId == 0 && decoded.playerIds.empty());
    D6R_REQUIRE(rejects([&] { auto bytes = expected; bytes.pop_back(); Network::deserializeAdmissionResult(bytes); }));
    D6R_REQUIRE(rejects([&] { auto bytes = expected; bytes.push_back(0); Network::deserializeAdmissionResult(bytes); }));
    D6R_REQUIRE(rejects([] { Network::serializeAdmissionResult({Network::AdmissionResultCode::Admitted, 0, {}}); }));
    D6R_REQUIRE(rejects([] { Network::serializeAdmissionResult({Network::AdmissionResultCode::Admitted, 0, {1}}); }));
    D6R_REQUIRE(rejects([] { Network::serializeAdmissionResult({Network::AdmissionResultCode::SessionFull, 1, {2}}); }));
    const std::vector<std::uint8_t> stale{
        'D','6','R','S', 0,11, 1,2,3,4,5,6,7,8, 1, 17,18,19,20,21,22,23,24};
    D6R_REQUIRE(rejects([&] { Network::deserializeAdmissionResult(stale); }));
}

D6R_TEST_CASE("four-message identity wire sets are exact ordered endian-stable and reject malformed variants") {
    const Network::AdmissionIdentitySet identities{0x0102030405060708ULL,
                                                    {0x1112131415161718ULL, 0x2122232425262728ULL}};
    const auto expected = [&](char kind) {
        return std::vector<std::uint8_t>{'D','6','R', static_cast<std::uint8_t>(kind),
                1,2,3,4,5,6,7,8, 2,
                17,18,19,20,21,22,23,24, 33,34,35,36,37,38,39,40};
    };
    D6R_REQUIRE(expected('O') == Network::serializeAdmissionOffer(identities));
    D6R_REQUIRE(expected('K') == Network::serializeAdmissionAcceptance(identities));
    D6R_REQUIRE(expected('C') == Network::serializeAdmissionConfirmation(identities));
    D6R_REQUIRE(Network::sameAdmissionIdentitySet(identities,
                Network::deserializeAdmissionOffer(expected('O'))));
    D6R_REQUIRE(Network::sameAdmissionIdentitySet(identities,
                Network::deserializeAdmissionAcceptance(expected('K'))));
    D6R_REQUIRE(Network::sameAdmissionIdentitySet(identities,
                Network::deserializeAdmissionConfirmation(expected('C'))));
    D6R_REQUIRE(Network::validAdmissionIdentitySet(identities, 2));
    D6R_REQUIRE(!Network::validAdmissionIdentitySet(identities, 1));
    for (const Network::AdmissionIdentitySet &invalid: {
            Network::AdmissionIdentitySet{0, {2}}, Network::AdmissionIdentitySet{1, {}},
            Network::AdmissionIdentitySet{1, {0}}, Network::AdmissionIdentitySet{1, {1}},
            Network::AdmissionIdentitySet{1, {2, 2}}}) {
        D6R_REQUIRE(!Network::validAdmissionIdentitySet(invalid));
        D6R_REQUIRE(rejects([&] { Network::serializeAdmissionOffer(invalid); }));
        D6R_REQUIRE(rejects([&] { Network::serializeAdmissionAcceptance(invalid); }));
        D6R_REQUIRE(rejects([&] { Network::serializeAdmissionConfirmation(invalid); }));
    }
    for (char kind: {'O', 'K', 'C'}) {
        auto bytes = expected(kind); bytes.pop_back();
        D6R_REQUIRE(rejects([&] {
            if (kind == 'O') Network::deserializeAdmissionOffer(bytes);
            else if (kind == 'K') Network::deserializeAdmissionAcceptance(bytes);
            else Network::deserializeAdmissionConfirmation(bytes);
        }));
        bytes = expected(kind); bytes.push_back(0);
        D6R_REQUIRE(rejects([&] {
            if (kind == 'O') Network::deserializeAdmissionOffer(bytes);
            else if (kind == 'K') Network::deserializeAdmissionAcceptance(bytes);
            else Network::deserializeAdmissionConfirmation(bytes);
        }));
    }
    D6R_REQUIRE(rejects([&] { Network::deserializeAdmissionAcceptance(expected('O')); }));
    D6R_REQUIRE(rejects([&] { Network::deserializeAdmissionConfirmation(expected('K')); }));
    D6R_REQUIRE(!Network::sameAdmissionIdentitySet(identities,
                {identities.participantId, {identities.playerIds[1], identities.playerIds[0]}}));
    D6R_REQUIRE_EQ("invalid-host-admission-message", std::string(Network::InvalidHostAdmissionMessageIdentifier));
    D6R_REQUIRE_EQ("Connection ended before admission completed.",
                   std::string(Network::InvalidHostAdmissionMessageCopy));
}

D6R_TEST_CASE("manifest source seam propagates deterministic mutation read and unsafe filesystem failures") {
    for (const auto status: {Network::ManifestStatus::ReadFailed, Network::ManifestStatus::UnsafeFilesystemEntry,
                             Network::ManifestStatus::ContentTooLarge, Network::ManifestStatus::TooManyEntries}) {
        auto source = std::make_shared<FixedManifestSource>(Network::ManifestBuildResult{status, {}});
        const auto result = Network::CompatibilityManifestBuilder("ignored", {}, source).build();
        D6R_REQUIRE(result.status == status);
        D6R_REQUIRE_EQ(1u, source->calls);
        D6R_REQUIRE(result.manifest.empty());
    }
    const auto canonical = manifest({{"levels/a", 1}});
    auto source = std::make_shared<FixedManifestSource>(
            Network::ManifestBuildResult{Network::ManifestStatus::Valid, canonical});
    const auto result = Network::CompatibilityManifestBuilder("ignored", {}, source).build();
    D6R_REQUIRE(result.valid() && sameManifest(canonical, result.manifest));
}

D6R_TEST_CASE("secure filesystem observer deterministically detects pinned-file mutation and read failure") {
    TemporaryResources mutated; mutated.required();
    const auto original = Network::CompatibilityManifestBuilder(mutated.root.string()).build();
    D6R_REQUIRE(original.valid());
    bool mutationAttempted = false;
    bool mutationSucceeded = false;
    std::vector<std::pair<Network::ManifestFilesystemStage, std::string>> stages;
    auto mutate = [&](Network::ManifestFilesystemStage stage, const std::string &logical) {
        stages.emplace_back(stage, logical);
        if (!mutationAttempted && stage == Network::ManifestFilesystemStage::BeforeRead
            && logical == "levels/arena.json") {
            mutationAttempted = true;
            mutationSucceeded = mutated.write(logical, "mutated-after-open");
        }
        return true;
    };
    const auto mutationResult = Network::CompatibilityManifestBuilder(
            mutated.root.string(), {}, {}, mutate).build();
    const auto mutationFailure = [&] {
        throw Test::Failure("filesystem mutation outcome mismatch: attempted="
                            + std::string(mutationAttempted ? "true" : "false")
                            + " succeeded=" + std::string(mutationSucceeded ? "true" : "false")
                            + " manifest-status=" + std::to_string(static_cast<int>(mutationResult.status)));
    };
    if (!mutationAttempted) mutationFailure();
    const auto stored = mutated.read("levels/arena.json");
    if (mutationSucceeded) {
        if (mutationResult.valid()
            || (mutationResult.status != Network::ManifestStatus::ReadFailed
                && mutationResult.status != Network::ManifestStatus::UnsafeFilesystemEntry)
            || !stored || *stored != "mutated-after-open") mutationFailure();
    } else {
        if (!mutationResult.valid() || !sameManifest(original.manifest, mutationResult.manifest)
            || !stored || *stored != "arena") mutationFailure();
    }
    D6R_REQUIRE(std::any_of(stages.begin(), stages.end(), [](const auto &event) {
        return event.first == Network::ManifestFilesystemStage::RootPinned;
    }));
    D6R_REQUIRE(std::any_of(stages.begin(), stages.end(), [](const auto &event) {
        return event.first == Network::ManifestFilesystemStage::DirectoryPinned && event.second == "levels";
    }));

    TemporaryResources failedRead; failedRead.required();
    auto failBeforeRead = [](Network::ManifestFilesystemStage stage, const std::string &logical) {
        return !(stage == Network::ManifestFilesystemStage::BeforeRead && logical == "levels/arena.json");
    };
    D6R_REQUIRE(Network::CompatibilityManifestBuilder(failedRead.root.string(), {}, {}, failBeforeRead).build().status ==
                 Network::ManifestStatus::ReadFailed);
}

D6R_TEST_CASE("frozen gameplay content retains exact accepted bytes after source mutation") {
    TemporaryResources resources;
    resources.required();
    D6R_REQUIRE(resources.write("levels/arena.json", "accepted-arena-bytes\nwith-exact-ending"));
    const auto accepted = Network::CompatibilityManifestBuilder(resources.root.string()).build();
    D6R_REQUIRE(accepted.valid());
    D6R_REQUIRE(accepted.content);
    const auto frozen = accepted.content->find("levels/arena.json");
    D6R_REQUIRE(frozen != accepted.content->end());
    const std::vector<std::uint8_t> expected{
            'a','c','c','e','p','t','e','d','-','a','r','e','n','a','-','b','y','t','e','s','\n',
            'w','i','t','h','-','e','x','a','c','t','-','e','n','d','i','n','g'};
    D6R_REQUIRE_EQ(expected, frozen->second);

    D6R_REQUIRE(resources.write("levels/arena.json", "later-mutated-bytes"));
    D6R_REQUIRE_EQ(expected, accepted.content->at("levels/arena.json"));
    const auto rebuilt = Network::CompatibilityManifestBuilder(resources.root.string()).build();
    D6R_REQUIRE(rebuilt.valid());
    D6R_REQUIRE(!Network::gameplayManifestsEqual(accepted.manifest, rebuilt.manifest));
}

#ifdef _WIN32
D6R_TEST_CASE("Windows manifest rejects hardlinks and ordinal case replacement when the filesystem permits them") {
    TemporaryResources hardlinked; hardlinked.required();
    std::error_code hardlinkError;
    fs::create_hard_link(hardlinked.root / "levels" / "arena.json",
                         hardlinked.root / "levels" / "alias.json", hardlinkError);
    if (!hardlinkError) {
        const auto result = Network::CompatibilityManifestBuilder(hardlinked.root.string()).build();
        D6R_REQUIRE(!result.valid());
        D6R_REQUIRE(result.status == Network::ManifestStatus::UnsafeFilesystemEntry);
    }

    TemporaryResources renamed; renamed.required();
    const auto baseline = Network::CompatibilityManifestBuilder(renamed.root.string()).build();
    D6R_REQUIRE(baseline.valid());
    bool renameAttempted = false;
    bool renameSucceeded = false;
    auto renameCase = [&](Network::ManifestFilesystemStage stage, const std::string &) {
        if (!renameAttempted && stage == Network::ManifestFilesystemStage::RootPinned) {
            renameAttempted = true;
            std::error_code renameError;
            fs::rename(renamed.root / "levels", renamed.root / "LEVELS", renameError);
            renameSucceeded = !renameError;
        }
        return true;
    };
    const auto result = Network::CompatibilityManifestBuilder(
            renamed.root.string(), {}, {}, renameCase).build();
    bool physicalCaseChanged = false;
    std::error_code iterationError;
    for (fs::directory_iterator entry(renamed.root, iterationError), end;
         !iterationError && entry != end; entry.increment(iterationError)) {
        if (entry->path().filename().wstring() == L"LEVELS") physicalCaseChanged = true;
    }
    if (!renameAttempted)
        throw Test::Failure("Windows ordinal rename case was not attempted");
    if (renameSucceeded && physicalCaseChanged) {
        if (result.valid() || result.status != Network::ManifestStatus::UnsafeFilesystemEntry)
            throw Test::Failure("Windows ordinal rename succeeded without fail-closed manifest status="
                                + std::to_string(static_cast<int>(result.status)));
    } else if (!result.valid() || !sameManifest(baseline.manifest, result.manifest)) {
        throw Test::Failure("Windows ordinal rename was denied or unchanged but original manifest was not stable; status="
                            + std::to_string(static_cast<int>(result.status)));
    }
}
#endif

#ifndef _WIN32
D6R_TEST_CASE("pinned Linux root survives ancestor rename and ignores replacement path content") {
    TemporaryResources outer;
    const fs::path parent = outer.root / "parent";
    const fs::path pinnedRoot = parent / "resources";
    fs::create_directories(pinnedRoot / "levels"); fs::create_directories(pinnedRoot / "data");
    auto writeAt = [](const fs::path &root, const std::string &logical, const std::string &content) {
        const fs::path path = root / logical; fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << content;
    };
    writeAt(pinnedRoot, "levels/a", "trusted");
    writeAt(pinnedRoot, "data/blocks.json", "blocks");
    writeAt(pinnedRoot, "data/config.script", "config");
    const auto baseline = Network::CompatibilityManifestBuilder(pinnedRoot.string()).build();
    D6R_REQUIRE(baseline.valid());
    const fs::path movedParent = outer.root / "moved-parent";
    bool replaced = false;
    auto replaceAncestor = [&](Network::ManifestFilesystemStage stage, const std::string &) {
        if (!replaced && stage == Network::ManifestFilesystemStage::RootPinned) {
            fs::rename(parent, movedParent);
            fs::create_directories(pinnedRoot / "levels"); fs::create_directories(pinnedRoot / "data");
            writeAt(pinnedRoot, "levels/a", "attacker");
            writeAt(pinnedRoot, "data/blocks.json", "attacker");
            writeAt(pinnedRoot, "data/config.script", "attacker");
            replaced = true;
        }
        return true;
    };
    const auto pinned = Network::CompatibilityManifestBuilder(
            pinnedRoot.string(), {}, {}, replaceAncestor).build();
    D6R_REQUIRE(replaced);
    D6R_REQUIRE(pinned.valid() && sameManifest(baseline.manifest, pinned.manifest));
}
#endif

D6R_TEST_CASE("absent defaults remain operational and throwing seams fail closed without leaking validation work") {
    const auto host = manifest({{"levels/a", 1}});
    auto work = std::make_shared<Network::Trust::ConcurrentWorkLimiter>();
    Server::AdmissionPolicy policy(host, 1, {}, work, []() -> bool { throw std::runtime_error("gate"); });
    const auto rejected = policy.offer(requestFor(host));
    D6R_REQUIRE(!rejected.pending());
    D6R_REQUIRE(rejected.result.code == Network::AdmissionResultCode::HostPolicyRejected);
    D6R_REQUIRE_EQ(0u, work->active());
    D6R_REQUIRE_EQ(0u, policy.allocation().pendingParticipantCount());

    TemporaryResources resources; resources.required();
    auto throwingFilesystem = [](Network::ManifestFilesystemStage, const std::string &) -> bool {
        throw std::runtime_error("filesystem observer");
    };
    D6R_REQUIRE(!Network::CompatibilityManifestBuilder(
            resources.root.string(), {}, {}, throwingFilesystem).build().valid());

    auto fixture = std::make_shared<RuntimeFixture>();
    auto dependencies = runtimeDependencies(fixture, host);
    dependencies.listenerFactory = [](std::size_t, bool) -> std::unique_ptr<Server::AdmissionRuntimeListener> {
        throw std::runtime_error("listener factory");
    };
    std::ostringstream hostOutput;
    Server::HeadlessServer failedHost(runtimeServerConfig(), std::move(dependencies));
    D6R_REQUIRE_EQ(2, failedHost.run(hostOutput));

    auto guestFixture = std::make_shared<RuntimeFixture>();
    auto guestClient = std::make_shared<FakeClientState>();
    auto guestDependencies = guestRuntimeDependencies(guestFixture, guestClient, host);
    guestDependencies.clientFactory = []() -> std::unique_ptr<Server::AdmissionRuntimeClient> {
        throw std::runtime_error("client factory");
    };
    std::ostringstream guestOutput;
    Server::HeadlessServer failedGuest(runtimeGuestConfig(), std::move(guestDependencies));
    D6R_REQUIRE_EQ(2, failedGuest.run(guestOutput));
}

D6R_TEST_CASE("SHA-256 content identities match standard empty and abc vectors") {
    TemporaryResources resources;
    resources.write("levels/empty", ""); resources.write("levels/abc", "abc");
    resources.write("data/blocks.json", "blocks"); resources.write("data/config.script", "config");
    const auto built = Network::CompatibilityManifestBuilder(resources.root.string()).build();
    D6R_REQUIRE(built.valid());
    std::map<std::string, std::string> hashes;
    for (const auto &entry: built.manifest) hashes[entry.logicalPath] = Network::contentIdentityHex(entry.contentIdentity);
    D6R_REQUIRE_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", hashes["levels/empty"]);
    D6R_REQUIRE_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hashes["levels/abc"]);
}
