# NET-10 — Mixed session browser

Functional authority: [network-browser](../../screens/network-browser.md) and [host-directory](../../network-host-directory.md). States retain NET-10-loading, NET-10-results, NET-10-empty, NET-10-stale, and NET-10-unavailable, with NET-10-dedicated-idle, NET-10-dedicated-reserved, and NET-10-dedicated-unsupported added within the existing task. Structural source: [NET-10](../wireframes/NET-10/NET-10.svg).

Visual impact is limited to mixed classification, selected details, and eligibility in the existing browser. No new product screen is added.

## Current presentation and allocation

The [current unified presentation](../../design.md#unified-network-presentation) applies: native grey bevels, inset white bodies, blue headers, fixed grey headings, readable flat disabled controls, black non-color focus, and existing press and traversal semantics. No submenu, security theme, account, service configuration, sorting, or filtering is added.

- **UX-NET-10-001** BROWSE SESSIONS must remain above fixed directory feedback.
- **UX-NET-10-002** The table must retain a white inset body and fixed grey heading strip.
- **UX-NET-10-003** Row selection must retain the existing fill/text and textual marker with a separate focus outline.
- **UX-NET-10-004** The six columns must be `Session / endpoint`, `Hosting`, `Players`, `Phase`, `Access`, and `Admission` with complete headings at standard text size.
- **UX-NET-10-005** Selected details must remain below the table and above paging with room for full endpoint/identity, eligibility, and reachability guidance.
- **UX-NET-10-006** Previous page and Next page must retain visible boundaries beside the current page label.
- **UX-NET-10-007** The page label must not invent a total page count.
- **UX-NET-10-008** Join selected, Refresh, and Direct connect must retain the first footer row and Back the second row.
- **UX-NET-10-009** The table must lose visible rows before details, paging, or footer overlap.
- **UX-NET-10-010** Loading, empty, stale, and unavailable feedback must retain the existing allocation and footer placement.
- **UX-NET-10-011** Session / endpoint must retain the largest column allocation.
- **UX-NET-10-012** Long row endpoints must clip inside that column while selected details expose their complete validated value by wrapping or existing detail scrolling.
- **UX-NET-10-013** Full selected session identity and advertised game mode must remain separately labeled and accessible without reducing text size or splitting UTF-8 characters.

Approximate column widths are 256, 112, 72, 104, 96, and 144 logical pixels. Preserve standard metrics, current clearance, scrollbar allocation, fixed-height rows, and full-row pointer targets. Mode and identity text must not cross table columns; selected details own their full values.

Entry workflow: MENU-01 → Network → Join in the NET-01 popup → mixed NET-10. Join selected continues to common Password/local setup under NET-PASS-001–018 with hosting/security mode retained. Direct connect remains independent. Back returns to NET-01 with Host focused. Layout, controls and eligibility stay fixed to07b2402; only approved Access text and protection-aware focus annotations change.

## Mixed row content

- **UX-NET-10-014** Hosting must show `Player-hosted` or `Dedicated` from validated decoded classification rather than infer it from an address or credential flag.
- **UX-NET-10-015** Access must show `No password` or `Password` from validated `none`/`password` metadata consistent with `passwordRequired` for either hosting mode.
- **UX-NET-10-016** Access must not disclose any password, owner token or reconnect credential.
- **UX-NET-10-017** Hosting and Access must not imply verified operator identity, certificate validation, or universal reachability.
- **UX-NET-10-018** Players must show the authoritative advertised count and effective capacity as `count / limit`, including reserved slots under the product contract.
- **UX-NET-10-019** A published ready empty dedicated session must show `Idle` and `0 / limit` without inventing participants, a controller, a round, or a different identity.
- **UX-NET-10-020** Dedicated idle details must say `Ready empty session. Your role is confirmed only after admission.`
- **UX-NET-10-021** Controller reservation must show `Reserved` phase and truthful occupancy while Admission follows the separately advertised eligibility.
- **UX-NET-10-022** Phase must retain `Lobby`, `Round 1`, or the approved closed-phase presentation for active sessions.
- **UX-NET-10-023** Admission must show `Open`, `Full`, `Closed`, `Busy`, or `Unconfirmed` from the approved availability state without treating spare capacity as proof of availability.
- **UX-NET-10-024** Cleanup/busy details must explain the established temporary block without offering controller election, publication retry, or service administration.
- **UX-NET-10-025** Unknown or unvalidated hosting/protection must not default to Player-hosted, No password, or Open or open a credential task.

If invalid metadata is retained for inspection under the decoder contract, show Unknown/Unconfirmed and disable every joining path. If the page is rejected, use existing unavailable/stale feedback. This does not alter product's fail-closed or legacy-consumer policy. Directory-owner authorization is not verified X.509 game endpoint identity.

Retired Invite protection must never be relabeled Password or No password. Safe exclusion or fail-closed unavailable feedback follows the atomic unreleased-v2 contract; hosting remains explicit. No v3 adapter, new control or geometry change is introduced.

## Selection, unsupported clients, and recovery

- **UX-NET-10-026** Join selected must open the applicable bound NET-03 task without immediately connecting or reserving controller authority.
- **UX-NET-10-027** Selected dedicated rows must enter NET-03-dedicated-selected with locked hosting/protection, endpoint and session identity. Protected selection initially focuses Password under UX-NET-03-063; No password selection initially focuses local setup under UX-NET-03-064 while Password remains optional and reachable.
- **UX-NET-10-028** Selected player-hosted rows must retain NET-03-E and their existing Password/local-setup focus behavior.
- **UX-NET-10-029** Updated Mac/Pi clients must keep dedicated rows visible and readable without enabling Join selected, row Confirm, or dedicated credential entry.
- **UX-NET-10-030** Unsupported selected details must say `Dedicated joining requires Linux x86-64 or Windows x86-64.` and show a flat disabled Join action.
- **UX-NET-10-031** Stale or expired selection must require successful refresh before joining and must not transfer its binding to Direct connect.
- **UX-NET-10-032** Refresh must retain selection by the approved immutable listing/session identity rather than row position.
- **UX-NET-10-033** Idle-to-lobby or eligible first-round changes with the same session identity must update details without claiming replacement or controller assignment.
- **UX-NET-10-034** A disappeared or replaced selection must not select or join a replacement automatically and must show the product's refresh/reselection outcome.
- **UX-NET-10-035** Back from selected dedicated setup or failure must restore the browser without changing to another row; failure recovery must permit refresh.
- **UX-NET-10-036** Independent Direct connect must clear selection binding and secrets before entering the existing unbound task.

Full, closed, stale, busy, and unsupported listings remain inspectable. The primary disabled reason is stale Refresh first, then fresh unsupported-client feedback, otherwise busy/full/closed. Details may explain multiple conditions. Unknown or stale metadata must not claim Open. A credential requirement alone must not disable otherwise eligible joining.

## Input and feedback acceptance

Preserve existing allocation, actions, and scrolling. Each full row is one pointer selection target; selection does not connect. Initial focus enters results when available, Refresh for empty results when enabled, and Direct connect during a pending initial read. Preserve Tab/reverse traversal, controller navigation, page actions, and disabled nonactivation. Join selected, row Confirm, and pointer activation must use the same eligibility rules.

Keep current text scale, inverse pointer mapping, and non-color cues for loading, stale, unavailable, no-selection, and removed-selection feedback. Selected details must say `A listing does not guarantee reachability.` No public-address prohibition or NAT-traversal claim is added.

Check mixed player-hosted Password, dedicated Password and dedicated No password listings; ready idle, current, reserved, freshness, busy, full, closed and unsupported states; long endpoints/identities; paging, refresh retention, replacement and independent Direct. Protected/optional focus and unsupported No password nonactivation remain required. The scoped matrix changes only existing row inputs/expectations. Stills do not prove certificate validation, common PAKE, protocol binding, leases, admission or lifecycle authority.
