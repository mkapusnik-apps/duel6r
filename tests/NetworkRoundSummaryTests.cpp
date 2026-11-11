#include <algorithm>
#include <set>
#include <limits>
#include <sstream>
#include "source/NetworkRoundSummary.h"
#include "source/Person.h"
#include "tests/TestHarness.h"

using namespace Duel6;
namespace S = Duel6::NetworkRoundSummary;
namespace R = Duel6::Network::Replication;

namespace {
    R::CanonicalState summary(unsigned players = 2, unsigned teams = 0) {
        R::CanonicalState state;
        state.phase = R::Phase::RoundSummary; state.currentRoundNumber = 2; state.settings.roundLimit = 3;
        state.roundEndCountdown = 240; state.settings.teamCount = teams;
        state.settings.mode = teams ? "Team deathmatch" : "Deathmatch";
        for (unsigned index = 0; index < players; ++index) {
            R::PlayerState player;
            player.playerId = 100 + index; player.ownerParticipantId = 1 + index % 2;
            player.displayName = "Player " + std::to_string(index);
            player.team = teams ? 1 + index % teams : 0;
            state.players.push_back(player); state.score.ranking.push_back(player.playerId);
            R::ScoreRowState score;
            score.playerId = player.playerId; score.kills = index + 3; score.assists = index + 2;
            score.deaths = index; score.cumulativePoints = 25 - index; score.roundPoints = 999;
            state.score.players.push_back(score);
        }
        for (unsigned team = teams; team > 0; --team) state.score.teamRanking.push_back(team);
        return state;
    }

    std::string trim(std::string value) {
        value.erase(0, value.find_first_not_of(' '));
        return value;
    }
}

D6R_TEST_CASE("Non-final legal numeric extremes keep name lifecycle and every marked cell within bounded columns") {
    for (unsigned teams: {0u, 4u}) {
        auto state = summary(15, teams);
        state.participants = {{1, true, R::ConnectionState::Reconnecting}, {2}};
        for (auto &player: state.players) player.displayName = std::string(64, 'W');
        state.players[1].lifeState = R::LifeState::Departed;
        for (auto &score: state.score.players) {
            score.kills = score.assists = score.deaths = std::numeric_limits<std::uint64_t>::max();
            score.cumulativePoints = std::numeric_limits<std::int64_t>::min();
        }
        const auto rows = S::rows(state);
        if (teams) {
            // Four players on Alpha: sums must not wrap at UInt64 / Int64.
            const auto alpha = std::find_if(rows.begin(), rows.end(), [](const auto &r) { return r.name == "Alpha"; });
            D6R_REQUIRE(alpha != rows.end());
            D6R_REQUIRE_EQ(std::string("73786976294838206460"), alpha->values[0]);
            D6R_REQUIRE_EQ(std::string("73786976294838206460"), alpha->values[1]);
            D6R_REQUIRE_EQ(std::string("73786976294838206460"), alpha->values[2]);
            D6R_REQUIRE_EQ(std::string("1.00"), alpha->values[3]);
            D6R_REQUIRE_EQ(std::string("-36893488147419103232"), alpha->values[4]);
        }
        for (int width: {850, 1280, 1920}) {
            const auto layout = S::layout(rows, "Rounds: 2|3", "Round outcome: No winner", width, 700);
            D6R_REQUIRE(layout.nameWidth >= 16 * 16 + 5); // … plus full Reconnecting cue, with player padding.
            D6R_REQUIRE(layout.tableLeft >= 16);
            for (const auto &row: rows) {
                const int padding = row.team ? 0 : 5;
                const auto label = S::rowLabel(row, layout.nameWidth - padding);
                const auto glyphs = [](const std::string &s) {
                    return int(std::count_if(s.begin(), s.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; }));
                };
                D6R_REQUIRE(glyphs(label) * 16 + padding <= layout.nameWidth);
                D6R_REQUIRE(!label.empty());
                if (!row.lifecycle.empty()) D6R_REQUIRE(label.find(row.lifecycle) != std::string::npos);
                const auto text = S::numericText(row, layout);
                D6R_REQUIRE_EQ(layout.tableWidth - layout.nameWidth, glyphs(text) * 16);
                std::istringstream cells(text.substr(1));
                std::string cell;
                for (std::size_t col = 0; col < row.values.size(); ++col) {
                    D6R_REQUIRE(bool(std::getline(cells, cell, '|')));
                    D6R_REQUIRE_EQ(layout.columnCharacters[col] - 1, glyphs(cell));
                    const auto value = trim(cell);
                    if (int(row.values[col].size()) < layout.columnCharacters[col]) D6R_REQUIRE_EQ(row.values[col], value);
                    else D6R_REQUIRE_EQ(row.values[col].front() == '-' ? std::string("-…") : std::string("…"), value);
                }
            }
        }
    }
    // Exact reviewer reproduction, and ordinary cells sharing its allocation.
    auto state = summary();
    state.score.players[0].kills = state.score.players[0].assists = state.score.players[0].deaths = 1000000000;
    state.score.players[0].cumulativePoints = 2000000000;
    const auto rows = S::rows(state);
    for (int width: {850, 1280}) {
        const auto layout = S::layout(rows, "Rounds: 2|3", "Round outcome: Player 0", width, 700);
        D6R_REQUIRE(layout.nameWidth >= 21);
        const auto text = S::numericText(rows[0], layout);
        D6R_REQUIRE_EQ(width == 850, text.find("…") != std::string::npos);
        D6R_REQUIRE(S::numericText(rows[1], layout).find("…") == std::string::npos);
    }
}

