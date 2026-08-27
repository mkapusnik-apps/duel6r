# NET-03 — Join setup and connecting

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It configures a guest's direct endpoint and local players, then truthfully reports connection progress. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, and `NET-AC-007`–`NET-AC-009` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Entry is `NET-01` → Join. Confirmed admission enters `NET-04`; failure enters `NET-08`; Cancel during connection restores editable setup; Back returns to `NET-01`.

## Representative layout

- Use the scaled retro canvas with a `JOIN NETWORK SESSION` title.
- Show Hostname or address and Port fields, plus the guest's Local Persons and Local Players configuration.
- The representative state is `Connecting to 192.168.1.24:27015…` with two configured local players and a visible Cancel action.
- Preserve the entered endpoint and local configuration while connecting and after a recoverable failure.

## Navigation and significant variants

- Editable setup enables Connect only for a non-empty hostname/address, valid port, and one or more valid local players.
- Resolving and Connecting states must name the endpoint and must not imply lobby admission.
- Cancel stops the attempt and returns to editable setup without showing Disconnected as though a session existed.
- Exact-release/content mismatch, capacity, host rejection, timeout, and unreachable endpoint enter `NET-08` with distinct available reasons.
- Successful admission alone enters `NET-04`; join-in-progress rejection is explicit when the host already started.

## Truthful copy, disabled reasons, and input

- Example reasons are `Enter a hostname or address`, `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No server browser, discovery, Internet, account, password, or matchmaking affordance may appear.
- Focus order is Hostname/address → Port → local-player controls → Connect → Back. During connection, focus is Cancel.
- Keyboard Tab/Shift+Tab and controller directions traverse controls; Enter/Space/controller Confirm activates; Escape/controller Back cancels an attempt or returns to `NET-01` from editable setup.

Planned representative screenshot: [`SS-017`](../screenshots/README.md#ss-017).
