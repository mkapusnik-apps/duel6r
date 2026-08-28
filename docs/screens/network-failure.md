# NET-08 — Connection or session failure

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It gives an actionable and truthful outcome for startup/initial-connection failures and terminal reconnect outcomes. It implements `NET-AC-002`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 defines the compatibility and admission outcomes for this planned screen in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.

Host startup or initial Join failure enters from `NET-02` or `NET-03`; terminal reconnect rejection or deadline expiry also enters here. The host application's local supervisor routes only the host here when its hosted service stops unexpectedly. Retry repeats a retained initial attempt when valid; Edit setup returns to editable `NET-02` or `NET-03`; Return to Network enters `NET-01`. Only an accepted intentional host End notice uses guest `NET-09`.

## Representative layout

- Use the scaled retro canvas with `CONNECTION FAILED` or `SESSION ENDED` as the textual state.
- Name the attempted endpoint and show one concrete reason.
- The representative state is `Host unreachable.` for the preserved direct endpoint.
- Show a short recovery instruction, then Retry when valid, `Edit setup`, and `Return to Network`.
- Keep unsupported-feature marketing or speculative diagnostic controls out of the screen.

## Significant variants

- Endpoint validation remains inline in `NET-03` and does not enter this screen.
- Guest-local manifest validation shares the initial 10-second attempt. A specific invalid result established before the deadline enters this screen without resolution or connection; otherwise the generic attempt timeout applies.
- Initial admission uses the fixed malformed, trust/authorization, release, invalid-manifest, content, match-started, capacity, policy, and success order. Initial transport failure uses the fixed name-resolution, unreachable/refusal, incomplete-response, and timeout precedence.
- Reconnect terminal variants use only fixed non-disclosing authorization, missing-reservation, release, content, or expiry copy. `Reconnect time expired. The session could not be restored.` never claims host end or player removal.
- Host-local supervised failure uses exactly `Hosted session stopped unexpectedly.` It is shown only to the hosting application and is never evidence or copy sent to guests.
- Host startup and complete guest connection have separate 10-second boundaries. A specific reason confirmed before the applicable deadline replaces generic timeout.
- Retry is disabled with a reason when required input is no longer valid or the session no longer accepts admission.
- Compatibility mismatch states use the exact release/content copy in the product specification and do not offer negotiation.
- Retry is disabled for terminal reconnect outcomes when the original reservation cannot restore. Edit setup may begin a new initial journey but must not be labeled as reconnect Retry.
- Only an accepted intentional host End notice routes guests to `NET-09`; every unexpected host failure remains guest `NET-07` until terminal rejection or deadline expiry.

## Exact initial outcome mapping

Before any host result, local guest content may produce `guest-gameplay-content-manifest-invalid` with `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.` Retry is disabled until restart; Edit setup returns to retained `NET-03`; Return to Network enters `NET-01`.

The runtime must stop at the first applicable complete host result in the table order.

| Order | Machine identifier | Exact visible copy | Recovery mapping |
|---:|---|---|---|
| 1 | `malformed-request` | `Connection request rejected.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 2 | `not-authorized` | `Connection not authorized.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 3 | `protocol-incompatible` | `Network release mismatch. Use the same supported game release as the host.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 4 | `network-release-mismatch` | `Network release mismatch. Use the same supported game release as the host.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 5 | `required-capability-unsupported` | `Network release mismatch. Use the same supported game release as the host.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 6 | `gameplay-content-manifest-invalid` | `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 7 | `gameplay-content-mismatch` | `Gameplay content mismatch. Use the host's exact supported gameplay content.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 8 | `match-already-started` | `Match already started. Join-in-progress is not supported.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 9 | `session-full` | `Session is full.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 10 | `host-policy-rejected` | `Host rejected the connection.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 11 | `admitted` | No rejection copy. | Enter `NET-04`; do not show `NET-08`. |

Without a complete host response, the runtime must use the first applicable transport result in this order.

| Order | Condition | Exact visible copy | Recovery mapping |
|---:|---|---|---|
| 1 | Name-resolution failure | `Host name could not be resolved.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 2 | Unreachable or refused connection | `Host unreachable.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 3 | Reset or close before complete admission | `Connection ended before admission completed.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 4 | No complete result at the deadline | `Connection timed out.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |

A complete host response that the guest accepts before the deadline must take precedence over a later transport symptom.
User Cancel and local inline validation must take precedence over every result in these tables.

## Host and reconnect outcome mapping

| Condition | Exact visible copy | Recovery mapping |
|---|---|---|
| Invalid host manifest | `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.` | Disable Retry for the current application session; Edit setup → retained `NET-02`; Return to Network → `NET-01`; leave no listener or session. |
| Invalid guest-local manifest | `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.` | Disable Retry until application restart; Edit setup → retained `NET-03`; Return to Network → `NET-01`; perform no resolution or connection. |
| Host-local supervised service failure | `Hosted session stopped unexpectedly.` | Disable Retry when no valid retained initial attempt exists; Edit setup → retained `NET-02`; Return to Network → `NET-01`; never use this outcome as guest evidence. |
| Terminal reconnect authorization failure | `Reconnect authorization failed. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect missing reservation or removed participant | `Reconnect reservation is no longer available. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect protocol, release, or capability mismatch | `Network release mismatch. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect invalid manifest or content mismatch | `Gameplay content mismatch. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Reconnect deadline expiry | `Reconnect time expired. The session could not be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |

## Truthful copy, focus, and input

- Never reduce every failure to `Disconnected`; use the most specific confirmed reason available.
- Focus defaults to Retry when enabled, otherwise Edit setup or Return to Network. Disabled reconnect Retry remains readable with `This session cannot be restored.` Edit setup and Return to Network remain distinct destinations.
- Keyboard Tab or directional controller input traverses available actions; Enter/Space/controller Confirm activates; Escape/controller Back returns to `NET-01`.
- Copy and focus must not rely only on red color or transient motion.
- The complete reason must remain visible as text until the user selects a recovery action.
- A disabled Retry control must remain readable and must show its textual reason.
- The focus order must follow Retry when enabled, Edit setup, and Return to Network.
- The focus order must skip Retry when Retry is disabled.
- The screen must not expose a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.

Planned representative screenshot: [`SS-022`](../screenshots/README.md#ss-022).
