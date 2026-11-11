#ifndef DUEL6_NETWORK_ROUND_SUMMARY_H
#define DUEL6_NETWORK_ROUND_SUMMARY_H

#include <array>
#include "Color.h"
#include "network/StateReplication.h"

namespace Duel6::NetworkRoundSummary {
    constexpr int TextHeight = 32, GlyphWidth = 16, SeparatorHeight = 8;

    struct Row {
        Network::Replication::Identity playerId = 0;
        std::string name, lifecycle;
        std::array<std::string, 5> values;
        Color nameColor, background;
        bool team = false, nested = false, separatorBefore = false;
    };

    // Read-only, authority-ordered cumulative rows. No awards, sorting, saved
    // persons, round-local deltas, or inferred waiting-player participation.
    bool applies(const Network::Replication::CanonicalState &state);
    Color curtainColor(const Network::Replication::CanonicalState &state);
    std::vector<Row> rows(const Network::Replication::CanonicalState &state);

    struct Layout {
        // Bottom-left client pixels, matching the unchanged Local Play panel.
        int left = 0, bottom = 0, width = 0, height = 0;
        int tableLeft = 0, tableWidth = 0, scoreY = 0, firstRowY = 0;
        int nameWidth = 0, phaseY = 0, outcomeY = 0;
        std::array<int, 5> columnCharacters{}; // Includes each leading |.
        std::vector<std::string> outcomeRows;
        std::size_t first = 0, count = 0, maximumFirst = 0;
    };

    Layout layout(const std::vector<Row> &rows, const std::string &progress,
                  const std::string &outcome, int clientWidth, int clientHeight,
                  std::size_t requestedFirst = 0);
    std::string rowLabel(const Row &row, int availablePixels);
    std::string numericText(const Row &row, const Layout &layout);
    std::string headings(const Layout &layout);
}

#endif
