# NET-01 — Network entry

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It separates player-hosted network play from local-only Play and implements the entry decisions in [`docs/network-play-first-release.md`](../network-play-first-release.md), especially `NET-AC-001`–`NET-AC-003`, `NET-AC-009`, and `NET-AC-015`.

Entry is `MENU-01` → `Network (F2)`. Host continues to `NET-02`, Join continues to `NET-03`, and Back returns to `MENU-01` without starting a network service.

## Representative layout

- Use the centered, scaled 850 by 700 retro menu canvas and persistent menu background.
- Keep the banner/version header, then show a `NETWORK PLAY` panel with `Host`, `Join`, and `Back` actions.
- Show concise scope copy: `Same machine or LAN`, `Direct address and port`, and `Linux / Windows x86-64`.
- Show `Player-hosted • 2–15 participants • 2–15 players` as textual constraints.
- Do not show endpoint fields until Join or host settings until Host.

## Navigation and state variants

- Default focus is Host; Host opens `NET-02`, Join opens `NET-03`, and Back returns to `MENU-01`.
- Returning from setup or a recoverable failure restores this screen with no active session claim.
- If network initialization is unavailable, Host and Join are disabled with `Network runtime unavailable`; Back remains enabled.
- No discovery, matchmaking, Internet, account, password, dedicated-server, join-in-progress, or migration action may appear.

## Copy, focus, and input

- Labels must put actions before shortcuts where shortcuts are shown.
- The focused action must use the standard visible pressed/focus treatment and text must remain readable without color.
- Keyboard Tab/Shift+Tab or directional controller input moves Host → Join → Back and reverses predictably; Enter, Space, or controller Confirm activates; Escape or controller Back returns to `MENU-01`.
- Pointer activation is optional to the task but must use inverse scaled-canvas coordinates.

Planned representative screenshot: [`SS-015`](../screenshots/README.md#ss-015).
