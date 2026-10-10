# NET-03 — Direct setup and operator-configured dedicated admission

Functional authority: [NET-03](../../screens/network-join.md), NET-DIR-013/014, NET-PASS-001–010, NET-ADM-011, NET-PUB-017–021, and retained NET-JOIN-PUB requirements. NET-JOIN-PUB-002 and NET-PUB-016/022 are retired. Structural sources: [NET-03-E](../wireframes/NET-03/NET-03-E.svg), [NET-03-P](../wireframes/NET-03/NET-03-P.svg), and unchanged [NET-03 connecting SVG](../wireframes/NET-03/NET-03.svg). Current develop's [locked-task reference](../../screens/wireframes/network-join.md) remains context for unchanged connecting structure.

States: NET-03-password, NET-03-live-admission, NET-03-PUBLIC-EDIT, and NET-03-PUBLIC-CONNECTING. Visual impact: one direct-only service-type row and a dedicated credential/guidance variant within the current setup task. No deployment, environment, account, or nested connection screen is added.

## Shared structure

- **UX-NET-03-001** Editable setup must retain a fixed endpoint region, two equal-height setup panels, and fixed feedback/actions.
- **UX-NET-03-002** Address, Port, and the applicable credential label must align beside separate inset value regions.
- **UX-NET-03-003** Credential help and origin context must remain outside value regions.
- **UX-NET-03-004** Setup panels must retain current develop's list and row-action treatment without host-only controls.
- **UX-NET-03-005** Connect and Back must retain their centered action column below feedback.
- **UX-NET-03-006** Connecting must retain one full-width locked local-player panel beneath the endpoint.
- **UX-NET-03-007** Connecting must retain Slot, Person, and Control columns and a persistent locked/read-only label.
- **UX-NET-03-008** Connecting must keep status, deadline text, and its sole Cancel action below the roster without clipping the fifteenth slot.
- **UX-NET-03-009** Long endpoint and player values must remain inside their regions.

## Direct service-type row

- **UX-NET-03-010** Supported Linux x86-64 and Windows x86-64 direct setup must show `Service type` above Address in the existing setup-context allocation.
- **UX-NET-03-011** The row must show `Player-hosted session` or `Dedicated service` as its current value.
- **UX-NET-03-012** The row must use a grey whole-row control, inset current value, plain `↻` cue, and one row-level focus keyline.
- **UX-NET-03-013** The cycle cue must not create a separate pointer or focus target.
- **UX-NET-03-014** Confirm, Enter, Space, or a pointer press must change the approved service type once without held repetition or automatic connection.
- **UX-NET-03-015** Changing service type must keep focus on that row.
- **UX-NET-03-016** Browser-origin setup must retain selected-session context without an editable service-type row.
- **UX-NET-03-017** macOS and Raspberry Pi direct setup must retain current player-hosted layout and traversal without a dedicated option or placeholder target.

The direct row replaces the current setup-context line; it does not require a new panel. Retain initial Address focus on ordinary direct entry. Forward traversal must follow Service type, Address, Port, applicable credential, local setup, Connect, and Back. Reverse traversal from Address must reach Service type on supported direct setup. Browser entry retains initial Password focus when protected and local-setup focus when unprotected. Browser traversal remains Address, Port, Password, local setup, Connect, and Back.

## Operator endpoint and invitation

- **UX-NET-03-018** First dedicated setup must show empty Address and Port fields for operator input.
- **UX-NET-03-019** Dedicated fields must not prefill a gameplay hostname, staging endpoint, production endpoint, or service port.
- **UX-NET-03-020** Later dedicated setup must show only retained operator input permitted by the functional contract.
- **UX-NET-03-021** Dedicated setup must show `Invite` after Port instead of the player-hosted Password field.
- **UX-NET-03-022** Dedicated help must say `Invite required` outside the field.
- **UX-NET-03-023** Invite must render one asterisk per entered character and the standard focused-field insertion cue.
- **UX-NET-03-024** Invite must not reveal the last typed character or offer a reveal action.
- **UX-NET-03-025** Focused invitation text must scroll horizontally to keep the insertion position visible.
- **UX-NET-03-026** A cleared invitation must appear as an empty field rather than a retained mask.
- **UX-NET-03-027** Invitation input must remain absent from endpoint, progress, failure, lobby, and diagnostic copy.
- **UX-NET-03-028** Dedicated guidance must say `The first admitted participant controls the session.`
- **UX-NET-03-029** Dedicated guidance must say `Leaving as host ends the session.`
- **UX-NET-03-030** Dedicated setup must not claim a verified connection before security and admission confirmation.
- **UX-NET-03-031** The screen must not offer certificate bypass, plaintext fallback, or another endpoint as automatic recovery.

NET-JOIN-PUB-010–019 own syntax, validation, paste, retention, and clearing. Render missing-invitation feedback as `Enter an invitation`. Render invalid-invitation feedback as `Use 1–256 printable ASCII characters without spaces.` Do not echo, trim, or silently shorten rejected invitation input. Explicit paste must target only the focused invitation. Text entry uses supported keyboard input; controller navigation must not imply an on-screen keyboard.

## Player-hosted and browser preservation

- Player-hosted setup must retain its masked Password and current optional/required help.
- Browser setup must retain its selected endpoint, session identity, directory origin, and contextual Back.
- A dedicated default or mode change must not rewrite browser-selected setup.
- Endpoint edits must remove obsolete listing claims under NET-DIR-014.
- First-round context must retain `Round 1 in progress. You will play immediately if admitted.`
- Listing context must retain `The host confirms availability when you connect.`
- Unlocked endpoint-only direct setup must retain truthful first-contact identity guidance under NET-PASS-010.
- That limitation must not appear as a dedicated certificate waiver or a claim about password-protected admission.

## Allocation, feedback, and acceptance

- Dedicated guidance must stay above the setup panels below the endpoint block.
- Dedicated extra content must reduce visible list rows rather than cover fixed feedback or actions.
- Both list bodies must scroll independently under fixed headings.
- The last visible control, its focus outline, and its pointer bounds must stay inside the list allocation.
- Empty operator fields and invitation must remain recognizable before focus.
- Validation must remain visible while invalid input leaves setup editable.
- Invitation rejection recovery must focus Invite after Edit setup.
- Connecting must remove all editable targets and focus Cancel.
- Cancel must restore permitted endpoint, mode, credential, and local-player state without a false Disconnected or lobby claim.
- Complete admission must show only the current confirmed NET-04 or NET-05 destination.

Check 850 by 700 and 1280 by 720 containment, 1920 by 1080 pointer scaling, all fifteen selectable local slots, long valid endpoints/invites, explicit paste, clearing, both mode changes, and directory-origin preservation. The [scoped matrix](../screenshots/README.md#dedicated-extension-scoped-capture-matrix) adds no browser or platform Cartesian campaign. NET-03-P remains the materially distinct editable dedicated variant; NET-03-E and locked connecting retain their existing identities.
