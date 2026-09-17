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

// Driven by PublicDedicatedProcessTests.py: no credentials in argv or output.
using namespace Duel6;
using namespace std::chrono_literals;
static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
class TestControl final : public Control {
public:
    bool pressed = false;
    bool isPressed() const override { return pressed; }
};
int main(int argc, char **argv) {
    try {
        require(argc == 4, "expected port, resource root, scenario");
        auto *jump = new TestControl;
        PlayerControls controls("test controller", new TestControl, new TestControl, jump,
                                new TestControl, new TestControl, new TestControl, new TestControl);
        Client::NetworkSessionRuntime first, second, observer;
        const auto *secret = std::getenv("D6R_TEST_INVITE");
        auto invitation = [&] {
            auto value = std::make_shared<Network::PublicSession::Secret>();
            value->value = secret ? secret : "";
            return value;
        };
        Network::Endpoint endpoint{"127.0.0.1", static_cast<std::uint16_t>(std::stoi(argv[1]))};
        auto pump = [&](auto predicate, auto timeout) {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            do {
                first.update(); second.update(); observer.update();
                if (predicate()) return true;
                std::this_thread::sleep_for(5ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };
        using J = Client::NetworkJourney;
        require(first.join(endpoint, argv[2], {{"Controller", &controls}}, true, invitation()), "join start");
        const std::string scenario = argv[3];
        if (scenario.compare(0, 7, "attack-") == 0) {
            namespace H = Network::HostComposition;
            namespace R = Network::Replication;
            require(pump([&] { return first.snapshot().journey == J::Lobby; }, 12s), "attack controller admission");
            require(second.join(endpoint, argv[2], {{"Attacker"}}, true, invitation()), "attacker join start");
            require(pump([&] { return second.snapshot().journey == J::Lobby; }, 12s), "attacker admission");
            require(observer.join(endpoint, argv[2], {{"Observer"}}, true, invitation()), "observer join start");
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
            require(pump([&] { return std::filesystem::exists(root / "attack-rejected"); }, 3s),
                    "exact malicious TLS frame and server-side close not observed");
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
            first.endSession();
            require(pump([&] { return observer.snapshot().journey == J::HostEnded; }, 5s), "unaffected participant did not receive legitimate end");
            std::cout << "PASS exact malicious frame observed over TLS; server rejected; authority/state retained\n";
        } else if (scenario.compare(0, 7, "notice-") == 0 || scenario == "recovery" || scenario == "expiry") {
            require(pump([&] { return first.snapshot().journey == J::Lobby; }, 12s), "review first admission");
            const auto session = first.snapshot().canonical->sessionId;
            const auto controller = first.snapshot().localParticipantId;
            require(second.join(endpoint, argv[2], {{"Guest"}}, true, invitation()), "review guest start");
            require(pump([&] { return second.snapshot().journey == J::Lobby
                && first.snapshot().canonical->participants.size() == 2; }, 12s), "review guest admission");
            const bool match = scenario.find("-match") != std::string::npos;
            const bool summary = scenario.find("-summary") != std::string::npos;
            if (match || summary) {
                Network::HostComposition::Setup setup;
                setup.localPlayerNames = {"Controller"};
                setup.fixedLevel = "levels/arena.json";
                setup.roundLimit = 1; setup.quickLiquid = summary;
                require(first.updateHostSetup(setup), "review setup enqueue");
                require(pump([&] { return second.snapshot().canonical->settings.quickLiquid == summary
                    && second.snapshot().canonical->settings.roundLimit == 1; }, 5s), "review setup propagation");
                first.setReady(true); second.setReady(true);
                require(pump([&] { const auto s = first.snapshot(); return std::all_of(
                    s.canonical->participants.begin(), s.canonical->participants.end(),
                    [](const auto &p) { return p.ready; }); }, 5s), "review readiness");
                first.startMatch();
                require(pump([&] { return second.snapshot().journey == J::Match; }, 10s), "review active match");
                if (summary) require(pump([&] { return second.snapshot().journey == J::Summary; }, 90s), "review final summary");
            }
            const auto phase = second.snapshot().journey;
            const auto root = std::filesystem::path(argv[2]);
            auto request = [&](const std::string &action, unsigned target) {
                { std::ofstream out(root / "action.tmp"); out << target << ' ' << action << ' ' << session; }
                std::filesystem::rename(root / "action.tmp", root / "action");
                require(pump([&] { return std::filesystem::exists(root / "acted"); }, 5s), "fixture action acknowledgement");
                std::filesystem::remove(root / "acted");
            };
            if (scenario == "recovery") {
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
                require(second.join(endpoint, argv[2], {{"Fresh"}}, true, invitation()), "post-expiry fresh join start");
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
                } else {
                    require(pump([&] { return second.snapshot().journey == J::Failure; }, 5s), "established TLS notice did not select terminal failure");
                    const auto state = second.snapshot();
                    require(state.failure == (maintenance ? Network::PublicSession::Maintenance : Network::PublicSession::ControllerExpired), "established TLS terminal reason mismatch");
                    require(!state.reconnectSeconds && !state.retryAllowed, "terminal notice retained recovery/retry");
                }
            }
        } else if (scenario == "concurrent") {
            require(second.join(endpoint, argv[2], {{"Contender"}}, true, invitation()), "concurrent join start");
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
        } else if (scenario != "flow") {
            require(pump([&] { return first.snapshot().journey == J::Failure; }, 12s), "failure deadline");
            const auto state = first.snapshot();
            require(!state.host && !state.localParticipantId && !state.canonical, "rejection granted authority");
            if (scenario == "security") {
                require(state.securityFailure, "missing security failure classification");
                require(state.failure == Network::PublicSession::SecurityFailure, "security failure copy");
            } else {
                require(state.authorizationRejected, "missing authorization classification");
                require(state.failure == "Connection not authorized.", "authorization failure copy");
            }
        } else {
            if (!pump([&] { return first.snapshot().journey == J::Lobby; }, 12s))
                throw std::runtime_error("first admission: " + first.snapshot().failure
                    + " journey=" + std::to_string(static_cast<int>(first.snapshot().journey)));
            require(first.snapshot().host && first.snapshot().publicSession, "first participant not controller");
            const auto controller = first.snapshot().localParticipantId;
            const auto session = first.snapshot().canonical->sessionId;
            require(second.join(endpoint, argv[2], {{"Guest"}}, true, invitation()), "guest join start");
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
            first.endSession();
            require(pump([&] { return first.snapshot().journey == J::Inactive
                && second.snapshot().journey == J::HostEnded; }, 5s), "confirmed end outcome");
            first.reset(); second.reset();
            require(second.join(endpoint, argv[2], {{"New controller"}}, true, invitation()), "fresh join");
            require(pump([&] { return second.snapshot().journey == J::Lobby; }, 12s), "fresh session admission");
            require(second.snapshot().host, "fresh first admission lacks control");
            require(second.snapshot().canonical->sessionId != session, "ended session restored");
            require(!second.snapshot().canonical->participants.front().ready, "fresh session retained readiness");
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
