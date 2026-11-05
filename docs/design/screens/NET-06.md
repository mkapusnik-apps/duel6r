# NET-06 — Shared final summary

Functional authority: [NET-06](../../screens/network-summary.md), `NET-06-completed-host` / `NET-06-completed-guest`, hosting variant `NET-06-PUBLIC`, SUMMARY-VIEW-001–007 / SUMMARY-VIEW-AC-001–005, NET-PUB-008 and HSL-PUB-005–010. Structural authority: [NET-06](../wireframes/NET-06/NET-06.svg). Appearance authority: [Shared final presentation](../../design.md#shared-final-presentation), UX-FINAL-001–034 and UX-NET-06-001–007. These requirements replace the earlier menu shell; the legacy file stays in place and is not migrated.

Visual impact is high: the entire completed-result context becomes the common local/network arena-backed shell. The structure, sizing, font hierarchy, outcome grouping, scrolling containment and completion notice come from the single appearance owner above. No separate network card, banner, version region or menu scaling remains. Host and Guest are footer variants of the same stable wireframe, not separate result screens.

- Dedicated session context must use `Dedicated session` wherever an existing summary region shows connection context.
- The role must remain the service-confirmed Host or Guest.
- Result values, complete outcome identities, supported scrolling, and session-only persistence semantics must remain unchanged.
- Actions must retain the role-specific presentation in the functional contract.
- Confirmed termination must use the NET-08 or NET-09 presentation rather than claim that results were saved or the session can resume.

The current summary has no endpoint or service-type result region; do not add one. A permitted one-player completion uses the same shell, including No winner when authoritative. Departed winners retain their result identity and explicit Departed marker. A participant that only waited must not gain fabricated result participation. Final winner and cumulative leader must remain distinguishable. Interrupted results still use NET-04, not this completed screen.

The [coordinated matrix](../screenshots/README.md#arena-parity-and-shared-final-presentation) replaces the old SS-020 representative for the changed shell. It includes a Guest multiwinner/departed supplement because the role-specific footer and outcome overflow are materially different risks. Existing dedicated native acceptance and deferrals are unchanged; common Linux visuals do not establish those behaviors.

The updated product baseline permits retained final-arena context and UX-owned ordering of outcome identities → cumulative ranking → settings → per-round details. Existing lifecycle timing remains unchanged: network summary actions become available only at normal summary entry after the final delay. Every required field remains accessible. No new action, saved history, Elo field, result rule, advance action or destination is proposed.
