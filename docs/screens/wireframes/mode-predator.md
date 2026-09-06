# MODE-01 wireframe — Predator gameplay

Representative viewport: 1280 by 900 debug client.
This state uses the undivided PLAY-01 shared arena geometry for every supported player count.
The role cue materially changes a player, so this state has its own wireframe and evidence entry.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event feed                     one shared arena           standard ranking     │
│                                                                                │
│                     opaque marine                                              │
│                         ●                                                      │
│                                                                                │
│      predator body at 10% alpha                                                │
│      (faint silhouette) + fully readable weapon                                │
│                                                                                │
│                                                    opaque marine                │
│                                                         ●                      │
│~~~~~~~~~~~~~~~~~~~~~~~~~~~~ arena water/terrain ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
└────────────────────────────────────────────────────────────────────────────────┘
```

All marines and the predator remain in the same gameplay view.

Representative screenshot: [`SS-007`](../../screenshots/README.md#ss-007).
