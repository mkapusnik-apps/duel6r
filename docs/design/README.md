# UX documentation

## Scope and authority

The [product inventory](../screens/README.md) owns screen identities, functional states, and behavior. The existing [visual design system](../design.md#unified-network-presentation) owns reusable appearance. Incoming develop `0625f259da7e038a40327ee1e4e67fab0526fa73` owns the NET-01 modal and current shared event/presentation baseline. Corrected dedicated/mixed authority at `757f009be97a6b4a6fe88cef498a39ede4afe251` owns the six-column browser, bound NET-03-B, opaque Invite correction and scoped reconnect behavior. Preserve both approved baselines through their bounded composition; do not migrate legacy sources or create another design system.

The [network-play contract](../network-play-first-release.md) owns NET-PUB and NET-ADM. The [join contract](../screens/network-join.md) owns NET-JOIN-PUB. The [trust policy](../network-trust-and-abuse-limits.md) and [lifecycle contract](../network-host-service-lifecycle.md) own TRU-PUB and HSL-PUB. The [directory contract](../network-host-directory.md) owns mixed player-hosted and dedicated listings and bound selection. The dedicated service is a headless variant of the existing hosted session. This integration adds no account, environment picker, provisioning task, compiled gameplay endpoint, menu, or product screen.

## Principles

- The UI must keep Local Play and the existing player-hosted journeys unchanged.
- The UI must distinguish service type, confirmed participant role, and connection state.
- The UI must preserve a browser-selected endpoint, session identity, and directory origin.
- The UI must not classify all player-hosted connections as insecure private LAN.
- The UI must show decoded dedicated listings on updated macOS and Raspberry Pi clients without enabling dedicated admission or credential entry.
- The UI must explain departure and reconnect consequences without implying receipt of an undelivered request.
- The UI must not infer a terminal cause from silence or an endpoint name.
- Each added control must use current develop's appearance and supported input methods.

## Screen specifications

Network-capable experimental macOS uses the existing NET-01 through NET-10 journeys. The former `MENU-01-MAC-LOCAL` / `MENU-02-MAC-LOCAL` notice and UX-MAC-MSG-001–011 are [historical-only](screens/MENU-02.md). The [NET-01 owner](screens/NET-01.md#presentation-requirements) now defines a compact modal over the actual main menu with Host, browser-entry Join, and Back. UX-NET-01-006–008 are retired; entry scope copy is removed. Only the NET-01 structural SVG changes for this bounded refinement; all other wireframes remain unchanged. Preserve the corrected Mac drawable/pointer baseline throughout network transitions. Actual native Mac visual assessment remains [deferred](screenshots/README.md#macos-deferred-coverage), separate from mandatory native automated acceptance.

| Screen | Authoritative presentation | Structural wireframes |
|---|---|---|
| NET-01 | [Current compact entry modal](screens/NET-01.md) over actual retained MENU-01 | Exact incoming [NET-01](wireframes/NET-01/NET-01.svg) from develop0625f25 |
| NET-02 | Current develop's [host setup](screens/NET-02.md), unchanged by this extension | NET-02, NET-02-LA, NET-02-LE, NET-02-P from the fixed develop baseline |
| NET-03 | [Direct setup and bound dedicated selection](screens/NET-03.md) | Unchanged player-hosted [NET-03-E](wireframes/NET-03/NET-03-E.svg); unchanged unbound dedicated [NET-03-P](wireframes/NET-03/NET-03-P.svg); selected dedicated [NET-03-B](wireframes/NET-03/NET-03-B.svg); unchanged [NET-03 connecting SVG](wireframes/NET-03/NET-03.svg), with current develop's [locked-task context](../screens/wireframes/network-join.md) |
| NET-04 | [Current settings and dedicated role context](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Dedicated status within current match presentation](screens/NET-05.md) | Existing NET-05; unchanged NET-05-C, NET-05-S, NET-05-R |
| NET-06 | [Dedicated summary boundary](screens/NET-06.md) over the current design-system owning section | Existing NET-06 |
| NET-07 | [Dedicated reconnect](screens/NET-07.md) over the current design-system owning section | Existing NET-07; NET-05-C for confirmation |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Confirmed intentional end](screens/NET-09.md) over the current design-system owning section | Existing NET-09 |
| NET-10 | [Mixed session browser](screens/NET-10.md) within the existing task | [NET-10](wireframes/NET-10/NET-10.svg), updated only for mixed classification, selected details, and eligibility |

NET-03-P remains the unbound direct dedicated editable variant because it includes the service-type row, invitation help, and controller guidance with reduced visible setup rows. Player-hosted browser setup remains NET-03-E. NET-03-B is the selected dedicated variant with locked endpoint, session identity, and hosting/protection context, initial Invite focus, and contextual Back. These are variants of NET-03, not new product screens. Connecting retains the existing locked NET-03 task. No nested menu or second secret dialog is added. NET-03-P's older browser annotation continues to refer to player-hosted NET-03-E; NET-03-B owns selected dedicated structure.

Accept the approved incoming NET-01 SVG byte-for-byte. All other wireframes, including the existing mixed NET-10, bound NET-03-B, direct NET-03-P, player-hosted NET-03-E, NET-02 variants, NET-04-R and NET-05 confirmation/context diagrams, remain byte-exact to corrected757f009. This integration does not regenerate them or migrate legacy diagrams. NET-04 and NET-04-R retain current settings structure and dedicated header context.

## Shared visual and input requirements

- Menu-context screens must retain the centered 850 by 700 logical canvas, background, banner, runtime version, scale cap, and keyline.
- Controls must use the existing grey bevel, inset white value/list, blue heading, readable flat disabled, and black non-color focus treatments.
- A service-type choice must use one whole-row cycle target with a plain `↻` cue.
- Host settings must retain whole-row cycles, inset current values, and square toggle states with On/Off text.
- Appearance reuse must not add spinner arrows, dropdown targets, text-entry rounds, held repeat, or new host-setting focus stops.
- Existing press timing, reverse traversal, disabled-focus retention, modal input protection, and scaled pointer bounds must remain unchanged.
- Hidden and unsupported controls must have no interaction target.
- New content must stay inside the current 24-logical-pixel menu margin.
- Adjacent control bounds must keep the current clearance rules.
- Long values and focus outlines must remain inside their owning regions.
- Constrained bodies must lose visible rows before text size is reduced or fixed feedback/actions are covered.
- Contextual panels must keep their existing client-relative bounds and retained context.

No new tokens, mobile layout, or accessibility mode is introduced. Existing macOS drawable/pointer corrections and deferred native Mac evidence remain applicable only to their approved player-hosted scope. Dedicated clients are Linux x86-64 and Windows x86-64 under NET-PUB-AC-006.

The [scoped dedicated matrix](screenshots/README.md#dedicated-extension-scoped-capture-matrix) owns changed evidence. It does not invalidate every develop representative or revive PR90's retired seventeen-row plan. Developer owns capture after team declares functional closure. Native Windows dedicated verification remains required and unwaived. Historical cloud and draft packets are not current acceptance evidence.
