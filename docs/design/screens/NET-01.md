# NET-01 — Network entry

Functional authority: [NET-01](../../screens/network-entry.md), `NET-DIR-009/015`, `NET-PUB-016`–`NET-PUB-021`, and state `NET-01-PUBLIC-ENTRY`. Wireframe: [NET-01](../wireframes/NET-01/NET-01.svg). Visual impact: localized scope reconciliation, not a new navigation workflow.

## Hierarchy and interaction

- **UX-NET-01-001** The NETWORK PLAY heading must occupy the primary panel title strip above scope copy.
- **UX-NET-01-002** The four actions must retain one centered column with 300 by 32 logical px bounds and 13 logical px vertical gaps.
- **UX-NET-01-003** Scope copy must remain separate from the first action by at least one text row.
- **UX-NET-01-004** Focus must not change an enabled action's surface or caption alignment.
- **UX-NET-01-005** An unavailable action must retain a readable boundary and persistent reason without moving Back off screen.
- The action column must show `Host`, `Browse sessions`, `Direct connect`, and `Back` in that order.
- Host must retain initial focus and its NET-02 destination.
- Browse sessions must retain its NET-10 destination.
- Direct connect must open NET-03 without starting admission or assigning a role.
- Back must retain its MENU-01 destination.
- Scope copy must identify player-hosted sessions and the invite-only dedicated pilot without promising Internet reachability.
- Scope copy must retain `Directory or direct address`.
- Directory failure must not disable Host, Direct connect, or Back.
- Keyboard and controller traversal must retain the visible action order.
- Pointer targets must retain the existing scaled control bounds.

The pilot uses the existing direct-setup path. Do not replace Browse sessions with a pilot action. Do not rename Host to Host private LAN, because develop permits eligible public-unicast listening addresses. The pilot has no directory listing, matchmaking, account task, or deployment task.

## Acceptance

All four actions and scope lines must fit without scrolling at 850 by 700 and 1280 by 720. Host must show a visible non-color focus cue. Keyboard, controller, and pointer activation must reach the same product-owned destinations. Local Play must remain outside this flow. Capture: [SS-015](../screenshots/README.md#network-presentation-current-capture-matrix).
