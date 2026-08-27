# NET-04 wireframe — Host waiting in lobby

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. Representative data is 3 participants, 6 players, and one unready guest. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│ NETWORK LOBBY • Host • 192.168.1.24:27015                            │
│ 3 participants • 6 players                                          │
│ ┌ PARTICIPANTS / AUTHORITATIVE ROSTER ┐ ┌ HOST MATCH SETTINGS ────┐ │
│ │ Host       Ready       Ada, Bruno   │ │ Deathmatch              │ │
│ │ Guest 1    Ready       Cora, Diego  │ │ Level / rounds / rules  │ │
│ │ Guest 2    Not ready   Emi, Farah   │ │ Roster order            │ │
│ └─────────────────────────────────────┘ └──────────────────────────┘ │
│                                                                      │
│ [ Ready ] [ Start match — disabled ] [ End session ]                 │
│ Waiting for Guest 2 to be ready                                     │
└──────────────────────────────────────────────────────────────────────┘
```

- Role, ownership, connection, and readiness are textual; guests see host settings as read-only.
- Configuration, roster, or membership changes clear every participant's readiness and refresh the visible disabled reason.
- Disconnect, reconnect, capacity, guest, and between-match variants remain in the specification.

Planned representative screenshot: [`SS-018`](../../screenshots/README.md#ss-018).
