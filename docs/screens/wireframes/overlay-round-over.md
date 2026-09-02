# OVER-02 wireframe — Round-over summary

Representative viewport: 1280 by 900 debug client.
This representative state is a non-final Team deathmatch result after the next round of a resumed match.
The match resumes with two completed rounds and shows the result after round 3 of 5.
The result retains the undivided shared arena and adds the winner curtain and score panel.
The panel is the only visible round-count location in this state.
The screen has no mobile layout, so this single desktop wireframe covers the target overlay.

```text
┌───────────────── undivided shared arena under dark red curtain ────────────────┐
│ winner event text                                      [no arena round count]   │
│                                                                                │
│              ┌────────────────────────────────────────────┐                    │
│              │                              Rounds: 3|5  │ ← 32 px progress row
│              ├──────────── solid blue heading strip ──────┤                    │
│              │                 ---SCORE---                 │ ← 36 px strip      │
│              │                   K   A   D  K/D  PTS       │                    │
│              │ Alpha          | team totals               │ ← team row           │
│              │ Ada, Amir      | aligned nested rows        │ ← one group           │
│              │ ────────────────────────────────────────── │ ← 2 px rule / 8 px band
│              │ Bravo + two aligned nested player rows      │                    │
│              │ ────────────────────────────────────────── │ ← group boundary      │
│              │ Charlie + two aligned nested player rows    │                    │
│              │ ────────────────────────────────────────── │ ← group boundary      │
│              │ Delta + two aligned nested player rows      │ ← no trailing band    │
│              └────────────────────────────────────────────┘                    │
│                                                                                │
│                     next-round input has no visible prompt                      │
└────────────────────────────────────────────────────────────────────────────────┘
```

The panel label uses the exact format `Rounds: <played>|<total>`.
The representative `3|5` value includes the completed round and the two rounds completed before resume.
The progress label and `---SCORE---` use 32 px white text on separate baseline rows.
The top edge of the progress row is 32 px below the translucent outer panel's top bound.
The progress row spans the panel inner width with 16 px inset from each outer-panel side.
The right edge of `Rounds: 3|5` is 16 px inside the translucent outer panel's right bound.
The `---SCORE---` heading remains horizontally centered in its strip.
The progress row uses the translucent panel surface.
The heading strip uses the existing solid blue fill and extends 5 px beyond each outer-panel side.
The outer panel uses 16 px horizontal padding and 32 px vertical padding around its content.
The representative panel adds one 32 px progress row without changing score-table row spacing.
The top-center arena progress is hidden from the first summary frame through the last summary frame.
The top-center arena progress returns in the first visible frame of the next active round.
The representative state uses four teams and two nested players per team.
Each team row touches its nested player rows without an internal gap.
Each adjacent group boundary uses an 8 px band.
Each band contains a 2 px white rule at 70% opacity with 3 px of clear space above and below it.
The rule spans the score-table width and does not cross the panel padding.
The three separator bands add 24 px to the representative panel height.
Two-team and three-team states use the same treatment with one and two separator bands.
Deathmatch and Predator outcomes use the unchanged overlay geometry without these separators.
An unlimited-match variant omits the panel label and its 32 px row.
The final summary remains unchanged.
The active-round Team Tab overlay uses the same separator treatment without the progress row or curtain.

Representative screenshot: [`SS-011`](../../screenshots/README.md#ss-011).
