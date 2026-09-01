# OVER-01 — Score-tab overlay

## Purpose and traceability

This overlay lets players inspect detailed match ranking during an active round.
Entry occurs when Tab is pressed before the round has a winner.
Exit occurs when Tab is pressed again or when a new round starts.
The overlay implements `SCO-018`, `MOD-TM-010`–`MOD-TM-011`, and `UI-011` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp:62-83` and `source/WorldRenderer.cpp:120-164,561-563`.

## Layout and hierarchy

- The overlay must match [`overlay-score-tab.md`](wireframes/overlay-score-tab.md).
- The active arena must remain visible behind the centered panel.
- The active arena must remain one undivided shared view.
- The panel must contain the heading `---SCORE---`.
- The panel must contain the column heading `K`, `A`, `D`, `K/D`, and `PTS`.
- Rows must use the current game mode ranking structure.
- Team mode must use team rows with nested player rows.
- Each team row must stay directly adjacent to that team's nested player rows.
- An 8 px separator band must separate adjacent team groups.
- A 2 px `team-group-separator` rule must cross the score-table width at the vertical center of each separator band.
- The separator band must keep 3 px of clear inner-panel space above and below the rule.
- The last team group must not have a separator band after it.
- The panel height must add 8 px for each boundary between team groups.
- The separator treatment must support two through four teams.
- Team names, team colors, row colors, headings, values, ranking order, column alignment, and Tab behavior must remain unchanged.
- Predator and free-for-all modes must preserve their ranking rows.
- The panel must use the translucent outer and inner surfaces from `docs/design.md`.

## States, controls, and recovery

- Tab must toggle the panel only while the round has no winner.
- The arena simulation must remain the underlying context.
- The score values must reflect current persistent match statistics.
- A ranking with long names must increase panel width through the implemented text-length calculation.
- An empty ranking is not reachable after a valid match starts.
- The overlay has no close button and no pointer interaction.

## Accessibility and viewport behavior

- Text headings must identify every numeric column.
- Team names must supplement team row colors.
- The separator rule and separator space must identify team boundaries without color.
- The panel must calculate its center from current client dimensions.
- The panel may grow with ranking row count and name length.
- The implementation does not add scrolling or clipping recovery for an oversized panel.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-010`](../screenshots/README.md#ss-010).
