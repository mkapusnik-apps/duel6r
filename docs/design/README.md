# UX documentation

## Scope and authority

This index owns new UX specifications. The existing [visual design system](../design.md) remains canonical for unchanged styling. This task does not migrate legacy documentation. The [product inventory](../screens/README.md) owns screen identities and functional contracts.

The host-directory work is a localized extension of the graphical network flow, not a redesign. The [host-directory contract](../network-host-directory.md) owns NET-DIR-001–016 and NET-PASS-001–007. The [network-play contract](../network-play-first-release.md) owns NET-ADM-001–012. These canonical requirements and the current functional screen targets supersede conflicting legacy exclusions. UX does not define service protocols, heartbeat intervals, expiry durations, or password policy.

Product reserves **NET-10** in the [browser contract](../screens/network-browser.md). Its stable states are `NET-10-loading`, `NET-10-results`, `NET-10-empty`, `NET-10-stale`, and `NET-10-unavailable`. Related states are `NET-02-password`, `NET-03-password`, `NET-03-live-admission`, `NET-08-password-rejected`, and `NET-08-admission-closed`.

## Principles

- The UI must keep local play and direct connection discoverable during directory failure.
- The UI must distinguish a listing from a reachable host.
- The UI must distinguish password protection, match phase, and admission availability.
- The UI must preserve the existing desktop canvas, controls, and arena presentation.
- The UI must keep feedback visible without requiring color recognition.

## Screen specifications

| Screen | UX ownership for this extension | Structural wireframes |
|---|---|---|
| [NET-01](screens/NET-01.md) | Network navigation | NET-01 |
| [NET-02](screens/NET-02.md) | Host password field | NET-02 |
| [NET-03](screens/NET-03.md) | Shared direct/directory join setup | NET-03-E; existing NET-03 connecting |
| [NET-04](screens/NET-04.md) | Host listing feedback | Existing NET-04 and NET-04-R |
| [NET-05](screens/NET-05.md) | First-round arrival presentation | Existing NET-05 |
| [NET-08](screens/NET-08.md) | Admission rejection presentation | Existing NET-08 |
| [NET-10](screens/NET-10.md) | Session browser | NET-10 |

The [capture matrix](screenshots/README.md#host-directory-current-capture-matrix) owns coverage for this extension. Unaffected legacy coverage remains in the linked legacy manifest. No shared visual token changes are needed.

## Resolved contract alignment

The new UX documents own changed presentation only. Unchanged legacy sections remain authoritative. The canonical functional targets resolve the earlier navigation, admission, and password questions; no additional product decision is requested here.

Registration is automatic after host readiness under NET-DIR-001. All active registrations remain visible under NET-DIR-002; UX adds no loopback or private-address filter. Endpoint presentation follows NET-DIR-016 and must not imply verified reachability or silently change the listening interface. Assigned public unicast IPv4 options follow NET-HOST-IF-002–003 without an Internet guarantee. Password rejection uses `Connection not authorized.` under NET-PASS-004. Input validation must follow supported implementation limits without inventing a new UX password policy. Browser-origin failure permits return to NET-10 for refresh. Full, closed, and stale listings cannot start browser joins under NET-DIR-012. Publication recovery follows NET-DIR-015 without a match restart.
