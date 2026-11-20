#include <algorithm>
#include <climits>
#include <set>
#include "source/FinalSummaryPresentation.h"
#include "tests/TestHarness.h"

using namespace Duel6;
using namespace Duel6::FinalSummaryPresentation;

namespace {
    Model ordinary(Scope scope = Scope::Local) {
        Model result;
        result.scope = scope;
        result.completedRounds = result.lastRoundNumber = result.roundLimit = 123;
        result.matchOutcome = {false, "", {{"Final winner"}}};
        result.lastRoundOutcome = result.matchOutcome;
        result.cumulative.columns = {{""}, {"K", true}, {"A", true}, {"D", true},
                                     {"K/D", true}, {"PTS", true}};
        result.cumulative.rows = {{{"1 Cumulative leader", "14", "2", "3", "4.67", "19"}},
                                  {{"2 Final winner", "3", "0", "4", "0.75", "4"}}};
        return result;
    }

    bool hasText(const Plan &plan, const std::string &value) {
        return std::any_of(plan.commands.begin(), plan.commands.end(), [&](const Command &command) {
            return command.kind == Command::Kind::Text && command.text == value;
        });
    }

    const Command &findText(const Plan &plan, const std::string &value) {
        const auto found = std::find_if(plan.commands.begin(), plan.commands.end(), [&](const Command &command) {
            return command.kind == Command::Kind::Text && command.text == value;
        });
        D6R_REQUIRE(found != plan.commands.end());
        return *found;
    }

    void sameRect(const Rect &left, const Rect &right) {
        D6R_REQUIRE_EQ(left.x, right.x); D6R_REQUIRE_EQ(left.y, right.y);
        D6R_REQUIRE_EQ(left.width, right.width); D6R_REQUIRE_EQ(left.height, right.height);
    }

    Model teams() {
        auto model = ordinary();
        model.cumulative.rows.clear();
        const std::vector<std::string> names{"Alpha", "Bravo", "Charlie", "Delta"};
        int person = 1;
        for (int team = 0; team < 4; ++team) {
            Row header{{std::to_string(team + 1) + " " + names[team], "9", "2", "4", "2.25", "12"}};
            header.teamSeparatorBefore = team != 0;
            static const Color colors[] = {Color::RED, Color::GREEN, Color::YELLOW, Color::MAGENTA};
            header.teamColor = colors[team];
            model.cumulative.rows.push_back(header);
            for (int index = 0; index < (team == 3 ? 3 : 4); ++index) {
                Row player{{"Profile " + std::to_string(person++), "1", "0", "1", "1.00", "2"}};
                player.nested = true;
                model.cumulative.rows.push_back(player);
            }
        }
        model.matchOutcome = {false, "Bravo", {}};
        model.lastRoundOutcome = model.matchOutcome;
        return model;
    }
}

