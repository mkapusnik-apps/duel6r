# NET-03 — Direct setup and bound browser selection

Functional authority: [NET-03](../../screens/network-join.md), NET-DIR-013/014/027/032, common NET-PASS-001–018, NET-ADM-011, NET-PUB-017–021 and retained NET-JOIN-PUB-001/006/008–011. Product owns the common encrypted PAKE connection, optional password, supported clients, hosting lifecycle, and admission outcomes. Invitation-only functional and UX identifiers are retired, never reused. Structural sources are [NET-03-E selected player-hosted](../wireframes/NET-03/NET-03-E.svg), [NET-03-P common Direct](../wireframes/NET-03/NET-03-P.svg), [NET-03-B selected dedicated](../wireframes/NET-03/NET-03-B.svg), and [NET-03 connecting](../wireframes/NET-03/NET-03.svg). Only P changes geometry; E, B, and connecting change affected text or annotations only.

States: NET-03-password, NET-03-live-admission, NET-03-PUBLIC-EDIT, NET-03-PUBLIC-CONNECTING, and NET-03-dedicated-selected retain their IDs. P represents unbound Direct setup for either hosting lifecycle, not a selectable dedicated transport. Visual impact is localized: remove the direct service-type control and platform exclusions; correct security and role copy. No deployment, certificate setup, fingerprint, account, or nested connection screen is added. The common journey applies to Linux x86-64, Windows x86-64, Linux AArch64/Pi 5, and experimental Apple Silicon macOS 14+. This does not expand hosting support or add Intel macOS.

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

## Common Direct setup

- **UX-NET-03-010** Direct setup must omit Service type and transport selection on every approved client.
- **UX-NET-03-011** Direct setup must show Address, Port, Password, local-player setup, Connect, and Back in that reading order.
- **UX-NET-03-016** Browser-origin setup must retain selected-session context and decoded hosting type without an editable service-type row.
- **UX-NET-03-017** Direct setup must use the same structure and traversal on all four approved client targets.

UX-NET-03-012–015 are retired selector-only identifiers and must never be reused. The removed row and cycle cue must leave no keyboard, controller, pointer, or wheel target. Initial Direct focus must remain Address. Forward traversal must follow Address → Port → Password → local setup → Connect → Back; reverse traversal must reverse the cycle, including Address → Back. Preserve existing disabled-action traversal behavior. Browser entry in either hosting mode retains initial Password focus when protected and local-setup focus when unprotected. Browser traversal follows Password → local setup → Connect → Back, skipping all bound read-only values in both directions. Tab/Shift+Tab and supported controller directions navigate; Enter/Space/controller Confirm and pointer activation retain existing semantics. Text and explicit paste use supported keyboard input, without an added on-screen keyboard.

P must reuse the ordinary direct endpoint/local-setup allocation. The former selector region must show noninteractive `Direct connection` context, not a blank framed control. Direct setup must not reserve the smaller dedicated-only list allocation for an unknown endpoint. The two equal-height lists must use the ordinary player-hosted setup space. Their headings must stay fixed while each body scrolls. Fixed feedback and Connect/Back must not move into a scrolling body.

## Operator endpoint and common Password

- **UX-NET-03-018** First Direct setup must use the existing ordinary Direct field initialization without a dedicated-only empty-field variant.
- **UX-NET-03-019** Direct setup must not prefill a deployed gameplay hostname, staging endpoint, or production endpoint.
- **UX-NET-03-020** Later Direct setup must show only the one retained direct endpoint context permitted by the functional contract.
- **UX-NET-03-028** Known dedicated context must describe role confirmation without assigning controller authority before admission.
- **UX-NET-03-029** Unknown Direct context must not show dedicated-only controller or host-departure guidance.
- **UX-NET-03-030** Setup must not claim authenticated host identity or a confirmed participant role from an endpoint, listing, hosting label, or encrypted channel alone.
- **UX-NET-03-031** The screen must not offer certificate bypass, plaintext fallback, or another endpoint as automatic recovery.

UX-NET-03-021–027/038/042 are retired invitation-only identifiers and must never be reused. NET-JOIN-PUB-010/011 retain endpoint syntax; NET-PASS-003/011–018 own common password validation, explicit paste, correction, retention and erasure. Reject a whole invalid insertion with secret-free feedback and the previous value unchanged; do not trim, normalize, truncate or impose sticky invalid history. Existing valid input remains eligible under ordinary setup checks. Explicit paste targets only the focused Password field. Product's NET-PASS-008–010 owns security guarantees for the common channel; there is no dedicated certificate prerequisite or user-selected crypto mode.

