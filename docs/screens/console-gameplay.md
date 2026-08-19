# CONS-02 — Console over gameplay

## Purpose and traceability

This overlay provides runtime inspection and commands while the active arena remains visible.
Entry occurs when backquote is pressed during gameplay.
Exit occurs when backquote is pressed again or a console command closes the console.
The overlay implements `CFG-001`–`CFG-002` and `CFG-008`–`CFG-020` from [`docs/features.md`](../features.md).
Primary sources are `source/Application.cpp:158-185`, `source/Video.cpp:82-90`, and `source/console/ConsoleRenderer.cpp`.

## Layout and hierarchy

- The overlay must match [`console-gameplay.md`](wireframes/console-gameplay.md).
- The current gameplay frame and overlays must remain rendered behind the console.
- The console must span the complete client width at the top of the visible client area.
- The console must show up to 15 recent history rows.
- The console must show history, red separator, input prompt, and cursor.
- The opaque console surface may obscure the upper part of the arena.

## States, controls, and recovery

- The console must receive text input and discrete key-down events while it is open.
- Those discrete key-down events must not reach the gameplay context while the console is open.
- Independently, key-down and key-up events must update shared held-keyboard state, and controller controls must continue reading shared controller state. Either state may continue to cause player actions while the console is open.
- The underlying fixed-step gameplay update must continue.
- Long history and input must use the implemented scrolling cues.
- Closing the console must route subsequent discrete key-down events to the gameplay context and stop text input.
- The console has no pause cue.
- The console has no translucent mode.
- Mouse events continue to reach the underlying context, although gameplay has no mouse actions.

## Accessibility and viewport behavior

- Text history and prompt structure must supplement console color.
- The console does not provide a visible pause warning or close control.
- Console width and height must derive from client and font dimensions.
- The overlay may appear over full-screen or split-screen gameplay without a separate layout.
- The representative wireframe uses full-screen gameplay because the console geometry is unchanged.

## Screenshot link

Representative evidence: [`SS-014`](../screenshots/README.md#ss-014).
