# NET-03 — Join setup and connecting

Functional contract: [network-join](../../screens/network-join.md), including NET-OWN-001–003 and ADM-OWN-001. States `NET-03-password` and `NET-03-live-admission` consume [NET-PASS-001–007 and NET-DIR-013–014](../../network-host-directory.md) and [NET-ADM-011](../../network-play-first-release.md). Wireframes: [NET-03-E editable setup](../wireframes/NET-03/NET-03-E.svg) and [existing NET-03 connecting](../../screens/wireframes/network-join.md).

## Shared setup

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
