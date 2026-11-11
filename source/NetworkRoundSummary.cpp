#include "NetworkRoundSummary.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace Duel6::NetworkRoundSummary {
    namespace {
        namespace R = Network::Replication;
        std::size_t length(std::string_view text) {
            return std::count_if(text.begin(), text.end(), [](unsigned char c) { return (c & 0xc0) != 0x80; });
        }

        std::string clip(const std::string &text, std::size_t characters) {
            if (length(text) <= characters) return text;
            if (characters == 0) return {};
            std::size_t end = 0, remaining = characters - 1;
            while (end < text.size() && remaining) {
                ++end;
                while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) ++end;
                --remaining;
            }
            return text.substr(0, end) + "…";
        }

        std::string lifecycle(const R::CanonicalState &state, const R::PlayerState &player) {
            if (player.lifeState == R::LifeState::Departed) return "Departed";
            if (player.lifeState == R::LifeState::Dead) return "Dead";
            if (player.lifeState == R::LifeState::Waiting) return "Waiting";
            const auto owner = std::find_if(state.participants.begin(), state.participants.end(), [&](const auto &p) {
                return p.participantId == player.ownerParticipantId;
            });
            return owner != state.participants.end() && owner->connection == R::ConnectionState::Reconnecting
                    ? "Reconnecting" : "";
        }

        std::array<std::string, 5> values(const R::ScoreRowState &score) {
            // Preserve Person's float/truncated-hundredths convention, including
            // zero deaths, without narrowing replicated counters to Int32.
            const float ratio = score.deaths ? float(score.kills) / float(score.deaths) : float(score.kills);
            const auto whole = std::floor(static_cast<long double>(ratio));
            std::ostringstream kd;
            kd << std::fixed << std::setprecision(0) << whole << '.' << std::setw(2) << std::setfill('0')
               << static_cast<int>((ratio - static_cast<float>(whole)) * 100);
            return {std::to_string(score.kills), std::to_string(score.assists), std::to_string(score.deaths),
                    kd.str(), std::to_string(score.cumulativePoints)};
        }

        std::vector<std::string> wrap(std::string text, std::size_t columns) {
            std::vector<std::string> result;
            while (!text.empty()) {
                std::size_t end = 0, count = 0;
                while (end < text.size() && count < columns) {
                    ++end;
                    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) ++end;
                    ++count;
                }
                if (end < text.size()) {
                    const auto space = text.rfind(' ', end);
                    if (space != std::string::npos && space != 0) end = space;
                }
                result.push_back(text.substr(0, end));
                text.erase(0, end);
                while (!text.empty() && text.front() == ' ') text.erase(0, 1);
            }
            return result;
        }
    }

    bool applies(const R::CanonicalState &state) {
        return state.phase == R::Phase::RoundSummary && state.settings.roundLimit > state.currentRoundNumber;
    }

    std::vector<Row> rows(const R::CanonicalState &state) {
        std::vector<Row> result;
        const auto appendPlayers = [&](unsigned team, bool nested, R::ScoreRowState *total) {
            for (const auto id: state.score.ranking) {
                const auto player = std::find_if(state.players.begin(), state.players.end(), [&](const auto &p) {
                    return p.playerId == id && (!nested || p.team == team);
                });
                if (player == state.players.end()) continue;
                const auto score = std::find_if(state.score.players.begin(), state.score.players.end(), [&](const auto &s) {
                    return s.playerId == id;
                });
                Row row;
                row.playerId = id; row.name = player->displayName; row.lifecycle = lifecycle(state, *player);
                row.nested = nested;
                row.nameColor = player->lifeState == R::LifeState::Waiting ? Color::WHITE
                        : player->lifeState == R::LifeState::Alive ? Color::YELLOW : Color::RED;
                row.background = Color(0, 0, 255, 178);
                if (score != state.score.players.end()) {
                    row.values = values(*score);
                    if (total) {
                        total->kills += score->kills; total->assists += score->assists;
                        total->deaths += score->deaths; total->cumulativePoints += score->cumulativePoints;
                    }
                }
                result.push_back(std::move(row));
            }
        };
        if (state.score.teamRanking.empty()) appendPlayers(0, false, nullptr);
        else {
            static const char *names[] = {"", "Alpha", "Bravo", "Charlie", "Delta"};
            static const Color colors[] = {Color::WHITE, Color::RED, Color::GREEN, Color::YELLOW, Color::MAGENTA};
            for (const auto team: state.score.teamRanking) {
                if (team == 0 || team > 4) continue;
                Row row;
                row.name = names[team]; row.team = true; row.separatorBefore = !result.empty();
                row.nameColor = Color::BLACK; row.background = colors[team].withAlpha(178);
                const auto header = result.size();
                result.push_back(std::move(row));
                R::ScoreRowState total;
                appendPlayers(team, true, &total);
                result[header].values = values(total);
                for (auto index = header + 1; index < result.size(); ++index)
                    result[index].background = colors[team].withAlpha(178).scale(0.2f);
            }
        }
        return result;
    }

    Layout layout(const std::vector<Row> &rows, const std::string &progress, const std::string &outcome,
                  int clientWidth, int clientHeight, std::size_t requestedFirst) {
        Layout result;
        result.columnCharacters = {4, 4, 4, 6, 5};
        int nameCharacters = 0;
        for (const auto &row: rows) {
            nameCharacters = std::max(nameCharacters, int(length(row.name)
                    + (row.lifecycle.empty() ? 0 : length(row.lifecycle) + 3)));
            for (std::size_t col = 0; col < row.values.size(); ++col)
                result.columnCharacters[col] = std::max(result.columnCharacters[col], int(row.values[col].size()) + 1);
        }
        int numericWidth = 0;
        for (const auto count: result.columnCharacters) numericWidth += count * GlyphWidth;
        // Local OVER-02: (maximum name + 26) glyphs, 16px outer padding,
        // and the score strip's additional 5px overhang on either side.
        result.tableWidth = std::min((nameCharacters + 3) * GlyphWidth + numericWidth, clientWidth - 74);
        result.nameWidth = result.tableWidth - numericWidth;
        result.width = std::max(result.tableWidth, int(length(progress)) * GlyphWidth) + 32;
        result.left = (clientWidth - result.width) / 2;
        result.tableLeft = (clientWidth - result.tableWidth) / 2;
        result.outcomeRows = wrap(outcome, std::max(1, (result.width - 32) / 8));

        // Actions end at y=118. Even the maximum current status/waiting stack
        // ends below that. Keep 16px between these fixed regions, the plain
        // outcome/countdown text, and the panel. Only the body can lose rows.
        result.phaseY = 134;
        result.outcomeY = result.phaseY + 22;
        const int minimumBottom = result.outcomeY + int(result.outcomeRows.size()) * 18 + 16;
        const int maximumTop = clientHeight - 16;
        const int bodyBudget = std::max(TextHeight, maximumTop - minimumBottom - 192);
        int used = 0;
        result.maximumFirst = rows.size();
        while (result.maximumFirst > 0) {
            const auto previous = result.maximumFirst - 1;
            const int added = TextHeight + (result.maximumFirst < rows.size()
                    && rows[result.maximumFirst].separatorBefore ? SeparatorHeight : 0);
            if (used + added > bodyBudget) break;
            used += added; --result.maximumFirst;
        }
        result.first = std::min(requestedFirst, result.maximumFirst);
        used = 0;
        for (auto index = result.first; index < rows.size(); ++index) {
            const int added = TextHeight + (index > result.first && rows[index].separatorBefore ? SeparatorHeight : 0);
            if (used + added > bodyBudget) break;
            used += added; ++result.count;
        }
        result.height = 192 + (result.maximumFirst != 0 ? bodyBudget : used);
        result.bottom = std::clamp((clientHeight - result.height) / 2, minimumBottom,
                                  std::max(minimumBottom, maximumTop - result.height));
        result.scoreY = result.bottom + result.height - 96;
        result.firstRowY = result.scoreY - 64;
        // Keep the plain network footer adjacent to a centered local panel.
        const int shift = result.bottom - minimumBottom;
        result.phaseY += shift; result.outcomeY += shift;
        return result;
    }

    std::string rowLabel(const Row &row, int availablePixels) {
        const auto columns = std::size_t(std::max(0, availablePixels / GlyphWidth));
        const std::string suffix = row.lifecycle.empty() ? "" : " • " + row.lifecycle;
        const auto suffixLength = length(suffix);
        return clip(row.name, columns > suffixLength ? columns - suffixLength : 0) + suffix;
    }

    std::string numericText(const Row &row, const Layout &layout) {
        std::string text;
        for (std::size_t col = 0; col < row.values.size(); ++col) {
            text += '|';
            text.append(std::max(0, layout.columnCharacters[col] - 1 - int(row.values[col].size())), ' ');
            text += row.values[col];
        }
        return text;
    }

    std::string headings(const Layout &layout) {
        // Match the original local heading exactly at its ordinary allocation;
        // extra numeric digits expand only the corresponding column.
        static const char *parts[] = {" K  ", "  A ", "  D ", " K/D  ", "PTS  "};
        static const int minimum[] = {4, 4, 4, 6, 5};
        std::string result;
        for (int col = 0; col < 5; ++col) {
            result.append(layout.columnCharacters[col] - minimum[col], ' ');
            result += parts[col];
        }
        return result;
    }
}
