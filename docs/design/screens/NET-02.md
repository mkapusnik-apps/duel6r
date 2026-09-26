# NET-02 — Host setup

Functional contract: [network-host-setup](../../screens/network-host-setup.md), including NET-HOST-IF-001–012 and NET-OWN-001. State `NET-02-password` consumes [NET-PASS-001 and NET-DIR-001/015](../../network-host-directory.md). Wireframe: [NET-02](../wireframes/NET-02/NET-02.svg).

## Hierarchy and controls

- The screen must retain Port, Listening interface, Persons, Local players and controls, and its existing footer.
- Listening interface must present the eligible loopback, assigned private, and assigned public unicast IPv4 choices defined by NET-HOST-IF-002–003.
- Public address scope must not imply guaranteed Internet reachability.
- A field labeled `Password (optional)` must follow Listening interface.
- The password field must use the full endpoint-field width.
- Help below the field must say `Leave empty for no password.`
- The field must mask entered characters.
- The initial password field must be empty.
- The UI must not place a password in a heading, status line, directory row, or endpoint.
- Port must keep initial focus.
- Password must follow Listening interface in focus order.
- Existing person and control actions must follow Password in focus order.
- The endpoint block may grow by two text rows.
- The roster region must shrink by that same height.
- Roster headings and the footer must remain fixed.
- Long rosters must scroll inside their existing regions.
- The UI must reserve one wrapped help line for `Listing does not guarantee reachability. Direct play remains available.`
- Password validation must appear beside the field without exposing its value.
- Starting and Cancelling must retain their existing locked setup and Cancel behavior.
- Startup copy must not imply successful directory registration.

## Acceptance

At 850 × 700, the password label, mask, help, player count, validation, and footer must remain separate and readable. Expanded interface options must remain inside the canvas. Field focus must remain visible without color. Directory registration failure must not be presented as hosted-session startup failure when the host service is running. NET-04 owns registration feedback.
