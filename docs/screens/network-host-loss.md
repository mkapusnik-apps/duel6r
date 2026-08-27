# NET-09 — Host loss

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It states that loss of the player host ends the authoritative session and cannot migrate or resume. It implements `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, and `NET-AC-015` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Host loss from `NET-04`, `NET-05`, `NET-06`, or a reconnect attempt enters this screen for guests. `Return to Network` enters `NET-01`.

## Representative layout

- Preserve the current lobby, arena, or summary context under a blocking readable panel where possible.
- Show `HOST CONNECTION LOST`, `The session has ended`, and `This session cannot be resumed`.
- Show `Session-only results were not saved to local statistics or Elo` when match activity had begun.
- Provide one primary action: `Return to Network`.

## Significant variants

- Lobby host loss omits match-result copy but still states that the session ended.
- Match and summary host loss include the session-only persistence statement.
- The screen has no countdown, election, reconnect-to-new-host, Save result, or Continue match action.
- A separate confirmed host-ended cancellation may use `NET-08`; unexpected authoritative host loss always uses this screen.

## Truthful copy, focus, and input

- `Disconnected` alone is insufficient; host ownership loss and final session outcome must both be explicit.
- Return to Network is focused by default and is always enabled.
- Keyboard Enter, Space, or Escape and controller Confirm or Back activate Return to Network; pointer activation remains available.
- The state must remain legible without color and must not animate as though reconnection is in progress.

Planned representative screenshot: [`SS-023`](../screenshots/README.md#ss-023).
