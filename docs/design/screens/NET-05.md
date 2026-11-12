# NET-05 — Current match presentation and dedicated context

Functional authority: [NET-05](../../screens/network-match.md), existing NET-VIS and NRP, product-owned NET-ADM-001–016 and NET-SOLO-001–006, NET-PUB-008, and HSL-PUB-005–010. States include NET-05-PUBLIC and product-confirmed NET-05-waiting-for-next-round. Structural sources: existing [NET-05](../../screens/wireframes/network-match.md), [NET-05-C](../wireframes/NET-05/NET-05-C.svg), [NET-05-S](../wireframes/NET-05/NET-05-S.svg), [NET-05-R](../wireframes/NET-05/NET-05-R.svg), and incoming [NET-05-W](../wireframes/NET-05/NET-05-W.svg).

Visual impact: restore the world and gameplay HUD to [Local and network arena parity](../../design.md#local-and-network-arena-parity), UX-ARENA-001–026, under PRES-ARENA-001–012 / AC-001–005 and REP-PRES-010 / REP-PRES-AC-005. [NET-05 SVG](../wireframes/NET-05/NET-05.svg) is the current live structural source; its anchors preserve Local Play. This supersedes preservation of defective network projection, water, attachments, layers or animation. Keep the existing default network visual set, approved background fallback, compact status, framed controls and contextual panels. The local arena is not redesigned.

## Presentation and allocation

- **UX-NET-05-001** Leave session, End session, and eligible Advance round must retain persistent framed controls, sizes and order. Only the non-final limited-round footer moves vertically as specified below to clear the relocated counter; other states retain their existing bounds.
- **UX-NET-05-002** Panels must not add gameplay-focus targets outside their existing context.
- **UX-NET-05-003** NET-05-C must retain its centered 640-client-pixel maximum width, 16-client-pixel edge clearance, and two separate actions.
- **UX-NET-05-004** The confirmation title must precede wrapped consequence copy and the fixed explicit-action/Cancel row.
- **UX-NET-05-005** Confirmation must retain explicit-action-first focus and opening-input protection.
- **UX-NET-05-006** NET-05-S must retain its arena placement without a menu banner or backdrop.
- **UX-NET-05-007** NET-05-S must keep authoritative-score title, session scope, current result-state labels, and body separate.
- **UX-NET-05-008** NET-05-S must not advertise unsupported scrolling or invent completed-result rows.
- **UX-NET-05-009** A non-final NET-05-R score panel must use the centered Local Play OVER-02 allocation when it fits. When reserved network regions constrain it, the panel must remain horizontally centered with at least 16 client px clearance from client edges, outcome/countdown, status, waiting feedback, and session actions; only its ranking body may lose visible rows.
- **UX-NET-05-010** The score panel must use OVER-02's centered `---SCORE---` strip, column headings, 32 px score text and row pitch, translucent outer/inner layers, and flat score surfaces instead of the grey raised network panel and inset ranking card. The counter uses OVER-02's external bottom-center placement; remove its old panel row.
- **UX-NET-05-011** Network outcome and phase/countdown text must remain separate below the common score table, using standard readable gameplay text without a menu card. They must not replace a score column or imply that the cumulative leader won the round.
- **UX-NET-05-012** The table must match Local Play's non-Team row treatment and Team header/nested-row colors. Each Team group must retain the local 8 px separator band and centered 2 px rule between groups only. Network lifecycle text and existing non-color focus feedback must remain legible.
- **UX-NET-05-013** First-second active and later frozen phases must retain one structural variant without timing or eligibility changes.
- **UX-NET-05-014** Long values must remain contained without smaller text or covered footers. Non-final numeric overflow must follow the bounded fallback below.
- **UX-NET-05-015** Long outcomes must wrap above phase/countdown text without covering the common score table or session actions.
- **UX-NET-05-016** A constrained ranking body must retain the fixed external counter and column headings, 32 px rows, existing ranking navigation and position feedback. Extra outcome lines and the reserved footer must reduce visible ranking height before any fixed region is covered.

NET-05-S position information is plain non-actionable text. Do not add enabled arrows, scroll handlers, focus stops, or shortcuts. Tab retains its existing discrete toggle. Result navigation in NET-04-R and NET-06 and ranking navigation in NET-05-R remain unchanged. Confirmation over lobby, summary, or reconnect uses NET-05-C with that context's product-owned consequence.

## Non-final round-summary parity

Functional authority is [UI-RND-011–013 and AC-RND-001](../../features.md#status-ranking-and-shared-arena-view). This applies only after an outcome when another configured round remains. Local Play [OVER-02](../../screens/overlay-round-over.md) and its [existing wireframe](../../screens/wireframes/overlay-round-over.md) are the reference, not the former network panel. [NET-05-R](../wireframes/NET-05/NET-05-R.svg) shows the common table with preserved network-only regions. The [current counter-placement matrix](../screenshots/README.md#non-final-counter-placement) owns the bounded placement replacement; prior parity evidence retains its unchanged scope.

Use the existing local panel padding, measured table width, white heading/progress text, aligned K/A/D/K/D/PTS columns, Team identity colors, nested row treatment, and winner curtain. A network rank prefix must not displace the local name/score column allocation. Keep the current network name-containment and lifecycle cues; this change adds no horizontal-navigation input. If width is constrained, contain the name before applying the numeric-overflow fallback. Do not shrink score text to fit more rows.

**Bounded numeric overflow:** Show complete numeric text in the existing Local Play format whenever it fits. Numeric columns may expand only within the available table width while retaining a visible name fragment or its existing ellipsis and the applicable lifecycle cue. If complete numeric text still cannot fit, the affected cell must show a right-aligned `…`, or `-…` for a negative value, entirely inside its column. This marker means that the value does not fit, not zero or missing statistics. Keep all five column headings and cells, ordinary values that fit, 32 px text and row pitch, and panel containment. Do not clip unmarked digits, clamp or round a value to make it fit, remove a column, or let numeric expansion erase the name region. This display-only exception applies to player and Team aggregate cells in non-final NET-05-R; authoritative values, totals, ratios and ranking remain unchanged. It adds no reveal action or horizontal navigation and does not change Local Play or final-result exact-value access.

The local score core remains the reference even though network-only outcome/countdown and actions require additional bounds. Keep these additions separate from the table. Preserve the existing Host Advance round then End session actions, Guest Leave session, focus order, opening-input protection, and noninteractive session-only/script/status copy. Both round-end phases use the same structure. No local Continue prompt, local non-final scroll input, new network action, final-style outcome/history section, or `End of Game` notice is introduced.

**Non-final counter footer:** Use the exact counter backing, padding, center and bottom inset in [OVER-02](../../screens/overlay-round-over.md#layout-and-hierarchy). The complete client bottom is the anchor, not the score panel or the top of network status. Do not reinterpret the existing completed/total values as rounds remaining. Keep the counter fixed when ranking scrolls, in both round-end phases, with no pointer/focus target.

Reserve the bottom-center counter first. The status bar must start 16 client px above the counter backing's top, retaining its 16 px side insets, existing text, 4 px inner padding, wrapping and three-row limit. Place the existing action/waiting row 16 px above the status bar's top: retain right-aligned 34 px action buttons, their existing horizontal gaps/order, and waiting feedback to their left without overlap. Keep outcome/countdown above that row with at least 16 px clearance; the score panel must retain its existing separation from those texts. Only these necessary vertical offsets and removal of the old counter row are authorized. Do not move the camera, shrink score text, change controls/status content, or add an action. At constrained viewports, the existing network ranking body yields rows to this fixed footer and retains its current navigation.

Local OVER-02 uses the same counter relocation, including unchanged resumed count semantics; its unlimited variant remains unchanged. NET-05-S, active live ranking/progress, the final-round delay, NET-06 and OVER-03 retain their existing contracts and evidence. A genuine conflict returns to team rather than silently changing those surfaces.

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

Use an ordinary real dedicated Connected match for changed status coverage. The scoped confirmation representative uses the dedicated controller's isolated-End consequence over NET-07; guest Leave and other contexts retain their current structure and focused QA. Unchanged NET-05-S may retain develop evidence with source-impact confirmation rather than receive an automatic replacement. The NET-05-R placement follow-up uses the [current counter-placement matrix](../screenshots/README.md#non-final-counter-placement), not a repeat of the previous three-row parity campaign. A completed summary cannot substitute for a non-final result when that result presentation changes. Preserve degraded/resynchronizing containment, all modes, maximum ranking, and real first-round arrival. The [dedicated scoped matrix](../screenshots/README.md#dedicated-extension-scoped-capture-matrix) retains its status-only scope; it does not claim new Invisibility, orientation, result-extreme, or gameplay-continuity acceptance.

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

The live representative shows one living owned player in a genuinely eligible active state with no winner/pause treatment. NET-05-W shows actual admitted waiting with no owned actor. Specific bonus/degraded fixtures remain focused obligations, not mandatory additions to these representatives. NET-05-C/S and all unrelated SVGs remain unchanged; NET-05-R uses the approved non-final parity layout above.
