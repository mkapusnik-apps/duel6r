# NET-08 — Connection or session failure

## Status, purpose, and requirements

`NET-08-password-rejected` uses `Connection not authorized.` for either hosting mode and permits Edit setup to correct the common Password in the same context under NET-PASS-003/004/011–013. Permitted endpoint, protection, selected binding and local-player setup remain retained; the editable correction buffer is not cleared merely because initial password authorization failed. `NET-08-admission-closed` retains identifier `match-already-started` with `Match completed. Join when the host returns to the lobby.` for final-outcome closure, not a first-round cutoff or blanket join-in-progress rejection. Successful Connected-waiting admission is not failure. Browser-origin failures permit return to `NET-10` for refresh. Directory read failures remain there and do not imply game end. Functional acceptance uses NET-DIR-AC-003, NET-PASS-AC-001–005 and NET-ADM-AC-003.

### Public pilot functional contract

These public states extend the LAN outcomes below. They apply to both the controller and guests and consume the owning security and lifecycle requirements without redefining their copy.

Presentation authority: [NET-08 UX contract](../design/screens/NET-08.md). NET-PASS-004, TRU-PUB-016, HSL-PUB-006 and HSL-PUB-007 own fixed copies. New protocol/code identifiers are developer-owned; existing functional state IDs remain stable. Certificate/identity/encryption failure stays separate from common Password rejection and offers Edit setup and contextual return without Retry or bypass. Selected dedicated correction keeps endpoint/session/type locked and focuses Password after Edit setup; initial browser-origin recovery also permits NET-10 refresh. Eligible secure Retry for other outcomes retains existing input/binding checks. SIGTERM without an authenticated terminal notice never selects maintenance copy.

| Functional state | Entry and outcome source | Permitted recovery |
|---|---|---|
| `NET-08-PUBLIC-SECURITY` | Server identity/encryption failure under TRU-PUB-016 | Edit setup to NET-03; Return to Network to NET-01; browser-origin return to NET-10 for refresh. No insecure bypass or direct Retry; no password exchange before certificate validation. |
| `NET-08-password-rejected` | Initial common Password failure under NET-PASS-004, either hosting mode | `Connection not authorized.`; Edit setup to correct Password in the same permitted bound/direct context, retaining local choices; contextual return to Network/browser. No credential disclosure or unlocked fallback. |
| `NET-08-PUBLIC-MAINTENANCE` | Confirmed service maintenance notice under HSL-PUB-007 | HSL-PUB-008; no restoration Retry. |
| `NET-08-PUBLIC-CONTROLLER-EXPIRED` | Confirmed service expiry notice under HSL-PUB-006 | HSL-PUB-008; no restoration Retry. |

Missing DNS, unreachable service and timeouts retain existing initial-connection outcomes. They must not be presented as successful deployment, admission or a known session end. An unconfirmed connection loss remains NET-07 until an authoritative terminal result or the original deadline. Edit setup retains permitted setup and common correction under NET-JOIN-PUB-006 and NET-PASS-013. No failure displays passwords or reconnect credentials. Endpoint/port/mode or listing/session changes erase the old context's password; terminal reconnect failure erases retained channel state.

Initial browser-selected dedicated failures retain bound hosting/security mode, operator frontend endpoint and expected session identity for credential correction or eligible secure Retry. They permit contextual return to NET-10 and refresh. Missing/unknown/inconsistent metadata or stale/wrong/ended/restarted identity must not allocate authority, redirect a fresh session or offer an insecure admission mode. Existing fixed error copy and precedence remain unchanged; no new peer-derived error or speculative cause is introduced. Endpoint/selection changes require refreshed selection, or an explicit unbound Direct transition that clears binding and secrets. Directory publication failure is an operator condition and does not itself select a participant session-failure outcome or grant controller publication actions.

Functional acceptance: TRU-PUB-AC-001/003, NET-JOIN-PUB-AC-001/002, NET-PASS-AC-001–005 and HSL-PUB-AC-002 cover rejection, recovery permissions, copy and truthful failure states. Retired invitation-only criteria grant no current authority. UX owns presentation and feedback.

This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It gives an actionable and truthful outcome for startup/initial-connection failures and terminal reconnect outcomes. It implements `NET-AC-002`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 defines the compatibility and admission outcomes for this screen in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.
Issue #31 defines host startup and post-readiness service outcomes for this screen in [`docs/network-host-service-lifecycle.md`](../network-host-service-lifecycle.md).
Issue #31 must not implement this graphical screen.
Issue #32 defines authoritative match failures for this screen in [`docs/network-authoritative-headless-match.md`](../network-authoritative-headless-match.md).
Issue #32 must not implement this graphical screen.
Issue #38 owns its graphical controls, focus, disabled reasons, and visual evidence.

