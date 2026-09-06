# OVER-03 wireframe — Game-over summary

Representative viewport: 1280 by 900 debug client.
The representative visual table is the final four-team variant over the undivided shared arena.
The workflow is final and does not start another round.
This material exit state has its own wireframe and evidence entry.
The screen has no mobile layout, so this single desktop wireframe covers the target overlay.

```text
┌───────────────── undivided shared arena under dark red curtain ────────────────┐
│ final outcome event                                   final round counter       │
│                                                                                │
│              ┌────────────────────────────────────────────┐                    │
│              │                 ---SCORE---                 │                    │
│              │                   K   A   D  K/D  PTS       │                    │
│              │ Alpha          | team totals               │ ← team row         │
│              │ Ada, Amir      | aligned nested rows        │ ← one group         │
│              │ ────────────────────────────────────────── │ ← 2 px rule / 8 px band
│              │ Bravo + two aligned nested player rows      │                    │
│              │ ────────────────────────────────────────── │ ← group boundary    │
│              │ Charlie + two aligned nested player rows    │                    │
│              │ ────────────────────────────────────────── │ ← group boundary    │
│              │ Delta + two aligned nested player rows      │ ← no trailing band  │
│              └────────────────────────────────────────────┘                    │
│                                                                                │
│                              clear gap ≥ 16 px                                  │
│                         ┌──────────────────┐                                    │
│                         │   End of Game    │ ← centered bottom notice           │
│                         └──────────────────┘   bottom inset 16 px               │
└────────────────────────────────────────────────────────────────────────────────┘
```

The representative state uses four teams and two nested players per team.
Each team row touches its nested player rows without an internal gap.
Each adjacent group boundary uses the same 8 px band as non-final `OVER-02`.
Each band contains a 2 px white rule at 70% opacity with 3 px of clear space above and below it.
The rule spans the score-table width and does not cross the panel padding.
The final Team panel adds one 8 px band for each adjacent team boundary.
The `End of Game` notice uses white 32 px score-summary text on a solid blue rectangle.
The notice keeps at least 16 px of horizontal text padding and 8 px of vertical text padding.
The notice aligns to the horizontal center of the client.
The notice bottom edge is 16 px above the bottom client edge.
At least 16 px of clear space separates the notice from the score panel.
The notice does not overlap, clip, cover, replace, or reduce score content.
The score panel stays client-centered when the clear-space requirement fits.
The score panel moves upward only as needed when the preferred position does not fit.
The final non-Team variant keeps the existing separator-free score structure.
The final state does not show `Game Over`, `Exit`, or `Return to menu`.

Representative screenshot: [`SS-012`](../../screenshots/README.md#ss-012).
