# NET-06 — Public summary presentation

Functional authority: [NET-06](../../screens/network-summary.md), state `NET-06-PUBLIC`, NET-PUB-008 and HSL-PUB-005–010. Structural reference: existing [NET-06](../../screens/wireframes/network-summary.md).

Visual impact is localized role and session-context copy. No new layout or control is needed.

- Public session context must use `Public session` rather than `LAN session` wherever the summary shows connection context.
- The role must remain the service-confirmed Host or Guest.
- Existing completed result headings, outcome rows, scrolling, and session-only persistence notices must remain unchanged.
- Actions must retain the role-specific presentation in the functional contract.
- Confirmed termination must use the NET-08 or NET-09 presentation rather than claim that results were saved or the session can resume.

SS-020 remains the representative. Acceptance requires complete result access and readable role-correct actions at the supported minimum sizes. A local trusted TLS session can produce this state through an actual completed match.

NET-PUB-VIS-AC-001–004 in the [screen inventory](../../screens/README.md#public-pilot-visual-evidence-scope) permit an ordinary actual completed summary for the SS-020 public profile at `docs/design/screenshots/NET-06/NET-06.public.png`. Preserve the legacy fourteen-winner/maximum-name/departed-winner scenario and its evidence separately. This changes capture scope only, not result requirements or functional regression acceptance. See the [capture plan](../screenshots/README.md#remaining-representative-scope-and-reproduction).
