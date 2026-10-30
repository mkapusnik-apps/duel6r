# NET-05 — Current match presentation and dedicated context

Functional authority: [NET-05](../../screens/network-match.md), existing NET-VIS and NRP, product-owned NET-ADM-001–016 and NET-SOLO-001–006, NET-PUB-008, and HSL-PUB-005–010. States include NET-05-PUBLIC and product-confirmed NET-05-waiting-for-next-round. Structural sources: existing [NET-05](../../screens/wireframes/network-match.md), [NET-05-C](../wireframes/NET-05/NET-05-C.svg), [NET-05-S](../wireframes/NET-05/NET-05-S.svg), [NET-05-R](../wireframes/NET-05/NET-05-R.svg), and incoming [NET-05-W](../wireframes/NET-05/NET-05-W.svg).

Visual impact: restore the world and gameplay HUD to [Local and network arena parity](../../design.md#local-and-network-arena-parity), UX-ARENA-001–026, under PRES-ARENA-001–012 / AC-001–005 and REP-PRES-010 / REP-PRES-AC-005. [NET-05 SVG](../wireframes/NET-05/NET-05.svg) is the current live structural source; its anchors preserve Local Play. This supersedes preservation of defective network projection, water, attachments, layers or animation. Keep the existing default network visual set, approved background fallback, compact status, framed controls and contextual panels. The local arena is not redesigned.

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

Immediate first-round arrival remains distinct from product-confirmed waiting admission. Neither route may add a false lobby, spectator mode, or blanket join-in-progress rejection. Product determines hosting-specific eligibility and the actual destination.

## Dedicated status

- The dedicated status region must use `Dedicated session` instead of legacy LAN-only scope copy.
- Role must remain the confirmed Host or Guest.
- Status must retain its wrapping, three-row limit, and no-focus behavior.
- Host End and guest Leave must retain distinct consequences and NET-05-C structure.
- Explicit controller departure must not imply termination of the remote service process.
- Retained dedicated context in NET-07 and NET-09 must keep the same identity and orientation.
- Dedicated status must omit directory publication state and Retry publication targets without changing player-hosted publication feedback.

## Acceptance

Use an ordinary real dedicated Connected match for changed status coverage. The scoped confirmation representative uses the dedicated controller's isolated-End consequence over NET-07; guest Leave and other contexts retain their current structure and focused QA. Unchanged NET-05-S and NET-05-R may retain develop evidence with source-impact confirmation rather than receive automatic replacements. A completed summary cannot substitute for a non-final result when that result presentation changes. Preserve degraded/resynchronizing containment, all modes, maximum ranking, and real first-round arrival. The [scoped matrix](../screenshots/README.md#dedicated-extension-scoped-capture-matrix) owns changed capture; it does not claim new Invisibility, orientation, result-extreme, or gameplay-continuity acceptance.

## Connected waiting and solo presentation

Product owns admission, waiting eligibility, opposition, outcomes, and spawn timing. The UI presents only authoritative state. These requirements do not authorize a dedicated admission route absent its functional contract.

- **UX-NET-05-017** A confirmed Connected-waiting participant must retain the current undimmed shared arena and truthful Connected status rather than lobby, reconnect, pause, or spectator presentation.
- **UX-NET-05-018** Waiting feedback must show `Your players join next round.` when another round remains, otherwise `Waiting for the next match.`, without promising a new match start.
- **UX-NET-05-019** Each Waiting roster entry must use a textual Waiting marker without an owned body, normal gear, living/winner styling, or fabricated score/result participation.
- **UX-NET-05-020** Waiting must retain score access, role-appropriate Leave/End controls, confirmations, and pointer/focus behavior; feedback must remain noninteractive.
- **UX-NET-05-021** Authoritative eligible spawn or final outcome must clear the corresponding waiting feedback/markers without an early or duplicate presentation from local input, timers, or stale state.
- **UX-NET-05-022** At 1280 by 720, waiting feedback must wrap above network status without obscuring ranking, progress, events, player status, or actions or changing camera/world scale.
- **UX-NET-05-023** Product-confirmed playable solo state must use normal active presentation without speculative pause, victory, winner, or solo-policy controls.
- **UX-NET-05-024** A confirmed no-winner completion must use existing completion/countdown presentation; waiting must not create a separate result screen or false continuation claim.

Network status must retain the actual confirmed role and hosting label. The private LAN example in NET-05-W is not permission to label a dedicated connection LAN-only. Dedicated scope uses the existing Dedicated session copy and role consequences.

The live representative shows one living owned player in a genuinely eligible active state with no winner/pause treatment. NET-05-W shows actual admitted waiting with no owned actor. Specific bonus/degraded fixtures remain focused obligations, not mandatory additions to these representatives. NET-05-C/S/R and all unrelated SVGs remain unchanged.
