#include <algorithm>
#include <filesystem>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include "TestHarness.h"

// Follow the existing application-test access convention, without adding a
// production test API. All application sources in this target use Mac layout.
#define private public
#include "source/Application.h"
#undef private

namespace {
    using namespace Duel6;
    using Viewport = std::array<GLint, 4>;
    int drawableWidth = 1280, drawableHeight = 900;
    bool drawableAvailable = true;
    std::vector<Viewport> viewportWrites;

    Viewport viewport() {
        Viewport value{};
        glGetIntegerv(GL_VIEWPORT, value.data());
        return value;
    }

    void expectFullDrawable() {
        const Viewport expected{0, 0, drawableWidth, drawableHeight};
        D6R_REQUIRE_EQ(expected, viewport());
        D6R_REQUIRE(!viewportWrites.empty());
        for (const auto &write : viewportWrites) D6R_REQUIRE_EQ(expected, write);
        viewportWrites.clear();
    }

    struct Paths {
        const std::filesystem::path support = std::filesystem::path(D6R_TEST_BINARY_DIR) / "mac-viewport-persons";
        Paths() {
            std::filesystem::remove_all(support);
            MacLocal::preparePaths(std::filesystem::path(D6R_TEST_SOURCE_DIR) / "resources", support);
        }
        ~Paths() { std::filesystem::remove_all(support); }
    };

    struct ContextCleanup {
        ~ContextCleanup() { while (Context::exists()) Context::getCurrent().close(); }
    };

    void clickLogical(Application &app, int x, int y) {
        auto &menu = *app.menu;
        int windowWidth, windowHeight;
        SDL_GetWindowSize(app.video->window, &windowWidth, &windowHeight);
        // Encode a point inside the displayed logical control in SDL window
        // coordinates. Dispatch uses the unchanged production pointer transform.
        const int screenWidth = app.video->getScreen().getClientWidth();
        const int screenHeight = app.video->getScreen().getClientHeight();
        SDL_Event event{};
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.state = SDL_PRESSED;
        event.button.x = int((menu.menuTranslationX + x * menu.menuScale) * windowWidth / screenWidth);
        event.button.y = int((screenHeight - menu.menuTranslationY - y * menu.menuScale) * windowHeight / screenHeight);
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event));
        event.type = SDL_MOUSEBUTTONUP;
        event.button.state = SDL_RELEASED;
        D6R_REQUIRE_EQ(1, SDL_PushEvent(&event));
        app.processEvents(menu);
    }
}

// Only SDL's size report is modelled; rendering and GL viewport writes are real.
extern "C" void __wrap_SDL_GL_GetDrawableSize(SDL_Window *, int *width, int *height) {
    if (drawableAvailable) {
        *width = drawableWidth;
        *height = drawableHeight;
    }
}

extern "C" void __real_glViewport(GLint, GLint, GLsizei, GLsizei);
extern "C" void __wrap_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    viewportWrites.push_back({x, y, width, height});
    __real_glViewport(x, y, width, height);
}

D6R_TEST_CASE("Mac drawable reset queries current pixels and retains viewport on unavailable surfaces") {
    Paths paths;
    char name[] = "mac-viewport-tests";
    char *arguments[] = {name};
    Application app(1, arguments);
    const Matrix projection = app.video->getRenderer().getProjectionMatrix();
    for (const auto size : {std::array<int, 2>{1280, 900}, {2560, 1800}, {1920, 1125}, {1001, 751}}) {
        drawableWidth = size[0]; drawableHeight = size[1];
        viewportWrites.clear();
        app.video->resetDrawableViewport();
        expectFullDrawable();
        for (int index = 0; index < 16; ++index)
            D6R_REQUIRE_EQ(projection.getStorage()[index], app.video->getRenderer().getProjectionMatrix().getStorage()[index]);
        D6R_REQUIRE_EQ(1280, app.video->getScreen().getClientWidth());
        D6R_REQUIRE_EQ(900, app.video->getScreen().getClientHeight());
    }
    const Viewport retained = viewport();
    for (const auto size : {std::array<int, 2>{0, 0}, {0, 900}, {1280, 0}, {-1, 900}, {1280, -1}}) {
        drawableWidth = size[0]; drawableHeight = size[1];
        app.video->resetDrawableViewport();
        D6R_REQUIRE_EQ(retained, viewport());
        D6R_REQUIRE(viewportWrites.empty());
    }
    drawableAvailable = false;
    app.video->resetDrawableViewport();
    D6R_REQUIRE_EQ(retained, viewport());
    D6R_REQUIRE(viewportWrites.empty());
    drawableAvailable = true;
    drawableWidth = 1600; drawableHeight = 1200;
    app.video->resetDrawableViewport();
    expectFullDrawable();
}

