# PLAY-01 wireframe — Live full-screen gameplay

Representative viewport: 1280 by 900 debug client.
The client dimensions are fluid, but the overlay anchors remain screen-relative.
The screen has no mobile layout, so this single desktop wireframe covers the implemented full-screen layout.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event messages                 [Rounds: current|max]        live ranking       │
│ Player: event text                                          name | points       │
│                                                                                │
│  background texture · level walls · decorative sprites                         │
│                                                                                │
│       pickup                 ┌ name · ammo · bonus ┐                            │
│                              ├ reload/air/bonus/HP ┤                            │
│                player + gun └ round-kill points ──┘         projectile         │
│                                                                                │
│  elevator                 terrain routes                   explosion            │
│                                                                                │
│~~~~~~~~~~~~~~~~~~~~~~~~~~~~ translucent water ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
└────────────────────────────────────────────────────────────────────────────────┘
```

Start variant: blue-dark fade plus yellow spiked location rings.
Optional variants: FPS at upper right, ranking hidden, round counter absent.

Representative screenshot: [`SS-003`](../../screenshots/README.md#screenshot-matrix).
