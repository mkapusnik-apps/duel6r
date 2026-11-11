# OVER-02 wireframe — Round-over summary

Representative viewport: 1280 by 900 debug client.
This representative state is a non-final Team deathmatch result after the next round of a resumed match.
The match resumes with two completed rounds and shows the result after round 3 of 5.
The result retains the undivided shared arena and adds the animated winner-curtain sequence and score panel.
The bottom-center footer is the only visible round-count location in this state.
The screen has no mobile layout, so this single desktop wireframe covers the target overlay.

```text
┌──────────── undivided shared arena under animated dark red curtain ────────────┐
│ winner event text                                      [no arena round count]   │
│                                                                                │
│              ┌────────────────────────────────────────────┐                    │
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
│                          ┌───────────────────┐                                 │
│                          │    Rounds: 3|5    │ ← measured black backing         │
│                          └───────────────────┘ ← 16 px above client bottom       │
└────────────────────────────────────────────────────────────────────────────────┘
```

The external counter uses the exact format `Rounds: <played>|<total>`.
The representative `3|5` value includes the completed round and the two rounds completed before resume.
The counter and `---SCORE---` retain 32 px white text.
The counter backing is centered on the complete client, not the panel; its bottom is 16 client px above the client bottom.
Its measured text has 8 px horizontal and 4 px vertical inner padding, giving a 40 px backing height.
Keep at least 16 px between the counter backing and the score panel. The exact common placement is owned by [OVER-02](../overlay-round-over.md#layout-and-hierarchy).
The `---SCORE---` heading remains horizontally centered in its strip.
The heading strip uses the existing solid blue fill and extends 5 px beyond each outer-panel side.
The outer panel uses 16 px horizontal padding and 32 px vertical padding around its content.
Remove the old panel-top-right label and its 32 px row. Keep the score heading as the first panel content row and preserve score-table row spacing.
The top-center arena progress is hidden from the first summary frame through the last summary frame.
The top-center arena progress returns in the first visible frame of the next active round.
The curtain must continue to fade toward the documented dark red treatment after the opening phase.
The representative state uses four teams and two nested players per team.
Each team row touches its nested player rows without an internal gap.
Each adjacent group boundary uses an 8 px band.
Each band contains a 2 px white rule at 70% opacity with 3 px of clear space above and below it.
The rule spans the score-table width and does not cross the panel padding.
The three separator bands add 24 px to the representative panel height.
Two-team and three-team states use the same treatment with one and two separator bands.
Deathmatch and Predator outcomes use the unchanged overlay geometry without these separators.
An unlimited-match variant retains its existing layout without this counter or footer allocation.
Final Team and non-Team summaries retain their separate existing final-screen contract; this footer does not apply to them.
The active-round Team Tab overlay uses the same separator treatment without the progress row or curtain.

Representative screenshot: [`SS-011`](../../screenshots/README.md#ss-011).
