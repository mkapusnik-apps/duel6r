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
│ Local players: 2 • Lobby 1–15 • Match 2–15 participants/players    │
│                                                                      │
│                 [ Start session ]  [ Back ]                          │
│                 <validation or startup status>                       │
└──────────────────────────────────────────────────────────────────────┘
```

- Start session is enabled only when port and local-player configuration are valid; its disabled reason remains visible.
- Pending startup shows `Starting session…` and `Startup can take up to 10 seconds.`.
- Pending startup locks setup and shows Cancel as the only action.
- Accepted Cancel shows `Cancelling session…` with no activatable control until cleanup completes.
- Completed Cancel returns to editable setup with all values retained and no listener.
- Pending startup never shows listening, readiness, connection, admission, or playable copy.
- Failure variants and Retry/Edit setup/Return destinations remain in the screen specification rather than separate wireframes.
- An invalid host manifest uses the exact blocking reason in the screen specification, disables Retry for the application session, and leaves no listener or session.
- Keyboard/controller focus follows fields, roster controls, Start session, then Back.

Planned representative screenshot: [`SS-016`](../../screenshots/README.md#ss-016).