Direct must keep hosting-kind and role presentation neutral while admission is pending. The destination may show hosting kind and controller or Guest only after the product-confirmed admission result. This result must not insert a new selection or confirmation step. Browser setup may show its validated selected hosting kind before admission, but that label must remain read-only and must not grant a role.

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
- **UX-NET-03-063** Protected browser selection in either hosting mode must initially focus Password.
- **UX-NET-03-064** No password browser selection in either hosting mode must initially focus local setup while Password remains visible, optional, and reachable in traversal.
- **UX-NET-03-065** Eligible correction must return to the same bound task without unlocking endpoint/session/type or changing local choices.

## Player-hosted and browser preservation

- Player-hosted setup must retain its masked Password and current optional/required help.
- Browser setup must retain its selected endpoint, session identity, directory origin, and contextual Back.
- Entering Direct must not rewrite browser-selected setup in place.
- Browser-selected targets are read-only in both hosting modes. Explicit Back or Return to browser followed by independent Direct connect is the only target-unbinding route and clears Password under NET-PASS-013; no endpoint edit may silently switch a bound task to Direct.
- Immediate-play context must be limited to product-confirmed first-round eligibility; later or unknown context must not reuse its immediate-play promise.
- Listing context must retain `The host confirms availability when you connect.`
- Empty-password first contact must retain `Endpoint-only first contact does not authenticate host identity.` in the existing supporting-feedback region for either hosting lifecycle, including selected No password context.
- Security guidance must not describe encryption without a password as protection from an active intermediary.
- Security guidance must not claim personal identity from a shared password.
- The UI must not add a mandatory trust dialog, certificate control, fingerprint comparison, account, or warning confirmation.

## Allocation, feedback, and acceptance

- Known selected dedicated guidance must stay above the setup panels below the endpoint block.
- Selected context must reduce visible list rows rather than cover fixed feedback or actions.
- Both list bodies must scroll independently under fixed headings.
- The last visible control, its focus outline, and its pointer bounds must stay inside the list allocation.
- Empty operator fields and optional Password must remain recognizable before focus.
- Validation must remain visible while invalid input leaves setup editable.
- Common Password rejection recovery must focus Password after Edit setup with permitted correction retained.
- Connecting must remove all editable targets and focus Cancel.
- Cancel must restore permitted endpoint, selected binding, credential, and local-player state without a false Disconnected or lobby claim.
- Complete admission must show only the current confirmed NET-04 or NET-05 destination.

