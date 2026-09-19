# NET-01 — Public and private network entry

Functional authority: [NET-01](../../screens/network-entry.md), state `NET-01-PUBLIC-ENTRY`, NET-PUB-016–021. Wireframe: [NET-01](../wireframes/NET-01/NET-01.svg).

Visual impact: localized navigation and scope-copy change. No new screen or presentation class.

## Hierarchy and interaction

- The title must remain `NETWORK PLAY`.
- The first action must be `Connect` and use the NET-03 route in the functional contract.
- The second action must be `Host private LAN` and use the unchanged NET-02 route.
- `Back` must remain last and return to the main menu.
- The entry must explain that the public service requires an invite.
- The entry must keep private-LAN hosting discoverable beside the public path.
- The entry must not imply that selecting Connect reserves a session or assigns Host.
- The equal-width action column must remain centered below scope text.
- Focus must start on Connect and follow the action column from top to bottom.
- Unavailable runtime feedback must remain persistent above the actions.

The public/private distinction is a label and routing refinement, not a server browser. Local Play remains unchanged. No staging switch, account flow, server discovery, or session list is proposed.

## Acceptance

- A user must find the default connection path without entering a server address on NET-01.
- A user must still find private-LAN hosting and return to Local Play.
- Labels and focus outlines must fit at 850 by 700 and 1280 by 720.
- Keyboard, controller, and pointer actions must reach the same destinations.

Representative: SS-015, existing NET-01 default state, 1920 by 1080. See the [capture matrix](../screenshots/README.md#public-service-extension--pending-coverage).