D6R_TEST_CASE("Compact finals have one interim score strip and separate measured final cues at supported sizes") {
    for (const auto size: {std::pair<int, int>{850, 700}, {1280, 720}, {1280, 900}, {1920, 1080}}) {
        const auto local = compose(ordinary(), size.first, size.second);
        const auto host = compose(ordinary(Scope::NetworkHost), size.first, size.second);
        const auto guest = compose(ordinary(Scope::NetworkGuest), size.first, size.second);
        for (const auto *p: {&local, &host, &guest}) {
            D6R_REQUIRE_EQ(1, std::count_if(p->commands.begin(), p->commands.end(), [](const auto &c) {
                return c.text == "---SCORE---";
            }));
            D6R_REQUIRE(!hasText(*p, "SCORE"));
            D6R_REQUIRE(p->layout.title.x >= 16);
            D6R_REQUIRE(p->layout.title.x + p->layout.title.width <= size.first - 16);
            D6R_REQUIRE(p->layout.panel.y >= 16);
            D6R_REQUIRE_EQ(p->layout.panel.x - 5, p->layout.title.x);
            D6R_REQUIRE_EQ(p->layout.panel.width + 10, p->layout.title.width);
            D6R_REQUIRE_EQ(36, p->layout.title.height);
            const auto &counter = findText(*p, "Rounds: 123 | 123");
            D6R_REQUIRE_EQ(counter.bounds.width + 16, p->layout.counter.width);
            D6R_REQUIRE_EQ(40, p->layout.counter.height);
            D6R_REQUIRE_EQ(size.first / 2, p->layout.counter.x + p->layout.counter.width / 2);
            D6R_REQUIRE_EQ(16, p->layout.notice.y - p->layout.counter.y - p->layout.counter.height);
            D6R_REQUIRE_EQ(16, size.second - p->layout.notice.y - p->layout.notice.height);
            D6R_REQUIRE_EQ(Color(128, 0, 0, 200), p->commands.front().color);
            D6R_REQUIRE_EQ(32, findText(*p, "---SCORE---").size);
        }
        D6R_REQUIRE_EQ(0, local.layout.footer.height);
        D6R_REQUIRE_EQ(0, local.layout.status.height);
        D6R_REQUIRE_EQ(0, local.layout.navigation.height);
        D6R_REQUIRE_EQ((size.second - local.layout.panel.height) / 2, local.layout.panel.y);
        D6R_REQUIRE_EQ(224, local.layout.panel.height); // Actual OVER-02:160 + two32px rows.
        D6R_REQUIRE_EQ(16, host.layout.counter.y - host.layout.footer.y - host.layout.footer.height);
        D6R_REQUIRE_EQ(16, guest.layout.footer.y - guest.layout.status.y - guest.layout.status.height);
        D6R_REQUIRE(guest.layout.panel.y + guest.layout.panel.height + 16 <= guest.layout.status.y);
    }
}

D6R_TEST_CASE("Compact local captions disappear without mutating outcomes and network detail retains No winner") {
    auto model = ordinary();
    const auto local = compose(model, 1280, 720);
    for (const auto *removed: {"Local Play", "Match outcome: Final winner",
                              "Last completed round: 123 / Final winner", "Cumulative ranking", "Player / team"})
        D6R_REQUIRE(!hasText(local, removed));
    D6R_REQUIRE(hasText(local, "1 Cumulative leader"));
    D6R_REQUIRE_EQ("Final winner", model.matchOutcome.winners.front().name);
    for (const auto &command: local.commands) {
        D6R_REQUIRE(command.text.find("Session only") == std::string::npos);
        D6R_REQUIRE(command.text.find("saved") == std::string::npos);
        if (command.kind == Command::Kind::Text && command.bounds.y < local.layout.rows.y)
            D6R_REQUIRE_EQ(Color(255), command.color);
    }
    model.scope = Scope::NetworkGuest;
    model.matchOutcome = {}; model.lastRoundOutcome = {};
    model.cumulative.rows.resize(1); // permitted solo result, not a new winner
    const auto solo = compose(model, 850, 700);
    D6R_REQUIRE(hasText(solo, "Completed / Session only"));
    D6R_REQUIRE(hasText(solo, "Waiting for host to return to lobby or end session"));
    D6R_REQUIRE(hasText(solo, "Not saved to local statistics or Elo"));
    D6R_REQUIRE(!hasText(solo, "No winner")); // Cumulative scores remain the initial view.
    const auto detail = compose(model, 850, 700, {INT_MAX, 0});
    D6R_REQUIRE(hasText(detail, "No winner"));
    D6R_REQUIRE_EQ(std::size_t(2), solo.bounds.totalRows);
}

