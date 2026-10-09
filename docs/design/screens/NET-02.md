# NET-02 — Host setup

Functional contract: [network-host-setup](../../screens/network-host-setup.md), including NET-HOST-IF-001–024 and NET-OWN-001. State `NET-02-password` consumes [NET-PASS-001 and NET-DIR-001/015](../../network-host-directory.md). Listening startup consumes [HSL-IF-001–005](../../network-host-service-lifecycle.md); publication consumes [NET-DIR-017–020](../../network-host-directory.md). Wireframes: [NET-02 editable](../wireframes/NET-02/NET-02.svg), [NET-02-LA wildcard popup](../wireframes/NET-02/NET-02-LA.svg), [NET-02-LE explicit popup](../wireframes/NET-02/NET-02-LE.svg), and [NET-02-P pending startup](../wireframes/NET-02/NET-02-P.svg). These variants do not add functional screen identities. NET-02-P remains unchanged.

Visual impact is localized to the Listening interface field, popup, and listening validation feedback. This section supersedes the legacy single-address selector presentation in `docs/design.md`; unrelated styling and input rules remain authoritative. Product owns mode initialization, retention, eligibility, startup, and publication outcomes. No design-system token changes are required.

## Presentation and allocation

The [shared visual baseline](../../design.md#unified-network-presentation) applies.

- **UX-NET-02-001** The host task must keep a fixed title and endpoint region above two equal-height setup panels.
- **UX-NET-02-002** Endpoint labels must occupy a fixed left column while the remaining width contains separately framed Port, Listening interface, and Password values.
- **UX-NET-02-003** The editable Port must keep its five digits visible without horizontal scrolling.
- **UX-NET-02-004** The collapsed interface selector must show the listening mode and selection summary defined below.
- **UX-NET-02-005** Persons and Local players and controls must each have a title strip and an independently contained white list body.
- **UX-NET-02-006** Each player row must keep its existing control-assignment target separate from its Remove button.
- **UX-NET-02-007** The setup lists must give remaining horizontal space to person and control values after row-action bounds are reserved.
- **UX-NET-02-008** The fixed lower region must keep local-player count, validation, and help above the centered Start session and Back action column.
- **UX-NET-02-009** The listening popup must overlay lower body content inside the canvas without moving the setup headings or footer.
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

The editable representative must show that fields and row actions remain recognizable while Password is focused. The two popup representatives must show wildcard coverage and an independent explicit subset. The progress representative retains its prior scope when its presentation remains unchanged. Capture details are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).

## Listening summary and popup

These requirements apply to `NET-02-listen-all`, `NET-02-listen-explicit`, `NET-02-listen-empty`, `NET-02-listen-invalid`, and `NET-02-listening-popup`. NET-02-LA represents the popup in wildcard mode. NET-02-LE represents the popup in explicit mode. Empty, unavailable, enumeration-failure, and overflow variants use the same structure; they do not need additional permanent wireframes.

- **UX-NET-02-012** Wildcard summary must read `Listen on all (0.0.0.0)`.
- **UX-NET-02-013** A valid single explicit selection must show its complete IPv4 literal before its scope label.
- **UX-NET-02-014** Multiple explicit selections must use `Explicit: N addresses` without implying that every available address is selected.
- **UX-NET-02-015** Empty explicit selection must use `Explicit: no addresses selected`.
- **UX-NET-02-016** Invalid explicit selection must use `Explicit: selection unavailable` and retain a readable validation reason below the setup panels.
- **UX-NET-02-017** The collapsed selector must keep its existing full field width and a persistent popup affordance.
- **UX-NET-02-018** The popup must align with the selector's value region and start below that field with at least 8 logical px of clearance.
- **UX-NET-02-019** The popup must keep a fixed heading, `Listen on all` row, mode help, and closing-action region around a flexible address-list viewport.
- **UX-NET-02-020** The address-list viewport must reduce visible rows before it reduces text size or covers the fixed footer.
- **UX-NET-02-021** Each address row must show one checkbox, its complete IPv4 literal, and a scope label such as `Same machine`, `Private LAN`, or `Public IPv4`.
- **UX-NET-02-022** Long interface descriptions must clip after the IPv4 literal and scope label inside the row.
- **UX-NET-02-023** Wildcard mode must show eligible address checkboxes checked with a flat disabled frame and readable text.
- **UX-NET-02-024** Wildcard help must say `Uses 0.0.0.0. The operating system controls coverage.` and `Turn off Listen on all to choose individual addresses.`.
- **UX-NET-02-025** Explicit help must say `Only checked addresses will listen on the selected port.` and `Every selected bind must succeed.`.
- **UX-NET-02-026** An unavailable selected address must retain its complete literal, checked checkbox, and textual `Unavailable` marker in the explicit list.
- **UX-NET-02-027** An unavailable selected address must remain clearable rather than appear as a disabled checked choice.
- **UX-NET-02-028** Enumeration failure must show `Individual addresses could not be listed.` in the list region without disabling the wildcard checkbox or presenting wildcard setup as invalid.
- **UX-NET-02-029** Empty explicit selection must show `Select at least one listening interface.` in the persistent validation region.
- **UX-NET-02-030** A stale explicit selection must show the product's exact `Selected listening interface is no longer available. Choose another interface.` reason in the persistent validation region.
- **UX-NET-02-031** Unknown current eligibility in explicit mode must show `Listening interfaces could not be verified. Use Listen on all or correct the selection.` without claiming that a specific address has disappeared.
- **UX-NET-02-032** The popup must provide a framed `Close` action and visible `Changes are kept when closed.` help rather than Apply or Cancel actions.
- **UX-NET-02-033** The popup must contain pointer interaction and focus while it is open so that covered setup controls and Start session cannot activate.
- **UX-NET-02-034** The popup must use the existing network panel, inset list, checkbox, disabled-state, and non-color focus treatments without new visual tokens.

