# OVER-03 — Game-over summary

## Purpose and traceability

This overlay reports final ranking after the last round of a finite match.
Entry occurs when the configured final round gains a winner or no-winner result.
Exit occurs when Escape closes gameplay after the final wait has completed, or when Shift+Escape closes gameplay earlier.
The overlay implements `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014`, `UI-GAME-001`–`UI-GAME-007`, and `AC-070`–`AC-072` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp:51-79,158-164`, `source/Round.cpp:231-237`, and `source/WorldRenderer.cpp:121-230,554-583`.

## Layout and hierarchy

- The overlay must match [`overlay-game-over.md`](wireframes/overlay-game-over.md).
- The arena must remain visible behind the dark red curtain.
- The arena must remain one undivided shared view.
- The final score panel must use the same visual table structure as round over.
- The final score panel must remain the primary information region.
- The top round counter and bottom completion notice must remain secondary status regions.
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
- A final limited Deathmatch or Team state must reserve a dedicated notice region at the bottom of the client.
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
- A final limited Deathmatch or Team state must show the final round counter in the top-center counter region.
- The final round counter must preserve the current round-counter content and values.
- The final round counter and the `---SCORE---` heading must use the same 32 px character height.
- The final round counter must use white text on the existing opaque black counter backing.
- The backing width must grow from the measured complete counter text.
- The backing must keep visible inner space on the left and right of the counter text.
- Every rendered counter character must remain inside the backing and the client bounds.
- The layout must first use the available middle height between the top counter allocation and the bottom notice allocation.
- A score panel that fits in the available middle height must keep at least 16 px of clear arena or curtain space from the counter backing.
- A score panel that fits in the available middle height must not overlap the counter backing.
- A score panel that is taller than the available middle height may overlap the counter backing.
- An oversized score panel must use the minimum backing overlap needed after the layout uses the complete available middle height.
- The overlap must not reach, cover, clip, or reduce the contrast of any counter character.
- The overlap must not cover, clip, or reduce the contrast of any score heading, row, value, or separator.

## Content and containment

- The final Team state must support two through four teams and two through 15 players.
- The final Deathmatch state must support two through 15 players.
- The backing must contain the complete counter when the final round and configured limit use multiple digits.
- The score panel width must continue to grow through the existing ranking text-length calculation.
- A long player name must use the existing score-table width behavior.
- Score rows and the `End of Game` text must remain on one line.
- The overlay must not add wrapping, truncation, or scrolling.
- The counter text, score content, and notice must remain inside the complete client area at the representative 1280 by 900 viewport.
- The notice region must not cover the live ranking, outcome message, or final round progress when those elements remain visible.
- The curtain must remain behind the score panel and the notice.

## States, controls, and recovery

- The state must occur only when Rounds is greater than zero and the final configured round ends.
- The `End of Game` notice and the enlarged final round counter must appear only in the final limited Deathmatch and Team variants.
- Predator, non-final, unlimited, and active-round score overlays must remain unchanged.
- A completed finite Deathmatch must update Elo on the final round; Predator and Team deathmatch must not update Elo.
- Every mode must preserve the shared arena overlay geometry.
- Escape must close gameplay when the round is over.
- Shift+Escape must close gameplay at any time.
- The state must not automatically start another round.
- The menu must become the next visible context after close.
- The overlay has no pointer control.
- The notice must not create a pointer target or a keyboard focus target.
- The final round counter and score panel must not create pointer targets or keyboard focus targets.
- The implementation must not show `Game Over`, `Exit`, or `Return to menu` text.

## Accessibility and viewport behavior

- Column headings must identify score values.
- Team names must supplement team colors.
- The separator rule and separator space must identify team boundaries without color.
- The literal `End of Game` must identify the final state without reliance on curtain color or workflow timing.
- The notice text and surface must preserve high contrast over every arena background and every curtain-opacity frame.
- The white final round counter must preserve high contrast against its opaque black backing.
- The panel and curtain must adapt to current client dimensions.
- The notice must remain horizontally centered and 16 px above the bottom edge at each supported desktop viewport.
- The final round counter backing must remain horizontally centered in the top counter region at each supported desktop viewport.
- The final round counter text must remain fully contained and readable at each supported desktop viewport.
- The score panel must apply the fitted-panel or oversized-panel rule at each supported desktop viewport.
- The layout must allocate top-counter, middle-score, and bottom-notice regions before it permits overlap.
- The layout must keep at least 16 px between the counter backing and a score panel that fits in the middle region.
- If the score panel is taller than the middle region, the panel may extend into the counter backing by only the required excess height.
- In the oversized state, the counter text must remain visually above any intersecting backing surface.
- In the oversized state, score content must remain visually distinct from the counter text and backing.
- The oversized state must not wrap, truncate, scale down, or remove counter text or score content to create separation.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Observable acceptance

- A final limited four-team summary must show three separator bands.
- Each separator band must match the non-final Team summary in width, height, rule thickness, opacity, and inner spacing.
- The final score must show every expected team row and nested player row.
- A final limited Deathmatch summary and a final limited Team summary must show the exact `End of Game` notice.
- The bottom notice must read exactly `End of Game`.
- The notice must remain legible against the final curtain and arena.
- A visible clear gap must separate the notice from the score panel.
- No score content may appear below, behind, or inside the notice.
- The final round counter and `---SCORE---` must have the same visible character height.
- The final round counter backing must contain the complete counter with visible inner space.
- A fitted score panel must keep a visible gap of at least 16 px from the final round counter backing.
- An oversized score panel may overlap only the counter backing by the minimum required vertical depth.
- An oversized score panel must leave every counter character and every item of score content complete, unobscured, and readable.
- A Deathmatch final summary must keep its separator-free score table.
- Only a Team final summary may use the Team-group separator treatment.

## Screenshot link

Representative evidence: [`SS-012`](../screenshots/README.md#ss-012).