D6R_TEST_CASE("Mac arena overlays and returned menu restore drawable without changing pointer or layout") {
    Paths paths;
    char name[] = "mac-viewport-transitions";
    char *arguments[] = {name};
    Application app(1, arguments);
    ContextCleanup cleanup;
    auto &menu = *app.menu;
    auto &game = *app.game;
    Context::push(menu);
    const float menuScale = menu.menuScale;
    const int menuX = menu.menuTranslationX, menuY = menu.menuTranslationY;
    for (const char *person : {"Viewport One", "Viewport Two"}) {
        menu.textbox->setText(person);
        menu.addPerson();
    }
    menu.addPlayer(0); menu.addPlayer(1);
    menu.roundsTextbox->setText("2");

    for (const auto size : {std::array<int, 2>{1280, 900}, {2560, 1800}, {1920, 1125}}) {
        drawableWidth = size[0]; drawableHeight = size[1];
        // A stale viewport must not leak into the menu, even before gameplay.
        glViewport(11, 17, 320, 225);
        viewportWrites.clear();
        menu.render();
        expectFullDrawable();
        clickLogical(app, 80, 300);
        D6R_REQUIRE(menu.textbox->isFocused());
        clickLogical(app, 808, 415);
        D6R_REQUIRE(menu.roundsTextbox->isFocused());
        menu.roundsTextbox->setText("2");
        menu.play({"levels/duel_01.json"});
        D6R_REQUIRE(Context::getCurrent().is(game));
        D6R_REQUIRE_EQ(2, game.getPlayers().size());

        viewportWrites.clear();
        game.render(); // GL1 background callback, live arena and HUD reset.
        D6R_REQUIRE(viewportWrites.size() >= 3);
        expectFullDrawable();
        game.displayScoreTab = true;
        game.render();
        expectFullDrawable();
        // Render-state fixtures exercise real summary/curtain branches. These
        // are automated boundary checks, not genuine native result captures.
        game.getRound().winner = true;
        game.render();
        expectFullDrawable();
        game.nextRound();
        game.render();
        expectFullDrawable();
        game.getRound().winner = true;
        game.render();
        expectFullDrawable();
        if (!app.console.isActive()) app.console.toggle();
        app.video->renderConsole(app.console, *app.font);
        D6R_REQUIRE_EQ((Viewport{0, 0, drawableWidth, drawableHeight}), viewport());
        app.console.toggle();
        game.close();
        D6R_REQUIRE(Context::getCurrent().is(menu));
        // Simulate a changed backing surface between game and menu frames.
        drawableWidth += 137; drawableHeight += 91;
        viewportWrites.clear();
        menu.render();
        expectFullDrawable();
        D6R_REQUIRE_EQ(menuScale, menu.menuScale);
        D6R_REQUIRE_EQ(menuX, menu.menuTranslationX);
        D6R_REQUIRE_EQ(menuY, menu.menuTranslationY);
        D6R_REQUIRE_EQ(std::string("2"), menu.roundsTextbox->getText());
        clickLogical(app, 80, 300);
        D6R_REQUIRE(menu.textbox->isFocused());
        clickLogical(app, 808, 415);
        D6R_REQUIRE(menu.roundsTextbox->isFocused());
    }
}
