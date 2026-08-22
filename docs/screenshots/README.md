# Implementation screenshot manifest

## Assessment status

This manifest records exactly one representative implementation screenshot for each wireframe.
The final UX assessment reviewed the three-artifact issue #15 packet at PR #17 head `d9f4ca824be64661b2517404ceaa2d21a790ff73` on 2026-08-22.
The assessment confirms that `SS-001`, `SS-002`, and `SS-003` replace their invalidated baseline evidence and conform.
The other 11 representative files remain current and conforming.
All 14 wireframes now have current representative evidence.

## Baseline packet provenance

- Branch: `feature-documentation-audit-fixes`.
- Capture source SHA: `12cd6dca742b90293f552fefa3bfd3a8871aa7a2`.
- Screenshot artifact commit: `9cacfbf1eb9bcd1334522b2a4390605d42d1a076`.
- Artifact commit date: 2026-08-17.
- Capture date: The packet does not provide a separate capture timestamp.
- Operating system: Ubuntu 24.04 Docker.
- Build type: Debug.
- Renderer: GL4.
- Lua: Enabled.
- Graphics environment: Mesa software rendering.
- Display environment: Xvfb.
- Client viewport: 1280 by 900 px for every screenshot.
- Capture workflow: The developer reproduced each local workflow and stored the PNG at the matrix destination.

The capture source commit is the first commit in PR #8.
The screenshot artifact commit is the next commit in the same PR and adds only the 14 screenshot files.
This manifest assessment is recorded by a later documentation commit.
The packet therefore has a coherent source-to-artifact chain.
The artifact commit date provides packet chronology, but it does not replace a separate capture timestamp.

## Issue #15 replacement packet provenance

- Pull request: `#17`.
- Branch: `feature/safe-empty-match-start`.
- Capture source SHA: `e193fe1`.
- Final assessed head and artifact commit: `d9f4ca824be64661b2517404ceaa2d21a790ff73`.
- Capture date: The packet does not provide a separate capture timestamp.
- Operating system: Ubuntu 24.04 Docker.
- Build type: Release.
- Renderer: GL4.
- Lua: Enabled.
- Graphics environment: Mesa software rendering.
- Display environment: Xvfb.
- Client viewport: 1280 by 900 px for all three screenshots.
- Capture workflow: The developer reproduced each matrix workflow from source SHA `e193fe1` and stored each unchanged PNG at the matrix destination.

The final head commits the three captured artifacts without changing their pixels.
The source-to-artifact chain is coherent for final visual assessment.

## Screenshot matrix

