# NET-05 — Public match presentation

Functional authority: [NET-05](../../screens/network-match.md), state `NET-05-PUBLIC`, NET-PUB-008 and HSL-PUB-005–010. Structural references: existing [NET-05](../../screens/wireframes/network-match.md) and [NET-05-C](../wireframes/NET-05/NET-05-C.svg).

Visual impact is localized status copy. No new layout or control is needed.

- The public status region must replace `LAN session` with `Public session`.
- The role label must show the confirmed Host or Guest role.
- The existing status allocation, word wrapping, three-row limit, and no-focus behavior must remain unchanged.
- Host End and guest Leave confirmations must retain NET-05-C structure and their distinct consequences.
- An explicit host departure must not appear to stop the remote service process.
- The arena, orientation, player visuals, ranking, round status, and session-only score notices must remain unchanged.

Acceptance uses SS-019 for NET-05 and SS-026 for NET-05-C. QA must verify both role confirmations and absence of hidden action activation. Retained public arena contexts in NET-07 and NET-09 must keep the same status identity and orientation.

NET-PUB-VIS-AC-001–004 in the [screen inventory](../../screens/README.md#public-pilot-visual-evidence-scope) permit an ordinary actual active match for the SS-019 public profile at `docs/design/screenshots/NET-05/NET-05.public.png`. The legacy degraded/Invisibility/held-weapon scenario and its evidence remain separate and unchanged. The ordinary public capture does not verify that combination or establish functional regression acceptance. See the [capture plan](../screenshots/README.md#remaining-representative-scope-and-reproduction).
