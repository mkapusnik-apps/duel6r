#ifndef DUEL6_MATCH_RESULT_RULES_H
#define DUEL6_MATCH_RESULT_RULES_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Duel6::MatchResultRules {
    // The common prefix of Person and authoritative player ordering. A tie is
    // deliberately unresolved: local play has no roster-order tie-break, whereas
    // network callers must append their authoritative roster-order comparison.
    struct RankingKey {
        std::int64_t points = 0;
        std::uint64_t wins = 0;
        std::uint64_t damage = 0;
    };

    template<typename Points, typename Wins, typename Damage>
    inline int compareRankingValues(Points lp, Wins lw, Damage ld, Points rp, Wins rw, Damage rd) {
        if (lp != rp) return lp > rp ? -1 : 1;
        if (lw != rw) return lw > rw ? -1 : 1;
        if (ld != rd) return ld > rd ? -1 : 1;
        return 0;
    }
    inline int compareRanking(const RankingKey &left, const RankingKey &right) {
        return compareRankingValues(left.points, left.wins, left.damage, right.points, right.wins, right.damage);
    }

    // Team ordering has only this common key. Do not import player tie-breaks
    // into local teams; network teams append fixed Alpha..Delta order on ties.
    inline int compareTeamPoints(std::int64_t left, std::int64_t right) {
        return left == right ? 0 : left > right ? -1 : 1;
    }

    enum class Mode { Deathmatch, Predator, TeamDeathmatch };
    enum class OutcomeKind { Pending, NoWinner, Players, Team };
    struct Survivor {
        std::uint64_t identity = 0;
        std::uint8_t team = 0;
        bool predator = false;
    };
    struct Outcome {
        OutcomeKind kind = OutcomeKind::Pending;
        std::uint8_t team = 0;
        std::vector<std::uint64_t> winners;
    };

    // Call ONLY at the authority's normal active-round evaluation point. This
    // neither awards wins nor writes/finalizes a result. Survivors are supplied in
    // authority order, already filtered for eligibility. Local callers pass true;
    // network callers compute opposition from spawned, non-departed membership
    // (not living players), excluding waiting players. A solo network survivor
    // without opposition remains pending, but zero survivors still means No winner.
    // Never reevaluate an established outcome after departure or during drawing.
    inline Outcome evaluateSurvivors(Mode mode, const std::vector<Survivor> &alive,
                                     bool oppositionPermitsWinner) {
        if (alive.empty()) return {OutcomeKind::NoWinner, 0, {}};
        if (!oppositionPermitsWinner) return {};
        bool finished = false;
        if (mode == Mode::Deathmatch) finished = alive.size() == 1;
        else if (mode == Mode::Predator) {
            finished = alive.size() == 1 || std::none_of(alive.begin(), alive.end(),
                    [](const Survivor &player) { return player.predator; });
        } else {
            finished = std::all_of(alive.begin(), alive.end(), [&alive](const Survivor &player) {
                return player.team == alive.front().team;
            });
        }
        if (!finished) return {};
        Outcome result{mode == Mode::TeamDeathmatch ? OutcomeKind::Team : OutcomeKind::Players,
                       mode == Mode::TeamDeathmatch ? alive.front().team : std::uint8_t(0), {}};
        for (const auto &player: alive) result.winners.push_back(player.identity);
        return result;
    }
}

#endif
