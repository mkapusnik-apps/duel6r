#ifndef DUEL6_ROUND_SUMMARY_COUNTER_H
#define DUEL6_ROUND_SUMMARY_COUNTER_H

#include <string>

namespace Duel6 {
    class Renderer;
    class Font;

    // Shared finite non-final counter, in complete-client bottom-left pixels.
    namespace RoundSummaryCounter {
        constexpr int TextHeight = 32, HorizontalPadding = 8, VerticalPadding = 4;
        constexpr int Bottom = 16, Height = TextHeight + 2 * VerticalPadding, Clearance = 16;
        constexpr int ClearTop = Bottom + Height + Clearance;

        void render(Renderer &renderer, const Font &font, int clientWidth, const std::string &text);
    }
}

#endif
