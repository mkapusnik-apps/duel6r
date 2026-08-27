# NET-04 — Network lobby and readiness

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It exposes participant ownership, local-player configuration, host-owned match settings, authoritative roster order, and readiness before a match. It implements `NET-AC-004`–`NET-AC-009` and `NET-AC-015` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host or guest admission enters from `NET-02` or `NET-03`. Host Start match enters `NET-05`; final-summary Return to lobby enters here with readiness cleared. Guest leave or host cancellation returns through `NET-01`; guest disconnect enters `NET-07`.

## Representative layout

- Use the scaled retro canvas with session endpoint, `Host` or `Guest`, and exact participant/player totals as text.
- Group players under each participant row and label participant connection and readiness as `Host • Ready`, `Guest • Not ready`, or equivalent text.
- Show host match settings and authoritative roster order. Guests see host-owned controls as read-only.
- The representative host state shows 3 participants, 6 players, and one named unready guest.
- Footer shows Ready/Not ready, host-only Start match, and Leave or End session as appropriate.

## Navigation and significant variants

- A participant edits only its own persons, profiles, and controls; the host edits match settings and roster order.
- Any material configuration, roster, or membership change clears every participant's readiness and displays `Readiness cleared: configuration changed`.
- Start match remains disabled until `2 <= participants <= players <= 15` and all participants are ready. The reason names unready participants or invalid configuration.
- Host End session requires confirmation and sends guests to `NET-01` with `Host ended the session`.
- Guest Leave removes that guest immediately. Unintentional guest disconnect enters `NET-07`; a restored lobby returns to its current authoritative state.
- Admission closes at match start; there is no join-in-progress control.

## Truthful copy, disabled reasons, and input

- Required labels include participant role, ownership, readiness, connection state, and `Session only` where score history is visible.
- Example Start reason: `Waiting for Guest 2 to be ready`. Example Ready reason: `Assign a control to Cora`.
- Focus order follows participant-owned controls, Ready, host-owned settings where applicable, Start match, and Leave/End session. Read-only controls are skipped.
- Keyboard Tab or directional controller input traverses; Enter/Space/controller Confirm activates; Escape/controller Back focuses Leave/End session rather than silently abandoning the session.

Planned representative screenshot: [`SS-018`](../screenshots/README.md#ss-018).