D6R_TEST_CASE("Non-final team display totals retain mixed signed points and zero-death aggregate ratio") {
    auto state = summary(2, 1);
    state.score.players[0].cumulativePoints = std::numeric_limits<std::int64_t>::min();
    state.score.players[1].cumulativePoints = std::numeric_limits<std::int64_t>::max();
    state.score.players[0].deaths = state.score.players[1].deaths = 0;
    const auto rows = S::rows(state);
    D6R_REQUIRE_EQ(std::string("-1"), rows[0].values[4]);
    D6R_REQUIRE_EQ(std::string("7.00"), rows[0].values[3]);
}

D6R_TEST_CASE("Non-final curtain follows Local Play countdown alpha and excludes live and final paths") {
    auto state = summary();
    for (const auto countdown: {360u, 300u, 240u, 180u, 60u, 0u}) {
        state.roundEndCountdown = countdown;
        const auto expected = static_cast<Uint8>(200 - 200 * ((float(countdown) / 60) / D6_GAME_OVER_WAIT));
        D6R_REQUIRE_EQ(Color(128, 0, 0, expected), S::curtainColor(state));
    }
    state.settings.roundLimit = 2; D6R_REQUIRE_EQ(0, int(S::curtainColor(state).getAlpha()));
    state.settings.roundLimit = 3;
    for (auto phase: {R::Phase::ActiveRound, R::Phase::FinalSummary, R::Phase::Lobby}) {
        state.phase = phase; D6R_REQUIRE_EQ(0, int(S::curtainColor(state).getAlpha()));
    }
}

D6R_TEST_CASE("Non-final score columns preserve authoritative order cumulative assists and Local Play ratios") {
    auto state = summary();
    state.score.ranking = {101, 100}; // Deliberately not a UI-sorted order.
    const auto rows = S::rows(state);
    D6R_REQUIRE_EQ(std::size_t(2), rows.size());
    D6R_REQUIRE_EQ(101u, rows[0].playerId);
    D6R_REQUIRE_EQ((std::array<std::string, 5>{"4", "3", "1", "4.00", "24"}), rows[0].values);
    D6R_REQUIRE_EQ((std::array<std::string, 5>{"3", "2", "0", "3.00", "25"}), rows[1].values);
    for (int kills = 0; kills < 80; ++kills) for (int deaths = 0; deaths < 40; ++deaths) {
        state.score.players[0].kills = kills; state.score.players[0].deaths = deaths;
        D6R_REQUIRE_EQ(trim(Person::getKillsToDeathsRatio(kills, deaths)), S::rows(state)[1].values[3]);
    }
    state.score.players[0].cumulativePoints = -7;
    D6R_REQUIRE_EQ(std::string("-7"), S::rows(state)[1].values[4]);
}

