# Implementation screenshot manifest

## Assessment status

This manifest requires exactly one representative implementation screenshot for each wireframe.
The shared arena view is a visual-impact change.
UX assessed the eight fresh artifacts against PR #21 at head `453eb2376b6477dd61a4317430a00da57f6f6a53` on 2026-08-23.
Three fresh artifacts conform.
Five fresh artifacts required replacement because the captured setup did not match the applicable wireframe and matrix entry.
Replacement artifacts for those five entries were captured on 2026-08-23 and are pending UX reassessment.
The three unaffected menu and menu-console screenshots remain valid.
The obsolete `PLAY-02`, `PLAY-03`, and `PLAY-04` artifacts are not evidence and are not in this manifest.

## Provenance requirements

Each supplied artifact must include the implementation branch, source SHA, environment, route or workflow, state, viewport, and destination path.
Fresh entries must use the branch and source SHA that contain the shared arena implementation.
The capture environment should use the Debug build, GL4 renderer, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
If the developer uses another environment, the developer must record the difference.

The unaffected packet has this provenance:

- Branch: `feature-documentation-audit-fixes`.
- Source SHA: `12cd6dca742b90293f552fefa3bfd3a8871aa7a2`.
- Screenshot artifact commit: `9cacfbf1eb9bcd1334522b2a4390605d42d1a076`.
- Environment: Debug, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.

The fresh shared-arena packet has this provenance:

- Branch: `feature/remove-split-screen`.
- Implementation source SHA: `be5c9f5315a60eb9b1052e914fab22254d41c5f0`.
- Screenshot artifact commit: `f3747a9bf2b082f4a9b2e43e474f006ec88c516c`.
- Environment: Debug, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Worktree state: implementation files were clean; earlier uncommitted screenshot artifacts were present while the remaining matrix entries were captured.

The replacement shared-arena artifacts have this provenance:

- Branch: `feature/remove-split-screen`.
- Implementation source SHA: `be5c9f5315a60eb9b1052e914fab22254d41c5f0`.
- Screenshot artifact commit: pending in the current worktree.
- Environment: Debug, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Worktree state: the application binary was built from the implementation source SHA; the UX-owned manifest, tester-owned regression harness, and changelog integration changes were present while the replacement artifacts were captured.
- Runtime setup: normal menu controls and public Lua player controls drove the documented match states; no application source or saved score data was altered to fabricate outcomes.

## Screenshot matrix

