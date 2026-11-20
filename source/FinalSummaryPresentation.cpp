#include "FinalSummaryPresentation.h"

#include <algorithm>
#include <limits>
#include "Font.h"
#include "renderer/Renderer.h"
#include "ScoreSummaryGeometry.h"

namespace Duel6::FinalSummaryPresentation {
    namespace {
        constexpr int Glyph = 8, Pitch = 18, HeaderHeight = 64, NavigationHeight = 36;
        const Color White(255), Black(0), Header(0, 0, 255);

        std::vector<std::size_t> boundaries(const std::string &text) {
            std::vector<std::size_t> result;
            for (std::size_t i = 0; i < text.size(); ++i)
                if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) result.push_back(i);
            result.push_back(text.size());
            return result;
        }

        int textWidth(const std::string &text, int height = 16) {
            return static_cast<int>(boundaries(text).size() - 1) * height / 2;
        }

        void fill(Plan &plan, Rect rect, Color color) {
            if (rect.width > 0 && rect.height > 0)
                plan.commands.push_back({Command::Kind::Fill, rect, color, {}, 0});
        }

        void frame(Plan &plan, Rect rect, Color color, int width = 2) {
            plan.commands.push_back({Command::Kind::Frame, rect, color, {}, width});
        }

        void text(Plan &plan, int x, int y, const std::string &value, int height = 16, Color color = White) {
            if (!value.empty()) plan.commands.push_back({Command::Kind::Text,
                    {x, y, textWidth(value, height), height}, color, value, height});
        }

        // Clip whole glyphs, never bytes. We intentionally do NOT add ellipses:
        // horizontal navigation reveals the original complete cell verbatim.
        void clippedText(Plan &plan, int x, int y, const std::string &value, const Rect &clip,
                         int height = 16, Color color = White) {
            if (y < clip.y || y + height > clip.y + clip.height) return;
            const int glyph = height / 2;
            const auto offsets = boundaries(value);
            const int count = static_cast<int>(offsets.size()) - 1;
            const int first = std::clamp((clip.x - x + glyph - 1) / glyph, 0, count);
            const int end = std::clamp((clip.x + clip.width - x) / glyph, first, count);
            text(plan, x + first * glyph, y, value.substr(offsets[first], offsets[end] - offsets[first]), height, color);
        }

        Rect intersection(Rect a, Rect b) {
            const int right = std::min(a.x + a.width, b.x + b.width);
            const int bottom = std::min(a.y + a.height, b.y + b.height);
            a.x = std::max(a.x, b.x); a.y = std::max(a.y, b.y);
            a.width = std::max(0, right - a.x); a.height = std::max(0, bottom - a.y);
            return a;
        }

        int scoreColumnWidth(const std::string &heading) {
            if (heading == "K" || heading == "A" || heading == "D") return 64;
            if (heading == "K/D") return 96;
            if (heading == "PTS") return 80;
            return 0;
        }

        int scoreHeadingInset(const std::string &heading) {
            if (heading == "K" || heading == "K/D") return 16;
            if (heading == "A" || heading == "D") return 32;
            return 0;
        }

        struct MeasuredTable {
            const Table *table = nullptr;
            std::vector<int> widths;
            std::vector<int> rowTops;
            int top = 0, height = 0, width = 0, header = HeaderHeight, textHeight = 16, pitch = Pitch;
        };

