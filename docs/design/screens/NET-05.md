# NET-05 — Match controls and contextual panels

Functional contract: [network-match](../../screens/network-match.md). Existing NET-VIS and NRP presentation requirements remain applicable. [NET-ADM-001–012 and NET-ADM-AC-001–003](../../network-play-first-release.md) own admission and arrival. Structural sources: [NET-05 live arena](../../screens/wireframes/network-match.md), [NET-05-C confirmation](../wireframes/NET-05/NET-05-C.svg), [NET-05-S pre-winner score overlay](../wireframes/NET-05/NET-05-S.svg), and [NET-05-R non-final round result](../wireframes/NET-05/NET-05-R.svg). The two new variants document existing tasks, not new functional states or actions.

## Presentation and allocation

The [shared visual baseline](../../design.md#unified-network-presentation) applies to session controls and panels only. The arena, camera, world assets, gameplay HUD, and compact translucent network status remain unchanged.

- **UX-NET-05-001** Existing Leave session, End session, and eligible Advance round actions must use persistent framed buttons inside their current client-relative bounds.
- **UX-NET-05-002** A panel must not add gameplay-focus targets outside its existing interaction context.
- **UX-NET-05-003** NET-05-C must keep its centered 640 px maximum-width panel, at least 16 px client inset, and two separate action targets.
- **UX-NET-05-004** The confirmation prompt must occupy the blue title strip above wrapped consequence copy and the fixed Confirm-action/Cancel row.
- **UX-NET-05-005** Confirmation must retain Confirm-action-first focus and its existing opening-input protection.
- **UX-NET-05-006** NET-05-S must retain the existing score-overlay placement over the arena without adding a menu banner or backdrop.
- **UX-NET-05-007** NET-05-S must keep its authoritative-score title, session scope, current result-state labels, and existing result viewport separate.
- **UX-NET-05-008** NET-05-S must use fixed headings and a white inset body without presenting unsupported scrolling as an enabled action or inventing completed-result rows when none exist.
- **UX-NET-05-009** NET-05-R must retain its client-relative centered panel below the top arena edge and above network status and session actions.
- **UX-NET-05-010** NET-05-R must keep round progress above its blue SCORE strip and ranking body.
- **UX-NET-05-011** NET-05-R must keep round outcome and phase/countdown text below the ranking without overlap.
- **UX-NET-05-012** Ranking rows inside NET-05-R must retain their existing player/team state colors on the gameplay `ranking-surface` inside the inset body rather than move those colors onto a white list.
- **UX-NET-05-013** The first-second active phase and later frozen phase must use the same structural variant without changing advance eligibility or timing.
- **UX-NET-05-014** A contextual panel must contain long values and allow the existing body navigation without shrinking text or covering its footer.
- **UX-NET-05-015** A long round outcome must wrap in a bounded region above phase/countdown text before it can overlap that text.
- **UX-NET-05-016** Extra outcome rows must reduce the visible ranking height while its existing scrolling keeps every ranking row reachable.

The ranking-color exception in UX-NET-05-012 preserves gameplay semantics and avoids low-contrast yellow-on-white rows. It does not permit a separate button or focus theme. Panel frames and action controls still use the shared menu vocabulary. Confirmation over NET-04, NET-06, or NET-07 uses NET-05-C's structure with the owning screen's existing prompt and consequences.

NET-05-S has no supported scroll handlers in the baseline. It may retain truthful row/column position information as plain non-actionable text. Retained arrow symbols must not have enabled bevels, focus outlines, pointer targets, or shortcut promises. This correction must not add handlers, focus stops, or shortcuts. Tab retains its existing discrete overlay toggle. Working result navigation in NET-04-R and NET-06 and ranking navigation in NET-05-R must remain unchanged.

## First-round arrival

Entry from `NET-03-live-admission` uses the existing arena layout without a new panel.

- An admitted first-round arrival must enter the current confirmed shared arena directly.
- The screen must not show a new-round introduction for every existing player because one participant arrived.
- The arriving player's name and normal gear indicators must remain readable using existing gameplay presentation.
- The UI must not show a spectator or next-round-waiting message for an arrival admitted to play immediately.
- Existing players, current world objects, round progress, and Predator presentation must remain visually continuous.
- A directory refresh must not appear over live play or take gameplay focus.
- The arrival must not add a split viewport, modal, or new gameplay action.

## Acceptance

The live representative must retain the complete arena, truthful Connected status, and readable framed session action. Two players are sufficient for the default styling representative; a specific bonus, level, mirror, or degraded condition is not required in that image. NET-05-C must expose complete consequence copy and both actions at 1280 × 720. NET-05-S must show the real pre-winner score state without claiming completion or supported scrolling. NET-05-R must show a real non-final frozen phase with separate progress, outcome, positive countdown, ranking, and host actions. Capture routes are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).

Focused QA must cover the active first second, guest absence of Advance round, all ranking modes and maximum rows, long winner names, opening/held/repeated confirmation input, Tab toggling, all confirmation contexts, degraded/resynchronizing status, and first-round arrival. Arrival is a supplemental state, not another representative. A still cannot prove unchanged gameplay, recovery timing, Predator retention, input consumption, or admission boundaries.

The simpler live representative does not replace degraded/resynchronizing containment checks or certify Invisibility, orientation, or maximum-player behavior. These checks follow the evidence-scope rules in the current matrix. The NET-05-S check must confirm that no unsupported arrow looks enabled and no new input is consumed for scrolling.
