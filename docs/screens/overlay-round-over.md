# OVER-02 — Round-over summary

## Purpose and traceability

This overlay reports ranking after a non-final round and provides the transition to the next round.
Entry occurs when the game mode reports a winner or no winner in a non-final finite match or an unlimited match.
Exit occurs automatically after the wait or through an accepted advance input.
The overlay implements `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012`, `UI-RND-001`–`UI-RND-006`, and `AC-044` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:146-180`, `source/Game.cpp:51-79,130-169`, and `source/WorldRenderer.cpp:120-178,502-534`.

## Layout and hierarchy

- The overlay must match [`overlay-round-over.md`](wireframes/overlay-round-over.md).
- The arena must remain visible behind a dark red curtain.
- The arena must remain one undivided shared view.
- The centered score panel must use the same structure as the score-tab overlay.
- A non-final limited-match summary must show `Rounds: <played>|<total>` in the top-right corner of the score panel.
- The label must align to the right inside the solid blue heading strip.
- The label must use the white 32 px score-summary text style.
- The label must not overlap the centered `---SCORE---` heading.
- The score panel must be wide enough to keep the heading and the label legible.
- Outcome event messages may remain visible with the arena overlays.
- The existing round progress above the arena must remain visible when a finite limit exists.

## States, controls, and recovery

- A sole Deathmatch survivor must receive `You have won!`.
- A no-winner result must produce `End of round - no winner`.
- Predator and team modes must use their implemented outcome messages.
- Deathmatch, Predator, and Team deathmatch must use the same shared arena overlay geometry.
- `<played>` must include the round that has just ended.
- `<played>` must include rounds completed before a resumed match.
- `<total>` must equal the configured positive round limit.
- An unlimited-match summary must not show the score-panel round-progress label.
- A final game summary must remain unchanged and must not show the new score-panel round-progress label.
- An active-round Tab score overlay must remain unchanged and must not show the new score-panel round-progress label.
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
- The score-panel round-progress label must stay at the panel's top-right corner at each supported desktop viewport.
- The implementation does not show an explicit `Continue` prompt.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-011`](../screenshots/README.md#ss-011).
