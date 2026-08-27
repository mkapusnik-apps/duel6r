# NET-09 — Host-ended or host-loss session outcome

## Status, purpose, and requirements

This is a target blocking overlay for downstream issue #38; it is not implemented. It distinguishes confirmed host End session from unexpected host loss and never implies migration or resume. It implements `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

Confirmed host End session or unexpected host loss from `NET-04`, `NET-05`, `NET-06`, or `NET-07` enters the applicable variant for guests. `Return to Network` enters `NET-01`. The host itself goes directly to `NET-01` after confirmed End session.

## Representative layout

- Preserve the last confirmed lobby, arena, summary, or reconnect context under a blocking readable panel. Do not impose the 850 by 700 menu canvas on this screen.
- Host-ended variant copy is `HOST ENDED SESSION`, `The host ended the session`, and `This session cannot be resumed`.
- Unexpected-loss variant copy is `HOST CONNECTION LOST`, `The session ended because the host connection was lost`, and `This session cannot be resumed`.
- Show `Session-only results were not saved to local statistics or Elo` when match activity had begun.
- Provide one primary action: `Return to Network`.

## Significant variants

- Lobby variants omit match-result copy but still state the terminal cause.
- Match, summary, and reconnect-context variants include the session-only persistence statement.
- The screen has no countdown, election, reconnect-to-new-host, Save result, or Continue match action.
- Confirmed host end and unexpected host loss both use this screen with distinct copy. Neither uses `NET-08`.

## Truthful copy, focus, and input

- `Disconnected` alone is insufficient; the exact host-ended or host-loss cause and terminal session outcome must both be explicit.
- Return to Network is focused by default and is always enabled.
- Keyboard Enter, Space, or Escape and controller Confirm or Back activate Return to Network; pointer activation remains available.
- The state must remain legible without color and must not animate as though reconnection is in progress.

Planned representative screenshot: [`SS-023`](../screenshots/README.md#ss-023).
