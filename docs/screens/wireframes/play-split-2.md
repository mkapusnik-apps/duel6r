# PLAY-02 wireframe — Two-player split-screen

Representative viewport: 1280 by 900 debug client.
Each camera is 636 by 446 px at this viewport.
The screen has no mobile layout, so this single desktop wireframe covers the implemented geometry.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ black unused    ┌────────────── PLAYER 2 CAMERA ──────────────┐ black unused   │
│                 │ arena · player messages · local camera      │                │
│                 │                                             │                │
│                 └─────────────────────────────────────────────┘                │
│                         4 px red camera boundaries                              │
│ black unused    ┌────────────── PLAYER 1 CAMERA ──────────────┐ black unused   │
│                 │ arena · player messages · local camera      │                │
│                 │                                             │                │
│                 └─────────────────────────────────────────────┘                │
└────────────────────────────────────────────────────────────────────────────────┘
```

A dead camera receives a 50% red curtain.

Representative screenshot: [`SS-004`](../../screenshots/README.md#ss-004).
