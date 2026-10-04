#ifndef DUEL6_MAC_POINTER_H
#define DUEL6_MAC_POINTER_H

#include <SDL2/SDL_events.h>

namespace Duel6::MacLocal {
    // Invert the window -> drawable -> GL viewport mapping without changing the
    // existing projection or menu layout. Output remains top-left based: the
    // ordinary Application conversion performs the Y inversion exactly once.
    class PointerTransform {
        double scaleX = 1, scaleY = 1, offsetX = 0, offsetY = 0;

    public:
        PointerTransform(int windowWidth, int windowHeight, int drawableWidth, int drawableHeight,
                         int viewportX, int viewportY, int viewportWidth, int viewportHeight,
                         int screenWidth, int screenHeight) {
            // A minimized/unavailable surface must not cause division by zero.
            if (windowWidth <= 0 || windowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0
                || viewportWidth <= 0 || viewportHeight <= 0) return;
            scaleX = double(drawableWidth) * screenWidth / windowWidth / viewportWidth;
            scaleY = double(drawableHeight) * screenHeight / windowHeight / viewportHeight;
            offsetX = -double(viewportX) * screenWidth / viewportWidth;
            offsetY = -double(drawableHeight - viewportY - viewportHeight) * screenHeight / viewportHeight;
        }

        void position(int &x, int &y) const {
            x = int(x * scaleX + offsetX);
            y = int(y * scaleY + offsetY);
        }

        void event(SDL_Event &event) const {
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                position(event.button.x, event.button.y);
            } else if (event.type == SDL_MOUSEMOTION) {
                position(event.motion.x, event.motion.y);
                event.motion.xrel = int(event.motion.xrel * scaleX);
                event.motion.yrel = int(event.motion.yrel * scaleY);
            }
        }
    };
}

#endif
