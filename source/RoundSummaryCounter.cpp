#include "RoundSummaryCounter.h"
#include "Font.h"

namespace Duel6::RoundSummaryCounter {
    void render(Renderer &renderer, const Font &font, int clientWidth, const std::string &text) {
        const int textWidth = font.getTextWidth(text, TextHeight);
        const float left = (clientWidth - textWidth - 2 * HorizontalPadding) * 0.5f;
        renderer.setBlendFunc(BlendFunc::None);
        renderer.quadXY(Vector(left, Bottom), Vector(textWidth + 2 * HorizontalPadding, Height), Color::BLACK);
        font.print(left + HorizontalPadding, Bottom + VerticalPadding, 0, Color::WHITE, text, TextHeight);
    }
}
