# CONS-01 wireframe — Console over menu

Representative viewport: 1280 by 900 debug client.
The console sits against the top of the visible client area.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│████████████████████ full-width console surface █████████████████████████████████│
│ startup or command history                                                     │
│ ...                                                                            │
│ ... up to 15 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
│                         centered main menu                                      │
│                                                                                │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

The console surface is `#EEDD00`.
The input cursor blinks.

Representative screenshot: [`SS-013`](../../screenshots/README.md#ss-013).
