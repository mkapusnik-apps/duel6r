# UX documentation

## Scope and authority

The [product inventory](../screens/README.md) owns screen identities, functional states and behavior. The existing visual design system owns appearance. Baseline `07b2402d3a8046a10f5ea06315c9978f3247019a` preserves the0625f25 NET-01 modal,675c405 NET-04 alignment, mixed browser, bound setup and scoped reconnect. Common optional Password is now owned by NET-PASS-001–018 and the updated NET-03/NET-08/NET-10 requirements. Invitation-only criteria are retired without reusing their IDs. Preserve current structure; no generic widget, screen or workflow redesign is authorized.

The [network-play contract](../network-play-first-release.md) owns NET-PUB and NET-ADM. The [join contract](../screens/network-join.md) owns NET-JOIN-PUB. The [trust policy](../network-trust-and-abuse-limits.md) and [lifecycle contract](../network-host-service-lifecycle.md) own TRU-PUB and HSL-PUB. The [directory contract](../network-host-directory.md) owns mixed player-hosted and dedicated listings and bound selection. The dedicated service is a headless variant of the existing hosted session. This integration adds no account, environment picker, provisioning task, compiled gameplay endpoint, menu, or product screen.

## Principles

- The UI must keep Local Play and unrelated player-hosted journeys unchanged while applying the approved common Password input/correction procedure to both modes.
- The UI must distinguish service type, confirmed participant role, and connection state.
- The UI must preserve a browser-selected endpoint, session identity, and directory origin.
- The UI must not classify all player-hosted connections as insecure private LAN.
- Approved client eligibility and removal of platform-only dedicated blocks follow [NET-10](screens/NET-10.md#selection-and-recovery).
- The UI must explain departure and reconnect consequences without implying receipt of an undelivered request.
- The UI must not infer a terminal cause from silence or an endpoint name.
- Each added control must use current develop's appearance and supported input methods.

## Screen specifications

The approved local/network arena restoration and complete final-screen unification use the existing [design-system owner](../design.md#local-and-network-arena-parity) and [shared final owner](../design.md#shared-final-presentation). This explicit change supersedes older no-redesign/menu-summary statements only for these affected surfaces. No legacy migration or global token change is authorized. [PLAY-01](screens/PLAY-01.md), [OVER-03](screens/OVER-03.md), NET-05 and NET-06 link the new affected SVGs. The [coordinated matrix](screenshots/README.md#arena-parity-and-shared-final-presentation) stays unreleased until functional closure.

Network-capable experimental macOS uses existing NET-01 through NET-10 journeys. The former local-only notice and UX-MAC-MSG-001–011 remain historical-only. The NET-01 owner retains the compact Host/Join/Back modal over the actual menu; retired UX-NET-01-006–008 remain retired. Preserve corrected Mac drawable/pointer behavior. Current common-connection scope and capture limits follow [NET-03](screens/NET-03.md), [NET-10](screens/NET-10.md), and the [current matrix](screenshots/README.md#unified-connection-current-capture-matrix); historical platform exclusions below do not override those owners.

| Screen | Authoritative presentation | Structural wireframes |
|---|---|---|
| PLAY-01 | [Unchanged local arena reference](screens/PLAY-01.md) | [PLAY-01](wireframes/PLAY-01/PLAY-01.svg) |
| OVER-03 | [Local shared final binding](screens/OVER-03.md) | [OVER-03](wireframes/OVER-03/OVER-03.svg) |
| NET-01 | [Current compact entry modal](screens/NET-01.md) over actual retained MENU-01 | Exact incoming [NET-01](wireframes/NET-01/NET-01.svg) from develop0625f25 |
| NET-02 | [Host setup, common Password and hosted capacity](screens/NET-02.md) | Text-only NET-02/LA/LE updates; unchanged NET-02-P |
| NET-03 | [Common Password, selector-free Direct and bound setup](screens/NET-03.md) | NET-03-P removes the selector and restores ordinary Direct allocation; NET-03-E/B and connecting change affected text/annotations only |
| NET-04 | [Current settings and dedicated role context](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Local-reference parity and existing hosting/waiting status](screens/NET-05.md) | [NET-05](wireframes/NET-05/NET-05.svg); unchanged NET-05-W/C/S/R |
| NET-06 | [Network shared final binding](screens/NET-06.md) over the common final owning section | [NET-06](wireframes/NET-06/NET-06.svg) |
| NET-07 | [Dedicated reconnect](screens/NET-07.md) over the current design-system owning section | Existing NET-07; NET-05-C for confirmation |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Confirmed intentional end](screens/NET-09.md) over the current design-system owning section | Existing NET-09 |
| NET-10 | [Latest unified mixed browser](screens/NET-10.md) | Mixed NET-10/E/U composed from developf21a9c1 with six headings, inline states, upper Refresh and single footer |

The [NET-03 owning specification](screens/NET-03.md) defines common Direct NET-03-P, bound player-hosted NET-03-E, bound dedicated NET-03-B, and locked connecting. It owns selector removal, neutral Direct guidance, truthful first-contact copy, protection-aware focus, and unchanged selected binding. These are existing variants, not new screens, nested menus, or a second secret dialog. Common whole-event input feedback and correction follow NET-PASS-011–018, not retired Invite bounds/history.

Current visual intent retains mixed browser columns, inline white-body states, upper Refresh, single-row footer, ongoing admission, common Password, and immutable browser selection. The current [NET-03](screens/NET-03.md), [NET-08](screens/NET-08.md), and [NET-10](screens/NET-10.md) owners supersede prior selector/certificate/platform-only restrictions on those surfaces. Every unrelated wireframe remains byte-for-byte unchanged. No regeneration or legacy migration is authorized. Older grey/five-heading/browser-editable images do not establish current conformance.

## Shared visual and input requirements

- Menu-context screens must retain the centered 850 by 700 logical canvas, background, banner, runtime version, scale cap, and keyline.
- Controls must use the existing grey bevel, inset white value/list, blue heading, readable flat disabled, and black non-color focus treatments.
- Direct and browser setup controls must follow [NET-03](screens/NET-03.md#common-direct-setup); the retired service-type choice creates no target.
- Host settings must retain whole-row cycles, inset current values, and square toggle states with On/Off text.
- Appearance reuse must not add spinner arrows, dropdown targets, text-entry rounds, held repeat, or new host-setting focus stops.
- Existing press timing, reverse traversal, disabled-focus retention, modal input protection, and scaled pointer bounds must remain unchanged.
- Hidden and unsupported controls must have no interaction target.
- New content must stay inside the current 24-logical-pixel menu margin.
- Adjacent control bounds must keep the current clearance rules.
- Long values and focus outlines must remain inside their owning regions.
- Constrained bodies must lose visible rows before text size is reduced or fixed feedback/actions are covered.
- Contextual panels must keep their existing client-relative bounds and retained context.

No new tokens, mobile layout, or accessibility mode is introduced. Product-owned NET-PUB-AC-006 defines the approved client/platform evidence obligations; the [current matrix](screenshots/README.md#unified-connection-current-capture-matrix) distinguishes representative common-UI capture from native acceptance. Existing evidence is not proof of newly eligible dedicated paths.

The [current coordinated matrix](screenshots/README.md#unified-connection-current-capture-matrix) owns affected evidence. Prior [dedicated evidence](screenshots/README.md#dedicated-extension-scoped-capture-matrix) retains its original lineage and unaffected scope, not current common-channel acceptance. Developer owns capture after team declares functional closure. Native verification obligations remain product/team-owned and unwaived. Historical cloud and draft packets are not current acceptance evidence.
