# NET-04 — Lobby and retained results

Functional contract: [network-lobby](../../screens/network-lobby.md). Existing requirements NET-OWN-002–009 remain applicable. [NET-DIR-001–007 and NET-DIR-015](../../network-host-directory.md) own registration and recovery. Structural sources: [NET-04](../../screens/wireframes/network-lobby.md) and [NET-04-R](../wireframes/NET-04/NET-04-R.svg).

## Presentation and allocation

The [shared visual baseline](../../design.md#unified-network-presentation) applies to both existing wireframes. It changes panel and control appearance without changing the roster, settings, results, or action hierarchy.

- **UX-NET-04-001** The fixed session header must keep endpoint, role, totals, and directory feedback above the body.
- **UX-NET-04-002** The main body must retain its approximately two-thirds membership/roster and one-third host-settings allocation with an 8-logical-pixel gap.
- **UX-NET-04-003** Participant and roster sections must use fixed column headings over white inset bodies.
- **UX-NET-04-004** Role, Connection, Readiness, and Owned must remain separate participant columns.
- **UX-NET-04-005** An owned person or control value must have an editable boundary while another participant's value retains its read-only presentation.
- **UX-NET-04-006** Host settings must use framed existing value controls without adding new spinner arrows or toggle targets.
- **UX-NET-04-007** Guest settings must retain their read-only explanation without actionable raised frames.
- **UX-NET-04-008** Ready, Start match, Leave or End session, roster-order controls, and eligible Retry publication must each have a persistent action boundary.
- **UX-NET-04-009** The disabled Start reason and script-policy text must remain outside the roster and settings bodies above the footer.
- **UX-NET-04-010** NET-04-R must retain its approximately 215-logical-pixel result region below current membership and settings.
- **UX-NET-04-011** The retained result must use a separate title strip and fixed outcome labels above its scrolling white result body.
- **UX-NET-04-012** Current membership and historical result identities must remain in visibly separate regions.
- **UX-NET-04-013** Added group frames must reduce visible body rows before they cover result scrolling controls, readiness, or the footer.

Reading order remains session status, current membership and settings, retained results when present, persistent feedback, and actions. Existing focus traversal remains unchanged even where role-specific traversal differs from the spatial reading order. The older NET-04-R SVG omits the common banner for diagram space; this does not authorize removing the current banner or version.

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

The lobby representative must show a running host, directory failure, separately readable participant columns, and an unready participant's disabled Start reason. The retained-result representative must separate current membership from Completed history and keep the outcome headings, scroll feedback, readiness, and footer visible. Behavioral QA must prove that publication failure does not terminate the session. No new wireframe is needed for these status variants.

Focused QA must cover guest-owned editing, guest read-only settings, maximum membership, non-Team hidden settings, all Team settings, interrupted results, departed winners, long UTF-8 names, both result-scroll extremes, publication retry focus, and host/guest confirmations. The representative routes and supported viewports are in the [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix).
