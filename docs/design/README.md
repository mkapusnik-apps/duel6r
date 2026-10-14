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
| NET-02 | [Host setup, common Password and hosted capacity](screens/NET-02.md) | Text-only NET-02/LA/LE updates; unchanged NET-02-P |
| NET-03 | [Common Password, direct and bound setup in both hosting modes](screens/NET-03.md) | Unchanged connecting; approved text/focus-annotation-only NET-03-E, NET-03-P and NET-03-B, with all geometry unchanged |
| NET-04 | [Current settings and dedicated role context](screens/NET-04.md) | [NET-04](wireframes/NET-04/NET-04.svg); [NET-04-R](wireframes/NET-04/NET-04-R.svg) |
| NET-05 | [Hosting-aware active and Connected-waiting presentation](screens/NET-05.md) | Existing live NET-05; incoming NET-05-W; unchanged NET-05-C/S/R |
| NET-06 | [Dedicated summary boundary](screens/NET-06.md) over the current design-system owning section | Existing NET-06 |
| NET-07 | [Dedicated reconnect](screens/NET-07.md) over the current design-system owning section | Existing NET-07; NET-05-C for confirmation |
| NET-08 | [Failure and recovery](screens/NET-08.md) | Existing NET-08 |
| NET-09 | [Confirmed intentional end](screens/NET-09.md) over the current design-system owning section | Existing NET-09 |
| NET-10 | [Latest unified mixed browser](screens/NET-10.md) | Mixed NET-10/E/U composed from developf21a9c1 with six headings, inline states, upper Refresh and single footer |

NET-03-P remains unbound direct dedicated setup with its existing service-type row, optional Password and controller guidance. Browser-selected player-hosted NET-03-E and dedicated NET-03-B both retain read-only endpoint, identity, hosting/protection and selected origin. Protected selection requires Password with initial Password focus; No password keeps the optional field reachable with initial local-setup focus. Player-hosted context contains no dedicated PKI/controller copy; unlocked player-hosted first-contact guidance applies even to browser labels. Only independent Direct exposes endpoint editing. Connecting stays locked. These are existing variants, not new states/screens, nested menus or a second secret dialog. The transport composition creates no nested UI. Common whole-event input feedback and correction follow NET-PASS-011–018, not retired Invite bounds/history.

Current visual intent composes develop `f21a9c1889ff4382bd20e2654b758783be49d07d` with common Password and immutable browser selection in both hosting modes. NET-10/E/U adopt inline white-body states, upper Refresh, single-row footer and revised traversal while retaining six columns and mixed context. NET-02/LA/LE change only required capacity/Password text. NET-03-E changes only ongoing context while retaining bound targets and Password focus. NET-05-W adds the incoming waiting structure. NET-04/NET-04-R, NET-03-P/B, connecting, NET-02-P, NET-05-C/S/R, NET-08 and every unrelated SVG remain byte-for-byte unchanged from checkpoint72adb399. No regeneration or legacy migration is authorized. Older grey/five-heading/browser-editable images do not establish current conformance.

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
