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
- Reconnect, intentional leave, score overlay, and host-loss variants remain in the specification.

Planned representative screenshot: [`SS-019`](../../screenshots/README.md#ss-019).
