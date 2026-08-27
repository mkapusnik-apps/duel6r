# NET-03 wireframe — Join connecting

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         JOIN NETWORK SESSION                         │
│ Hostname or address [192.168.1.24____]  Port [27015]                 │
│                                                                      │
│ ┌──────── LOCAL PLAYERS 2 ─────────────────────────────────────────┐ │
│ │ Ada   profile A   Keyboard                                      │ │
│ │ Bruno profile B   Controller 1                                  │ │
│ └──────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│                 Connecting to 192.168.1.24:27015…                    │
│                 Connection deadline: 10 seconds total                │
│                         [ Cancel ]                                   │
└──────────────────────────────────────────────────────────────────────┘
```

- The endpoint and two local players remain visible while the attempt is pending.
- The screen does not claim connection or lobby admission before confirmation.
- Inline validation remains on editable `NET-03`. Cancel returns there with endpoint and players retained. Specific failures precede generic timeout.
- Retry, Edit setup, Return to Network, and other failure variants remain in the screen specification.

Planned representative screenshot: [`SS-017`](../../screenshots/README.md#ss-017).
