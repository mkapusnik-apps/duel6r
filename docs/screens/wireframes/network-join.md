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
- The screen does not claim connection or lobby admission before the exact final `admitted` confirmation.
- Inline validation remains on editable `NET-03`. Cancel returns there with endpoint and players retained.
- Complete host rejections use the exact identifier order in the screen specification; malformed or inconsistent complete host messages use the fixed invalid-host-message outcome.
- Without a complete response, name-resolution failure, unreachable or refusal, incomplete admission, and timeout use the exact order and copy in the screen specification.
- User copy is fixed and never displays peer-supplied release IDs, manifest paths, credentials, policy values, or payloads.
- Retry, Edit setup, Return to Network, and other failure variants remain in the screen specification.

Planned representative screenshot: [`SS-017`](../../screenshots/README.md#ss-017).
