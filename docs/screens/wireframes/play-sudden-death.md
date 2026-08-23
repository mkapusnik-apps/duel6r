# PLAY-05 wireframe — Sudden-death rising water

Representative viewport: 1280 by 900 debug client.
This state uses the PLAY-01 shared arena and overlay anchors.
The raised environmental boundary materially changes the task, so this state has its own wireframe.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event feed                                             live ranking            │
│                                                                                │
│             one undivided view · shrinking safe arena                           │
│                 player ●                 ● player                               │
│                                                                                │
│~~~~~~~~~~~~~~~~~~~~~~~ RISING WATER SURFACE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
│~~~~ submerged terrain ~~~~~ pickup ~~~~~ air bar near submerged player ~~~~~~~~│
│~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
│~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│
└────────────────────────────────────────────────────────────────────────────────┘
```

The water rises in timed discrete steps.
The state has no sudden-death banner.
All remaining players stay visible in the shared arena.

Representative screenshot: [`SS-009`](../../screenshots/README.md#ss-009).
