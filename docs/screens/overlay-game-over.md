# OVER-03 — Game-over summary

## Purpose and traceability

This overlay reports final ranking after the last round of a finite match.
Entry occurs when the configured final round gains a winner or no-winner result.
Exit occurs when Escape closes gameplay after the final wait has completed, or when Shift+Escape closes gameplay earlier.
The overlay implements `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014`, `UI-GAME-001`–`UI-GAME-004`, and `AC-070`–`AC-071` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp:51-79,158-164`, `source/Round.cpp:231-237`, and `source/WorldRenderer.cpp:166-169,565-570`.

## Layout and hierarchy

- The overlay must match [`overlay-game-over.md`](wireframes/overlay-game-over.md).
- The arena must remain visible behind the dark red curtain.
- The arena must remain one undivided shared view.
- The final score panel must use the same visual table structure as round over.
- The final score must include K, A, D, K/D, and PTS.
- Final Elo updates do not appear as a separate gameplay panel.
- In Team deathmatch, each team row must stay directly adjacent to that team's nested player rows.
- In Team deathmatch, an 8 px separator band must separate adjacent team groups.
- A 2 px `team-group-separator` rule must cross the score-table width at the vertical center of each separator band.
- Each separator band must keep 3 px of clear inner-panel space above and below the rule.
- The last team group must not have a separator band after it.
- The final Team panel height must add 8 px for each boundary between team groups.
- The separator treatment must support two through four teams.
- The separator treatment must not change team names, team colors, row colors, score values, ranking order, columns, or row alignment.
- The final Team state must reserve a dedicated notice region at the bottom of the client.
- The notice region must show the exact text `End of Game`.
- The notice must use white 32 px score-summary text on a solid blue rectangular surface.
- The notice surface must keep at least 16 px of horizontal text padding and 8 px of vertical text padding.
- The notice must align to the horizontal center of the client.
- The bottom edge of the notice must be 16 px from the bottom client edge.
- The notice and the score panel must have separate visible bounds.
- The notice must keep at least 16 px of clear space from the score panel.
- The score panel must keep its current client-centered position when that position satisfies the clear-space requirement.
- The score panel may move upward only by the minimum distance needed to satisfy the clear-space requirement.
- The notice must not overlap, clip, cover, replace, or reduce any score heading, row, value, or panel content.

## Content and containment

- The final Team state must support two through four teams and two through 15 players.
- The score panel width must continue to grow through the existing ranking text-length calculation.
- A long player name must use the existing score-table width behavior.
- Score rows and the `End of Game` text must remain on one line.
- The overlay must not add wrapping, truncation, or scrolling.
- The score panel and notice must remain inside the complete client area at the representative 1280 by 900 viewport.
- The notice region must not cover the live ranking, outcome message, or final round progress when those elements remain visible.
- The curtain must remain behind the score panel and the notice.

## States, controls, and recovery

- The state must occur only when Rounds is greater than zero and the final configured round ends.
- A completed finite Deathmatch must update Elo on the final round; Predator and Team deathmatch must not update Elo.
- Every mode must preserve the shared arena overlay geometry.
- Escape must close gameplay when the round is over.
- Shift+Escape must close gameplay at any time.
- The state must not automatically start another round.
- The menu must become the next visible context after close.
- The overlay has no pointer control.
- The notice must not create a pointer target or a keyboard focus target.
- The implementation must not show `Game Over`, `Exit`, or `Return to menu` text.

## Accessibility and viewport behavior

- Column headings must identify score values.
- Team names must supplement team colors.
- The separator rule and separator space must identify team boundaries without color.
- The literal `End of Game` must identify the final state without reliance on curtain color or workflow timing.
- The notice text and surface must preserve high contrast over every arena background and every curtain-opacity frame.
- The panel and curtain must adapt to current client dimensions.
- The notice must remain horizontally centered and 16 px above the bottom edge at each supported desktop viewport.
- The score panel must preserve the clear-space requirement at each supported desktop viewport.
- If the viewport cannot contain the existing score panel and the notice at their preferred positions, the layout must move the score panel upward before it permits overlap.
- The existing oversized-score-panel behavior remains unchanged after the layout uses the available non-overlapping height.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Observable acceptance

- A final limited four-team summary must show three separator bands.
- Each separator band must match the non-final Team summary in width, height, rule thickness, opacity, and inner spacing.
- The final score must show every expected team row and nested player row.
- The bottom notice must read exactly `End of Game`.
- The notice must remain legible against the final curtain and arena.
- A visible clear gap must separate the notice from the score panel.
- No score content may appear below, behind, or inside the notice.
- A non-Team final summary must keep its current separator-free layout and must not gain the Team-only treatment.

## Screenshot link

Representative evidence: [`SS-012`](../screenshots/README.md#ss-012).
