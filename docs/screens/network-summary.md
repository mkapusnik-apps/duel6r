# NET-06 — Final network session summary

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It presents final authoritative match results without implying local persistence. It implements `NET-AC-010`, `NET-AC-011`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Normal match completion enters from `NET-05`. Host Return to lobby sends all connected participants to `NET-04` with readiness cleared; guest Leave returns to `NET-01`; unexpected host contact failure enters guest `NET-07`. Only a valid End session notice accepted through the current established session enters guest `NET-09`.

## Representative layout

- Use the final arena frame or retro summary context consistently with the existing score-summary visual language.
- Show result state `Completed` or `Interrupted`.
- Show final-round winner identity, winning team, or `No winner`.
- Show mode, applicable team count and Friendly fire, Assistance, Quick Liquid, Burnable Trees, level plan, round limit, nonzero match seed, and completed-round count.
- Show one row for each completed round with round number, level identity, orientation, winner or `No winner`, and authoritative roster order.
- Show each player identity, owner participant, display name, applicable team, applicable `Departed` state, rounds played, shots, hits, kills, deaths, assists, wins, penalties, survival time, damage, assisted damage, and total points.
- Show per-round player values and cumulative match values.
- Show team totals when the mode uses teams.
- Place the exact label `Session only` beside the heading or result table and show `Not saved to local statistics or Elo` as supporting copy.
- Show host-only `Return to lobby` and `End session`; guests see a waiting status and `Leave`.

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
- Total points must equal kills plus wins plus assists minus penalties.
- An interrupted result must show exactly `Session only • Interrupted • No winner`.
- An interrupted result must include only completed rounds.
- The result must not show Elo, Elo trend, Elo game count, local person records, saved profile state, endpoints, credentials, or persistent history.

## Truthful copy, disabled reasons, and input

- `Session only` must remain visible without relying on tooltip or color.
- `Completed`, `Interrupted`, `No winner`, `Departed`, rank, and team must remain visible as text.
- Column headings must identify every displayed result value.
- The table must not use color or row position as the only rank, winner, team, or departure cue.
- Result content must not disclose an endpoint, credential, raw filesystem value, or peer-supplied diagnostic value.
- Guests see `Waiting for host to return to lobby or end session`; disabled host actions must not appear enabled.
- Focus order is Return to lobby → End session for host, or Leave for guest. Confirmation dialogs have Confirm then Cancel and retain visible focus.
- Keyboard arrows/Tab and controller directions traverse; Enter/Space/controller Confirm activates; Escape/controller Back opens the applicable Leave/End confirmation.

Planned representative screenshot: [`SS-020`](../screenshots/README.md#ss-020).
