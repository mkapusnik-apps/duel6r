# NET-08 — Connection or session failure

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It gives an actionable and truthful outcome for recoverable startup, connection, or reconnect failures. It implements `NET-AC-002`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host startup or Join failure enters from `NET-02` or `NET-03`; guest reconnect expiry may also enter here. Retry repeats the retained valid attempt; Edit setup returns to editable `NET-02` or `NET-03` with data retained; Return to Network enters `NET-01`. Host end/loss uses `NET-09`, not this generic screen.

## Representative layout

- Use the scaled retro canvas with `CONNECTION FAILED` or `SESSION ENDED` as the textual state.
- Name the attempted endpoint and show one concrete reason. The representative state is `Host unreachable` for the preserved direct endpoint.
- Show a short recovery instruction, then Retry when valid, `Edit setup`, and `Return to Network`.
- Keep unsupported-feature marketing or speculative diagnostic controls out of the screen.

## Significant variants

- Endpoint validation remains inline in `NET-03` and does not enter this screen.
- Distinct reasons include host startup failure, host unreachable, connection timeout, session full, host rejection, exact release mismatch, exact gameplay-content mismatch, join-in-progress prohibited, transport failure, and reconnect expiry. Host-ended/loss states use `NET-09`.
- Host startup and complete guest connection have separate 10-second boundaries. A specific reason confirmed before the applicable deadline replaces generic timeout.
- Retry is disabled with a reason when required input is no longer valid or the session no longer accepts admission.
- Compatibility mismatch states use the exact release/content copy in the product specification and do not offer negotiation.
- Reconnect expiry states that reserved players were removed and does not claim that the match paused.
- Host end or loss must route to the distinct `NET-09` variant so no migration or resumability is implied.

## Truthful copy, focus, and input

- Never reduce every failure to `Disconnected`; use the most specific confirmed reason available.
- Focus defaults to Retry when enabled, otherwise Edit setup. Disabled Retry remains readable with its reason. Edit setup and Return to Network remain distinct destinations.
- Keyboard Tab or directional controller input traverses available actions; Enter/Space/controller Confirm activates; Escape/controller Back returns to `NET-01`.
- Copy and focus must not rely only on red color or transient motion.

Planned representative screenshot: [`SS-022`](../screenshots/README.md#ss-022).
