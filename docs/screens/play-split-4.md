# PLAY-04 — Four-player split-screen

## Purpose and traceability

This layout gives four players separate cameras during active gameplay.
Entry occurs when a four-player round receives F2 from full-screen mode.
Exit occurs when F2 restores full-screen mode or the round changes.
The layout implements `UI-002`–`UI-007`, `UI-009`–`UI-010`, and `UI-014` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:136-142` and `source/WorldRenderer.cpp:389-415,508-527`.

## Layout and hierarchy

- The layout must match [`play-split-4.md`](wireframes/play-split-4.md).
- Player 1 must use the upper-left view.
- Player 2 must use the upper-right view.
- Player 3 must use the lower-left view.
- Player 4 must use the lower-right view.
- Red gutters must form the two-by-two boundaries.
- Each view must render its own arena camera and applicable player messages.

## States, controls, and recovery

- A dead player's view must receive a translucent red curtain.
- The global live ranking must not appear in split-screen mode.
- Screen-level round and FPS counters may remain visible.
- F2 must restore the shared full-screen view.
- Five-player and larger matches must not enter this layout.
- The implementation has no empty-camera visual state.

## Accessibility and viewport behavior

- Quadrant position must identify player order before color.
- Red boundaries must identify view separation.
- The layout must derive all view dimensions from the current client area.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-006`](../screenshots/README.md#screenshot-matrix).
