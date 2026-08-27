# NET-04 wireframe — Host waiting in lobby

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. Representative data is 3 participants, 6 players, and one unready guest. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│ NETWORK LOBBY • Host • 192.168.1.24:27015                            │
│ 3 participants • 6 players                                          │
│ ┌ PARTICIPANTS / AUTHORITATIVE ROSTER ┐ ┌ HOST MATCH SETTINGS ────┐ │
│ │ Role    Connection  Readiness Players│ │ Deathmatch              │ │
│ │ Host    Connected   Ready     Ada…   │ │ Level / rounds / rules  │ │
│ │ Guest   Connected   Ready     Cora…  │ │ Roster order            │ │
│ │ Guest   Connected   Not ready Emi…   │ │                         │ │
│ └─────────────────────────────────────┘ └──────────────────────────┘ │
│                                                                      │
│ [ Ready ] [ Start match — disabled ] [ End session ]                 │
│ Waiting for Guest 2 to be ready                                     │
└──────────────────────────────────────────────────────────────────────┘
```

- Role, ownership, Connection, and Readiness occupy separate textual columns; guests see host settings as read-only.
- Configuration, roster, or membership changes clear every participant's readiness and refresh the visible disabled reason.
- A Reconnecting row retains readiness but blocks Start by participant name. Ambiguous isolation stays in reconnect for the full deadline.
- Lobby removal batches clear all readiness, perform no winner evaluation, retain completed results, and label departed rows. Host-alone, Leave/End confirmations, and other variants remain in the specification.
- An active-round or non-final-summary interruption returns here with `Session only • Interrupted • No winner` and any completed-round outcome retained.

Planned representative screenshot: [`SS-018`](../../screenshots/README.md#ss-018).
