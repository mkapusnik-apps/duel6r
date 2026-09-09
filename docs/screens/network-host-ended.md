# NET-09 — Host-ended session outcome

## Status, purpose, and requirements

This is a target blocking overlay for downstream issue #38; it is not implemented. It presents only a valid intentional host End session notice accepted through the guest's current established session. It implements `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
Its retained arena context implements `NET-VIS-003` through `NET-VIS-010`, `NET-VIS-AC-002` through `NET-VIS-AC-004`, `REP-PRES-001` through `REP-PRES-006`, and `REP-PRES-AC-001` through `REP-PRES-AC-003`.

A valid intentional host End session notice accepted through the current established session enters this guest overlay from `NET-04`, `NET-05`, `NET-06`, or `NET-07`. `Return to Network` enters `NET-01`. The host itself goes directly to `NET-01`. Every unexpected host failure remains guest `NET-07` until terminal rejection or expiry.

## Representative layout

- Preserve the last confirmed lobby, arena, summary, or reconnect context under a blocking readable panel. Do not impose the 850 by 700 menu canvas on this screen.
- Copy is exactly `HOST ENDED SESSION`, `The host ended the session`, and `This session cannot be resumed`.
- Show `Session-only results were not saved to local statistics or Elo` when match activity had begun.
- Provide one primary action: `Return to Network`.
- Center the blocking panel in the client and keep at least 16 px from every client edge.
- Limit the panel width to 640 px or the available client width, whichever is smaller.
- Keep the heading and Return to Network action visible.
- Wrap the fixed explanatory copy at word boundaries.
- Keep the panel content inside the client without clipping at every supported desktop viewport.
- A retained arena context must keep the default network visuals from the last complete accepted state.
- A retained arena context must not switch any player to a selected-profile appearance.

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
