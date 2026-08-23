# CONS-02 wireframe — Console over gameplay

Representative viewport: 1280 by 900 debug client.
The console geometry matches CONS-01 and overlays the active shared arena frame.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│████████████████████ full-width console surface █████████████████████████████████│
│ recent game or command history                                                  │
│ ...                                                                            │
│ ... up to 15 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
│ event feed · one undivided active arena · ranking                               │
│                                                                                │
│ players and world continue to update                                            │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

The console obscures the upper arena and receives text input and discrete key-down events instead of routing those events to the gameplay context.
Key-down and key-up events still update shared held-keyboard state, and controller controls still read shared controller state. Either state may continue to cause player actions while the simulation continues.
The console does not provide a gameplay view-layout control.

Representative screenshot: [`SS-014`](../../screenshots/README.md#ss-014).
