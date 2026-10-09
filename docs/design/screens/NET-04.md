# NET-04 — Lobby and retained results

Functional contract: [network-lobby](../../screens/network-lobby.md). Existing requirements NET-OWN-002–009 remain applicable. [NET-DIR-001–007 and NET-DIR-015](../../network-host-directory.md) own registration and recovery. Structural sources: [NET-04](../wireframes/NET-04/NET-04.svg) and [NET-04-R](../wireframes/NET-04/NET-04-R.svg). The [legacy NET-04 diagram](../../screens/wireframes/network-lobby.md) remains context for unchanged relationships.

## Presentation and allocation

The [shared visual baseline](../../design.md#unified-network-presentation) applies to both existing wireframes. It changes panel and control appearance without changing the roster, settings, results, or action hierarchy.

- **UX-NET-04-001** The fixed session header must keep endpoint, role, totals, and directory feedback above the body.
- **UX-NET-04-002** The main body must retain its approximately two-thirds membership/roster and one-third host-settings allocation with an 8-logical-pixel gap.
- **UX-NET-04-003** Participant and roster sections must use fixed column headings over white inset bodies.
- **UX-NET-04-004** Role, Connection, Readiness, and Owned must remain separate participant columns.
- **UX-NET-04-005** An owned person or control value must have an editable boundary while another participant's value retains its read-only presentation.
- **UX-NET-04-006** Host settings must use the MENU-01 inset-value and square checkbox-state vocabulary inside their existing whole-row controls without adding spinner arrows or toggle targets.
- **UX-NET-04-007** Guest settings must retain their read-only explanation without actionable raised frames.
- **UX-NET-04-008** Ready, Start match, Leave or End session, visible roster-order controls, and eligible Retry publication must each have a persistent action boundary, while NET-04-R must use a persistent host-only capability indication for its focus-dependent reorder action.
- **UX-NET-04-009** The disabled Start reason and script-policy text must remain outside the roster and settings bodies above the footer.
- **UX-NET-04-010** NET-04-R must retain its approximately 215-logical-pixel result region below current membership and settings.
- **UX-NET-04-011** The retained result must use a separate title strip and fixed outcome labels above its scrolling white result body.
- **UX-NET-04-012** Current membership and historical result identities must remain in visibly separate regions.
- **UX-NET-04-013** Added group frames must reduce visible body rows before they cover result scrolling controls, readiness, or the footer.

Reading order remains session status, current membership and settings, retained results when present, persistent feedback, and actions. Existing focus traversal remains unchanged even where role-specific traversal differs from the spatial reading order. The older NET-04-R SVG omits the common banner for diagram space; this does not authorize removing the current banner or version.

### Host game-settings appearance

These requirements apply to NET-04 and NET-04-R. Product retains authority over all values, permissions, mutations, and outcomes. Appearance reuse does not import the local Spinner or Textbox interaction.

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

| Existing setting | Visual mapping | Unchanged functional boundary |
|---|---|---|
| Mode | `↻ Mode: <value>` with inset value | Existing three-mode forward cycle |
| Team count | `↻ Teams: <value>` with inset value | Existing 2/3/4 cycle; Team mode only |
| Friendly fire | Square, label, On/Off | Existing whole-row toggle; Team mode only |
| Level plan | `↻ <plan value>` with inset value; the complete plan text identifies this row | Existing Fixed level / Shuffle all levels / Random level cycle |
| Fixed level | `↻ Level: <name>` with inset value | Existing level cycle also selects Fixed level |
| Round limit | `↻ Rounds 1–99: <value>` with inset value | Existing forward increment and 99-to-1 wrap |
| Assistance / Quick Liquid / Burnable Trees | Square, unchanged label, On/Off | Existing whole-row toggle |

The cycle marker replaces the existing row-marker space; it does not require extra allocation. Keep at least 2 logical px of inner caption space in compact rows. The inset surface identifies the current value, not a text-entry region. The square uses the main-menu state-dependent frame; it is not a new button. Preserve one activation on pointer press, no held repeat, existing Tab/directional traversal, Enter/Space/controller Confirm activation, host-only editing, guest read-only traversal, authoritative rejection, readiness clearing, and content-blocked availability. Left/right must not imply value decrement or increment. Keep Fixed level available under the existing plan behavior. Retained Team preferences and result/reorder behavior remain unchanged.

### Retained-lobby reorder indication

- **UX-NET-04-014** NET-04-R must identify host roster reordering before that action receives focus.
- **UX-NET-04-015** The indication must explain the existing access path: Tab or Down/Up to the Reorder item, then Enter/Space, or controller directional traversal followed by Confirm.
- **UX-NET-04-016** The indication must remain plain non-interactive help without a button frame, focus target, or pointer activation region.
- **UX-NET-04-017** The actual Reorder action must retain its baseline focus-dependent visibility, current-player identification, activation targets, pointer behavior, and reorder outcome.
- **UX-NET-04-018** The guest variant must retain its read-only roster explanation without a host reorder capability indication.

The indication belongs in the existing current-roster region, not the historical result table. It must not claim that left/right selects a move direction or that an unfocused row is clickable. The focused Reorder item identifies the current roster position and person as in the baseline. A control that remains focused while disabled must keep its focus outline without accepting activation.

## Listing feedback

- Host status must distinguish the running session from its directory listing.
- Host status must use `Directory: Registering…`, `Directory: Listed`, or `Directory: Unavailable` from confirmed registration state.
- Loss of confirmed listing freshness must not leave an unconditional Listed claim.
- Directory failure help must say `Session is still running. Share the endpoint for direct connection.`
- A failed publication must show `Retry publication` next to directory status.
- Retry publication must remain separate from Start match and must not imply a session restart.
- Retry publication must follow existing lobby actions in keyboard and controller traversal.
- A pending publication retry must show persistent progress and prevent duplicate activation.
- Listing status must occupy the existing header/status region above the roster.
- Status must wrap within that region without overlapping current membership or retained results.
- The status region may grow by one text row at the expense of scrollable membership height.
- Directory feedback must not replace Ready, Start match, Leave, or End session.
- Listing feedback must not capture focus or open a blocking modal.
- The same feedback rules must apply to NET-04-R.
- The UI must not claim that a listed session is reachable from every machine.

## Acceptance

The lobby representative must show a running host, separately readable participant columns, and an unready participant's disabled Start reason. Use directory failure when the authorized environment provides it. Both representatives must show Team controls, mixed toggle states, readable cycling values, and unchanged surrounding regions. The retained-result representative must separate current membership from Completed history and keep the outcome headings, scroll feedback, readiness, and footer visible. It must also show the non-interactive host reorder capability indication while focus is outside Reorder. A constrained guest supplement must show truthful read-only settings without host action cues. Behavioral QA must prove that publication failure does not terminate the session. No extra wireframe is needed for these role or status variants.

Focused QA must cover guest-owned editing, guest read-only settings, maximum membership, non-Team hidden settings, all Team settings, interrupted results, departed winners, long UTF-8 names, both result-scroll extremes, publication retry focus, and host/guest confirmations. Settings checks must verify 99-to-1 wrap, unchanged whole-row press targets, no typing or held repeat, Fixed level's existing plan change, readiness clearing, and keyboard/controller traversal. Reorder checks must verify the indication before focus, the existing action after traversal, unchanged focused pointer activation, and no new target on the help text. Disabled-but-focused controls must remain visibly focused and non-activatable. The affected representatives and guest supplement are in the [current matrix](../screenshots/README.md#host-game-settings-current-coverage).
