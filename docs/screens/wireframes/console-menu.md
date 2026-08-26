# CONS-01 wireframe — Console over menu

Representative viewport: 1920 by 1080 px desktop client.
The console sits against the top of the visible client area.
The screen has no mobile layout, so this single desktop wireframe covers the implemented overlay.

```text
┌──────────────────────────────── 1920 × 1080 ───────────────────────────────────┐
│████████████████████ full-width console surface █████████████████████████████████│
│ startup or command history                                                     │
│ ...                                                                            │
│ ... up to 15 visible history rows                                               │
│================================================================================│ red
│]_                                                                              │
│──────────────────────────────── 3 px black edge ────────────────────────────────│
│ blurred session still │ scaled/keylined four-panel menu │ blurred session still │
│                                                                                │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

The console surface is `#EEDD00`.
The input cursor blinks.
The complete client width determines the console width.
The console remains unscaled and does not inherit the menu transform.
The fixed 850 by 700 logical menu canvas remains centered at the 135% scale cap under the console.
The visible Game Settings area below the console must show the checked Burnable Trees checkbox in the representative default setup.
The visible client below the console uses the same selected still, cover crop, blur, and 55% scrim as `MENU-01` in the same session.

Representative screenshot: [`SS-013`](../../screenshots/README.md#ss-013).
