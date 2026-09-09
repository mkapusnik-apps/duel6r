# NET-03 — Join setup and connecting

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It configures a guest's direct endpoint and local players, then truthfully reports connection progress. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
It preserves `INP-001` through `INP-010` and implements `NIN-OWN-006` and `NIN-BOUND-003` in [`docs/network-authoritative-player-input.md`](../network-authoritative-player-input.md).
Issue #30 defines the compatibility and admission outcomes for this planned flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.

Entry is `NET-01` → Join. Complete validated production admission enters `NET-04`; failure enters `NET-08`; Cancel during connection restores editable setup; Back returns to `NET-01`.

## Representative layout

- Use the scaled retro canvas with a `JOIN NETWORK SESSION` title.
- Show Hostname or address and Port fields, plus the guest's Local Persons and Local Players configuration.
- The representative state is `Connecting to 192.168.1.24:27015…` with two configured local players and a visible Cancel action.
- Preserve the entered endpoint and local configuration while connecting and after a recoverable failure.
- Keep the screen content inside the shared 24-logical-pixel canvas margin.
- Use a fixed title and endpoint region, a flexible local-player region, and a fixed status and action region.
- Give Hostname or address the remaining width after the Port field and the 8-logical-pixel field gap.
- Keep local-player headings fixed while local-player rows scroll vertically.
- Keep each person, profile, and control assignment on one row and clip it inside its column.
- Keep connecting status above Cancel and wrap it at word boundaries.
- Keep Cancel visible when retained setup or connecting copy is longer than the representative values.

## Navigation and significant variants

- Editable setup performs hostname/address and port validation inline. Invalid input never leaves `NET-03` and never starts the connection clock.
- Connect starts the single 10-second attempt. Guest-local gameplay-manifest validation, resolution, connection, request, admission offer, exact guest acceptance, atomic host commit, host-clock calibration, final `admitted` confirmation, initial full snapshot validation, and lobby handoff all share that boundary. There is no separate offer timer.
- Guest-local manifest validation must finish before resolution. An invalid result established before the deadline uses `guest-gameplay-content-manifest-invalid` and the exact `NET-08` copy, performs no connection, disables Retry until restart, and retains endpoint and local players for Edit setup.
- Cancel stops the attempt and returns to editable setup with endpoint and local-player configuration retained; it does not show Disconnected as though a session existed.
- Cancel is evaluated before every queued host message and immediately before acceptance or outcome publication. At the deadline, the client must drain already queued complete messages atomically. A valid rejection or a complete valid production admission result received strictly before the deadline must retain precedence over delayed polling or a later close. A late success input must not create admission.
- After Cancel and local validation, a complete host response must use the first applicable result in this order: `malformed-request`, `not-authorized`, `protocol-incompatible`, `network-release-mismatch`, `required-capability-unsupported`, `gameplay-content-manifest-invalid`, `gameplay-content-mismatch`, `match-already-started`, `session-full`, `host-policy-rejected`, and `admitted`.
- Without a complete valid rejection or complete valid production admission result, initial transport outcomes use name-resolution failure, unreachable/refusal, reset/close before complete admission, then generic timeout. A complete valid rejection or complete valid production admission result received strictly before the deadline must outrank later generic transport symptoms. An offer alone must not report success.
- User copy must use the exact fixed messages in `NET-08`.
- User copy must not include a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.
- A guest must validate the exact final confirmation, one valid host-clock calibration result, and one complete valid initial full snapshot before it enters `NET-04`.
- The guest must receive all three success inputs strictly before the single total deadline.
- The final confirmation must contain the exact offered participant identity, original player count, and ordered player identities.
- The initial snapshot must contain the same participant identity and ordered owned-player identities.
- The snapshot production time must be valid under the host-clock calibration result.
- A final confirmation without valid calibration and a valid initial snapshot must not report success.
- A malformed, trailing, unexpected, or inconsistent complete host offer, rejection, or confirmation closes the attempt as `invalid-host-admission-message` with `Connection ended before admission completed.`.
- Join-in-progress rejection is explicit when the host already started.
- `NET-08` Retry repeats the retained attempt, Edit setup returns here with all data retained, and Return to Network enters `NET-01`.

## Truthful copy, disabled reasons, and input

- Example reasons are `Enter a hostname or address`, `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No server browser, discovery, Internet, account, password, or matchmaking affordance may appear.
- Focus order is Hostname/address → Port → local-player controls → Connect → Back. During connection, focus is Cancel.
- Keyboard Tab/Shift+Tab and controller directions traverse controls; Enter/Space/controller Confirm activates; Escape/controller Back cancels an attempt or returns to `NET-01` from editable setup.
- A result must remain visible as text until the user selects a recovery action.
- The guest must be able to select any established keyboard preset or detected supported controller preset for each owned local player.
- The setup must permit the same control preset for more than one owned local player.
- Controller detection and connection changes must preserve the established local device behavior.

Planned representative screenshot: [`SS-017`](../screenshots/README.md#ss-017).
