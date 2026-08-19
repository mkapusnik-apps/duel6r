# PLAY-03 — Three-player split-screen

## Purpose and traceability

This layout gives three players separate cameras during active gameplay.
Entry occurs when a three-player round receives F2 from full-screen mode.
Exit occurs when F2 restores full-screen mode or the round changes.
The layout implements `UI-002`–`UI-007`, `UI-009`–`UI-010`, and `UI-014` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:129-134` and `source/WorldRenderer.cpp:389-415,508-527`.

## Layout and hierarchy

- The layout must match [`play-split-3.md`](wireframes/play-split-3.md).
- Player 3 must use the centered upper half-size view.
- Player 1 must use the lower-left half-size view.
- Player 2 must use the lower-right half-size view.
- Black unused side regions must flank the upper view.
- A 4 px red frame must bound each camera and separate the views.
- Each view must render its own arena camera and applicable player messages.

## States, controls, and recovery

- A dead player's view must receive a translucent red curtain.
- The global live ranking must not appear in split-screen mode.
- Screen-level round and FPS counters may remain visible.
- F2 must restore the shared full-screen view.
- The implementation has no empty-camera or disconnected-player visual state.

## Accessibility and viewport behavior

- Position must identify player order before color.
- Red boundaries must identify view separation.
- The layout must derive all view dimensions from the current client area.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-005`](../screenshots/README.md#ss-005).
