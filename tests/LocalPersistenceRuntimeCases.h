// Included by NetworkSessionRuntimeTests.cpp to reuse its real Application and
// loopback fixtures. No production state/outcome/timer setters are used here.
#ifdef __linux__
namespace PersistenceJourney {
    using namespace Duel6;
    constexpr const char *peoplePath = "data/persons.json";
    constexpr const char *levelPath = "levels/persistence-platforms.json";

    std::string bytes(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::binary);
        D6R_REQUIRE(input.good());
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    // Fixture geometry only: real movement, water damage, mode rules, round
    // callbacks and disk writers produce every statistic and outcome asserted.
    void platforms() {
        std::ofstream out(levelPath);
        out << "{\"width\":13,\"height\":7,\"blocks\":[";
        for (int y = 6; y >= 0; --y) for (int x = 0; x < 13; ++x) {
            if (y != 6 || x != 0) out << ',';
            out << (y <= 1 ? 4 : y == 3 && (x == 1 || x == 4 || x == 7 || x == 10) ? 1 : 0);
        }
        out << "],\"elevators\":[]}";
        D6R_REQUIRE(out.good());
    }

    struct App {
        char name[30] = "persistence-journey";
        char *argv[1] = {name};
        Application application{1, argv};
        App() {
            D6R_REQUIRE(!Context::exists());
            Context::push(*application.menu);
            application.video->screen = ScreenParameters(850, 700, 24, 24, 0, false);
        }
        ~App() { while (Context::exists()) Context::pop(); }
        void key(SDL_Keycode code, Uint16 modifiers = 0) {
            application.keyEvent(Context::getCurrent(), KeyPressEvent(code, SysEvent::ButtonState::PRESSED, modifiers));
            application.keyEvent(Context::getCurrent(), KeyPressEvent(code, SysEvent::ButtonState::RELEASED, modifiers));
        }
        void people(unsigned count = 15) {
            auto &menu = *application.menu;
            D6R_REQUIRE(menu.persons.isEmpty());
            for (unsigned i = 0; i < count; ++i) {
                menu.textbox->setText("Persist" + std::to_string(i));
                menu.addPerson();
                menu.addPlayer(static_cast<Int32>(i));
            }
            D6R_REQUIRE_EQ(count, menu.playerListBox->size());
        }
        void start(int mode, int rounds, bool resume = false) {
            auto &menu = *application.menu;
            menu.gameModeSwitch->setCurrent(mode);
            menu.roundsTextbox->setText(std::to_string(rounds));
            menu.quickLiquidCheckBox->setChecked(false);
            // First player stays; other ordinary keyboard-controlled players walk
            // off their starting platforms. No damage or result is injected.
            for (Size i = 0; i < menu.playerListBox->size(); ++i) menu.controlSwitch[i]->setCurrent(i ? 1 : 0);
            if (resume) {
                SDL_Event event{}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_y;
                D6R_REQUIRE_EQ(1, SDL_PushEvent(&event));
            }
            menu.play({levelPath});
            D6R_REQUIRE(application.game->isCurrent());
        }
        void outcome() {
            auto &game = *application.game;
            application.keyEvent(game, KeyPressEvent(SDLK_a, SysEvent::ButtonState::PRESSED, 0));
            for (unsigned tick = 0; tick < 90 * 45 && !game.getRound().hasWinner(); ++tick) game.update(1.0f / 90);
            application.keyEvent(game, KeyPressEvent(SDLK_a, SysEvent::ButtonState::RELEASED, 0));
            D6R_REQUIRE(game.getRound().hasWinner());
            D6R_REQUIRE(!game.isOver());
            D6R_REQUIRE(game.getRound().getRemainingGameOverWait() > 5.0f);
        }
        void complete() {
            auto &game = *application.game;
            for (unsigned tick = 0; tick < 90 * 8 && !game.isOver(); ++tick) game.update(1.0f / 90);
            D6R_REQUIRE(game.isOver());
        }
        void leave(Uint16 modifiers) {
            key(SDLK_ESCAPE, modifiers);
            D6R_REQUIRE(application.game->isClosed());
            Context::pop(); // Exactly the Application::run closed-context dispatch.
            D6R_REQUIRE(application.menu->isCurrent());
        }
        void quit() {
            key(SDLK_ESCAPE);
            D6R_REQUIRE(application.menu->isClosed());
            Context::pop();
            D6R_REQUIRE(!Context::exists());
        }
    };

