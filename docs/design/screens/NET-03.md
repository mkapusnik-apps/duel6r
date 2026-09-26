# NET-03 — Join setup and connecting

Functional contract: [network-join](../../screens/network-join.md), including NET-OWN-001–003 and ADM-OWN-001. States `NET-03-password` and `NET-03-live-admission` consume [NET-PASS-001–007 and NET-DIR-013–014](../../network-host-directory.md) and [NET-ADM-011](../../network-play-first-release.md). Wireframes: [NET-03-E editable setup](../wireframes/NET-03/NET-03-E.svg) and [existing NET-03 connecting](../../screens/wireframes/network-join.md).

## Shared setup

The [shared visual baseline](../../design.md#unified-network-presentation) applies. NET-03-E retains the endpoint, setup, and footer relationships in its SVG. The existing NET-03 legacy diagram remains authoritative for the separate locked connecting task.

- **UX-NET-03-001** Editable setup must use a fixed endpoint region, two equal-height setup panels, and a fixed feedback/action region.
- **UX-NET-03-002** Address, Port, and Password labels must align in a left column beside separately framed white value regions.
- **UX-NET-03-003** Password help and directory context must remain outside the editable value regions.
- **UX-NET-03-004** The two setup panels must use the same list and row-action treatment as NET-02 without importing host-only controls.
- **UX-NET-03-005** Connect and Back must remain in their existing centered action column below validation and admission context.
- **UX-NET-03-006** Connecting must replace editable setup with one full-width locked local-player panel beneath the retained endpoint.
- **UX-NET-03-007** Locked connecting rows must keep separate Slot, Person, and Control columns with a persistent locked/read-only label.
- **UX-NET-03-008** Connecting must keep its status, deadline text, and sole framed Cancel action below the roster without clipping the fifteenth row.
- **UX-NET-03-009** Long endpoint and player values must remain inside their regions without covering the Port, control, or status text.

- Directory selection must reuse the existing join task instead of adding a password dialog.
- The directory path must prefill the selected endpoint.
- Directory context must retain the advertised session identity under NET-DIR-014.
- The context line must identify the selected session using its endpoint.
- Direct entry must retain editable Address and Port.
- A field labeled `Password` must follow Port on both paths.
- The direct path must show `Enter a password only if the host requires one.`
- A listed protected session must show `Password required` next to the field.
- Password input must be masked.
- A password must never appear in connection progress or failure copy.
- The setup must retain person selection, local control assignment, count, Connect, and Back.
- The editable layout must keep its header and actions fixed while the roster scrolls.
- An advertised first-round session must show `Round 1 in progress. You will play immediately if admitted.`
- The same context region must say `The host confirms availability when you connect.`
- A listing must not cause the screen to claim successful connection.

## Focus and feedback

- Direct entry must initially focus Address.
- Directory entry with a required password must initially focus Password.
- Directory entry without a required password should initially focus local-player setup.
- Traversal must follow Address, Port, Password, roster actions, Connect, and Back.
- Editable endpoint changes must remove claims that describe the previously selected listing.
- Back from directory setup should restore NET-10 and its selected row if that row still exists.
- Back from direct setup must retain its NET-01 destination.
- Connecting must retain the existing NET-03 locked roster layout and Cancel focus.
- Cancel must return to editable setup without losing local-player choices.
- A password correction must not require repeated player setup.
- After validated admission, the UI must show the host-confirmed lobby or arena without an intermediate false lobby claim.

## Acceptance

NET-03-E is a distinct structural variant because editable setup contains endpoint, password, and roster actions absent from the existing connecting layout. No password overlay is needed. Contextual Back and NET-08 browser recovery provide the return-to-browser path required by NET-DIR-014. Password entry and correction remain outside the connection deadline under NET-PASS-003. The UI must use `Connection not authorized.` for NET-08-password-rejected rather than disclose which secret check failed.

The editable representative must show the required-password focus and first-round context without obscuring Connect or Back. The connecting representative must use an actual pending attempt with retained endpoint and locked slots, not a replacement screen or injected runtime state. Focused QA must cover direct entry, invalid fields, optional password, Cancel retention, fifteen locked slots, and long valid input. Capture details are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).
