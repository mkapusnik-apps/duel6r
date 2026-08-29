# Player-hosted service lifecycle

## Status and authority

This document is the authoritative product target for GitHub issue #31. It defines supervision of the first-release player-hosted service.

This document defines target behavior, not implemented or playable network support. The current status remains in [`networking.md`](networking.md).

The approved scope and journeys are in [`network-play-first-release.md`](network-play-first-release.md). This document must not change those boundaries.

The production transport contract is in [`networking.md`](networking.md). Compatibility and admission behavior is in [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md).

The trust and resource policy is in [`network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md). This document must not weaken those contracts.

Local Play remains subject to [`features.md`](features.md). This document does not change Local Play behavior.

## Terms

- **Host application:** The game instance in which the host starts and controls a player-hosted session.
- **Hosted service:** The separate service that owns the authoritative session and its listening endpoint.
- **Owned service:** A hosted service that the host application started and must supervise until final cleanup.
- **Startup attempt:** The period from Start session activation until readiness, cancellation, or a startup failure.
- **Readiness:** Confirmation that all readiness conditions in this document are true.
- **Stop request:** A request to end an owned service and release all service resources.
- **Retained setup:** The valid host endpoint, local-player setup, and other editable values from `NET-02`.

## Ownership model

- One host application must own no more than one hosted service at one time.
- The host application must create the hosted service only after the user activates Start session.
- The host application must supervise the owned service until the application confirms final cleanup.
- The host application must not adopt an unrelated or previously running service as its owned service.
- A startup retry must create a new startup attempt after cleanup of the prior attempt.
- A stale readiness or failure result from a prior attempt must not change the current attempt.
- The hosted service must own the authoritative session after readiness.
- The hosted service must own the listener, host participant, admitted participants, roster ownership, and authoritative session state.
- The host application must own presentation, local input collection, the host journey, and supervision status.
- A guest must not gain host authority through a service lifecycle message.

The host application must not report cleanup while an owned service or its owned service resources remain active.

## Host service state machine

The host application must use these product states:

| State | Meaning | Permitted next states |
|---|---|---|
| `No service` | No owned service or listener exists. Host setup can be edited. | `Starting`, `Application exit` |
| `Starting` | One owned startup attempt is active. Setup is locked. | `Active`, `Stopping`, `Startup failed` |
| `Active` | Readiness is confirmed. One authoritative hosted session exists. | `Stopping`, `Session failed` |
| `Stopping` | The host application requested service cleanup. | `No service`, `Application exit` |
| `Startup failed` | Startup ended without readiness. No owned service or listener remains. | `Starting`, `No service`, `Application exit` |
| `Session failed` | An active service stopped without an applicable stop request. No hosted session remains. | `No service`, `Application exit` |
| `Application exit` | Application shutdown is in progress. | None |

The state names are product states. They do not prescribe implementation names or a process-control design.

### Start

Start session must be available only when the retained setup is valid. The host must own from 1 through 15 valid local players.

Start session must move `No service` or an eligible `Startup failed` state to `Starting`. It must start one new startup deadline.

The host application must not start another attempt while `Starting`, `Active`, or `Stopping`.

### Active session

Readiness must move `Starting` to `Active` only when all readiness conditions are true strictly before the deadline.

The host application must ignore readiness after cancellation, failure selection, timeout selection, or application-exit selection.

An unrequested hosted-service stop in `Active` must move the host application to `Session failed`.

### Stop

Cancel during `Starting` must move the attempt to `Stopping`. Confirmed End session during `Active` must also move the session to `Stopping`.

Normal application shutdown from `Starting` or `Active` must move directly toward `Application exit` through bounded service cleanup.

The host application must complete cleanup within three seconds after a stop request. At the deadline, it must stop any remaining owned service work.

The host application must release the owned endpoint, service work, and queued service data before it reaches `No service` or completes exit.

Repeated stop requests must keep the first stop deadline. They must not create another service or extend cleanup.

## Readiness contract

The hosted service is ready only when all these conditions are true:

1. The hosted service is running and remains under host-application supervision.
2. The host compatibility claim satisfies the issue #30 contract.
3. The host gameplay-content manifest is valid and frozen for the session.
4. The host participant and all host player identities are stable, unique, and nonzero.
5. The production listener has bound the approved endpoint.
6. The listener is accepting production transport connections.
7. The service can apply the approved compatibility and admission contract to a new connection.
8. No cancellation, shutdown, failure, or deadline outcome has already won.

Process creation alone is not readiness. A bound endpoint without active accept processing is not readiness.

Transport connection alone is not participant admission. Readiness must not claim that a guest is connected, compatible, admitted, or playable.

The host application must enter `NET-04` only after it accepts one complete readiness result for the current startup attempt.

## Startup deadline and outcome precedence

Start session activation begins one 10-second deadline on the host session clock. Readiness must complete strictly before that deadline.

Readiness at or after the deadline must fail as `host-service-startup-timed-out`. The host application must leave no listener or session.

Before readiness, the host application must select the first applicable outcome in this order:

1. User Cancel accepted before another terminal result.
2. Application shutdown accepted before another terminal result.
3. A specific issue #30 host compatibility failure confirmed before the deadline.
4. A port conflict confirmed before the deadline.
5. A hosted-service early exit confirmed before the deadline.
6. Another specific startup failure confirmed before the deadline.
7. Readiness confirmed strictly before the deadline.
8. No complete result at the deadline.

Cancel is not a failure. It must return to editable `NET-02` after cleanup.

A complete specific result confirmed before the deadline must not change to timeout because cleanup completes later.

An invalid host manifest must keep `host-gameplay-content-manifest-invalid` and its issue #30 behavior. This document does not replace that identifier.

## Fixed outcomes and copy

The following machine identifiers and visible copy are exact and case-sensitive:

| Condition | Machine identifier | Exact visible copy | Retry |
|---|---|---|---|
| The service cannot start for a confirmed reason other than the listed specific outcomes. | `host-service-start-failed` | `Hosted session could not start.` | Enabled after cleanup when retained setup remains valid. |
| The selected endpoint cannot bind because its port is unavailable. | `host-service-port-unavailable` | `The selected port is unavailable. Choose another port and try again.` | Enabled after cleanup when retained setup remains valid. |
| The hosted service exits before readiness and before an applicable stop request. | `host-service-exited-before-ready` | `Hosted session stopped before it was ready.` | Enabled after cleanup when retained setup remains valid. |
| No complete startup result exists at the 10-second deadline. | `host-service-startup-timed-out` | `Hosted session startup timed out.` | Enabled after cleanup when retained setup remains valid. |
| The hosted service stops after readiness without an applicable stop request. | `host-service-stopped-unexpectedly` | `Hosted session stopped unexpectedly.` | Disabled. The ended session cannot be restored by startup Retry. |

The host application must use `host-service-port-unavailable` only for a confirmed local bind conflict. Other bind failures use `host-service-start-failed`.

The copy must not contain an endpoint, process value, filesystem path, command, credential, peer value, payload, or operating-system error text.

Trusted local diagnostics may contain an enumerated failure category. They must follow the redaction rules in the trust policy.

## Retained setup, Retry, and destinations

Cancel, startup failure, and startup timeout must retain all editable `NET-02` setup. This setup includes the endpoint and local-player configuration.

Retry must repeat the retained startup attempt only after final cleanup. Retry must use the same 10-second startup deadline rules.

Retry must be enabled only when all these conditions are true:

- no owned service or listener remains;
- retained setup remains locally valid;
- the outcome does not require application restart;
- the outcome did not end a previously active session.

The invalid-host-manifest outcome must keep Retry disabled for the current application session, as required by issue #30.

After `host-service-stopped-unexpectedly`, Retry must be disabled. `Edit setup` must return to retained `NET-02` for a new session.

`Edit setup` must return to editable `NET-02` with retained setup. `Return to Network` must enter `NET-01` after cleanup.

The product must not label a new startup attempt as restoration of an ended session.

## End session, application shutdown, and failure

### Intentional End session

Only the confirmed host action End session is an intentional host end. It must use the confirmation and destinations in the first-release specification.

After confirmation, the hosted service must attempt to send the valid intentional host-end notice through each current established guest session.

The host must then stop the hosted service and return to `NET-01`. The host must discard session-only results.

A guest may enter `NET-09` only after it accepts the valid notice through its current established session.

Failure to deliver the notice must not delay host cleanup beyond three seconds. An affected guest must use the issue #36 ambiguous reconnect journey.

### Normal application shutdown

Normal application shutdown is not the End session action. It must not create or imply a valid intentional host-end notice.

The host application must stop and clean the owned service before application exit completes. It must discard session-only results.

Guests must treat lost contact as ambiguous. They must remain subject to the issue #36 reconnect deadline and outcomes.

### Host application crash or forced termination

A host application crash is not an intentional host end. It must not create a guest-visible host-end claim.

The owned service must not remain after the host application terminates. The product must not depend on normal application cleanup for this guarantee.

Guests must treat the resulting loss of contact as ambiguous. The future issue #36 behavior must apply.

### Hosted-service failure after readiness

An unrequested stop after readiness must end the authoritative hosted session. The host must enter `NET-08` with `host-service-stopped-unexpectedly`.

The host must discard the session-only result set. The host must not restart the service automatically.

The host must not send or synthesize an intentional host-end notice after this failure. Guests must use the issue #36 ambiguous reconnect behavior.

## Service supervision and orphan prevention

- The host application must detect service creation failure, readiness, early exit, normal exit, and unexpected exit.
- The host application must associate each service result with exactly one current startup attempt or active session.
- The host application must reject stale, duplicate, malformed, or unrelated service status without changing the current state.
- Cancellation and shutdown must prevent a later readiness result from entering `Active`.
- The hosted service must not continue listening after startup cancellation, startup failure, timeout, intentional End session, or application shutdown.
- The hosted service must not continue after host application crash or forced termination.
- The application must not report a completed cancellation, return destination, or completed exit while an owned service remains.

This specification defines required outcomes. It does not prescribe an operating-system supervision mechanism.

## Secret and configuration handling

First release introduces no startup password, authentication token, certificate, or reconnect credential.

The host application may provide only validated, non-secret startup configuration to the hosted service. It must not place a secret in startup arguments.

The product must not place a secret in an environment value, generated command, log, diagnostic, crash output, or user-visible copy.

Future issue #36 reconnect credentials must not become startup configuration. The hosted service must create and manage them under the trust policy.

If a later approved feature requires a host-service secret, that feature must define a separate safe transfer contract before implementation.

## Local Play independence

- `Play (F1)` must not create, find, adopt, start, stop, or supervise a hosted service.
- Local Play must not bind a listener or wait for hosted-service readiness.
- Local Play must start and complete without network availability.
- A network startup failure or retained network setup must not change Local Play.
- Exiting Local Play must not run hosted-service cleanup when the application owns no hosted service.
- This document must not change local profiles, scripts, controls, statistics, Elo, saves, or match behavior.

## Downstream handoff

- Issue #30 owns host compatibility validation, manifest freezing, host identity assignment, and invalid-host-manifest behavior before readiness.
- Issue #36 owns guest disconnect detection, reservations, reconnect credentials, retries, expiry, restoration, and guest destinations.
- Issue #38 owns graphical presentation, focus, actions, disabled reasons, and visual evidence for the states and copy in this document.
- Issue #40 owns artifact inclusion and supported deployment instructions.
- Issue #41 owns complete release-candidate validation.

Issue #36 must not treat normal application shutdown, host crash, forced termination, or service failure as an intentional host end.

Issue #38 must not show `NET-04`, listening, ready, connected, admitted, or playable before the applicable runtime confirmation.

## Visual impact

This issue has visual impact because it defines pending, cancellation, failure, Retry, and destination states with fixed visible copy.

Issue #38 owns graphical implementation. UX must translate these states without changing their behavior, copy, precedence, or destinations.

The affected target states are `NET-02` and `NET-08`. This document does not change their UX-owned layouts or wireframes.

## Non-goals

- Graphical implementation of `NET-02`, `NET-04`, or `NET-08`.
- Dedicated-server startup, packaging, operation, or adoption.
- Automatic service restart or restoration of an ended session.
- Host migration.
- Guest reconnect implementation or reconnect credential exchange.
- Gameplay simulation, replication, remote input, scoring, or persistence implementation.
- Internet, NAT traversal, discovery, matchmaking, accounts, authentication, encryption, or public hosting.
- Changes to Local Play.
- A playable-networking or release-readiness claim.

## Acceptance criteria

- **HSL-AC-001 — Ownership:** One host application must own no more than one supervised hosted service and must not adopt an unrelated service.
- **HSL-AC-002 — Explicit start:** Only Start session must create a hosted service attempt. Local Play must create none.
- **HSL-AC-003 — State transitions:** Every start, ready, cancel, failure, End session, shutdown, and exit outcome must follow the defined state machine.
- **HSL-AC-004 — Readiness:** The host must enter `NET-04` only after every readiness condition is true strictly before the startup deadline.
- **HSL-AC-005 — Admission dependency:** Issue #30 host validation, manifest freezing, and stable host identities must complete before readiness.
- **HSL-AC-006 — Deadline:** A startup without a complete readiness or specific failure result at 10 seconds must use `host-service-startup-timed-out`.
- **HSL-AC-007 — Cancellation:** Cancel before readiness must retain setup, leave no listener or session, and ignore later readiness.
- **HSL-AC-008 — Specific startup failures:** Port conflict, early exit, other startup failure, and timeout must use their exact identifiers and copy.
- **HSL-AC-009 — Outcome precedence:** Cancel, shutdown, specific compatibility failure, specific startup failure, readiness, and timeout must use the defined precedence.
- **HSL-AC-010 — Retry:** Retry eligibility, retained setup, disabled states, and destinations must follow this document after final cleanup.
- **HSL-AC-011 — Unexpected stop:** An unrequested post-readiness stop must show the exact host failure and must not restart or restore the session.
- **HSL-AC-012 — Intentional end:** Only confirmed End session may send the intentional host-end notice and route an accepting guest to `NET-09`.
- **HSL-AC-013 — Other termination:** Normal application shutdown, crash, forced termination, and service failure must not imply intentional host end to guests.
- **HSL-AC-014 — Cleanup:** Cancel, failure, End session, and application shutdown must release all owned service resources within three seconds.
- **HSL-AC-015 — Orphan prevention:** An owned hosted service must not remain after host application termination.
- **HSL-AC-016 — Redaction:** Startup configuration and all outcomes must satisfy the secret and non-disclosure rules.
- **HSL-AC-017 — Local independence:** Local Play must start and complete without any hosted service or network availability.
- **HSL-AC-018 — Scope truth:** Completion of issue #31 alone must not create or support a playable-networking claim.

## Future acceptance evidence

When team requests product acceptance, the evidence packet must identify the implementation state, environment, scenario, and observation.

For acceptance, the evidence packet must cover:

- one successful startup that proves readiness strictly before 10 seconds;
- Cancel before readiness, including retained setup and completed cleanup;
- port conflict, early exit, generic start failure, and startup timeout outcomes;
- a post-readiness service failure and the host-only `NET-08` result;
- intentional End session with accepted and undelivered guest notice variants;
- normal application shutdown, host application crash, and forced termination without an orphan service;
- Local Play startup and completion without a hosted service;
- reviewer assessment of ownership, stale-result rejection, redaction, and cleanup behavior;
- tester results for state transitions, deadlines, Retry, and destinations;
- supported-platform evidence from DevOps when hosted checks become applicable.

Optional supporting evidence may include bounded timing records, process-lifecycle diagnostics, or recordings of later issue #38 states.

Recordings and screenshots do not prove service cleanup by themselves. UX must assess supplied visual artifacts after issue #38 implements the affected states.

Completion of this specification does not assign product acceptance. Issue #41 remains the complete network-play release gate.