D6R_TEST_CASE("Final role actions retain order permission focus geometry and disabled hit testing") {
    auto model = ordinary(Scope::NetworkHost);
    model.actions = {{Action::Leave}, {Action::EndSession, false, true}, {Action::ReturnToLobby}};
    const auto host = compose(model, 1280, 900);
    D6R_REQUIRE_EQ(std::size_t(2), host.actions.size());
    D6R_REQUIRE_EQ(Action::ReturnToLobby, host.actions[0].state.action);
    D6R_REQUIRE_EQ(Action::EndSession, host.actions[1].state.action);
    const auto &first = host.actions[0].bounds;
    D6R_REQUIRE_EQ(Action::ReturnToLobby, *hitAction(host, first.x + 1, first.y + 1));
    const auto &disabled = host.actions[1].bounds;
    D6R_REQUIRE(!hitAction(host, disabled.x + 1, disabled.y + 1));
    D6R_REQUIRE(!hitAction(host, first.x + 1, first.y - 1));
    D6R_REQUIRE(host.actions[1].state.focused);
    model.scope = Scope::NetworkGuest;
    const auto guest = compose(model, 1280, 900);
    D6R_REQUIRE_EQ(std::size_t(1), guest.actions.size());
    D6R_REQUIRE_EQ(Action::Leave, guest.actions[0].state.action);
    D6R_REQUIRE_EQ(640, guest.actions[0].bounds.x + guest.actions[0].bounds.width / 2);
    model.scope = Scope::Local;
    const auto local = compose(model, 1280, 900);
    D6R_REQUIRE(local.actions.empty());
    for (const auto &command: local.commands)
        D6R_REQUIRE(!local.layout.footer.contains(command.bounds.x, command.bounds.y));
}

D6R_TEST_CASE("Four team final nesting keeps fifteen profiles and exactly three centered separator bands") {
    const auto model = teams();
    const auto plan = compose(model, 1280, 900);
    D6R_REQUIRE_EQ(std::size_t(19), plan.bounds.totalRows);
    D6R_REQUIRE(plan.bounds.vertical > 0);
    D6R_REQUIRE_EQ(32 + 19 * 32 + 3 * 8, plan.bounds.contentHeight);
    std::set<std::string> seen;
    for (int offset = 0; offset <= plan.bounds.vertical + 18; offset += 18) {
        const auto page = compose(model, 1280, 900, {offset, 0});
        for (const auto &command: page.commands) if (command.kind == Command::Kind::Text) seen.insert(command.text);
    }
    for (int i = 1; i <= 15; ++i) D6R_REQUIRE(seen.count("Profile " + std::to_string(i)));
    const auto &team = findText(plan, "1 Alpha");
    const auto &player = findText(plan, "Profile 1");
    D6R_REQUIRE_EQ(32, player.bounds.y - team.bounds.y);
    D6R_REQUIRE(player.bounds.x > team.bounds.x);
    D6R_REQUIRE_EQ(40, findText(plan, "2 Bravo").bounds.y - findText(plan, "Profile 4").bounds.y);
    std::size_t separators = 0;
    for (const auto &command: plan.commands) if (command.color == Color(255, 255, 255, 178)) {
        ++separators;
        D6R_REQUIRE_EQ(2, command.bounds.height);
    }
    D6R_REQUIRE(separators > 0 && separators <= 3);
    const auto minimum = compose(model, 1280, 720);
    D6R_REQUIRE(minimum.bounds.vertical > 0);
    const auto bottom = compose(model, 1280, 720, {INT_MAX, INT_MAX});
    D6R_REQUIRE(hasText(bottom, "Profile 15"));
    D6R_REQUIRE(hasText(bottom, "K/D"));
    D6R_REQUIRE(hasText(bottom, "PTS"));
    D6R_REQUIRE(hasText(bottom, "---SCORE---"));
    sameRect(minimum.layout.footer, bottom.layout.footer);
    sameRect(minimum.layout.notice, bottom.layout.notice);
}

