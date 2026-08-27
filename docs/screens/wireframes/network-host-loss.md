# NET-09 wireframe — Host lost

Target representative viewport: 1280 by 900 px with the last authoritative arena context. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                    last authoritative arena frame                    │
│            ┌─────────── HOST CONNECTION LOST ───────────┐            │
│            │ The session has ended.                     │            │
│            │ This session cannot be resumed.            │            │
│            │ Session-only results were not saved to     │            │
│            │ local statistics or Elo.                   │            │
│            │ [ Return to Network ]                      │            │
│            └─────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

- Host loss, terminal session outcome, no migration, and no persistence are explicit text.
- Lobby and summary-context variants remain in the specification.
- Return to Network is the only action and is keyboard/controller focused by default.

Planned representative screenshot: [`SS-023`](../../screenshots/README.md#ss-023).
