# CONS-01 wireframe — Console over menu

Representative viewport: 1706 by 938 px desktop client.
The console sits against the top of the visible client area.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1706 × 938 ────────────────────────────────────┐
│████████████████████ full-width console surface █████████████████████████████████│
│ startup or command history                                                     │
│ ...                                                                            │
│ ... up to 15 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
│              black matte │ centered four-panel main menu │ black matte          │
│                                                                                │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

The console surface is `#EEDD00`.
The input cursor blinks.
The complete client width determines the console width.
The fixed 850 by 700 px menu canvas remains centered under the console.
The visible Game Settings area below the console must show the checked Burnable Trees checkbox in the representative default setup.

Representative screenshot: [`SS-013`](../../screenshots/README.md#ss-013).
