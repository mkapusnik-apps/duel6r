# PLAY-01 — Live full-screen gameplay

## Purpose and traceability

This screen presents the active arena and the immediate combat status for all local players.
Entry occurs after the Play workflow creates a round.
Exit occurs when the round advances, the final game closes, or Shift+Escape closes gameplay.
The screen implements the live-play requirements in `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, `UI-001`, and `UI-008`–`UI-015` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp`, `source/Round.cpp`, `source/WorldRenderer.cpp`, and `resources/levels/`.

## Layout and hierarchy

- The arena must fill the current client area as shown in [`play-fullscreen.md`](wireframes/play-fullscreen.md).
- The selected level background and static geometry must form the base layer.
- Elevators, pickups, players, water, indicators, shots, and explosions must use the implemented render order.
- Event messages must stack from the upper-left area in wireframe coordinates.
- Live ranking must occupy the upper-right area in wireframe coordinates when ranking is on.
- A finite-round counter must appear at the top center in wireframe coordinates.
- Player status must remain near each player in world space.

## Visible behavior and state variants

- Round start must use a blue-dark fade and yellow player-location rings.
- Live ranking must show name and points.
- Living ranking text must be yellow.
- Dead ranking text must be red.
- Event messages must include player name prefixes in full-screen mode.
- A status request or state change may reveal name, ammunition, reload, air, bonus, health, and round-kill cues.
- Pickups and dropped weapons must remain visible world objects.
- Water must remain a translucent world hazard.
- An empty event queue must leave the upper-left area clear.
- Ranking off must leave the upper-right area clear.
- An unlimited match must omit the round counter.
- Resource load failures have no in-screen recovery state.

## Controls and feedback

- Assigned player controls must drive movement, jump, crouch, shooting, weapon pick or swap, and status display.
- F2 must toggle split-screen when fewer than five players are present.
- F4 must toggle live ranking.
- Tab must toggle the score overlay before a winner exists.
- F10 must save a screenshot and report the path to the console.
- Shift+Escape must close gameplay.

## Accessibility and viewport behavior

- Player names and spatial positions must supplement player colors when the name indicator is visible.
- Bar length must supplement status-bar color.
- Event text must supplement combat animation and sound.
- The arena camera must adapt to the current client dimensions.
- More than four players must remain in this shared full-screen layout.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-003`](../screenshots/README.md#screenshot-matrix).
