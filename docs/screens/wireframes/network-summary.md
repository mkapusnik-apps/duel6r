# NET-06 wireframe — Final three-round summary

Target representative viewport: 1280 by 900 px. The final arena context remains behind the existing translucent summary language. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                         final arena frame                            │
│            ┌────────── NETWORK MATCH COMPLETE ──────────┐            │
│            │ Session only                               │            │
│            │ Not saved to local statistics or Elo       │            │
│            │ Round 1   winner / scores                  │            │
│            │ Round 2   winner / scores                  │            │
│            │ Round 3   winner / final totals            │            │
│            │ [ Return to lobby ] [ End session ]        │            │
│            └─────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

- The exact `Session only` label is visible with the authoritative final results.
- Retained lobby results label departed rows `Departed`; the next match replaces this set and session end/loss discards it.
- Guest Leave, reconnect, host End session, and host-loss variants and confirmations remain in the specification.
- Keyboard/controller focus is explicit and does not depend on color.

Planned representative screenshot: [`SS-020`](../../screenshots/README.md#ss-020).
