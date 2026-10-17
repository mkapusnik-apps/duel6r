# NET-04 — Lobby and retained results

Functional authority: [NET-04](../../screens/network-lobby.md), `NET-OWN-002`–`NET-OWN-009`, `NET-DIR-001`–`NET-DIR-007/015`, and `NET-PUB-001`–`NET-PUB-015`. Pilot states: `NET-04-PUBLIC-CONTROLLER` and `NET-04-PUBLIC-GUEST`. Structural sources: [NET-04](../wireframes/NET-04/NET-04.svg) and develop's [NET-04-R](../wireframes/NET-04/NET-04-R.svg).

Visual impact: adopt the ordinary aligned-panel refinement from develop `675c405a921fae235f1ac1ca37eb7e84ed988065` over the headless baseline `a5d3e441beff05242449a616a3af1559fbaef663`. Preserve existing unified appearance, whole-row settings, retained results, role consequences, and reorder indication. The geometry requirements below compose the wider ordinary Participants region with the dedicated header clearance; they do not change permissions or add controls.

## Presentation and allocation

The [unified visual baseline](../../design.md#unified-network-presentation) applies to both wireframes.

- **UX-NET-04-001** The fixed header must keep endpoint, role, totals, and applicable status above the body.
- **UX-NET-04-002** The body must retain approximately two-thirds membership/roster and one-third host settings with at least 8 logical pixels of clearance under the owning geometry below.
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
- **UX-NET-04-031** Settings appearance must not add a help row, move the owning geometry defined below, or cover adjacent membership, results, status, or actions.

These current develop requirements apply equally to the service-confirmed controller. Preserve forward cycling, whole-row pointer press, no held repeat, existing Tab/directional traversal, Enter/Space/controller Confirm, host-only authority, readiness clearing, content-blocked availability, and hidden Team rows. Do not import local Spinner or Textbox interaction. Keep Fixed level available under the existing level-plan behavior. Existing Team preferences and retained-result/reorder access remain unchanged.

### Ordinary and retained lobby geometry

These values use the existing 850 by 700 logical canvas and bottom-left origin. The SVG uses top-left diagram coordinates. Its low-fidelity text and example content do not prescribe production typography or a fixed capture dataset.

| Region | X | Width | Bottom | Top |
|---|---:|---:|---:|---:|
| OrdinaryParticipants playerhostHostGuest | 40 | 526 | 392 | 482 |
| OrdinaryParticipants dedicatedHostGuest | 40 | 526 | 392 | 476 |
| OrdinaryPlayersAndControls | 40 | 526 | 196 | 384 |
| OrdinaryHostMatchSettings | 578 | 238 | 196 | 482 |
| RetainedParticipants playerhostHost | 40 | 358 | 410 | 476 |
| RetainedParticipants playerhostGuest | 40 | 358 | 410 | 500 |
| RetainedParticipants dedicatedHostGuest | 40 | 358 | 410 | 476 |
| RetainedHostMatchSettings | 406 | 410 | 350 | 482 |
| Retainedresult | 32 | 786 | 135 | 350 |

- **UX-NET-04-032** Ordinary Participants must match the 526-logical-pixel Players And Controls width for both roles and hosting variants.
- **UX-NET-04-033** Ordinary player-hosted Participants must align its top with Host Match Settings at 482 for both Host and Guest.
- **UX-NET-04-034** Dedicated Participants must retain top 476 for both roles to preserve its separate endpoint and consequence header.
- **UX-NET-04-035** Ordinary Host Match Settings must align its bottom with Players And Controls at 196 without moving setting rows or adding targets.
- **UX-NET-04-036** Ordinary Participants must show two visible fixed-height rows under its fixed headings and expose all supported membership through scrolling.
- **UX-NET-04-037** Ordinary participant and roster scrolling must account for two visible participant rows and six visible roster rows for either local role.
- **UX-NET-04-038** Participant headings, inset body, clipping, and interaction regions must use the same current owning bounds without narrowing the displayed columns.
- **UX-NET-04-039** Retained lobby must keep its existing role-dependent participant allocation, compact settings grid, result span, navigation, and footer.
- **UX-NET-04-040** Dedicated retained Guest must use the dedicated header clearance and one visible participant row, not the ordinary player-hosted Guest's taller header allocation.
- **UX-NET-04-041** Layout row counts and header variants must not grant Host controls or publication rights to a Guest.
- **UX-NET-04-042** Fixed feedback, script policy, Ready, Start, Leave or End, and result-navigation targets must remain unobscured at both minimum viewports.

Ordinary participant baselines remain 416 and 398 with the existing 18-logical-pixel pitch. Retained baselines remain 416 for Host and dedicated Guest, and 440/422 for player-hosted Guest. Keep retained-result scrolling independent from current membership. Preserve full access to fifteen participants/players, owned-slot focus, reserved/reconnecting labels, readiness reasons, and all existing current-versus-historical identity rules. A layout flag must not substitute for the service-confirmed permission role.

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
- Dedicated participants must not receive Retry publication, listing-owner, or operator controls.
- Password and reconnect values must remain absent.
- End session must retain its everyone consequence confirmation.
- Guest Leave must retain its participant-only consequence confirmation.
- The UI must not add host transfer or migration.

Dedicated publication is service-owned. This screen requires no new passive publication-status region. Directory origin may be identified as origin context, but it must not become a continuing Listed claim inferred from selection. Ready idle directory sessions have no participant/controller lobby in the client; NET-04 appears only after actual admission. Existing Host/Guest authority, settings, consequences, and both wireframes remain unchanged.

## Acceptance

Preserve complete headings, owned-slot editing, guest read-only settings, readiness reasons, result access, and footer at both minimum sizes. Long endpoints must stay inside the header. Maximum membership must scroll under fixed headings. Capture current Team settings with mixed toggle states and full cycle values on the controller, then constrained guest read-only context. Ordinary completed dedicated results do not replace maximum-name, fourteen-winner, departed-winner, or interrupted-result obligations. Preserve player-hosted publication recovery separately. Capture and state dependencies are in the [scoped matrix](../screenshots/README.md#dedicated-extension-scoped-capture-matrix).

## Product-eligible solo Start presentation

NET-SOLO and the hosting-specific functional contract own Start eligibility. UI state must follow that authority; participant count alone must not override it.

- **UX-NET-04-043** A product-eligible one-ready-player lobby must show the existing enabled Start match action without an obsolete solo-disabled reason or extra confirmation.
- **UX-NET-04-044** Solo Start must retain current action bounds, enabled/focused treatment, settings, and readiness feedback without a new control or policy setting.

Connection, validity, content, and readiness reasons remain when applicable. The representative must focus Start match in the actual eligible one-player state. NET-04 and NET-04-R SVG geometry remains unchanged. Retained Team settings/results and guest read-only coverage remain in their existing rows.

Imported develop solo references UX-NET-04-032/033 map to UX-NET-04-043/044. Existing UX-NET-04-032–042 retain their aligned-panel geometry meanings.
