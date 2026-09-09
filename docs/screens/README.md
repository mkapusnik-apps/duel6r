# Screen inventory

This inventory is authoritative for product screens and materially distinct full-screen visual states.
The target baseline uses the shared arena view requirements in `docs/features.md`.
The product has no implemented URL routes.
Each route value below therefore names a reproducible local workflow.
`NET-01`–`NET-09` and the `MENU-01` Network action are approved target specifications for issue #28. They are not implemented screens or evidence of playable network support; downstream issue #38 owns implementation.
Issue #30 defines protocol, compatibility, admission, and exact outcome copy for planned `NET-02`, `NET-03`, and `NET-08` states.
Issue #30 does not implement these graphical screens.
Issue #31 defines hosted-service lifecycle states, exact outcome copy, precedence, Retry eligibility, and destinations for planned `NET-02`, `NET-08`, and the intentional-end boundary of `NET-09`.
Issue #31 does not implement graphical network UI and does not add a wireframe.
Issue #32 defines authoritative headless match behavior, fixed result and failure copy, and future states for `NET-04`, `NET-05`, `NET-06`, and `NET-08`.
Issue #32 does not implement graphical network UI and does not add a wireframe.
Issue #34 defines stable replicated identities and presentation-independent state replication for the same durable screen identities.
Issue #34 does not implement graphical network UI and does not add a wireframe.
Issue #35 implements presentation-ready responsiveness, correction, degraded-network, resynchronization, and recovery handoffs for the existing `NET-05` target.
Issue #35 does not add or change a rendered screen, layout, graphical state, or wireframe.
Issue #38 owns graphical consumption and accessibility for the issue #35 handoffs.
Issue #38 retains ownership of graphical presentation, focus, controls, disabled reasons, screenshots, and visual conformance for these target states.
First-release network setup selects persons and controls without profile selectors.
First-release network match and retained arena contexts use deterministic built-in default network visuals.
Selected-profile appearance parity is deferred to issue #84.
Host startup and guest Connect finalize each participant's displayed local-player count for admission.
An admitted participant may edit person and control values only on existing owned slots.
`NET-04` provides no individual add, remove, or transfer action.
Reconnect restores the same reserved slot identities and ownership.