D6R_TEST_CASE("Non-final team rows aggregate K A D PTS before calculating the ratio and retain nested ordering") {
    auto state = summary(8, 4);
    state.score.ranking = {107, 106, 105, 104, 103, 102, 101, 100};
    const auto rows = S::rows(state);
    D6R_REQUIRE_EQ(std::size_t(12), rows.size());
    D6R_REQUIRE_EQ(std::string("Delta"), rows[0].name);
    D6R_REQUIRE_EQ(107u, rows[1].playerId); D6R_REQUIRE_EQ(103u, rows[2].playerId);
    D6R_REQUIRE_EQ((std::array<std::string, 5>{"16", "14", "10", "1.60", "40"}), rows[0].values);
    D6R_REQUIRE(rows[0].team && !rows[0].nested && !rows[0].separatorBefore);
    D6R_REQUIRE_EQ(Color::BLACK, rows[0].nameColor);
    D6R_REQUIRE_EQ(Color(255, 0, 255, 178), rows[0].background);
    D6R_REQUIRE(rows[1].nested && !rows[1].team);
    D6R_REQUIRE_EQ(Color(51, 0, 51, 178), rows[1].background);
    D6R_REQUIRE_EQ(3, std::count_if(rows.begin(), rows.end(), [](const auto &r) { return r.separatorBefore; }));
    D6R_REQUIRE(!rows.back().separatorBefore);
    D6R_REQUIRE_EQ(std::string("Alpha"), rows[9].name);
}

D6R_TEST_CASE("Non-final lifecycle rows retain waiting departed and reconnecting identity without invented scores") {
    auto state = summary(4);
    state.participants = {{1, true, R::ConnectionState::Reconnecting}, {2}};
    state.players[1].lifeState = R::LifeState::Waiting;
    state.players[2].lifeState = R::LifeState::Departed;
    state.players[3].lifeState = R::LifeState::Dead;
    state.score.players[1] = {}; state.score.players[1].playerId = 101;
    const auto rows = S::rows(state);
    D6R_REQUIRE_EQ(std::string("Reconnecting"), rows[0].lifecycle);
    D6R_REQUIRE_EQ(std::string("Waiting"), rows[1].lifecycle);
    D6R_REQUIRE_EQ((std::array<std::string, 5>{"0", "0", "0", "0.00", "0"}), rows[1].values);
    D6R_REQUIRE_EQ(Color::WHITE, rows[1].nameColor);
    D6R_REQUIRE_EQ(std::string("Departed"), rows[2].lifecycle);
    D6R_REQUIRE_EQ(std::string("Dead"), rows[3].lifecycle);
    D6R_REQUIRE_EQ(Color::RED, rows[3].nameColor);
    D6R_REQUIRE_EQ(Color(0, 0, 255, 178), rows[3].background);
    D6R_REQUIRE_EQ(102u, rows[2].playerId);
}

D6R_TEST_CASE("Non-final ordinary score allocation matches Local Play padding typography and centered surfaces") {
    auto state = summary();
    state.players[0].displayName = "Maple"; state.players[1].displayName = "Oak";
    const auto rows = S::rows(state);
    const auto layout = S::layout(rows, "Rounds: 1|2", "Round outcome: Maple", 1280, 900);
    D6R_REQUIRE_EQ(496, layout.tableWidth); // (5 + 26) * 16, from OVER-02.
    D6R_REQUIRE_EQ(528, layout.width);
    D6R_REQUIRE_EQ(256, layout.height);
    D6R_REQUIRE_EQ((900 - layout.height) / 2, layout.bottom);
    D6R_REQUIRE_EQ((1280 - layout.width) / 2, layout.left);
    D6R_REQUIRE_EQ(16, layout.tableLeft - layout.left);
    D6R_REQUIRE_EQ(std::string(" K    A   D  K/D  PTS  "), S::headings(layout));
    D6R_REQUIRE_EQ(std::string("|  3|  2|  0| 3.00|  25"), S::numericText(rows[0], layout));
    D6R_REQUIRE_EQ(std::size_t(0), layout.maximumFirst);
}

