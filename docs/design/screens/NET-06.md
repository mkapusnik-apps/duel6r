# NET-06 — Shared final summary

Functional authority: [NET-06](../../screens/network-summary.md), `NET-06-completed-host` / `NET-06-completed-guest`, hosting variant `NET-06-PUBLIC`, SUMMARY-VIEW-001–007 / SUMMARY-VIEW-AC-001–005, NET-PUB-008 and HSL-PUB-005–010. Structural authority: [NET-06](../wireframes/NET-06/NET-06.svg). Appearance authority: [Shared final presentation](../../design.md#shared-final-presentation), UX-FINAL-001–034 and UX-NET-06-001–007. These requirements replace the earlier menu shell; the legacy file stays in place and is not migrated.

The completed-result context uses the same compact OVER-02-derived scoreboard as Local Play, not the large final-window shell. Start with cumulative scores under exactly one `---SCORE---` strip, without a separate SCORE title or fixed scope/outcome/ranking hierarchy. Required network session-only copy and actual role actions occupy compact external regions; their presence does not prescribe empty local regions. Additional network fields may require a wider viewport, but history must not force a short primary table to maximum height. Supporting sections keep 16 px text; player-statistics tables keep 32 px rows without repeated score strips. No new tab, details screen, expansion control, menu canvas or banner is introduced.

- Dedicated session context must use `Dedicated session` wherever an existing summary region shows connection context.
- The role must remain the service-confirmed Host or Guest.
- Result values, complete outcome identities, supported scrolling, and session-only persistence semantics must remain unchanged.
- Actions must retain the role-specific presentation in the functional contract.
- Confirmed termination must use the NET-08 or NET-09 presentation rather than claim that results were saved or the session can resume.

The current summary has no endpoint or service-type result region; do not add one. A permitted one-player completion uses the same shell, including No winner when authoritative. Departed winners retain their result identity and explicit Departed marker. A participant that only waited must not gain fabricated result participation. Final winner and cumulative leader must remain distinguishable. Interrupted results still use NET-04, not this completed screen.

The [compact-final matrix](../screenshots/README.md#compact-final-scoreboard) owns affected replacement and focused evidence. The broader matrix's unrelated arena/platform obligations retain their scope; this change does not require replaying its twelve-round or native fourteen-winner journey solely for removed main-screen chrome. Changed detail placement and geometry still require current source-tied consumer coverage. Existing dedicated native acceptance and deferrals remain unchanged.

The default body starts with cumulative scores, followed through existing continuous navigation by result identities, settings and per-round details. Do not remove complete outcomes/identities, owner, Departed, rank, statistics, settings, seed or round history to simplify the main view. Last-round outcome remains in that round's detail rather than a duplicate fixed caption. Existing lifecycle timing remains unchanged: actions become available only at normal summary entry after the final delay. No new action, saved history, Elo field, result rule, advance action or destination is proposed.