D6R_TEST_CASE("Fourteen departed Predator winners remain complete in outcome detail after cumulative scores") {
    auto model = ordinary(Scope::NetworkGuest);
    model.matchOutcome = {false, "", {}};
    for (int i = 1; i <= 14; ++i)
        model.matchOutcome.winners.push_back({"Marine " + std::to_string(i), std::to_string(100 + i), "", i == 14});
    model.lastRoundOutcome = model.matchOutcome;
    const auto plan = compose(model, 1280, 720);
    D6R_REQUIRE(hasText(plan, "1 Cumulative leader"));
    D6R_REQUIRE(!hasText(plan, "Outcome identities"));
    D6R_REQUIRE_EQ(std::size_t(16), plan.bounds.totalRows);
    std::set<std::string> seen;
    for (int y = 0; y <= plan.bounds.vertical + 18; y += 18) {
        const auto page = compose(model, 1280, 720, {y, 0});
        for (const auto &command: page.commands) if (command.kind == Command::Kind::Text) seen.insert(command.text);
        D6R_REQUIRE(hasText(page, "---SCORE---"));
        D6R_REQUIRE(hasText(page, "Not saved to local statistics or Elo"));
    }
    for (int i = 1; i <= 14; ++i) {
        D6R_REQUIRE(seen.count("Marine " + std::to_string(i)));
        D6R_REQUIRE(seen.count(std::to_string(100 + i)));
    }
    D6R_REQUIRE(seen.count("Departed"));
    D6R_REQUIRE(seen.count("Match"));
    D6R_REQUIRE(seen.count("Outcome identities"));
}

D6R_TEST_CASE("Final body exposes long UTF8 values intact with no ellipsis and numeric cells aligned right") {
    auto model = ordinary();
    // Sixty-four UTF-8 bytes, followed by a long identity/metadata value. All
    // clipping is viewport clipping, never destructive shortening of the model.
    const std::string name = std::string(32, 'A') + "žžžžžžžžžžžžžžžž";
    const std::string value = name + std::string(140, 'x') + "終";
    model.cumulative.rows[0].cells[0] = value;
    const auto plan = compose(model, 850, 700);
    D6R_REQUIRE(plan.bounds.horizontal > 0);
    std::set<std::string> slices;
    for (int x = 0; x <= plan.bounds.horizontal + 8; x += 8) {
        const auto page = compose(model, 850, 700, {0, x});
        for (const auto &command: page.commands) if (command.kind == Command::Kind::Text) {
            const auto &s = command.text;
            D6R_REQUIRE(s.find("...") == std::string::npos);
            if (!s.empty()) D6R_REQUIRE((static_cast<unsigned char>(s.front()) & 0xc0) != 0x80);
            if (command.bounds.y == page.layout.rows.y) slices.insert(s);
        }
    }
    bool foundLast = false, foundAccent = false;
    for (const auto &slice: slices) {
        foundLast |= slice.find("終") != std::string::npos;
        foundAccent |= slice.find("ž") != std::string::npos;
    }
    D6R_REQUIRE(foundLast); D6R_REQUIRE(foundAccent);
    const auto last = compose(model, 850, 700, {0, INT_MAX});
    D6R_REQUIRE(hasText(last, "---SCORE---"));
    const auto &points = findText(last, "19");
    const auto &heading = findText(last, "PTS");
    D6R_REQUIRE_EQ(points.bounds.x + points.bounds.width, heading.bounds.x + heading.bounds.width + 32);
    D6R_REQUIRE_EQ(value, model.cumulative.rows[0].cells[0]);
}

