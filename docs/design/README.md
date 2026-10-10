# UX documentation

## Scope and authority

The [product inventory](../screens/README.md) owns screen identities, functional states, and behavior. The existing [visual design system](../design.md#unified-network-presentation) owns reusable appearance. Current develop at `412abfaae857ab65ba21746819de54c3ac4446be` is the visual integration baseline. Its unified network controls, host-settings refinement, listening multiselect, release/content line, and platform context take precedence over older PR90 presentation. Do not migrate legacy sources or create another design system.

The [network-play contract](../network-play-first-release.md) owns NET-PUB and NET-ADM. The [join contract](../screens/network-join.md) owns NET-JOIN-PUB. The [trust policy](../network-trust-and-abuse-limits.md) and [lifecycle contract](../network-host-service-lifecycle.md) own TRU-PUB and HSL-PUB. The [directory contract](../network-host-directory.md) owns player-hosted listings and passwords. The dedicated extension adds no listing, account, environment picker, provisioning task, or published gameplay endpoint.

## Principles

- The UI must keep Local Play and the existing player-hosted journeys unchanged.
- The UI must distinguish service type, confirmed participant role, and connection state.
- The UI must preserve a browser-selected endpoint, session identity, and directory origin.
- The UI must not classify all player-hosted connections as insecure private LAN.
- The UI must not show unsupported dedicated entry on macOS or Raspberry Pi.
- The UI must explain departure and reconnect consequences without implying receipt of an undelivered request.
- The UI must not infer a terminal cause from silence or an endpoint name.
- Each added control must use current develop's appearance and supported input methods.

## Screen specifications

| Screen | Owning presentation | Structural sources |
|---|---|---|
| NET-01 | [Current entry](screens/NET-01.md), unchanged appearance | [NET-01](wireframes/NET-01/NET-01.svg) |
| NET-02 | Current develop's [host setup](screens/NET-02.md), unchanged by this extension | NET-02, NET-02-LA, NET-02-LE, NET-02-P from the fixed develop baseline |
| NET-03 | [Direct setup and dedicated variant](screens/NET-03.md) | [NET-03-E](wireframes/NET-03/NET-03-E.svg); [NET-03-P](wireframes/NET-03/NET-03-P.svg); unchanged [NET-03 connecting SVG](wireframes/NET-03/NET-03.svg), with current develop's [locked-task context](../screens/wireframes/network-join.md) |
| NET-04 | [Current settings and dedicated role context](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Dedicated status within current match presentation](screens/NET-05.md) | Existing NET-05; unchanged NET-05-C, NET-05-S, NET-05-R |
| NET-06 | [Dedicated summary boundary](screens/NET-06.md) over the current design-system owning section | Existing NET-06 |
| NET-07 | [Dedicated reconnect](screens/NET-07.md) over the current design-system owning section | Existing NET-07; NET-05-C for confirmation |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Confirmed intentional end](screens/NET-09.md) over the current design-system owning section | Existing NET-09 |
| NET-10 | Current develop's [browser](screens/NET-10.md), unchanged by this extension | NET-10 from the fixed develop baseline |

NET-03-P remains the dedicated editable variant because it includes the service-type row, invitation help, and controller guidance with reduced visible setup rows. It is not a new product screen. Browser setup remains NET-03-E. Connecting retains the existing locked NET-03 task. NET-03-P does not require a nested menu or a second secret dialog.

Accept all unaffected incoming develop sources byte-for-byte. In particular, preserve NET-02, NET-02-LA, NET-02-LE, NET-02-P, NET-10, and unchanged NET-05 contextual diagrams. These files are supplied by the develop merge; this specification does not reconstruct them from PR90's older versions. NET-04 and NET-04-R retain the current develop settings structure, with only dedicated header context added.

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
