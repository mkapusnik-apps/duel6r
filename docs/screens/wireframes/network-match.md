# NET-05 wireframe — Six-player LAN Deathmatch

Target representative viewport: 1280 by 900 px. The gameplay renderer fills the client with one undivided arena. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│ events             Round 2 | 3              ranking: six players    │
│                                                                      │
│                 complete shared LAN arena                            │
│       P1        P2          P3          P4       P5        P6        │
│        terrain • hazards • pickups • shots • water                   │
│                                                                      │
│ Host • LAN session • Connected                 Session only scores   │
└──────────────────────────────────────────────────────────────────────┘
```

- Existing arena, ranking, progress, event, and player-status presentation remains available.
- Compact textual network status does not obscure play or create player-specific views.
- Guest `Leave session` and host `End session` use consequence panels with Confirm and Cancel over this unchanged arena.
- Every unexpected host failure remains in guest `NET-07` through the fixed deadline; only a valid intentional End session notice accepted through the established session uses `NET-09`.
- Active-round batches evaluate one winner condition and may create `Session only • Interrupted • No winner`; non-final summaries preserve completed rounds before continue/interruption. Degraded-host-only continuation and other variants remain in the specification.

Planned representative screenshot: [`SS-019`](../../screenshots/README.md#ss-019).