| ID | Screen ID | Wireframe | Scenario and setup | Viewport | Artifact | UX assessment | Status |
|---|---|---|---|---:|---|---|---|
| <a id="ss-001"></a>`SS-001` | `MENU-01` | [Main menu](../screens/wireframes/menu-main.md) | Use two selected players, no successfully loaded levels, and no enabled weapons. Select Play, dismiss the blocking message with a keyboard key, and capture the recovered menu. | 1280x900 | [`failed-start-recovered-1280x900.png`](MENU-01/failed-start-recovered-1280x900.png) | The complete centered menu remains visible after dismissal. Alpha and Beta remain in the roster. Deathmatch, checked Assistance, checked Quick Liquid, Rounds 0, and the visible person statistics remain unchanged. No warning, disabled Play style, resume prompt, or statistics-clear prompt remains visible. | Conforms |
| <a id="ss-002"></a>`SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | Use two selected players, no successfully loaded levels, and no enabled weapons. Select Play and capture before dismissal. | 1280x900 | [`start-blocked-no-levels-no-weapons-1280x900.png`](MENU-02/start-blocked-no-levels-no-weapons-1280x900.png) | The centered one-line strip shows `No usable levels loaded. No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.` with the specified pink surface, red text, and black frame over the unchanged menu. The report identifies both errors in separate sentences. No resume or statistics-clear prompt is visible. | Conforms |
| <a id="ss-003"></a>`SS-003` | `PLAY-01` | [Full-screen play](../screens/wireframes/play-fullscreen.md) | Use two selected players, at least one successfully loaded level, and at least one enabled weapon. Complete the existing valid-start prompts and capture active Deathmatch after the start fade. | 1280x900 | [`valid-start-live-1280x900.png`](PLAY-01/valid-start-live-1280x900.png) | The valid start enters the unchanged full-screen arena. The live ranking, level geometry, background, elevators, water, players, weapons, pickups, and player status cues are visible. The empty event area and absent round counter agree with an empty event queue and the captured unlimited-round setting. Invalid setup is evidenced by `SS-002` because it must not create a PLAY-01 frame. | Conforms |
| <a id="ss-004"></a>`SS-004` | `PLAY-02` | [Two-player split](../screens/wireframes/play-split-2.md) | Run active two-player Deathmatch and press F2 while both players are alive. | 1280x900 | [`live-1280x900.png`](PLAY-02/live-1280x900.png) | Two centered half-size cameras form a vertical stack. Black side regions and red camera boundaries agree with the renderer. | Conforms |
| <a id="ss-005"></a>`SS-005` | `PLAY-03` | [Three-player split](../screens/wireframes/play-split-3.md) | Run active three-player Deathmatch and press F2 while all players are alive. | 1280x900 | [`live-1280x900.png`](PLAY-03/live-1280x900.png) | Player 3 uses the centered upper camera while Players 1 and 2 use the lower cameras, matching the renderer and corrected `UI-004`. | Conforms |
| <a id="ss-006"></a>`SS-006` | `PLAY-04` | [Four-player split](../screens/wireframes/play-split-4.md) | Run active four-player Deathmatch and press F2 while all players are alive. | 1280x900 | [`live-1280x900.png`](PLAY-04/live-1280x900.png) | Four equal cameras use the implemented two-by-two grid and red boundaries. | Conforms |
| <a id="ss-007"></a>`SS-007` | `MODE-01` | [Predator](../screens/wireframes/mode-predator.md) | Run Predator with three living players and ranking on. | 1280x900 | [`live-1280x900.png`](MODE-01/live-1280x900.png) | The lower-left predator body is faint while its weapon and status label remain readable. The two marines remain opaque. | Conforms |
| <a id="ss-008"></a>`SS-008` | `MODE-02` | [Team mode](../screens/wireframes/mode-team.md) | Run `Team deathmatch (2 teams, FF: off)` with two living players per team and ranking on. | 1280x900 | [`live-1280x900.png`](MODE-02/live-1280x900.png) | Alpha and Bravo apparel overrides are visible. The ranking uses named team rows with nested player rows. | Conforms |
| <a id="ss-009"></a>`SS-009` | `PLAY-05` | [Sudden death](../screens/wireframes/play-sudden-death.md) | Run a two-player state with Quick Liquid on after water rises into the safe arena. | 1280x900 | [`rising-water-1280x900.png`](PLAY-05/rising-water-1280x900.png) | Raised water visibly displaces the safe area. One player is submerged and uses an air indicator. The ranking remains in place. | Conforms |
| <a id="ss-010"></a>`SS-010` | `OVER-01` | [Score tab](../screens/wireframes/overlay-score-tab.md) | Press Tab during a live three-player free-for-all round with non-zero score values. | 1280x900 | [`score-tab-1280x900.png`](OVER-01/score-tab-1280x900.png) | The centered translucent panel shows the score heading and all required columns over the live arena without a winner curtain. | Conforms |
| <a id="ss-011"></a>`SS-011` | `OVER-02` | [Round over](../screens/wireframes/overlay-round-over.md) | Finish round 1 of a two-round match with one winner and three ranked players. | 1280x900 | [`round-over-1280x900.png`](OVER-02/round-over-1280x900.png) | The dark red curtain, outcome messages, finite-round counter, and centered score panel show the non-final result state. | Conforms |
| <a id="ss-012"></a>`SS-012` | `OVER-03` | [Game over](../screens/wireframes/overlay-game-over.md) | Finish the only round of a one-round finite match with one winner and three ranked players. | 1280x900 | [`game-over-1280x900.png`](OVER-03/game-over-1280x900.png) | The final score panel and dark red curtain remain over the arena. The state has no invented Game Over or exit label. | Conforms |
| <a id="ss-013"></a>`SS-013` | `CONS-01` | [Console over menu](../screens/wireframes/console-menu.md) | Open the console from the populated menu with recent startup output and an empty input line. | 1280x900 | [`open-1280x900.png`](CONS-01/open-1280x900.png) | The opaque yellow console spans the top width and shows 15 history rows, the red separator, the input prompt, and the lower black edge. | Conforms |
| <a id="ss-014"></a>`SS-014` | `CONS-02` | [Console over play](../screens/wireframes/console-gameplay.md) | Open the console during active two-player Deathmatch with recent game output. | 1280x900 | [`open-1280x900.png`](CONS-02/open-1280x900.png) | The opaque yellow console spans the top width over the active arena and shows the required history, separator, prompt, and edge. | Conforms |

## Findings

### Resolved findings

- `PLAY-03` shows Player 3 in the centered upper camera and Players 1 and 2 in the lower cameras.
- Product corrected `UI-004` to make that implemented layout authoritative.
- The existing artifact now conforms without implementation work or recapture.
- `SS-001`, `SS-002`, and `SS-003` now provide current replacement evidence for the approved safe-start behavior.
- The three replacement artifacts conform to their screen specifications and wireframes.

### Non-blocking findings

- `MENU-01` showed that the banner is above the setup controls and that the primary action row is at the bottom.
- `PLAY-02`, `PLAY-03`, and `PLAY-04` showed the renderer camera order, black unused regions, and 4 px red camera boundaries.
- `CONS-01` and `CONS-02` showed that the console is at the top of the visible client and uses 15 history rows.
- UX documentation correction was required for these implemented layouts.
- Developer implementation work and replacement capture are not required for these corrected documentation differences.
- The packet does not supply a separate capture timestamp.
- A future packet should include the capture timestamp directly instead of relying on the artifact commit date for chronology.
- The red prerequisite text on the pink message surface has low contrast for normal-size text.
- The contrast limitation is part of the approved implemented color tokens and does not make this issue #15 packet nonconforming.
- The three screenshots provide representative visual evidence for the affected wireframes.
- Static screenshots do not independently prove keyboard event consumption, mouse blocking, selected-map validation, preservation of non-visible played-round data, or restart-only content reload behavior.

## Coverage and freshness

- Required wireframes: 14.
- Required representative screenshots: 14.
- Current representative screenshots supplied: 14.
- Current representative screenshots assessed: 14.
- Conforming screenshots: 14.
- Nonconforming screenshots: 0.
- Pending screenshots: 0.
- Stale baseline screenshots excluded from current representative coverage: 3.
- Extra current representative screenshots: 0.
- Screenshots per wireframe: exactly 1.
- Covered screen specifications: 14.
- Manifest assessment date: 2026-08-22.
- Baseline freshness basis for 11 unchanged representatives: capture source SHA `12cd6dca742b90293f552fefa3bfd3a8871aa7a2` and screenshot artifact commit `9cacfbf1eb9bcd1334522b2a4390605d42d1a076`.
- Issue #15 freshness basis for `SS-001`, `SS-002`, and `SS-003`: capture source SHA `e193fe1` and final artifact commit `d9f4ca824be64661b2517404ceaa2d21a790ff73`.
- Current coverage status: complete and conforming.
