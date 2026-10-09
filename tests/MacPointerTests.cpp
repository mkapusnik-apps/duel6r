#include "TestHarness.h"
#include "RecordingRenderer.h"
#include "source/platform/MacPointer.h"
#include "source/platform/MacLocal.h"
#include "source/gui/Desktop.h"
#include "source/gui/Button.h"
#include "source/gui/TextBox.h"
#include "source/gui/ListBox.h"

namespace {
    using namespace Duel6;
    using MacLocal::PointerTransform;
    constexpr int width = 1700, height = 1400;

    // A 1.25x centered logical canvas; integer interior targets avoid ambiguous
    // edge rounding. These are modelled dimensions, not measurements of a Mac.
    struct MenuControls {
        Test::RecordingRenderer renderer;
        Gui::Desktop desktop{renderer};
        Gui::Textbox *name = new Gui::Textbox(desktop);
        Gui::Button *add = new Gui::Button(desktop);
        Gui::ListBox *persons = new Gui::ListBox(desktop, true);
        int clicks = 0;
        MenuControls() {
            desktop.screenSize(width, height, 850, 700, 300, 200, 1.25f);
            name->setPosition(14, 308, 30, 10, "ABC");
            add->setPosition(268, 308, 52, 22);
            add->onClick([this](Gui::Button &) { ++clicks; });
            persons->setPosition(14, 535, 36, 12, 18);
            for (int i = 0; i < 20; ++i) persons->addItem(std::to_string(i));
        }
        void button(const PointerTransform &transform, int x, int y, bool down, int count = 1) {
            SDL_Event event{};
            event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            event.button.x = x; event.button.y = y;
            event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.clicks = count;
            transform.event(event);
            desktop.mouseButtonEvent(MouseButtonEvent(event.button.x, height - event.button.y,
                SysEvent::MouseButton::LEFT, down ? SysEvent::ButtonState::PRESSED : SysEvent::ButtonState::RELEASED,
                event.button.clicks == 2));
        }
    };
}

D6R_TEST_CASE("macOS displaced pointer model misses without conversion and focuses/clicks/selects with it") {
    MenuControls menu;
    const PointerTransform identity(width, height, width, height, 0, 0, width, height, width, height);
    // Half-sized window; drawable and viewport remain full-sized. Old code
    // treats window coordinates as projection coordinates and misses targets.
    const PointerTransform scaled(850, 700, width, height, 0, 0, width, height, width, height);
    menu.button(identity, 200, 412, true);
    D6R_REQUIRE(!menu.name->isFocused());
    menu.button(scaled, 200, 412, true); // logical (80, 300)
    D6R_REQUIRE(menu.name->isFocused());
    std::string text = "ABC";
    menu.desktop.textInputEvent(TextInputEvent(text));
    D6R_REQUIRE_EQ(text, menu.name->getText());
    menu.button(identity, 330, 412, true);
    menu.button(identity, 330, 412, false);
    D6R_REQUIRE_EQ(0, menu.clicks);
    menu.button(scaled, 330, 412, true); // logical (288, 300)
    D6R_REQUIRE_EQ(0, menu.clicks);
    menu.button(scaled, 330, 412, false);
    D6R_REQUIRE_EQ(1, menu.clicks);
    menu.button(identity, 200, 275, true);
    D6R_REQUIRE_EQ(-1, menu.persons->selectedIndex());
    menu.button(scaled, 200, 275, true); // logical (80, 520)
    D6R_REQUIRE_EQ(0, menu.persons->selectedIndex());
    int x = 200, y = 275;
    scaled.position(x, y);
    menu.desktop.mouseWheelEvent(MouseWheelEvent(x, height - y, 0, -1));
    menu.button(scaled, 200, 275, true);
    D6R_REQUIRE_EQ(3, menu.persons->selectedIndex());
    int doubleClicks = 0;
    menu.persons->onDoubleClick([&](int index, const std::string &) { doubleClicks += index; });
    menu.button(scaled, 200, 275, true, 2);
    D6R_REQUIRE_EQ(3, doubleClicks);
}

D6R_TEST_CASE("macOS equal-size pointer retains GUI behavior and motion cancels a pressed button") {
    MenuControls menu;
    const PointerTransform identity(width, height, width, height, 0, 0, width, height, width, height);
    menu.button(identity, 400, 824, true);
    D6R_REQUIRE(menu.name->isFocused());
    menu.button(identity, 660, 824, true);
    menu.button(identity, 660, 824, false);
    D6R_REQUIRE_EQ(1, menu.clicks);
    const PointerTransform scaled(850, 700, width, height, 0, 0, width, height, width, height);
    menu.button(scaled, 330, 412, true);
    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 400; motion.motion.y = 500;
    motion.motion.xrel = 70; motion.motion.yrel = 88;
    motion.motion.state = SDL_BUTTON_LMASK;
    scaled.event(motion);
    D6R_REQUIRE_EQ(140, motion.motion.xrel);
    D6R_REQUIRE_EQ(176, motion.motion.yrel);
    menu.desktop.mouseMotionEvent(MouseMotionEvent(motion.motion.x, height - motion.motion.y,
        motion.motion.xrel, motion.motion.yrel, motion.motion.state));
    menu.button(scaled, 330, 412, false);
    D6R_REQUIRE_EQ(1, menu.clicks);
}

D6R_TEST_CASE("macOS mapping uses actual drawable viewport and independent noninteger axis ratios") {
    // Same visual position under different window/backing sizes and an inset
    // viewport. Re-querying dimensions supplies a new transform after a change.
    for (const auto &mapping : {
            PointerTransform(1000, 800, 2000, 1600, 100, 200, 1600, 1200, 1700, 1400),
            PointerTransform(1000, 800, 1000, 800, 50, 100, 800, 600, 1700, 1400)}) {
        int x = 450, y = 400;
        mapping.position(x, y);
        D6R_REQUIRE_EQ(850, x);
        D6R_REQUIRE_EQ(700, y);
        x = 50; y = 100;
        mapping.position(x, y);
        D6R_REQUIRE_EQ(0, x);
        D6R_REQUIRE_EQ(0, y);
    }
    const PointerTransform minimized(0, 0, 0, 0, 0, 0, 0, 0, width, height);
    int x = 12, y = 34;
    minimized.position(x, y);
    D6R_REQUIRE_EQ(12, x);
    D6R_REQUIRE_EQ(34, y);
}

D6R_TEST_CASE("macOS menu and network wheel positions share one window-to-client conversion") {
    PointerTransform mapping(850, 700, 1700, 1400, 0, 0, 1700, 1400, 1700, 1400);
    SDL_Event event{};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.x = 425; event.button.y = 350;
    mapping.event(event);
    int wheelX = 425, wheelY = 350;
    mapping.position(wheelX, wheelY);
    D6R_REQUIRE_EQ(event.button.x, wheelX);
    D6R_REQUIRE_EQ(event.button.y, wheelY);
    D6R_REQUIRE_EQ(850, wheelX);
    D6R_REQUIRE_EQ(700, 1400 - wheelY);
}
