# OVER-03 — Game-over summary

## Purpose and traceability

This overlay reports final ranking after the last round of a finite match.
Entry occurs when the configured final round gains a winner or no-winner result.
Exit occurs when Escape closes gameplay after the final wait has completed, or when Shift+Escape closes gameplay earlier.
The overlay implements `LIF-018`, `SCO-022`–`SCO-023`, and `UI-013`–`UI-014` from [`docs/features.md`](../features.md).
Primary sources are `source/Game.cpp:51-79,158-164`, `source/Round.cpp:231-237`, and `source/WorldRenderer.cpp:166-169,565-570`.

## Layout and hierarchy

- The overlay must match [`overlay-game-over.md`](wireframes/overlay-game-over.md).
- The arena must remain visible behind the dark red curtain.
- The final centered score panel must use the same visual table structure as round over.
- The final score must include K, A, D, K/D, and PTS.
- Final Elo updates do not appear as a separate gameplay panel.

## States, controls, and recovery

- The state must occur only when Rounds is greater than zero and the final configured round ends.
- A completed finite Deathmatch must update Elo on the final round; Predator and Team deathmatch must not update Elo.
- Escape must close gameplay when the round is over.
- Shift+Escape must close gameplay at any time.
- The state must not automatically start another round.
- The menu must become the next visible context after close.
- The overlay has no pointer control.
- The implementation does not show `Game Over`, `Exit`, or `Return to menu` text.

## Accessibility and viewport behavior

- Column headings must identify score values.
- The persistent final layout must distinguish this state by workflow, not by unique title copy.
- The panel and curtain must adapt to current client dimensions.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-012`](../screenshots/README.md#ss-012).
