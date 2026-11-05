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
        result.cumulative.columns = {{"Rank / Player"}, {"K", true}, {"A", true}, {"D", true},
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
            header.teamColor = Color(255, 0, 0);
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

D6R_TEST_CASE("Local and network finals share the complete client pixel shell at all approved desktop sizes") {
    for (const auto size: {std::pair<int, int>{850, 700}, {1280, 720}, {1280, 900}, {1920, 1080}}) {
        const auto local = compose(ordinary(), size.first, size.second);
        const auto host = compose(ordinary(Scope::NetworkHost), size.first, size.second);
        const auto guest = compose(ordinary(Scope::NetworkGuest), size.first, size.second);
        for (const auto *other: {&host, &guest}) {
            sameRect(local.layout.panel, other->layout.panel);
            sameRect(local.layout.body, other->layout.body);
            sameRect(local.layout.title, other->layout.title);
            sameRect(local.layout.counter, other->layout.counter);
            sameRect(local.layout.footer, other->layout.footer);
            sameRect(local.layout.notice, other->layout.notice);
        }
        D6R_REQUIRE_EQ(16, local.layout.panel.y);
        D6R_REQUIRE_EQ(size.second - 136, local.layout.panel.y + local.layout.panel.height);
        D6R_REQUIRE(local.layout.panel.x >= 16);
        D6R_REQUIRE(local.layout.panel.width <= 1184);
        D6R_REQUIRE_EQ(16, local.layout.footer.y - local.layout.panel.y - local.layout.panel.height);
        D6R_REQUIRE_EQ(16, local.layout.notice.y - local.layout.footer.y - local.layout.footer.height);
        D6R_REQUIRE_EQ(16, size.second - local.layout.notice.y - local.layout.notice.height);
        const auto &score = findText(local, "SCORE");
        D6R_REQUIRE(score.bounds.x + score.bounds.width + 16 <= local.layout.counter.x);
        const auto &counter = findText(local, "Rounds: 123 | 123");
        D6R_REQUIRE_EQ(counter.bounds.width + 16, local.layout.counter.width);
        D6R_REQUIRE_EQ(32, score.size);
        D6R_REQUIRE_EQ(32, findText(local, "End of Game").size);
        D6R_REQUIRE_EQ(Color(128, 0, 0, 200), local.commands.front().color);
    }
}

D6R_TEST_CASE("Final scope and outcome text never promote cumulative leader or claim a local save") {
    auto model = ordinary();
    const auto local = compose(model, 1280, 720);
    D6R_REQUIRE(hasText(local, "Local Play"));
    D6R_REQUIRE(hasText(local, "Match outcome: Final winner"));
    D6R_REQUIRE(hasText(local, "Last completed round: 123 / Final winner"));
    D6R_REQUIRE(hasText(local, "Cumulative ranking"));
    D6R_REQUIRE(hasText(local, "1 Cumulative leader"));
    for (const auto &command: local.commands) {
        D6R_REQUIRE(command.text.find("Session only") == std::string::npos);
        D6R_REQUIRE(command.text.find("saved") == std::string::npos);
        if (command.kind == Command::Kind::Text) D6R_REQUIRE_EQ(Color(255), command.color);
    }
    model.scope = Scope::NetworkGuest;
    model.matchOutcome = {}; model.lastRoundOutcome = {};
    model.cumulative.rows.resize(1); // permitted solo result, not a new winner
    const auto solo = compose(model, 850, 700);
    D6R_REQUIRE(hasText(solo, "Completed / Session only / Waiting for host to return to lobby or end session"));
    D6R_REQUIRE(hasText(solo, "Not saved to local statistics or Elo"));
    D6R_REQUIRE(hasText(solo, "Match outcome: No winner"));
    D6R_REQUIRE(hasText(solo, "Last completed round: 123 / No winner"));
    D6R_REQUIRE_EQ(std::size_t(1), solo.bounds.totalRows);
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
    D6R_REQUIRE_EQ(first.x, guest.actions[0].bounds.x);
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
    D6R_REQUIRE_EQ(0, plan.bounds.vertical);
    for (int i = 1; i <= 15; ++i) D6R_REQUIRE(hasText(plan, "Profile " + std::to_string(i)));
    const auto &team = findText(plan, "1 Alpha");
    const auto &player = findText(plan, "Profile 1");
    D6R_REQUIRE_EQ(18, player.bounds.y - team.bounds.y);
    D6R_REQUIRE(player.bounds.x > team.bounds.x);
    D6R_REQUIRE_EQ(26, findText(plan, "2 Bravo").bounds.y - findText(plan, "Profile 4").bounds.y);
    std::size_t separators = 0;
    for (const auto &command: plan.commands) if (command.color == Color(255, 255, 255, 178)) {
        ++separators;
        D6R_REQUIRE_EQ(2, command.bounds.height);
    }
    D6R_REQUIRE_EQ(std::size_t(3), separators);
    const auto minimum = compose(model, 1280, 720);
    D6R_REQUIRE(minimum.bounds.vertical > 0);
    const auto bottom = compose(model, 1280, 720, {INT_MAX, INT_MAX});
    D6R_REQUIRE(hasText(bottom, "Profile 15"));
    D6R_REQUIRE(hasText(bottom, "K/D"));
    D6R_REQUIRE(hasText(bottom, "PTS"));
    D6R_REQUIRE(hasText(bottom, "Cumulative ranking"));
    sameRect(minimum.layout.footer, bottom.layout.footer);
    sameRect(minimum.layout.notice, bottom.layout.notice);
}

D6R_TEST_CASE("Fourteen departed Predator winners have complete scoped outcome rows before ranking") {
    auto model = ordinary(Scope::NetworkGuest);
    model.matchOutcome = {false, "", {}};
    for (int i = 1; i <= 14; ++i)
        model.matchOutcome.winners.push_back({"Marine " + std::to_string(i), std::to_string(100 + i), "", i == 14});
    model.lastRoundOutcome = model.matchOutcome;
    const auto plan = compose(model, 1280, 720);
    D6R_REQUIRE(hasText(plan, "Match outcome: 14 winners / See outcome rows"));
    D6R_REQUIRE(hasText(plan, "Last completed round: 123 / 14 winners / See outcome rows"));
    D6R_REQUIRE(hasText(plan, "Outcome identities"));
    D6R_REQUIRE_EQ(std::size_t(30), plan.bounds.totalRows);
    std::set<std::string> seen;
    for (int y = 0; y <= plan.bounds.vertical + 18; y += 18) {
        const auto page = compose(model, 1280, 720, {y, 0});
        for (const auto &command: page.commands) if (command.kind == Command::Kind::Text) seen.insert(command.text);
        D6R_REQUIRE(hasText(page, "Match outcome: 14 winners / See outcome rows"));
        D6R_REQUIRE(hasText(page, "Not saved to local statistics or Elo"));
    }
    for (int i = 1; i <= 14; ++i) {
        D6R_REQUIRE(seen.count("Marine " + std::to_string(i)));
        D6R_REQUIRE(seen.count(std::to_string(100 + i)));
    }
    D6R_REQUIRE(seen.count("Departed"));
    D6R_REQUIRE(seen.count("Match outcome"));
    D6R_REQUIRE(seen.count("Cumulative ranking"));
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
    D6R_REQUIRE(hasText(last, "Cumulative ranking"));
    const auto &points = findText(last, "19");
    const auto &heading = findText(last, "PTS");
    D6R_REQUIRE_EQ(points.bounds.x + points.bounds.width, heading.bounds.x + heading.bounds.width);
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
    D6R_REQUIRE(hasText(top, "Cumulative ranking"));
    D6R_REQUIRE(hasText(bottom, "Per-round results"));
    D6R_REQUIRE(hasText(bottom, "Assisted damage"));
    D6R_REQUIRE(hasText(bottom, "50"));
    D6R_REQUIRE_EQ(top.layout.body.y + 42, findText(bottom, "Assisted damage").bounds.y);
    D6R_REQUIRE(hasText(bottom, "Completed / Session only"));
    D6R_REQUIRE(hasText(bottom, "Match outcome: Final winner"));
    D6R_REQUIRE_EQ(std::size_t(54), bottom.bounds.totalRows);
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
        D6R_REQUIRE(hasText(plan, "Cumulative ranking"));
    }
    for (const auto &value: values) D6R_REQUIRE(observed.count(value));
    for (const auto &column: model.cumulative.columns) D6R_REQUIRE(observed.count(column.heading));
    D6R_REQUIRE_EQ(values, model.cumulative.rows.front().cells);
    D6R_REQUIRE_EQ(std::size_t(1), initial.bounds.totalRows);
}