D6R_TEST_CASE("Final completed-only local navigation isolates console wait wheel area Shift and unrelated input") {
    auto model = teams();
    model.cumulative.rows[0].cells[0] = std::string(180, 'A');
    const auto plan = compose(model, 1280, 720);
    Scroll scroll;
    LocalNavigation input;
    input.input = LocalInput::PageDown;
    D6R_REQUIRE(!navigateLocal(scroll, plan, input));
    input.completed = true; input.consoleOwnsInput = true;
    D6R_REQUIRE(!navigateLocal(scroll, plan, input));
    input.consoleOwnsInput = false;
    D6R_REQUIRE(!navigateLocal(scroll, compose(ordinary(Scope::NetworkHost), 1280, 720), input));
    D6R_REQUIRE(navigateLocal(scroll, plan, input));
    D6R_REQUIRE(scroll.vertical > 0);
    D6R_REQUIRE_EQ(0, scroll.horizontal);
    const int vertical = scroll.vertical;
    input.shift = true;
    D6R_REQUIRE(navigateLocal(scroll, plan, input));
    D6R_REQUIRE_EQ(vertical, scroll.vertical);
    D6R_REQUIRE_EQ(64, scroll.horizontal);
    input.input = LocalInput::Wheel; input.wheelY = 1;
    D6R_REQUIRE(!navigateLocal(scroll, plan, input));
    input.pointerX = plan.layout.body.x; input.pointerY = plan.layout.body.y;
    D6R_REQUIRE(navigateLocal(scroll, plan, input));
    D6R_REQUIRE_EQ(56, scroll.horizontal);
    input.shift = false;
    D6R_REQUIRE(navigateLocal(scroll, plan, input));
    D6R_REQUIRE_EQ(std::max(0, vertical - 18), scroll.vertical);
    input.input = LocalInput::Other;
    D6R_REQUIRE(!navigateLocal(scroll, plan, input));
    move(scroll, plan.bounds, INT_MAX, INT_MAX);
    D6R_REQUIRE_EQ(plan.bounds.vertical, scroll.vertical);
    D6R_REQUIRE_EQ(plan.bounds.horizontal, scroll.horizontal);
    move(scroll, plan.bounds, INT_MIN, INT_MIN);
    D6R_REQUIRE_EQ(0, scroll.vertical); D6R_REQUIRE_EQ(0, scroll.horizontal);
    const auto shortPlan = compose(ordinary(), 1280, 900);
    move(scroll, shortPlan.bounds, 8, 8);
    D6R_REQUIRE_EQ(0, scroll.vertical); D6R_REQUIRE_EQ(0, scroll.horizontal);
}

D6R_TEST_CASE("Network required details stay after cumulative results and keep current table headings visible") {
    auto model = ordinary(Scope::NetworkHost);
    model.settings = {{"Match settings", {{"Setting"}, {"Value"}},
                       {{{"Mode", "Predator"}}, {{"Seed", "123456"}}}}};
    Table round{"Per-round results", {{"Round"}, {"Player identity"}, {"Damage", true}, {"Assisted damage", true}}, {}};
    for (int i = 1; i <= 50; ++i) round.rows.push_back({{std::to_string(i), "Player 17", "999", "11"}});
    model.rounds.push_back(round);
    const auto top = compose(model, 1280, 720);
    const auto bottom = compose(model, 1280, 720, {INT_MAX, 0});
    D6R_REQUIRE(hasText(top, "1 Cumulative leader"));
    D6R_REQUIRE(hasText(bottom, "Per-round results"));
    D6R_REQUIRE(hasText(bottom, "Assisted damage"));
    D6R_REQUIRE(hasText(bottom, "50"));
    D6R_REQUIRE_EQ(top.layout.body.y + 42, findText(bottom, "Assisted damage").bounds.y);
    D6R_REQUIRE(hasText(bottom, "Completed / Session only"));
    D6R_REQUIRE(!hasText(bottom, "Match outcome: Final winner"));
    D6R_REQUIRE_EQ(std::size_t(55), bottom.bounds.totalRows);
}

