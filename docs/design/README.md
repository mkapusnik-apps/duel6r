# UX documentation

## Scope and authority

This index owns new UX specifications. The existing [visual design system](../design.md#unified-network-presentation) owns the approved network presentation requirements `UX-NET-001`–`UX-NET-024` and `UX-NET-AC-01`–`UX-NET-AC-08`. The native MENU-01 is the visual reference. This work does not migrate legacy documentation or alter product behavior. The [product inventory](../screens/README.md) owns screen identities and functional contracts.

The host-directory work is a localized extension of the graphical network flow, not a redesign. The [host-directory contract](../network-host-directory.md) owns NET-DIR-001–016 and NET-PASS-001–007. The [network-play contract](../network-play-first-release.md) owns NET-ADM-001–012. These canonical requirements and the current functional screen targets supersede conflicting legacy exclusions. UX does not define service protocols, heartbeat intervals, expiry durations, or password policy.

Product reserves **NET-10** in the [browser contract](../screens/network-browser.md). Its stable states are `NET-10-loading`, `NET-10-results`, `NET-10-empty`, `NET-10-stale`, and `NET-10-unavailable`. Related states are `NET-02-password`, `NET-03-password`, `NET-03-live-admission`, `NET-08-password-rejected`, and `NET-08-admission-closed`.

## Principles

- The UI must keep local play and direct connection discoverable during directory failure.
- The UI must distinguish a listing from a reachable host.
- The UI must distinguish password protection, match phase, and admission availability.
- The UI must preserve the existing desktop canvas, controls, and arena presentation.
- The UI must keep feedback visible without requiring color recognition.
- The UI must make actionable and editable regions identifiable before focus moves to them.
- Visual reuse must preserve network-specific input, ownership, and disabled-state semantics.
- Contextual panels must preserve the underlying task rather than replace live gameplay with a menu.

## Screen specifications

| Screen | Authoritative presentation | Structural wireframes |
|---|---|---|
| [NET-01](screens/NET-01.md) | Entry and navigation | [NET-01](wireframes/NET-01/NET-01.svg) |
| [NET-02](screens/NET-02.md) | Editable and pending host setup | [NET-02](wireframes/NET-02/NET-02.svg), [NET-02-P](wireframes/NET-02/NET-02-P.svg) |
| [NET-03](screens/NET-03.md) | Shared direct/directory join and connecting | [NET-03-E](wireframes/NET-03/NET-03-E.svg), [NET-03](../screens/wireframes/network-join.md) |
| [NET-04](screens/NET-04.md) | Lobby, listing feedback, retained results | [NET-04](../screens/wireframes/network-lobby.md), [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| [NET-05](screens/NET-05.md) | Network controls and contextual panels; no arena/HUD redesign | [NET-05](../screens/wireframes/network-match.md), [NET-05-C](wireframes/NET-05/NET-05-C.svg), [NET-05-S](wireframes/NET-05/NET-05-S.svg), [NET-05-R](wireframes/NET-05/NET-05-R.svg) |
| NET-06 | [Existing design-system owning section](../design.md#net-06--completed-summary-presentation) and [functional contract](../screens/network-summary.md) | [NET-06](../screens/wireframes/network-summary.md) |
| NET-07 | [Existing design-system owning section](../design.md#net-07--reconnect-presentation) and [functional contract](../screens/network-reconnect.md) | [NET-07](../screens/wireframes/network-reconnect.md) |
| [NET-08](screens/NET-08.md) | Failure and recovery presentation | [NET-08](../screens/wireframes/network-failure.md) |
| NET-09 | [Existing design-system owning section](../design.md#net-09--intentional-host-end-presentation) and [functional contract](../screens/network-host-ended.md) | [NET-09](../screens/wireframes/network-host-ended.md) |
| [NET-10](screens/NET-10.md) | Session browser | [NET-10](wireframes/NET-10/NET-10.svg) |

The [current capture matrix](screenshots/README.md#network-presentation-current-capture-matrix) owns 16 network representatives. NET-02-P, NET-05-S, and NET-05-R make existing materially different tasks explicit; they do not add product screens or behavior. Existing legacy diagrams remain at their current locations. Shared token values remain unchanged; network presentation adopts the native panel and control treatments. Prior accepted images are references, not evidence for this new treatment.

Read each screen's functional contract before its visual requirements. Changed presentation in the owning sections above supersedes conflicting older appearance examples only. Product owns legacy inventory counts and functional state IDs; UX must not invent replacement states or copy the functional requirements into this index.

## Resolved contract alignment

The new UX documents own changed presentation only. Unchanged legacy sections remain authoritative. The canonical functional targets resolve the earlier navigation, admission, and password questions; no additional product decision is requested here.

Registration is automatic after host readiness under NET-DIR-001. All active registrations remain visible under NET-DIR-002; UX adds no loopback or private-address filter. Endpoint presentation follows NET-DIR-016 and must not imply verified reachability or silently change the listening interface. Assigned public unicast IPv4 options follow NET-HOST-IF-002–003 without an Internet guarantee. Password rejection uses `Connection not authorized.` under NET-PASS-004. Input validation must follow supported implementation limits without inventing a new UX password policy. Browser-origin failure permits return to NET-10 for refresh. Full, closed, and stale listings cannot start browser joins under NET-DIR-012. Publication recovery follows NET-DIR-015 without a match restart.
