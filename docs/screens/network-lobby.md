# NET-04 — Network lobby and readiness

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It exposes participant ownership, local-player configuration, host-owned match settings, authoritative roster order, readiness, and retained session results. It implements `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host or guest admission enters from `NET-02` or `NET-03`. Host Start match enters `NET-05`; final-summary Return to lobby enters here with readiness cleared. Confirmed guest Leave sends that guest to `NET-01`. Confirmed host End session sends the host to `NET-01` and guests to `NET-09`. Any unexpected host contact failure enters guest `NET-07`; only a valid End session notice accepted through the current established session enters guest `NET-09`.

## Representative layout

- Use the scaled retro canvas with session endpoint, `Host` or `Guest`, and exact participant/player totals as text.
- Group players under each participant row. Use separate Role, Connection, and Readiness columns, such as `Guest | Reconnecting | Ready`; do not combine these states into one ambiguous label.
- Show host match settings and authoritative roster order. Guests see host-owned controls as read-only.
- The representative host state shows 3 participants, 6 players, and one named unready guest.
- Footer shows Ready/Not ready, host-only Start match, and Leave or End session as appropriate.

## Navigation and significant variants

- A participant edits only its own persons, profiles, and controls; the host edits match settings and roster order.
- A host-alone lobby is valid with `1 <= admitted participants <= players <= 15`, but Start remains disabled until 2–15 participants are connected, 2–15 players exist, each participant owns at least one, and all are ready.
- Any configuration, roster, admission, expiry, or intentional-leave mutation clears every participant's readiness and displays the reason.
- A disconnected admitted guest row changes Connection to `Reconnecting`, retains its prior readiness text, and blocks Start with `Waiting for <participant> to reconnect`.
- Reconnect restores prior readiness unless another clearing mutation occurred. Expiry or intentional Leave removes that participant and players and clears every remaining readiness value.
- Lobby removals are one atomic batch: clear every remaining readiness value, perform no winner evaluation, retain the completed `Session only` result, and label affected retained rows `Departed`.
- Guest Leave opens `Leave session? Your players will be removed and you will return to Network.` Confirm removes the guest and enters guest `NET-01`; Cancel returns to the lobby.
- Host End session opens `End session for everyone?` Confirm sends host to `NET-01` and guests to host-ended `NET-09`; Cancel returns to the lobby.
- Admission closes at match start; there is no join-in-progress control.
- Completed result rows remain labeled `Session only`; departed rows show `Departed`; starting a new match clears the retained result and session end discards it.
- When an active-round or non-final-summary batch leaves fewer than two roster players, this lobby shows the current `Session only • Interrupted • No winner` result and retains any already completed round outcome.

## Truthful copy, disabled reasons, and input

- Required separate labels include participant role, ownership, `Connected` or `Reconnecting`, readiness, and `Session only` where score history is visible.
- Example Start reason: `Waiting for Guest 2 to be ready`. Example Ready reason: `Assign a control to Cora`.
- Focus order follows participant-owned controls, Ready, host-owned settings where applicable, Start match, and Leave/End session. Read-only controls are skipped.
- Keyboard Tab or directional controller input traverses; Enter/Space/controller Confirm activates; Escape/controller Back focuses Leave/End session rather than silently abandoning the session.

Planned representative screenshot: [`SS-018`](../screenshots/README.md#ss-018).
