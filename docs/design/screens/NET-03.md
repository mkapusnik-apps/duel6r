# NET-03 — Endpoint and invite entry

Functional authority: [NET-03](../../screens/network-join.md), NET-JOIN-PUB-001–019 and NET-JOIN-PUB-AC-001–003. Wireframe: [NET-03](../wireframes/NET-03/NET-03.svg). States: `NET-03-PUBLIC-EDIT` and `NET-03-PUBLIC-CONNECTING`; private setup retains its existing states.

Visual impact: one invite row and public-service context in the existing endpoint region. Reuse the current Persons and Local Players controls. Do not add a public host setup screen.

## Endpoint region

- The title must be `CONNECT TO NETWORK SESSION` because the confirmed role can be Host or Guest.
- The first row must show a `Connection type` selector with `Public (encrypted)` and `Private LAN (trusted)` options.
- The address field must show `duel.netusite.cz` on the approved production-default path.
- The address field must remain editable for a custom endpoint.
- The field must fit `staging.duel.netusite.cz` without clipping at the compatibility floor.
- The address row must include a separate Port field wide enough for five digits.
- Initial public setup must display the contract's default port `26660`.
- The label must identify the address as `Server address`, not the participant Host.
- Public connection guidance must state `Invite required` before Connect.
- The Invite field must sit directly below the endpoint controls.
- Invite text must render one asterisk per entered character with the standard focus underscore.
- The field must never show the last typed character or a reveal action.
- The focused mask must scroll horizontally to keep the insertion position visible.
- The Invite field and its focus target must be absent in private-LAN mode.
- The reserved Invite row must instead show `Trusted private LAN only` in private-LAN mode without moving the setup panels.
- Invite input must not appear in connection, error, lobby, or diagnostic copy.
- The screen must not advertise a connected or encrypted state before the runtime verifies it.
- The screen must not show a certificate-bypass or plaintext-fallback action.

The production hostname is the default, not evidence of service readiness. Staging is entered as a custom endpoint for testing. This direction adds no environment picker. NET-JOIN-PUB-010–019 own input syntax, typing/paste, and invitation lifetime. The UI must show a cleared invitation as an empty field, not a retained mask. An unchanged cancelled attempt must restore its retained mask. A hostname or port edit must immediately remove that mask under the functional contract.

## Allocation and focus

- The endpoint region must fit above logical y=290 in the wireframe.
- Server address must receive the remaining row width after the Port control and the control gap.
- The Invite field must span the same width as the address field.
- The Persons and Local Players panels must keep equal heights and independent vertical scrolling.
- Panel headings, validation text, and footer actions must remain fixed while rows scroll.
- The footer must retain Connect and Back in editable setup.
- Connecting must retain only the approved Cancel action and lock setup.
- Validation must remain visible near its field or in the fixed status region.
- Initial focus must be Connection type.
- Focus order must follow Connection type, Server address, Port, Invite in public mode, local setup controls, Connect, and Back.
- Connecting must focus Cancel.
- Text entry requires the supported keyboard input method; controller navigation must not imply an on-screen keyboard exists.

Keep the existing person/control selection mechanisms. Invite entry must support explicit keyboard paste into the focused field as required by NET-JOIN-PUB-014. No automatic clipboard reading or additional Paste button is needed. Invalid input feedback must not echo the submitted value. Missing input must show `Enter an invitation`. Invalid input must show `Use 1–256 printable ASCII characters without spaces.` The screen must present rejection without silently shortening or changing the submitted value. Connect must remain visibly unavailable when the current setup fails the functional validation contract.

## Feedback and acceptance

- The public pre-connection note must state `The first admitted participant controls the session.`
- A second line must state `Leaving as host ends the session. Server updates may end the session.`
- The connecting label must say Connecting until admission completes.
- The public path must not use player-hosted `Starting session` or listening-interface copy.
- Inline invalid input must leave the screen editable under the approved validation contract.
- Cancel and failure recovery must preserve non-secret setup under the existing contract.
- Invite retention and clearing must follow NET-JOIN-PUB-015–018.
- Maximum-length valid addresses and invites must not cover local setup, feedback, or actions.
- The public and private paths must remain distinguishable without color.

SS-017 uses editable public setup as the new representative to expose the endpoint and Invite field. Connecting and Cancel remain required behavioral checks, not new wireframe IDs.
