#ifndef DUEL6_SCORE_SUMMARY_GEOMETRY_H
#define DUEL6_SCORE_SUMMARY_GEOMETRY_H

namespace Duel6::ScoreSummaryGeometry {
    // Actual OVER-02 geometry, shared without importing live ranking/state.
    constexpr int SidePadding = 16;
    constexpr int BaseHeight = 160;
    constexpr int StripTop = 28, StripHeight = 36, StripOverhang = 5;
    constexpr int CaptionTop = 32, ColumnsTop = 64, FirstRowTop = 96;
    constexpr int BottomSpace = 64;

    struct Frame {
        int left, top, width, height;
    };

    inline Frame centered(int tableWidth, int rowHeight, int clientWidth, int clientHeight) {
        const int width = tableWidth + 2 * SidePadding;
        const int height = BaseHeight + rowHeight;
        return {clientWidth / 2 - tableWidth / 2 - SidePadding,
                (clientHeight - height) / 2, width, height};
    }
}

#endif