Check 850 by 700 and 1280 by 720 containment, 1920 by 1080 pointer scaling, all fifteen selectable local slots, long valid endpoints, multibyte 128-byte password boundaries, case/spaces, atomic invalid-event feedback, explicit paste, context erasure, required/optional focus, selector-target removal, and directory-origin preservation. Direct readiness must use neutral `Ready to connect.` rather than promise identity verification or say that Join starts a player-hosted session. Validation and input-rejection feedback must keep priority in the existing feedback region; supporting identity guidance must remain readable without covering that region. The [coordinated matrix](../screenshots/README.md#unified-connection-current-capture-matrix) adds no screen or Cartesian campaign.

## Bound dedicated selection

Functional authority: [network-join](../../screens/network-join.md) and [network-browser](../../screens/network-browser.md). NET-03-dedicated-selected binds decoded endpoint, hosting/protection, and nonzero session identity. The ready empty session exists before selection or admission; the service confirms controller or guest authority on actual admission. An idle-to-lobby or eligible first-round change with the same identity is not a replacement.

- **UX-NET-03-032** Selected dedicated setup must use NET-03-B inside the current canvas and setup hierarchy.
- **UX-NET-03-033** The context must identify `Dedicated service` and `Selected from browser` as read-only text without a cycle cue or mode target.
- **UX-NET-03-034** Address and Port must show the bound common-channel gameplay endpoint in readable flat disabled fields without editing, caret, focus stop, or pointer activation.
- **UX-NET-03-035** The selected session identity must remain separately labeled and fully accessible by wrapping within the fixed context region.
- **UX-NET-03-036** Selected context must describe advertised phase and occupancy without claiming verified reachability or reserving controller authority.
- **UX-NET-03-037** Supporting copy must say `The service confirms availability and your role when you connect.`
- **UX-NET-03-039** Bound setup must retain the current local-person/control task, count, validation, Connect, and Back.
- **UX-NET-03-040** Traversal must follow Password, local setup, Connect and Back and skip every bound read-only value. Initial focus follows protected/No password requirements UX-NET-03-063/064 without removing the optional Password stop.
- **UX-NET-03-041** Back must return to NET-10 without connecting or silently selecting another listing.
- **UX-NET-03-043** A confirmed endpoint/session replacement must require browser refresh and reselection rather than unlock fields or substitute a new target.
- **UX-NET-03-044** Explicit independent Direct connect must clear selection binding and secrets and retain its existing unbound setup; it must not carry selected credentials to a different target.
- **UX-NET-03-045** Connecting must retain the existing locked NET-03 structure and Cancel focus without revealing Password or claiming a role before admission.
- **UX-NET-03-046** Cancel and eligible correction must restore the same permitted bound endpoint/session/type and local setup under the functional contract.
- **UX-NET-03-047** All four approved clients must permit eligible selected dedicated setup, including Password and No password rows, without a platform-warning step.
- **UX-NET-03-048** Bound metadata and help must remain outside Password and above the independently scrolling setup bodies.
- **UX-NET-03-049** Context overflow must reduce visible body rows before it covers validation/actions or reduces text size.
- **UX-NET-03-050** The UI must not equate directory-owner authorization, listing/session identity, or the Dedicated label with authenticated host identity.

NET-03-B's endpoint, session, hosting and protection are not editable. Its geometry stays unchanged; the B diagram is the protected example with Password focus. No password uses the same visible optional field with initial local-setup focus and the common first-contact guidance, not a new diagram or task. NET-03-P is common Direct with unknown-protection Password optional. Check both minimum viewports with required/optional empty and masked Password, long bound context, retained local choices and contextual Back/correction. No credential-binding dialog, reveal action or fallback is added.

## Bound selection in both hosting modes

- **UX-NET-03-066** Browser-selected hosting, protection, and session identity must remain read-only in both hosting modes.
- **UX-NET-03-067** Browser-selected Address and Port must use readable flat disabled fields without editing, caret, focus stop, or pointer activation in both hosting modes.
- **UX-NET-03-068** Selected player-hosted context must show `Player-hosted session` and `Selected from browser` without a service-type cycle target.
- **UX-NET-03-069** Selected player-hosted session identity must remain separately labeled and readable in the existing context allocation.
- **UX-NET-03-070** Forward and reverse traversal must skip every bound read-only value while keeping Password reachable in both hosting modes.
- **UX-NET-03-071** Cancel, Edit setup, and eligible Retry must retain the selected presentation without exposing target-editing controls.
- **UX-NET-03-072** Player-hosted context must not use dedicated controller-assignment copy or certificate-verification copy.

NET-03-E remains the protected selected player-hosted representative: locked Address/Port, labeled hosting/origin/session context, masked Password with initial focus, and the same local setup allocation. No password selection uses the same allocation with optional Password and initial local focus. Only independent Direct setup exposes endpoint editing and initial Address focus. E changes only the obsolete Direct-selector annotation; its rectangles, keylines, coordinates, and fixed actions remain unchanged. No new screen, state, control or failure diagram is added.

## Ongoing admission context

Product-owned NET-ADM-001–016 determine eligibility and confirmed participation. These presentation requirements do not unlock browser selection, change common Password, or select an admission/spawn policy.

- **UX-NET-03-073** Known later-match context must show `Match in progress. Your players join next round if admitted.`; known final-round context must show `Final round in progress. You may not play in this match if admitted.`; unknown/direct context must not promise immediate participation.
- **UX-NET-03-074** Admission context must wrap at word boundaries within its existing allocation before it covers validation, Password, local setup, or Connect/Back.
- **UX-NET-03-075** Complete authoritative admission must show the confirmed lobby, immediate-play arena, or Connected-waiting arena without a false intermediate lobby or failure.

Keep `The host confirms availability when you connect.`, masked Password, protection-aware focus, selected identity, and contextual Back. Dedicated context retains availability/role guidance without an identity-check claim; player-hosted context must not inherit controller guidance. NET-03-E is the protected ongoing player-hosted representative. Locked connecting retains its structure and must report success only after complete admission.

Imported develop ongoing references UX-NET-03-010/011/012 map to UX-NET-03-073/074/075, respectively. Canonical UX-NET-03-010/011 own common Direct presentation; canonical UX-NET-03-012 is retired. No identifier has two definitions.
