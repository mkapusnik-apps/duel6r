# UX documentation

## Authority

The [product screen inventory](../screens/README.md) owns screen identities and functional contracts. The existing [visual design system](../design.md) remains authoritative for unchanged styling. This task does not migrate legacy documentation.

This directory specifies presentation for the approved public, encrypted, invite-only dedicated-service extension. The reconciled public sections in the product screen contracts, trust policy, and service lifecycle contract own behavior. The extension specifications below are ready for implementation. Public presentation supersedes legacy LAN-only copy only on the public path. No application implementation or screenshot capture is included here.

## Principles

- The interface must keep Local Play independent of network availability.
- The interface must distinguish public-service connection from private-LAN hosting.
- The interface must show a confirmed participant role separately from connection state.
- The interface must explain the consequence of leaving before the user confirms it.
- The interface must not infer a termination cause from silence.
- The interface must reuse existing desktop controls and navigation patterns.

## Screen direction

| Screen | UX extension | Structural source |
|---|---|---|
| NET-01 | [Network entry](screens/NET-01.md) | [NET-01](wireframes/NET-01/NET-01.svg) |
| NET-03 | [Endpoint and invite](screens/NET-03.md) | [NET-03](wireframes/NET-03/NET-03.svg) |
| NET-04 | [Confirmed role and lobby](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); existing [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Public match status](screens/NET-05.md) | Existing NET-05 and NET-05-C |
| NET-06 | [Public summary status](screens/NET-06.md) | Existing NET-06 |
| NET-07 | [Reconnect](screens/NET-07.md) | Existing [NET-07](../screens/wireframes/network-reconnect.md) |
| NET-08 | [Failure](screens/NET-08.md) | Existing [NET-08](../screens/wireframes/network-failure.md) |
| NET-09 | [Confirmed end](screens/NET-09.md) | Existing [NET-09](../screens/wireframes/network-host-ended.md) |

The new SVGs retain existing wireframe IDs. They are the structural references for the extension's changed regions and do not add screen identities. Legacy prose wireframes remain context for unchanged regions, not competing public-mode direction.

The [capture matrix and assessment](screenshots/README.md#public-service-extension--pending-coverage) own this extension's current coverage and visual gate. Existing accepted baseline evidence stays in its legacy manifest. This is not a migration of historical evidence.

## Cross-screen presentation

- The public-service path must use the existing 850 by 700 logical menu canvas.
- The public-service path must retain the existing menu scaling and 24-logical-pixel inner margin.
- Adjacent controls must keep at least 8 logical pixels of clear space.
- New fields must use existing text-field, focus, disabled, and overflow treatments.
- Keyboard and controller traversal must skip hidden, read-only, and disabled controls.
- Pointer targets must follow the same scaled bounds as visible controls.
- Editable field text must remain inside its field.
- Focused field text must keep the insertion position visible.
- Long prose must wrap without covering actions.
- No mobile layout or new appearance profile is introduced.

No shared token change is needed. A duplicate `design-system.md` would create a second authority and is intentionally not added.

## Reconciled functional references

- [Public session contract](../network-play-first-release.md): NET-PUB-001–023; NET-PUB-AC-001–006.
- [Connection contract](../screens/network-join.md): NET-JOIN-PUB-001–019; NET-JOIN-PUB-AC-001–003.
- [Trust policy](../network-trust-and-abuse-limits.md): TRU-PUB-001–019; TRU-PUB-AC-001–003.
- [Lifecycle contract](../network-host-service-lifecycle.md): HSL-PUB-001–013; HSL-PUB-AC-001–002.

The screen specifications reference these authorities rather than define new behavior. There are no remaining product questions for this presentation scope. The existing design system still owns unchanged tokens, controls, scaling, and input feedback. Public-mode role and status copy is specified in the owning screen extensions above.
