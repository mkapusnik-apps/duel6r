# OVER-03 — Final game summary

## Purpose and traceability

`OVER-03` presents the final local ranking after the last round of a finite match. `NET-06` presents the completed authoritative network result. They use one common whole-screen presentation with different data and permitted actions. These stable screen IDs identify functional journeys, not separate visual designs.

Local entry occurs when the configured final round gains a winner or no-winner result. Escape closes gameplay after the final wait completes. Shift+Escape closes gameplay earlier. Network entry, return, departure, recovery, and result lifetime remain in [NET-06](network-summary.md).

Local requirements are `LIF-018`–`LIF-029`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014`, `UI-GAME-001`–`UI-GAME-007`, and `AC-070`–`AC-072` in [features.md](../features.md). Network result requirements remain in [network play](../network-play-first-release.md#session-only-result-lifecycle) and [the authoritative match contract](../network-authoritative-headless-match.md#session-only-result).

## Common final-screen contract

This section is the authoritative source for `SUMMARY-VIEW` requirements. UX owns the shared whole-screen appearance in [design.md](../design.md) and its linked screen specifications, including [NET-06](../design/screens/NET-06.md). Separate mandatory local arena-overlay and network menu-background treatments do not apply to the common final screen. The common appearance includes finite Predator results; non-final and active-round score overlays remain separate states.

- **SUMMARY-VIEW-001** Local and network final results must use one common whole-screen presentation, not only a common score table.
- **SUMMARY-VIEW-002** The common screen must apply the same presentation rules to equivalent result content, with source-specific content and permitted actions.
- **SUMMARY-VIEW-003** The screen must preserve each result value and ranking order supplied by the applicable gameplay authority.
- **SUMMARY-VIEW-004** The screen must keep the meaning of a match outcome separate from cumulative ranking.
- **SUMMARY-VIEW-005** The screen must keep all required result content accessible at the supported roster size.
- **SUMMARY-VIEW-006** The screen must offer only the actions permitted by the local or network journey.
- **SUMMARY-VIEW-007** Opening, drawing, navigating, or closing the screen must not duplicate a score award, outcome, or required persistence operation.

Common appearance covers the background/context treatment, screen frame, headings, result regions, score presentation, and status/action treatment. UX defines their geometry, typography, overflow, focus, and visual rules. Conditional network details and role-specific actions do not require a separate visual design. Appearance reuse does not require Local Play to acquire network-only history, identities, seed data, connection status, or participant actions.

## Local content and actions

- The local final result must retain K, A, D, K/D, and PTS.
- Local rankings and score values must retain the existing mode rules.
- Team results must retain team identity and membership for two through four teams.
- Local finite results must support two through 15 players in each applicable mode.
- Local Deathmatch and Team results must retain `End of Game` and final round progress under `UI-GAME-002`–`UI-GAME-007`.
- Final Elo updates must not appear as a separate gameplay panel.
- Local Play must retain its existing Escape and Shift+Escape actions and return to the main menu.
- The local final result must not start another round automatically.
- The screen must not add a local host, Leave session, Return to lobby, or End session action.
- Local results must not use `Session only` or `Not saved to local statistics or Elo` to describe local persistence.

`LIF-020`–`LIF-029`, `SCO-022`–`SCO-024`, and `PER-001`–`PER-005` retain authority over local saves, continuation, and Elo. A network result must not enter these local persistence paths. Shared ranking presentation does not introduce a new tie-break rule for Local Play or remove the network tie-break rules.

### Local completed-result navigation

This section owns local overflow navigation for `SUMMARY-VIEW-005`. It supplies the functional input binding for [OVER-03](../design/screens/OVER-03.md) and `UX-FINAL-020`–`UX-FINAL-026` in [the shared final presentation](../design.md#shared-final-presentation).

- **SUMMARY-LOCAL-NAV-001** In `OVER-03-completed`, Page Up and Page Down must scroll result content vertically toward preceding and following content, respectively.
- **SUMMARY-LOCAL-NAV-002** In `OVER-03-completed`, the mouse wheel must scroll result content vertically only when the pointer is inside the result body.
- **SUMMARY-LOCAL-NAV-003** For navigation under `SUMMARY-LOCAL-NAV-001` and `SUMMARY-LOCAL-NAV-002`, Shift must select horizontal scrolling instead of vertical scrolling, with up toward the left and down toward the right.
- **SUMMARY-LOCAL-NAV-004** The result view must consume these navigation inputs only when `OVER-03-completed` is the active input context.
- **SUMMARY-LOCAL-NAV-005** Result navigation must stop at the content bounds and make no change on an axis without overflow.

`OVER-03-final-wait` must not consume these inputs for result navigation. Console input retains `CFG-002` precedence. Navigation changes only the visible part of result content; it does not change completion timing, outcome, ranking, persistence, or the exit destination. Escape, Shift+Escape, Tab, and assigned player/controller actions retain their existing behavior. No local controller navigation binding, exit control, or confirmation is added. Network result navigation remains unchanged.

## Functional states and transitions

| State | Entry and displayed meaning | Permitted transition |
|---|---|---|
| `OVER-03-final-wait` | Local final outcome exists; final wait is not complete | Shift+Escape closes gameplay; completion of the wait permits ordinary Escape |
| `OVER-03-completed` | Local final wait is complete; bounded result navigation is available | Navigation stays in this state; Escape or Shift+Escape returns to the main menu |
| `NET-06-completed-host` | Completed network result; participant has host/controller authority | Existing Return to lobby, confirmed End session, or network recovery |
| `NET-06-completed-guest` | Completed network result; participant is a guest | Existing confirmed Leave or network recovery; host Return moves connected participants to the lobby |

`NET-06-PUBLIC` remains a hosting-context variant, not a separate result design. Interrupted network results go directly to `NET-04`, never to the completed final screen. Network waiting players that never spawned have no match-participation result row. Departed identities and established outcomes remain intact under `NET-RES-001`–`NET-RES-002`. The screen does not decide winners, complete a round, extend a match, or transfer controller authority.

## Acceptance criteria

- **SUMMARY-VIEW-AC-001 — Whole-screen parity:** Local and network final Deathmatch, Predator, and Team results use the same approved whole-screen presentation rules at 1280 × 900 and the 1280 × 720 evaluation minimum. Differences are attributable to required content, available data, and permitted actions, not a separate local overlay versus network menu design.
- **SUMMARY-VIEW-AC-002 — Data integrity:** Common score quantities and team totals remain correct for equivalent result inputs. Local mode rankings remain unchanged. Network rows retain points, wins, damage, and authoritative roster-order precedence, with team ties resolved in fixed team order. A cumulative leader different from the final-round winner does not replace the network match outcome.
- **SUMMARY-VIEW-AC-003 — Complete content:** Required content remains readable and accessible at the supported maximum roster. Network coverage includes 14 Predator winners, departed winner identities, supported 64 UTF-8-byte names, and both result-scroll extremes. Local coverage includes team membership, K/A/D/K/D/PTS, and complete multi-digit final round progress.
- **SUMMARY-VIEW-AC-004 — Actions and timing:** Local final-wait and completed exits retain their eligibility and destinations. Host/controller and guest actions retain their permissions, exact confirmation consequences, and destinations. A guest cannot advance, return everyone to the lobby, or end the session. Repeated presentation or recovery creates no extra round, award, or outcome.
- **SUMMARY-VIEW-AC-005 — Persistence and lifecycle:** Offline Local Play retains saves, continuation, and eligible Elo updates. Network results retain `Session only`, no local statistics/Elo writes, waiting/departed semantics, same-session lobby retention, and discard on session end. Interrupted network results do not enter this screen.
- **SUMMARY-VIEW-AC-006 — Local overflow access:** In `OVER-03-completed`, wheel and Page Up / Page Down expose vertical overflow. Their Shift variants expose horizontal overflow without vertical movement. Wheel navigation applies inside the result body. All required rows and complete overflowing identities remain accessible at both scroll extremes. Navigation stops at bounds and does not move an axis without overflow.
- **SUMMARY-VIEW-AC-007 — Local input isolation:** The same inputs do not navigate or get consumed by the result view during `OVER-03-final-wait` or while the console owns input. Navigation in the completed state leaves results, persistence, and completion timing unchanged. Escape, Shift+Escape, Tab, assigned player/controller actions, and network navigation retain their existing behavior.

Behavioral evidence must cover values, authority, transitions, and persistence. UX assesses the whole screen through its coordinated screenshot matrix; static images alone do not establish lifecycle correctness. Existing `SS-012` and `SS-020` identify local and network result coverage, not acceptance of a changed presentation.
