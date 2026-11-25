# NET-06 — Final network session summary

## Status, purpose, and requirements

### Public pilot functional contract

`NET-06-PUBLIC` retains the existing completed, session-only summary under `NET-PUB-008`. The controller has the existing host Return to lobby and End session permissions. Controller End uses the confirmation and destinations in the [NET-04 public contract](network-lobby.md). Guests retain Leave. All dedicated participants use `NET-07` on ambiguous contact loss. Confirmed terminal notices use the outcomes in HSL-PUB-005 through HSL-PUB-010; maintenance does not offer saved results or session restoration.

Functional acceptance: `NET-PUB-AC-002`–`NET-PUB-AC-004`, `NET-PUB-AC-006`, and `HSL-PUB-AC-002` cover permissions, return to lobby, result lifetime, and interruption. UX owns public role and status presentation.

This screen presents a completed authoritative match result without implying local persistence. It implements `NET-AC-010`, `NET-AC-011`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [network play](../network-play-first-release.md), and `SUMMARY-VIEW-001`–`SUMMARY-VIEW-007` in the [common final-screen contract](overlay-game-over.md#common-final-screen-contract). `NET-06-completed-host` and `NET-06-completed-guest` share the whole-screen presentation with local `OVER-03`; their functional permissions remain different.

Normal match completion enters from `NET-05`. An interrupted match enters `NET-04` directly and never enters this screen. Host Return to lobby sends all connected participants to `NET-04` with readiness cleared; guest Leave returns to `NET-01`; unexpected host contact failure enters guest `NET-07`. Only a valid End session notice accepted through the current established session enters guest `NET-09`.

## Required result content

UX owns the common whole-screen appearance in [design.md](../design.md) and [NET-06](../design/screens/NET-06.md). A separate mandatory menu canvas or arena overlay is not a functional requirement. Every winner must remain readable with full display name, player identity, outcome scope, and departure state independently of live membership under `NET-RES-001`–`NET-RES-002`. Existing result navigation must expose supported 64 UTF-8-byte names without splitting characters.

- Use the common final-screen presentation under `SUMMARY-VIEW-001`–`SUMMARY-VIEW-007`.
- Show result state `Completed`.
- Retain the match outcome as the configured final-round winner identity, winning team, or `No winner` in the existing scrollable result detail, separate from cumulative scores. Do not repeat it as a fixed `Match outcome` caption above the main score table.
- Retain the last completed-round number and outcome in the corresponding round detail. Do not add a duplicate fixed `Last completed round` row; completed match and final-round outcomes remain the same authoritative value.
- Show mode, applicable team count and Friendly fire, Assistance, Quick Liquid, Burnable Trees, level plan, round limit, nonzero match seed, and completed-round count.
- Show one row for each completed round with round number, level identity, orientation, winner or `No winner`, and authoritative roster order.
- Show each player identity, owner participant, display name, applicable team, applicable `Departed` state, rounds played, shots, hits, kills, deaths, assists, wins, penalties, survival time, damage, assisted damage, and total points.
- Show per-round player values and cumulative match values.
- Start with cumulative scores under the single `---SCORE---` strip, without an additional `SCORE` title or `Cumulative ranking` caption. Keep outcome and round detail distinguishable from that table.
- Show team totals when the mode uses teams.
- Keep exact `Session only` and `Not saved to local statistics or Elo` copy in the compact network supporting region outside the scrolling table.
- Show host-only `Return to lobby` and `End session`; guests see a waiting status and `Leave`.
- Keep `Session only`, `Completed`, and the no-persistence disclaimer visible while the user navigates result content. Outcome identities and last-round values must remain accessible through the existing result navigation; they need not occupy fixed rows.
- Identify match settings, round results, and cumulative results as distinct content.
- Let the result viewport scroll vertically when all sections do not fit.
- Provide horizontal scrolling inside a table when all required columns do not fit.
- Keep action controls visible while result content scrolls.

### Score columns and supporting detail

The cumulative score table and each per-round player score table must show player display names and exactly `K`, `A`, `D`, `K/D`, and `PTS`, in that order, matching Local Play. These mean kills, assists, deaths, kills-to-deaths ratio, and points. Use Local Play's existing integer and two-decimal K/D display convention, including its truncation and zero-death case; this changes presentation, not authoritative values.

Cumulative scores retain their completed-round scope; each per-round score table retains that round's values. Team score rows must show summed player K, A, D, and PTS, with K/D calculated from the team's total kills and deaths rather than summed player ratios. Preserve authoritative player/team ordering and membership.

Rank, player identity, owner participant, team/departure metadata, rounds played, shots, hits, wins, penalties, survival time, damage, and assisted damage must remain readable in supporting result/round detail through the existing navigation, explicitly associated with the same player and cumulative or round scope. Preserve each field's authoritative scope; do not invent a per-round rank. They must not add columns or identity/owner strings to the compact name-and-score table. Keep applicable `Departed` text in that detail and preserve complete display-name/identity access for duplicate or long names. No required field above may be dropped from the result or made inaccessible.

The same score-column selection and full supporting-field access apply when these tables appear in retained `NET-04-R` results, including interrupted results. This does not redesign the retained lobby, add controls or change result availability. Active-play Tab, live ranking, non-final summaries and the final-round delay retain their existing contracts; no completed rows or new metrics are introduced there.

## Navigation and significant variants

- The host can return all connected participants to `NET-04`; readiness is cleared and the completed result remains `Session only` in the lobby until a new match starts and clears it.
- Host End session opens `End session for everyone?` Confirm sends the host to `NET-01`, guests to host-ended `NET-09`, and discards session results; Cancel returns to the summary.
- Guest Leave opens `Leave session? Your players will be removed and you will return to Network.` Confirm enters guest `NET-01`; Cancel returns to the summary. A departed participant remains labeled `Departed` in retained result rows for participants still in session.
- An unintentional guest disconnect receives the original host-clock 30-second reservation through `NET-07` and returns to the current summary on strictly-before-deadline success.
- Final-summary removal never reevaluates or replaces the completed match outcome; it retains the result and labels affected rows `Departed`.
- Accepted intentional host End enters `NET-09` and discards the result; every unexpected host failure remains guest `NET-07` until terminal rejection or deadline expiry.
- Starting a new match clears this result set; no result survives as local persistence.
- The summary must not offer Save, Elo update, account upload, or local-statistics merge.
- Player rows must rank by total points, wins, damage, and authoritative roster order in descending precedence.
- Team rows must rank by team point total and then Alpha, Bravo, Charlie, and Delta.
- Player rows inside a team must use the player ranking rules.
- A ranking leader must not receive a champion label or treatment.
- Total points must equal kills plus wins plus assists minus penalties.
- The result must not show Elo, Elo trend, Elo game count, local person records, saved profile state, endpoints, credentials, or persistent history.

## Truthful copy, disabled reasons, and input

- `Session only` must remain visible without relying on tooltip or color.
- `Completed` must remain visible as text. `No winner`, `Departed`, rank, team and each required outcome must remain readable as text in their applicable result or round detail, not inferred from color or cumulative position.
- Column headings must identify every displayed result value.
- Required rank, winner, team, and departure information must remain explicit in the applicable result detail; color or score-row position alone must not substitute for it.
- The table must not imply that the highest cumulative rank is the match outcome.
- Result content must not disclose an endpoint, credential, raw filesystem value, or peer-supplied diagnostic value.
- Guests see `Waiting for host to return to lobby or end session`; disabled host actions must not appear enabled.
- Focus order is Return to lobby → End session for host, or Leave for guest. Confirmation dialogs have Confirm then Cancel and retain visible focus.
- Keyboard arrows/Tab and controller directions traverse; Enter/Space/controller Confirm activates; Escape/controller Back opens the applicable Leave/End confirmation.

## Acceptance

`SUMMARY-VIEW-AC-001`–`SUMMARY-VIEW-AC-005` cover common appearance, result meaning, complete content, permissions, and persistence. `NET-RES-AC-001` retains complete departed and multiwinner coverage. Network round completion must retain its final delay before this screen; shared local appearance must not expose Return to lobby early. End session retains its existing availability during `NET-05`. Waiting players must not gain result rows before participation. A result with no winner must not acquire a winner from cumulative ranking.

For score-column parity, verify the exact Local Play columns and formatting, Team aggregates, unchanged cumulative/per-round scope, and complete access to every displaced field in both final and retained results.

UX owns the coordinated representative coverage, including the [current SS-020 compact final capture](../design/screenshots/README.md#current-ss-020). Earlier visual artifacts do not establish acceptance of the common final-screen appearance.
