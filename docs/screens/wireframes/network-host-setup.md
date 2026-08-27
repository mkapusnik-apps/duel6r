# NET-02 wireframe — Host setup

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         HOST NETWORK SESSION                         │
│ Same machine or LAN • Linux / Windows x86-64                        │
│ Port [27015____]                                                     │
│                                                                      │
│ ┌──────── PERSONS ────────┐  ┌──────── LOCAL PLAYERS 2 ───────────┐ │
│ │ available persons       │  │ Ada   profile A   Keyboard         │ │
│ │ person/profile choices  │  │ Bruno profile B   Controller 1     │ │
│ │ [Add]                   │  │ [Remove] [Reorder]                  │ │
│ └─────────────────────────┘  └────────────────────────────────────┘ │
│ Local players: 2 • Session capacity: 2–15 players                   │
│                                                                      │
│                 [ Start session ]  [ Back ]                          │
│                 <validation or startup status>                       │
└──────────────────────────────────────────────────────────────────────┘
```

- Start session is enabled only when port and local-player configuration are valid; its disabled reason remains visible.
- Starting, failure, and cancellation variants remain in the screen specification rather than separate wireframes.
- Keyboard/controller focus follows fields, roster controls, Start session, then Back.

Planned representative screenshot: [`SS-016`](../../screenshots/README.md#ss-016).
