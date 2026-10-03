# NET-03 — Shared connection setup and dedicated pilot

Functional authority: [NET-03](../../screens/network-join.md), `NET-DIR-013/014`, `NET-PASS-001`–`NET-PASS-010`, `NET-ADM-011`, and `NET-JOIN-PUB-001`–`NET-JOIN-PUB-019`. States: `NET-03-password`, `NET-03-live-admission`, `NET-03-PUBLIC-EDIT`, and `NET-03-PUBLIC-CONNECTING`. Structural sources: [NET-03-E](../wireframes/NET-03/NET-03-E.svg), [NET-03-P](../wireframes/NET-03/NET-03-P.svg), and [NET-03 connecting](../wireframes/NET-03/NET-03.svg).

Visual impact: retain develop's editable/connecting separation and add a bounded pilot invitation task. Do not add a public host-setup screen or password overlay.

## Shared setup

The [unified visual baseline](../../design.md#unified-network-presentation) applies. The pilot must use the same field, list, button, focus, and disabled treatments.

- **UX-NET-03-001** Editable setup must use a fixed endpoint region, two equal-height setup panels, and a fixed feedback/action region.
- **UX-NET-03-002** Address, Port, and the applicable credential label must align beside separately framed white value regions.
- **UX-NET-03-003** Credential help and directory context must remain outside editable value regions.
- **UX-NET-03-004** Setup panels must retain NET-02's list and row-action treatment without host-only controls.
- **UX-NET-03-005** Connect and Back must remain in the existing centered action column below validation and admission context.
- **UX-NET-03-006** Connecting must use one full-width locked local-player panel beneath the retained endpoint.
- **UX-NET-03-007** Connecting must retain separate Slot, Person, and Control columns and a persistent locked/read-only label.
- **UX-NET-03-008** Connecting must keep status, deadline text, and the sole Cancel action below the roster without clipping the fifteenth row.
- **UX-NET-03-009** Long endpoint and player values must remain inside their regions.
- Directory selection must reuse NET-03-E and prefill the selected endpoint.
- Directory context must retain the advertised identity under NET-DIR-014.
- A pilot default must not replace a browser-selected address or port.
- Endpoint edits must remove claims about the previous listing under the functional contract.
- Player-hosted setup must retain the masked Password field and develop's optional/required help.
- Both editable variants must show the explicit service-type selector without changing a browser-selected endpoint.
- Player-hosted setup must not display `Trusted private LAN only` or claim that encryption is absent.
- Unlocked endpoint-only direct setup must show `Endpoint-only first contact does not authenticate host identity.` in the existing context region.
- That limitation must not appear as a certificate waiver on the dedicated pilot or as a claim about password-protected admission.
- Browser-origin setup must retain its return-to-browser path.
- Direct setup must retain its NET-01 Back destination.
- First-round context must retain `Round 1 in progress. You will play immediately if admitted.`
- Listing context must retain `The host confirms availability when you connect.`
- Admission must show only the current confirmed lobby or arena, without an intermediate false lobby claim.

## Pilot endpoint and invitation

The service-type distinction identifies the dedicated lifecycle, not a plaintext security option. NET-JOIN-PUB-001 owns the explicit dedicated/player-hosted choice. Use `Public dedicated pilot` and `Player-hosted session` as presentation labels. Keep product's direct-entry default and browser-selected mode. Do not infer mode or verified identity from an address.

- Pilot setup must show its service-type selector above Address.
- The pilot address must remain editable for production, staging, and custom endpoints.
- Fresh pilot setup must display `duel.netusite.cz` and separate Port `26660` only on the product-approved default path.
- The address field must fit `staging.duel.netusite.cz` at the compatibility floor.
- Port must keep five digits visible.
- The pilot must show a separate field labeled `Invite` after Port.
- Pilot help must say `Invite required` outside the field.
- The pilot must not present the invitation as the player-hosted Password.
- Invite must render one asterisk per entered character and the standard focus underscore.
- Invite must not reveal the last typed character or provide a reveal action.
- Explicit paste must target only the focused Invite field.
- The UI must show a cleared invitation as an empty field rather than a retained mask.
- Retention and clearing must follow NET-JOIN-PUB-015–018.
- Credential input must not appear in progress, failure, lobby, endpoint, or diagnostic copy.
- Public guidance must state `The first admitted participant controls the session.`
- Public guidance must state `Leaving as host ends the session. Server updates may end the session.`
- The UI must not claim encrypted connection or verified server identity before confirmation.
- The UI must not offer certificate bypass or plaintext fallback.

Production is a default address, not evidence of availability. Staging uses the same editable field. Do not add an environment picker or contact a public service for capture. Input validation must use product's bounds. Missing invitation feedback must say `Enter an invitation`. Invalid invitation feedback must say `Use 1–256 printable ASCII characters without spaces.` Do not echo or silently shorten the rejected input.

## Allocation, input, and acceptance

- NET-03-P must keep its endpoint and guidance above the two setup panels below the banner.
- The pilot's extra rows must reduce visible list rows rather than cover validation or actions.
- Each setup list must scroll independently under fixed headings.
- Credential masks must clip inside their field.
- Focused text must scroll horizontally to keep the insertion position visible.
- Direct player-hosted entry must retain initial Address focus.
- Protected browser entry must retain initial Password focus.
- Unprotected browser entry should initially focus local-player setup.
- Fresh pilot entry must focus the service-type selector.
- Pilot traversal must follow service type, Address, Port, Invite, local setup, Connect, and Back.
- Player-hosted traversal must include service type before Address, Port, Password, local setup, Connect, and Back while retaining each route's initial focus.
- Connecting must focus Cancel and remove editable focus and pointer targets.
- Cancel must retain local-player choices and permitted endpoint/credential state.
- Keyboard, controller, and pointer activation semantics must remain unchanged.
- Text entry must use supported keyboard input without implying an on-screen keyboard.

Acceptance requires recognizable unfocused fields, non-overlapping row focus, complete actions, masked credentials, and truthful admission context at both minimum viewports. Test production-default entry separately from browser-selected and custom endpoint retention. Test all three with Cancel and failure recovery. Test player-hosted first-round admission through its real browser route. The [coordinated matrix](../screenshots/README.md#network-presentation-current-capture-matrix) retains NET-03-E and locked NET-03 and adds NET-03-P only for the materially different pilot setup.
