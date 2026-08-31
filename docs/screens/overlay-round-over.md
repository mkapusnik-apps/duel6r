# OVER-02 — Round-over summary

## Purpose and traceability

This overlay reports ranking after a non-final round and provides the transition to the next round.
Entry occurs when the game mode reports a winner or no winner in a non-final finite match or an unlimited match.
Exit occurs automatically after the wait or through an accepted advance input.
The overlay implements `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012`, `UI-RND-001`–`UI-RND-010`, and `AC-052` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:146-180`, `source/Game.cpp:51-79,130-169`, and `source/WorldRenderer.cpp:120-178,514-547`.

## Layout and hierarchy

- The overlay must match [`overlay-round-over.md`](wireframes/overlay-round-over.md).
- The arena must remain visible behind a dark red curtain.
- The arena must remain one undivided shared view.
- The centered score panel must use the same structure as the score-tab overlay.
- The score panel must align its horizontal and vertical center to the client center.
- A non-final limited-match summary must show `Rounds: <played>|<total>` in a dedicated row above the solid blue heading strip.
- The progress row must be the top content row of the score panel.
- The top edge of the progress row must equal the top outer-panel bound plus 32 px.
- The progress row must span from 16 px inside the left outer-panel bound to 16 px inside the right outer-panel bound.
- The progress label must align to the right edge of the progress row.
- The right edge of the rendered progress label must equal the right outer-panel bound minus 16 px.
- The label must use the white 32 px score-summary text style.
- The progress row must be 32 px high.
- The progress row must use the translucent panel surface and must not use the solid blue heading fill.
- The solid blue heading strip must be 36 px high.
- The `---SCORE---` text must align to the horizontal center of the heading strip.
- The progress-row baseline and the score-heading baseline must be 32 px apart.
- The progress label must not overlap or replace the `---SCORE---` heading strip.
- The content width must equal the largest of the score-table width, 200 px, and the measured progress-label width.
- The score-table width must equal 16 px times the ranking maximum text length plus 26 characters.
- The translucent outer panel width must equal the content width plus 32 px.
- The solid blue heading strip width must equal the content width plus 42 px.
- The limited-summary content height must equal 32 px multiplied by the sum of four and the number of visible ranking rows.
- Each top-level ranking row and each nested ranking row must count as one visible ranking row.
- The translucent outer panel height must equal the limited-summary content height plus 64 px.
- The progress row must add exactly 32 px to the unchanged score-summary panel height.
- The score table must keep its existing 32 px row height and alignment.
- Outcome event messages may remain visible with the arena overlays.
- The top-center arena round progress must be hidden for every frame that shows the limited-match summary panel.
- The limited-match summary must show only the panel progress label as round-count information.

## States, controls, and recovery

- A sole Deathmatch survivor must receive `You have won!`.
- A no-winner result must produce `End of round - no winner`.
- Predator and team modes must use their implemented outcome messages.
- Deathmatch, Predator, and Team deathmatch must use the same shared arena overlay geometry.
- `<played>` must include the round that has just ended.
- `<played>` must include rounds completed before a resumed match.
- `<total>` must equal the configured positive round limit.
- An unlimited-match summary must not show the score-panel round-progress label.
- An unlimited-match summary must not reserve the 32 px progress row.
- A final game summary must remain unchanged and must not show the new score-panel round-progress label.
- An active-round Tab score overlay must remain unchanged and must not show the new score-panel round-progress label.
- The first visible frame of the next active limited round must restore the top-center arena round progress.
- The next active round must not retain the summary progress row.
- The dark red curtain must fade in during the game-over wait.
- F1 must advance after a winner exists when the round is not final.
- Shift+F1 must advance before the normal winner condition permits it.
- Any key must advance after the first three seconds of the winner wait.
- Automatic advance must occur when the wait reaches zero for a non-final round.
- The overlay has no pointer control.

## Accessibility and viewport behavior

- Outcome messages must provide textual result cues.
- Column headings must identify score values.
- The panel and curtain must adapt to current client dimensions.
- The progress label must remain right-aligned 16 px inside the score panel at each supported desktop viewport.
- The score heading must remain horizontally centered at each supported desktop viewport.
- The panel must keep its specified pixel dimensions and must recalculate its center from the client dimensions.
- The implementation does not show an explicit `Continue` prompt.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-011`](../screenshots/README.md#ss-011).
