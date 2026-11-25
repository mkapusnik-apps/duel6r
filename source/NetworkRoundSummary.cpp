#include "NetworkRoundSummary.h"
#include "RoundSummaryCounter.h"
#include "ScoreSummaryGeometry.h"
#include "gamemodes/Team.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace Duel6::NetworkRoundSummary {
    namespace {
        namespace R = Network::Replication;
        namespace T = RoundScoreTable;
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
            if (player.lifeState == R::LifeState::Waiting) return "Waiting";
            const auto owner = std::find_if(state.participants.begin(), state.participants.end(), [&](const auto &p) {
                return p.participantId == player.ownerParticipantId;
            });
            return owner != state.participants.end() && owner->connection == R::ConnectionState::Reconnecting
                    ? "Reconnecting" : "";
        }

        std::string ratioText(float kills, float deaths) {
            // Preserve Person's float/truncated-hundredths convention, including
            // zero deaths, without narrowing replicated counters to Int32.
            const float ratio = deaths ? kills / deaths : kills;
            const auto whole = std::floor(static_cast<long double>(ratio));
            std::ostringstream kd;
            kd << std::fixed << std::setprecision(0) << whole << '.' << std::setw(2) << std::setfill('0')
               << static_cast<int>((ratio - static_cast<float>(whole)) * 100);
            return kd.str();
        }

        T::Values values(const R::ScoreRowState &score) {
            return {std::to_string(score.kills), std::to_string(score.assists), std::to_string(score.deaths),
                    ratioText(float(score.kills), float(score.deaths)), std::to_string(score.cumulativePoints)};
        }

        // A validated replicated roster can contain several UInt64 counters.
        // Keep display-only team sums exact even beyond one machine integer.
        // Three base-1e9 limbs cover MaxReplicatedPlayers * UInt64::max().
        struct Total {
            static constexpr std::uint64_t Base = 1000000000;
            std::array<std::uint32_t, 3> digits{};
            void add(std::uint64_t value) {
                for (auto &digit: digits) {
                    const auto sum = digit + value % Base;
                    value = value / Base + sum / Base;
                    digit = static_cast<std::uint32_t>(sum % Base);
                }
            }
            std::string text() const {
                std::ostringstream out;
                bool started = false;
                for (auto index = digits.size(); index-- > 0;) {
                    if (!started && digits[index] == 0 && index != 0) continue;
                    if (started) out << std::setw(9) << std::setfill('0');
                    out << digits[index]; started = true;
                }
                return out.str();
            }
            float asFloat() const {
                long double value = 0;
                for (auto index = digits.size(); index-- > 0;) value = value * Base + digits[index];
                return static_cast<float>(value);
            }
        };
        static_assert(R::MaxReplicatedPlayers <= 10000000, "Display aggregate capacity");

        std::string difference(Total positive, Total negative) {
            const bool below = std::lexicographical_compare(positive.digits.rbegin(), positive.digits.rend(),
                                                            negative.digits.rbegin(), negative.digits.rend());
            if (below) std::swap(positive, negative);
            std::int64_t borrow = 0;
            for (std::size_t index = 0; index < positive.digits.size(); ++index) {
                auto digit = std::int64_t(positive.digits[index]) - negative.digits[index] - borrow;
                borrow = digit < 0 ? 1 : 0;
                if (borrow) digit += static_cast<std::int64_t>(Total::Base);
                positive.digits[index] = static_cast<std::uint32_t>(digit);
            }
            return (below ? "-" : "") + positive.text();
        }

        T::Values teamValues(const std::vector<Row> &rows, std::size_t header, const R::CanonicalState &state) {
            Total kills, assists, deaths, positive, negative;
            for (auto index = header + 1; index < rows.size(); ++index) {
                const auto score = std::find_if(state.score.players.begin(), state.score.players.end(), [&](const auto &s) {
                    return s.playerId == rows[index].playerId;
                });
                if (score == state.score.players.end()) continue;
                kills.add(score->kills); assists.add(score->assists); deaths.add(score->deaths);
                if (score->cumulativePoints >= 0) positive.add(static_cast<std::uint64_t>(score->cumulativePoints));
                else negative.add(static_cast<std::uint64_t>(-(score->cumulativePoints + 1)) + 1);
            }
            return {kills.text(), assists.text(), deaths.text(), ratioText(kills.asFloat(), deaths.asFloat()),
                    difference(positive, negative)};
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

    Color curtainColor(const R::CanonicalState &state) {
        if (!applies(state)) return Color(128, 0, 0, 0);
        const float remaining = static_cast<float>(std::min<std::uint64_t>(360, state.roundEndCountdown)) / 60.0f;
        return Color(128, 0, 0, static_cast<Uint8>(200 - 200 * (remaining / D6_GAME_OVER_WAIT)));
    }

    std::string teamName(unsigned team) {
        return team > 0 && team <= TEAMS.size() ? TEAMS[team - 1].name : std::to_string(team);
    }

    std::vector<Row> rows(const R::CanonicalState &state) {
        std::vector<Row> result;
        const auto appendPlayers = [&](unsigned team, bool nested) {
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
                }
                result.push_back(std::move(row));
            }
        };
        if (state.score.teamRanking.empty()) appendPlayers(0, false);
        else {
            for (const auto team: state.score.teamRanking) {
                if (team == 0 || team > TEAMS.size()) continue;
                Row row;
                row.name = teamName(team); row.team = true; row.separatorBefore = !result.empty();
                const Color background = TEAMS[team - 1].color.withAlpha(178);
                row.nameColor = Color::BLACK; row.background = background;
                const auto header = result.size();
                result.push_back(std::move(row));
                appendPlayers(team, true);
                result[header].values = teamValues(result, header, state);
                for (auto index = header + 1; index < result.size(); ++index)
                    result[index].background = background.scale(0.2f);
            }
        }
        return result;
    }

    Footer footer(int statusRows) {
        const int statusHeight = 8 + std::max(1, statusRows) * 16;
        const int actionY = RoundSummaryCounter::ClearTop + statusHeight + 16;
        return {RoundSummaryCounter::ClearTop, statusHeight, actionY, actionY + 34 + 16};
    }

    Layout layout(const std::vector<Row> &rows, const std::string &outcome,
                  int clientWidth, int clientHeight, std::size_t requestedFirst, int statusRows) {
        Layout result;
        auto columns = T::OrdinaryColumns;
        auto desiredColumns = columns;
        int nameCharacters = 0;
        int minimumNameWidth = T::GlyphWidth + T::PlayerNameInset;
        for (const auto &row: rows) {
            const int suffixLength = row.lifecycle.empty() ? 0 : int(length(row.lifecycle)) + 3;
            // Preserve Local Play's ordinary allocation (Ranking measures stored name bytes).
            nameCharacters = std::max(nameCharacters, int(row.name.size()) + suffixLength);
            minimumNameWidth = std::max(minimumNameWidth,
                    (1 + suffixLength) * T::GlyphWidth + T::PlayerNameInset);
            for (std::size_t col = 0; col < row.values.size(); ++col)
                desiredColumns[col] = std::max(desiredColumns[col], int(row.values[col].size()) + 1);
        }
        const int edgeClearance = 16;
        const int tableInset = ScoreSummaryGeometry::SidePadding + ScoreSummaryGeometry::StripOverhang;
        const int tableWidth = std::min(T::preferredWidth(nameCharacters, desiredColumns),
                                        clientWidth - 2 * (edgeClearance + tableInset));
        // First contain names, retaining an indication plus the full lifecycle
        // suffix. Only then spend the remaining whole glyphs on numeric growth.
        int extra = (tableWidth - minimumNameWidth - T::numericWidth(columns)) / T::GlyphWidth;
        bool expanded = true;
        while (extra > 0 && expanded) {
            expanded = false;
            for (std::size_t col = 0; col < desiredColumns.size() && extra > 0; ++col) {
                if (columns[col] >= desiredColumns[col]) continue;
                ++columns[col]; --extra; expanded = true;
            }
        }
        result.outcomeRows = wrap(outcome, std::max(1, tableWidth / 8));

        // Reserve the actual status and action/waiting stack above the external
        // counter. Only the ranking body yields space to this fixed footer.
        result.phaseY = footer(statusRows).phaseY;
        result.outcomeY = result.phaseY + 22;
        const int minimumBottom = result.outcomeY + int(result.outcomeRows.size()) * 18 + 16;
        const int maximumTop = clientHeight - edgeClearance;
        const int bodyBudget = std::max(T::TextHeight, maximumTop - minimumBottom - ScoreSummaryGeometry::BaseHeight);
        int used = 0;
        result.maximumFirst = rows.size();
        while (result.maximumFirst > 0) {
            const auto previous = result.maximumFirst - 1;
            const int added = T::TextHeight + (result.maximumFirst < rows.size()
                    && rows[result.maximumFirst].separatorBefore ? T::SeparatorHeight : 0);
            if (used + added > bodyBudget) break;
            used += added; --result.maximumFirst;
        }
        result.first = std::min(requestedFirst, result.maximumFirst);
        used = 0;
        for (auto index = result.first; index < rows.size(); ++index) {
            const int added = T::TextHeight + (index > result.first && rows[index].separatorBefore ? T::SeparatorHeight : 0);
            if (used + added > bodyBudget) break;
            used += added; ++result.count;
        }
        static_cast<T::Layout &>(result) = T::layout(tableWidth, result.maximumFirst != 0 ? bodyBudget : used,
                clientWidth, clientHeight, minimumBottom, maximumTop, columns);
        // Keep the plain network footer adjacent to a centered local panel.
        const int shift = result.bottom - minimumBottom;
        result.phaseY += shift; result.outcomeY += shift;
        return result;
    }

    std::string rowLabel(const Row &row, int availablePixels) {
        const auto columns = std::size_t(std::max(0, availablePixels / T::GlyphWidth));
        const std::string suffix = row.lifecycle.empty() ? "" : " • " + row.lifecycle;
        const auto suffixLength = length(suffix);
        if (columns <= suffixLength) return clip(suffix, columns);
        return clip(row.name, columns > suffixLength ? columns - suffixLength : 0) + suffix;
    }

    std::string numericText(const Row &row, const Layout &layout) {
        return T::numericText(row.values, layout.columnCharacters, true);
    }

    std::vector<T::Row> visibleRows(const std::vector<Row> &rows, const Layout &layout) {
        std::vector<T::Row> result;
        result.reserve(layout.count);
        for (auto index = layout.first; index < layout.first + layout.count; ++index) {
            const auto &row = rows[index];
            const int padding = row.team ? 0 : T::PlayerNameInset;
            result.push_back({rowLabel(row, layout.nameWidth - padding), numericText(row, layout),
                              row.nameColor, row.background, row.team, row.separatorBefore});
        }
        return result;
    }
}