D6R_TEST_CASE("Final network table retains every supplied identity ownership and statistic across horizontal navigation") {
    auto model = ordinary(Scope::NetworkGuest);
    model.cumulative.columns = {{"Rank", true}, {"Display name"}, {"Player identity"}, {"Owner participant"},
            {"Team"}, {"Departure"}, {"Rounds played", true}, {"Shots", true}, {"Hits", true},
            {"Kills", true}, {"Deaths", true}, {"Assists", true}, {"Wins", true}, {"Penalties", true},
            {"Survival ticks", true}, {"Damage", true}, {"Assisted damage", true}, {"Points", true}};
    const std::vector<std::string> values{"1", "Retained identity", "18446744073709551614",
            "18446744073709551613", "Bravo", "Departed", "100", "101", "102", "103", "104",
            "105", "106", "107", "108", "109", "110", "207"};
    model.cumulative.rows = {{values}};
    const auto initial = compose(model, 1280, 720);
    std::set<std::string> observed;
    for (int x = 0; x <= initial.bounds.horizontal + 8; x += 8) {
        const auto plan = compose(model, 1280, 720, {0, x});
        for (const auto &command: plan.commands)
            if (command.kind == Command::Kind::Text) observed.insert(command.text);
        D6R_REQUIRE(hasText(plan, "---SCORE---"));
    }
    for (const auto &value: values) D6R_REQUIRE(observed.count(value));
    for (const auto &column: model.cumulative.columns) D6R_REQUIRE(observed.count(column.heading));
    D6R_REQUIRE_EQ(values, model.cumulative.rows.front().cells);
    D6R_REQUIRE_EQ(std::size_t(2), initial.bounds.totalRows);
}

D6R_TEST_CASE("Final reference score tables center measured content and preserve supplied local life colors") {
    auto model = ordinary();
    model.cumulative.rows[0].identityColor = Color::RED; // Leader is not the surviving winner.
    model.cumulative.rows[1].identityColor = Color::YELLOW;
    for (const auto size: {std::pair{850, 700}, std::pair{1280, 720}, std::pair{1280, 900},
                          std::pair{1920, 1080}}) {
        const auto plan = compose(model, size.first, size.second);
        const auto &leader = findText(plan, "1 Cumulative leader");
        const auto &winner = findText(plan, "2 Final winner");
        D6R_REQUIRE_EQ(32, leader.size);
        D6R_REQUIRE_EQ(32, winner.bounds.y - leader.bounds.y);
        D6R_REQUIRE_EQ(Color::RED, leader.color);
        D6R_REQUIRE_EQ(Color::YELLOW, winner.color);
        D6R_REQUIRE(!hasText(plan, "Cumulative ranking"));
        const auto &strip = findText(plan, "---SCORE---");
        D6R_REQUIRE_EQ(32, strip.size);
        D6R_REQUIRE_EQ(plan.layout.panel.y + 32, strip.bounds.y);
        D6R_REQUIRE_EQ(plan.layout.body.y, findText(plan, "PTS").bounds.y);
        D6R_REQUIRE_EQ(plan.layout.body.y + 32, leader.bounds.y);
        const auto surface = std::find_if(plan.commands.begin(), plan.commands.end(), [&](const Command &command) {
            return command.kind == Command::Kind::Fill && command.bounds.y == leader.bounds.y - 1
                    && command.color == Color(0, 0, 255, 178);
        });
        D6R_REQUIRE(surface != plan.commands.end());
        D6R_REQUIRE_EQ(surface->bounds.width, plan.layout.rows.width);
        D6R_REQUIRE_EQ(plan.layout.rows.x + (plan.layout.rows.width - surface->bounds.width) / 2, surface->bounds.x);
        D6R_REQUIRE_EQ(surface->bounds.x + (surface->bounds.width - strip.bounds.width) / 2, strip.bounds.x);
        D6R_REQUIRE_EQ(32, surface->bounds.height);
        D6R_REQUIRE_EQ(Color::YELLOW, findText(plan, "19").color);
        D6R_REQUIRE_EQ(Color::WHITE, findText(plan, "PTS").color);
        std::size_t pipes = 0;
        for (const auto &command: plan.commands) if (command.text == "|") {
            ++pipes;
            D6R_REQUIRE_EQ(32, command.size);
            D6R_REQUIRE_EQ(Color::YELLOW, command.color);
        }
        D6R_REQUIRE_EQ(std::size_t(10), pipes);
        D6R_REQUIRE_EQ(0, plan.bounds.horizontal);
    }
}

