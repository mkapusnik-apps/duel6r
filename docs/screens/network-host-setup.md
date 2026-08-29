# NET-02 — Host setup

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It collects the direct listening port and host local players before creating a player-hosted session. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-009`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Issue #30 defines the host compatibility result for this planned flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.
Issue #31 defines the hosted-service lifecycle for this planned flow in [`docs/network-host-service-lifecycle.md`](../network-host-service-lifecycle.md).
Issue #31 must not implement this graphical screen.
Issue #38 owns its graphical controls, focus, disabled reasons, and visual evidence.

Entry is `NET-01` → Host. Successful confirmed startup enters `NET-04`; startup failure enters `NET-08`; Back returns to `NET-01` before a session exists.

## Representative layout

- Use the scaled retro canvas with a `HOST NETWORK SESSION` title.
- Show an editable Port field and a read-only support note: `Same machine or LAN • Linux / Windows x86-64`.
- Show the host's local Persons and Local Players panels with person, profile, and control assignment for each selected player.
- Show a capacity line such as `Local players: 2 • Lobby 1–15 • Match 2–15 participants and players`.
- Footer actions are `Start session` and `Back`, with a persistent reason line below or adjacent to Start session.

## Navigation and significant variants

- Editable setup is the representative state. Start session remains disabled until the port is valid and the host owns at least one valid local player.
- Starting must show exactly `Starting session…` and `Startup can take up to 10 seconds.`.
- Starting must lock every setup control.
- Starting must replace Start session and Back with Cancel.
- Starting must not show Retry or permit another startup attempt.
- Starting must not say `Listening`, `Ready`, `Connected`, or `Playable`.
- Accepted Cancel must show exactly `Cancelling session…` until cleanup completes.
- Cancelling must keep every setup control locked and must not accept another Cancel.
- Completed Cancel must return to editable `NET-02` with the port and local-player setup retained.
- Completed Cancel must not show a success or failure message.
- Confirmed startup enters `NET-04` and identifies this participant as Host.
- Confirmed readiness strictly before 10 seconds enters `NET-04`; at or after 10 seconds startup fails and leaves no listener.
- A port conflict or transport startup failure that is confirmed before timeout enters `NET-08` with the specific supported reason.
- A hosted-service exit before readiness enters `NET-08` with `Hosted session stopped before it was ready.`.
- A startup attempt without a complete result at the deadline enters `NET-08` with `Hosted session startup timed out.`.
- Another confirmed startup failure enters `NET-08` with `Hosted session could not start.`.
- An invalid host gameplay-content manifest must enter `NET-08` with `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.`
- An invalid host gameplay-content manifest must leave no listener or session.
- An invalid host gameplay-content manifest must disable Retry for the current application session.
- Edit setup must return here with the port and local-player setup retained.
- Return to Network must enter `NET-01`.
- A confirmed invalid host manifest must take precedence over a later startup timeout.
- An accepted user Cancel must take precedence over a later startup result.
- An accepted application shutdown must take precedence over a later startup result.
- A complete specific failure before the deadline must take precedence over a later timeout.
- Readiness must enter `NET-04` only when it is complete strictly before the deadline.
- Back discards uncommitted network setup only; it does not alter the local-only `MENU-01` setup.

## Truthful copy, disabled reasons, and input

- Example disabled reasons are `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No dedicated-server, Internet exposure, password, discovery, or NAT control may appear.
- Editable focus order is Port → Persons/local-player controls in reading order → Start session → Back.
- Starting focus must move to Cancel.
- Cancelling must keep a visible status, but it must have no activatable control.
- Tab or directional input must move focus.
- Enter, Space, or controller Confirm must activate the focused control.
- Escape or controller Back must activate Back in editable setup.
- Escape or controller Back must activate Cancel while Starting.
- Every selected local player and control assignment must be operable by keyboard and controller, with a visible textual focus state.
- The invalid host-manifest reason and disabled Retry reason must remain readable without color, sound, or transient motion.

Planned representative screenshot: [`SS-016`](../screenshots/README.md#ss-016).
