# NET-06 wireframe — Completed final three-round summary

Target representative viewport: 1280 by 900 px. The final arena context remains behind the existing translucent summary language. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│                         final arena frame                            │
│            ┌────────── NETWORK MATCH COMPLETE ──────────┐            │
│            │ Session only                               │            │
│            │ Not saved to local statistics or Elo       │            │
│            │ Result state: Completed                    │            │
│            │ Match outcome: final-round outcome         │            │
│            │ Last completed round: 3 • outcome          │            │
│            │ Match settings • seed • completed rounds   │            │
│            │ Round results • level • orientation        │            │
│            │ Cumulative ranking • final totals          │            │
│            │ [ Return to lobby ] [ End session ]        │            │
│            └─────────────────────────────────────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

- The exact `Session only` label is visible with the authoritative final results.
- This screen presents only a `Completed` result.
- The match outcome equals the configured final-round outcome.
- Match outcome, last completed-round outcome, and cumulative ranking use separate labels.
- The cumulative ranking leader does not receive a champion label or treatment.
- Final-summary departure never reevaluates the completed outcome; retained rows use `Departed`. Starting a new match clears this set, while intentional host end or host-local supervised failure discards the host result.
- Guest Leave, full-deadline reconnect, and intentional host End session variants and confirmations remain in the specification.
- Keyboard/controller focus is explicit and does not depend on color.
- An interrupted result enters `NET-04` directly and never appears here.
- The result provides all completed match, round, player, and applicable team fields from issue #32.
- Ranking uses labeled columns and textual rank, team, winner, No winner, and Departed values.

Planned representative screenshot: [`SS-020`](../../screenshots/README.md#ss-020).
