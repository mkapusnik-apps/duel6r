# PLAY-05 — Sudden-death rising water

## Purpose and traceability

This state shows the environmental pressure that ends a prolonged round.
Entry occurs when the selected game mode reports sudden death.
Exit occurs when the round gains a winner or the game closes.
The state implements `ENV-002`–`ENV-007`, `ENV-009`–`ENV-013`, and `UI-008`–`UI-015` from [`docs/features.md`](../features.md).
Primary sources are `source/Round.cpp:146-200`, `source/gamemodes/GameModeBase.cpp:73-75`, and `source/gamemodes/TeamDeathMatch.cpp:173-193`.

## Prerequisites and triggers

- Quick Liquid must trigger sudden death immediately after the winner check permits it.
- Free-for-all and Predator must otherwise trigger sudden death when two players remain from a match that started with more than two players.
- Team mode must otherwise trigger sudden death when any team has fewer than two living members.

## Layout and hierarchy

- The state must preserve the active arena and overlay layout in [`play-sudden-death.md`](wireframes/play-sudden-death.md).
- The state must keep one undivided arena that shows all remaining players.
- Water must remain in the world geometry layer.
- Water must rise in discrete steps after each implemented wait interval.
- The water surface must become a dominant moving boundary as safe space decreases.
- Player air indicators may appear near submerged players.

## Visible behavior, controls, and recovery

- Water must drain air and then life according to gameplay behavior.
- The state must retain event messages, ranking, counters, and player indicators that are active.
- Standard player and screen controls must remain available.
- The state has no warning banner or text label for sudden death.
- The state has no stop or reversal action.
- Round completion is the implemented recovery path.

## Accessibility and viewport behavior

- Rising geometry and the changing safe area must supplement water color.
- Air-bar fill length must supplement its blue color.
- The implementation does not provide a textual sudden-death cue.
- Water must follow the shared arena and level dimensions.
- The hazard must not create another viewport layout.

## Screenshot link

Representative evidence: [`SS-009`](../screenshots/README.md#ss-009).
