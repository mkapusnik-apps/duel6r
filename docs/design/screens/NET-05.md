# NET-05 — Match controls and contextual panels

Functional authority: [NET-05](../../screens/network-match.md), existing NET-VIS and NRP requirements, `NET-ADM-001`–`NET-ADM-012`, `NET-PUB-008`, and `HSL-PUB-005`–`HSL-PUB-010`. Pilot state: `NET-05-PUBLIC`. Structural sources: existing [NET-05](../../screens/wireframes/network-match.md), [NET-05-C](../wireframes/NET-05/NET-05-C.svg), develop's [NET-05-S](../wireframes/NET-05/NET-05-S.svg), and develop's [NET-05-R](../wireframes/NET-05/NET-05-R.svg).

Visual impact: use develop's framed controls and contextual panels. The pilot adds status and consequence copy only. Do not redesign the arena, camera, world assets, gameplay HUD, or compact translucent network status.

## Presentation and allocation

- **UX-NET-05-001** Leave session, End session, and eligible Advance round must retain persistent framed controls in their existing bounds.
- **UX-NET-05-002** Panels must not add gameplay-focus targets outside their existing context.
- **UX-NET-05-003** NET-05-C must retain its centered 640-client-pixel maximum width, 16-client-pixel edge clearance, and two separate actions.
- **UX-NET-05-004** The confirmation title must precede wrapped consequence copy and the fixed explicit-action/Cancel row.
- **UX-NET-05-005** Confirmation must retain explicit-action-first focus and opening-input protection.
- **UX-NET-05-006** NET-05-S must retain its arena placement without a menu banner or backdrop.
- **UX-NET-05-007** NET-05-S must keep authoritative-score title, session scope, current result-state labels, and body separate.
- **UX-NET-05-008** NET-05-S must not advertise unsupported scrolling or invent completed-result rows.
- **UX-NET-05-009** NET-05-R must retain its client-relative placement below the top edge and above status and session actions.
- **UX-NET-05-010** NET-05-R must retain round progress above its SCORE heading and ranking body.
- **UX-NET-05-011** Outcome and phase/countdown text must remain separate below ranking.
- **UX-NET-05-012** NET-05-R ranking must retain gameplay state colors on the gameplay ranking surface rather than a white list.
- **UX-NET-05-013** First-second active and later frozen phases must retain one structural variant without timing or eligibility changes.
- **UX-NET-05-014** Long values must remain contained without smaller text or covered footers.
- **UX-NET-05-015** Long outcomes must wrap above phase/countdown text.
- **UX-NET-05-016** Extra outcome lines must reduce visible ranking height while existing scrolling keeps every row reachable.

NET-05-S position information is plain non-actionable text. Do not add enabled arrows, scroll handlers, focus stops, or shortcuts. Tab retains its existing discrete toggle. Result navigation in NET-04-R and NET-06 and ranking navigation in NET-05-R remain unchanged. Confirmation over lobby, summary, or reconnect uses NET-05-C with that context's product-owned consequence.

## First-round arrival

- Confirmed player-hosted first-round admission must enter the current arena directly.
- Arrival must not restart introduction for existing players.
- Arrival must not show spectator or next-round-waiting copy for a participant admitted to play immediately.
- Existing players, world objects, progress, and Predator presentation must remain continuous.
- Directory refresh must not appear over play or take gameplay focus.
- Arrival must not add a viewport, modal, or action.

Dedicated round-one admission uses the same confirmed live-arena destination under NET-ADM-011. Do not restore the old blanket join-in-progress rejection or add a pilot waiting lobby.

## Dedicated pilot status

- The pilot status region must use `Public session` instead of legacy LAN-only scope copy.
- Role must remain the confirmed Host or Guest.
- Status must retain its wrapping, three-row limit, and no-focus behavior.
- Host End and guest Leave must retain distinct consequences and NET-05-C structure.
- Explicit controller departure must not imply termination of the remote service process.
- Retained public context in NET-07 and NET-09 must keep the same identity and orientation.

## Acceptance

Use an ordinary real Connected match for the live representative. Capture actual pre-winner data for NET-05-S and an actual non-final frozen outcome for NET-05-R. A completed summary cannot substitute for the non-final result. Check all confirmation contexts, both role actions, degraded/resynchronizing status, long outcomes, maximum ranking, all modes, and first-round arrival. Public representatives do not establish Invisibility, orientation, extreme-result, or gameplay-continuity regression acceptance. The [coordinated matrix](../screenshots/README.md#network-presentation-current-capture-matrix) owns capture.