The implementation supports desktop display viewports only.
Each screen uses one desktop wireframe because the implementation does not define a mobile layout.
The `MENU-01` visual baseline was updated on 2026-08-23 from the approved retro menu direction in the connected Stitch project.
The update changes the menu presentation and control layout without adding a screen or changing product behavior.
The 2026-08-24 Game Settings update adds a default-enabled Burnable Trees checkbox below Quick Liquid and moves Rounds down by one row.
This update affects `MENU-01`, the menu background in `MENU-02`, and the visible menu area in `CONS-01`.
This update does not add a screen or a wireframe.
The 2026-08-26 approved menu presentation preserves that 850 by 700 logical layout while uniformly scaling and centering it, replacing the black matte with one session-persistent blurred gameplay still under a 55% black scrim, and adding a black canvas keyline.
The same presentation appears in `MENU-01`, behind messages in `MENU-02`, and below the unscaled full-width console in `CONS-01`.
This presentation change invalidates the prior representative screenshots for those three existing wireframes but does not add a screen or wireframe.
The 2026-08-29 Rounds update changes the visible focus, empty, restored-zero, and session-retained-value states in `MENU-01`.
This update does not add a screen or wireframe.
The `MENU-01` representative screenshot must show the applied positive value after gameplay returns during the same application session.
The corrected 2026-08-31 round-progress update changes only the non-final limited-match state of `OVER-02`.
The update adds `Rounds: <played>|<total>` in a dedicated row above the score heading strip.
The update right-aligns the label 16 px inside the round-summary panel's right bound.
The update hides the top-center arena progress while the summary is visible and restores it when the next round begins.
The update does not add a screen or a wireframe.
The 2026-08-31 consolidated Teams update gives `MENU-01` two conditional-layout wireframes.
The non-Team wireframe hides the Team settings and uses standard roster rows.
The Teams wireframe shows both Team settings and uses the selected team-count colors.
The update does not add a full-screen state or a new screen identifier.
The localized roster-order update keeps these two wireframes because one must show the controls absent and one must show them present.
The non-Team wireframe hides `Equalize` and `Shuffle` and removes their interaction targets.
The Teams wireframe shows the full `Equalize` and `Shuffle` labels.
The approved 2026-09-01 Team score-overview update affects the Team variants of `OVER-01` and non-final `OVER-02`.
The update adds an 8 px separator band with a centered 2 px rule between adjacent team groups.
The update does not change non-Team score overviews or `OVER-03`.
The update does not add a screen or a wireframe.
The consolidated-person-list update replaces the separate Elo and available-person panels with one `PERSONS` panel.
The update affects both `MENU-01` wireframes and does not add a screen or wireframe.
The approved PR #58 refinement splits the Persons and Players combined region 50:50.
Persons and Players each use 315 logical px, both 5 px setup-panel gaps remain, and Game Settings remains unchanged.
The refinement affects both `MENU-01` wireframes and does not add a screen or wireframe.
The localized PR #60 refinement puts the person-name field and `Add` in one row.
It puts `Remove`, `<<`, and `>>` in a separate row that aligns with the Teams roster-order row.
The person-action row keeps the same position when roster-order controls are hidden.
The refinement affects both `MENU-01` wireframes and does not add a screen or wireframe.
The localized PR #62 refinement moves the person-name row down by one standard list row.
It expands the Persons list by one standard list row.
It gives the person actions, roster-order actions, and batch controller-detection action one common button height with visible caption padding.
It renames the batch controller-detection action to `Detect All` and keeps `D` for each row action.
The refinement affects both `MENU-01` wireframes and does not add a screen or wireframe.
The localized PR #65 refinement puts `Remove` at the bottom-left and `>>` at the bottom-right of Persons.
It puts `<<` at the bottom-left and `Detect All` at the bottom-right of Players.
It centers `Equalize` and `Shuffle` as one group in the available span between the Players edge controls.
The refinement affects both `MENU-01` wireframes and does not add a screen or wireframe.
The final game-summary update affects the limited Deathmatch and Team variants of `OVER-03`.
The Team variant keeps the separator treatment already used in non-final `OVER-02`.
Both variants show a separate bottom `End of Game` notice.
Both variants show a contained final round counter at the same character height as the score heading.
The update does not change Predator, unlimited or non-final summaries, `OVER-01`, `OVER-02`, or the stable `OVER-03` wireframe count.

