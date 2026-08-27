# Screen inventory

This inventory is authoritative for product screens and materially distinct full-screen visual states.
The target baseline uses the shared arena view requirements in `docs/features.md`.
The product has no implemented URL routes.
Each route value below therefore names a reproducible local workflow.
`NET-01`–`NET-09` and the `MENU-01` Network action are approved target specifications for issue #28. They are not implemented screens or evidence of playable network support; downstream issue #38 owns implementation.

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

| Screen ID | Screen or state | Specification | Wireframe | Functional requirements | Primary source |
|---|---|---|---|---|---|
| `MENU-01` | Main menu and session setup; planned Network entry | [Specification](menu-main.md) | [Wireframe](wireframes/menu-main.md) | Existing local requirements plus target `NET-AC-002`, `NET-AC-009`, `NET-AC-015` | Current: `source/Menu.cpp`; target: `docs/network-play-first-release.md` |
| `MENU-02` | Menu blocking message over planned Network footer | [Specification](menu-message.md) | [Wireframe](wireframes/menu-message.md) | Existing local requirements plus target `NET-AC-015`, `NET-AC-017` | Current: `source/Menu.cpp`; target footer: `docs/network-play-first-release.md` |
| `PLAY-01` | Live shared arena gameplay for 2–15 players | [Specification](play-fullscreen.md) | [Wireframe](wireframes/play-fullscreen.md) | `LIF-001`–`LIF-022`, `INP-012`–`INP-017`, `PLY-001`–`PLY-010`, `ENV-001`–`ENV-013`, `CMB-001`–`CMB-020`, `BON-001`–`BON-020`, `SCO-001`–`SCO-018`, `UI-001`–`UI-020` | Target: `docs/features.md`; context: `source/WorldRenderer.cpp` |
| `MODE-01` | Predator live gameplay | [Specification](mode-predator.md) | [Wireframe](wireframes/mode-predator.md) | `MOD-PR-001`–`MOD-PR-008`, `UI-001`–`UI-020` | `source/gamemodes/Predator.cpp` |
| `MODE-02` | Team live gameplay and ranking | [Specification](mode-team.md) | [Wireframe](wireframes/mode-team.md) | `SET-020`–`SET-021`, `SCO-005`–`SCO-006`, `SCO-013`–`SCO-017`, `MOD-TM-001`–`MOD-TM-011`, `UI-001`–`UI-020` | `source/gamemodes/TeamDeathMatch.cpp` |
| `PLAY-05` | Sudden-death rising water | [Specification](play-sudden-death.md) | [Wireframe](wireframes/play-sudden-death.md) | `ENV-002`–`ENV-007`, `ENV-009`–`ENV-013`, `UI-001`–`UI-020` | `source/Round.cpp:146-200` |
| `OVER-01` | Score-tab overlay | [Specification](overlay-score-tab.md) | [Wireframe](wireframes/overlay-score-tab.md) | `SCO-018`, `MOD-TM-010`–`MOD-TM-011`, `UI-011` | `source/Game.cpp:62-83` |
| `OVER-02` | Round-over summary | [Specification](overlay-round-over.md) | [Wireframe](wireframes/overlay-round-over.md) | `LIF-011`–`LIF-017`, `MOD-DM-001`–`MOD-DM-003`, `MOD-PR-005`–`MOD-PR-008`, `MOD-TM-005`–`MOD-TM-011`, `UI-012` | `source/WorldRenderer.cpp:120-169,472-505,565-570` |
| `OVER-03` | Game-over summary | [Specification](overlay-game-over.md) | [Wireframe](wireframes/overlay-game-over.md) | `LIF-018`, `SCO-022`–`SCO-023`, `UI-013`–`UI-014` | `source/Game.cpp:51-79,158-164` |
| `CONS-01` | Console over menu with planned Network footer | [Specification](console-menu.md) | [Wireframe](wireframes/console-menu.md) | Existing console requirements plus target `NET-AC-015`, `NET-AC-017` | Current: `source/console/ConsoleRenderer.cpp`; target footer: `docs/network-play-first-release.md` |
| `CONS-02` | Console over gameplay | [Specification](console-gameplay.md) | [Wireframe](wireframes/console-gameplay.md) | `CFG-001`–`CFG-002`, `CFG-008`–`CFG-020` | `source/console/ConsoleRenderer.cpp` |
| `NET-01` | Target network entry | [Specification](network-entry.md) | [Wireframe](wireframes/network-entry.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-015`, `NET-AC-017`, `NET-AC-019` | Target: `docs/network-play-first-release.md` |
| `NET-02` | Target host setup | [Specification](network-host-setup.md) | [Wireframe](wireframes/network-host-setup.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-009`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019` | Target: `docs/network-play-first-release.md` |
| `NET-03` | Target join setup and connecting | [Specification](network-join.md) | [Wireframe](wireframes/network-join.md) | `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019` | Target: `docs/network-play-first-release.md` |
| `NET-04` | Target lobby and readiness | [Specification](network-lobby.md) | [Wireframe](wireframes/network-lobby.md) | `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018` | Target: `docs/network-play-first-release.md` |
| `NET-05` | Target network match shared arena | [Specification](network-match.md) | [Wireframe](wireframes/network-match.md) | `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018` | Target: `docs/network-play-first-release.md` |
| `NET-06` | Target final session summary | [Specification](network-summary.md) | [Wireframe](wireframes/network-summary.md) | `NET-AC-010`, `NET-AC-011`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018` | Target: `docs/network-play-first-release.md` |
| `NET-07` | Target guest reconnect | [Specification](network-reconnect.md) | [Wireframe](wireframes/network-reconnect.md) | `NET-AC-006`, `NET-AC-009`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017` | Target: `docs/network-play-first-release.md` |
| `NET-08` | Target connection or session failure | [Specification](network-failure.md) | [Wireframe](wireframes/network-failure.md) | `NET-AC-002`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-011`, `NET-AC-013`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019` | Target: `docs/network-play-first-release.md` |
| `NET-09` | Host-ended session outcome | [Specification](network-host-ended.md) | [Wireframe](wireframes/network-host-ended.md) | `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018` | Target: `docs/network-play-first-release.md` |

## Target network navigation

```text
MENU-01 → NET-01 → Host → NET-02 → NET-04
                   Join → NET-03 → NET-04
NET-02 startup Cancel → editable NET-02 with setup retained and no listener
NET-03 connection Cancel → editable NET-03 with setup retained
NET-04 → NET-05 → NET-06 → NET-04
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
```

Back from `NET-01` returns to `MENU-01`. `Play (F1)` remains local-only and does not enter this graph. Match admission closes at `NET-04` → `NET-05`; the target has no join-in-progress or host-migration path.

## Coverage rules

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
