# NET-02 — Host setup

Functional contract: [network-host-setup](../../screens/network-host-setup.md), including NET-HOST-IF-001–012 and NET-OWN-001. State `NET-02-password` consumes [NET-PASS-001 and NET-DIR-001/015](../../network-host-directory.md). Wireframes: [NET-02 editable](../wireframes/NET-02/NET-02.svg) and [NET-02-P pending startup](../wireframes/NET-02/NET-02-P.svg). NET-02-P covers the existing Starting and Cancelling states, not a new functional state.

## Presentation and allocation

The [shared visual baseline](../../design.md#unified-network-presentation) applies.

- **UX-NET-02-001** The host task must keep a fixed title and endpoint region above two equal-height setup panels.
- **UX-NET-02-002** Endpoint labels must occupy a fixed left column while the remaining width contains separately framed Port, Listening interface, and Password values.
- **UX-NET-02-003** The editable Port must keep its five digits visible without horizontal scrolling.
- **UX-NET-02-004** The collapsed interface selector must keep the complete IPv4 literal visible before its scope text.
- **UX-NET-02-005** Persons and Local players and controls must each have a title strip and an independently contained white list body.
- **UX-NET-02-006** Each player row must keep its existing control-assignment target separate from its Remove button.
- **UX-NET-02-007** The setup lists must give remaining horizontal space to person and control values after row-action bounds are reserved.
- **UX-NET-02-008** The fixed lower region must keep local-player count, validation, and help above the centered Start session and Back action column.
- **UX-NET-02-009** The expanded interface list must overlay only lower body content inside the canvas without moving the setup headings or footer.
- **UX-NET-02-010** NET-02-P must present the existing startup status and timing help in a fixed status group above the sole Cancel action.
- **UX-NET-02-011** Cancelling must use the same progress allocation without an enabled Cancel, Start session, or Back affordance.

NET-02-P preserves the current separate progress presentation. It does not introduce a new editable form, setup summary, progress percentage, or directory-success claim. Retained setup remains available through the existing completed-cancel transition.

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

The editable representative must show that fields and row actions remain recognizable while Password is focused. The progress representative must show real Starting status and focused Cancel without a success claim. Focused QA must check maximum local slots, long names/control labels, open-selector scrolling, an ineligible retained interface, invalid Port, disabled Start, and Cancelling through cleanup. Capture details are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).
