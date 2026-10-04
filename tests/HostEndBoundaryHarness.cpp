#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <list>
#include <unordered_map>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tests/TestHarness.h"
#include "tests/HostEndBoundaryControl.h"
#include "source/client/HostServiceSupervisor.h"
#define private public
#include "source/Application.h"
#include "source/Menu.h"
#include "source/NetworkMenu.h"
#undef private

namespace {
    using namespace Duel6;
    using namespace std::chrono_literals;
    namespace fs = std::filesystem;

    std::uint16_t freePort() {
        const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
        D6R_REQUIRE(descriptor >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const int bound = ::bind(descriptor, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        socklen_t size = sizeof(address);
        const int queried = ::getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size);
        ::close(descriptor);
        D6R_REQUIRE(bound == 0 && queried == 0);
        return ntohs(address.sin_port);
    }

    void key(Application &application, NetworkMenu &menu, SDL_Keycode code) {
        for (const auto type: {SDL_KEYDOWN, SDL_KEYUP}) {
            SDL_Event event{};
            event.type = type; event.key.type = type; event.key.keysym.sym = code;
            D6R_REQUIRE_EQ(1, SDL_PushEvent(&event));
            application.processEvents(menu);
        }
    }

    int run(int argc, char **argv) {
        fs::path runDirectory, captures;
        std::string scenario = "host-end", captureState = "both";
        unsigned port = 0, holdSeconds = 0;
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            D6R_REQUIRE(index + 1 < argc);
            const std::string value = argv[++index];
            if (argument == "--run-dir") runDirectory = fs::absolute(value);
            else if (argument == "--scenario") scenario = value;
            else if (argument == "--port") port = std::stoul(value);
            else if (argument == "--hold-seconds") holdSeconds = std::stoul(value);
            else if (argument == "--capture-dir") captures = fs::absolute(value);
            else if (argument == "--capture-state") captureState = value;
            else throw std::runtime_error("Unknown harness argument");
        }
        D6R_REQUIRE(!runDirectory.empty() && (scenario == "host-end" || scenario == "ordinary-close"));
        D6R_REQUIRE(port <= 65535 && holdSeconds <= 300);
        D6R_REQUIRE(captureState == "both" || captureState == "host-ended" || captureState == "entry");
        D6R_REQUIRE(captures.empty() || scenario == "host-end");
        // Each invocation owns new data/session files; never reset shared data.
        D6R_REQUIRE(fs::create_directory(runDirectory));
        fs::copy(D6R_TEST_RESOURCE_DIR, runDirectory, fs::copy_options::recursive);
        fs::copy_file(D6R_RUNTIME_TEST_SERVER, runDirectory / "duel6r-server");
        fs::current_path(runDirectory);
        const Network::Endpoint endpoint{"127.0.0.1", static_cast<std::uint16_t>(port ? port : freePort())};
        std::cout << "[boundary] scenario=" << scenario << ";endpoint=127.0.0.1:" << endpoint.port << std::endl;

        auto control = std::make_shared<Test::HostEndBoundaryControl>();
        Test::installHostEndBoundaryControl(control);
        char name[] = "duel6r-host-end-boundary-harness";
        char *arguments[] = {name};
        Application application(1, arguments);
        auto k1 = PlayerControls::keyboardControls("K1", application.input,
                SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_RSHIFT, SDLK_RETURN);
        auto k2 = PlayerControls::keyboardControls("K2", application.input,
                SDLK_a, SDLK_d, SDLK_w, SDLK_s, SDLK_q, SDLK_1, SDLK_2);
        NetworkMenu host(*application.service, application.gameResources, application.menu->menuBannerTexture,
                         [&] { application.menu->renderMenuBackground(); });
        Context::push(*application.menu);
        // Close guest contexts before the host/controls on every failure path.
        struct PopContexts { ~PopContexts() { while (Context::exists()) Context::pop(); } } popContexts;
        auto &guest = *application.menu->networkMenu;
        Network::HostComposition::Setup setup;
        setup.localPlayerNames = {"Boundary host"};
        setup.levelPlan = "Fixed level"; setup.fixedLevel = "levels/duel_01.json";
        setup.quickLiquid = false;
        host.localPlayers = {{"Boundary host", k1.get(), "K1"}};
        host.hostSetup = setup;
        guest.open({{"Boundary guest", k2.get(), "K2"}}, setup, {"Boundary guest"}, {setup.fixedLevel});
        D6R_REQUIRE(host.runtime.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, runDirectory.string(), setup, host.localPlayers));