The mode row is separate from the individual-address list. A checked wildcard row is not a select-all shortcut for explicit mode. The wildcard list illustrates OS coverage; it is not a complete interface-coverage guarantee. Do not add refresh, discovery, per-address ports, publication-address selection, or network-configuration actions.

## Popup input, focus, and containment

- **UX-NET-02-035** Opening the popup must place focus on `Listen on all` without toggling it from the opening input.
- **UX-NET-02-036** Tab, reverse Tab, and keyboard or controller directional navigation must reach the mode row, each enabled address row in displayed order, and Close.
- **UX-NET-02-037** Confirm, Enter, or Space on an enabled checkbox must toggle that checkbox without closing the popup.
- **UX-NET-02-038** Navigation to an off-screen address must scroll the list to show the full focused row and its focus outline.
- **UX-NET-02-039** Pointer activation bounds must include the checkbox and its label without overlapping another row.
- **UX-NET-02-040** Escape, controller Back, or Close must close the popup and return focus to Listening interface without activating the screen's Back action.
- **UX-NET-02-041** Disabled wildcard address rows must not activate or imply independently editable selections.
- **UX-NET-02-042** A focus retained on a row that becomes disabled must remain visible under the shared disabled-focus rule.
- **UX-NET-02-043** Mode changes must keep focus on Listen on all so that enabling address choices does not trigger an address toggle.
- **UX-NET-02-044** The popup must reserve space for readable list-position feedback when its address rows overflow.
- **UX-NET-02-045** The popup must keep its fixed controls and wrapped help inside the existing 850 × 700 logical canvas at every supported desktop scale.

The popup ends above the local-player count and validation region. It can cover Password and the roster bodies temporarily; closing it restores their visibility without changing their allocation. The diagrams show approximate space allocation, not production typography or appearance. No mobile presentation is introduced.

## Listening UX acceptance

- **UX-NET-02-AC-001** At 850 × 700, 1280 × 720, and 1280 × 900, the collapsed summaries, popup heading, mode row, full IPv4 literals, Close action, and persistent validation must remain readable without overlap.
- **UX-NET-02-AC-002** Wildcard and explicit checkboxes must be distinguishable by their checked, disabled, focused, and enabled treatments without relying on color.
- **UX-NET-02-AC-003** Keyboard, controller, and pointer input must permit an independent explicit subset and popup closure without starting a service or leaving NET-02.
- **UX-NET-02-AC-004** Empty, stale, and unknown-eligibility explicit states must show a disabled Start reason and a discoverable recovery path through Listening interface.
- **UX-NET-02-AC-005** Enumeration failure in wildcard mode must preserve the wildcard summary and enabled listening-mode control without adding a listening-validation blocker.
- **UX-NET-02-AC-006** Overflow and maximum-length descriptions must preserve full addresses, focus bounds, fixed mode help, Close, and the underlying status/footer.

Focused QA must also cover first explicit initialization, later mode restoration, a newly eligible unchecked address, stale-selection clearing, return focus, popup opening-input protection, maximum local slots, long names/control labels, invalid Port, and Cancel/Edit setup retention. Product criteria NET-HOST-IF-AC-001–007 and HSL-AC-019 own bind success, atomic cleanup, and reset behavior; screenshots alone cannot prove them. NET-04 owns publication-unavailable feedback and keeps its existing wireframes.
