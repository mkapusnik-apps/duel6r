# Screen inventory

This inventory is authoritative for product screens and materially distinct full-screen visual states.
The target baseline uses the shared arena view requirements in `docs/features.md`.
The product has no implemented URL routes.
Each route value below therefore names a reproducible local workflow.

The implementation supports desktop display viewports only.
Each screen uses one desktop wireframe because the implementation does not define a mobile layout.
The `MENU-01` visual baseline was updated on 2026-08-23 from the approved retro menu direction in the connected Stitch project.
The update changes the menu presentation and control layout without adding a screen or changing product behavior.
The 2026-08-24 Game Settings update adds a default-enabled Burnable Trees checkbox below Quick Liquid and moves Rounds down by one row.
This update affects `MENU-01`, the menu background in `MENU-02`, and the visible menu area in `CONS-01`.
This update does not add a screen or a wireframe.

| Screen ID | Screen or state | Specification | Wireframe | Functional requirements | Primary source |
|---|---|---|---|---|---|
| `MENU-01` | Main menu and session setup | [Specification](menu-main.md) | [Wireframe](wireframes/menu-main.md) | `SET-001`–`SET-029`, `LIF-023`–`LIF-029`, `INP-001`–`INP-011`, `SCO-019`–`SCO-024`, `PER-001`–`PER-005` | Stitch screen `681ae093051749fd922ab74454f47121`; behavior: `source/Menu.cpp` |
| `MENU-02` | Menu blocking message | [Specification](menu-message.md) | [Wireframe](wireframes/menu-message.md) | `SET-003`, `SET-006`–`SET-007`, `SET-022`, `LIF-023`–`LIF-029`, `INP-008`–`INP-009` | `source/Menu.cpp:368-486` |
| `PLAY-01` | Live shared arena gameplay for 2–15 players | [Specification](play-fullscreen.md) | [Wireframe](wireframes/play-fullscreen.md) | `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, `UI-001`–`UI-020` | Target: `docs/features.md`; context: `source/WorldRenderer.cpp` |
| `MODE-01` | Predator live gameplay | [Specification](mode-predator.md) | [Wireframe](wireframes/mode-predator.md) | `MOD-PR-001`–`MOD-PR-008`, `UI-001`–`UI-020` | `source/gamemodes/Predator.cpp` |
| `MODE-02` | Team live gameplay and ranking | [Specification](mode-team.md) | [Wireframe](wireframes/mode-team.md) | `SET-020`–`SET-021`, `SCO-005`–`SCO-006`, `SCO-013`–`SCO-017`, `MOD-TM-001`–`MOD-TM-011`, `UI-001`–`UI-020` | `source/gamemodes/TeamDeathMatch.cpp` |
| `PLAY-05` | Sudden-death rising water | [Specification](play-sudden-death.md) | [Wireframe](wireframes/play-sudden-death.md) | `ENV-002`–`ENV-007`, `ENV-009`–`ENV-013`, `UI-001`–`UI-020` | `source/Round.cpp:146-200` |
| `OVER-01` | Score-tab overlay | [Specification](overlay-score-tab.md) | [Wireframe](wireframes/overlay-score-tab.md) | `SCO-018`, `MOD-TM-010`–`MOD-TM-011`, `UI-011` | `source/Game.cpp:62-83` |
| `OVER-02` | Round-over summary | [Specification](overlay-round-over.md) | [Wireframe](wireframes/overlay-round-over.md) | `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012` | `source/WorldRenderer.cpp:120-169,472-505,565-570` |
| `OVER-03` | Game-over summary | [Specification](overlay-game-over.md) | [Wireframe](wireframes/overlay-game-over.md) | `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014` | `source/Game.cpp:51-79,158-164` |
| `CONS-01` | Console over menu | [Specification](console-menu.md) | [Wireframe](wireframes/console-menu.md) | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |
| `CONS-02` | Console over gameplay | [Specification](console-gameplay.md) | [Wireframe](wireframes/console-gameplay.md) | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |

## Coverage rules

- Each listed screen must have one linked wireframe.
- Each wireframe must have exactly one representative implementation screenshot entry.
- Minor loading, empty, disabled, focus, and error variants must stay in the applicable screen specification.
- A new full-screen flow state or a material layout change must receive a stable screen ID.
- An overlay that materially changes the primary task must receive a stable screen ID.
- Screenshot status must remain `Pending` until the requested implementation state is reached and captured.

`PLAY-02`, `PLAY-03`, and `PLAY-04` are retired identifiers.
They must not be reused for another screen.
`PLAY-01` is authoritative for each mode and each supported player count.

Unresolved functional and visual decisions are consolidated in [GitHub issue #7](https://github.com/mkapusnik-apps/duel6r/issues/7). The current implementation remains authoritative until those decisions are made and implemented.
