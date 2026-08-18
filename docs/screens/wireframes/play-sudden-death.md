# PLAY-05 wireframe — Sudden-death rising water

Representative viewport: 1280 by 900 debug client.
This state uses the PLAY-01 overlay anchors.
The raised environmental boundary materially changes the task, so this state has its own wireframe.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event feed                                             live ranking            │
│                                                                                │
│                       shrinking safe arena                                      │
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

Representative screenshot: [`SS-009`](../../screenshots/README.md#ss-009).
