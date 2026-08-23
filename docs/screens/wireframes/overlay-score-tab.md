# OVER-01 wireframe — Score-tab overlay

Representative viewport: 1280 by 900 debug client.
The panel size depends on name length and ranking row count.
The screen has no mobile layout, so this single desktop wireframe covers the implemented centered overlay.

```text
┌────────────────── one undivided live shared arena continues ───────────────────┐
│                                                                                │
│                 ┌──────────────────────────────────────┐                       │
│                 │              ---SCORE---             │ blue header strip     │
│                 │                 K   A   D  K/D  PTS   │                       │
│                 │ player one   |  2 | 1 | 1 | 2.0 | 8 │                       │
│                 │ player two   |  1 | 0 | 2 | 0.5 | 3 │                       │
│                 │ player three |  0 | 1 | 1 | 0.0 | 1 │                       │
│                 └──────────────────────────────────────┘                       │
│                    translucent white outer + blue inner                        │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

No winner curtain appears in this state.
Free-for-all, Predator, and Team ranking structures use this centered overlay model.

Representative screenshot: [`SS-010`](../../screenshots/README.md#ss-010).
