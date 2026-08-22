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
Entry guard variant: if no successfully loaded level or no enabled weapon exists, no arena frame is created.
The entry guard keeps MENU-02 over MENU-01, so it has no separate PLAY-01 visual layout.
The representative PLAY-01 evidence must use a valid configuration and must confirm that the existing live layout is unchanged.

Representative screenshot: [`SS-003`](../../screenshots/README.md#ss-003).