D6R_TEST_CASE("Non-final constrained team body keeps fixed headings all rows reachable and local separator pitch") {
    const auto rows = S::rows(summary(15, 4));
    for (const auto &size: {std::pair{850, 700}, std::pair{1280, 720}, std::pair{1280, 900}}) {
        std::set<std::size_t> reached;
        const auto initial = S::layout(rows, "Rounds: 2|3", "Round outcome: Team 4", size.first, size.second);
        D6R_REQUIRE(initial.maximumFirst > 0);
        for (std::size_t first = 0; first <= initial.maximumFirst; ++first) {
            const auto layout = S::layout(rows, "Rounds: 2|3", "Round outcome: Team 4", size.first, size.second, first);
            D6R_REQUIRE_EQ(initial.scoreY, layout.scoreY);
            D6R_REQUIRE(layout.left - 5 >= 16 && layout.left + layout.width + 5 <= size.first - 16);
            D6R_REQUIRE(layout.bottom + layout.height <= size.second - 16);
            D6R_REQUIRE(layout.phaseY >= 134);
            D6R_REQUIRE(layout.outcomeY + 16 + 16 <= layout.bottom);
            int y = layout.firstRowY;
            for (auto index = first; index < first + layout.count; ++index) {
                if (index > first && rows[index].separatorBefore) y -= 8;
                D6R_REQUIRE(y >= layout.bottom + 64);
                D6R_REQUIRE(y + 32 <= layout.scoreY - 32);
                reached.insert(index); y -= 32;
            }
        }
        D6R_REQUIRE_EQ(rows.size(), reached.size());
        const auto last = S::layout(rows, "Rounds: 2|3", "Round outcome: Team 4", size.first, size.second, 999);
        D6R_REQUIRE_EQ(rows.size(), last.first + last.count);
    }
    const auto eight = S::layout(S::rows(summary(8, 4)), "Rounds: 2|3", "Round outcome: Team 4", 1280, 900);
    D6R_REQUIRE_EQ(192 + 12 * 32 + 3 * 8, eight.height);
    D6R_REQUIRE_EQ(std::size_t(0), eight.maximumFirst);
}

D6R_TEST_CASE("Non-final long names and outcomes stay contained without shrinking score rows or losing numeric fields") {
    auto state = summary(15, 4);
    for (auto &player: state.players) player.displayName = std::string(52, 'W') + "éééééé";
    state.players[0].lifeState = R::LifeState::Departed;
    state.score.players[0].kills = 12345; state.score.players[0].assists = 23456;
    state.score.players[0].deaths = 34567; state.score.players[0].cumulativePoints = -45678;
    const auto rows = S::rows(state);
    for (const auto &size: {std::pair{850, 700}, std::pair{1280, 720}}) {
        const auto shortOutcome = S::layout(rows, "Rounds: 2|3", "Round outcome: No winner", size.first, size.second);
        const auto layout = S::layout(rows, "Rounds: 2|3", "Round outcome: " + std::string(350, 'X'), size.first, size.second);
        D6R_REQUIRE(layout.count < shortOutcome.count);
        D6R_REQUIRE(layout.outcomeRows.size() > 1);
        for (const auto &row: rows) {
            const auto label = S::rowLabel(row, layout.nameWidth - 5);
            D6R_REQUIRE(label.find("…") != std::string::npos || row.team);
            if (!row.lifecycle.empty()) D6R_REQUIRE(label.find(" • Departed") != std::string::npos);
            const auto text = S::numericText(row, layout);
            D6R_REQUIRE_EQ(5, std::count(text.begin(), text.end(), '|'));
            D6R_REQUIRE_EQ(std::size_t(layout.tableWidth - layout.nameWidth), text.size() * 16);
        }
        D6R_REQUIRE(layout.bottom >= layout.outcomeY + int(layout.outcomeRows.size()) * 18 + 16);
    }
}

D6R_TEST_CASE("Non-final parity dispatch excludes active Tab final delay final results and unlimited legacy summaries") {
    auto state = summary();
    for (const auto countdown: {360u, 300u, 1u}) { state.roundEndCountdown = countdown; D6R_REQUIRE(S::applies(state)); }
    state.settings.roundLimit = 2; D6R_REQUIRE(!S::applies(state));
    state.settings.roundLimit = 0; D6R_REQUIRE(!S::applies(state));
    state.settings.roundLimit = 3;
    for (const auto phase: {R::Phase::ActiveRound, R::Phase::FinalSummary, R::Phase::Lobby}) {
        state.phase = phase; D6R_REQUIRE(!S::applies(state));
    }
}
