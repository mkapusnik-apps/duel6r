# NET-09 — Host-ended or definitive-termination outcome

## Status, purpose, and requirements

This is a target blocking overlay for downstream issue #38; it is not implemented. It distinguishes a valid intentional host-end notice from independently definitive session termination that cannot arise solely from guest isolation. It implements `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

A valid host-end notice or independently definitive termination from `NET-04`, `NET-05`, `NET-06`, or `NET-07` enters the applicable guest variant. `Return to Network` enters `NET-01`. The host itself goes directly to `NET-01` after confirmed End session. Ambiguous isolation never enters this screen.

## Representative layout

- Preserve the last confirmed lobby, arena, summary, or reconnect context under a blocking readable panel. Do not impose the 850 by 700 menu canvas on this screen.
- Host-ended variant copy is `HOST ENDED SESSION`, `The host ended the session`, and `This session cannot be resumed`.
- Definitive-termination variant copy is `SESSION TERMINATED`, `The session ended and cannot be restored`, and `This session cannot be resumed`.
- Show `Session-only results were not saved to local statistics or Elo` when match activity had begun.
- Provide one primary action: `Return to Network`.

## Significant variants

- Lobby variants omit match-result copy but still state the terminal cause.
- Match, summary, and reconnect-context variants include the session-only persistence statement.
- The screen has no countdown, election, reconnect-to-new-host, Save result, or Continue match action.
- Host end and definitive termination use distinct fixed copy. Silence, refusal, unreachable, reset, temporary failure, no response, and deadline expiry never use this screen.

## Truthful copy, focus, and input

- `Disconnected` alone is insufficient; the valid host-ended or definitive-termination outcome must be explicit without peer-supplied values.
- Return to Network is focused by default and is always enabled.
- Keyboard Enter, Space, or Escape and controller Confirm or Back activate Return to Network; pointer activation remains available.
- The state must remain legible without color and must not animate as though reconnection is in progress.

Planned representative screenshot: [`SS-023`](../screenshots/README.md#ss-023).
