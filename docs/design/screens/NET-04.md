# NET-04 — Lobby listing feedback

Functional contract: [network-lobby](../../screens/network-lobby.md). Existing requirements NET-OWN-002–009 remain applicable. [NET-DIR-001–007 and NET-DIR-015](../../network-host-directory.md) own registration and recovery. Structural sources: [NET-04](../../screens/wireframes/network-lobby.md) and [NET-04-R](../wireframes/NET-04/NET-04-R.svg).

This is a localized text refinement. It does not change the roster, settings, retained-result region, or action hierarchy.

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

Acceptance requires a running host lobby with directory failure and a retained-result lobby with listing feedback. Behavioral QA must prove that heartbeat failure does not terminate direct or local play. No new wireframe is needed for these status variants.