        std::vector<MeasuredTable> measure(const std::vector<Table> &tables, int availableWidth,
                                            ScrollBounds &bounds) {
            std::vector<MeasuredTable> result;
            for (std::size_t index = 0; index < tables.size(); ++index) {
                const auto &table = tables[index];
                MeasuredTable measured;
                measured.table = &table;
                measured.top = bounds.contentHeight;
                if (table.score) {
                    measured.header = index == 0 ? 32 : 56;
                    measured.textHeight = measured.pitch = 32;
                }
                measured.height = measured.header;
                for (std::size_t i = 0; i < table.columns.size(); ++i) {
                    const auto &column = table.columns[i];
                    measured.widths.push_back(std::max({column.minimumWidth,
                            table.score && column.numeric ? scoreColumnWidth(column.heading) : 0,
                            textWidth(column.heading, measured.textHeight)
                                + (table.score && i == 0 ? 48 : 16)}));
                }
                for (const auto &row: table.rows) {
                    if (row.teamSeparatorBefore) measured.height += 8;
                    measured.rowTops.push_back(measured.height);
                    measured.height += measured.pitch;
                    for (std::size_t i = 0; i < row.cells.size() && i < measured.widths.size(); ++i) {
                        const int inset = table.score && i == 0 ? 32
                                : !table.score && i == 0 && row.teamColor ? 20 : 0;
                        measured.widths[i] = std::max(measured.widths[i],
                                textWidth(row.cells[i], measured.textHeight) + 16 + inset);
                    }
                }
                for (int width: measured.widths) measured.width += width;
                // Identity receives remaining width AFTER all numeric columns.
                if (!table.score && !measured.widths.empty() && measured.width < availableWidth) {
                    auto identity = std::find_if(table.columns.begin(), table.columns.end(),
                            [](const Column &column) { return !column.numeric; });
                    const auto index = identity == table.columns.end() ? 0 : identity - table.columns.begin();
                    measured.widths[index] += availableWidth - measured.width;
                    measured.width = availableWidth;
                }
                measured.width = std::max(measured.width, textWidth(table.title) + 16);
                if (table.score) measured.width = std::max(measured.width, textWidth("---SCORE---", 32));
                bounds.contentWidth = std::max(bounds.contentWidth, measured.width);
                bounds.contentHeight += measured.height;
                bounds.totalRows += table.rows.size();
                result.push_back(std::move(measured));
            }
            return result;
        }

        const char *caption(Action action) {
            switch (action) {
                case Action::ReturnToLobby: return "Return to lobby";
                case Action::EndSession: return "End session";
                case Action::Leave: return "Leave";
            }
            return "";
        }

        void button(Plan &plan, Rect rect, const std::string &caption, bool enabled,
                    bool focused, bool pressed) {
            fill(plan, rect, Color(192));
            if (enabled) {
                const Color light = pressed ? Black : Color(235);
                const Color dark = pressed ? Color(235) : Black;
                fill(plan, {rect.x, rect.y, rect.width, 2}, light);
                fill(plan, {rect.x, rect.y, 2, rect.height}, light);
                fill(plan, {rect.x, rect.y + rect.height - 2, rect.width, 2}, dark);
                fill(plan, {rect.x + rect.width - 2, rect.y, 2, rect.height}, dark);
            } else frame(plan, rect, Black, 1);
            if (focused) frame(plan, {rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4}, Black);
            text(plan, rect.x + (rect.width - textWidth(caption)) / 2 + int(pressed),
                 rect.y + (rect.height - 16) / 2 + int(pressed), caption, 16, Black);
        }

