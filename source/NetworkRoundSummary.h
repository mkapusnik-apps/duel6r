#ifndef DUEL6_NETWORK_ROUND_SUMMARY_H
#define DUEL6_NETWORK_ROUND_SUMMARY_H

#include "RoundScoreTable.h"
#include "network/StateReplication.h"

namespace Duel6::NetworkRoundSummary {
    struct Row {
        Network::Replication::Identity playerId = 0;
        std::string name, lifecycle;
        RoundScoreTable::Values values;
        Color nameColor, background;
        bool team = false, nested = false, separatorBefore = false;
    };

    // Read-only, authority-ordered cumulative rows. No awards, sorting, saved
    // persons, round-local deltas, or inferred waiting-player participation.
    bool applies(const Network::Replication::CanonicalState &state);
    Color curtainColor(const Network::Replication::CanonicalState &state);
    std::string teamName(unsigned team);
    std::vector<Row> rows(const Network::Replication::CanonicalState &state);

    struct Layout : RoundScoreTable::Layout {
        int phaseY = 0, outcomeY = 0;
        std::vector<std::string> outcomeRows;
        std::size_t first = 0, count = 0, maximumFirst = 0;
    };

    struct Footer {
        int statusY, statusHeight, actionY, phaseY;
    };
    Footer footer(int statusRows);
    Layout layout(const std::vector<Row> &rows, const std::string &outcome,
                  int clientWidth, int clientHeight, std::size_t requestedFirst = 0,
                  int statusRows = 2);
    std::string rowLabel(const Row &row, int availablePixels);
    std::string numericText(const Row &row, const Layout &layout);
    std::vector<RoundScoreTable::Row> visibleRows(const std::vector<Row> &rows, const Layout &layout);
}

#endif
