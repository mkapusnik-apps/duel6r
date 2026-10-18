#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include "source/client/HostServiceSupervisor.h"
// Test-only injection into the real established guest writer, bypassing UI and
// sendHostAction authority suppression. No production diagnostic API is added.
#define private public
#include "source/client/NetworkSessionRuntime.h"
#undef private
#include "source/network/PublicSession.h"
#include "source/network/SessionTransport.h"
#include "source/network/CompatibilityManifest.h"

// Driven by PublicDedicatedProcessTests.py: no credentials in argv or output.
using namespace Duel6;
using namespace std::chrono_literals;
static void require(bool value, const std::string &message) {
    if (!value) throw std::runtime_error(message);
}
class TestControl final : public Control {
public:
    bool pressed = false;
    bool isPressed() const override { return pressed; }
};
static void failureContext(const std::filesystem::path &root, const char *event,
                           Client::NetworkSessionRuntime &first,
                           Client::NetworkSessionRuntime &second,
                           Client::NetworkSessionRuntime &observer) noexcept {
    try {
        std::ofstream file(root / "client-failure-context");
        std::ostringstream receipt;
        // Fixed local peer aliases, not session identities. An absent marker
        // leaves its boundary unconfirmed; it never establishes End from EOF.
        receipt << "client-receipt version=1 event=" << event
                << " action-present=" << std::filesystem::exists(root / "action")
                << " ack-present=" << std::filesystem::exists(root / "acted")
                << " attack-present=" << std::filesystem::exists(root / "attack.bin")
                << " rejected-present=" << std::filesystem::exists(root / "attack-rejected")
                 << " peer-Leave-forwarded-1-present=" << std::filesystem::exists(root / "peer-Leave-forwarded-1")
                 << " peer-End-received-2-present=" << std::filesystem::exists(root / "peer-End-received-2")
                 << " peer-End-forwarded-2-present=" << std::filesystem::exists(root / "end-forwarded-2")
                 << " summary-write-failed-present=" << std::filesystem::exists(root / "summary-write-failed")
                << " peer-End-received-3-present=" << std::filesystem::exists(root / "peer-End-received-3")
                << " peer-End-forwarded-3-present=" << std::filesystem::exists(root / "end-forwarded-3") << '\n';
        const auto describe = [&](const char *alias, Client::NetworkSessionRuntime &runtime) {
            std::lock_guard<std::mutex> lock(runtime.mutex);
            const auto &state = runtime.current;
            int connection = -1;
            if (state.canonical) for (const auto &participant : state.canonical->participants)
                if (participant.participantId == state.localParticipantId)
                    connection = static_cast<int>(participant.connection);
            receipt << "runtime alias=" << alias << " journey=" << static_cast<int>(state.journey)
                    << " phase=" << (state.canonical ? static_cast<int>(state.canonical->phase) : -1)
                    << " round=" << (state.canonical ? unsigned(state.canonical->currentRoundNumber) : 0)
                    << " countdown=" << (state.canonical ? state.canonical->roundEndCountdown : 0)
                    << " replica-connection=" << connection << " failure-present=" << !state.failure.empty()
                    << " security-failure=" << state.securityFailure << " cancelled=" << runtime.guestCancelled.load()
                    << " guest-commands=" << runtime.pendingGuestCommands.size()
                    << " host-commands=" << runtime.pendingHostCommands.size()
                    << " host-end-pending=" << runtime.pendingHostEnd << '\n';
        };
        describe("controller", first); describe("guest", second); describe("observer", observer);
        file << receipt.str(); file.flush();
        std::cerr << receipt.str(); // Before require throws and runtime destructors run.
    } catch (...) {} // Preserve the original failing assertion even if receipt I/O fails.
}
int main(int argc, char **argv) {
    try {
        require(argc == 4, "expected port, resource root, scenario");
        auto *jump = new TestControl;
        PlayerControls controls("test controller", new TestControl, new TestControl, jump,
                                new TestControl, new TestControl, new TestControl, new TestControl);
        Client::NetworkSessionRuntime first, second, observer;
        std::string input;
        require(static_cast<bool>(std::getline(std::cin, input)), "password fixture input required");
        auto configured = std::make_shared<Network::SessionPassword>(input);
        Network::Trust::secureEraseMemory(input.data(), input.size()); input.clear();
        auto password = [&] { return configured; };
        Network::Endpoint endpoint{"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1]))};
        auto pumpUntil = [&](auto predicate, auto deadline) {
            do {
                first.update(); second.update(); observer.update();
                if (predicate()) return true;
                std::this_thread::sleep_for(5ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };
        auto pump = [&](auto predicate, auto timeout) {
            return pumpUntil(predicate, std::chrono::steady_clock::now() + timeout);
        };
        using J = Client::NetworkJourney;
        auto peerSummaryStatus = [&] {
            const auto controller = first.snapshot(), guest = second.snapshot();
            auto describe = [](const auto &state) {
                return "journey=" + std::to_string(static_cast<int>(state.journey))
                    + " phase=" + std::to_string(state.canonical ? static_cast<int>(state.canonical->phase) : -1)
                    + " result=" + std::to_string(state.canonical && state.canonical->result.available);
            };
            return "controller " + describe(controller) + "; guest " + describe(guest);
        };
        const std::string scenario = argv[3];
        if (scenario != "directory" && scenario != "directory-reservation")
            require(first.join(endpoint, argv[2], {{"Controller", &controls}}, password()), "unbound common Direct join start");
        if (scenario == "directory") {
            require(std::getenv("D6R_DIRECTORY_URL") != nullptr, "owned directory fixture required");
            Client::DirectoryPage page;
            auto lastRead = std::chrono::steady_clock::time_point{};
            auto refresh = [&] {
                if (std::chrono::steady_clock::now() - lastRead >= 250ms) {
                    page = Client::directoryPage({}); lastRead = std::chrono::steady_clock::now();
                }
            };
            std::optional<Client::DirectoryListing> selected;
            require(pump([&] {
                refresh();
                for (const auto &row: page.listings)
                    if (row.dedicated() && row.endpoint.port == endpoint.port && row.phase == "idle") selected = row;
                return selected.has_value();
            }, 10s), "prepared empty directory session before first admission");
            require(selected->players == 0 && selected->sessionId != std::string(32, '0') && selected->joinable(), "idle listing metadata");
            endpoint = selected->endpoint;
            const auto original = selected->sessionId;
            const auto legacy = Client::directoryRequest("GET", "/v1/listings");
            require(legacy.status == 200 && Client::decodeDirectoryPage(legacy.body).available, "legacy read");
            for (const auto &row: Client::decodeDirectoryPage(legacy.body).listings)
                require(row.endpoint.port != endpoint.port, "legacy disclosed dedicated endpoint");
            require(observer.join(endpoint, argv[2], {{"Wrong selection"}}, password(), Client::directorySessionId(1), true), "wrong selected attempt starts securely");
            require(pump([&] { return observer.snapshot().journey == J::Failure; }, 12s), "wrong selected outcome");
            require(observer.snapshot().selectedSessionChanged && observer.snapshot().localParticipantId == 0
                && !observer.snapshot().canonical, "wrong selection allocated authority");
            observer.reset();
            require(first.join(endpoint, argv[2], {{"Controller", &controls}}, password(), original, true), "bound first join start");
            require(second.join(endpoint, argv[2], {{"Guest"}}, password(), original, true), "concurrent bound join start");
            require(pump([&] { return first.snapshot().journey == J::Lobby && second.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 2; }, 12s), "concurrent bound commits");
            require(first.snapshot().host != second.snapshot().host, "exactly one committed controller");
            auto &controller = first.snapshot().host ? first : second;
            auto &guest = first.snapshot().host ? second : first;
            const auto session = controller.snapshot().canonical->sessionId;
            require(Client::directorySessionId(session) == original && guest.snapshot().canonical->sessionId == session, "first commit changed advertised identity");
            require(pump([&] { refresh(); return std::any_of(page.listings.begin(), page.listings.end(), [&](const auto &row) {
                return row.sessionId == original && row.players == 2 && row.phase == "lobby"; }); }, 5s), "authoritative lobby publication");
            Network::HostComposition::Setup setup;
            setup.localPlayerNames = {controller.snapshot().host && &controller == &first ? "Controller" : "Guest"};
            setup.fixedLevel = "levels/arena.json"; setup.roundLimit = 1; setup.quickLiquid = true;
            setup.assistance = !controller.snapshot().canonical->settings.assistance;
            require(controller.updateHostSetup(setup), "bound controller setup");
            require(pump([&] { return first.snapshot().canonical->settings.quickLiquid && second.snapshot().canonical->settings.quickLiquid
                && first.snapshot().canonical->settings.assistance == setup.assistance
                && second.snapshot().canonical->settings.assistance == setup.assistance; }, 5s), "setup propagated");
            first.setReady(true); second.setReady(true);
            require(pump([&] { const auto state = controller.snapshot();
                return std::all_of(state.canonical->participants.begin(), state.canonical->participants.end(),
                    [](const auto &p) { return p.ready; }); }, 5s), "bound readiness");
            controller.startMatch();
            require(pump([&] { refresh(); return first.snapshot().journey == J::Summary && second.snapshot().journey == J::Summary; }, 90s), "bound real completed match");
            require(controller.snapshot().canonical->sessionId == session, "results changed identity");
            controller.returnToLobby();
            require(pump([&] { refresh(); return first.snapshot().journey == J::Lobby && second.snapshot().journey == J::Lobby; }, 5s), "bound same-session return");
            require(controller.snapshot().canonical->sessionId == session, "Return changed identity");
            controller.endSession();
            require(pump([&] { return guest.snapshot().journey == J::HostEnded; }, 5s), "bound accepted End");
            first.reset(); second.reset();
            std::optional<Client::DirectoryListing> fresh;
            require(pump([&] { refresh(); for (const auto &row: page.listings)
                if (row.dedicated() && row.endpoint.port == endpoint.port && row.phase == "idle" && row.sessionId != original) fresh = row;
                return fresh.has_value(); }, 10s), "fresh distinct empty publication after cleanup");
            require(fresh->players == 0, "fresh session manufactured players");
            require(observer.join(endpoint, argv[2], {{"Old selection"}}, password(), original, true), "old selection starts");
            require(pump([&] { return observer.snapshot().journey == J::Failure; }, 12s), "old selection failure");
            require(observer.snapshot().selectedSessionChanged && observer.snapshot().localParticipantId == 0, "old row redirected fresh authority");
            observer.reset();
            require(observer.join(endpoint, argv[2], {{"Fresh controller"}}, password(), fresh->sessionId, true), "explicit refreshed selection");
            require(pump([&] { return observer.snapshot().journey == J::Lobby; }, 12s), "fresh bound admission");
            require(observer.snapshot().host && Client::directorySessionId(observer.snapshot().canonical->sessionId) == fresh->sessionId, "fresh role/identity");
            observer.endSession();
            std::cout << "PASS real directory browse/TLS bound idle admission, concurrent roles, completed Return, End and old/fresh selection isolation\n";
        } else if (scenario == "directory-reservation") {
            Client::DirectoryPage page;
            auto lastRead = std::chrono::steady_clock::time_point{};
            auto refresh = [&] {
                if (std::chrono::steady_clock::now() - lastRead >= 250ms) {
                    page = Client::directoryPage({}); lastRead = std::chrono::steady_clock::now();
                }
            };
            std::optional<Client::DirectoryListing> selected;
            require(pump([&] { refresh(); for (const auto &row: page.listings)
                if (row.dedicated() && row.endpoint.port == endpoint.port && row.phase == "idle") selected = row;
                return selected.has_value(); }, 10s), "R1 real prepared browser selection");
            endpoint = selected->endpoint;
            require(first.join(endpoint, argv[2], {{"Controller", &controls}}, password(), selected->sessionId, true), "R1 bound controller start");
            require(pump([&] { return first.snapshot().journey == J::Lobby && first.snapshot().host; }, 12s), "R1 controller committed");
            const auto original = *first.snapshot().canonical;
            const auto content = Network::CompatibilityManifestBuilder(argv[2]).build();
            require(content.valid(), "R1 guest content");
            Network::SessionTransportDependencies transport;
            transport.secureSession = true; transport.password = password(); transport.enforceNetworkSessionPolicy = true;
            Network::TcpClient offeredGuest(transport);
            require(offeredGuest.start(endpoint) && offeredGuest.waitForConnected(5s), "R1 actual guest TLS");
            auto connection = offeredGuest.connection(); require(connection != nullptr, "R1 actual guest connection");
            auto request = Network::makeLocalAdmissionRequest(1, content.manifest, {"Deferred guest"});
            request.expectedSessionId = original.sessionId;
            require(connection->send(Network::serializeAdmissionRequest(request)) == Network::SendResult::Accepted, "R1 request while controller connected");
            std::optional<Network::AdmissionOfferPayload> offer;
            require(pump([&] { Network::TransportFrame frame; if (connection->receive(frame))
                offer = Network::deserializeAdmissionOffer(frame.payload); return offer.has_value(); }, 5s), "R1 genuine provisional offer");
            require(offer->sessionId == original.sessionId, "R1 bound provisional identity");
            const auto root = std::filesystem::path(argv[2]);
            auto action = [&](const char *name) {
                { std::ofstream out(root / "action.tmp"); out << 1 << ' ' << name << ' ' << original.sessionId; }
                std::filesystem::rename(root / "action.tmp", root / "action");
                require(pump([&] { return std::filesystem::exists(root / "acted"); }, 5s), "R1 fixture action acknowledged");
                std::filesystem::remove(root / "acted");
            };
            action("eof"); // Existing real transport-loss path; no terminal notice is injected.
            require(pump([&] { refresh(); return first.snapshot().journey == J::Reconnecting
                && std::any_of(page.listings.begin(), page.listings.end(), [&](const auto &row) {
                    return row.sessionId == selected->sessionId && row.phase == "reserved" && row.admissionState() == "closed" && row.players == 1;
                }); }, 5s), "R1 authoritative Reserved/Closed before acceptance");
            const auto countdown = first.snapshot().reconnectSeconds;
            require(connection->send(Network::serializeAdmissionAcceptance(*offer)) == Network::SendResult::Accepted, "R1 acceptance after reservation");
            std::optional<Network::AdmissionResult> rejection;
            require(pump([&] { Network::TransportFrame frame; if (connection->receive(frame))
                rejection = Network::deserializeAdmissionResult(frame.payload); return rejection.has_value(); }, 5s), "R1 late offer fixed rejection, not authority");
            require(rejection->code == Network::AdmissionResultCode::HostPolicyRejected
                && rejection->participantId == 0 && rejection->playerIds.empty(), "R1 no committed identity in rejection");
            require(first.snapshot().reconnectSeconds && countdown && *first.snapshot().reconnectSeconds <= *countdown, "R1 no reservation countdown restart");
            action("resume");
            require(pump([&] { return first.snapshot().journey == J::Lobby && first.snapshot().host; }, 12s), "R1 original credential restores controller");
            const auto restored = *first.snapshot().canonical;
            require(restored.sessionId == original.sessionId && restored.hostParticipantId == original.hostParticipantId
                && restored.participants.size() == 1 && restored.players.size() == 1
                && restored.players.front().playerId == original.players.front().playerId, "R1 no ghost guest/player or controller migration");
            require(pump([&] { refresh(); return std::any_of(page.listings.begin(), page.listings.end(), [&](const auto &row) {
                return row.sessionId == selected->sessionId && row.phase == "lobby" && row.players == 1;
            }); }, 5s), "R1 publication resumes same identity and retained count");
            offeredGuest.close(); first.endSession();
            require(pump([&] { refresh(); return std::any_of(page.listings.begin(), page.listings.end(), [&](const auto &row) {
                return row.endpoint.port == endpoint.port && row.phase == "idle" && row.players == 0 && row.sessionId != selected->sessionId;
            }); }, 10s), "R1 accepted End cleans before fresh empty identity");
            std::cout << "PASS R1 actual TLS/directory offered->Reserved/Closed->late acceptance rejected; original controller/identity/credential retained\n";
        } else if (scenario.compare(0, 7, "attack-") == 0) {
            namespace H = Network::HostComposition;
            namespace R = Network::Replication;
            require(pump([&] { return first.snapshot().journey == J::Lobby; }, 12s), "attack controller admission");
            require(second.join(endpoint, argv[2], {{"Attacker"}}, password(), {}, true), "attacker join start");
            require(pump([&] { return second.snapshot().journey == J::Lobby; }, 12s), "attacker admission");
            require(observer.join(endpoint, argv[2], {{"Observer"}}, password(), {}, true), "observer join start");
            require(pump([&] { return observer.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 3; }, 12s), "observer admission");
            const bool summary = scenario == "attack-return";
            const bool advance = scenario == "attack-advance";
            H::Setup setup;
            setup.localPlayerNames = {"Controller"}; setup.fixedLevel = "levels/arena.json";
            setup.quickLiquid = summary || advance; setup.roundLimit = advance ? 2 : 1;
            setup.assistance = !first.snapshot().canonical->settings.assistance;
            require(first.updateHostSetup(setup), "legitimate setup");
            require(pump([&] {
                for (auto *runtime : {&first, &second, &observer}) {
                    const auto s = runtime->snapshot();
                    if (!s.canonical || s.canonical->settings.assistance != setup.assistance
                        || s.canonical->settings.quickLiquid != setup.quickLiquid
                        || s.canonical->settings.roundLimit != setup.roundLimit) return false;
                }
                return true;
            }, 5s), "setup synchronization");
            first.setReady(true); second.setReady(true); observer.setReady(true);
            require(pump([&] { const auto s = first.snapshot(); return std::all_of(s.canonical->participants.begin(),
                s.canonical->participants.end(), [](const auto &p) { return p.ready; }); }, 5s), "applicable ready lobby");
            if (summary || advance) {
                first.startMatch();
                // Each real TLS participant receives replication independently.
                // Do not take the controller's baseline merely because the
                // observer reached FinalSummary: the controller may still hold
                // RoundSummary, whose legitimate transition is not an attack.
                const auto applicablePhase = summary ? R::Phase::FinalSummary : R::Phase::RoundSummary;
                require(pump([&] {
                    const auto reference = first.snapshot();
                    if (!reference.canonical || reference.canonical->phase != applicablePhase) return false;
                    for (auto *runtime : {&first, &second, &observer}) {
                        const auto s = runtime->snapshot();
                        if (!s.canonical || s.canonical->phase != applicablePhase
                            || s.canonical->sessionId != reference.canonical->sessionId
                            || s.canonical->matchId != reference.canonical->matchId
                            || s.canonical->currentRoundNumber != reference.canonical->currentRoundNumber
                            || (summary && !s.canonical->result.available)) return false;
                        // Keep the bounded rejection observation clear of the
                        // natural six-second round transition in AdvanceRound.
                        if (advance && (s.canonical->currentRoundNumber != 1
                            || s.canonical->roundEndCountdown <= 180)) return false;
                    }
                    return true;
                }, 90s), "all participants synchronized in applicable control phase");
            }
            const auto before = *first.snapshot().canonical;
            auto fingerprint = [](const R::CanonicalState &s) {
                std::ostringstream out;
                const auto &v = s.settings;
                out << s.sessionId << ':' << s.hostParticipantId << ':' << v.mode << ':' << unsigned(v.teamCount)
                    << ':' << v.friendlyFire << ':' << v.levelPlan << ':' << v.fixedLevel << ':' << unsigned(v.roundLimit)
                    << ':' << v.assistance << ':' << v.quickLiquid << ':' << v.burnableTrees;
                for (const auto &p : s.players) out << ':' << p.playerId << ':' << p.ownerParticipantId << ':' << unsigned(p.rosterPosition);
                return out.str();
            };
            std::vector<std::uint8_t> attack;
            if (scenario == "attack-start") attack = H::serializeAction(H::Kind::StartMatch);
            else if (scenario == "attack-return") attack = H::serializeAction(H::Kind::ReturnToLobby);
            else if (scenario == "attack-advance") attack = H::serializeAction(H::Kind::AdvanceRound);
            else if (scenario == "attack-roster") attack = H::serializeRosterMove(before.players.front().playerId, 1);
            else if (scenario == "attack-setup") { setup.roundLimit = 9; attack = H::serializeSetupUpdate(setup); }
            else throw std::runtime_error("unknown attack case");
            require(!attack.empty(), "empty attack");
            const auto root = std::filesystem::path(argv[2]);
            { std::ofstream file(root / "attack.bin", std::ios::binary); file.write(reinterpret_cast<const char *>(attack.data()), attack.size()); }
            {
                std::lock_guard<std::mutex> lock(second.mutex);
                second.pendingGuestCommands.push_back(attack);
            }
            const bool rejected = pump([&] { return std::filesystem::exists(root / "attack-rejected"); }, 3s);
            if (!rejected) failureContext(root, "attack-marker", first, second, observer);
            require(rejected, "exact malicious TLS frame and server-side close not observed");
            const auto after = first.snapshot();
            require(after.host && !second.snapshot().host && !observer.snapshot().host, "attack changed authority");
            require(after.canonical && fingerprint(*after.canonical) == fingerprint(before), "attack mutated settings or ownership/roster");
            require(after.canonical->phase == before.phase && after.canonical->currentRoundNumber == before.currentRoundNumber,
                    "attack changed applicable phase/round");
            require(observer.snapshot().journey != J::Reconnecting && observer.snapshot().journey != J::Failure,
                    "unaffected observer interrupted");
            if (summary) require(after.canonical->result.serialized == before.result.serialized, "attack replaced completed result");
            // Real controller authority and the uninvolved participant's stream
            // remain usable after rejection of the authenticated attacker.
            // Arm the observer relay before End: a competing ordinary reverse
            // write must not hide the service's actual authenticated notice.
            // Coordination consumes the original total five-second End budget.
            const auto endDeadline = std::chrono::steady_clock::now() + 5s;
            { std::ofstream arm(root / "attack-end-arm"); }
            require(pumpUntil([&] { return std::filesystem::exists(root / "attack-end-armed"); }, endDeadline),
                    "authorization observer terminal relay armed");
            first.endSession();
            const bool ended = pumpUntil([&] { return observer.snapshot().journey == J::HostEnded; }, endDeadline);
            if (!ended) failureContext(root, "attack-end-outcome", first, second, observer);
            require(ended, "unaffected participant did not receive legitimate end");
            std::cout << "PASS exact malicious frame observed over TLS; server rejected; authority/state retained\n";
        } else if (scenario.compare(0, 7, "notice-") == 0 || scenario == "recovery" || scenario == "expiry"
                   || scenario == "summary-return" || scenario == "summary-return-ordered") {
            require(pump([&] { return first.snapshot().journey == J::Lobby; }, 12s), "review first admission");
            const auto session = first.snapshot().canonical->sessionId;
            const auto controller = first.snapshot().localParticipantId;
            require(second.join(endpoint, argv[2], {{"Guest"}}, password(), {}, true), "review guest start");
            require(pump([&] { return second.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 2; }, 12s), "review guest admission");
            const bool match = scenario.find("-match") != std::string::npos;
            const bool orderedSummary = scenario == "summary-return-ordered";
            const bool summaryReturn = scenario == "summary-return" || orderedSummary;
            const bool summary = scenario.find("-summary") != std::string::npos || summaryReturn;
            bool orderedIntermediateObserved = false;
            if (match || summary) {
                Network::HostComposition::Setup setup;
                setup.localPlayerNames = {"Controller"};
                setup.fixedLevel = "levels/arena.json";
                setup.roundLimit = 1; setup.quickLiquid = summary;
                // The lobby can already have this round limit/Quick Liquid
                // value. Require a distinguishable real update to reach BOTH
                // clients before sending Ready on their independent streams;
                // otherwise a later SetupUpdate legitimately clears guest Ready.
                setup.assistance = !first.snapshot().canonical->settings.assistance;
                require(first.updateHostSetup(setup), "review setup enqueue");
                require(pump([&] {
                    for (auto *runtime : {&first, &second}) {
                        const auto s = runtime->snapshot();
                        if (!s.canonical || s.canonical->settings.assistance != setup.assistance
                            || s.canonical->settings.quickLiquid != setup.quickLiquid
                            || s.canonical->settings.roundLimit != setup.roundLimit
                            || s.canonical->settings.fixedLevel != setup.fixedLevel
                            || s.canonical->settings.levelPlan != setup.levelPlan
                            || s.canonical->settings.mode != setup.mode) return false;
                    }
                    return true;
                }, 5s), "review setup propagation to both clients");
                first.setReady(true); second.setReady(true);
                require(pump([&] {
                    for (auto *runtime : {&first, &second}) {
                        const auto s = runtime->snapshot();
                        if (!s.canonical || s.canonical->participants.size() != 2
                            || !std::all_of(s.canonical->participants.begin(), s.canonical->participants.end(),
                                           [](const auto &p) { return p.ready; })) return false;
                    }
                    return true;
                }, 5s), "review readiness");
                first.startMatch();
                require(pump([&] { return second.snapshot().journey == J::Match; }, 10s), "review active match");
                if (summary) {
                    // One original 90s deadline includes guest publication,
                    // ordered release and controller observation; never restart.
                    const auto deadline = std::chrono::steady_clock::now() + 90s;
                    const bool observed = pumpUntil([&] {
                        if (!summaryReturn) return second.snapshot().journey == J::Summary;
                        const auto controllerState = first.snapshot(), guestState = second.snapshot();
                        const auto complete = [session](const auto &state) {
                            return state.journey == J::Summary && state.canonical
                                && state.canonical->phase == Network::Replication::Phase::FinalSummary
                                && state.canonical->sessionId == session && state.canonical->result.available;
                        };
                        if (orderedSummary && !orderedIntermediateObserved
                            && complete(guestState) && !complete(controllerState)) {
                            require(controllerState.canonical && !controllerState.canonical->result.available,
                                    "ordered summary did not select missing controller result: " + peerSummaryStatus());
                            std::cout << "PASS ordered guest summary precedes controller result: " << peerSummaryStatus() << '\n';
                            { std::ofstream release(std::filesystem::path(argv[2]) / "summary-release"); }
                            orderedIntermediateObserved = true;
                        }
                        return complete(controllerState) && complete(guestState);
                    }, deadline);
                    require(observed, "review final summary prerequisite: " + peerSummaryStatus());
                    if (orderedSummary) require(orderedIntermediateObserved, "ordered summary prerequisite was not exercised");
                }
            }
            const auto phase = second.snapshot().journey;
            const auto root = std::filesystem::path(argv[2]);
            auto request = [&](const std::string &action, unsigned target) {
                { std::ofstream out(root / "action.tmp"); out << target << ' ' << action << ' ' << session; }
                std::filesystem::rename(root / "action.tmp", root / "action");
                const bool acknowledged = pump([&] { return std::filesystem::exists(root / "acted"); }, 5s);
                if (!acknowledged) failureContext(root, "action-marker", first, second, observer);
                require(acknowledged, "fixture action acknowledgement");
                std::filesystem::remove(root / "acted");
            };
            if (summaryReturn) {
                require(first.snapshot().canonical->result.available && second.snapshot().canonical->result.available,
                        "dedicated summary lost completed result: " + peerSummaryStatus());
                const auto completedResult = first.snapshot().canonical->result.serialized;
                require(second.snapshot().canonical->result.serialized == completedResult,
                        "dedicated peers received different completed results: " + peerSummaryStatus());
                first.returnToLobby();
                const bool returned = pump([&] { return first.snapshot().journey == J::Lobby
                    && second.snapshot().journey == J::Lobby; }, 5s);
                require(returned, "dedicated same-session return: " + peerSummaryStatus());
                for (auto *runtime : {&first, &second}) {
                    const auto state = runtime->snapshot();
                    require(state.canonical->sessionId == session && state.canonical->result.available,
                            "dedicated return replaced session or result: " + peerSummaryStatus());
                    require(state.canonical->result.serialized == completedResult,
                            "dedicated return changed completed result: " + peerSummaryStatus());
                    require(std::none_of(state.canonical->participants.begin(), state.canonical->participants.end(),
                                         [](const auto &p) { return p.ready; }), "dedicated return retained readiness");
                }
                // Keep the original total End observation budget, including relay
                // coordination. Only the ordered fixture has an authenticated
                // intermediary that could discard End on a competing write-fatal.
                const auto endDeadline = std::chrono::steady_clock::now() + 5s;
                if (orderedSummary) {
                    { std::ofstream arm(root / "summary-end-arm"); }
                    require(pumpUntil([&] { return std::filesystem::exists(root / "summary-end-armed"); },
                                      endDeadline), "ordered summary terminal relay armed");
                }
                first.endSession();
                const bool ended = pumpUntil([&] { return first.snapshot().journey == J::Inactive
                    && second.snapshot().journey == J::HostEnded; }, endDeadline);
                if (!ended) failureContext(root, "summary-return-end", first, second, observer);
                require(ended, "dedicated summary return end");
            } else if (scenario == "recovery") {
                request("recovery", 1);
                require(pump([&] { return first.snapshot().journey == J::Reconnecting; }, 5s), "controller did not enter recovery");
                require(pump([&] { return first.snapshot().journey == J::Lobby; }, 12s), "TLS controller did not restore");
                require(first.snapshot().host && first.snapshot().localParticipantId == controller
                    && first.snapshot().canonical->sessionId == session, "TLS recovery changed controller identity");
                require(!second.snapshot().host, "TLS recovery transferred authority");
                first.endSession();
            } else if (scenario == "expiry") {
                request("expiry", 1);
                require(pump([&] { return second.snapshot().journey == J::Failure; }, 36s), "controller expiry did not terminate established guest");
                require(second.snapshot().failure == Network::PublicSession::ControllerExpired, "controller expiry reason");
                request("resume", 2);
                first.reset(); second.reset();
                require(second.join(endpoint, argv[2], {{"Fresh"}}, password(), {}, true), "post-expiry fresh join start");
                require(pump([&] { return second.snapshot().journey == J::Lobby; }, 12s), "post-expiry fresh admission");
                require(second.snapshot().host && second.snapshot().canonical->sessionId != session, "post-expiry stale session restored");
                second.endSession();
            } else {
                const bool wrong = scenario.find("wrong") != std::string::npos;
                const bool eof = scenario.find("eof") != std::string::npos;
                const bool maintenance = scenario.find("maintenance") != std::string::npos;
                request(eof ? "eof" : wrong ? "wrong" : maintenance ? "maintenance" : "controller", 2);
                if (wrong || eof) {
                    if (eof) require(pump([&] { return second.snapshot().journey == J::Reconnecting; }, 5s), "EOF did not remain ambiguous recovery");
                    pump([] { return false; }, 500ms);
                    const auto state = second.snapshot();
                    require(state.failure != Network::PublicSession::Maintenance
                        && state.failure != Network::PublicSession::ControllerExpired, "unconfirmed terminal reason inferred");
                    require(state.journey == phase || state.journey == J::Reconnecting, "wrong-session/EOF selected terminal outcome");
                    if (eof) request("observed-eof", 2);
                } else {
                    require(pump([&] { return second.snapshot().journey == J::Failure; }, 5s), "established TLS notice did not select terminal failure");
                    const auto state = second.snapshot();
                    require(state.failure == (maintenance ? Network::PublicSession::Maintenance : Network::PublicSession::ControllerExpired), "established TLS terminal reason mismatch");
                    require(!state.reconnectSeconds && !state.retryAllowed, "terminal notice retained recovery/retry");
                }
            }
        } else if (scenario == "concurrent") {
            require(second.join(endpoint, argv[2], {{"Contender"}}, password(), {}, true), "concurrent join start");
            require(pump([&] { return first.snapshot().journey == J::Lobby
                && second.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 2
                && second.snapshot().canonical->participants.size() == 2; }, 12s), "concurrent admission");
            const auto a = first.snapshot(), b = second.snapshot();
            require(a.host != b.host, "concurrent admission did not elect exactly one controller");
            require(a.canonical->sessionId == b.canonical->sessionId, "concurrent joins created separate sessions");
            require(std::count_if(a.canonical->participants.begin(), a.canonical->participants.end(),
                    [](const auto &p) { return p.host; }) == 1, "canonical controller cardinality");
            (a.host ? first : second).endSession();
            require(pump([&] { return (a.host ? first : second).snapshot().journey == J::Inactive
                && (a.host ? second : first).snapshot().journey == J::HostEnded; }, 5s), "concurrent session end");
        } else if (scenario != "flow" && scenario != "flow-race" && scenario != "flow-fatal-before-end"
                   && scenario != "flow-reuse") {
            require(pump([&] { return first.snapshot().journey == J::Failure; }, 12s), "failure deadline");
            const auto state = first.snapshot();
            require(!state.host && !state.localParticipantId && !state.canonical, "rejection granted authority");
            if (scenario == "legacy-profile") {
                require(state.failure == "Connection not authorized."
                        || state.failure == "Connection ended before admission completed."
                        || state.failure == "Connection timed out."
                        || state.failure == Network::PublicSession::SecurityFailure, "unconfirmed legacy profile outcome");
            } else {
                require(state.failure == "Connection not authorized.", "missing authorization classification");
                require(state.failure == "Connection not authorized.", "authorization failure copy");
            }
        } else {
            if (!pump([&] { return first.snapshot().journey == J::Lobby; }, 12s))
                throw std::runtime_error("first admission: " + first.snapshot().failure
                    + " journey=" + std::to_string(static_cast<int>(first.snapshot().journey)));
            require(first.snapshot().host && first.snapshot().publicSession, "first participant not controller");
            const auto controller = first.snapshot().localParticipantId;
            const auto session = first.snapshot().canonical->sessionId;
            if (scenario == "flow-reuse") {
                const auto initial = *first.snapshot().canonical;
                require(initial.phase == Network::Replication::Phase::Lobby && initial.participants.size() == 1
                    && initial.players.size() == 1 && initial.hostParticipantId == controller
                    && !initial.round && initial.currentRoundNumber == 0 && !initial.result.available,
                    "initial ready-empty service did not admit exactly its first controller/player");
                std::cout << "PASS actual initial controller and clean authoritative lobby state\n";
            }
            require(second.join(endpoint, argv[2], {{"Guest"}}, password(), {}, true), "guest join start");
            require(pump([&] { return second.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 2; }, 12s), "guest admission");
            require(!second.snapshot().host, "guest became controller");
            require(second.snapshot().canonical->sessionId == session, "multiple sessions");
            second.startMatch(); second.returnToLobby();
            pump([] { return false; }, 200ms);
            require(first.snapshot().journey == J::Lobby && second.snapshot().journey == J::Lobby,
                    "guest control changed phase");
            first.startMatch();
            pump([] { return false; }, 200ms);
            require(first.snapshot().journey == J::Lobby, "unready match started");
            first.setReady(true); second.setReady(true);
            require(pump([&] { const auto s = first.snapshot(); return s.canonical &&
                std::all_of(s.canonical->participants.begin(), s.canonical->participants.end(),
                            [](const auto &p) { return p.ready; }); }, 5s), "ready propagation");
            first.startMatch();
            require(pump([&] { return first.snapshot().journey == J::Match
                && second.snapshot().journey == J::Match; }, 10s), "encrypted gameplay start");
            const auto tick = first.snapshot().canonical->phaseTime;
            require(pump([&] { return first.snapshot().canonical->phaseTime > tick + 10
                && second.snapshot().canonical->phaseTime > tick; }, 5s), "authoritative gameplay did not advance");
            require(first.snapshot().localParticipantId == controller, "controller identity changed");
            const auto originalPlayers = first.snapshot().canonical->players;
            require(observer.join(endpoint, argv[2], {{"Round one arrival", &controls}}, password(), {}, true),
                    "dedicated live arrival start");
            require(pump([&] { return observer.snapshot().journey == J::Match
                && first.snapshot().canonical->participants.size() == 3
                && second.snapshot().canonical->participants.size() == 3; }, 10s), "dedicated live arrival did not enter arena");
            require(!observer.snapshot().host && observer.snapshot().canonical->sessionId == session,
                    "dedicated live arrival granted controller or new session");
            for (const auto &original : originalPlayers) {
                const auto players = first.snapshot().canonical->players;
                require(std::any_of(players.begin(), players.end(), [&](const auto &p) {
                    return p.playerId == original.playerId && p.ownerParticipantId == original.ownerParticipantId;
                }), "dedicated live arrival replaced existing identity");
            }
            auto controllerY = [&] {
                const auto state = second.snapshot();
                for (const auto &player : state.canonical->players)
                    if (player.ownerParticipantId == controller) return player.positionY;
                throw std::runtime_error("controller player missing");
            };
            pump([] { return false; }, 500ms);
            const auto beforeJump = controllerY();
            jump->pressed = true;
            require(pump([&] { return controllerY() > beforeJump; }, 3s), "sampled controller jump not replicated over TLS");
            jump->pressed = false;
            if (scenario == "flow-reuse")
                std::cout << "PASS actual Start, advancing authoritative gameplay and sampled input replication\n";
            const auto endDeadline = std::chrono::steady_clock::now() + 5s;
            if (scenario == "flow-race" || scenario == "flow-fatal-before-end") {
                { std::ofstream arm(std::filesystem::path(argv[2]) / "flow-arm"); }
                require(pumpUntil([&] { return std::filesystem::exists(std::filesystem::path(argv[2]) / "flow-armed"); }, endDeadline),
                        "missing bounded reverse-traffic qualification");
                observer.setReady(true); observer.setReady(false);
                if (scenario == "flow-fatal-before-end") {
                    { std::ofstream trigger(std::filesystem::path(argv[2]) / "flow-trigger-fatal"); }
                    require(pumpUntil([&] { return std::filesystem::exists(std::filesystem::path(argv[2]) / "flow-fatal-before-End"); }, endDeadline),
                            "fatal-before-End qualification missing");
                }
            }
            first.endSession();
            if (scenario == "flow-fatal-before-end") {
                require(pumpUntil([&] { return first.snapshot().journey == J::Inactive
                    && second.snapshot().journey == J::HostEnded && observer.snapshot().journey == J::Failure; }, endDeadline),
                        "unreceived End must select scoped reconnect failure, not invented End");
                require(observer.snapshot().failure == Network::Trust::ReconnectAuthorizationFailureCopy,
                        "old-scope denial after unreceived End");
                require(std::filesystem::exists(std::filesystem::path(argv[2]) / "flow-negative-qualified"),
                        "negative ordering was not qualified");
                std::cout << "PASS fatal-before-End retains ambiguity and actual scoped denial\n";
                std::cout << "PASS public dedicated " << scenario << '\n';
                return 0;
            }
            const bool ended = pumpUntil([&] { return first.snapshot().journey == J::Inactive
                && second.snapshot().journey == J::HostEnded && observer.snapshot().journey == J::HostEnded; }, endDeadline);
            if (!ended) failureContext(std::filesystem::path(argv[2]), "flow-end-outcome", first, second, observer);
            require(ended,
                "confirmed end outcome: controller journey=" + std::to_string(static_cast<int>(first.snapshot().journey))
                + " guest journey=" + std::to_string(static_cast<int>(second.snapshot().journey))
                + " arrival journey=" + std::to_string(static_cast<int>(observer.snapshot().journey))
                + " guest failure=" + second.snapshot().failure
                + " arrival failure=" + observer.snapshot().failure);
            first.reset(); second.reset();
            if (scenario == "flow-reuse") {
                observer.reset();
                const auto root = std::filesystem::path(argv[2]);
                { std::ofstream marker(root / "reuse-ended"); }
                require(pump([&] { return std::filesystem::exists(root / "reuse-ready"); }, 5s),
                        "native production readiness probe after End missing");
            }
            // This proxy intentionally gives every participant one source IPv4.
            // The preceding denial plus three joins exhaust the four-attempt
            // burst. Preserve the production 20/minute refill policy: one new
            // attempt is eligible after three seconds, without resetting it.
            pump([] { return false; }, 3100ms);
            if (scenario == "flow-reuse") {
                require(observer.join(endpoint, argv[2], {{"Ended selection"}}, password(),
                                      Client::directorySessionId(session), true), "old bound selection starts");
                require(pump([&] { return observer.snapshot().journey == J::Failure; }, 12s),
                        "old bound selection did not fail");
                const auto denied = observer.snapshot();
                require(denied.selectedSessionChanged && !denied.host && denied.localParticipantId == 0
                    && !denied.canonical, "ended selection allocated or fell back to fresh authority");
                observer.reset();
                std::cout << "PASS ended session selection rejects before allocation without fallback\n";
                // The explicit rejected attempt consumes the same per-source
                // budget. Wait its normal refill, never reset or bypass it.
                pump([] { return false; }, 3100ms);
            }
            require(second.join(endpoint, argv[2], {{"New controller"}}, password(), {}, true), "fresh join");
            const bool fresh = pump([&] { return second.snapshot().journey == J::Lobby; }, 12s);
            require(fresh, "fresh session admission: " + second.snapshot().failure);
            require(second.snapshot().host, "fresh first admission lacks control");
            require(second.snapshot().canonical->sessionId != session, "ended session restored");
            require(!second.snapshot().canonical->participants.front().ready, "fresh session retained readiness");
            if (scenario == "flow-reuse") {
                const auto freshState = *second.snapshot().canonical;
                require(freshState.phase == Network::Replication::Phase::Lobby
                    && freshState.participants.size() == 1 && freshState.players.size() == 1
                    && freshState.hostParticipantId == second.snapshot().localParticipantId
                    && freshState.players.front().ownerParticipantId == second.snapshot().localParticipantId
                    && !freshState.round && freshState.currentRoundNumber == 0 && freshState.completedRounds == 0
                    && !freshState.result.available && freshState.result.serialized.empty(),
                    "fresh ready-empty reuse retained old membership, match or results");
                std::cout << "PASS fresh ready service admits only new controller/player with no retained round/results\n";
            }
            second.endSession();
            require(pump([&] { return second.snapshot().journey == J::Inactive; }, 5s), "fresh session end");
        }
        std::cout << "PASS public dedicated " << scenario << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL public dedicated: " << error.what() << '\n';
        return 1;
    }
}
