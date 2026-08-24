# CONS-01 — Console over menu

## Purpose and traceability

This overlay provides runtime inspection and commands while the menu remains visible.
Entry occurs when backquote is pressed from the menu.
Exit occurs when backquote is pressed again or a console command closes the console.
The overlay implements `CFG-001`–`CFG-002` and `CFG-008`–`CFG-020` from [`docs/features.md`](../features.md).
Primary sources are `source/Application.cpp:158-185`, `source/Video.cpp:82-90`, and `source/console/ConsoleRenderer.cpp`.

## Layout and hierarchy

- The overlay must match [`console-menu.md`](wireframes/console-menu.md).
- The complete menu must remain rendered behind the console.
- The visible menu area behind the console must use the approved black matte and four-panel canvas.
- The visible Game Settings area must include the Burnable Trees checkbox from `MENU-01`.
- The console must span the complete client width at the top of the visible client area.
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
- The menu canvas under the console must remain centered in the complete client area.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-013`](../screenshots/README.md#ss-013).
