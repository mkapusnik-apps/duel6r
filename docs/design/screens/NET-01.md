# NET-01 — Network entry

Functional contract: [network-entry](../../screens/network-entry.md), including existing NET-AC-001–003 and NET-AC-015–019. [NET-DIR-009 and NET-DIR-015](../../network-host-directory.md) own browser entry and directory independence. Wireframe: [NET-01](../wireframes/NET-01/NET-01.svg).

## Changed presentation

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
- Each action must retain the existing width, height, focus treatment, and pointer behavior.

## Acceptance

All four actions must fit the existing canvas without a new panel or scrolling. Local Play must remain outside this network flow. The affected representative is NET-01 in the capture matrix.
