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
│            │ [ Cancel reconnect ]                     │              │
│            └───────────────────────────────────────────┘              │
└──────────────────────────────────────────────────────────────────────┘
```

- The countdown, continued simulation, reserved-player behavior, and cancellation consequence are textual.
- Lobby, summary, success, expiry, and host-loss variants remain in the specification.
- The state offers no Pause or host-migration action.

Planned representative screenshot: [`SS-021`](../../screenshots/README.md#ss-021).
