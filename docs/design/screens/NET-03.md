# NET-03 — Direct setup and operator-configured dedicated admission

Functional authority: [NET-03](../../screens/network-join.md), NET-DIR-013/014, common NET-PASS-001–018, NET-ADM-011, NET-PUB-017–021 and retained NET-JOIN-PUB-001/006/008–011. Invitation-only functional and UX identifiers are retired, never reused. Structural sources retain [NET-03-E](../wireframes/NET-03/NET-03-E.svg), text-only updated [NET-03-P](../wireframes/NET-03/NET-03-P.svg) and [NET-03-B](../wireframes/NET-03/NET-03-B.svg), and unchanged [NET-03 connecting SVG](../wireframes/NET-03/NET-03.svg). Geometry is byte-exact to baseline07b2402; the current locked-task reference remains context for connecting.

States: NET-03-password, NET-03-live-admission, NET-03-PUBLIC-EDIT, NET-03-PUBLIC-CONNECTING, and NET-03-dedicated-selected. Visual impact: the existing direct-only service-type row and dedicated credential/guidance variant, plus bound dedicated selection within the current setup task. No deployment, environment, account, or nested connection screen is added.

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
- **UX-NET-03-016** Browser-origin setup must retain selected-session context and decoded hosting type without an editable service-type row.
- **UX-NET-03-017** macOS and Raspberry Pi direct setup must retain current player-hosted layout and traversal without a dedicated option or placeholder target.

The direct row replaces the current setup-context line; it does not require a new panel. Retain initial Address focus on ordinary direct entry. Forward traversal must follow Service type, Address, Port, applicable credential, local setup, Connect, and Back. Reverse traversal from Address must reach Service type on supported direct setup. Browser entry retains initial Password focus when protected and local-setup focus when unprotected. Browser traversal remains Address, Port, Password, local setup, Connect, and Back.

## Operator endpoint and common Password

- **UX-NET-03-018** First dedicated setup must show empty Address and Port fields for operator input.
- **UX-NET-03-019** Dedicated fields must not prefill a gameplay hostname, staging endpoint, production endpoint, or service port.
- **UX-NET-03-020** Later dedicated setup must show only retained operator input permitted by the functional contract.
- **UX-NET-03-028** Dedicated guidance must say `The first admitted participant controls the session.`
- **UX-NET-03-029** Dedicated guidance must say `Leaving as host ends the session.`
- **UX-NET-03-030** Dedicated setup must not claim a verified connection before security and admission confirmation.
- **UX-NET-03-031** The screen must not offer certificate bypass, plaintext fallback, or another endpoint as automatic recovery.

UX-NET-03-021–027/038/042 are retired invitation-only identifiers and must never be reused. NET-JOIN-PUB-010/011 retain endpoint syntax; NET-PASS-003/011–018 own common password validation, explicit paste, correction, retention and erasure. Reject a whole invalid insertion with secret-free feedback and the previous value unchanged; do not trim, normalize, truncate or impose sticky invalid history. Existing valid input remains eligible under ordinary setup checks. Explicit paste targets only the focused Password field. Text entry uses supported keyboard input; controller navigation does not imply an on-screen keyboard. Dedicated identity verification remains mandatory even when Password is empty.

### Common Password presentation

- **UX-NET-03-051** Both service types must show a separate inset Password field after Port.
- **UX-NET-03-052** Direct setup and No password selection must show Password optional outside the field.
- **UX-NET-03-053** Protected selection must show Password required outside the field.
- **UX-NET-03-054** Password must render asterisks for complete entered characters with the standard focused-field insertion cue.
- **UX-NET-03-055** Password must not reveal the last typed character or offer a reveal action.
- **UX-NET-03-056** Focused masks must scroll horizontally to keep the insertion position visible.
- **UX-NET-03-057** An empty or cleared Password must show an empty inset field rather than a retained mask.
- **UX-NET-03-058** Password values must remain absent from endpoint, progress, failure, lobby, and diagnostic copy.
- **UX-NET-03-059** A rejected input event must leave the current mask and insertion state consistent with the unchanged value.
- **UX-NET-03-060** Input rejection must show secret-free feedback in the existing validation region while Password remains editable.
- **UX-NET-03-061** Empty protected-selection submission must remain on setup with Password focused and visible required feedback.
- **UX-NET-03-062** Password rejection correction must focus Password with permitted endpoint, binding, and local choices retained.
- **UX-NET-03-063** Protected dedicated selection must initially focus Password.
- **UX-NET-03-064** No password dedicated selection must initially focus local setup while Password remains visible, optional, and reachable in traversal.
- **UX-NET-03-065** Eligible correction must return to the same bound task without unlocking endpoint/session/type or changing local choices.

## Player-hosted and browser preservation

