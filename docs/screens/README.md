# Screen inventory

This inventory is authoritative for product screens and materially distinct full-screen visual states.
The baseline uses the native implementation at branch `feature-documentation-audit-fixes`, capture source SHA `12cd6dca742b90293f552fefa3bfd3a8871aa7a2`.
The product has no implemented URL routes.
Each route value below therefore names a reproducible local workflow.

The implementation supports desktop display viewports only.
Each screen uses one desktop wireframe because the implementation does not define a mobile layout.

| Screen ID | Screen or state | Specification | Wireframe | Stitch artifact | Functional requirements | Primary source |
|---|---|---|---|---|---|---|
| `MENU-01` | Main menu and session setup | [Specification](menu-main.md) | [Wireframe](wireframes/menu-main.md) | [`MENU-01 — Main menu and session setup`](https://stitch.withgoogle.com/projects/1219346282527961142/screens/681ae093051749fd922ab74454f47121) | `SET-001`–`SET-023`, `LIF-023`–`LIF-029`, `INP-001`–`INP-011`, `SCO-019`–`SCO-024`, `PER-001`–`PER-005` | `source/Menu.cpp` |
| `MENU-02` | Menu blocking message | [Specification](menu-message.md) | [Wireframe](wireframes/menu-message.md) | Pending: `MENU-02 — Menu blocking message` | `SET-003`, `SET-006`–`SET-007`, `SET-022`, `LIF-023`–`LIF-029`, `INP-008`–`INP-009` | `source/Menu.cpp:368-486` |
| `PLAY-01` | Live full-screen gameplay | [Specification](play-fullscreen.md) | [Wireframe](wireframes/play-fullscreen.md) | Pending: `PLAY-01 — Live full-screen gameplay` | `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, `UI-001`, `UI-008`–`UI-015` | `source/WorldRenderer.cpp` |
| `PLAY-02` | Two-player split-screen | [Specification](play-split-2.md) | [Wireframe](wireframes/play-split-2.md) | [`PLAY-02 — Two-player split-screen`](https://stitch.withgoogle.com/projects/1219346282527961142/screens/bfb50cc23087486a9df5decc3a6e79f5) | `UI-002`–`UI-007`, `UI-009`–`UI-010`, `UI-014` | `source/Round.cpp:100-127` |
| `PLAY-03` | Three-player split-screen | [Specification](play-split-3.md) | [Wireframe](wireframes/play-split-3.md) | Pending: `PLAY-03 — Three-player split-screen` | `UI-002`–`UI-007`, `UI-009`–`UI-010`, `UI-014` | `source/Round.cpp:129-134` |
| `PLAY-04` | Four-player split-screen | [Specification](play-split-4.md) | [Wireframe](wireframes/play-split-4.md) | Pending: `PLAY-04 — Four-player split-screen` | `UI-002`–`UI-007`, `UI-009`–`UI-010`, `UI-014` | `source/Round.cpp:136-142` |
| `PLAY-05` | Sudden-death rising water | [Specification](play-sudden-death.md) | [Wireframe](wireframes/play-sudden-death.md) | Pending: `PLAY-05 — Sudden-death rising water` | `ENV-002`–`ENV-007`, `ENV-009`–`ENV-013`, `UI-008`–`UI-015` | `source/Round.cpp:146-200` |
| `MODE-01` | Predator live gameplay | [Specification](mode-predator.md) | [Wireframe](wireframes/mode-predator.md) | Pending: `MODE-01 — Predator live gameplay` | `MOD-PR-001`–`MOD-PR-008`, `UI-008`–`UI-015` | `source/gamemodes/Predator.cpp` |
| `MODE-02` | Team live gameplay and ranking | [Specification](mode-team.md) | [Wireframe](wireframes/mode-team.md) | [`MODE-02 — Team live gameplay and ranking`](https://stitch.withgoogle.com/projects/1219346282527961142/screens/50a748fc2039487986e9480120c909ec) | `SET-020`–`SET-021`, `SCO-005`–`SCO-006`, `SCO-013`–`SCO-017`, `MOD-TM-001`–`MOD-TM-011`, `UI-008`–`UI-015` | `source/gamemodes/TeamDeathMatch.cpp` |
| `OVER-01` | Score-tab overlay | [Specification](overlay-score-tab.md) | [Wireframe](wireframes/overlay-score-tab.md) | Pending: `OVER-01 — Score-tab overlay` | `SCO-018`, `MOD-TM-010`–`MOD-TM-011`, `UI-011` | `source/Game.cpp:62-83` |
| `OVER-02` | Round-over summary | [Specification](overlay-round-over.md) | [Wireframe](wireframes/overlay-round-over.md) | Pending: `OVER-02 — Round-over summary` | `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012` | `source/WorldRenderer.cpp:120-169,472-505,565-570` |
| `OVER-03` | Game-over summary | [Specification](overlay-game-over.md) | [Wireframe](wireframes/overlay-game-over.md) | Pending: `OVER-03 — Game-over summary` | `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014` | `source/Game.cpp:51-79,158-164` |
| `CONS-01` | Console over menu | [Specification](console-menu.md) | [Wireframe](wireframes/console-menu.md) | Pending: `CONS-01 — Console over menu` | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |
| `CONS-02` | Console over gameplay | [Specification](console-gameplay.md) | [Wireframe](wireframes/console-gameplay.md) | Pending: `CONS-02 — Console over gameplay` | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |

## Stitch workspace mapping

The artifacts belong to [Stitch project `1219346282527961142`](https://stitch.withgoogle.com/projects/1219346282527961142).
Each stable artifact identifier maps to exactly one authoritative wireframe.
The artifact prompts and the linked screen specifications record variants and workflows.
Stitch artifacts are design references and are not implementation screenshot evidence.
The 2026-08-22 synchronization attempts submitted generation requests for all 14 identifiers.
The final focused pass resubmitted each of the 11 missing identifiers individually and checked the screen listing after each request.
Stitch returns three unique screen records.
Eleven required records remain pending after the final completion checks.
The available `MENU-01` artifact does not conform because its settings and action structure differ from the documented native menu.
The available `PLAY-02` artifact does not conform because it shows four cameras instead of two centered cameras.
Final in-place correction requests for `MENU-01` and `PLAY-02` timed out without a confirmed update.
The owner must complete the pending records and the two in-place corrections in the Stitch UI if the asynchronous requests do not appear later.
The project coverage and conformance therefore remain blocked.

## Coverage rules

- Each listed screen must have one linked wireframe.
- Each wireframe must have exactly one representative implementation screenshot entry.
- Minor loading, empty, disabled, focus, and error variants must stay in the applicable screen specification.
- A new full-screen flow state or a material layout change must receive a stable screen ID.
- An overlay that materially changes the primary task must receive a stable screen ID.
- Screenshot status must remain `Pending` until the requested implementation state is reached and captured.

Unresolved functional and visual decisions are consolidated in [GitHub issue #7](https://github.com/mkapusnik-apps/duel6r/issues/7). The current implementation remains authoritative until those decisions are made and implemented.
