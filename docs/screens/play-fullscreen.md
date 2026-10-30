# PLAY-01 — Live shared arena gameplay

## Purpose and traceability

This screen presents the active arena and the immediate combat status for all local players.
Entry occurs after the Play workflow creates a round.
Exit occurs when the round advances, the final game closes, or Shift+Escape closes gameplay.
The screen implements the live-play requirements in `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, and `UI-001`–`UI-020` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp`, `source/Round.cpp`, `source/WorldRenderer.cpp`, and `resources/levels/`.

## Common arena presentation contract

This section owns functional presentation parity for `PLAY-01` and `NET-05`, including their world-state variants and retained arena context in `NET-07` and `NET-09`. Local gameplay rules remain in [features.md](../features.md). Network authority, default visuals, prediction, and recovery remain in the network contracts. UX owns appearance in [design.md](../design.md) and [NET-05](../design/screens/NET-05.md).

**Equivalent presentation inputs** mean the same level, mirror state, world values, applicable visual resources, event age, presentation time, and viewport. They do not mean equal wall-clock arrival times or identical random choices in separately started matches. Local profiles and network default visuals remain distinct supported inputs.

- **PRES-ARENA-001** Local and network play must use the same visible world placement and layer order for equivalent presentation inputs.
- **PRES-ARENA-002** Presentation must treat gameplay and result state as read-only input.
- **PRES-ARENA-003** Network presentation must not create or advance a second gameplay simulation.
- **PRES-ARENA-004** Water must show the type and surface position supplied by the applicable gameplay authority.
- **PRES-ARENA-005** Equivalent water inputs must produce the same visible geometry, surface animation speed, material appearance, and transparency in local and network play.
- **PRES-ARENA-006** Trees and elevators must keep their visual anchors aligned with their world positions in normal and mirrored levels.
- **PRES-ARENA-007** Player status must show gameplay quantities and proportions in their user-facing units, not their encoded transport values.
- **PRES-ARENA-008** Equivalent player states and accepted events must select the same applicable movement, hit, weapon-pick, and state-transition animations for the same visual resources.
- **PRES-ARENA-009** Water-entry events and tree-burning state must show their applicable splash, burn animation, and completed burned state.
- **PRES-ARENA-010** World animation time must advance with the presented active world and stop while that world is frozen in the round-end phase.
- **PRES-ARENA-011** Presentation must restore an ongoing animation or effect at its current applicable age after recovery, without replaying an expired event.
- **PRES-ARENA-012** A new round must not inherit the prior round's transient animation or effect state.

`PRES-ARENA-010` concerns world animation, including water and player animation. It does not stop the round-end countdown, curtain transition, connection feedback, or confirmation controls. Retained non-current context remains identified under `NRP-REC-013`–`NRP-REC-017`; it does not show simulated progress while waiting for current state. Recovery uses the current accepted world and phase, not elapsed disconnected time to advance the retained world. Network prediction of a locally controlled player's pose remains permitted only under `NRP-PRS-005`–`NRP-PRS-010`.

Presentation time and frame rate must not change damage, collision, input eligibility, random choices, water-rise intervals, tree-burning rules, score, or outcome timing. The network simulation retains the 60 Hz requirement in `NRP-BUD-013`. Local profiles, scripts, sound policy, and background selection retain their existing contracts. Network visual resource selection remains governed by `NET-VIS-003`–`NET-VIS-017`.

### Functional states and acceptance

The shared functional states are active world, first-second round-end update, frozen round-end world, and next-round initialization. Network retained non-current context and restored current context remain `NET-07`/resynchronization states, not a new pause action. Final results use the [common final-screen contract](overlay-game-over.md#common-final-screen-contract).

- **PRES-ARENA-AC-001 — Geometry and water:** Equivalent normal and mirrored asymmetric levels preserve world orientation, tree roots, elevator support surfaces, entity placement, and layer order in local and network views. Blue, red, and green water show the correct authoritative type, surface, transparency, and animation speed. Rising water still follows `ENV-009` and `ENV-QL-005`–`ENV-QL-006`.
- **PRES-ARENA-AC-002 — Status values:** Equivalent life, air, ammunition, reload, and bonus values produce equivalent status text and proportions. Include empty, partial, and full values where applicable; transport scaling must not leak into the display.
- **PRES-ARENA-AC-003 — Animation and effects:** Movement, accepted hit, weapon pick, state transitions, water entry, and tree burning show the applicable animations and completed states. Burnable Trees off and the `shit thrower` exclusion retain `ENV-018` and `ENV-021`. Network profiles do not replace default visuals.
- **PRES-ARENA-AC-004 — Time and recovery:** World animations continue in the first second after an outcome, freeze in the final five seconds, and resume through the correct next-round initialization. Countdown and UI transitions remain active. A full snapshot, equivalent incremental state, and recovery restore current effects without restarting a burn, replaying a splash, or restoring a removed entity.
- **PRES-ARENA-AC-005 — Authority and independence:** Presentation changes do not alter gameplay decisions or authoritative outcomes. Network play retains its 60 Hz simulation, allowed movement prediction, solo/waiting rules, and host advancement permissions. Local Play remains available offline with its controls, profiles, scripts, and persistence unchanged.

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
