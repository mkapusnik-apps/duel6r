# PLAY-03 wireframe — Three-player split-screen

Representative viewport: 1280 by 900 debug client.
Each camera is 636 by 446 px at this viewport.
The screen has no mobile layout, so this single desktop wireframe covers the implemented geometry.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ black unused    ┌────────────── PLAYER 3 CAMERA ──────────────┐ black unused   │
│                 │ arena · local camera · messages             │                │
│                 └─────────────────────────────────────────────┘                │
│                         4 px red camera boundaries                              │
│┌────────────── PLAYER 1 ──────────────┐red┌────────────── PLAYER 2 ────────────┐│
││ arena · local camera · messages      │   │ arena · local camera · messages    ││
│└──────────────────────────────────────┘   └────────────────────────────────────┘│
└────────────────────────────────────────────────────────────────────────────────┘
```

A dead camera receives a 50% red curtain.

Representative screenshot: [`SS-005`](../../screenshots/README.md#ss-005).
