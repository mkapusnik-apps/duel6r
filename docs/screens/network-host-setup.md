# NET-02 — Host setup

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It collects the direct listening port and host local players before creating a player-hosted session. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-009`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Entry is `NET-01` → Host. Successful confirmed startup enters `NET-04`; startup failure enters `NET-08`; Back returns to `NET-01` before a session exists.

## Representative layout

- Use the scaled retro canvas with a `HOST NETWORK SESSION` title.
- Show an editable Port field and a read-only support note: `Same machine or LAN • Linux / Windows x86-64`.
- Show the host's local Persons and Local Players panels with person, profile, and control assignment for each selected player.
- Show a capacity line such as `Local players: 2 • Lobby 1–15 • Match 2–15 participants and players`.
- Footer actions are `Start session` and `Back`, with a persistent reason line below or adjacent to Start session.

## Navigation and significant variants

- Editable setup is the representative state. Start session remains disabled until the port is valid and the host owns at least one valid local player.
- `Starting session…` locks editable controls, shows the 10-second startup boundary, and offers Cancel; it must not say `Listening` or open the lobby early.
- Cancel returns to editable `NET-02`, retains port and local-player setup, and confirms no listener or session remains.
- Confirmed startup enters `NET-04` and identifies this participant as Host.
- Confirmed readiness strictly before 10 seconds enters `NET-04`; at or after 10 seconds startup fails and leaves no listener.
- Port conflict, transport startup, or content preparation failure confirmed before timeout enters `NET-08` with that specific reason. Retry repeats retained setup; Edit setup returns here with all setup retained; Return to Network enters `NET-01`.
- Back discards uncommitted network setup only; it does not alter the local-only `MENU-01` setup.

## Truthful copy, disabled reasons, and input

- Example disabled reasons are `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No dedicated-server, Internet exposure, password, discovery, or NAT control may appear.
- Focus order is Port → Persons/local-player controls in reading order → Start session → Back. Tab/directional input moves focus; Enter/Space/controller Confirm activates; Escape/controller Back activates Back unless startup is pending, when it activates Cancel and restores editable setup.
- Every selected local player and control assignment must be operable by keyboard and controller, with a visible textual focus state.

Planned representative screenshot: [`SS-016`](../screenshots/README.md#ss-016).
