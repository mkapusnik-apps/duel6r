# NET-08 — Connection or session failure

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It gives an actionable and truthful outcome for recoverable connection or session failures. It implements `NET-AC-002`, `NET-AC-007`–`NET-AC-009`, and `NET-AC-011`–`NET-AC-014` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host startup or Join failure enters from `NET-02` or `NET-03`; guest reconnect expiry may also enter here. Retry returns to the applicable configured setup or performs the same valid attempt; Return to Network enters `NET-01`. Host loss uses `NET-09`, not this generic screen.

## Representative layout

- Use the scaled retro canvas with `CONNECTION FAILED` or `SESSION ENDED` as the textual state.
- Name the attempted endpoint and show one concrete reason. The representative state is `Host unreachable` for the preserved direct endpoint.
- Show a short recovery instruction, then Retry when valid and `Return to Network`.
- Keep unsupported-feature marketing or speculative diagnostic controls out of the screen.

## Significant variants

- Distinct reasons include invalid endpoint, host startup failure, host unreachable, timeout, session full, host rejection, exact-release mismatch, content mismatch, join-in-progress prohibited, transport failure, reconnect expired, and host-ended session.
- Retry is disabled with a reason when required input is no longer valid or the session no longer accepts admission.
- Compatibility mismatch states direct the user to use the exact supported release/content; they do not offer negotiation.
- Reconnect expiry states that reserved players were removed and does not claim that the match paused.
- Host loss must route to `NET-09` so no migration or resumability is implied.

## Truthful copy, focus, and input

- Never reduce every failure to `Disconnected`; use the most specific confirmed reason available.
- Focus defaults to Retry when enabled, otherwise Return to Network. Disabled Retry remains readable with its reason.
- Keyboard Tab or directional controller input traverses available actions; Enter/Space/controller Confirm activates; Escape/controller Back returns to `NET-01`.
- Copy and focus must not rely only on red color or transient motion.

Planned representative screenshot: [`SS-022`](../screenshots/README.md#ss-022).
