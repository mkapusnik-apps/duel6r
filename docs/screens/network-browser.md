# NET-10 — Host browser

## Purpose and authority

This screen lets the user find active registered player-hosted and ready dedicated sessions, including a prepared zero-occupancy dedicated session, and select one for admission. [NET-DIR-001 through NET-DIR-033](../network-host-directory.md) own its functional behavior. Listing, selection and provisional offers grant no controller, host, participant or operator authority.

## Entry, actions, and transitions

- Entry from `NET-01` → Join must request current directory results under NET-ENTRY-003 and NET-DIR-009.
- The existing Direct connect action must open independent direct setup in `NET-03`, including when the directory is unavailable under NET-DIR-015.
- Refresh must request current results without starting a game connection.
- Match phase and admission are independent: later ongoing sessions may show Started/Open and offer joining. Completed sessions remain genuinely Closed. Later-match selected details explain next-round participation rather than promise immediate spawn. Known later context is `Match in progress. Your players join next round if admitted.` Known final context is `Final round in progress. You may not play in this match if admitted.` Unknown context makes no immediate-play promise.
- Selecting an eligible listing must enter `NET-03` with its bound hosting/security mode, endpoint and expected advertised nonzero session identity. Mode-aware missing, unknown or inconsistent metadata must reject without connection or fallback.
- Protected listings in either hosting mode must lead to common Password entry with initial Password focus. No password selection must initially focus local setup with an optional, reachable Password field. Dedicated setup retains its derived mode/protection locked in `NET-03-dedicated-selected` under NET-PASS-003/011–013.
- Visible successful results must retain full, admission-closed and unsupported rows as inspectable, with Join disabled and the applicable reason.
- Age-stale successful results must remain inspectable as unconfirmed, with Join disabled.
- A pending refresh must not permit Join.
- A failed read must replace the visible result area with the unavailable state and hide cached rows, selected details, caret and row actions.
- A successful empty read must show the empty state without cached rows, selected details, caret or row actions.
- The browser may retain the selected listing/session identity internally while its rows are hidden. A successful recovery may restore that selection only for the same valid identity and binding. If it is absent or changed, no replacement row is selected automatically; the user must explicitly select another row.
- The page group must be hidden when neither previous nor next navigation is available. Valid pagination must remain available where a direction exists, including successful empty pages with a continuation.
- Pointer, wheel, keyboard and controller input must exclude hidden rows and hidden paging targets.
- Direct connect, Back, and applicable recovery actions remain available under their existing contracts. Read failure must not end an active session or change admission authority.
- Updated Mac/Pi clients must show dedicated rows as inspectable with Join disabled and a textual unsupported reason, not dedicated Password entry or player-hosted fallback, including No password rows.
- Back must return to `NET-01` without starting or ending a hosted session.
- Failed browser-origin admission must permit return here for refresh.

## Functional states

| State ID | Meaning and outcome |
| --- | --- |
| `NET-10-loading` | A bounded read or refresh is pending. Join is disabled. Retained successful results may remain inspectable as unconfirmed, but cache hidden by an earlier failure stays hidden until success. |
| `NET-10-results` | A successful current read supplies visible mixed listings and applicable details. There is no generic success-status claim required. Eligibility still follows freshness, capacity, phase, platform and actual admission checks. |
| `NET-10-empty` | A successful read returns no rows for the current page. No cached rows, details, caret or row targets appear. Valid previous/next navigation may remain available. |
| `NET-10-stale` | Previously successful rows have exceeded the freshness bound without a failed read replacing them. They remain inspectable as unconfirmed; Join requires successful refresh. |
| `NET-10-unavailable` | The latest read failed or timed out. Cached rows and selected details are hidden. Internal identity retention grants no visible selection, Join eligibility or authority. Applicable recovery actions remain available. |
| `NET-10-dedicated-idle` | A ready dedicated session has a prepared nonzero identity, zero participants/players, no controller/round and effective capacity. Selection reserves no slot or controller role; first committed admission assigns the controller. The empty session cannot start a match. |
| `NET-10-dedicated-reserved` | The current dedicated session retains its identity and authoritative reserved players/occupancy during the fixed controller reconnect reservation. No migration or fresh-controller promise is made; current phase/capacity and actual admission authority determine eligibility. |
| `NET-10-dedicated-unsupported` | A dedicated row is visible and inspectable on an updated client without dedicated participation support. Join is disabled with a textual reason and no dedicated Password/fallback path; ordinary player-hosted listings remain usable. |

The screen must disclose that listings do not guarantee reachability. It must not expose secrets or imply that remote private/loopback endpoints identify a reachable host on the user's network.

Mixed rows must distinguish hosting/security mode, Password versus No password protection consistent with `passwordRequired` in either mode, admission phase including dedicated idle and authoritative occupancy/effective capacity including reserved players. Directory owner authorization is not gameplay endpoint identity. The dedicated endpoint is only the operator TLS frontend, never the private backend/wildcard/directory origin. Disclosure requires explicit mode-aware support; legacy v1 receives player-hosted-only rows or fails closed. The atomic unreleased-v2 update safely excludes or rejects retired Invite/unknown protection, never relabels it as password, and uses existing 60-second lease expiry without a running database migration or v3 adapter. No password, reusable proof, owner token or reconnect credential appears in a listing.

The selected identity remains bound if idle changes to lobby or first-round while setup is open; the actual service determines role and destination. End/expiry/restart makes old selections invalid immediately before directory removal, even if old/new rows coexist after lost removal. Rejection precedes authority allocation and never redirects to the fresh session. A changed endpoint or selection requires refresh; explicit unbound Direct transition clears selected mode/identity binding and secrets. Same-session Return preserves identity. Directory failure must not stop current gameplay, direct joining or Local Play.

## Functional acceptance

NET-DIR-AC-001 through NET-DIR-AC-005 and NET-DIR-AC-007 through NET-DIR-AC-009 apply with NET-PASS-AC-001–005. Evidence must cover Password/No password rows in both modes, protection-aware focus, explicit non-empty input on No password, dedicated certificate verification, ready idle/concurrent first commits, phase/capacity/reservations, same-session Return/fresh reuse, retired/unknown/legacy/unsupported fail-closed behavior, stale/wrong/ended/restarted selections, disabled joining, bounded browsing and failure independence. NET-DIR-AC-006 retains its player-hosted listening-coverage scope. These are approved targets, not implementation or conformance claims.

UX owns layout, navigation presentation, feedback, focus, accessibility, and responsive behavior in [NET-10](../design/screens/NET-10.md). This contract does not select controls or geometry.

UX owns latest retained-six-heading white-body inline empty/unavailable states, upper Refresh, single-row footer and traversal under the composed NET-10 requirements, preserving identifier meanings. The generic `Sessions listed` success line is removed; successful current unselected/empty results leave the reason blank without a replacement instruction. The six-column mixed layout, endpoint allocation, hosting/Password/No password distinctions and both-mode binding remain authoritative; older private-only projections do not replace them.
