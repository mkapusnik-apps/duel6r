# MODE-02 wireframe — Team gameplay and ranking

Representative viewport: 1280 by 900 debug client.
The representative state uses two teams and four players.
Three-team and four-team variants add team groups without changing the layout model.
Every team and player remains in one undivided arena view.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event feed · one shared arena                   ┌ Alpha · team points ───────┐ │
│                                                 │   player A | points         │ │
│   Alpha player: red apparel                     │   player C | points         │ │
│          ●                                      ├ Bravo · team points ───────┤ │
│                                                 │   player B | points         │ │
│                         Bravo player: green     │   player D | points         │ │
│                                ●                └─────────────────────────────┘ │
│                                                                                │
│   team color overrides: headband · trousers · hair top                         │
│                                                                                │
│~~~~~~~~~~~~~~~~~~~~~~~~~~~~ arena water/terrain ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
└────────────────────────────────────────────────────────────────────────────────┘
```

Representative screenshot: [`SS-008`](../../screenshots/README.md#ss-008).
