# MODE-01 — Predator live gameplay

## Purpose and traceability

This state presents the implemented Predator role difference during the standard arena layout.
Entry occurs when the menu starts a match with `Predator` selected.
Exit occurs through normal round or game progression.
The state implements `MOD-PR-001`–`MOD-PR-008` and `UI-008`–`UI-015` from [`docs/features.md`](../features.md).
Primary sources are `source/gamemodes/Predator.cpp`, `source/gamemodes/PredatorPlayerEventListener.cpp`, and `source/WorldRenderer.cpp`.

## Layout and hierarchy

- The layout must preserve the undivided shared arena structure in [`mode-predator.md`](wireframes/mode-predator.md).
- The shared arena must show the predator and all marines.
- The randomly selected predator must use body alpha `0.1`.
- The predator's weapon must remain visible.
- Other players must remain opaque.
- The live ranking must use the standard free-for-all row layout.

## States, controls, and recovery

- Each round must select one predator at random.
- Non-predator players must receive 10 additional ammunition at round initialization.
- Predator death must produce `Marines won!` messages for living marines.
- A sole surviving predator must receive `Predator won!`.
- No surviving player must produce `End of round - no winner`.
- The implementation does not show a persistent textual role badge during live play.
- The implementation has no role-selection loading state.
- Standard gameplay controls and overlays must remain available.

## Accessibility and viewport behavior

- Near transparency and the visible weapon are the implemented predator cues.
- The live screen does not provide a persistent non-color text cue for the predator role.
- Result messages must provide textual outcome cues.
- The viewport must follow the shared arena rules for every supported player count.
- The representative wireframe uses three players because role presentation does not create another responsive layout.

## Screenshot link

Representative evidence: [`SS-007`](../screenshots/README.md#ss-007).
