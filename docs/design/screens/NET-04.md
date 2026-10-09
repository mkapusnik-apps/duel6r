# NET-04 — Lobby and retained results

Functional authority: [NET-04](../../screens/network-lobby.md), `NET-OWN-002`–`NET-OWN-009`, `NET-DIR-001`–`NET-DIR-007/015`, and `NET-PUB-001`–`NET-PUB-015`. Pilot states: `NET-04-PUBLIC-CONTROLLER` and `NET-04-PUBLIC-GUEST`. Structural sources: [NET-04](../wireframes/NET-04/NET-04.svg) and develop's [NET-04-R](../wireframes/NET-04/NET-04-R.svg).

Visual impact: use develop `412abfaae857ab65ba21746819de54c3ac4446be` as the base. Preserve its unified groups, current host-settings appearance, retained results, and reorder indication. Add dedicated role and consequence copy only in the header.

## Presentation and allocation

The [unified visual baseline](../../design.md#unified-network-presentation) applies to both wireframes.

- **UX-NET-04-001** The fixed header must keep endpoint, role, totals, and applicable status above the body.
- **UX-NET-04-002** The body must retain approximately two-thirds membership/roster and one-third host settings with an 8-logical-pixel gap.
- **UX-NET-04-003** Participant and roster sections must use fixed headings above inset list bodies.
- **UX-NET-04-004** Role, Connection, Readiness, and Owned must remain separate participant columns.
- **UX-NET-04-005** Owned editable values must retain a field boundary distinct from remote read-only values.
- **UX-NET-04-006** Host settings must use the MENU-01 inset-value and square checkbox-state vocabulary inside their existing whole-row controls without adding spinner arrows or toggle targets.
- **UX-NET-04-007** Guest settings must retain a read-only explanation without actionable frames.
- **UX-NET-04-008** Existing actions must retain persistent boundaries, except NET-04-R's focus-dependent reorder action, which must retain its persistent host-only capability indication.
- **UX-NET-04-009** The disabled Start reason and script-policy text must remain above the footer outside the body lists.
- **UX-NET-04-010** NET-04-R must retain approximately 215 logical pixels for results below current membership and settings.
- **UX-NET-04-011** Retained results must keep a separate title strip and fixed outcome labels above their scrolling body.
- **UX-NET-04-012** Current membership and historical identities must remain in separate regions.
- **UX-NET-04-013** Added header copy and frames must reduce visible body rows before they cover results, readiness, or footer actions.
- **UX-NET-04-014** NET-04-R must identify host reordering before focus reaches that action.
- **UX-NET-04-015** Reorder help must retain the existing Tab/Up/Down or controller-direction access path followed by Enter/Space or Confirm.
- **UX-NET-04-016** Reorder help must remain non-interactive without a button frame or input target.
- **UX-NET-04-017** The actual Reorder action must retain baseline focus-dependent visibility, current-player identification, pointer behavior, and outcome.
- **UX-NET-04-018** Guests must retain the read-only roster explanation without host reorder help.

Reading order remains header, current membership/settings, retained results when present, feedback, and actions. Keep the banner/version even where the older retained-result diagram omits it for space. Keep baseline traversal, including visible focus retained on a disabled action without activation.

### Current host game-settings appearance

- **UX-NET-04-019** Settings must retain their existing order and whole-row activation bounds.
- **UX-NET-04-020** NET-04 must retain its vertical settings stack, and NET-04-R must retain its compact row-major two-column settings grid.
- **UX-NET-04-021** Mode, Team count, Level plan, Fixed level, and Round limit must show inset current values without separate directional buttons or dropdown indicators.
- **UX-NET-04-022** Host cycling rows must show a plain `↻` cycle marker inside the existing row without a separate frame or target.
- **UX-NET-04-023** Friendly fire, Assistance, Quick Liquid, and Burnable Trees must show a 16-logical-pixel square state indicator beside their labels inside the existing toggle row.
- **UX-NET-04-024** Toggle rows must retain readable On or Off text as well as the square state indicator.
- **UX-NET-04-025** Round limit must show `Rounds 1–99` and its current value without a caret, focus underscore, or typing instruction.
- **UX-NET-04-026** Each setting must use one row-level focus keyline without separate label, value, or indicator focus stops.
- **UX-NET-04-027** Hidden Team rows must leave no visible placeholder or interaction target.
- **UX-NET-04-028** Guest settings must show current values and On or Off text without host cycle markers or enabled checkbox treatment.
- **UX-NET-04-029** Full supported mode and level-plan values must remain readable at the standard text size in both layouts.
- **UX-NET-04-030** Long fixed-level names must remain contained within the existing value bounds without splitting UTF-8 characters.
- **UX-NET-04-031** Settings appearance must not add a help row, move the panel bounds, or cover adjacent membership, results, status, or actions.

These current develop requirements apply equally to the service-confirmed controller. Preserve forward cycling, whole-row pointer press, no held repeat, existing Tab/directional traversal, Enter/Space/controller Confirm, host-only authority, readiness clearing, content-blocked availability, and hidden Team rows. Do not import local Spinner or Textbox interaction. Keep Fixed level available under the existing level-plan behavior. Existing Team preferences and retained-result/reorder access remain unchanged.

## Player-hosted listing feedback

- Player-hosted status must distinguish the session from its directory listing.
- It must use confirmed `Directory: Registering…`, `Directory: Listed`, or `Directory: Unavailable` state.
- Publication failure must retain `Session is still running. Share the endpoint for direct connection.`
- Eligible Retry publication must remain beside directory feedback and separate from Start match.
- Pending publication retry must retain persistent progress and prevent duplicate activation.
- Retry publication must retain its position after existing lobby actions in traversal.
- Status overflow may take one body row without moving fixed results or actions.
- Listing feedback must not open a modal or promise endpoint reachability.
- NET-04-R must retain the same feedback treatment.

## Dedicated role presentation

- Dedicated headers must show `Dedicated session` and the service-confirmed `Host` or `Guest` role.
- Endpoint must occupy a separate bounded line from role and totals.
- Host guidance must say `You control this session. Leaving ends it for everyone.`
- Guest guidance must say `The host controls this session. It ends when the host leaves.`
- The guidance must wrap within at most two reserved header lines.
- A pilot Host must not be described as running the server process on this computer.
- Host controls must appear only after authoritative role confirmation.
- Dedicated sessions must not show a directory listing claim or Retry publication target.
- Invitation values must remain absent.
- End session must retain its everyone consequence confirmation.
- Guest Leave must retain its participant-only consequence confirmation.
- The UI must not add host transfer or migration.

## Acceptance

Preserve complete headings, owned-slot editing, guest read-only settings, readiness reasons, result access, and footer at both minimum sizes. Long endpoints must stay inside the header. Maximum membership must scroll under fixed headings. Capture current Team settings with mixed toggle states and full cycle values on the controller, then constrained guest read-only context. Ordinary completed dedicated results do not replace maximum-name, fourteen-winner, departed-winner, or interrupted-result obligations. Preserve player-hosted publication recovery separately. Capture and state dependencies are in the [scoped matrix](../screenshots/README.md#dedicated-extension-scoped-capture-matrix).
