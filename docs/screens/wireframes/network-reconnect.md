# NET-07 wireframe — Guest reconnect at 24 seconds

Target representative viewport: 1280 by 900 px with active arena context. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                 active match continues in background                 │
│            ┌────────────── RECONNECTING ──────────────┐              │
│            │ Reconnecting to 192.168.1.24:27015…      │              │
│            │ 24 seconds remaining                     │              │
│            │ Your player slots are reserved           │              │
│            │ Match continues while you reconnect      │              │
│            │ Reserved players receive no input and    │              │
│            │ remain in play                           │              │
│            │ [ Leave session ]                        │              │
│            └───────────────────────────────────────────┘              │
└──────────────────────────────────────────────────────────────────────┘
```

- The positive ceiling countdown, continued simulation, reserved-player behavior, and action consequence are textual; active reconnect never shows `0`.
- `Leave session` opens `Leave session? Your reserved players will be removed now and reconnect will stop.` Confirm enters `NET-01`; Cancel preserves the original deadline.
- Resolution, refusal, unreachable, reset, temporary failure, and no response remain retryable in this state against the original deadline.
- Outcome order is host end, independently definitive termination, accepted restore, terminal rejection, retryable ambiguity, then deadline expiry.
- Expiry enters `NET-08` with `Reconnect time expired. The session could not be restored.` and never claims host end or player removal. Lifecycle-specific host-side batching and terminal variants remain in the specification.
- The state offers no Pause or host-migration action.

Planned representative screenshot: [`SS-021`](../../screenshots/README.md#ss-021).
