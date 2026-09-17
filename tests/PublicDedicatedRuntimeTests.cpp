#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "source/client/NetworkSessionRuntime.h"
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
        Client::NetworkSessionRuntime first, second;
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
                first.update(); second.update();
                if (predicate()) return true;
                std::this_thread::sleep_for(5ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        };
        using J = Client::NetworkJourney;
        require(first.join(endpoint, argv[2], {{"Controller", &controls}}, true, invitation()), "join start");
        const std::string scenario = argv[3];
        if (scenario.compare(0, 7, "notice-") == 0 || scenario == "recovery" || scenario == "expiry") {
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