D6R_TEST_CASE("Final Team score rows use complete named fills subdued children and no color swatches") {
    const auto plan = compose(teams(), 1280, 900);
    const auto &team = findText(plan, "1 Alpha");
    const auto &child = findText(plan, "Profile 1");
    D6R_REQUIRE_EQ(Color::BLACK, team.color);
    D6R_REQUIRE_EQ(Color::WHITE, child.color);
    bool parentSurface = false, childSurface = false;
    for (const auto &command: plan.commands) {
        if (command.kind != Command::Kind::Fill) continue;
        D6R_REQUIRE(!(command.bounds.width == 12 && command.bounds.height == 10));
        if (command.bounds.y == team.bounds.y - 1 && command.bounds.height >= 31) {
            parentSurface = command.color == Color::RED.withAlpha(178);
        }
        if (command.bounds.y == child.bounds.y - 1 && command.bounds.height == 32) {
            childSurface = command.color == Color::RED.withAlpha(178).scale(0.2f);
        }
    }
    D6R_REQUIRE(parentSurface); D6R_REQUIRE(childSurface);
}

D6R_TEST_CASE("Final mixed score and supporting tables retain independent sticky heights and bounded glyphs") {
    auto model = ordinary(Scope::NetworkGuest);
    model.matchOutcome = {false, "", {}};
    for (int i = 0; i < 14; ++i) model.matchOutcome.winners.push_back({std::string(32, 'z') + "žž", std::to_string(i)});
    model.settings = {{"Match settings", {{"Setting"}, {"Value"}}, {{{"Mode", "Predator"}}}}};
    Table statistics{"Round 123 player statistics", model.cumulative.columns, model.cumulative.rows, true};
    statistics.rows.resize(30, model.cumulative.rows.front());
    model.rounds = {{"Round details", {{"Field"}, {"Value"}}, {{{"Level", "Duel 16"}}}}, statistics};
    for (const auto size: {std::pair{850, 700}, std::pair{1280, 720}, std::pair{1280, 900},
                          std::pair{1920, 1080}}) {
        const auto first = compose(model, size.first, size.second);
        std::set<std::string> seen;
        for (int y = 0; y <= first.bounds.vertical + 18; y += 18) {
            for (int x: {0, first.bounds.horizontal}) {
                const auto plan = compose(model, size.first, size.second, {y, x});
                for (const auto &command: plan.commands) {
                    if (command.kind != Command::Kind::Text) continue;
                    seen.insert(command.text);
                    if (command.bounds.y < plan.layout.body.y
                        || command.bounds.y >= plan.layout.body.y + plan.layout.body.height) continue;
                    D6R_REQUIRE(command.bounds.x >= plan.layout.rows.x);
                    D6R_REQUIRE(command.bounds.x + command.bounds.width <= plan.layout.rows.x + plan.layout.rows.width);
                    D6R_REQUIRE(command.bounds.y + command.bounds.height <= plan.layout.body.y + plan.layout.body.height);
                    if (command.text == "Mode" || command.text == "Predator" || command.text == "Duel 16")
                        D6R_REQUIRE_EQ(16, command.size);
                    if (command.text == "19") D6R_REQUIRE_EQ(32, command.size);
                }
                sameRect(first.layout.notice, plan.layout.notice);
                sameRect(first.layout.footer, plan.layout.footer);
                D6R_REQUIRE_EQ(1, std::count_if(plan.commands.begin(), plan.commands.end(), [](const auto &c) {
                    return c.text == "---SCORE---";
                }));
            }
        }
        D6R_REQUIRE(seen.count("Mode")); D6R_REQUIRE(seen.count("Duel 16"));
        const auto bottom = compose(model, size.first, size.second, {INT_MAX, 0});
        D6R_REQUIRE(hasText(bottom, "Round 123 player statistics"));
        D6R_REQUIRE(hasText(bottom, "---SCORE---"));
        D6R_REQUIRE_EQ(bottom.layout.body.y + 24, findText(bottom, "PTS").bounds.y);
        D6R_REQUIRE_EQ(Color::WHITE, findText(bottom, "1 Cumulative leader").color);
    }
}

