# MODE-02 — Team live gameplay and ranking

## Purpose and traceability

This state presents team identity, team grouping, and friendly-fire mode selection effects.
Entry occurs when the menu starts one of the 2-team, 3-team, or 4-team Deathmatch variants.
Exit occurs through normal round or game progression.
The state implements `SET-020`–`SET-021`, `SCO-005`–`SCO-006`, `SCO-013`–`SCO-017`, `MOD-TM-001`–`MOD-TM-011`, and `UI-008`–`UI-015` from [`docs/features.md`](../features.md).
Primary source is `source/gamemodes/TeamDeathMatch.cpp`.

## Layout and hierarchy

- The state must use the arena structure in [`mode-team.md`](wireframes/mode-team.md).
- Player assignment must alternate by roster index across the selected team count.
- Team color must override headband, trousers, and hair-top color.
- A player with no hair or short hair must receive a headband before the team color is applied.
- Live ranking must show named team rows.
- Each team row must contain nested player rows.
- Team rows and nested rows must use their implemented team-tinted backgrounds.

## Implemented variants and recovery

- The menu must provide two, three, and four team counts.
- Each team count must provide friendly fire on and off variants.
- Team names must be Alpha, Bravo, Charlie, and Delta in that order.
- Team colors must use the tokens in `docs/design.md`.
- A winning team must produce `Team <name> won!` for its players.
- No surviving player must produce `End of round - no winner`.
- The ranking must sort teams by points and must sort nested players by points.
- The implementation has no disconnected-team or unbalanced-team warning state.

## Controls, accessibility, and viewport behavior

- Standard gameplay controls and overlays must remain available.
- Team names in ranking must supplement team colors.
- Team apparel uses color without an in-world text team label.
- The viewport must follow standard full-screen or split-screen rules.
- The representative wireframe uses two teams in full-screen mode because all team counts use the same ranking layout.

## Screenshot link

Representative evidence: [`SS-008`](../screenshots/README.md#screenshot-matrix).
