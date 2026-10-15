#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
#include <mbedtls/ssl.h>
#include "source/client/NetworkSessionRuntime.h"
#include "source/network/CompatibilityManifest.h"
#include "source/network/PublicSession.h"
#include "source/network/SessionLifecycle.h"
#include "PasswordBufferObservation.h"

using namespace Duel6;
using namespace std::chrono_literals;
namespace L = Network::Lifecycle;
using J = Client::NetworkJourney;
using Clock = std::chrono::steady_clock;
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }

// Observers delegate every maintained SDK operation unchanged. Protocol copies
// are bounded, erased and private to this fixture; never grant runtime authority.
namespace Observed {
    std::atomic<unsigned> offers{0}, ends{0}, notices{0}, authDenials{0}, fatalHandshakes{0};
    std::atomic<std::uint64_t> session{0};
    std::atomic<unsigned> handshakeCalls{0};
    std::mutex mutex;
    std::optional<L::ReconnectGrant> original, latest;
    std::atomic<bool> capture{false};
    void grant(const L::ReconnectGrant &value) {
        if (!capture) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (!original) original = value;
        if (value.sessionId == original->sessionId && value.participantId == original->participantId) latest = value;
    }
    L::ReconnectGrant saved(bool first) {
        std::lock_guard<std::mutex> lock(mutex);
        require((first ? original : latest).has_value(), "actual reconnect grant observer missing");
        return *(first ? original : latest);
    }
}
extern "C" int __real_mbedtls_ssl_read(mbedtls_ssl_context *, unsigned char *, std::size_t);
extern "C" int __wrap_mbedtls_ssl_read(mbedtls_ssl_context *ctx, unsigned char *data, std::size_t size) {
    const auto result = __real_mbedtls_ssl_read(ctx, data, size);
    if (result > 0 && result <= static_cast<int>(Network::Trust::MaxAdmissionPayloadBytes)) {
        std::vector<std::uint8_t> payload(data, data + result);
        struct Erase { std::vector<std::uint8_t> &value; ~Erase() { Network::Trust::secureEraseMemory(value.data(), value.size()); } } erase{payload};
        if (const auto value = L::deserializeReconnectGrant(payload)) Observed::grant(*value);
        if (const auto value = L::deserializeReconnectResponse(payload)) {
            if (value->outcome == L::ReconnectOutcome::AuthorizationFailed) ++Observed::authDenials;
            if (value->nextGrant) Observed::grant(*value->nextGrant);
        }
        if (L::deserializeIntentionalHostEnd(payload)) ++Observed::ends;
        if (!Network::PublicSession::terminalReason(payload, Observed::session.load()).empty()) ++Observed::notices;
        try { (void) Network::deserializeAdmissionOffer(payload); ++Observed::offers; } catch (...) {}
    }
    return result;
}
extern "C" int __real_mbedtls_ssl_handshake(mbedtls_ssl_context *);
extern "C" int __wrap_mbedtls_ssl_handshake(mbedtls_ssl_context *ctx) {
    ++Observed::handshakeCalls;
    const auto result = __real_mbedtls_ssl_handshake(ctx);
    if (result < 0 && result != MBEDTLS_ERR_SSL_WANT_READ && result != MBEDTLS_ERR_SSL_WANT_WRITE)
        ++Observed::fatalHandshakes;
    return result;
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    try {
        require(argc == 5, "expected endpoint port, resource root, scenario and scoped-peer port");
        const std::filesystem::path root(argv[2]);
        const std::string scenario(argv[3]);
        std::string input;
        require(static_cast<bool>(std::getline(std::cin, input)), "private old Password fixture input");
        auto fixture = std::make_shared<Network::SessionPassword>(input);
        Network::Trust::secureEraseMemory(input.data(), input.size()); input.clear();
        require(static_cast<bool>(std::getline(std::cin, input)), "private new Password fixture input");
        auto rotated = std::make_shared<Network::SessionPassword>(input);
        Network::Trust::secureEraseMemory(input.data(), input.size()); input.clear();
        Network::Endpoint endpoint{"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1]))};
        Client::NetworkSessionRuntime first, second;
        auto pump = [&](auto predicate, Clock::time_point absolute) {
            do {
                first.update(); second.update();
                if (predicate()) return true;
                std::this_thread::sleep_for(5ms);
            } while (Clock::now() < absolute);
            return false;
        };
        auto join = [&](auto &runtime, std::shared_ptr<const Network::SessionPassword> password, const char *name) {
            require(runtime.join(endpoint, root.string(), {{name}}, std::move(password), {}, true), "real public runtime join start");
        };
        auto signal = [&](const char *name) { std::ofstream out(root / name); out << "ready\n"; };
        auto action = [&](const char *name, std::uint64_t session, unsigned binding = 1) {
            { std::ofstream out(root / "action.tmp"); out << binding << ' ' << name << ' ' << session; }
            std::filesystem::rename(root / "action.tmp", root / "action");
            require(pump([&] { return std::filesystem::exists(root / "acted"); }, Clock::now() + 5s), "actual relay action acknowledged");
            std::filesystem::remove(root / "acted");
        };
        auto watched = [&](bool wrong = false) {
            return PasswordBufferObservation::watch(wrong ? std::string_view("incorrect-private-fixture") : std::string_view(fixture->value()));
        };
        auto erased = [&](const auto &weak, Clock::time_point absolute) {
            require(pump([&] { return PasswordBufferObservation::released.load(); }, absolute), "connection Password allocation not released at owned terminal boundary");
            require(PasswordBufferObservation::erased && weak.expired(), "connection Password not zero before real allocator free");
            std::cout << "PASS known connection-owned Password zero-before-free " << scenario << " (bytes/addresses withheld)\n";
        };
        if (scenario == "pw-cancel" || scenario == "pw-terminal") {
            auto password = watched(scenario == "pw-terminal");
            std::weak_ptr<const Network::SessionPassword> weak(password);
            const auto absolute = Clock::now() + 12s;
            join(first, std::move(password), "Unadmitted");
            if (scenario == "pw-cancel") {
                require(pump([&] { return std::filesystem::exists(root / "outer-handshake-1") && Observed::handshakeCalls > 0; }, absolute), "real outer handshake and SDK entry before cancelled held inner handshake");
                first.cancel();
                require(pump([&] { return first.snapshot().journey == J::Inactive; }, absolute), "cancelled runtime terminal");
            } else {
                require(pump([&] { return first.snapshot().journey == J::Failure; }, absolute), "actual wrong-Password terminal failure");
                require(first.snapshot().failure == "Connection not authorized." && !first.snapshot().canonical
                        && first.snapshot().localParticipantId == 0, "terminal handshake failure allocated authority");
                require(Observed::fatalHandshakes > 0, "wrong-Password did not execute maintained fatal handshake");
            }
            erased(weak, absolute);
        } else if (scenario == "pw-shutdown") {
            auto password = watched(); std::weak_ptr<const Network::SessionPassword> weak(password);
            join(first, std::move(password), "Controller");
            require(pump([&] { return first.snapshot().journey == J::Lobby; }, Clock::now() + 12s), "shutdown actual admission");
            Observed::session = first.snapshot().canonical->sessionId;
            signal("shutdown-ready");
            const auto absolute = Clock::now() + 5s;
            require(pump([&] { return first.snapshot().journey == J::Failure; }, absolute), "actual service shutdown terminal");
            require(first.snapshot().failure == Network::PublicSession::Maintenance && Observed::notices > 0,
                    "shutdown must use actual authenticated maintenance notice");
            erased(weak, absolute);
        } else {
            Observed::capture = scenario == "scoped-resume";
            std::weak_ptr<const Network::SessionPassword> reservedPassword;
            if (scenario == "restart-same" || scenario == "restart-rotated") {
                auto password = watched(); reservedPassword = password;
                join(first, std::move(password), "Controller");
            } else join(first, fixture, "Controller");
            require(pump([&] { return first.snapshot().journey == J::Lobby && first.snapshot().host; }, Clock::now() + 12s), "original controller admission");
            const auto original = *first.snapshot().canonical;
            Observed::session = original.sessionId;
            if (scenario == "pw-leave" || scenario == "pw-end" || scenario == "pw-disposal") {
                auto password = watched(); std::weak_ptr<const Network::SessionPassword> weak(password);
                auto disposable = std::make_unique<Client::NetworkSessionRuntime>();
                auto &guest = scenario == "pw-disposal" ? *disposable : second;
                join(guest, std::move(password), "Owned guest");
                require(pump([&] { guest.update(); return guest.snapshot().journey == J::Lobby
                        && first.snapshot().canonical->participants.size() == 2; }, Clock::now() + 12s), "owned guest actual admission");
                require(!PasswordBufferObservation::released && !weak.expired(), "active connection Password lost lifetime");
                const auto absolute = Clock::now() + 5s;
                if (scenario == "pw-disposal") disposable.reset();
                else if (scenario == "pw-leave") {
                    second.leave();
                    require(pump([&] { return second.snapshot().journey == J::Inactive; }, absolute), "final actual Leave boundary");
                } else {
                    first.endSession();
                    require(pump([&] { return second.snapshot().journey == J::HostEnded; }, absolute), "final actual signed End boundary");
                    require(Observed::ends > 0, "End must be received authenticated protocol proof");
                }
                erased(weak, absolute);
            } else {
                join(second, fixture, "Guest anchor");
                require(pump([&] { return second.snapshot().journey == J::Lobby
                        && first.snapshot().canonical->participants.size() == 2; }, Clock::now() + 12s), "anchor admission");
                if (scenario == "scoped-resume") {
                    const auto old = Observed::saved(true);
                    action("recovery", original.sessionId);
                    const auto absolute = Clock::now() + 12s;
                    require(pump([&] { return first.snapshot().journey == J::Reconnecting; }, absolute), "actual reservation/recovery begins");
                    require(pump([&] { return first.snapshot().journey == J::Lobby; }, absolute), "actual same-Password reservation restoration");
                    const auto current = Observed::saved(false);
                    require(current.reservationId != old.reservationId && current.credential.bytes != old.credential.bytes,
                            "real restoration must rotate reservation credential");
                    Network::SessionTransportDependencies dependencies;
                    dependencies.secureSession = dependencies.enforceNetworkSessionPolicy = true;
                    dependencies.password = fixture;
                    const auto content = Network::CompatibilityManifestBuilder(root.string()).build();
                    require(content.valid(), "actual scoped reconnect compatibility");
                    auto compatibility = Network::makeLocalAdmissionRequest(1, content.manifest, {"Scoped fixture"});
                    compatibility.expectedHostingKind = Network::HostingKind::Dedicated;
                    for (unsigned caseId = 0; caseId < 4; ++caseId) {
                        auto selected = caseId == 0 ? old : current;
                        if (caseId == 1) ++selected.sessionId;
                        if (caseId == 2) ++selected.participantId;
                        if (caseId == 3) ++selected.reservationId;
                        Network::TcpClient client(dependencies);
                        require(client.start({"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[4]))}) && client.waitForConnected(5s), "scoped real outer TLS connection");
                        auto connection = client.connection();
                        auto payload = L::serializeReconnectAttempt({{selected.sessionId, selected.participantId, selected.reservationId, selected.credential}, compatibility});
                        require(connection->sendSensitive(payload) == Network::SendResult::Accepted, "scoped real reconnect attempt queued");
                        L::eraseLifecycleCredentialPayload(payload);
                        std::optional<L::ReconnectResponse> response;
                        const auto beforeOffers = Observed::offers.load();
                        require(pump([&] {
                            Network::TransportFrame frame;
                            while (connection->receive(frame)) {
                                if (const auto parsed = L::deserializeReconnectResponse(frame.payload)) response = parsed;
                                require(!Network::Replication::deserializeReplicationFrame(frame.payload), "denied credential received canonical allocation");
                                L::eraseLifecycleCredentialPayload(frame.payload);
                            }
                            return response.has_value();
                        }, Clock::now() + 5s), "scoped actual authorization response");
                        require(response->sessionId == selected.sessionId && response->participantId == selected.participantId
                                && response->outcome == L::ReconnectOutcome::AuthorizationFailed && !response->nextGrant
                                && Observed::offers == beforeOffers, "replay/wrong scope must deny without fresh initial role/grant");
                        client.close();
                    }
                    std::cout << "PASS actual composed replay/wrong-scope authorization negatives; no canonical allocation\n";
                    unsigned binding = 0;
                    { std::ifstream observed(root / "outer-latest-binding"); observed >> binding; }
                    require(binding > 1, "actual restored outer binding observation missing");
                    action("recovery", original.sessionId, binding);
                    const auto restoreDeadline = Clock::now() + 12s;
                    require(pump([&] { return first.snapshot().journey == J::Reconnecting; }, restoreDeadline), "post-negative legitimate reservation begins");
                    require(pump([&] { return first.snapshot().journey == J::Lobby; }, restoreDeadline), "wrong scopes must not consume actual legitimate credential");
                    require(first.snapshot().host && first.snapshot().localParticipantId == original.hostParticipantId
                            && first.snapshot().canonical->sessionId == original.sessionId
                            && first.snapshot().canonical->participants.size() == 2, "scoped negatives changed retained identity/authority");
                    std::cout << "PASS actual composed resume rotates credential; replay/wrong session/participant/reservation deny; legitimate credential still restores\n";
                    first.endSession();
                } else {
                    require(scenario == "restart-same" || scenario == "restart-rotated", "unknown crypto coverage case");
                    second.reset(); // No competing retained client in the restart sample.
                    const auto beforeOffers = Observed::offers.load(), beforeEnds = Observed::ends.load();
                    const auto beforeNotices = Observed::notices.load(), beforeFatal = Observed::fatalHandshakes.load();
                    require(!PasswordBufferObservation::released && !reservedPassword.expired(), "original connection must retain Password until final owned boundary");
                    signal("restart-ready");
                    const auto absolute = Clock::now() + 12s;
                    require(pump([&] { return first.snapshot().journey == J::Reconnecting; }, absolute), "actual unconfirmed kill begins reserved recovery");
                    signal("restart-reconnecting");
                    if (scenario == "restart-same") {
                        require(pump([&] { return first.snapshot().journey == J::Failure; }, absolute), "same-Password old grant definitive authorization denial");
                        require(first.snapshot().failure == Network::Trust::ReconnectAuthorizationFailureCopy
                                && Observed::authDenials > 0, "same-Password old grant must recognize F02, not retry/initial admission");
                        erased(reservedPassword, absolute);
                        std::cout << "PASS same Password restart: real scoped F02 authorization response; no new initial role\n";
                    } else {
                        require(pump([&] { return Observed::fatalHandshakes > beforeFatal; }, absolute), "rotated old retained Password actual maintained handshake failure");
                        require(first.snapshot().journey == J::Reconnecting && first.snapshot().canonical
                                && first.snapshot().canonical->sessionId == original.sessionId
                                && first.snapshot().failure != Network::PublicSession::ControllerExpired
                                && first.snapshot().failure != Network::PublicSession::Maintenance,
                                "rotated Password failure fabricated terminal cause or fresh session");
                        require(!PasswordBufferObservation::released && !reservedPassword.expired(), "ambiguous rotated-Password reconnect must retain original Password until cancel/disposal");
                        std::cout << "PASS rotated Password restart: real SDK fatal handshake; no authenticated terminal/initial fallback\n";
                    }
                    require(Observed::offers == beforeOffers && Observed::ends == beforeEnds && Observed::notices == beforeNotices,
                            "old reserved reconnect must not invent signed End/expiry, fallback or new initial allocation");
                    first.reset();
                    if (scenario == "restart-rotated") erased(reservedPassword, absolute);
                    join(second, scenario == "restart-same" ? fixture : rotated, "Explicit fresh controller");
                    require(pump([&] { return second.snapshot().journey == J::Lobby; }, absolute), "explicit correct Password independently joins fresh initial session");
                    require(second.snapshot().host && second.snapshot().canonical->sessionId != original.sessionId
                            && second.snapshot().canonical->participants.size() == 1 && Observed::offers > beforeOffers,
                            "explicit fresh join must independently acquire new initial identity");
                    second.endSession();
                    std::cout << "PASS actual composed " << scenario << ": explicit correct Password independently allocates fresh initial session\n";
                }
            }
        }
        first.reset(); second.reset();
        std::cout << "PASS crypto coverage " << scenario << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL crypto coverage: " << error.what() << '\n';
        return 1;
    }
}
