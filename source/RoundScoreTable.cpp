#include "RoundScoreTable.h"
#include "Font.h"
#include "ScoreSummaryGeometry.h"
#include "renderer/Renderer.h"

#include <algorithm>
#include <numeric>

namespace Duel6::RoundScoreTable {
    int numericWidth(const Columns &columns) {
        return std::accumulate(columns.begin(), columns.end(), 0) * GlyphWidth;
    }

    int preferredWidth(int nameCharacters, const Columns &columns) {
        return (nameCharacters + 3) * GlyphWidth + numericWidth(columns);
    }

    Layout layout(int tableWidth, int bodyHeight, int clientWidth, int clientHeight,
                  int minimumBottom, int maximumTop, const Columns &columns) {
        namespace G = ScoreSummaryGeometry;
        const auto frame = G::centered(std::max(tableWidth, 200), bodyHeight, clientWidth, clientHeight);
        // Convert from the local table's top-left centering to client drawing coordinates.
        int bottom = clientHeight - frame.top - frame.height;
        if (maximumTop > 0) bottom = std::min(bottom, maximumTop - frame.height);
        if (minimumBottom > 0) bottom = std::max(bottom, minimumBottom);
        const int top = bottom + frame.height;
        return {frame.left, bottom, frame.width, frame.height,
                clientWidth / 2 - tableWidth / 2, tableWidth,
                top - G::CaptionTop - TextHeight, top - G::FirstRowTop - TextHeight,
                tableWidth - numericWidth(columns), columns};
    }

    std::string headings(const Layout &layout) {
        static const char *parts[] = {" K  ", "  A ", "  D ", " K/D  ", "PTS  "};
        std::string result;
        for (std::size_t col = 0; col < OrdinaryColumns.size(); ++col) {
            result.append(layout.columnCharacters[col] - OrdinaryColumns[col], ' ');
            result += parts[col];
        }
        return result;
    }

    std::string numericText(const Values &values, const Columns &columns, bool bounded) {
        std::string text;
        for (std::size_t col = 0; col < values.size(); ++col) {
            const int available = columns[col] - 1;
            const auto &value = values[col];
            text += '|';
            if (bounded && int(value.size()) > available) {
                const bool negative = value.front() == '-';
                text.append(available - (negative ? 2 : 1), ' ');
                text += negative ? "-…" : "…";
            } else {
                text.append(std::max(0, available - int(value.size())), ' ');
                text += value;
            }
        }
        return text;
    }

    void render(Renderer &renderer, const Font &font, const Layout &layout, const std::vector<Row> &rows) {
        using namespace ScoreSummaryGeometry;
        renderer.setBlendFunc(BlendFunc::SrcAlpha);
        renderer.quadXY(Vector(layout.left, layout.bottom), Vector(layout.width, layout.height),
                        Color(255, 255, 255, 80));
        renderer.quadXY(Vector(layout.left + 2, layout.bottom + 2), Vector(layout.width - 4, layout.height - 4),
                        Color(0, 0, 255, 80));
        renderer.quadXY(Vector(layout.left - StripOverhang, layout.scoreY),
                        Vector(layout.width + 2 * StripOverhang, StripHeight), Color::BLUE);
        const auto title = "---SCORE---";
        const auto columns = headings(layout);
        const int numericX = layout.tableLeft + layout.nameWidth;
        font.print(layout.left + (layout.width - font.getTextWidth(title, TextHeight)) / 2,
                   layout.scoreY, 0, Color::WHITE, title, TextHeight);
        font.print(numericX, layout.scoreY - TextHeight, 0, Color::WHITE, columns, TextHeight);

        int rowY = layout.firstRowY;
        for (std::size_t index = 0; index < rows.size(); ++index) {
            const auto &row = rows[index];
            renderer.setBlendFunc(BlendFunc::SrcAlpha);
            if (index != 0 && row.separatorBefore) {
                rowY -= SeparatorHeight;
                renderer.quadXY(Vector(layout.tableLeft, rowY + TextHeight + SeparatorHeight / 2),
                                Vector(layout.tableWidth, 2), Color(255, 255, 255, 178));
            }
            renderer.quadXY(Vector(layout.tableLeft, rowY + 1), Vector(layout.tableWidth, TextHeight), row.background);
            font.print(layout.tableLeft + (row.teamHeader ? 0 : PlayerNameInset),
                       rowY, 0, row.nameColor, row.name, TextHeight);
            font.print(numericX, rowY, 0, row.teamHeader ? row.nameColor : Color::YELLOW, row.numeric, TextHeight);
            rowY -= TextHeight;
        }
    }
}