D6R_TEST_CASE("Short network panel height follows primary rows not hidden outcome settings or history") {
    for (const auto scope: {Scope::NetworkHost, Scope::NetworkGuest}) {
        auto model = ordinary(scope);
        const auto shortPlan = compose(model, 1280, 900);
        model.settings = {{"Settings", {{"Field"}, {"Value"}}, {}}};
        model.settings.front().rows.resize(100, Row{{"Seed", "123456789"}});
        model.rounds = {{"Round 99 player results", model.cumulative.columns, {}, true}};
        model.rounds.front().rows.resize(150, model.cumulative.rows.front());
        model.matchOutcome.winners.resize(14, Identity{std::string(64, 'x'), "123"});
        const auto withHistory = compose(model, 1280, 900);
        sameRect(shortPlan.layout.panel, withHistory.layout.panel);
        D6R_REQUIRE(withHistory.bounds.vertical > shortPlan.bounds.vertical);
        D6R_REQUIRE_EQ((900 - shortPlan.layout.panel.height) / 2, shortPlan.layout.panel.y);
        model.cumulative.rows.push_back(model.cumulative.rows.front());
        D6R_REQUIRE_EQ(shortPlan.layout.panel.height + 32, compose(model, 1280, 900).layout.panel.height);
    }
}

D6R_TEST_CASE("Fitting compact local table has actual interim width no navigation and no reserved network rows") {
    auto model = ordinary();
    model.cumulative.rows = {{{"Ada", "0", "0", "0", " 0.00", "1"}},
                             {{"Bea", "0", "0", "1", " 0.00", "-1"}},
                             {{"Cora", "0", "0", "1", " 0.00", "-1"}}};
    const auto plan = compose(model, 1280, 900);
    D6R_REQUIRE_EQ(512, plan.layout.panel.width); // OVER-02:(longest4+26)*16+32.
    D6R_REQUIRE_EQ(256, plan.layout.panel.height);
    D6R_REQUIRE_EQ(384, plan.layout.panel.x);
    D6R_REQUIRE_EQ(322, plan.layout.panel.y);
    D6R_REQUIRE_EQ(0, plan.bounds.vertical); D6R_REQUIRE_EQ(0, plan.bounds.horizontal);
    D6R_REQUIRE_EQ(0, plan.layout.navigation.height);
    D6R_REQUIRE_EQ(0, plan.layout.footer.height); D6R_REQUIRE_EQ(0, plan.layout.status.height);
    D6R_REQUIRE_EQ(0, plan.layout.horizontalBack.width);
    D6R_REQUIRE(!hasText(plan, "<")); D6R_REQUIRE(!hasText(plan, ">"));
    D6R_REQUIRE_EQ(plan.layout.panel.x + 21, findText(plan, "Ada").bounds.x);
    D6R_REQUIRE_EQ(plan.layout.panel.y + 96, findText(plan, "Ada").bounds.y);
    D6R_REQUIRE_EQ(32, findText(plan, "Bea").bounds.y - findText(plan, "Ada").bounds.y);
    D6R_REQUIRE_EQ(64, findText(plan, "A").bounds.x - findText(plan, "K").bounds.x - 16);
    // Captions are not stripped from real data by a text-based filter.
    model.cumulative.rows.front().cells[0] = "Match outcome";
    D6R_REQUIRE(hasText(compose(model, 1280, 900), "Match outcome"));
}
