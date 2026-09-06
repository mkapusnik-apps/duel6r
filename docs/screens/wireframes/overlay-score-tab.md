# OVER-01 wireframe — Score-tab overlay

Representative viewport: 1280 by 900 debug client.
The panel size depends on name length, ranking row count, and Team group count.
The screen has no mobile layout, so this single desktop wireframe covers the implemented centered overlay.

```text
┌────────────────── one undivided live shared arena continues ───────────────────┐
│                                                                                │
│                 ┌──────────────────────────────────────┐                       │
│                 │              ---SCORE---             │ blue header strip     │
│                 │                 K   A   D  K/D  PTS   │                       │
│                 │ Alpha        |  4 | 2 | 3 | 1.3 | 12│ ← team row            │
│                 │ Ada          |  3 | 1 | 1 | 3.0 | 8 │   nested player       │
│                 │ Amir         |  1 | 1 | 2 | 0.5 | 4 │   nested player       │
│                 │ ──────────────────────────────────── │ ← 2 px rule in 8 px band
│                 │ Bravo       + two nested players     │ ← contiguous group    │
│                 │ ──────────────────────────────────── │ ← 8 px boundary       │
│                 │ Charlie     + two nested players     │ ← contiguous group    │
│                 │ ──────────────────────────────────── │ ← 8 px boundary       │
│                 │ Delta       + two nested players     │ ← no trailing band    │
│                 └──────────────────────────────────────┘                       │
│                    translucent white outer + blue inner                        │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

No winner curtain appears in this state.
The representative Team state uses four teams and two nested players per team.
Each team row touches its nested player rows without an internal gap.
Each adjacent group boundary uses an 8 px band.
Each band contains a 2 px white rule at 70% opacity with 3 px of clear space above and below it.
The rule spans the score-table width and does not cross the panel padding.
The separator adds 8 px to the panel height for each of the three group boundaries.
Two-team and three-team states use the same treatment with one and two separator bands.
Free-for-all and Predator structures use the unchanged centered overlay model.

Representative screenshot: [`SS-010`](../../screenshots/README.md#ss-010).
