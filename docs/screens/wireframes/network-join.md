# NET-03 wireframe — Join connecting

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         JOIN NETWORK SESSION                         │
│ Hostname or address [192.168.1.24____]  Port [27015]                 │
│                                                                      │
│ ┌──────── LOCAL PLAYERS 2 ─────────────────────────────────────────┐ │
│ │ Ada             Keyboard                                        │ │
│ │ Bruno           Controller 1                                    │ │
│ └──────────────────────────────────────────────────────────────────┘ │
│                                                                      │
│                 Connecting to 192.168.1.24:27015…                    │
│                 Connection deadline: 10 seconds total                │
│                         [ Cancel ]                                   │
└──────────────────────────────────────────────────────────────────────┘
```

- The endpoint and two local players remain visible while the attempt is pending.
- The screen must not claim connection or lobby admission before the guest validates the exact final `admitted` confirmation, valid host-clock calibration, and one complete valid initial full snapshot.
- The guest must receive all three success inputs strictly before the single total deadline.
- The initial snapshot must match the confirmed participant identity and ordered owned-player identities.
- The snapshot production time must be valid under the host-clock calibration result.
- Inline validation remains on editable `NET-03`. Cancel returns there with endpoint and players retained.
- Complete host rejections use the exact identifier order in the screen specification; malformed or inconsistent complete host messages use the fixed invalid-host-message outcome.
- Without a complete response, name-resolution failure, unreachable or refusal, incomplete admission, and timeout use the exact order and copy in the screen specification.
- User copy is fixed and never displays peer-supplied release IDs, manifest paths, credentials, policy values, or payloads.
- Retry, Edit setup, Return to Network, and other failure variants remain in the screen specification.
- The endpoint header and connecting footer remain fixed. Local-player rows use the flexible middle region and scroll under fixed headings.
- Local setup provides person selection and control assignment without a profile selector, profile column, profile value, or profile-editing action.
- Profile and cosmetic differences do not produce compatibility copy or block Connect.
- Editable setup provides Add and Remove controls for the pre-admission local-player count.
- Connect finalizes the displayed count and ordered slot set for the attempt.
- Connecting locks the slot count and order and shows no active Add, Remove, or Transfer action.

Planned representative screenshot: [`SS-017`](../../screenshots/README.md#ss-017).
