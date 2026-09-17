# NET-04 — Public-service lobby

Functional authority: [NET-04](../../screens/network-lobby.md), states `NET-04-PUBLIC-CONTROLLER` and `NET-04-PUBLIC-GUEST`, NET-PUB-001–015. Wireframes: [NET-04](../wireframes/NET-04/NET-04.svg) and unchanged [NET-04-R](../wireframes/NET-04/NET-04-R.svg). Existing host-alone, readiness, read-only, and retained-result variants remain applicable.

Visual impact: header and role clarification only. Preserve the participant/roster and settings allocation, retained result region, readiness controls, and action footer.

- The header must show the confirmed role as `Host` or `Guest`.
- The endpoint must occupy a separate bounded header line from role and membership totals.
- Public sessions must use `Public session`, not `LAN session`.
- The host note must state `You control this session. Leaving ends it for everyone.`
- The guest note must state `The host controls this session. It ends when the host leaves.`
- These notes must use at most two wrapped lines above the body.
- The public host must not be described as running the server on this computer.
- The Invite value must not appear in this screen.
- Participant Role, Connection, Readiness, and owned-player count must remain separate columns.
- Guest match settings must remain visibly read-only and must not receive focus.
- Host-only controls must appear only after authoritative role confirmation.
- Existing disabled reasons must remain next to the relevant action.
- The screen must not add a transfer-host action or an automatic migration claim.
- The End session confirmation must explain its effect on everyone.
- The guest Leave confirmation must retain its distinct participant-only consequence.

## Acceptance

- First admission and later admission must show the respective confirmed roles without an intermediate false Host claim.
- A guest must not receive editable host settings or an End session target.
- A host-alone lobby must remain distinct from a ready-to-start match.
- Long endpoint text must wrap or clip only inside its header region.
- Up to 15 participant and player rows must remain scrollable under fixed headings.
- NET-04-R must retain its existing historical-result allocation and complete outcome access.
- The host-departure note must not cover results, disabled reasons, or footer actions at 850 by 700.

Representative SS-018 remains a host lobby with three participants, six players, and one unready guest. A local trusted TLS service fixture may exercise the real public admission path. SS-025 remains the retained-result representative.

For this extension, NET-PUB-VIS-AC-001–004 in the [screen inventory](../../screens/README.md#public-pilot-visual-evidence-scope) permit an ordinary completed public result for the SS-025 public profile. Capture it at `docs/design/screenshots/NET-04/NET-04-R.public.png`. Preserve the legacy fourteen-winner/five-departed scenario, artifact, and assessment separately. See the [capture plan](../screenshots/README.md#remaining-representative-scope-and-reproduction). No result behavior or maximum-content requirement changes.
