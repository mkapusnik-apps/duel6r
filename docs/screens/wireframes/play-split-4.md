# PLAY-04 wireframe — Four-player split-screen

Representative viewport: 1280 by 900 debug client.
Each camera is 636 by 446 px at this viewport.
The screen has no mobile layout, so this single desktop wireframe covers the implemented geometry.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│┌────────────── PLAYER 3 ──────────────┐red┌────────────── PLAYER 4 ────────────┐│
││ arena · local camera · messages      │   │ arena · local camera · messages    ││
│└──────────────────────────────────────┘   └────────────────────────────────────┘│
│                         4 px red horizontal gutter                              │
│┌────────────── PLAYER 1 ──────────────┐red┌────────────── PLAYER 2 ────────────┐│
││ arena · local camera · messages      │   │ arena · local camera · messages    ││
│└──────────────────────────────────────┘   └────────────────────────────────────┘│
└────────────────────────────────────────────────────────────────────────────────┘
```

A dead camera receives a 50% red curtain.

Representative screenshot: [`SS-006`](../../screenshots/README.md#ss-006).
