# NET-04 — Lobby and retained results

Functional authority: [NET-04](../../screens/network-lobby.md), `NET-OWN-002`–`NET-OWN-009`, `NET-DIR-001`–`NET-DIR-007/015`, and `NET-PUB-001`–`NET-PUB-015`. Pilot states: `NET-04-PUBLIC-CONTROLLER` and `NET-04-PUBLIC-GUEST`. Structural sources: [NET-04](../wireframes/NET-04/NET-04.svg) and develop's [NET-04-R](../wireframes/NET-04/NET-04-R.svg).

Visual impact: preserve develop's unified groups, controls, retained results, and reorder indication. Add role and consequence copy only in the pilot header.

## Presentation and allocation

The [unified visual baseline](../../design.md#unified-network-presentation) applies to both wireframes.

- **UX-NET-04-001** The fixed header must keep endpoint, role, totals, and applicable status above the body.
- **UX-NET-04-002** The body must retain approximately two-thirds membership/roster and one-third host settings with an 8-logical-pixel gap.
- **UX-NET-04-003** Participant and roster sections must use fixed headings above inset list bodies.
- **UX-NET-04-004** Role, Connection, Readiness, and Owned must remain separate participant columns.
- **UX-NET-04-005** Owned editable values must retain a field boundary distinct from remote read-only values.
- **UX-NET-04-006** Host settings must retain framed value controls without new arrows or toggle targets.
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

## Dedicated pilot role presentation

- Pilot headers must show `Public session` and the service-confirmed `Host` or `Guest` role.
- Endpoint must occupy a separate bounded line from role and totals.
- Host guidance must say `You control this session. Leaving ends it for everyone.`
- Guest guidance must say `The host controls this session. It ends when the host leaves.`
- The guidance must wrap within at most two reserved header lines.
- A pilot Host must not be described as running the server process on this computer.
- Host controls must appear only after authoritative role confirmation.
- The pilot must not show a directory listing claim or Retry publication target.
- Invitation values must remain absent.
- End session must retain its everyone consequence confirmation.
- Guest Leave must retain its participant-only consequence confirmation.
- The UI must not add host transfer or migration.

## Acceptance

Preserve complete headings, owned-slot editing, guest read-only settings, readiness reasons, result access, and footer at both minimum sizes. Long endpoints must stay inside the header. Maximum membership must scroll under fixed headings. Ordinary completed public results may represent pilot copy; they do not replace maximum-name, fourteen-winner, departed-winner, or interrupted-result checks. Verify player-hosted publication recovery and pilot absence of publication actions separately. Capture and state dependencies are in the [coordinated matrix](../screenshots/README.md#network-presentation-current-capture-matrix).
