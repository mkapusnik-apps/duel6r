# NET-01 — Network entry

Functional contract: [network-entry](../../screens/network-entry.md), including existing NET-AC-001–003 and NET-AC-015–019. [NET-DIR-009 and NET-DIR-015](../../network-host-directory.md) own browser entry and directory independence. Wireframe: [NET-01](../wireframes/NET-01/NET-01.svg).

## Presentation requirements

The [shared visual baseline](../../design.md#unified-network-presentation) applies. The existing wireframe defines the centered task; the actual banner and version remain unchanged.

- **UX-NET-01-001** The NETWORK PLAY heading must occupy the primary panel title strip above scope copy.
- **UX-NET-01-002** The four actions must remain in one centered column with their existing 300 by 32 logical px bounds and 13 logical px vertical gaps.
- **UX-NET-01-003** Scope copy must remain separate from the first action by at least one text row.
- **UX-NET-01-004** A focused enabled action must retain the same surface and caption alignment as the other enabled actions.
- **UX-NET-01-005** An unavailable action must retain a readable boundary and its existing reason without moving Back off screen.
- **UX-NET-01-006** The platform-scope line must show exactly `Use the same supported release and gameplay content`.
- **UX-NET-01-007** This line must retain its existing position between the directory-scope line and the participant-limit line.
- **UX-NET-01-008** The line must use the existing text size and remain readable without overlap, clipping, or movement of adjacent content.

- The screen must retain its banner, scope region, and centered action column.
- The action column must show `Host`, `Browse sessions`, `Direct connect`, and `Back` in that order.
- The former `Join` action must use the clearer label `Direct connect`.
- Host must retain initial focus.
- Browse sessions must lead to NET-10.
- Direct connect must lead to the existing NET-03 task.
- Back must retain its MENU-01 destination.
- Scope copy must say `LAN-first player-hosted sessions` and `Directory or direct address`.
- The screen must not promise Internet connectivity or NAT traversal.
- A directory outage must not disable Host, Direct connect, or Back.
- Keyboard and controller traversal must follow the visible action order.
- Each action must retain the existing width, height, focus order, and pointer activation behavior.

## Acceptance

All four actions must fit the existing canvas without a second task panel or scrolling. Local Play must remain outside this network flow. At initial entry, Host must have a visible outer focus keyline and all four actions must remain identifiable as buttons. Keyboard, controller, and pointer checks must reach the same existing destinations. The representative and focused edge checks are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).

The scope line refers to the approved functional platform, release, and content contracts; it does not promise compatibility across all versions or verified support on every platform. This document is the sole exact-copy authority for that line.

The macOS extension must preserve the corrected pointer mapping and full-drawable baseline from PR #110 and PR #119 through entry, setup, browser, lobby, arena, results, reconnect, failures, confirmations, and return transitions. Keep existing logical layouts and text sizes; do not substitute a quarter-surface viewport or misaligned pointer regions. Native Mac visual conformance is deferred, not passed, under the [coordinated matrix](../screenshots/README.md#macos-deferred-coverage); automated evidence is not native visual assessment.

Dedicated setup uses Direct connect under NET-PUB-017–021 without changing this screen's appearance or actions. The NET-01-PUBLIC-ENTRY functional state does not authorize a new dedicated action, published endpoint claim, or supported-platform claim. Current develop's SS-015 evidence remains reusable within its original scope when presentation is unchanged.
