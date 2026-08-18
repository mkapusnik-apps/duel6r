# Implementation screenshot manifest

## Assessment status

This manifest records exactly one representative implementation screenshot for each wireframe.
The UX assessment reviewed all 14 supplied PNG files on 2026-08-18.
All 14 files meet the applicable visual requirements after the documented baseline corrections in this change.
`PLAY-03` conforms after product corrected `UI-004` to describe the implemented Player 3 upper-center layout.

## Packet provenance

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

## Screenshot matrix

| ID | Screen ID | Wireframe | Scenario and setup | Viewport | Artifact | UX assessment | Status |
|---|---|---|---|---:|---|---|---|
| <a id="ss-001"></a>`SS-001` | `MENU-01` | [Main menu](../screens/wireframes/menu-main.md) | Launch to a populated Deathmatch menu with four saved people, two selected players, persistent score data, Assistance on, Quick Liquid on, and Rounds `0`. | 1280x900 | [`default-1280x900.png`](MENU-01/default-1280x900.png) | The fixed grey canvas is centered. The banner and version are in the upper area. The populated setup lists, tables, settings, and bottom action row agree with the implementation. | Conforms |
| <a id="ss-002"></a>`SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | Select Clear or press F3 from the populated menu. | 1280x900 | [`confirmation-1280x900.png`](MENU-02/confirmation-1280x900.png) | The centered strip shows `Really delete? (Y/N)` with a pink surface, red text, and black frame over the unchanged menu. | Conforms |
| <a id="ss-003"></a>`SS-003` | `PLAY-01` | [Full-screen play](../screens/wireframes/play-fullscreen.md) | Run active two-player Deathmatch after the start fade with ranking on and a finite round limit. | 1280x900 | [`live-1280x900.png`](PLAY-01/live-1280x900.png) | The arena fills the client. The event message, live ranking, round counter, world objects, and player status cues are visible. | Conforms |
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

### Resolved finding

- `PLAY-03` shows Player 3 in the centered upper camera and Players 1 and 2 in the lower cameras.
- Product corrected `UI-004` to make that implemented layout authoritative.
- The existing artifact now conforms without implementation work or recapture.

### Non-blocking findings

- `MENU-01` showed that the banner is above the setup controls and that the primary action row is at the bottom.
- `PLAY-02`, `PLAY-03`, and `PLAY-04` showed the renderer camera order, black unused regions, and 4 px red camera boundaries.
- `CONS-01` and `CONS-02` showed that the console is at the top of the visible client and uses 15 history rows.
- UX documentation correction was required for these implemented layouts.
- Developer implementation work and replacement capture are not required for these corrected documentation differences.
- The packet does not supply a separate capture timestamp.
- A future packet should include the capture timestamp directly instead of relying on the artifact commit date for chronology.

## Coverage and freshness

- Required wireframes: 14.
- Required representative screenshots: 14.
- Supplied screenshots: 14.
- Assessed screenshots: 14.
- Conforming screenshots: 14.
- Nonconforming screenshots: 0.
- Pending screenshots: 0.
- Extra screenshots: 0.
- Screenshots per wireframe: exactly 1.
- Covered screen specifications: 14.
- Manifest assessment date: 2026-08-18.
- Manifest freshness basis: capture source SHA `12cd6dca742b90293f552fefa3bfd3a8871aa7a2` and screenshot artifact commit `9cacfbf1eb9bcd1334522b2a4390605d42d1a076`.
- Current coverage status: complete, fresh, and conforming.
