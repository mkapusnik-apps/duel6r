# NET-10 — Host browser

## Purpose and authority

This screen lets the user find active registered sessions and select one for admission. [NET-DIR-001 through NET-DIR-016](../network-host-directory.md) own its functional behavior. It grants no host or participant authority.

## Entry, actions, and transitions

- Entry from `NET-01` → Join must request current directory results under NET-ENTRY-003 and NET-DIR-009.
- The existing Direct connect action must open independent direct setup in `NET-03`, including when the directory is unavailable under NET-DIR-015.
- Refresh must request current results without starting a game connection.
- Selecting an eligible listing must enter `NET-03` with its endpoint and advertised session identity.
- Locked listings must lead to password entry in `NET-03`.
- Match phase and admission are independent: later ongoing sessions may show Started/Open and offer joining. Completed sessions remain genuinely Closed. Later-match selected details explain next-round participation; they must not promise immediate spawn.
- Full, admission-closed, and age-stale listings must remain readable without an enabled join action. Failed reads instead hide cached rows and selected identity, endpoint, and mode details, disable every browser join path, and preserve independent Direct connect.
- Back must return to `NET-01` without starting or ending a hosted session.
- Failed browser-origin admission must permit return here for refresh.

## Functional states

| State ID | Meaning and outcome |
| --- | --- |
| `NET-10-loading` | A bounded read is pending; no admission has occurred. |
| `NET-10-results` | Active listings are available, including locked, full, and admission-closed sessions. |
| `NET-10-empty` | A successful current read returned no active listings. |
| `NET-10-stale` | Age-stale retained results remain readable but are not current; refresh is required before joining. Failed-refresh cached results remain hidden. |
| `NET-10-unavailable` | The directory request failed or timed out; retry and Back remain available. |

The screen must disclose that listings do not guarantee reachability. It must not expose secrets or imply that remote private/loopback endpoints identify a reachable host on the user's network.

Successful results have no `Sessions listed` status. Empty and failed reads use distinct grey, noninteractive single-line descriptions inside the list allocation instead of rows. Pagination buttons and current-page indicator are hidden together when neither page direction exists; necessary multi-page navigation remains. Failed-read recovery restores the retained selection identity only if present, never silently choosing another session. Loading behavior, age-stale readability, and the 10/20/30-second bounds remain unchanged.

## Functional acceptance

NET-DIR-AC-001 through NET-DIR-AC-004 apply. Evidence must cover full and closed listings remaining visible, locked selection, all freshness states, bounded browsing, and recovery without blocking direct Join.

UX owns layout, navigation presentation, feedback, focus, accessibility, and responsive behavior. This contract does not select controls or geometry.