        int clampedOffset(std::int64_t value, int maximum) {
            return static_cast<int>(std::clamp<std::int64_t>(value, 0, maximum));
        }
    }

    bool Rect::contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }

    Plan compose(const Model &model, int clientWidth, int clientHeight, Scroll requested) {
        Plan plan;
        plan.scope = model.scope;
        auto &layout = plan.layout;
        layout.width = clientWidth; layout.height = clientHeight;
        const bool network = model.scope != Scope::Local;
        const int noticeWidth = textWidth("End of Game", 32) + 32;
        layout.notice = {(clientWidth - noticeWidth) / 2, clientHeight - 64, noticeWidth, 48};
        const std::string counter = "Rounds: " + std::to_string(model.completedRounds) + " | " + std::to_string(model.roundLimit);
        const int counterWidth = textWidth(counter, 32) + 16;
        layout.counter = {(clientWidth - counterWidth) / 2, layout.notice.y - 56, counterWidth, 40};
        const std::vector<Action> allowed = model.scope == Scope::NetworkHost
                ? std::vector<Action>{Action::ReturnToLobby, Action::EndSession}
                : model.scope == Scope::NetworkGuest ? std::vector<Action>{Action::Leave} : std::vector<Action>{};
        std::vector<std::string> status;
        int clearBottom = layout.counter.y - 16;
        if (network) {
            status = {"Completed / Session only", "Not saved to local statistics or Elo"};
            if (model.scope == Scope::NetworkGuest)
                status.insert(status.begin(), "Waiting for host to return to lobby or end session");
            const int actionWidth = model.scope == Scope::NetworkHost ? 436 : 200;
            layout.footer = {(clientWidth - actionWidth) / 2, layout.counter.y - 56, actionWidth, 40};
            layout.status = {16, layout.footer.y - 16 - int(status.size()) * Pitch,
                             clientWidth - 32, int(status.size()) * Pitch};
            clearBottom = layout.status.y - 16;
        }

        std::vector<Table> tables{model.cumulative};
        tables.back().score = true;
        if (network) {
            // Keep the established match outcome after cumulative scores. Last
            // round scope/number remains in the adapter's existing round detail.
            Table outcomes{"Outcome identities", {{"Outcome scope"}, {"Complete display name"}, {"Player identity"},
                                                  {"Team"}, {"Departure"}}, {}};
            const auto &outcome = model.matchOutcome;
            if (outcome.noWinner) outcomes.rows.push_back({{"Match", "No winner", "", "", ""}});
            if (!outcome.winningTeam.empty())
                outcomes.rows.push_back({{"Match", outcome.winningTeam, "", outcome.winningTeam, ""}});
            for (const auto &winner: outcome.winners)
                outcomes.rows.push_back({{"Match", winner.name, winner.player, winner.team,
                                          winner.departed ? "Departed" : ""}});
            if (!outcomes.rows.empty()) tables.push_back(std::move(outcomes));
            tables.insert(tables.end(), model.settings.begin(), model.settings.end());
            tables.insert(tables.end(), model.rounds.begin(), model.rounds.end());
        }
        // Measure content before allocating its frame. Hidden network history
        // changes scroll bounds, never the primary score panel's desired height.
        const int maximumWidth = std::max(1, clientWidth - 74);
        auto measured = measure(tables, maximumWidth, plan.bounds);
        const int viewportWidth = std::min(maximumWidth, measured.front().width);
        plan.bounds = {};
        measured = measure(tables, viewportWidth, plan.bounds);
        const int primaryHeight = measured.front().height;
        // A solo network result still needs one complete row under the tallest
        // supporting header. This minimum is independent of history length.
        const int desiredViewport = network ? std::max(primaryHeight, 88) : primaryHeight;
        const int maximumHeight = std::max(1, clearBottom - 16);
        const bool navigation = plan.bounds.contentHeight > desiredViewport
                || plan.bounds.contentWidth > viewportWidth || desiredViewport + 128 > maximumHeight;
        const int navHeight = navigation ? NavigationHeight : 0;
        auto geometry = ScoreSummaryGeometry::centered(viewportWidth,
                desiredViewport - 32 + navHeight, clientWidth, clientHeight);
        geometry.height = std::min(geometry.height, maximumHeight);
        geometry.top = std::min((clientHeight - geometry.height) / 2, clearBottom - geometry.height);
        layout.panel = {geometry.left, geometry.top, geometry.width, geometry.height};
        const auto &panel = layout.panel;
        layout.title = {panel.x - ScoreSummaryGeometry::StripOverhang,
                        panel.y + ScoreSummaryGeometry::StripTop,
                        panel.width + 2 * ScoreSummaryGeometry::StripOverhang,
                        ScoreSummaryGeometry::StripHeight};
        layout.body = {panel.x + ScoreSummaryGeometry::SidePadding,
                       panel.y + ScoreSummaryGeometry::ColumnsTop, viewportWidth,
                       std::max(0, panel.height - 128)};
        const int documentViewportHeight = std::max(0, layout.body.height - navHeight);
        if (navigation) layout.navigation = {layout.body.x, layout.body.y + documentViewportHeight,
                                            layout.body.width, 24};
        plan.bounds.vertical = std::max(0, plan.bounds.contentHeight - documentViewportHeight);
        plan.bounds.horizontal = std::max(0, plan.bounds.contentWidth - layout.body.width);
        // Fixed 18px steps can skip the entire visibility interval of a 32px
        // row under a 56px sticky header in a compact 96px viewport. Retain the
        // normal step size but visit each row's exact alignment in both
        // directions, without enlarging the panel or drawing partial rows.
        std::vector<int> alignments{0, plan.bounds.vertical};
        for (const auto &table: measured)
            for (const int rowTop: table.rowTops)
                alignments.push_back(clampedOffset(table.top + rowTop - table.header, plan.bounds.vertical));
        std::sort(alignments.begin(), alignments.end());
        for (const int alignment: alignments)
            while (plan.bounds.verticalPositions.back() < alignment)
                plan.bounds.verticalPositions.push_back(std::min(alignment, plan.bounds.verticalPositions.back() + Pitch));
        if (network && plan.bounds.horizontal) {
            layout.horizontalBack = {layout.navigation.x, layout.navigation.y, 32, 24};
            layout.horizontalForward = {layout.navigation.x + layout.navigation.width - 32,
                                         layout.navigation.y, 32, 24};
        }
        plan.scroll = {clampedOffset(requested.vertical, plan.bounds.vertical),
                        clampedOffset(requested.horizontal, plan.bounds.horizontal)};
        std::size_t stickyIndex = 0;
        for (std::size_t i = 0; i < measured.size(); ++i)
            if (measured[i].top <= plan.scroll.vertical) stickyIndex = i;
        layout.rows = {layout.body.x, layout.body.y + measured[stickyIndex].header, layout.body.width,
                       std::max(0, documentViewportHeight - measured[stickyIndex].header)};

        fill(plan, {0, 0, clientWidth, clientHeight}, Color(128, 0, 0, model.curtainAlpha));
        fill(plan, panel, Color(255, 255, 255, 80));
        fill(plan, {panel.x + 2, panel.y + 2, panel.width - 4, panel.height - 4}, Color(0, 0, 255, 80));
        fill(plan, layout.title, Header);
        text(plan, panel.x + (panel.width - textWidth("---SCORE---", 32)) / 2,
             panel.y + ScoreSummaryGeometry::CaptionTop, "---SCORE---", 32);
        fill(plan, layout.counter, Black);
        text(plan, layout.counter.x + 8, layout.counter.y + 4, counter, 32);
        for (std::size_t i = 0; i < status.size(); ++i)
            text(plan, (clientWidth - textWidth(status[i])) / 2, layout.status.y + int(i) * Pitch, status[i]);

        const auto tableLeft = [&](const MeasuredTable &table) {
            const int overflow = std::max(0, table.width - layout.rows.width);
            const int centered = table.table->score ? std::max(0, (layout.rows.width - table.width) / 2) : 0;
            return layout.rows.x + centered - std::min(plan.scroll.horizontal, overflow);
        };

        auto drawHeading = [&](const MeasuredTable &table, int top, const Rect &clip) {
            const bool score = table.table->score;
            const int originX = tableLeft(table);
            const bool primary = table.table == &tables.front();
            if (!primary) {
                if (!score) fill(plan, intersection({layout.rows.x, top + 4, layout.rows.width, 28}, clip), Header);
                clippedText(plan, layout.rows.x + 8, top + (score ? 0 : 10), table.table->title, clip);
            }
            int x = originX;
            for (std::size_t c = 0; c < table.widths.size(); ++c) {
                const auto &column = table.table->columns[c];
                const int standard = score && column.numeric ? scoreColumnWidth(column.heading) : 0;
                const int tx = standard ? x + table.widths[c] - standard + scoreHeadingInset(column.heading)
                        : column.numeric ? x + table.widths[c] - 8 - textWidth(column.heading, table.textHeight)
                        : x + (score ? 5 : 8);
                clippedText(plan, tx, top + (primary ? 0 : score ? 24 : 42), column.heading, clip, table.textHeight);
                x += table.widths[c];
            }
        };
        const Rect headerClip{layout.rows.x, layout.body.y, layout.rows.width, measured[stickyIndex].header};
        drawHeading(measured[stickyIndex], layout.body.y, headerClip);
        std::size_t rowNumber = 0, firstVisible = 0, lastVisible = 0;
        for (std::size_t t = 0; t < measured.size(); ++t) {
            const auto &table = measured[t];
            const bool score = table.table->score;
            const int top = layout.body.y + table.top - plan.scroll.vertical;
            if (t != stickyIndex) drawHeading(table, top, layout.rows);
            std::optional<Color> teamSurface;
            for (std::size_t r = 0; r < table.table->rows.size(); ++r) {
                ++rowNumber;
                const auto &row = table.table->rows[r];
                if (row.teamColor) teamSurface = row.teamColor->withAlpha(178);
                const int y = top + table.rowTops[r];
                if (row.teamSeparatorBefore) fill(plan,
                        intersection({score ? tableLeft(table) : layout.rows.x, y - 6,
                                      score ? table.width : layout.rows.width, 2}, layout.rows), Color(255, 255, 255, 178));
                if (y < layout.rows.y || y + table.textHeight > layout.rows.y + layout.rows.height) continue;
                if (!firstVisible) firstVisible = rowNumber;
                lastVisible = rowNumber;
                int x = tableLeft(table);
                if (score) {
                    const Color surface = row.teamColor ? *teamSurface : row.nested && teamSurface
                            ? teamSurface->scale(0.2f) : Color(0, 0, 255, 178);
                    const Rect surfaceClip{layout.rows.x, layout.rows.y - 1, layout.rows.width, layout.rows.height + 1};
                    fill(plan, intersection({x, y - 1, table.width, table.textHeight}, surfaceClip), surface);
                }
                for (std::size_t c = 0; c < table.widths.size(); ++c) {
                    if (c < row.cells.size()) {
                        int inset = score ? (c == 0 ? row.teamColor ? 0 : 5 : 16) : 8;
                        if (!score && c == 0 && row.teamColor) {
                            fill(plan, intersection({x + inset, y + 3, 12, 10}, layout.rows), *row.teamColor);
                            inset += 20;
                        }
                        const Color color = !score ? White : row.teamColor ? Black
                                : table.table->columns[c].numeric ? Color::YELLOW : row.identityColor;
                        if (score && c > 0) {
                            clippedText(plan, x, y, "|", layout.rows, 32, row.teamColor ? Black : Color::YELLOW);
                        }
                        const int tx = table.table->columns[c].numeric
                                ? x + table.widths[c] - (score ? 0 : 8) - textWidth(row.cells[c], table.textHeight) : x + inset;
                        clippedText(plan, tx, y, row.cells[c], layout.rows, table.textHeight, color);
                    }
                    x += table.widths[c];
                }
            }
        }
        if (plan.bounds.vertical) {
            const Rect track{layout.body.x + layout.body.width + 4, layout.rows.y, 8, layout.rows.height};
            fill(plan, track, Color(0, 0, 0, 120));
            const int thumb = std::min(track.height, std::max(16,
                    track.height * documentViewportHeight / std::max(1, plan.bounds.contentHeight)));
            const int y = track.y + static_cast<int>(static_cast<std::int64_t>(track.height - thumb)
                    * plan.scroll.vertical / plan.bounds.vertical);
            fill(plan, {track.x + 1, y, 6, thumb}, Color(192));
        }
        if (navigation) {
            const auto &nav = layout.navigation;
            std::string feedback;
            if (plan.bounds.vertical) feedback = "Rows " + std::to_string(firstVisible) + "-" + std::to_string(lastVisible)
                    + "/" + std::to_string(plan.bounds.totalRows);
            if (plan.bounds.horizontal) {
                if (!feedback.empty()) feedback += " / ";
                feedback += "Position " + std::to_string(plan.scroll.horizontal / Glyph + 1) + "-"
                        + std::to_string(std::min(plan.bounds.contentWidth, plan.scroll.horizontal + layout.rows.width) / Glyph)
                        + "/" + std::to_string((plan.bounds.contentWidth + Glyph - 1) / Glyph);
            }
            // Local feedback is read-only: no new clickable controls or bindings.
            if (network && plan.bounds.horizontal) {
                button(plan, layout.horizontalBack, "<", plan.scroll.horizontal > 0, false, false);
                button(plan, layout.horizontalForward, ">", plan.scroll.horizontal < plan.bounds.horizontal, false, false);
            }
            if (network && model.navigationFocused) frame(plan, {nav.x - 2, nav.y - 2, nav.width + 4, nav.height + 4}, Black);
            const Rect feedbackBounds{nav.x + 40, nav.y, std::max(0, nav.width - 80), nav.height};
            clippedText(plan, feedbackBounds.x, nav.y + 4, feedback, feedbackBounds);
        }

        int actionX = layout.footer.x;
        for (Action action: allowed) {
            const auto found = std::find_if(model.actions.begin(), model.actions.end(),
                    [action](const ActionState &state) { return state.action == action; });
            if (found == model.actions.end()) continue;
            const Rect rect{actionX, layout.footer.y, action == Action::ReturnToLobby ? 220 : 200, 40};
            plan.actions.push_back({*found, rect});
            button(plan, rect, caption(action), found->enabled, found->focused, found->enabled && found->pressed);
            actionX += rect.width + 16;
        }
        fill(plan, layout.notice, Header);
        text(plan, layout.notice.x + 16, layout.notice.y + 8, "End of Game", 32);
        return plan;
    }

    void render(Renderer &renderer, const Font &font, const Plan &plan) {
        const auto view = renderer.getViewMatrix(), model = renderer.getModelMatrix();
        renderer.setViewMatrix(Matrix::IDENTITY);
        renderer.setModelMatrix(Matrix::IDENTITY);
        renderer.enableDepthTest(false);
        renderer.enableDepthWrite(false);
        for (const auto &command: plan.commands) {
            const auto &rect = command.bounds;
            const Vector bottomLeft(rect.x, plan.layout.height - rect.y - rect.height);
            if (command.kind == Command::Kind::Text)
                font.print(bottomLeft.x, bottomLeft.y, 0, command.color, command.text, static_cast<float>(command.size));
            else {
                renderer.setBlendFunc(BlendFunc::SrcAlpha);
                if (command.kind == Command::Kind::Fill)
                    renderer.quadXY(bottomLeft, Vector(rect.width, rect.height), command.color);
                else renderer.frame(bottomLeft, Vector(rect.width, rect.height), command.size, command.color);
            }
        }
        renderer.setBlendFunc(BlendFunc::None);
        renderer.enableDepthWrite(true);
        renderer.setModelMatrix(model);
        renderer.setViewMatrix(view);
    }

    std::optional<Action> hitAction(const Plan &plan, int clientX, int clientY) {
        for (const auto &action: plan.actions)
            if (action.state.enabled && action.bounds.contains(clientX, clientY)) return action.state.action;
        return std::nullopt;
    }

    void move(Scroll &scroll, const ScrollBounds &bounds, int rows, int columns) {
        scroll.vertical = clampedOffset(scroll.vertical, bounds.vertical);
        if (rows) {
            const auto &positions = bounds.verticalPositions;
            const auto next = rows > 0 ? std::upper_bound(positions.begin(), positions.end(), scroll.vertical)
                                      : std::lower_bound(positions.begin(), positions.end(), scroll.vertical);
            const auto index = std::clamp<std::int64_t>(
                    (next - positions.begin()) + static_cast<std::int64_t>(rows) - (rows > 0 ? 1 : 0),
                    0, static_cast<std::int64_t>(positions.size()) - 1);
            scroll.vertical = positions[index];
        }
        scroll.horizontal = clampedOffset(static_cast<std::int64_t>(scroll.horizontal) + static_cast<std::int64_t>(columns) * Glyph,
                                          bounds.horizontal);
    }

    bool navigateLocal(Scroll &scroll, const Plan &plan, const LocalNavigation &input) {
        if (plan.scope != Scope::Local || !input.completed || input.consoleOwnsInput
            || input.input == LocalInput::Other) return false;
        int units = 0;
        if (input.input == LocalInput::Wheel) {
            if (!plan.layout.body.contains(input.pointerX, input.pointerY) || input.wheelY == 0) return false;
            // Convert in 64-bit space so even synthetic INT_MIN wheel input is bounded.
            const auto direction = -static_cast<std::int64_t>(input.wheelY);
            units = static_cast<int>(std::clamp<std::int64_t>(direction,
                    std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
        } else units = input.input == LocalInput::PageDown ? 8 : -8;
        move(scroll, plan.bounds, input.shift ? 0 : units, input.shift ? units : 0);
        return true;
    }
}
