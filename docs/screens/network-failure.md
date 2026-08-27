# NET-08 — Connection or session failure

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It gives an actionable and truthful outcome for startup/initial-connection failures and terminal reconnect outcomes. It implements `NET-AC-002`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host startup or initial Join failure enters from `NET-02` or `NET-03`; terminal reconnect rejection or deadline expiry also enters here. Retry repeats a retained initial attempt when valid; Edit setup returns to editable `NET-02` or `NET-03`; Return to Network enters `NET-01`. Valid host end or independently definitive termination uses `NET-09`.

## Representative layout

- Use the scaled retro canvas with `CONNECTION FAILED` or `SESSION ENDED` as the textual state.
- Name the attempted endpoint and show one concrete reason. The representative state is `Host unreachable` for the preserved direct endpoint.
- Show a short recovery instruction, then Retry when valid, `Edit setup`, and `Return to Network`.
- Keep unsupported-feature marketing or speculative diagnostic controls out of the screen.

## Significant variants

- Endpoint validation remains inline in `NET-03` and does not enter this screen.
- Initial admission uses the fixed malformed, trust/authorization, release, invalid-manifest, content, match-started, capacity, policy, and success order. Initial transport failure uses the fixed name-resolution, unreachable/refusal, incomplete-response, and timeout precedence.
- Reconnect terminal variants use only fixed non-disclosing authorization, missing-reservation, release, content, or expiry copy. `Reconnect time expired. The session could not be restored.` never claims host end or player removal.
- Host startup and complete guest connection have separate 10-second boundaries. A specific reason confirmed before the applicable deadline replaces generic timeout.
- Retry is disabled with a reason when required input is no longer valid or the session no longer accepts admission.
- Compatibility mismatch states use the exact release/content copy in the product specification and do not offer negotiation.
- Retry is disabled for terminal reconnect outcomes when the original reservation cannot restore. Edit setup may begin a new initial journey but must not be labeled as reconnect Retry.
- Valid host end or independently definitive termination routes to distinct `NET-09`; ambiguous isolation remains `NET-07` until deadline.

## Truthful copy, focus, and input

- Never reduce every failure to `Disconnected`; use the most specific confirmed reason available.
- Focus defaults to Retry when enabled, otherwise Edit setup or Return to Network. Disabled reconnect Retry remains readable with `This session cannot be restored.` Edit setup and Return to Network remain distinct destinations.
- Keyboard Tab or directional controller input traverses available actions; Enter/Space/controller Confirm activates; Escape/controller Back returns to `NET-01`.
- Copy and focus must not rely only on red color or transient motion.

Planned representative screenshot: [`SS-022`](../screenshots/README.md#ss-022).
