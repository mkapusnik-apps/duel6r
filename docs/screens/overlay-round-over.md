# OVER-02 — Round-over summary

## Purpose and traceability

This overlay reports ranking after a non-final round and provides the transition to the next round.
Entry occurs when the game mode reports a winner or no winner in a non-final finite match or an unlimited match.
Exit occurs automatically after the wait or through an accepted advance input.
The overlay implements `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012`, `UI-RND-001`–`UI-RND-010`, and `AC-052` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:146-180`, `source/Game.cpp:51-79,130-169`, and `source/WorldRenderer.cpp:120-178,514-547`.

## Layout and hierarchy

- The overlay must match [`overlay-round-over.md`](wireframes/overlay-round-over.md).
- The arena must remain visible behind an animated dark red curtain.
- The arena must remain one undivided shared view.
- The centered score panel must use the same structure as the score-tab overlay.
- The score panel must remain horizontally centered and vertically centered when the reserved footer permits it. It may move upward only as needed to keep the footer clear.
- A non-final limited-match summary must show the existing `Rounds: <played>|<total>` counter at the bottom center of the complete client, outside the score panel. This placement applies equally to Local Play and network summaries; it does not mean the bottom of the score panel or a remaining-rounds calculation.
- The counter must retain white 32 px score-summary text. Its flat opaque black backing must use the measured complete text width plus 8 px of padding on each side, and 4 px of padding above and below the 32 px text bounds.
- The backing must be horizontally centered on the client and have its bottom edge 16 client px above the bottom client edge. Its height is 40 px; in top-left wireframe coordinates its top is `clientHeight - 56`.
- The counter must keep at least 16 px of clear space from the score panel and other summary regions. It must remain readable over the arena and curtain without clipping, smaller text, an ellipsis, a focus target or an input target.
- The old panel-top-right label and its dedicated 32 px row must be removed, not left empty. The score heading becomes the first panel content row with the existing 32 px top padding.
- The solid blue heading strip must be 36 px high.
- The `---SCORE---` text must align to the horizontal center of the heading strip.
- The counter must not overlap or replace the `---SCORE---` heading strip.
- The content width must equal the larger of the score-table width and 200 px; the external counter must not determine panel width.
- The score-table width must equal 16 px times the ranking maximum text length plus 26 characters.
- The translucent outer panel width must equal the content width plus 32 px.
- The solid blue heading strip width must equal the content width plus 42 px.
- The summary content height must equal 32 px multiplied by the sum of three and the number of visible ranking rows, with no reserved progress row.
- Each top-level ranking row and each nested ranking row must count as one visible ranking row.
- In Team deathmatch, each team row must stay directly adjacent to that team's nested player rows.
- In Team deathmatch, an 8 px separator band must separate adjacent team groups.
- A 2 px `team-group-separator` rule must cross the score-table width at the vertical center of each separator band.
- The separator band must keep 3 px of clear inner-panel space above and below the rule.
- The last team group must not have a separator band after it.
- The limited-summary content height for Team deathmatch must add 8 px for each boundary between team groups.
- The separator treatment must support two through four teams.
- Team names, team colors, row colors, headings, values, ranking order, column alignment, controls, and progress behavior must remain unchanged.
- The translucent outer panel height must equal the limited-summary content height plus 64 px.
- Relocating the counter must remove only the old 32 px progress-row allocation; score-table row pitch and content remain unchanged.
- The score table must keep its existing 32 px row height and alignment.
- Outcome event messages may remain visible with the arena overlays.
- The top-center arena round progress must be hidden for every frame that shows the limited-match summary panel.
- The limited-match summary must show only the bottom-center counter as round-count information.

## States, controls, and recovery

- A sole Deathmatch survivor must receive `You have won!`.
- A no-winner result must produce `End of round - no winner`.
- Predator and team modes must use their implemented outcome messages.
- Deathmatch, Predator, and Team deathmatch must use the same shared arena overlay geometry.
- `<played>` must include the round that has just ended.
- `<played>` must include rounds completed before a resumed match.
- `<total>` must equal the configured positive round limit.
- An unlimited-match summary must not show or reserve space for the bottom-center counter and must keep its existing layout.
- A final game summary must not show this non-final counter.
- Final Team and non-Team game summaries must use the [common final-screen contract](overlay-game-over.md#common-final-screen-contract). This non-final overlay does not prescribe their appearance.
- An active-round Tab score overlay must remain unchanged and must not show the bottom-center summary counter.
- The first visible frame of the next active limited round must restore the top-center arena round progress.
- The next active round must not retain the bottom-center summary counter or its reserved footer.
- The dark red curtain must fade in during the game-over wait.
- F1 must advance after a winner exists when the round is not final.
- Shift+F1 must advance before the normal winner condition permits it.
- Any key must advance after the first three seconds of the winner wait.
- Automatic advance must occur when the wait reaches zero for a non-final round.
- The overlay has no pointer control.

## Accessibility and viewport behavior

- Outcome messages must provide textual result cues.
- Column headings must identify score values.
- The separator rule and separator space must identify team boundaries without color.
- The panel and curtain must adapt to current client dimensions.
- The counter backing must keep its exact horizontal center and 16 px bottom inset at each supported desktop viewport, independent of score-table width and scroll position.
- The score heading must remain horizontally centered at each supported desktop viewport.
- The panel must keep its specified pixel dimensions apart from the removed progress row and recalculate its placement from the client dimensions and footer clearance. This relocation adds no Local Play overflow navigation or other large-table behavior.
- The implementation does not show an explicit `Continue` prompt.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-011`](../screenshots/README.md#ss-011).
