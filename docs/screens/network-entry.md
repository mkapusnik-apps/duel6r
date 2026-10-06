# NET-01 — Network entry

## Status, purpose, and requirements

Current functional target: this screen must also provide entry to `NET-10` for central browsing under [NET-DIR-009](../network-host-directory.md). Host, direct Join, and Back remain available independently of directory availability. Earlier browser/password exclusions do not apply. UX owns the presentation of the additional action.

This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It separates player-hosted network play from local-only Play and implements `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-015`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 must not add this graphical entry or change the current Local Play menu.

Entry is `MENU-01` → `Network (F2)`, including pointer and F2 entry on experimental macOS. Host continues to `NET-02`, Browse sessions to `NET-10`, Direct connect to `NET-03`, and Back to `MENU-01` without starting a network service. Approved platform scope follows NET-AC-001 and the linked platform contracts, not a blanket verified-platform claim. Exact platform-scope copy is owned solely by [UX-NET-01-006](../design/screens/NET-01.md#presentation-requirements).

## Representative layout

- Use the centered, scaled 850 by 700 retro menu canvas and persistent menu background.
- Keep the banner/version header and the `NETWORK PLAY` panel with the four actions and scope treatment defined by [NET-01 presentation](../design/screens/NET-01.md).
- Show `Player-hosted • Lobby 1–15 • Match 2–15 participants and players` as textual constraints.
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
- Browsing, optional password admission, and round-one admission follow the current directory and network-play contracts; the earlier blanket exclusions are superseded. This entry must not add matchmaking, accounts, dedicated-server, NAT traversal, or host-migration actions or promise Internet reachability.

## Copy, focus, and input

- Labels must put actions before shortcuts where shortcuts are shown.
- The focused action must use the standard visible pressed/focus treatment and text must remain readable without color.
- Keyboard Tab/Shift+Tab or directional controller input follows the four-action order in the owning UX specification and reverses predictably; Enter, Space, or controller Confirm activates; Escape or controller Back returns to `MENU-01`.
- Pointer activation is optional to the task but must use inverse scaled-canvas coordinates.

Planned representative screenshot: [`SS-015`](../screenshots/README.md#ss-015).
