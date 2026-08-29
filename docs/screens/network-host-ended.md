# NET-09 — Host-ended session outcome

## Status, purpose, and requirements

This is a target blocking overlay for downstream issue #38; it is not implemented. It presents only a valid intentional host End session notice accepted through the guest's current established session. It implements `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).

A valid intentional host End session notice accepted through the current established session enters this guest overlay from `NET-04`, `NET-05`, `NET-06`, or `NET-07`. `Return to Network` enters `NET-01`. The host itself goes directly to `NET-01`. Every unexpected host failure remains guest `NET-07` until terminal rejection or expiry.

## Representative layout

- Preserve the last confirmed lobby, arena, summary, or reconnect context under a blocking readable panel. Do not impose the 850 by 700 menu canvas on this screen.
- Copy is exactly `HOST ENDED SESSION`, `The host ended the session`, and `This session cannot be resumed`.
- Show `Session-only results were not saved to local statistics or Elo` when match activity had begun.
- Provide one primary action: `Return to Network`.

## Significant variants

- Lobby context omits match-result copy. Match, summary, and reconnect contexts include the session-only persistence statement.
- The screen has no countdown, election, reconnect-to-new-host, Save result, or Continue match action.
- Silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, no response, terminal rejection, and deadline expiry never use this screen.
- Normal application shutdown, forced termination, and hosted-service failure never use this screen.
- Normal application shutdown must not send or imply an intentional host-end notice.
- A crash or forced termination must not send or imply an intentional host-end notice.
- A host-local post-readiness service failure must show host `NET-08` and must leave affected guests in the ambiguous `NET-07` journey.

## Truthful copy, focus, and input

- `Disconnected` alone is insufficient; the accepted intentional host-end outcome must be explicit without peer-supplied values.
- Return to Network is focused by default and is always enabled.
- Keyboard Enter, Space, or Escape and controller Confirm or Back activate Return to Network; pointer activation remains available.
- The state must remain legible without color and must not animate as though reconnection is in progress.

Planned representative screenshot: [`SS-023`](../screenshots/README.md#ss-023).