| Screen ID | Screen or state | Specification | Wireframe | Functional requirements | Primary source |
|---|---|---|---|---|---|
| `MENU-01` | Main menu and session setup; expanded consolidated person list; aligned and equal-height action buttons; conditional Team settings and roster-order controls; planned Network entry | [Specification](menu-main.md) | [Non-Team and Teams wireframes](wireframes/menu-main.md) | `SET-001`–`SET-091`, `AC-011`, `AC-040`–`AC-051`, `AC-053`–`AC-069`, plus target `NET-AC-002`, `NET-AC-009`, `NET-AC-015` | Target: `docs/features.md`; context: `source/Menu.cpp`; planned footer: `docs/network-play-first-release.md` |
| `MENU-02` | Menu blocking message over planned Network footer | [Specification](menu-message.md) | [Wireframe](wireframes/menu-message.md) | Existing local requirements plus target `NET-AC-015`, `NET-AC-017` | Current: `source/Menu.cpp`; target footer: `docs/network-play-first-release.md` |
| `PLAY-01` | Live shared arena gameplay for 2–15 players | [Specification](play-fullscreen.md) | [Wireframe](wireframes/play-fullscreen.md) | `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, `UI-001`–`UI-020` | Target: `docs/features.md`; context: `source/WorldRenderer.cpp` |
| `MODE-01` | Predator live gameplay | [Specification](mode-predator.md) | [Wireframe](wireframes/mode-predator.md) | `MOD-PR-001`–`MOD-PR-008`, `UI-001`–`UI-020` | `source/gamemodes/Predator.cpp` |
| `MODE-02` | Team live gameplay and ranking | [Specification](mode-team.md) | [Wireframe](wireframes/mode-team.md) | `SET-020`–`SET-021`, `SCO-005`–`SCO-006`, `SCO-013`–`SCO-017`, `MOD-TM-001`–`MOD-TM-011`, `UI-001`–`UI-020` | `source/gamemodes/TeamDeathMatch.cpp` |
| `PLAY-05` | Sudden-death rising water | [Specification](play-sudden-death.md) | [Wireframe](wireframes/play-sudden-death.md) | `ENV-002`–`ENV-007`, `ENV-009`–`ENV-013`, `UI-001`–`UI-020` | `source/Round.cpp:146-200` |
| `OVER-01` | Score-tab overlay | [Specification](overlay-score-tab.md) | [Wireframe](wireframes/overlay-score-tab.md) | `SCO-018`, `MOD-TM-010`–`MOD-TM-011`, `UI-011` | `source/Game.cpp:62-83` |
| `OVER-02` | Round-over summary | [Specification](overlay-round-over.md) | [Wireframe](wireframes/overlay-round-over.md) | `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012`, `UI-RND-001`–`UI-RND-010`, `AC-052` | Target: `docs/features.md`; context: `source/Game.cpp:130-169`, `source/WorldRenderer.cpp:120-178,514-547` |
| `OVER-03` | Game-over summary; limited Deathmatch and Team completion notice and final round counter | [Specification](overlay-game-over.md) | [Wireframe](wireframes/overlay-game-over.md) | `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014`, `UI-GAME-001`–`UI-GAME-007`, `AC-070`–`AC-072` | Target: `docs/features.md`; context: `source/Game.cpp:51-79,158-164`, `source/WorldRenderer.cpp:121-230,554-583` |
| `CONS-01` | Console over menu with planned Network footer | [Specification](console-menu.md) | [Wireframe](wireframes/console-menu.md) | Existing console requirements plus target `NET-AC-015`, `NET-AC-017` | Current: `source/console/ConsoleRenderer.cpp`; target footer: `docs/network-play-first-release.md` |
| `CONS-02` | Console over gameplay | [Specification](console-gameplay.md) | [Wireframe](wireframes/console-gameplay.md) | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |
| `NET-01` | Target network entry | [Specification](network-entry.md) | [Wireframe](wireframes/network-entry.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-015`, `NET-AC-017`, `NET-AC-019` | Target: `docs/network-play-first-release.md` |
| `NET-02` | Target host setup | [Specification](network-host-setup.md) | [Wireframe](wireframes/network-host-setup.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-009`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019`; `NET-VIS-001`–`NET-VIS-002`, `NET-VIS-009`–`NET-VIS-011`; `NET-VIS-AC-001`, `NET-VIS-AC-004`–`NET-VIS-AC-005`; `NET-OWN-001`; `NET-OWN-AC-001`; `INP-001`–`INP-010`; `NIN-OWN-006`; `NIN-BOUND-003`; issue #30 `AC-001`, `AC-009`, `AC-012`, `AC-017`, `AC-018`, `AC-020`, `AC-024`, `AC-025`, `ADM-OWN-001`, `ADM-OWN-AC-001`; `CMP-VIS-001`–`CMP-VIS-004`; `CMP-VIS-AC-001`; issue #31 `HSL-AC-003`–`HSL-AC-010`, `HSL-AC-014`, `HSL-AC-016`–`HSL-AC-018` | Targets: `docs/network-play-first-release.md`; `docs/network-authoritative-player-input.md`; `docs/network-compatibility-and-admission.md`; `docs/network-host-service-lifecycle.md` |
| `NET-03` | Target join setup and connecting | [Specification](network-join.md) | [Wireframe](wireframes/network-join.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019`; `NET-VIS-001`–`NET-VIS-002`, `NET-VIS-009`–`NET-VIS-011`; `NET-VIS-AC-001`, `NET-VIS-AC-004`–`NET-VIS-AC-005`; `NET-OWN-001`–`NET-OWN-003`; `NET-OWN-AC-001`–`NET-OWN-AC-002`; `INP-001`–`INP-010`; `NIN-OWN-006`; `NIN-BOUND-003`; issue #30 `AC-002`–`AC-023`, `ADM-OWN-001`, `ADM-OWN-AC-001`; `CMP-VIS-001`–`CMP-VIS-004`; `CMP-VIS-AC-001` | Targets: `docs/network-play-first-release.md`; `docs/network-authoritative-player-input.md`; `docs/network-compatibility-and-admission.md` |
| `NET-04` | Target lobby, readiness, and retained completed or interrupted result | [Specification](network-lobby.md) | [Wireframe](wireframes/network-lobby.md) | `NET-AC-004`–`NET-AC-008`, `NET-AC-013`–`NET-AC-014`, `NET-AC-016`–`NET-AC-019`; `NET-VIS-001`–`NET-VIS-002`, `NET-VIS-009`–`NET-VIS-011`; `NET-VIS-AC-001`, `NET-VIS-AC-004`–`NET-VIS-AC-005`; `NET-OWN-002`–`NET-OWN-009`; `NET-OWN-AC-002`–`NET-OWN-AC-005`; `INP-001`–`INP-010`; `NIN-OWN-006`, `NIN-OWN-008`–`NIN-OWN-009`; `NIN-OWN-AC-001`–`NIN-OWN-AC-002`; `NIN-BOUND-003`; issue #30 `ADM-OWN-001`–`ADM-OWN-006`, `ADM-OWN-AC-001`; `REP-OWN-001`–`REP-OWN-003`; `REP-OWN-AC-001`; `TRU-OWN-001`–`TRU-OWN-007`; issue #32 `AHM-AC-003`–`AHM-AC-007`, `AHM-AC-019`–`AHM-AC-026`, `AHM-AC-029`, `AHM-AC-032` | Targets: `docs/network-play-first-release.md`; `docs/network-authoritative-player-input.md`; `docs/network-state-replication.md`; `docs/network-trust-and-abuse-limits.md`; `docs/network-authoritative-headless-match.md`; `docs/network-compatibility-and-admission.md` |
| `NET-05` | Target network match shared arena and round-result phase, including degraded-network and visible-correction variants | [Specification](network-match.md) | [Wireframe](wireframes/network-match.md) | `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-009`–`NET-AC-014`, `NET-AC-016`–`NET-AC-019`; `NET-VIS-003`–`NET-VIS-011`; `NET-VIS-AC-002`–`NET-VIS-AC-005`; `REP-028`, `REP-041`, `REP-AC-002`, `REP-AC-012`; `REP-PRES-001`–`REP-PRES-006`; `REP-PRES-AC-001`–`REP-PRES-AC-003`; `CMP-VIS-001`–`CMP-VIS-004`; `CMP-VIS-AC-001`; `INP-011`–`INP-016`; `NIN-OWN-006`; `NIN-BOUND-003`; `NIN-COMP-AC-001`–`NIN-COMP-AC-004`; issue #32 `AHM-AC-012`–`AHM-AC-015`, `AHM-AC-019`, `AHM-AC-024`, `AHM-AC-032`; issue #35 `NRP-BUD-001`–`NRP-BUD-007`, `NRP-PRS-001`–`NRP-PRS-011`, `NRP-REC-001`–`NRP-REC-012`, `NRP-AUT-001`–`NRP-AUT-006`, `NRP-AC-001`–`NRP-AC-014` | Targets: `docs/network-play-first-release.md`; `docs/network-state-replication.md`; `docs/network-compatibility-and-admission.md`; `docs/network-authoritative-player-input.md`; `docs/network-authoritative-headless-match.md`; `docs/network-responsiveness-and-recovery.md` |
| `NET-06` | Target completed final session summary | [Specification](network-summary.md) | [Wireframe](wireframes/network-summary.md) | `NET-AC-010`, `NET-AC-011`, `NET-AC-014`, `NET-AC-016`–`NET-AC-018`; issue #32 `AHM-AC-016`, `AHM-AC-020`–`AHM-AC-023`, `AHM-AC-029`, `AHM-AC-032` | Targets: `docs/network-play-first-release.md`; `docs/network-authoritative-headless-match.md` |
| `NET-07` | Target guest reconnect | [Specification](network-reconnect.md) | [Wireframe](wireframes/network-reconnect.md) | `NET-AC-006`, `NET-AC-009`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019`; `NET-VIS-003`–`NET-VIS-010`; `NET-VIS-AC-002`–`NET-VIS-AC-004`; `NET-OWN-002`–`NET-OWN-003`, `NET-OWN-008`–`NET-OWN-009`; `NET-OWN-AC-002`, `NET-OWN-AC-005`; `REP-AC-004`; `REP-PRES-001`–`REP-PRES-006`; `REP-PRES-AC-001`–`REP-PRES-AC-003`; `TRU-OWN-007` | Targets: `docs/network-play-first-release.md`; `docs/network-state-replication.md`; `docs/network-trust-and-abuse-limits.md` |
| `NET-08` | Target connection, session, or authoritative-match failure | [Specification](network-failure.md) | [Wireframe](wireframes/network-failure.md) | `NET-AC-002`, `NET-AC-007`–`NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019`; issue #30 `AC-005`–`AC-010`, `AC-017`–`AC-023`, `AC-025`; issue #31 `HSL-AC-003`, `HSL-AC-006`, `HSL-AC-008`–`HSL-AC-011`, `HSL-AC-013`–`HSL-AC-016`, `HSL-AC-018`; issue #32 `AHM-AC-025`–`AHM-AC-031` | Targets: `docs/network-play-first-release.md`; `docs/network-compatibility-and-admission.md`; `docs/network-host-service-lifecycle.md`; `docs/network-authoritative-headless-match.md` |
| `NET-09` | Host-ended session outcome | [Specification](network-host-ended.md) | [Wireframe](wireframes/network-host-ended.md) | `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`–`NET-AC-019`; `NET-VIS-003`–`NET-VIS-010`; `NET-VIS-AC-002`–`NET-VIS-AC-004`; `REP-PRES-001`–`REP-PRES-006`; `REP-PRES-AC-001`–`REP-PRES-AC-003`; issue #31 `HSL-AC-012`–`HSL-AC-013` | Targets: `docs/network-play-first-release.md`; `docs/network-state-replication.md`; `docs/network-host-service-lifecycle.md` |

## Target network navigation

```text
MENU-01 → NET-01 → Host → NET-02 → NET-04
                   Join → NET-03 → NET-04
NET-02 startup Cancel → editable NET-02 with setup retained and no listener
NET-02 Starting → Cancel only; no setup edits, second Start, lobby, listening, or ready claim
NET-02 startup failure after cleanup → NET-08 → eligible Retry, retained NET-02, or NET-01
NET-03 connection Cancel → editable NET-03 with setup retained
NET-04 → NET-05 → completed NET-06 → following NET-04
NET-05 active-round or non-final-summary interruption → NET-04; never NET-06
NET-04 guest Leave confirm → guest NET-01; Cancel → NET-04
NET-05 guest Leave session confirm → guest NET-01; Cancel → NET-05
NET-06 guest Leave confirm → guest NET-01; Cancel → NET-06
connection/startup failure → NET-08 → Retry, Edit NET-02/NET-03, or NET-01
guest disconnect from NET-04, NET-05, or NET-06 → NET-07
NET-07 success → current authoritative NET-04/NET-05/NET-06
NET-07 Leave session confirm → guest NET-01; Cancel → NET-07 with deadline unchanged
NET-07 retryable resolution/refusal/unreachable/reset/timeout/host-crash/machine-or-listener-loss/temporary/no-response → remain NET-07
NET-07 terminal rejection or deadline expiry → NET-08 with reconnect Retry disabled
host End session confirm from NET-04/NET-05/NET-06 → host NET-01; guests host-ended NET-09
host-local supervised hosted-service failure → host NET-08; guests remain NET-07 until terminal rejection/expiry
normal application shutdown, crash, or forced termination → no guest NET-09 claim
```

Back from `NET-01` returns to `MENU-01`. `Play (F1)` remains local-only and does not enter this graph. Match admission closes at `NET-04` → `NET-05`; the target has no join-in-progress or host-migration path.

## Optional Stitch mapping

This mapping was reviewed on 2026-09-06 against canonical source SHA `ba35e6b5b3a1beafdcc761b27cc454c3d0235e01`.
The optional project is `projects/1219346282527961142`.
Its title is `Duel 6 Reloaded`.
Its visibility is private.
The authenticated access role is `OWNER`.
The mapping contains all 20 active stable screen IDs and all 21 active wireframes.
The mapping treats the repository as authoritative.
The mapping does not include retired `PLAY-02`, `PLAY-03`, or `PLAY-04` artifacts.
The mapping does not change the separate implementation screenshot manifest.

The classification describes only the value of the Stitch artifact for the current canonical wireframe.
`current` means that the reviewed artifact represents the complete current wireframe without a known discrepancy.
`partial` means that the artifact represents the primary task or hierarchy but does not establish complete conformance.
`exploratory` means that the artifact provides a visual direction that the product has not approved.
`stale` means that a later canonical change superseded part of the artifact.
`conflicting` means that the artifact contradicts current canonical intent.
`unavailable` means that the reviewed project has no applicable artifact.

| Stable screen ID | Stable wireframe ID | Product status | Stitch screen artifact | Classification | Current limitation |
|---|---|---|---|---|---|
| `MENU-01` | `MENU-01-A` Non-Team | Implemented | `681ae093051749fd922ab74454f47121` | `stale` | The artifact remains a retro-menu reference, but it predates the current three-panel layout, scaled photographic background, consolidated Persons list, action alignment, and planned Network footer. |
| `MENU-01` | `MENU-01-B` Teams | Implemented | `e26294cba3d946a0af458bcf33c275a0` | `stale` | The closest second menu exploration does not identify the Teams variant and does not establish the current conditional controls or Team setup. |
| `MENU-02` | `MENU-02` | Planned | `ec20957d0fef4060aa75de4b05750eb3` | `stale` | The artifact predates the planned Network footer and is not implementation evidence. |
| `PLAY-01` | `PLAY-01` | Implemented | `16de03a3b36e4447bbb1d28317b7bfea` | `partial` | The artifact is a useful native-baseline reference, but Stitch does not establish current 15-player implementation conformance. |
| `PLAY-05` | `PLAY-05` | Implemented | `323a56ca56204f669cc4c13bc14b44b9` | `partial` | The artifact represents sudden death in one shared arena, but it does not establish all current water, status, and overlay details. |
| `MODE-01` | `MODE-01` | Implemented | `0229745065204f29905aa41ad0b09bd8` | `partial` | The artifact represents Predator play, but it does not establish complete native rendering and HUD conformance. |
| `MODE-02` | `MODE-02` | Implemented | `50a748fc2039487986e9480120c909ec` | `partial` | The artifact represents Team play and ranking, but it does not establish complete current Team identity and HUD conformance. |
| `OVER-01` | `OVER-01` | Implemented | `172c3e16a6424bf1a7d95723038f3e43` | `stale` | The read-back does not confirm the later Team-group separator treatment. |
| `OVER-02` | `OVER-02` | Implemented | `46c697bc75274ba9a668b0641e077dc0` | `stale` | The read-back does not confirm the later separator treatment or current right-aligned round-progress row. |
| `OVER-03` | `OVER-03` | Implemented | `371ac0d850314ed49e0b6575e53caeac` | `stale` | The artifact predates the current limited Deathmatch notice and final round-counter size and containment requirements. |
| `CONS-01` | `CONS-01` | Planned | `d0ea32a65fe54e7b955fd41df3e59c17` | `stale` | The artifact predates the planned Network footer and is not implementation evidence. |
| `CONS-02` | `CONS-02` | Implemented | `45c9fba682d940b1bd3c38196d530800` | `partial` | The artifact represents the console over one gameplay context, but it does not establish complete native console and shared-arena conformance. |
| `NET-01` | `NET-01` | Planned | — | `unavailable` | The project has no applicable target network-entry artifact. |
| `NET-02` | `NET-02` | Planned | — | `unavailable` | The project has no applicable target host-setup artifact. |
| `NET-03` | `NET-03` | Planned | — | `unavailable` | The project has no applicable target join or connecting artifact. |
| `NET-04` | `NET-04` | Planned | — | `unavailable` | The project has no applicable target lobby artifact. |
| `NET-05` | `NET-05` | Planned | — | `unavailable` | The project has no applicable target network-match artifact. |
| `NET-06` | `NET-06` | Planned | — | `unavailable` | The project has no applicable target completed-summary artifact. |
| `NET-07` | `NET-07` | Planned | — | `unavailable` | The project has no applicable target reconnect artifact. |
| `NET-08` | `NET-08` | Planned | — | `unavailable` | The project has no applicable target failure artifact. |
| `NET-09` | `NET-09` | Planned | — | `unavailable` | The project has no applicable target host-ended artifact. |

This review used read-only project, inventory, and screen read-back operations.
No mass regeneration was useful because the available artifacts already provide supplementary references for implemented screens and the network UI remains planned.
No Stitch mutation was made.
The mapping is current for the complete active inventory and documents every known limitation.
This mapping does not imply product acceptance, runtime implementation, network implementation, or planned screenshot completion.

## Coverage rules

The current authoritative inventory contains 20 stable screen IDs and 21 wireframes.
Issue #16's 14-screen checklist is historical: it includes retired `PLAY-02`–`PLAY-04` and omits `NET-01`–`NET-09`.
It must not define the authoritative inventory or implementation screenshot coverage.

- Each listed screen must have one linked wireframe.
- Each wireframe must have exactly one representative screenshot entry, either conforming implementation evidence or a planned downstream capture.
- Minor loading, empty, disabled, focus, and error variants must stay in the applicable screen specification.
- A new full-screen flow state or a material layout change must receive a stable screen ID.
- An overlay that materially changes the primary task must receive a stable screen ID.
- Screenshot status must remain `Pending` until the requested implementation state is reached and captured.
- Planned target screens must use `Planned` until downstream implementation exists; planned entries are not current evidence.

`PLAY-02`, `PLAY-03`, and `PLAY-04` are retired identifiers.
They must not be reused for another screen.
`PLAY-01` is authoritative for each mode and each supported player count.

Unresolved functional and visual decisions are consolidated in [GitHub issue #7](https://github.com/mkapusnik-apps/duel6r/issues/7). The current implementation remains authoritative until those decisions are made and implemented.
