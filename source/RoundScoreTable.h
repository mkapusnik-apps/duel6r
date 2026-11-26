#ifndef DUEL6_ROUND_SCORE_TABLE_H
#define DUEL6_ROUND_SCORE_TABLE_H

#include <array>
#include <string>
#include <vector>
#include "Color.h"

namespace Duel6 {
    class Font;
    class Renderer;

    namespace RoundScoreTable {
        constexpr int TextHeight = 32, GlyphWidth = 16, SeparatorHeight = 8;
        constexpr int PlayerNameInset = 5;
        using Columns = std::array<int, 5>;
        using Values = std::array<std::string, 5>;
        constexpr Columns OrdinaryColumns{4, 4, 4, 6, 5}; // Includes each leading |.

        struct Layout {
            // Bottom-left client pixels.
            int left = 0, bottom = 0, width = 0, height = 0;
            int tableLeft = 0, tableWidth = 0, scoreY = 0, firstRowY = 0, nameWidth = 0;
            Columns columnCharacters = OrdinaryColumns;
        };

        struct Row {
            std::string name, numeric;
            Color nameColor, background;
            bool teamHeader, separatorBefore;
        };

        int numericWidth(const Columns &columns);
        int preferredWidth(int nameCharacters, const Columns &columns = OrdinaryColumns);
        Layout layout(int tableWidth, int bodyHeight, int clientWidth, int clientHeight,
                      int minimumBottom = 0, int maximumTop = 0, const Columns &columns = OrdinaryColumns);
        std::string headings(const Layout &layout);
        std::string numericText(const Values &values, const Columns &columns = OrdinaryColumns, bool bounded = false);
        void render(Renderer &renderer, const Font &font, const Layout &layout, const std::vector<Row> &rows);
    }
}

#endif
