# PLAY-01 — Live shared arena gameplay

## Purpose and traceability

This screen presents the active arena and the immediate combat status for all local players.
Entry occurs after the Play workflow creates a round.
Exit occurs when the round advances, the final game closes, or Shift+Escape closes gameplay.
The screen implements the live-play requirements in `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, and `UI-001`–`UI-020` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp`, `source/Round.cpp`, `source/WorldRenderer.cpp`, and `resources/levels/`.

## Layout and hierarchy

- One undivided arena must fill the current client area as shown in [`play-fullscreen.md`](wireframes/play-fullscreen.md).
- The arena must show the whole level and all players.
- The same layout must support two through 15 players.
- The screen must not contain player-specific camera regions or camera separators.
- The selected level background and static geometry must form the base layer.
- Elevators, pickups, players, water, indicators, shots, and explosions must use the implemented render order.
- Event messages must stack from the upper-left area in wireframe coordinates.
- Live ranking must occupy the upper-right area in wireframe coordinates when ranking is on.
- A finite-round counter must appear at the top center in wireframe coordinates.
- Player status must remain near each player in world space.

## Visible behavior and state variants

- Entry must require at least one successfully loaded level and at least one enabled weapon.
- A failed level or weapon prerequisite must prevent creation of this screen.
- A failed prerequisite must keep the user in the menu workflow and must show `MENU-02`.
- A valid start must keep the existing arena entry and start presentation unchanged.
- Round start must use a blue-dark fade and yellow player-location rings.
- Live ranking must show name and points.
- Living ranking text must be yellow.
- Dead ranking text must be red.
- Event messages must identify relevant players.
- A status request or state change may reveal name, ammunition, reload, air, bonus, health, and round-kill cues.
- Pickups and dropped weapons must remain visible world objects.
- Water must remain a translucent world hazard.
- An empty event queue must leave the upper-left area clear.
- Ranking off must leave the upper-right area clear.
- An unlimited match must omit the round counter.
- Resource load failures have no in-screen recovery state.

## Controls and feedback

- Assigned player controls must drive movement, jump, crouch, shooting, weapon pick or swap, and status display.
- F2 must not change the gameplay view.
- F4 must toggle live ranking.
- Tab must toggle the score overlay before a winner exists.
- F10 must save a screenshot and report the path to the console.
- Shift+Escape must close gameplay.

## Accessibility and viewport behavior

- Player names and spatial positions must supplement player colors when the name indicator is visible.
- Bar length must supplement status-bar color.
- Event text must supplement combat animation and sound.
- The arena camera must adapt to the current client dimensions.
- Every supported player count must remain in this shared arena layout.
- Predator, Team deathmatch, and Deathmatch must use this layout.
- Sudden death, score summaries, round progress, status, ranking, and console states must preserve this layout.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-003`](../screenshots/README.md#ss-003).
