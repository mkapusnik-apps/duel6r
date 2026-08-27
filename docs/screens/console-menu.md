# CONS-01 — Console over menu

## Purpose and traceability

This overlay provides runtime inspection and commands while the menu remains visible.
The approved target background includes the four equal-width `MENU-01` footer actions with distinct `Network (F2)`. Issue #28 does not implement that background change.
Entry occurs when backquote is pressed from the menu.
Exit occurs when backquote is pressed again or a console command closes the console.
The overlay implements `CFG-001`–`CFG-002` and `CFG-008`–`CFG-020` from [`docs/features.md`](../features.md).
Primary sources are `source/Application.cpp:158-185`, `source/Video.cpp:82-90`, and `source/console/ConsoleRenderer.cpp`.

## Layout and hierarchy

- The overlay must match [`console-menu.md`](wireframes/console-menu.md).
- The complete menu must remain rendered behind the console.
- The visible menu area behind the console must use the approved blurred and scrimmed session background plus the scaled four-panel canvas and keyline.
- The visible Game Settings area must include the Burnable Trees checkbox from `MENU-01`.
- The visible target footer must show `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` with the equal widths, gaps, and outer margins from `MENU-01`.
- The console must span the complete client width at the top of the visible client area.
- The console must remain at client scale and must not inherit the menu transform.
- The console must show up to 15 recent history rows.
- A red separator must sit between history and input.
- The input row must start with `]` or `<`.
- A black 3 px edge must bound the console.

## States, controls, and recovery

- The console must receive keyboard and text input while it is open.
- The underlying menu must not receive keyboard input while the console is open.
- The history separator must use `^` while history is scrolled.
- The input cursor must blink as an underscore or insert block.
- Long input must scroll horizontally and must replace the prompt with `<`.
- Command output and errors must appear in history text.
- Closing the console must restore menu keyboard input.
- Mouse events continue to reach the underlying context in the current application event flow.

## Accessibility and viewport behavior

- The prompt, separator, and text history must provide non-color structure.
- The console does not provide completion hints or a visible close control.
- Console width must recalculate from client width and font character width.
- Console height must derive from font character height and visible-row count.
- The uniformly scaled menu canvas under the console must remain centered in the complete client area.
- The same session-selected background must remain visible below the console and after the console closes.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Planned representative evidence for downstream issue #38: [`SS-013`](../screenshots/README.md#ss-013). The existing capture is not valid for the changed target footer.
