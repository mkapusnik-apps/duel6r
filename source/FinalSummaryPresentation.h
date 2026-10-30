#ifndef DUEL6_FINAL_SUMMARY_PRESENTATION_H
#define DUEL6_FINAL_SUMMARY_PRESENTATION_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Color.h"

namespace Duel6 {
    class Font;
    class Renderer;
}

namespace Duel6::FinalSummaryPresentation {
    enum class Scope { Local, NetworkHost, NetworkGuest };
    enum class Action { ReturnToLobby, EndSession, Leave };
    struct ActionState {
        Action action = Action::Leave;
        bool enabled = true;
        bool focused = false;
        bool pressed = false;
    };
    struct Identity {
        std::string name;
        // Empty for local play: never synthesize network identities/ownership.
        std::string player;
        std::string team;
        bool departed = false;
    };
    struct Outcome {
        bool noWinner = true;
        std::string winningTeam;
        std::vector<Identity> winners;
    };
    struct Column {
        std::string heading;
        bool numeric = false;
        int minimumWidth = 0;
    };
    struct Row {
        std::vector<std::string> cells;
        bool nested = false;
        // True on team rows EXCEPT the first team. Never on nested players.
        bool teamSeparatorBefore = false;
        std::optional<Color> teamColor;
    };
    struct Table {
        // Short section label; put all potentially long result metadata in cells.
        std::string title;
        std::vector<Column> columns;
        std::vector<Row> rows;
    };
    struct Model {
        Scope scope = Scope::Local;
        std::uint32_t completedRounds = 0;
        std::uint32_t roundLimit = 0;
        std::uint32_t lastRoundNumber = 0;
        Outcome matchOutcome;
        Outcome lastRoundOutcome;
        // Supply approved display fields only, never endpoints, credentials or
        // raw filesystem/peer diagnostics. Already ordered by the authority.
        // The view NEVER ranks,
        // awards, infers participation, or loads/saves Game/Person/network state.
        Table cumulative;
        std::vector<Table> settings;
        std::vector<Table> rounds;
        std::vector<ActionState> actions;
        bool navigationFocused = false;
        // Preserve local final-wait curtain fade; completed/network uses 200.
        std::uint8_t curtainAlpha = 200;
    };
    // All coordinates, including hit testing, are TOP-LEFT client pixels.
    struct Rect {
        int x = 0, y = 0, width = 0, height = 0;
        bool contains(int px, int py) const;
    };
    struct Layout {
        int width = 0, height = 0;
        Rect panel, title, body, footer, notice, counter;
        Rect rows, navigation, horizontalBack, horizontalForward;
    };
    struct Scroll {
        int vertical = 0; // document pixels
        int horizontal = 0; // pixels; navigation advances complete 8px glyphs
    };
    struct ScrollBounds {
        int vertical = 0, horizontal = 0;
        int contentHeight = 0, contentWidth = 0;
        std::size_t totalRows = 0;
    };
    struct ActionRegion {
        ActionState state;
        Rect bounds;
    };
    struct Command {
        enum class Kind { Fill, Frame, Text };
        Kind kind = Kind::Fill;
        Rect bounds;
        Color color;
        std::string text;
        int size = 16; // text height, or frame line width
    };
    struct Plan {
        Scope scope = Scope::Local;
        Layout layout;
        Scroll scroll;
        ScrollBounds bounds;
        std::vector<ActionRegion> actions;
        std::vector<Command> commands;
    };

    // Font has a fixed half-height glyph advance. UTF-8 clipping in this module
    // uses code-point boundaries and exactly matches Font::getTextWidth.
    Plan compose(const Model &model, int clientWidth, int clientHeight, Scroll requested = {});
    // Caller renders the full retained arena first (without an extra curtain,
    // ranking, final counter or outcome messages), then uses client orthographic
    // projection. No buffers are cleared. The plan owns ALL final-screen chrome.
    // Depth test/write are disabled for overlay and matrices restored afterward.
    // Depth writes are reenabled (including for the next buffer clear); callers
    // explicitly select depth testing before their next 3D pass.
    void render(Renderer &renderer, const Font &font, const Plan &plan);
    std::optional<Action> hitAction(const Plan &plan, int clientX, int clientY);

    enum class LocalInput { PageUp, PageDown, Wheel, Other };
    struct LocalNavigation {
        LocalInput input = LocalInput::Other;
        bool completed = false;
        bool consoleOwnsInput = false;
        bool shift = false;
        int pointerX = 0, pointerY = 0;
        int wheelY = 0; // positive = preceding/up/left
    };
    // Returns input consumption, not whether an offset changed. Never consumes
    // unrelated keys, final-wait inputs, console input or wheel outside the body.
    bool navigateLocal(Scroll &scroll, const Plan &plan, const LocalNavigation &input);
    // Shared bounded movement for EXISTING network bindings: 1 vertical unit is
    // an 18px row, 1 horizontal unit an 8px glyph. Page movement remains 8 rows.
    // Caller retains focus traversal, pointer ownership, confirms and controller.
    void move(Scroll &scroll, const ScrollBounds &bounds, int rows, int columns);
}

#endif