    void savedCounts(int rounds, int eloGames) {
        const auto data = Json::Parser().parse(peoplePath);
        D6R_REQUIRE_EQ(rounds, data.get("rounds").asInt());
        D6R_REQUIRE_EQ(15u, data.get("persons").getLength());
        D6R_REQUIRE_EQ(15u, data.get("playing").getLength());
        for (Size i = 0; i < 15; ++i) {
            const auto person = data.get("persons").get(i);
            D6R_REQUIRE_EQ(rounds, person.get("games").asInt());
            D6R_REQUIRE_EQ(eloGames, person.get("eloGames").asInt());
        }
    }

    void restored(App &app) {
        const auto data = Json::Parser().parse(peoplePath);
        auto &menu = *app.application.menu;
        D6R_REQUIRE_EQ(data.get("rounds").asInt(), app.application.game->getPlayedRounds());
        for (Size i = 0; i < data.get("persons").getLength(); ++i) {
            const auto saved = data.get("persons").get(i);
            const auto loaded = menu.persons.getByName(saved.get("name").asString()).toJson();
            for (const auto *field: {"shots", "hits", "kills", "deaths", "assistances", "wins", "penalties",
                    "games", "timeAlive", "totalGameTime", "totalDamage", "assistedDamage", "elo", "eloTrend", "eloGames"})
                D6R_REQUIRE_EQ(saved.get(field).asInt(), loaded.get(field).asInt());
            D6R_REQUIRE_EQ(data.get("playing").get(i).asString(), menu.playerListBox->getItem(i));
        }
    }

    // Fork only before any Application/SDL/thread exists. Each stage is a fresh
    // OS process; the next stage can observe previous state only through files.
    void process(const std::function<void()> &stage) {
        std::cout.flush(); std::cerr.flush();
        const auto pid = fork(); D6R_REQUIRE(pid >= 0);
        if (!pid) {
            int status = 0;
            try { stage(); }
            catch (const std::exception &error) { std::cerr << error.what() << '\n'; status = 1; }
            catch (...) { std::cerr << "Persistence child failed\n"; status = 1; }
            std::cout.flush(); std::cerr.flush(); _exit(status);
        }
        int status = 0;
        D6R_REQUIRE_EQ(pid, waitpid(pid, &status, 0));
        D6R_REQUIRE(WIFEXITED(status)); D6R_REQUIRE_EQ(0, WEXITSTATUS(status));
    }

    void navigation(App &app) {
        auto &game = *app.application.game;
        const auto disk = bytes(peoplePath);
        const auto written = std::filesystem::last_write_time(peoplePath);
        const auto plan = game.getWorldRenderer().finalSummaryPlan();
        // Short Deathmatch names can fit completely; Teams and multiwinner
        // Predator exercise actual overflow without inventing unsupported names.
        app.key(SDLK_PAGEDOWN);
        D6R_REQUIRE((game.getFinalScroll().vertical > 0) == (plan.bounds.vertical > 0));
        app.key(SDLK_PAGEUP); D6R_REQUIRE_EQ(0, game.getFinalScroll().vertical);
        app.key(SDLK_PAGEDOWN, KMOD_SHIFT); D6R_REQUIRE_EQ(0, game.getFinalScroll().vertical);
        game.mouseWheelEvent(MouseWheelEvent(plan.layout.body.x + 5,
            app.application.video->getScreen().getClientHeight() - plan.layout.body.y - 5, 0, -2));
        D6R_REQUIRE((game.getFinalScroll().vertical > 0) == (plan.bounds.vertical > 0));
        const auto offset = game.getFinalScroll().vertical;
        app.key(SDLK_BACKQUOTE); app.key(SDLK_PAGEUP);
        D6R_REQUIRE_EQ(offset, game.getFinalScroll().vertical); app.key(SDLK_BACKQUOTE);
        for (unsigned i = 0; i < 900; ++i) game.update(1.0f / 90);
        D6R_REQUIRE_EQ(disk, bytes(peoplePath));
        D6R_REQUIRE(written == std::filesystem::last_write_time(peoplePath));
        std::cout << "ASSERT final navigation and repeated completed updates: bytes/mtime unchanged\n";
    }
}

