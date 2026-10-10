# NET-01 — Network entry

## Status, purpose, and requirements

This screen provides entry to `NET-10` for central browsing of player-hosted and ready dedicated sessions, including prepared empty dedicated sessions, under [NET-DIR-009 and NET-DIR-021 through NET-DIR-033](../network-host-directory.md). Host, unbound direct Join and Back remain available independently of directory availability. Applicable Password/Invite and capability/unsupported behavior follows the current directory contract. UX owns presentation; no new screen or action layout is selected here.

### Public pilot functional contract

`NET-01-PUBLIC-ENTRY` consumes NET-PUB-017 through NET-PUB-021/024–025 through existing Direct connect or Browse sessions → selected dedicated setup. It does not replace Host, Browse sessions, Direct connect or Back, and adds no dedicated administrator or matchmaking action. NET-03 owns explicit unbound direct setup and browser-derived locked hosting/security mode, operator frontend endpoint and expected identity. Opening this screen creates no connection, controller or participant authority. Updated unsupported Mac/Pi clients may inspect dedicated listings but cannot join them; ordinary player-hosted entry remains unchanged.

Presentation authority: [NET-01 UX contract](../design/screens/NET-01.md). Its proposed action routing is approved by this functional contract.

Functional acceptance: `NET-PUB-AC-005` and `NET-PUB-AC-006` cover these entry destinations and offline independence. UX owns the presentation of the distinction between the invite-only public pilot and trusted private-LAN hosting.

This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It separates player-hosted network play from local-only Play and implements `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-015`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 must not add this graphical entry or change the current Local Play menu.

Entry is `MENU-01` → `Network (F2)`, including pointer and F2 entry on experimental macOS. Host continues to `NET-02`, Browse sessions to `NET-10`, Direct connect to `NET-03`, and Back to `MENU-01` without starting a network service. Approved platform scope follows NET-AC-001 and the linked platform contracts, not a blanket verified-platform claim. Exact platform-scope copy is owned solely by [UX-NET-01-006](../design/screens/NET-01.md#presentation-requirements). The dedicated extension retains the narrower scope in NET-PUB-AC-006.

## Representative layout

- Use the centered, scaled 850 by 700 retro menu canvas and persistent menu background.
- Keep the banner/version header and the `NETWORK PLAY` panel with the four actions and scope treatment defined by [NET-01 presentation](../design/screens/NET-01.md).
- Preserve the existing player-hosted `Lobby 1–15 • Match 2–15 participants and players` constraints without treating them as a minimum for the prepared dedicated idle session. Dedicated idle has zero participants/players under NET-PUB-024 and cannot start a match under NET-PUB-025; UX owns affected scope-copy presentation.
- Do not show endpoint fields until Join or host settings until Host.
- Keep the `NETWORK PLAY` panel inside the shared 24-logical-pixel canvas margin.
- Allocate the panel as a fixed title and scope region above one centered action column.
- Give all four actions one common width and height.
- Keep at least 8 logical px between adjacent actions.
- Wrap the scope copy at word boundaries before it reaches an action or panel edge.

## Navigation and state variants

- Default focus is Host; Host opens `NET-02`, Browse sessions opens `NET-10`, Direct connect opens `NET-03`, and Back returns to `MENU-01`.
- Returning from setup or a recoverable failure restores this screen with no active session claim.
- If network initialization is unavailable, Host and Join are disabled with `Network runtime unavailable`; Back remains enabled.
- No automatic LAN discovery, matchmaking, account, or migration action may appear. NET-DIR, NET-PASS, NET-ADM, and NET-PUB govern approved browsing, admission, and public setup.

## Copy, focus, and input

- Labels must put actions before shortcuts where shortcuts are shown.
- The focused action must use the standard visible pressed/focus treatment and text must remain readable without color.
- Keyboard Tab/Shift+Tab or directional controller input follows the four-action order in the owning UX specification and reverses predictably; Enter, Space, or controller Confirm activates; Escape or controller Back returns to `MENU-01`.
- Pointer activation is optional to the task but must use inverse scaled-canvas coordinates.

Planned representative screenshot: [`SS-015`](../screenshots/README.md#ss-015).
