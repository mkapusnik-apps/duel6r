# NET-07 — Guest reconnect

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It truthfully presents a guest's host-clock 30-second reconnect reservation and active-session behavior. It implements `NET-AC-006`, `NET-AC-009`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, and `NET-AC-017` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

An unintentional guest disconnect from `NET-04`, `NET-05`, or `NET-06` enters this state. Accepted restore returns to current authoritative lobby, match, or summary. A valid host-end notice or independently definitive termination enters `NET-09`; terminal rejection or deadline expiry enters `NET-08`.

## Representative layout

- At match time, preserve the current arena as context under a readable compact reconnect panel; lobby and summary use their applicable current context.
- Show `Reconnecting to <endpoint>…`, a numeric countdown, and `Your player slots are reserved`.
- The representative state shows `24 seconds remaining` at 1280 by 900.
- During an active round, show `Match continues while you reconnect` and `Reserved players receive no input and remain in play`.
- Offer `Leave session` with supporting consequence `Your reserved players will be removed now and reconnect will stop`.

## Navigation and significant variants

- The host starts one deadline at declared disconnect `D`. Automatic and manual attempts accept only strictly before `D + 30 seconds`, retain that deadline across repeats, and never claim success until current authoritative state is restored.
- Display `ceil(deadline - now)` while positive, so active values are `30` through `1`, never `0`. A later disconnect after successful restore creates a new reservation.
- Apply the fixed outcome order: host end; independently definitive termination; accepted reconnect before deadline; terminal rejection; retryable ambiguity; deadline expiry.
- Resolution failure, refusal, unreachable, reset, temporary transport failure, and no response are retryable ambiguous outcomes. Show their retry status but remain here through the original deadline.
- Invalid or expired reconnect credential, missing reservation or removed participant, and compatibility/trust rejection are terminal only when established by an authoritative response. They enter `NET-08` with Retry disabled.
- Match simulation, timers, hazards, connected input, combat, winner checks, and round progression continue behind this state.
- Success replaces this screen with the current `NET-04`, `NET-05`, or `NET-06` state; missed time is not rewound.
- Guest-side expiry states exactly `Reconnect time expired. The session could not be restored.` and enters `NET-08` with Retry disabled. It does not claim host end, definitive termination, reservation removal, or player removal.
- Authoritative host-side expiry follows the lifecycle-specific lobby, active-round, non-final-summary, or final-summary batch rules.

## Truthful copy, disabled reasons, and input

- Countdown text must use positive ceiling seconds and remain understandable without animation or color.
- Retry-now may be shown only if it does not extend the fixed deadline; while an attempt is active it is disabled with `Reconnect attempt in progress`.
- Focus defaults to `Leave session`. Keyboard/controller Confirm opens `Leave session? Your reserved players will be removed now and reconnect will stop.` Confirm enters `NET-01`; Cancel continues reconnect against the unchanged deadline. Escape/controller Back does not bypass confirmation.
- No Pause, host migration, or manual identity/account action may appear.
- No silence, refusal, unreachable, reset, temporary failure, timeout, or isolation-only state may use `NET-09` or claim that the host ended the session.

Planned representative screenshot: [`SS-021`](../screenshots/README.md#ss-021).