- Player-hosted setup must retain its masked Password and current optional/required help.
- Browser setup must retain its selected endpoint, session identity, directory origin, and contextual Back.
- A dedicated default or mode change must not rewrite browser-selected setup.
- Endpoint edits must remove obsolete listing claims under NET-DIR-014.
- First-round context must retain `Round 1 in progress. You will play immediately if admitted.`
- Listing context must retain `The host confirms availability when you connect.`
- Unlocked player-hosted endpoint-only direct setup must retain truthful first-contact identity guidance under NET-PASS-010.
- That limitation must not appear as a dedicated certificate waiver or a claim about password-protected admission.

## Allocation, feedback, and acceptance

- Dedicated guidance must stay above the setup panels below the endpoint block.
- Dedicated extra content must reduce visible list rows rather than cover fixed feedback or actions.
- Both list bodies must scroll independently under fixed headings.
- The last visible control, its focus outline, and its pointer bounds must stay inside the list allocation.
- Empty operator fields and optional Password must remain recognizable before focus.
- Validation must remain visible while invalid input leaves setup editable.
- Common Password rejection recovery must focus Password after Edit setup with permitted correction retained.
- Connecting must remove all editable targets and focus Cancel.
- Cancel must restore permitted endpoint, mode, credential, and local-player state without a false Disconnected or lobby claim.
- Complete admission must show only the current confirmed NET-04 or NET-05 destination.

Check 850 by 700 and 1280 by 720 containment, 1920 by 1080 pointer scaling, all fifteen selectable local slots, long valid endpoints, multibyte 128-byte password boundaries, case/spaces, atomic invalid-event feedback, explicit paste, context erasure, required/optional focus, both mode changes and directory-origin preservation. The scoped matrix adds no screen or Cartesian campaign. NET-03-P remains the editable dedicated variant; NET-03-E and locked connecting retain their identities and geometry.

## Bound dedicated selection

Functional authority: [network-join](../../screens/network-join.md) and [network-browser](../../screens/network-browser.md). NET-03-dedicated-selected binds decoded endpoint, hosting/protection, and nonzero session identity. The ready empty session exists before selection or admission; the service confirms controller or guest authority on actual admission. An idle-to-lobby or eligible first-round change with the same identity is not a replacement.

- **UX-NET-03-032** Selected dedicated setup must use NET-03-B inside the current canvas and setup hierarchy.
- **UX-NET-03-033** The context must identify `Dedicated service` and `Selected from browser` as read-only text without a cycle cue or mode target.
- **UX-NET-03-034** Address and Port must show the bound TLS frontend in readable flat disabled fields without editing, caret, focus stop, or pointer activation.
- **UX-NET-03-035** The selected session identity must remain separately labeled and fully accessible by wrapping within the fixed context region.
- **UX-NET-03-036** Selected context must describe advertised phase and occupancy without claiming verified reachability or reserving controller authority.
- **UX-NET-03-037** Supporting copy must say `The service confirms identity, availability, and your role when you connect.`
- **UX-NET-03-039** Bound setup must retain the current local-person/control task, count, validation, Connect, and Back.
- **UX-NET-03-040** Traversal must follow Password, local setup, Connect and Back and skip every bound read-only value. Initial focus follows protected/No password requirements UX-NET-03-063/064 without removing the optional Password stop.
- **UX-NET-03-041** Back must return to NET-10 without connecting or silently selecting another listing.
- **UX-NET-03-043** A confirmed endpoint/session replacement must require browser refresh and reselection rather than unlock fields or substitute a new target.
- **UX-NET-03-044** Explicit independent Direct connect must clear selection binding and secrets and retain its existing unbound setup; it must not carry selected credentials to a different target.
- **UX-NET-03-045** Connecting must retain the existing locked NET-03 structure and Cancel focus without revealing Password or claiming a role before admission.
- **UX-NET-03-046** Cancel and eligible correction must restore the same permitted bound endpoint/session/type and local setup under the functional contract.
- **UX-NET-03-047** An unsupported client must not enter selected dedicated setup or offer dedicated Password entry through any browser activation path, including No password rows.
- **UX-NET-03-048** Bound metadata and help must remain outside Password and above the independently scrolling setup bodies.
- **UX-NET-03-049** Context overflow must reduce visible body rows before it covers validation/actions or reduces text size.
- **UX-NET-03-050** The UI must not equate directory-owner authorization, listing identity, or the Dedicated label with verified X.509 identity.

NET-03-B's endpoint, session, hosting and protection are not editable. All existing geometry stays unchanged; the B diagram is the protected example with Password focus. No password uses the same visible optional field with initial local-setup focus, not a new diagram or task. NET-03-P remains direct and unknown-protection Password optional. Check both minimum viewports with required/optional empty and masked Password, long bound context, retained local choices and contextual Back/correction. No credential-binding dialog, reveal action or fallback is added.
