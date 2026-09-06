# NET-09 wireframe — Host-ended session overlay

Target representative viewport: 1280 by 900 px with the last authoritative arena context. The same blocking panel overlays the last lobby, summary, or reconnect context without imposing a fixed 850 by 700 canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                    last authoritative arena frame                    │
│            ┌──────────── HOST ENDED SESSION ─────────────┐            │
│            │ The host ended the session.                 │            │
│            │ This session cannot be resumed.            │            │
│            │ Session-only results were not saved to     │            │
│            │ local statistics or Elo.                   │            │
│            │ [ Return to Network ]                      │            │
│            └─────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

- The representative state uses only the fixed valid host-end copy shown above.
- Intentional terminal outcome, no migration/resume, and no persistence are explicit text.
- Silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, no response, terminal rejection, and deadline expiry cannot produce this overlay.
- Normal application shutdown, forced termination, and hosted-service failure cannot produce this overlay.
- Lobby, summary, and reconnect-context variants remain in the specification.
- Return to Network is the only action and is keyboard/controller focused by default.

Planned representative screenshot: [`SS-023`](../../screenshots/README.md#ss-023).