| ID | Screen ID | Wireframe | Route or workflow | Representative state and setup data | Viewport | Expected visible behavior | Destination path | Provenance and status |
|---|---|---|---|---|---:|---|---|---|
| <a id="ss-001"></a>`SS-001` | `MENU-01` | [Main menu](../screens/wireframes/menu-main.md) | Launch the local application to the setup menu. | Use four saved people, two selected players, Deathmatch, persistent score data, Assistance on, Quick Liquid on, and Rounds `0`. | 1280x900 | The fixed grey canvas, banner, version, populated lists, settings, and action row must match the menu wireframe. | `docs/screenshots/MENU-01/default-1280x900.png` | Existing packet provenance above. `Conforms`. |
| <a id="ss-002"></a>`SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | Select Clear or press F3 from the populated menu. | Show the delete confirmation over the unchanged menu. | 1280x900 | The centered strip must show `Really delete? (Y/N)` with the documented surface, text, and frame. | `docs/screenshots/MENU-02/confirmation-1280x900.png` | Existing packet provenance above. `Conforms`. |
| <a id="ss-003"></a>`SS-003` | `PLAY-01` | [Shared arena play](../screens/wireframes/play-fullscreen.md) | Start a local Deathmatch and press F2 once during live play. | Use 15 living players, ranking on, a finite round limit, and a state after the start fade. | 1280x900 | One undivided arena must show the whole level and all 15 players. F2 must not change the view. Live ranking, round progress, event messages, and player status must remain available. | `docs/screenshots/PLAY-01/shared-15-player-1280x900.png` | Replacement packet provenance above. Captured from an actual 15-player Deathmatch with ranking enabled and three rounds after pressing F2 once. `Pending UX reassessment`. |
| <a id="ss-007"></a>`SS-007` | `MODE-01` | [Predator](../screens/wireframes/mode-predator.md) | Start Predator from the menu. | Use three living players, one predator, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show the faint predator body, visible predator weapon, opaque marines, live ranking, round progress, events, and status cues. | `docs/screenshots/MODE-01/shared-live-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |
| <a id="ss-008"></a>`SS-008` | `MODE-02` | [Team mode](../screens/wireframes/mode-team.md) | Start `Team deathmatch (2 teams, FF: off)` from the menu. | Use two living players per team, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show all four players. Team apparel and named grouped ranking must remain visible with events, status cues, and round progress. | `docs/screenshots/MODE-02/shared-live-1280x900.png` | Replacement packet provenance above. Captured from an actual four-player `Team deathmatch (2 teams, FF: off)` with two players per team, grouped live ranking, and three rounds. `Pending UX reassessment`. |
| <a id="ss-009"></a>`SS-009` | `PLAY-05` | [Sudden death](../screens/wireframes/play-sudden-death.md) | Start Deathmatch with Quick Liquid on and reach rising water. | Use two living players, ranking on, raised water in the safe arena, and one submerged player. | 1280x900 | One undivided arena must show both players, raised water, the air indicator, live ranking, status cues, events, and round progress. | `docs/screenshots/PLAY-05/shared-rising-water-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |
| <a id="ss-010"></a>`SS-010` | `OVER-01` | [Score tab](../screens/wireframes/overlay-score-tab.md) | Press Tab during a live Team deathmatch round. | Use four players in two teams with non-zero K, A, D, K/D, and PTS values. | 1280x900 | The centered score panel must show grouped team rows over one undivided live arena. The winner curtain must not appear. | `docs/screenshots/OVER-01/shared-score-tab-1280x900.png` | Replacement packet provenance above. Captured by pressing Tab during round 5 of an actual four-player Team deathmatch after normal runtime play produced grouped team rows and non-zero K, A, D, K/D, and PTS values; no winner curtain is present. `Pending UX reassessment`. |
| <a id="ss-011"></a>`SS-011` | `OVER-02` | [Round over](../screens/wireframes/overlay-round-over.md) | Finish round 1 of a two-round Predator match. | Use one winning predator and three ranked players. | 1280x900 | The Predator outcome message, dark red curtain, round counter, and centered score panel must appear over one undivided arena. | `docs/screenshots/OVER-02/shared-round-over-1280x900.png` | Replacement packet provenance above. Captured after an actual predator won round 1 of a two-round, three-player Predator match; the runtime emitted `Predator won!` and opened the score panel and curtain. `Pending UX reassessment`. |
| <a id="ss-012"></a>`SS-012` | `OVER-03` | [Game over](../screens/wireframes/overlay-game-over.md) | Finish the only round of a one-round Team deathmatch. | Use two teams, four ranked players, and one winning team. | 1280x900 | The Team outcome, final score panel, dark red curtain, and final round progress must appear over one undivided arena. The state must not add an exit label. | `docs/screenshots/OVER-03/shared-game-over-1280x900.png` | Replacement packet provenance above. Captured after Team Bravo won the only round of an actual four-player Team deathmatch; the final grouped score panel, dark red curtain, and `1 | 1` round progress are visible without an exit label. `Pending UX reassessment`. |
| <a id="ss-013"></a>`SS-013` | `CONS-01` | [Console over menu](../screens/wireframes/console-menu.md) | Open the console from the populated menu. | Use recent startup output and an empty input line. | 1280x900 | The opaque yellow console must show 15 history rows, the red separator, the prompt, and the black edge. | `docs/screenshots/CONS-01/open-1280x900.png` | Existing packet provenance above. `Conforms`. |
| <a id="ss-014"></a>`SS-014` | `CONS-02` | [Console over play](../screens/wireframes/console-gameplay.md) | Open the console during active Deathmatch and enter or inspect any command that previously affected the gameplay view. | Use four living players, ranking on, a finite round limit, and recent game output. | 1280x900 | The console must overlay one undivided arena. The command must not create player-specific views. The simulation, ranking, round progress, events, and status state must remain present behind the console where visible. | `docs/screenshots/CONS-02/shared-open-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |

## Coverage

- Required wireframes: 11.
- Required representative screenshots: 11.
- Conforming unaffected screenshots: 3.
- Conforming fresh screenshots: 3.
- Replacement screenshots pending UX reassessment: 5.
- Screenshots per wireframe: exactly one.
- Retired screenshot matrix entries: `SS-004`, `SS-005`, and `SS-006`.
- Coverage status: 6 of 11 representative screenshots are confirmed conforming by the latest UX assessment; five replacement artifacts await reassessment.
- Visual coverage remains blocked pending UX reassessment of `SS-003`, `SS-008`, `SS-010`, `SS-011`, and `SS-012`.

The matrix uses one representative state for each wireframe.
The screen specifications and wireframes document other player counts, modes, interaction states, and accessibility limits without multiplying screenshot entries.