Host startup or initial Join failure enters from `NET-02` or `NET-03`; terminal reconnect rejection or deadline expiry also enters here. The host application's local supervisor routes only the host here when its hosted service stops unexpectedly. Retry repeats a retained initial attempt when valid; Edit setup returns to editable `NET-02` or `NET-03`; Return to Network enters `NET-01`. Only an accepted intentional host End notice uses guest `NET-09`.

## Representative layout

- Use the scaled retro canvas with `CONNECTION FAILED` or `SESSION ENDED` as the textual state.
- Show the attempted endpoint for a guest initial-connection failure.
- Do not show an endpoint for a host-service lifecycle failure.
- The representative state is `Host unreachable.` for the preserved direct endpoint.
- Show a short recovery instruction, then Retry when valid, `Edit setup`, and `Return to Network`.
- Keep unsupported-feature marketing or speculative diagnostic controls out of the screen.
- Keep the failure panel inside the shared 24-logical-pixel canvas margin.
- Use a fixed state heading, a flexible reason and instruction region, and a fixed recovery-action region.
- Keep the complete confirmed reason visible and wrap it at word boundaries.
- Keep an entered guest endpoint inside a bounded context row.
- Break an unspaced endpoint at a character boundary when it does not fit.
- Keep Retry, Edit setup, and Return to Network in one row when they fit.
- Wrap recovery actions to a second row in reading order before captions clip or actions overlap.
- Keep every enabled recovery action visible when the reason region scrolls.

## Significant variants

- Endpoint validation remains inline in `NET-03` and does not enter this screen.
- Guest-local manifest validation shares the initial 10-second attempt. A specific invalid result established before the deadline enters this screen without resolution or connection; otherwise the generic attempt timeout applies.
- Initial admission uses the fixed malformed, trust/authorization, release, invalid-manifest, content, match-started, capacity, policy, and success order. Initial transport failure uses the fixed name-resolution, unreachable/refusal, incomplete-response, and timeout precedence.
- A malformed, trailing, unexpected, or semantically inconsistent complete host offer, rejection, or final confirmation uses `invalid-host-admission-message` and exactly `Connection ended before admission completed.`. An undelivered partial frame remains subject to close or the total deadline.
- Reconnect terminal variants use only fixed non-disclosing authorization, missing-reservation, release, content, or expiry copy. `Reconnect time expired. The session could not be restored.` never claims host end or player removal.
- Host-local supervised failure uses exactly `Hosted session stopped unexpectedly.` It is shown only to the hosting application and is never evidence or copy sent to guests.
- Host startup and complete guest connection have separate 10-second boundaries. A specific reason confirmed before the applicable deadline replaces generic timeout.
- Host startup Retry must remain disabled with `Cleanup in progress.` until final cleanup completes.
- Eligible host startup Retry must repeat the retained setup as a new 10-second attempt.
- Host startup Retry must be enabled only when no owned service or listener remains, retained setup remains valid, the outcome does not require restart, and no active session ended.
- Retry is disabled with a reason when required input is no longer valid or the session no longer accepts admission.
- Compatibility mismatch states use the exact release/content copy in the product specification and do not offer negotiation.
- Retry is disabled for terminal reconnect outcomes when the original reservation cannot restore. Edit setup may begin a new initial journey but must not be labeled as reconnect Retry.
- Only an accepted intentional host End notice routes guests to `NET-09`; every unexpected host failure remains guest `NET-07` until terminal rejection or deadline expiry.

## Authoritative match outcome mapping

| Machine identifier | Exact service copy | Future graphical destination and actions |
|---|---|---|
| `authoritative-match-settings-invalid` | `Match settings are invalid. Correct the settings and try again.` | Stay in editable `NET-04`; clear readiness; allow the host to correct settings and request Start match later. |
| `authoritative-match-content-unavailable` | `The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.` | Show a blocking failure state from `NET-04`; clear readiness; disable Start match for this hosted session; keep host-only End session available. |
| `authoritative-match-runtime-failed` | `The authoritative match stopped unexpectedly.` | Stop match progression; publish no result; map the host application to `NET-08` with `Hosted session stopped unexpectedly.`; keep guests in `NET-07` until a terminal reconnect result or deadline expiry. |
| `authoritative-match-shutdown-failed` | `Authoritative match cleanup did not complete.` | Issue #32 defines no graphical destination or action. The headless process exits with status `4` and publishes no match result. Issue #38 must not invent a recovery action without an approved lifecycle mapping. |