D6R_TEST_CASE("Persistence finite offline restart resumes real rounds and updates Deathmatch Elo once") {
    using namespace PersistenceJourney;
    D6R_REQUIRE(!std::filesystem::exists(peoplePath)); platforms();
    process([] {
        App app; app.people(); app.start(0, 2); app.outcome(); savedCounts(1, 0);
        app.leave(KMOD_SHIFT); app.quit(); savedCounts(1, 0);
        std::cout << "ASSERT first real round saved; Shift Escape exits unfinished finite match without Elo\n";
    });
    const auto first = bytes(peoplePath);
    process([&] {
        App app; restored(app); D6R_REQUIRE_EQ(first, bytes(peoplePath));
        app.start(0, 2, true); D6R_REQUIRE_EQ(1, app.application.game->getCurrentRound());
        app.outcome(); savedCounts(2, 1);
        const auto data = Json::Parser().parse(peoplePath);
        D6R_REQUIRE_EQ(1140, data.get("persons").get(0).get("elo").asInt());
        for (Size i = 1; i < 15; ++i) D6R_REQUIRE_EQ(990, data.get("persons").get(i).get("elo").asInt());
        const auto saved = bytes(peoplePath);
        app.key(SDLK_ESCAPE); D6R_REQUIRE(!app.application.game->isClosed());
        app.key(SDLK_PAGEDOWN); D6R_REQUIRE_EQ(0, app.application.game->getFinalScroll().vertical);
        app.complete(); navigation(app); app.leave(0); app.quit();
        D6R_REQUIRE_EQ(saved, bytes(peoplePath)); savedCounts(2, 1);
        std::cout << "ASSERT restart/Resume preserves first round; second round rates 1140/990 once; wait Escape ignored, completed Escape exits\n";
    });
    const auto final = bytes(peoplePath);
    process([&] { App app; restored(app); savedCounts(2, 1); app.quit(); D6R_REQUIRE_EQ(final, bytes(peoplePath));
        std::cout << "ASSERT completed match restart restores every saved statistic, Elo, roster and round count\n"; });
}

namespace PersistenceJourney {
    void finalExit(int mode, bool duringWait) {
        D6R_REQUIRE(!std::filesystem::exists(peoplePath)); platforms();
        process([&] {
            App app; app.people(); app.start(mode, 1); app.outcome();
            savedCounts(1, mode == 0 ? 1 : 0);
            const auto saved = bytes(peoplePath);
            if (mode != 0) {
                const auto data = Json::Parser().parse(peoplePath);
                for (Size i = 0; i < 15; ++i) {
                    D6R_REQUIRE_EQ(1000, data.get("persons").get(i).get("elo").asInt());
                    D6R_REQUIRE_EQ(0, data.get("persons").get(i).get("eloTrend").asInt());
                }
            }
            app.key(SDLK_ESCAPE); D6R_REQUIRE(!app.application.game->isClosed());
            if (!duringWait) {
                app.complete();
                if (mode == 2) D6R_REQUIRE(app.application.game->getWorldRenderer().finalSummaryPlan().bounds.vertical > 0);
                navigation(app);
            }
            app.leave(KMOD_SHIFT); app.quit(); D6R_REQUIRE_EQ(saved, bytes(peoplePath));
            std::cout << "ASSERT mode=" << mode << " Shift Escape duringWait=" << duringWait
                      << "; persisted counters/Elo and exit bytes unchanged\n";
        });
    }
}
D6R_TEST_CASE("Persistence real local final wait Shift Escape preserves one Deathmatch Elo update") {
    PersistenceJourney::finalExit(0, true);
}
D6R_TEST_CASE("Persistence real Predator completion excludes Elo across navigation and Shift Escape") {
    PersistenceJourney::finalExit(1, false);
}
D6R_TEST_CASE("Persistence real Team completion excludes Elo across navigation and Shift Escape") {
    PersistenceJourney::finalExit(2, false);
}

