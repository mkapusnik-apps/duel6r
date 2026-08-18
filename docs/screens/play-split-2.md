# PLAY-02 — Two-player split-screen

## Purpose and traceability

This layout gives two players separate cameras during active gameplay.
Entry occurs when a two-player round receives F2 from full-screen mode.
Exit occurs when F2 restores full-screen mode or the round changes.
The layout implements `UI-002`–`UI-007`, `UI-009`–`UI-010`, and `UI-014` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:100-127` and `source/WorldRenderer.cpp:389-415,508-527`.

## Layout and hierarchy

- The layout must match [`play-split-2.md`](wireframes/play-split-2.md).
- Player 1 must use a centered lower half-size view.
- Player 2 must use a centered upper half-size view.
- Each view must use half the client width minus 4 px and half the client height minus 4 px.
- Black unused side regions must remain visible.
- A 4 px red frame must bound each camera and separate the two views.
- Each view must render its own arena camera.
- Each view must show only that player's event messages.

## States, controls, and recovery

- A dead player's view must receive a translucent red curtain.
- The global live ranking must not appear in split-screen mode.
- A finite-round counter and optional FPS counter must remain screen-level overlays.
- Player status indicators may appear in both cameras when world positions are visible.
- F2 must restore the shared full-screen view.
- F4 may change the ranking setting, but split-screen must continue to hide the live ranking.
- The implementation has no empty-camera state.

## Accessibility and viewport behavior

- Spatial order must identify Player 2 above Player 1.
- Red boundaries must identify view separation.
- The dead-view curtain must supplement the player's absent live state.
- View dimensions must derive from the current client dimensions.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-004`](../screenshots/README.md#ss-004).
