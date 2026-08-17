# CONS-01 wireframe — Console over menu

Representative viewport: 1280 by 900 debug client.
Renderer coordinates place the console at the bottom.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│                         centered main menu                                      │
│                                                                                │
│                                                                                │
│████████████████████ full-width console surface █████████████████████████████████│
│ startup or command history                                                     │
│ ...                                                                            │
│ ... up to 10 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
└────────────────────────────────────────────────────────────────────────────────┘
```

The console surface is `#EEDD00`.
The input cursor blinks.

Representative screenshot: [`SS-013`](../../screenshots/README.md#screenshot-matrix).