namespace PersistenceJourney {
    void networkNoWrites(bool observingHost) {
        using namespace std::chrono_literals;
        D6R_REQUIRE(!std::filesystem::exists(peoplePath));
        // A tiny valid arena fills naturally. This is gameplay fixture content,
        // not a fabricated final snapshot, result, statistic or shortened timer.
        const std::string level = "levels/persistence-network.json";
        { std::ofstream out(level); out << R"({"width":6,"height":4,"blocks":[1,1,1,1,1,1,1,0,0,0,0,1,1,0,0,0,0,1,1,1,1,1,1,1],"elevators":[]})"; }
        App app; app.people();
        app.key(SDLK_F2);
        auto &menu = *app.application.menu->networkMenu;
        D6R_REQUIRE(menu.isCurrent());
        const auto disk = bytes(peoplePath);
        const auto written = std::filesystem::last_write_time(peoplePath);
        const auto unchanged = [&] {
            D6R_REQUIRE_EQ(disk, bytes(peoplePath));
            D6R_REQUIRE(written == std::filesystem::last_write_time(peoplePath));
        };
        Client::NetworkSessionRuntime peer, unused;
        auto &host = observingHost ? menu.runtime : peer;
        auto &guest = observingHost ? peer : menu.runtime;
        Network::HostComposition::Setup setup;
        setup.localPlayerNames = {"Host"}; setup.fixedLevel = level; setup.roundLimit = 1; setup.quickLiquid = true;
        const Network::Endpoint endpoint{"127.0.0.1", unusedLoopbackPort()};
        D6R_REQUIRE(host.startHost(endpoint, D6R_RUNTIME_TEST_SERVER, ".", setup, {player("Host")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return host.snapshot().journey == Client::NetworkJourney::Lobby; }));
        D6R_REQUIRE(guest.join(endpoint, ".", {player("Guest")}));
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 10s, [&] { return guest.snapshot().journey == Client::NetworkJourney::Lobby; }));
        unchanged(); host.setReady(true); guest.setReady(true);
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return everyParticipantReady(host.snapshot(), true); }));
        host.startMatch();
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 60s, [&] {
            unchanged();
            return host.snapshot().journey == Client::NetworkJourney::Summary && guest.snapshot().journey == Client::NetworkJourney::Summary;
        }));
        D6R_REQUIRE_EQ(std::string("Completed"), menu.runtime.snapshot().canonical->result.state);
        const auto result = Network::Replication::retainedResultView(menu.runtime.snapshot().canonical->result);
        D6R_REQUIRE(result); D6R_REQUIRE_EQ(1u, result->completedRounds);
        app.key(SDLK_PAGEDOWN); app.key(SDLK_PAGEUP); app.key(SDLK_TAB); unchanged();
        if (observingHost) host.endSession(); else guest.leave();
        D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] {
            return menu.runtime.snapshot().journey == Client::NetworkJourney::Inactive;
        }));
        unchanged();
        if (!observingHost) { host.endSession(); D6R_REQUIRE(pumpRuntimes(host, guest, unused, 5s, [&] { return host.snapshot().journey == Client::NetworkJourney::Inactive; })); }
        Context::pop(); D6R_REQUIRE(app.application.menu->isCurrent());
        D6R_REQUIRE_EQ(disk, bytes(peoplePath)); app.quit(); D6R_REQUIRE_EQ(disk, bytes(peoplePath));
        savedCounts(0, 0);
        std::cout << "ASSERT actual " << (observingHost ? "Host" : "Guest")
                  << " lobby/active/Completed/navigation/End-or-Leave: persons bytes+mtime unchanged; menu exit content unchanged\n";
    }
}
D6R_TEST_CASE("Persistence actual network Host completion never writes local statistics or Elo") {
    PersistenceJourney::networkNoWrites(true);
}
D6R_TEST_CASE("Persistence actual network Guest completion never writes local statistics or Elo") {
    PersistenceJourney::networkNoWrites(false);
}
#endif
