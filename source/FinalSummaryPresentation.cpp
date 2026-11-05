#include "FinalSummaryPresentation.h"

#include <algorithm>
#include <limits>
#include "Font.h"
#include "renderer/Renderer.h"

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
        void clippedText(Plan &plan, int x, int y, const std::string &value, const Rect &clip) {
            if (y < clip.y || y + 16 > clip.y + clip.height) return;
            const auto offsets = boundaries(value);
            const int count = static_cast<int>(offsets.size()) - 1;
            const int first = std::clamp((clip.x - x + Glyph - 1) / Glyph, 0, count);
            const int end = std::clamp((clip.x + clip.width - x) / Glyph, first, count);
            text(plan, x + first * Glyph, y, value.substr(offsets[first], offsets[end] - offsets[first]));
        }

        Rect intersection(Rect a, Rect b) {
            const int right = std::min(a.x + a.width, b.x + b.width);
            const int bottom = std::min(a.y + a.height, b.y + b.height);
            a.x = std::max(a.x, b.x); a.y = std::max(a.y, b.y);
            a.width = std::max(0, right - a.x); a.height = std::max(0, bottom - a.y);
            return a;
        }

        std::string identityText(const Identity &identity) {
            std::string value = identity.name;
            if (!identity.player.empty()) value += " / Player " + identity.player;
            if (!identity.team.empty()) value += " / " + identity.team;
            if (identity.departed) value += " / Departed";
            return value;
        }

        std::string outcomeText(const Outcome &outcome) {
            if (outcome.noWinner) return "No winner";
            std::string value = outcome.winningTeam;
            for (const auto &winner: outcome.winners) {
                if (!value.empty()) value += " / ";
                value += identityText(winner);
            }
            return value;
        }

        std::string fixedOutcome(const Outcome &outcome, const std::string &label, int width,
                                 Table &overflow) {
            const std::string complete = label + outcomeText(outcome);
            if (textWidth(complete) <= width) return complete;
            // The input is an established outcome, never a ranking-derived one.
            // Keep match and last-round scope distinct even for identical winners.
            const std::string scope = label.substr(0, label.size() - 2);
            if (!outcome.winningTeam.empty())
                overflow.rows.push_back({{scope, outcome.winningTeam, "", outcome.winningTeam, ""}});
            for (const auto &winner: outcome.winners)
                overflow.rows.push_back({{scope, winner.name, winner.player, winner.team,
                                          winner.departed ? "Departed" : ""}});
            return label + std::to_string(outcome.winners.size())
                    + (outcome.winners.size() == 1 ? " winner" : " winners") + " / See outcome rows";
        }

        struct MeasuredTable {
            const Table *table = nullptr;
            std::vector<int> widths;
            std::vector<int> rowTops;
            int top = 0, height = 0, width = 0;
        };

        std::vector<MeasuredTable> measure(const std::vector<Table> &tables, int availableWidth,
                                            ScrollBounds &bounds) {
            std::vector<MeasuredTable> result;
            for (const auto &table: tables) {
                MeasuredTable measured;
                measured.table = &table;
                measured.top = bounds.contentHeight;
                measured.height = HeaderHeight;
                for (const auto &column: table.columns)
                    measured.widths.push_back(std::max(column.minimumWidth, textWidth(column.heading) + 16));
                for (const auto &row: table.rows) {
                    if (row.teamSeparatorBefore) measured.height += 8;
                    measured.rowTops.push_back(measured.height);
                    measured.height += Pitch;
                    for (std::size_t i = 0; i < row.cells.size() && i < measured.widths.size(); ++i) {
                        const int inset = i == 0 ? (row.nested ? 24 : 0) + (row.teamColor ? 20 : 0) : 0;
                        measured.widths[i] = std::max(measured.widths[i], textWidth(row.cells[i]) + 16 + inset);
                    }
                }
                for (int width: measured.widths) measured.width += width;
                // Identity receives remaining width AFTER all numeric columns.
                if (!measured.widths.empty() && measured.width < availableWidth) {
                    auto identity = std::find_if(table.columns.begin(), table.columns.end(),
                            [](const Column &column) { return !column.numeric; });
                    const auto index = identity == table.columns.end() ? 0 : identity - table.columns.begin();
                    measured.widths[index] += availableWidth - measured.width;
                    measured.width = availableWidth;
                }
                measured.width = std::max(measured.width, textWidth(table.title) + 16);
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
        const int panelWidth = std::max(0, std::min(1184, clientWidth - 32));
        layout.panel = {(clientWidth - panelWidth) / 2, 16, panelWidth, std::max(0, clientHeight - 152)};
        const auto &panel = layout.panel;
        layout.title = {panel.x + 2, 18, panel.width - 4, 40};
        layout.body = {panel.x + 16, 146, std::max(0, panel.width - 32), std::max(0, clientHeight - 298)};
        layout.footer = {panel.x, clientHeight - 120, panel.width, 40};
        const int noticeWidth = textWidth("End of Game", 32) + 32;
        layout.notice = {(clientWidth - noticeWidth) / 2, clientHeight - 64, noticeWidth, 48};
        const std::string counter = "Rounds: " + std::to_string(model.completedRounds) + " | " + std::to_string(model.roundLimit);
        const int counterWidth = textWidth(counter, 32) + 16;
        layout.counter = {panel.x + panel.width - 16 - counterWidth, 22, counterWidth, 32};
        layout.rows = {layout.body.x + 16, layout.body.y + HeaderHeight,
                       std::max(0, layout.body.width - 40),
                       std::max(0, layout.body.height - HeaderHeight - NavigationHeight)};
        layout.navigation = {layout.body.x + 16, layout.body.y + layout.body.height - 28,
                             std::max(0, layout.body.width - 40), 24};
        layout.horizontalBack = {layout.navigation.x, layout.navigation.y, 32, 24};
        layout.horizontalForward = {layout.navigation.x + layout.navigation.width - 32,
                                    layout.navigation.y, 32, 24};

        fill(plan, {0, 0, clientWidth, clientHeight}, Color(128, 0, 0, model.curtainAlpha));
        fill(plan, panel, Color(255, 255, 255, 80));
        fill(plan, {panel.x + 2, panel.y + 2, panel.width - 4, panel.height - 4}, Color(0, 0, 255, 80));
        fill(plan, layout.title, Header);
        text(plan, panel.x + 16, 22, "SCORE", 32);
        fill(plan, layout.counter, Black);
        text(plan, layout.counter.x + 8, layout.counter.y, counter, 32);
        if (model.scope == Scope::Local) text(plan, layout.body.x, 66, "Local Play");
        else {
            text(plan, layout.body.x, 66, model.scope == Scope::NetworkGuest
                    ? "Completed / Session only / Waiting for host to return to lobby or end session"
                    : "Completed / Session only");
            text(plan, layout.body.x, 82, "Not saved to local statistics or Elo");
        }
        Table outcomes{"Outcome identities", {{"Outcome scope"}, {"Complete display name"}, {"Player identity"},
                                             {"Team"}, {"Departure"}}, {}};
        const auto match = fixedOutcome(model.matchOutcome, "Match outcome: ", layout.body.width, outcomes);
        const auto last = fixedOutcome(model.lastRoundOutcome,
                "Last completed round: " + std::to_string(model.lastRoundNumber) + " / ", layout.body.width, outcomes);
        text(plan, layout.body.x, 102, match);
        text(plan, layout.body.x, 120, last);

        std::vector<Table> tables;
        if (!outcomes.rows.empty()) tables.push_back(std::move(outcomes));
        tables.push_back(model.cumulative);
        tables.back().title = "Cumulative ranking";
        tables.insert(tables.end(), model.settings.begin(), model.settings.end());
        tables.insert(tables.end(), model.rounds.begin(), model.rounds.end());
        const auto measured = measure(tables, layout.rows.width, plan.bounds);
        const int documentViewportHeight = std::max(0, layout.body.height - NavigationHeight);
        plan.bounds.vertical = std::max(0, plan.bounds.contentHeight - documentViewportHeight);
        plan.bounds.horizontal = std::max(0, plan.bounds.contentWidth - layout.rows.width);
        plan.scroll = {clampedOffset(requested.vertical, plan.bounds.vertical),
                       clampedOffset(requested.horizontal, plan.bounds.horizontal)};
        const int originX = layout.rows.x - plan.scroll.horizontal;
        std::size_t stickyIndex = 0;
        for (std::size_t i = 0; i < measured.size(); ++i)
            if (measured[i].top <= plan.scroll.vertical) stickyIndex = i;

        auto drawHeading = [&](const MeasuredTable &table, int top, const Rect &clip) {
            fill(plan, intersection({layout.rows.x, top + 4, layout.rows.width, 28}, clip), Header);
            // The section label stays fixed on BOTH axes; only table columns
            // follow horizontal navigation. Ranking never loses its scope label.
            clippedText(plan, layout.rows.x + 8, top + 10, table.table->title, clip);
            int x = originX;
            for (std::size_t c = 0; c < table.widths.size(); ++c) {
                const auto &column = table.table->columns[c];
                const int tx = column.numeric ? x + table.widths[c] - 8 - textWidth(column.heading) : x + 8;
                clippedText(plan, tx, top + 42, column.heading, clip);
                x += table.widths[c];
            }
        };
        const Rect headerClip{layout.rows.x, layout.body.y, layout.rows.width, HeaderHeight};
        drawHeading(measured[stickyIndex], layout.body.y, headerClip);
        std::size_t rowNumber = 0, firstVisible = 0, lastVisible = 0;
        for (std::size_t t = 0; t < measured.size(); ++t) {
            const auto &table = measured[t];
            const int top = layout.body.y + table.top - plan.scroll.vertical;
            if (t != stickyIndex) drawHeading(table, top, layout.rows);
            for (std::size_t r = 0; r < table.table->rows.size(); ++r) {
                ++rowNumber;
                const auto &row = table.table->rows[r];
                const int y = top + table.rowTops[r];
                if (row.teamSeparatorBefore) fill(plan,
                        intersection({layout.rows.x, y - 5, layout.rows.width, 2}, layout.rows), Color(255, 255, 255, 178));
                if (y < layout.rows.y || y + 16 > layout.rows.y + layout.rows.height) continue;
                if (!firstVisible) firstVisible = rowNumber;
                lastVisible = rowNumber;
                int x = originX;
                for (std::size_t c = 0; c < table.widths.size(); ++c) {
                    if (c < row.cells.size()) {
                        int inset = 8 + (c == 0 && row.nested ? 24 : 0);
                        if (c == 0 && row.teamColor) {
                            fill(plan, intersection({x + inset, y + 3, 12, 10}, layout.rows), *row.teamColor);
                            inset += 20;
                        }
                        const int tx = table.table->columns[c].numeric
                                ? x + table.widths[c] - 8 - textWidth(row.cells[c]) : x + inset;
                        clippedText(plan, tx, y, row.cells[c], layout.rows);
                    }
                    x += table.widths[c];
                }
            }
        }
        if (plan.bounds.vertical) {
            const Rect track{layout.body.x + layout.body.width - 12, layout.rows.y, 8, layout.rows.height};
            fill(plan, track, Color(0, 0, 0, 120));
            const int thumb = std::min(track.height, std::max(16,
                    track.height * documentViewportHeight / std::max(1, plan.bounds.contentHeight)));
            const int y = track.y + static_cast<int>(static_cast<std::int64_t>(track.height - thumb)
                    * plan.scroll.vertical / plan.bounds.vertical);
            fill(plan, {track.x + 1, y, 6, thumb}, Color(192));
        }
        if (plan.bounds.vertical || plan.bounds.horizontal || model.scope != Scope::Local) {
            const auto &nav = layout.navigation;
            const std::string feedback = "Rows " + std::to_string(firstVisible) + "-" + std::to_string(lastVisible)
                    + "/" + std::to_string(plan.bounds.totalRows) + " / Position "
                    + std::to_string(plan.scroll.horizontal / Glyph + 1) + "-"
                    + std::to_string(std::min(plan.bounds.contentWidth, plan.scroll.horizontal + layout.rows.width) / Glyph)
                    + "/" + std::to_string((plan.bounds.contentWidth + Glyph - 1) / Glyph);
            // Local feedback is read-only: no new clickable controls or bindings.
            if (model.scope != Scope::Local) {
                button(plan, layout.horizontalBack, "<", plan.scroll.horizontal > 0, false, false);
                button(plan, layout.horizontalForward, ">", plan.scroll.horizontal < plan.bounds.horizontal, false, false);
                if (model.navigationFocused) frame(plan, {nav.x - 2, nav.y - 2, nav.width + 4, nav.height + 4}, Black);
            }
            const Rect feedbackBounds{nav.x + 40, nav.y, std::max(0, nav.width - 80), nav.height};
            clippedText(plan, feedbackBounds.x, nav.y + 4, feedback, feedbackBounds);
        }

        int actionX = layout.footer.x;
        const std::vector<Action> allowed = model.scope == Scope::NetworkHost
                ? std::vector<Action>{Action::ReturnToLobby, Action::EndSession}
                : model.scope == Scope::NetworkGuest ? std::vector<Action>{Action::Leave} : std::vector<Action>{};
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
        scroll.vertical = clampedOffset(static_cast<std::int64_t>(scroll.vertical) + static_cast<std::int64_t>(rows) * Pitch,
                                        bounds.vertical);
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