- The graphical UI must not show the service copy and host-supervisor copy as two failures for one event.
- Runtime failure, cleanup failure, and host End session must not publish a completed or interrupted result.
- Runtime failure and cleanup failure must not imply intentional host end.
- A cleanup failure must replace an earlier process result.
- Issue #38 must preserve the exact copy when an approved requirement maps these outcomes to graphical states.

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
| 8 | `match-already-started` | `Match completed. Join when the host returns to the lobby.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`; browser-origin recovery → `NET-10` for refresh. |
| 9 | `session-full` | `Session is full.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 10 | `host-policy-rejected` | `Host rejected the connection.` | Retry the retained initial attempt when it is still valid; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 11 | `admitted` | No rejection copy. | Enter current `NET-04` or `NET-05`, including waiting, only after complete confirmation/calibration/snapshot validation; do not show `NET-08`. |

Without a complete host response, the runtime must use the first applicable transport result in this order.

| Order | Condition | Exact visible copy | Recovery mapping |
|---:|---|---|---|
| 1 | Name-resolution failure | `Host name could not be resolved.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 2 | Unreachable or refused connection | `Host unreachable.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 3 | Reset or close before complete admission | `Connection ended before admission completed.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |
| 4 | No complete result at the deadline | `Connection timed out.` | Retry the retained attempt; Edit setup → retained `NET-03`; Return to Network → `NET-01`. |

A complete valid rejection or exact final confirmation that the guest accepts before the deadline must take precedence over a later transport symptom. An admission offer alone must not report success.
User Cancel and local inline validation must take precedence over every result in these tables.

## Host and reconnect outcome mapping

| Condition | Exact visible copy | Recovery mapping |
|---|---|---|
| Invalid host manifest | `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.` | Disable Retry for the current application session; Edit setup → retained `NET-02`; Return to Network → `NET-01`; leave no listener or session. |
| Invalid guest-local manifest | `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.` | Disable Retry until application restart; Edit setup → retained `NET-03`; Return to Network → `NET-01`; perform no resolution or connection. |
| Host port unavailable | `The selected port is unavailable. Choose another port and try again.` | Enable Retry after cleanup when retained setup remains valid; Edit setup → retained `NET-02`; Return to Network → `NET-01`. |
| Generic host startup failure | `Hosted session could not start.` | Enable Retry after cleanup when retained setup remains valid; Edit setup → retained `NET-02`; Return to Network → `NET-01`. |
| Host service exited before readiness | `Hosted session stopped before it was ready.` | Enable Retry after cleanup when retained setup remains valid; Edit setup → retained `NET-02`; Return to Network → `NET-01`. |
| Host startup timeout | `Hosted session startup timed out.` | Enable Retry after cleanup when retained setup remains valid; Edit setup → retained `NET-02`; Return to Network → `NET-01`. |
| Host-local supervised service failure after readiness | `Hosted session stopped unexpectedly.` | Disable Retry; show `This ended session cannot be restored. Edit setup to start a new session.`; Edit setup → retained `NET-02`; Return to Network → `NET-01`; never use this outcome as guest evidence. |
| Terminal reconnect authorization failure | `Reconnect authorization failed. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect missing reservation or removed participant | `Reconnect reservation is no longer available. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect protocol, release, or capability mismatch | `Network release mismatch. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Terminal reconnect invalid manifest or content mismatch | `Gameplay content mismatch. This session cannot be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |
| Reconnect deadline expiry | `Reconnect time expired. The session could not be restored.` | Disable reconnect Retry; Edit setup may start a new initial `NET-03` journey; Return to Network → `NET-01`. |

## Truthful copy, focus, and input

- Never reduce every failure to `Disconnected`; use the most specific confirmed reason available.
- Focus defaults to Retry when enabled, otherwise Edit setup or Return to Network. Disabled reconnect Retry remains readable with `This session cannot be restored.` Edit setup and Return to Network remain distinct destinations.
- A port-unavailable host outcome must focus Edit setup by default.
- Other eligible host startup outcomes must focus Retry by default.
- A post-readiness hosted-service failure must focus Edit setup by default.
- Keyboard Tab or directional controller input traverses available actions; Enter/Space/controller Confirm activates; Escape/controller Back returns to `NET-01`.
- Copy and focus must not rely only on red color or transient motion.
- The complete reason must remain visible as text until the user selects a recovery action.
- A disabled Retry control must remain readable and must show its textual reason.
- Retry disabled during cleanup must show `Cleanup in progress.`.
- Retry disabled because setup is invalid must show `Edit setup before you retry.`.
- Retry disabled because application restart is required must show `Restart the application to try again.`.
- Retry disabled after an active session ends must show `This ended session cannot be restored. Edit setup to start a new session.`.
- The focus order must follow Retry when enabled, Edit setup, and Return to Network.
- The focus order must skip Retry when Retry is disabled.
- The screen must not expose a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.
- A host-service lifecycle outcome must not expose an endpoint, process value, command, credential, filesystem path, payload, or operating-system error text.

Planned representative screenshot: [`SS-022`](../screenshots/README.md#ss-022).
