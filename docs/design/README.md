# UX documentation

## Scope and authority

The [product inventory](../screens/README.md) owns screen and functional state IDs. The existing [visual design system](../design.md#unified-network-presentation) owns reusable styling, including `UX-NET-001`–`UX-NET-024` and `UX-NET-AC-01`–`UX-NET-AC-08`. Keep develop's unified network presentation as the base. Do not migrate the legacy design system or create a second token authority.

The [directory contract](../network-host-directory.md) owns `NET-DIR-*` and `NET-PASS-*`. The [network-play contract](../network-play-first-release.md) owns `NET-ADM-*` and `NET-PUB-*`. The [trust policy](../network-trust-and-abuse-limits.md) and [lifecycle contract](../network-host-service-lifecycle.md) own `TRU-PUB-*` and `HSL-PUB-*`. Product's reconciled contracts preserve secure player-hosted mode, browser-selected endpoints, and dedicated round-one admission. This UX direction does not decide protocols, admission policy, or termination mappings.

## Principles

- The UI must keep Local Play independent of network availability.
- The UI must keep Host, Browse sessions, Direct connect, and Back discoverable.
- The UI must distinguish a listing from a reachable session.
- The UI must distinguish service type, verified connection state, and confirmed participant role.
- The UI must not describe every player-hosted endpoint as an insecure private-LAN connection.
- The UI must preserve a browser-selected endpoint instead of replacing it with the pilot default.
- The UI must explain the consequence of leaving before confirmation.
- The UI must not infer a termination cause from silence.
- Visual reuse must preserve existing input, ownership, disabled-focus, and confirmation semantics.

## Screen specifications

| Screen | Owning presentation | Structural source |
|---|---|---|
| NET-01 | [Entry](screens/NET-01.md) | [NET-01](wireframes/NET-01/NET-01.svg) |
| NET-02 | Develop's [host setup](screens/NET-02.md), unchanged by the pilot | NET-02 and NET-02-P from develop |
| NET-03 | [Shared setup and pilot extension](screens/NET-03.md) | [NET-03-E](wireframes/NET-03/NET-03-E.svg); [NET-03-P](wireframes/NET-03/NET-03-P.svg); [NET-03](wireframes/NET-03/NET-03.svg) |
| NET-04 | [Lobby and retained results](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); NET-04-R from develop |
| NET-05 | [Match and contextual panels](screens/NET-05.md) | Existing NET-05; NET-05-C, NET-05-S, NET-05-R from develop |
| NET-06 | [Pilot summary extension](screens/NET-06.md), over the existing unified owning section | Existing NET-06 |
| NET-07 | [Pilot reconnect extension](screens/NET-07.md), over the existing unified owning section | Existing NET-07 |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Pilot confirmed-end extension](screens/NET-09.md), over the existing unified owning section | Existing NET-09 |
| NET-10 | Develop's [browser](screens/NET-10.md), unchanged by the pilot | NET-10 from develop |

Accept unchanged incoming develop UX files and wireframes at their existing paths. The links to those incoming sources become available when team/developer performs the merge. Do not restore PR90's older NET-04-R diagram over develop's host-reorder indication. Do not restore PR90's editable NET-03 diagram over develop's locked connecting task. NET-03-P is the only added structural variant. It makes the pilot invitation task explicit without adding a product screen, environment picker, password dialog, or dedicated directory listing.

## Cross-screen presentation

- Menu-context screens must retain the 850 by 700 logical canvas and existing uniform scaling.
- New content must stay inside the existing 24-logical-pixel margin below the banner and version.
- Adjacent controls must keep at least 8 logical pixels of clear space.
- Fields and actions must remain identifiable before focus moves to them.
- A disabled control that retains baseline focus must keep its focus outline without accepting activation.
- Hidden controls must have no focus or pointer target.
- Long values must remain inside their regions.
- Focused field text must keep the insertion position visible.
- Fixed headings, feedback, and actions must remain visible while body rows scroll.
- A constrained body must lose visible rows before it reduces text size or covers actions.
- Contextual panels must preserve their existing client-relative bounds and retained context.
- No mobile layout or new appearance profile is introduced.

Use the [single coordinated capture matrix](screenshots/README.md#network-presentation-current-capture-matrix). Earlier draft and develop screenshots are historical for the reconciled candidate. They cannot establish candidate acceptance. Developer owns capture after team declares functional closure. The unavailable native Windows graphical dedicated journey remains a blocker, not a waiver. No public deployment is required by this UX task.
