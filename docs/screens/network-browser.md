# NET-10 — Host browser

## Purpose and authority

This screen lets the user find active registered sessions and select one for admission. [NET-DIR-001 through NET-DIR-016](../network-host-directory.md) own its functional behavior. It grants no host or participant authority.

## Entry, actions, and transitions

- Entry from `NET-01` must request current directory results.
- Refresh must request current results without starting a game connection.
- Selecting an eligible listing must enter `NET-03` with its endpoint and advertised session identity.
- Locked listings must lead to password entry in `NET-03`.
- Full, admission-closed, and stale listings must remain readable without an enabled join action.
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

The screen must disclose that listings do not guarantee reachability. It must not expose secrets or imply that remote private/loopback endpoints identify a reachable host on the user's network.

## Functional acceptance

NET-DIR-AC-001 through NET-DIR-AC-004 apply. Evidence must cover full and closed listings remaining visible, locked selection, all freshness states, bounded browsing, and recovery without blocking direct Join.

UX owns layout, navigation presentation, feedback, focus, accessibility, and responsive behavior. This contract does not select controls or geometry.
