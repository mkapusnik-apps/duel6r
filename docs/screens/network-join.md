# NET-03 — Join setup and connecting

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It configures a guest's direct endpoint and local players, then truthfully reports connection progress. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 defines the compatibility and admission outcomes for this planned flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.

Entry is `NET-01` → Join. Confirmed admission enters `NET-04`; failure enters `NET-08`; Cancel during connection restores editable setup; Back returns to `NET-01`.

## Representative layout

- Use the scaled retro canvas with a `JOIN NETWORK SESSION` title.
- Show Hostname or address and Port fields, plus the guest's Local Persons and Local Players configuration.
- The representative state is `Connecting to 192.168.1.24:27015…` with two configured local players and a visible Cancel action.
- Preserve the entered endpoint and local configuration while connecting and after a recoverable failure.

## Navigation and significant variants

- Editable setup performs hostname/address and port validation inline. Invalid input never leaves `NET-03` and never starts the connection clock.
- Resolving and Connecting states name the endpoint, show that the single 10-second boundary includes resolution through admission, and do not imply lobby admission.
- Cancel stops the attempt and returns to editable setup with endpoint and local-player configuration retained; it does not show Disconnected as though a session existed.
- After Cancel and local validation, a complete host response must use the first applicable result in this order: `malformed-request`, `not-authorized`, `protocol-incompatible`, `network-release-mismatch`, `required-capability-unsupported`, `gameplay-content-manifest-invalid`, `gameplay-content-mismatch`, `match-already-started`, `session-full`, `host-policy-rejected`, and `admitted`.
- Without a complete host response, initial transport outcomes use name-resolution failure, unreachable/refusal, reset/close before complete admission, then generic timeout. A complete response accepted before the deadline outranks later generic transport symptoms.
- User copy must use the exact fixed messages in `NET-08`.
- User copy must not include a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.
- Successful admission alone enters `NET-04`; join-in-progress rejection is explicit when the host already started.
- `NET-08` Retry repeats the retained attempt, Edit setup returns here with all data retained, and Return to Network enters `NET-01`.

## Truthful copy, disabled reasons, and input

- Example reasons are `Enter a hostname or address`, `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No server browser, discovery, Internet, account, password, or matchmaking affordance may appear.
- Focus order is Hostname/address → Port → local-player controls → Connect → Back. During connection, focus is Cancel.
- Keyboard Tab/Shift+Tab and controller directions traverse controls; Enter/Space/controller Confirm activates; Escape/controller Back cancels an attempt or returns to `NET-01` from editable setup.
- A result must remain visible as text until the user selects a recovery action.

Planned representative screenshot: [`SS-017`](../screenshots/README.md#ss-017).
