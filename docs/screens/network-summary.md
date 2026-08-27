# NET-06 — Final network session summary

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It presents final authoritative match results without implying local persistence. It implements `NET-AC-010`, `NET-AC-011`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Normal match completion enters from `NET-05`. Host Return to lobby sends all connected participants to `NET-04` with readiness cleared; guest Leave returns to `NET-01`; guest disconnect enters `NET-07`; host loss enters `NET-09`.

## Representative layout

- Use the final arena frame or retro summary context consistently with the existing score-summary visual language.
- Show the match outcome, three-round result rows in the representative state, participant/player identity, and final authoritative totals.
- Place the exact label `Session only` beside the heading or result table and show `Not saved to local statistics or Elo` as supporting copy.
- Show host-only `Return to lobby` and `End session`; guests see a waiting status and `Leave`.

## Navigation and significant variants

- The host can return all connected participants to `NET-04`; readiness is cleared and the completed result remains `Session only` in the lobby until the next match replaces it.
- Host End session opens `End session for everyone?` Confirm sends the host to `NET-01`, guests to host-ended `NET-09`, and discards session results; Cancel returns to the summary.
- Guest Leave opens `Leave session? Your players will be removed and you will return to Network.` Confirm enters guest `NET-01`; Cancel returns to the summary. A departed participant remains labeled `Departed` in retained result rows for participants still in session.
- An unintentional guest disconnect receives the original host-clock 30-second reservation through `NET-07` and returns to the current summary on strictly-before-deadline success.
- If the host is lost, guests enter `NET-09`; no result is persisted.
- The next completed match replaces this result set; host end/loss discards it; no result survives as local persistence.
- The summary must not offer Save, Elo update, account upload, or local-statistics merge.

## Truthful copy, disabled reasons, and input

- `Session only` must remain visible without relying on tooltip or color.
- Guests see `Waiting for host to return to lobby or end session`; disabled host actions must not appear enabled.
- Focus order is Return to lobby → End session for host, or Leave for guest. Confirmation dialogs have Confirm then Cancel and retain visible focus.
- Keyboard arrows/Tab and controller directions traverse; Enter/Space/controller Confirm activates; Escape/controller Back opens the applicable Leave/End confirmation.

Planned representative screenshot: [`SS-020`](../screenshots/README.md#ss-020).