        const auto frame = [&] {
            application.processEvents(guest);
            host.update(0); guest.update(0);
            guest.render();
            application.video->screenUpdate(application.console, *application.font);
        };
        const auto pump = [&](auto predicate, std::chrono::milliseconds timeout) {
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            do {
                frame();
                if (predicate()) return true;
                std::this_thread::sleep_for(5ms);
            } while (std::chrono::steady_clock::now() < deadline);
            return predicate();
        };
        D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Lobby; }, 10s));
        D6R_REQUIRE(guest.runtime.join(endpoint, runDirectory.string(), guest.localPlayers));
        D6R_REQUIRE(pump([&] { return guest.runtime.snapshot().journey == Client::NetworkJourney::Lobby; }, 10s));
        host.runtime.setReady(true); guest.runtime.setReady(true);
        D6R_REQUIRE(pump([&] {
            const auto snapshot = host.runtime.snapshot();
            return snapshot.canonical && snapshot.canonical->participants.size() == 2
                    && std::all_of(snapshot.canonical->participants.begin(), snapshot.canonical->participants.end(),
                                   [](const auto &participant) { return participant.ready; });
        }, 5s));
        host.runtime.startMatch();
        D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Match
                && guest.runtime.snapshot().journey == Client::NetworkJourney::Match
                && guest.lastStableJourney == Client::NetworkJourney::Match; }, 10s));
        D6R_REQUIRE(guest.lastStableJourney == Client::NetworkJourney::Match);
        control->armed = true;
        if (scenario == "host-end") {
            key(application, host, SDLK_ESCAPE);
            D6R_REQUIRE(host.confirmation == NetworkMenu::Confirmation::End);
            frame(); // Release the opening gesture before confirming, as in normal input.
            key(application, host, SDLK_RETURN); // Confirm the real host End action.
        } else host.runtime.supervisor->applicationExit(); // Ordinary exit is never intentional End.

        const auto expected = scenario == "host-end" ? Client::NetworkJourney::HostEnded : Client::NetworkJourney::Reconnecting;
        if (!pump([&] { return guest.runtime.snapshot().journey == expected; }, 5s)) {
            std::ostringstream detail;
            detail << "host-journey=" << static_cast<unsigned>(host.runtime.snapshot().journey)
                   << ";guest-journey=" << static_cast<unsigned>(guest.runtime.snapshot().journey)
                   << ";confirmation=" << static_cast<unsigned>(host.confirmation)
                   << ";notices=" << control->notices << ";rejected-probes=" << control->rejectedProbes
                   << ";sealed-notices=" << control->sealedNotices << ";eligible=" << control->eligibleReceipt;
            Test::fail("actual close-boundary journey", __FILE__, __LINE__, detail.str());
        }
        D6R_REQUIRE_EQ(1u, control->rejectedProbes.load());
        D6R_REQUIRE_EQ(scenario == "host-end" ? 1u : 0u, control->sealedNotices.load());
        D6R_REQUIRE(scenario != "host-end" || control->eligibleReceipt);
        D6R_REQUIRE_EQ(1280, application.video->getScreen().getClientWidth());
        D6R_REQUIRE_EQ(900, application.video->getScreen().getClientHeight());
        D6R_REQUIRE(application.menu->hasMenuBackground);
        std::cout << "[boundary] actual-match=true;rejected-probes=" << control->rejectedProbes
                  << ";sealed-notices=" << control->sealedNotices << ";eligible-receipt=" << control->eligibleReceipt
                  << ";guest-journey=" << static_cast<unsigned>(guest.runtime.snapshot().journey)
                  << ";native-render=1280x900;renderer=" << application.video->getRenderer().getInfo().renderer << std::endl;

        const auto present = [&](const fs::path &filename, bool capture) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(holdSeconds);
            do { frame(); std::this_thread::sleep_for(16ms); } while (std::chrono::steady_clock::now() < until);
            if (capture) {
                fs::create_directories(captures);
                const auto path = captures / filename;
                D6R_REQUIRE(!fs::exists(path));
                guest.render();
                application.video->getRenderer().makeScreenshot().save(path.string());
                D6R_REQUIRE(fs::is_regular_file(path));
            }
        };
        if (scenario == "host-end") {
            D6R_REQUIRE(pump([&] { return host.runtime.snapshot().journey == Client::NetworkJourney::Inactive; }, 3s));
            present("host-ended-1280x900.png", !captures.empty() && captureState != "entry");
            key(application, guest, SDLK_RETURN); // Actual Return to Network, not a snapshot assignment.
            D6R_REQUIRE(pump([&] { return guest.runtime.snapshot().journey == Client::NetworkJourney::Inactive; }, 3s));
            D6R_REQUIRE(guest.setupScreen == NetworkMenu::SetupScreen::Entry && guest.focus == 0);
            present("NET-01.png", !captures.empty() && captureState != "host-ended");
        } else {
            const auto snapshot = guest.runtime.snapshot();
            D6R_REQUIRE(snapshot.reconnectSeconds && *snapshot.reconnectSeconds > 0);
            present({}, false);
        }
        guest.runtime.reset(); host.runtime.reset();
        std::cout << "[boundary] cleanup=complete;return-entry=" << (scenario == "host-end") << std::endl;
        return 0;
    }
}

int main(int argc, char **argv) {
    try { return run(argc, argv); }
    catch (const std::exception &error) { std::cerr << "[boundary] failure: " << error.what() << '\n'; return 1; }
    catch (...) { std::cerr << "[boundary] failure: native application exception\n"; return 1; }
}
