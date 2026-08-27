# NET-07 — Guest reconnect

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It truthfully presents a guest's host-clock 30-second reconnect reservation and active-session behavior. It implements `NET-AC-006`, `NET-AC-009`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, and `NET-AC-017` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

An unintentional guest disconnect from `NET-04`, `NET-05`, or `NET-06` enters this state. Success returns to the current authoritative lobby, match, or summary; failure/expiry enters `NET-08` and removes the guest. Host loss enters `NET-09`.

## Representative layout

- At match time, preserve the current arena as context under a readable compact reconnect panel; lobby and summary use their applicable current context.
- Show `Reconnecting to <endpoint>…`, a numeric countdown, and `Your player slots are reserved`.
- The representative state shows `24 seconds remaining` at 1280 by 900.
- During an active round, show `Match continues while you reconnect` and `Reserved players receive no input and remain in play`.
- Offer `Leave session` with supporting consequence `Your reserved players will be removed now and reconnect will stop`.

## Navigation and significant variants

- The host starts one deadline at declared disconnect `D`. Automatic and manual attempts accept only strictly before `D + 30 seconds`, retain that deadline across repeats, and never claim success until current authoritative state is restored.
- Display `ceil(deadline - now)` while positive, so active values are `30` through `1`, never `0`. A later disconnect after successful restore creates a new reservation.
- Match simulation, timers, hazards, connected input, combat, winner checks, and round progression continue behind this state.
- Success replaces this screen with the current `NET-04`, `NET-05`, or `NET-06` state; missed time is not rewound.
- Expiry states `Reconnect time expired • Your players were removed`; same-clock removals are atomic, add no removal combat statistics, and receive one post-batch winner evaluation.
- If fewer than two players remain after expiry, the match ends without a winner and remaining connected participants return to `NET-04`.

## Truthful copy, disabled reasons, and input

- Countdown text must use positive ceiling seconds and remain understandable without animation or color.
- Retry-now may be shown only if it does not extend the fixed deadline; while an attempt is active it is disabled with `Reconnect attempt in progress`.
- Focus defaults to `Leave session`. Keyboard/controller Confirm opens `Leave session? Your reserved players will be removed now and reconnect will stop.` Confirm enters `NET-01`; Cancel continues reconnect against the unchanged deadline. Escape/controller Back does not bypass confirmation.
- No Pause, host migration, or manual identity/account action may appear.

Planned representative screenshot: [`SS-021`](../screenshots/README.md#ss-021).
