# UX documentation

## Scope and authority

The [product inventory](../screens/README.md) owns screen identities, functional states and behavior. The existing visual design system owns appearance. Baseline `07b2402d3a8046a10f5ea06315c9978f3247019a` preserves the0625f25 NET-01 modal,675c405 NET-04 alignment, mixed browser, bound setup and scoped reconnect. Common optional Password is now owned by NET-PASS-001–018 and the updated NET-03/NET-08/NET-10 requirements. Invitation-only criteria are retired without reusing their IDs. Preserve current structure; no generic widget, screen or workflow redesign is authorized.

The [network-play contract](../network-play-first-release.md) owns NET-PUB and NET-ADM. The [join contract](../screens/network-join.md) owns NET-JOIN-PUB. The [trust policy](../network-trust-and-abuse-limits.md) and [lifecycle contract](../network-host-service-lifecycle.md) own TRU-PUB and HSL-PUB. The [directory contract](../network-host-directory.md) owns mixed player-hosted and dedicated listings and bound selection. The dedicated service is a headless variant of the existing hosted session. This integration adds no account, environment picker, provisioning task, compiled gameplay endpoint, menu, or product screen.

## Principles

- The UI must keep Local Play and unrelated player-hosted journeys unchanged while applying the approved common Password input/correction procedure to both modes.
- The UI must distinguish service type, confirmed participant role, and connection state.
- The UI must preserve a browser-selected endpoint, session identity, and directory origin.
- The UI must not classify all player-hosted connections as insecure private LAN.
- The UI must show decoded dedicated listings on updated macOS and Raspberry Pi clients without enabling dedicated admission or credential entry.
- The UI must explain departure and reconnect consequences without implying receipt of an undelivered request.
- The UI must not infer a terminal cause from silence or an endpoint name.
- Each added control must use current develop's appearance and supported input methods.

## Screen specifications

Network-capable experimental macOS uses existing NET-01 through NET-10 journeys. The former local-only notice and UX-MAC-MSG-001–011 remain historical-only. The NET-01 owner retains the compact Host/Join/Back modal over the actual menu; retired UX-NET-01-006–008 remain retired. This Password integration does not change that SVG or the675c405 aligned NET-04 geometry. Preserve corrected Mac drawable/pointer behavior and unsupported dedicated entry rules. Actual Mac visual assessment remains deferred under its existing native phase, separate from post-integration automatic verification.

| Screen | Authoritative presentation | Structural wireframes |
|---|---|---|
| NET-01 | [Current compact entry modal](screens/NET-01.md) over actual retained MENU-01 | Exact incoming [NET-01](wireframes/NET-01/NET-01.svg) from develop0625f25 |
| NET-02 | Current develop's [host setup](screens/NET-02.md), unchanged by this extension | NET-02, NET-02-LA, NET-02-LE, NET-02-P from the fixed develop baseline |
| NET-03 | [Common Password, direct and bound dedicated setup](screens/NET-03.md) | Unchanged NET-03-E/connecting; approved text/focus-annotation-only NET-03-P and NET-03-B, with all geometry unchanged |
| NET-04 | [Current settings and dedicated role context](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Dedicated status within current match presentation](screens/NET-05.md) | Existing NET-05; unchanged NET-05-C, NET-05-S, NET-05-R |
| NET-06 | [Dedicated summary boundary](screens/NET-06.md) over the current design-system owning section | Existing NET-06 |
| NET-07 | [Dedicated reconnect](screens/NET-07.md) over the current design-system owning section | Existing NET-07; NET-05-C for confirmation |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Confirmed intentional end](screens/NET-09.md) over the current design-system owning section | Existing NET-09 |
| NET-10 | [Mixed session browser](screens/NET-10.md) within the existing task | [NET-10](wireframes/NET-10/NET-10.svg), updated only for mixed classification, selected details, and eligibility |

NET-03-P remains unbound direct dedicated setup with its existing service-type row, optional Password and controller guidance. Player-hosted browser setup remains NET-03-E. NET-03-B retains locked endpoint, identity and hosting/protection with Password required/initial focus for protected selection, or the same optional field and initial local-setup focus for No password. Connecting stays locked. These are existing variants, not new states/screens, nested menus or a second secret dialog. The transport composition creates no nested UI. Common whole-event input feedback and correction follow NET-PASS-011–018, not retired Invite bounds/history.

Only NET-03-P, NET-03-B and NET-10 SVG text/focus annotations change for common Password; no positions, rectangles, headings, list allocation, keylines or geometry change. Every other SVG remains byte-exact to07b2402, including the current NET-01 modal, aligned NET-04, NET-04-R, NET-02 variants and confirmation diagrams. Do not regenerate or migrate wireframes. Historical screenshots, producer identities and assessments retain their original scope and are not common-password evidence.

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
