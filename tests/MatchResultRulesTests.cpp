#include "source/MatchResultRules.h"
#include "tests/TestHarness.h"

using namespace Duel6::MatchResultRules;

D6R_TEST_CASE("Common player ranking preserves points wins damage precedence without inventing a local tie break") {
    const RankingKey leader{12, 2, 100};
    D6R_REQUIRE_EQ(-1, compareRanking(leader, {11, 200, 9999}));
    D6R_REQUIRE_EQ(-1, compareRanking(leader, {12, 1, 9999}));
    D6R_REQUIRE_EQ(-1, compareRanking(leader, {12, 2, 99}));
    D6R_REQUIRE_EQ(0, compareRanking(leader, leader));
    D6R_REQUIRE_EQ(1, compareRanking({-10, 3, 400}, {-9, 0, 0}));
    D6R_REQUIRE_EQ(0, compareTeamPoints(8, 8));
    D6R_REQUIRE_EQ(-1, compareTeamPoints(8, -2));
    D6R_REQUIRE_EQ(1, compareTeamPoints(-2, 8));
}

D6R_TEST_CASE("Common terminal rules keep no winner independent of cumulative ranking and solo opposition") {
    for (auto mode: {Mode::Deathmatch, Mode::Predator, Mode::TeamDeathmatch}) {
        for (bool opposition: {false, true}) {
            const auto none = evaluateSurvivors(mode, {}, opposition);
            D6R_REQUIRE_EQ(OutcomeKind::NoWinner, none.kind);
            D6R_REQUIRE(none.winners.empty());
        }
        D6R_REQUIRE_EQ(OutcomeKind::Pending, evaluateSurvivors(mode, {{9, 1, true}}, false).kind);
    }
    // Local single survivor ends a round; network solo without an eligible
    // opposing member does not. The authority supplies that explicit distinction.
    const auto local = evaluateSurvivors(Mode::Deathmatch, {{9, 0, false}}, true);
    D6R_REQUIRE_EQ(OutcomeKind::Players, local.kind);
    D6R_REQUIRE_EQ(std::uint64_t(9), local.winners.front());
    D6R_REQUIRE_EQ(OutcomeKind::Pending,
            evaluateSurvivors(Mode::Deathmatch, {{9, 0, false}, {4, 0, false}}, true).kind);
}

D6R_TEST_CASE("Common Predator rules retain all fourteen surviving marines in authority order") {
    std::vector<Survivor> marines;
    for (std::uint64_t id = 15; id > 1; --id) marines.push_back({id, 0, false});
    auto result = evaluateSurvivors(Mode::Predator, marines, true);
    D6R_REQUIRE_EQ(OutcomeKind::Players, result.kind);
    D6R_REQUIRE_EQ(std::size_t(14), result.winners.size());
    for (std::size_t i = 0; i < marines.size(); ++i) D6R_REQUIRE_EQ(marines[i].identity, result.winners[i]);
    marines.push_back({1, 0, true});
    D6R_REQUIRE_EQ(OutcomeKind::Pending, evaluateSurvivors(Mode::Predator, marines, true).kind);
    result = evaluateSurvivors(Mode::Predator, {{1, 0, true}}, true);
    D6R_REQUIRE_EQ(OutcomeKind::Players, result.kind);
    D6R_REQUIRE_EQ(std::uint64_t(1), result.winners.front());
}

D6R_TEST_CASE("Common team result retains winning team and surviving recipients without awarding dead teammates") {
    const std::vector<Survivor> survivors{{9, 2, false}, {4, 2, false}};
    const auto result = evaluateSurvivors(Mode::TeamDeathmatch, survivors, true);
    D6R_REQUIRE_EQ(OutcomeKind::Team, result.kind);
    D6R_REQUIRE_EQ(std::uint8_t(2), result.team);
    D6R_REQUIRE_EQ(std::size_t(2), result.winners.size());
    D6R_REQUIRE_EQ(std::uint64_t(9), result.winners[0]);
    D6R_REQUIRE_EQ(std::uint64_t(4), result.winners[1]);
    D6R_REQUIRE_EQ(OutcomeKind::Pending,
            evaluateSurvivors(Mode::TeamDeathmatch, {{9, 2, false}, {4, 3, false}}, true).kind);
}

D6R_TEST_CASE("Common outcome evaluation is read only and requires caller owned waiting and departed filtering") {
    const std::vector<Survivor> eligible{{7, 1, false}};
    // A waiting player and a departed opponent are absent from eligible survivors
    // AND cannot establish opposition. Their retained result identities are not
    // inputs to this active-round evaluator at all.
    for (int i = 0; i < 3; ++i) {
        D6R_REQUIRE_EQ(OutcomeKind::Pending, evaluateSurvivors(Mode::Deathmatch, eligible, false).kind);
        D6R_REQUIRE_EQ(std::size_t(1), eligible.size());
        D6R_REQUIRE_EQ(std::uint64_t(7), eligible.front().identity);
    }
}
