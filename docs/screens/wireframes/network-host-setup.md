# NET-02 wireframe — Host setup

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         HOST NETWORK SESSION                         │
│ Same machine or LAN • Linux / Windows x86-64                        │
│ Port [27015____]                                                     │
│ Listening interface [192.168.1.24 (Private LAN)_______________]     │
│ ┌──────── PERSONS ────────┐  ┌──────── LOCAL PLAYERS 2 ───────────┐ │
│ │ available persons       │  │ Ada             Keyboard           │ │
│ │ person choices          │  │ Bruno           Controller 1       │ │
│ │ [Add]                   │  │ [Remove] [Reorder]                  │ │
│ └─────────────────────────┘  └────────────────────────────────────┘ │
│ Local players: 2 • Lobby 1–15 • Match 2–15 participants/players    │
│                                                                      │
│                 [ Start session ]  [ Back ]                          │
│                 <validation or startup status>                       │
└──────────────────────────────────────────────────────────────────────┘
```

- Start session is enabled only when Port, `Listening interface`, and local-player configuration are valid; its disabled reason remains visible.
- Port has initial focus.
- `Listening interface` follows Port in keyboard and controller focus order.
- First entry selects `127.0.0.1 (Same machine)` by default.
- An explicit LAN selection shows the complete eligible assigned private RFC1918 IPv4 literal and `Private LAN` in the collapsed selector.
- The expanded selector lists loopback and every currently eligible assigned private RFC1918 IPv4 literal once.
- The expanded selector excludes wildcard, unspecified, public, multicast, link-local, unassigned, network, and broadcast addresses.
- Each option uses one row, and a long scope label clips only after the complete IPv4 literal.
- The expanded list overlays lower content, keeps the split-panel top edge fixed, and scrolls inside the canvas when necessary.
- Confirm opens or accepts the selector; directional input changes the highlighted option; Back closes the open selector without leaving the screen.
- Start revalidates the selected address before startup begins.
- A stale selection returns to editable setup with `Selected listening interface is no longer available. Choose another interface.` and no service attempt.
- Pending startup shows `Starting session…` and `Startup can take up to 10 seconds.`.
- Pending startup locks setup and shows Cancel as the only action.
- Accepted Cancel shows `Cancelling session…` with no activatable control until cleanup completes.
- Completed Cancel returns to editable setup with all values retained and no listener.
- Pending startup never shows listening, readiness, connection, admission, or playable copy.
- Failure variants and Retry/Edit setup/Return destinations remain in the screen specification rather than separate wireframes.
- An invalid host manifest uses the exact blocking reason in the screen specification, disables Retry for the application session, and leaves no listener or session.
- Keyboard/controller focus follows Port, `Listening interface`, roster controls, Start session, then Back.
- The setup body is flexible between fixed header and footer regions. Both lists scroll vertically under fixed headings, and status grows upward without covering the setup or actions.
- Persons and Local Players provide person selection and control assignment without a profile selector, profile column, profile value, or profile-editing action.
- A profile or cosmetic difference does not create a setup warning or prevent Start session.
- Add and Remove change the local-player count only in editable setup.
- Start session finalizes the displayed count and ordered slot set for the attempt.
- Starting and Cancelling lock slot count and order.
- Cancel, failure, Edit setup, and eligible Retry retain the selected address while it remains eligible.
- Enumeration and selection do not discover peers or change host, Docker, firewall, route, NAT, or port-forwarding configuration.

Planned representative screenshot: [`SS-016`](../../screenshots/README.md#ss-016).
