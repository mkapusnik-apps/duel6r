# CONS-02 wireframe — Console over gameplay

Representative viewport: 1280 by 900 debug client.
The console geometry matches CONS-01 and overlays the active gameplay frame.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│ event feed · active arena · ranking                                             │
│                                                                                │
│ players and world continue to update                                            │
│                                                                                │
│████████████████████ full-width console surface █████████████████████████████████│
│ recent game or command history                                                  │
│ ...                                                                            │
│ ... up to 10 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
└────────────────────────────────────────────────────────────────────────────────┘
```

The console obscures the lower arena and captures keyboard input.

Representative screenshot: [`SS-014`](../../screenshots/README.md#screenshot-matrix).
