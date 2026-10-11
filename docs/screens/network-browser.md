# NET-10 — Host browser

## Purpose and authority

This screen lets the user find active registered player-hosted and ready dedicated sessions, including a prepared zero-occupancy dedicated session, and select one for admission. [NET-DIR-001 through NET-DIR-033](../network-host-directory.md) own its functional behavior. Listing, selection and provisional offers grant no controller, host, participant or operator authority.

## Entry, actions, and transitions

- Entry from `NET-01` → Join must request current directory results under NET-ENTRY-003 and NET-DIR-009.
- The existing Direct connect action must open independent direct setup in `NET-03`, including when the directory is unavailable under NET-DIR-015.
- Refresh must request current results without starting a game connection.
- Selecting an eligible listing must enter `NET-03` with its bound hosting/security mode, endpoint and expected advertised nonzero session identity. Mode-aware missing, unknown or inconsistent metadata must reject without connection or fallback.
- Protected player-hosted listings must lead to Password entry; dedicated listings must lead to Invite entry with their derived mode locked in `NET-03-dedicated-selected`.
- Full, admission-closed, stale and unsupported listings must remain readable without an enabled join action. Updated Mac/Pi clients must show dedicated rows as inspectable with Join disabled and a textual unsupported reason, not an Invite or player-hosted fallback.
- Back must return to `NET-01` without starting or ending a hosted session.
- Failed browser-origin admission must permit return here for refresh.

## Functional states

| State ID | Meaning and outcome |
| --- | --- |
| `NET-10-loading` | A bounded read is pending; no admission has occurred. |
| `NET-10-results` | Active listings are available, including locked, full, and admission-closed sessions. |
| `NET-10-empty` | A successful current read returned no active listings. |
| `NET-10-stale` | Retained results are not current; refresh is required before joining. |
| `NET-10-unavailable` | The directory request failed or timed out; retry and Back remain available. |
| `NET-10-dedicated-idle` | A ready dedicated session has a prepared nonzero identity, zero participants/players, no controller/round and effective capacity. Selection reserves no slot or controller role; first committed admission assigns the controller. The empty session cannot start a match. |
| `NET-10-dedicated-reserved` | The current dedicated session retains its identity and authoritative reserved players/occupancy during the fixed controller reconnect reservation. No migration or fresh-controller promise is made; current phase/capacity and actual admission authority determine eligibility. |
| `NET-10-dedicated-unsupported` | A dedicated row is visible and inspectable on an updated client without dedicated participation support. Join is disabled with a textual reason and no invitation/fallback path; ordinary player-hosted listings remain usable. |

The screen must disclose that listings do not guarantee reachability. It must not expose secrets or imply that remote private/loopback endpoints identify a reachable host on the user's network.

Mixed rows must distinguish hosting/security mode, Password versus Invite protection, admission phase including dedicated idle, and authoritative occupancy/effective capacity including reserved players. Directory owner authorization is not gameplay endpoint identity. The dedicated endpoint is only the operator TLS frontend, never the private backend/wildcard/directory origin. Disclosure requires explicit mode-aware consumer capability; legacy consumers receive player-hosted-only rows or fail closed, not an ignored appended-field shortcut. No password, invitation, proof, owner token or reconnect credential appears in a listing.

The selected identity remains bound if idle changes to lobby or first-round while setup is open; the actual service determines role and destination. End/expiry/restart makes old selections invalid immediately before directory removal, even if old/new rows coexist after lost removal. Rejection precedes authority allocation and never redirects to the fresh session. A changed endpoint or selection requires refresh; explicit unbound Direct transition clears selected mode/identity binding and secrets. Same-session Return preserves identity. Directory failure must not stop current gameplay, direct joining or Local Play.

## Functional acceptance

NET-DIR-AC-001 through NET-DIR-AC-005 and NET-DIR-AC-007 through NET-DIR-AC-009 apply. Evidence must cover mixed Password/Invite rows, ready dedicated idle/concurrent first commits, current phase/capacity/reservations, same-session Return/fresh reuse, unknown/legacy/unsupported fail-closed behavior, stale/wrong/ended/restarted selections, full/closed/stale/unsupported disabled joining, bounded browsing and failure independence. NET-DIR-AC-006 retains its player-hosted listening-coverage scope. These are approved functional targets, not new implementation or conformance claims.

UX owns layout, navigation presentation, feedback, focus, accessibility, and responsive behavior in [NET-10](../design/screens/NET-10.md). This contract does not select controls or geometry.
