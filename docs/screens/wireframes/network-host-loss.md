# NET-09 wireframe — Host-ended or definitive-termination overlay

Target representative viewport: 1280 by 900 px with the last authoritative arena context. The same blocking panel overlays the last lobby, summary, or reconnect context without imposing a fixed 850 by 700 canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                    last authoritative arena frame                    │
│            ┌──────────── SESSION TERMINATED ─────────────┐            │
│            │ The session ended and cannot be restored.  │            │
│            │ This session cannot be resumed.            │            │
│            │ Session-only results were not saved to     │            │
│            │ local statistics or Elo.                   │            │
│            │ [ Return to Network ]                      │            │
│            └─────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

- The representative independently definitive variant uses `SESSION TERMINATED`. The valid host-ended variant replaces the heading with `HOST ENDED SESSION` and says `The host ended the session`.
- Terminal outcome, no migration/resume, and no persistence are explicit text.
- Silence, refusal, unreachable, reset, temporary failure, no response, and deadline expiry cannot produce this overlay from guest isolation.
- Lobby, summary, and reconnect-context variants remain in the specification.
- Return to Network is the only action and is keyboard/controller focused by default.

Planned representative screenshot: [`SS-023`](../../screenshots/README.md#ss-023).
