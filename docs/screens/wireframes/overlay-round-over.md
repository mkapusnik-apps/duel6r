# OVER-02 wireframe — Round-over summary

Representative viewport: 1280 by 900 debug client.
This representative state is a non-final limited-match result after the next round of a resumed match.
The match resumes with two completed rounds and shows the result after round 3 of 5.
The result retains the undivided shared arena and adds the winner curtain and score panel.
The screen has no mobile layout, so this single desktop wireframe covers the target overlay.

```text
┌───────────────── undivided shared arena under dark red curtain ────────────────┐
│ winner event text                               existing Rounds: 3|5            │
│                                                                                │
│              ┌────────────────────────────────────────────┐                    │
│              │       ---SCORE---           Rounds: 3|5    │ ← panel top-right  │
│              │                   K   A   D  K/D  PTS       │                    │
│              │ winner         |  3 | 1 | 1 | 3.0 | 9     │                    │
│              │ player two     |  1 | 0 | 2 | 0.5 | 2     │                    │
│              │ player three   |  0 | 1 | 2 | 0.0 | 1     │                    │
│              └────────────────────────────────────────────┘                    │
│                                                                                │
│                     next-round input has no visible prompt                      │
└────────────────────────────────────────────────────────────────────────────────┘
```

The panel label uses the exact format `Rounds: <played>|<total>`.
The representative `3|5` value includes the completed round and the two rounds completed before resume.
Deathmatch, Predator, and Team outcomes use this overlay geometry.
An unlimited-match variant omits the panel label.
The final summary and the active-round Tab overlay remain unchanged.

Representative screenshot: [`SS-011`](../../screenshots/README.md#ss-011).
