# Implementation screenshot manifest

## Status and provenance

This manifest defines exactly one representative implementation screenshot for each wireframe.
All entries are pending because no implementation screenshots were supplied for this baseline.
The required source is branch `feature-documentation-audit-fixes` at SHA `8f98d3679c4c9091e8973a1cb7a3278f04deb946`.

Each capture record must include:

- Branch.
- Source SHA.
- Build type and renderer.
- Operating system and display environment.
- Workflow.
- Representative state and setup data.
- Client viewport in pixels.
- Artifact path.
- Capture date.

The developer must not substitute an unreachable state.
The developer must report an evidence gap when the requested state cannot be reached.

## Screenshot matrix

| ID | Screen ID | Wireframe | Workflow | Representative state and setup data | Viewport | Expected visible behavior | Destination | Status |
|---|---|---|---|---|---:|---|---|---|
| `SS-001` | `MENU-01` | [Main menu](../screens/wireframes/menu-main.md) | Launch the application to the menu. | At least four saved people, two selected players, non-empty persistent score and Elo tables, Deathmatch, Assistance on, Quick Liquid on, Rounds `0`. | 1280x900 debug client | Centered 850x700 control canvas, compact grey GUI, populated tables, setup controls, menu banner, version text. | `docs/screenshots/MENU-01/default-1280x900.png` | Pending |
| `SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | Select Clear or press F3. | Confirmation text `Really delete? (Y/N)` over the populated menu. | 1280x900 debug client | Centered pink message strip with red text and black frame. | `docs/screenshots/MENU-02/confirmation-1280x900.png` | Pending |
| `SS-003` | `PLAY-01` | [Full-screen play](../screens/wireframes/play-fullscreen.md) | Start a two-player Deathmatch. | Active round after the start fade, ranking on, finite round limit, one event message, one visible player status group. | 1280x900 debug client | Arena fills client; event feed, live ranking, round counter, world objects, and local player indicators are visible. | `docs/screenshots/PLAY-01/live-1280x900.png` | Pending |
| `SS-004` | `PLAY-02` | [Two-player split](../screens/wireframes/play-split-2.md) | Start two-player Deathmatch and press F2. | Both players alive during active play. | 1280x900 debug client | Two centered half-size views form a vertical stack with red gutters. | `docs/screenshots/PLAY-02/live-1280x900.png` | Pending |
| `SS-005` | `PLAY-03` | [Three-player split](../screens/wireframes/play-split-3.md) | Start three-player Deathmatch and press F2. | All three players alive during active play. | 1280x900 debug client | Two upper views and one centered lower view use red gutters. | `docs/screenshots/PLAY-03/live-1280x900.png` | Pending |
| `SS-006` | `PLAY-04` | [Four-player split](../screens/wireframes/play-split-4.md) | Start four-player Deathmatch and press F2. | All four players alive during active play. | 1280x900 debug client | Four equal views use a two-by-two grid with red gutters. | `docs/screenshots/PLAY-04/live-1280x900.png` | Pending |
| `SS-007` | `MODE-01` | [Predator](../screens/wireframes/mode-predator.md) | Start Predator with at least three players. | Predator and at least two marines alive; ranking on; predator in a readable open arena area. | 1280x900 debug client | Predator body is nearly transparent while the weapon remains visible; standard arena and ranking remain visible. | `docs/screenshots/MODE-01/live-1280x900.png` | Pending |
| `SS-008` | `MODE-02` | [Team mode](../screens/wireframes/mode-team.md) | Start `Team deathmatch (2 teams, FF: off)` with four players. | Two living players per team; ranking on; customized players use team overrides. | 1280x900 debug client | Alpha and Bravo apparel colors are visible; ranking has named team rows and nested player rows. | `docs/screenshots/MODE-02/live-1280x900.png` | Pending |
| `SS-009` | `PLAY-05` | [Sudden death](../screens/wireframes/play-sudden-death.md) | Start a match with Quick Liquid on and wait for water to rise. | At least two players alive; water has visibly displaced the safe arena area. | 1280x900 debug client | Rising translucent water is a dominant hazard while gameplay overlays remain in place. | `docs/screenshots/PLAY-05/rising-water-1280x900.png` | Pending |
| `SS-010` | `OVER-01` | [Score tab](../screens/wireframes/overlay-score-tab.md) | Press Tab during a live round before a winner exists. | Free-for-all match with at least three players and non-zero K, A, D, K/D, and PTS values. | 1280x900 debug client | Centered translucent score panel overlays the live arena without the winner curtain. | `docs/screenshots/OVER-01/score-tab-1280x900.png` | Pending |
| `SS-011` | `OVER-02` | [Round over](../screens/wireframes/overlay-round-over.md) | Finish a non-final finite-limit round. | One winner; remaining game-over wait is active; at least three ranked players. | 1280x900 debug client | Dark red curtain fades over the arena and the centered score panel is visible. | `docs/screenshots/OVER-02/round-over-1280x900.png` | Pending |
| `SS-012` | `OVER-03` | [Game over](../screens/wireframes/overlay-game-over.md) | Finish the final round of a finite-limit match. | Final-round winner; final score values are visible; wait has elapsed enough that Escape is accepted. | 1280x900 debug client | Final centered score panel and dark red curtain remain visible over the arena. | `docs/screenshots/OVER-03/game-over-1280x900.png` | Pending |
| `SS-013` | `CONS-01` | [Console over menu](../screens/wireframes/console-menu.md) | Open the console with backquote from the main menu. | Menu is populated; console has recent startup output; input line is empty. | 1280x900 debug client | Yellow console spans the bottom width over the menu with history, red separator, prompt, and blinking cursor. | `docs/screenshots/CONS-01/open-1280x900.png` | Pending |
| `SS-014` | `CONS-02` | [Console over play](../screens/wireframes/console-gameplay.md) | Open the console with backquote during full-screen gameplay. | Active two-player Deathmatch; arena and ranking visible behind console; recent game output in history. | 1280x900 debug client | Yellow console spans the bottom width over the live arena and captures text input. | `docs/screenshots/CONS-02/open-1280x900.png` | Pending |

## Coverage summary

- Required wireframes: 14.
- Required representative screenshots: 14.
- Supplied screenshots: 0.
- Pending screenshots: 14.
- Covered screen specifications after capture: 14.
- Current coverage status: blocked by missing implementation evidence.
