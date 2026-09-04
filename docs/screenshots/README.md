# Implementation screenshot manifest

## Assessment status

This manifest requires exactly one representative screenshot entry for each wireframe. An entry is `Planned` when downstream implementation does not exist. An entry is `Pending` when implementation evidence is required but unavailable. Planned and pending paths are not implementation evidence.
PR #69 refreshed every implemented representative destination without changing a stable screen, wireframe, or screenshot ID.
The ten implemented entries are `SS-001`, `SS-003`, `SS-007`–`SS-012`, `SS-014`, and `SS-024`.
Nine implemented entries remain current and conform at PR #69 head `f4708d337bb82be55c553c64608bd75ccd64121f`.
`SS-012` is `Pending` because the approved `UI-GAME-001`–`UI-GAME-004` change invalidates the prior final Team summary artifact.
The eleven entries `SS-002`, `SS-013`, and `SS-015`–`SS-023` remain `Planned` because their network-related graphical states are not implemented.
The planned entries are non-blocking and must not use fabricated or substitute artifacts.
The 29 PNG files that are not matrix destinations are historical or superseded and are not current conformance evidence.
The matrix contains exactly one representative entry for each of its 21 wireframes.
No screenshot entry was added or removed by this refresh.

## Provenance requirements

Each supplied artifact must include the implementation branch, source SHA, environment, route or workflow, state, viewport, and destination path.
Evidence for menu background selection or persistence must also include the selected background filename, runtime asset manifest revision, and session identifier.
Fresh menu entries must use the branch and source SHA that contain the approved menu implementation.
The capture environment should use GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
Fresh menu captures must use a 1920 by 1080 px client through a release display or an equivalent recorded capture configuration.
If the developer uses another environment, the developer must record the difference.

The stale pre-baseline menu packet has this provenance:

- Branch: `feature-documentation-audit-fixes`.
- Source SHA: `12cd6dca742b90293f552fefa3bfd3a8871aa7a2`.
- Screenshot artifact commit: `9cacfbf1eb9bcd1334522b2a4390605d42d1a076`.
- Environment: Debug, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.

This packet is historical only and is not current evidence for `MENU-01`, `MENU-02`, or `CONS-01`.

The PR #22 retro-menu packet has this provenance:

- Pull request: `#22`.
- Branch: `feature/menu-redesign`.
- Implementation source SHA: `af73c8a8c2bf11eb658897281bf105e27525557d`.
- Screenshot artifact commit: `bb5a74cf5a2ce5d56d12b2074eb74650544babea`.
- Packet and manifest head: `7f59ec7fcb7f9a12435bb9f542a040e12e69ef1a`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1706 by 938 px.
- Worktree state: the implementation worktree was clean at the source SHA; capture used an ignored Docker-built runtime bundle and deterministic saved-person fixture.
- Runtime setup: normal startup loaded four saved people with two selected players, persistent Elo and score data, Deathmatch, Assistance on, Quick Liquid on, and Rounds `0`; F3 opened the documented confirmation, and backquote opened the console.
- Artifact paths: `MENU-01/default-1706x938.png`, `MENU-02/confirmation-1706x938.png`, and `CONS-01/open-1706x938.png` under this directory.

The player-count replacement captures have this provenance:

- Branch: `feature/menu-redesign`.
- Implementation source SHA: `3b44dc2cfc912e8ff40a068ea83d78e0fd61f819`.
- Screenshot artifact commit: `b62a97afc16b6449cc0c3657438ad898caf230f0`.
- Final packet and manifest head: `a78ea6b1047be9ea471dd29cbdf487e2756454b4`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1706 by 938 px.
- Worktree state: the implementation worktree was clean at the source SHA; capture used a Docker-built runtime bundle and the same deterministic saved-person fixture as the original packet.
- Runtime setup: normal startup loaded four saved people with two selected players, persistent Elo and score data, Deathmatch, Assistance on, Quick Liquid on, and Rounds `0`; F3 opened the documented confirmation.
- Recaptured artifact paths: `MENU-01/default-1706x938.png` and `MENU-02/confirmation-1706x938.png` under this directory. `CONS-01/open-1706x938.png` was not recaptured.

The PR #23 Burnable Trees packet has this provenance:

- Pull request: `#23`.
- Branch: `feature/burnable-trees`.
- Implementation source SHA: `9dbc135cb00480b33d19a3c02ab7a492b2587342`.
- Screenshot artifact commit and packet head: `17753e8b8603e779338c4c2811ec317efaa8e3d9`.
- Environment: Ubuntu 24.04, GL4 software rendering, and Xvfb.
- Viewport: 1706 by 938 px.
- Runtime setup: normal startup showed four saved people, two selected players, persistent score data, Deathmatch, Assistance on, Quick Liquid on, Burnable Trees on, and Rounds `0`; F3 opened the documented confirmation, and backquote opened the console.
- Artifact paths: `MENU-01/default-1706x938.png`, `MENU-02/confirmation-1706x938.png`, and `CONS-01/open-1706x938.png` under this directory.
- Status: stale after the 2026-08-26 menu presentation approval; retained only as historical evidence.

The scaled-background packet for the currently implemented three-action footer was captured with this shared setup:

- Pull request: `#25`.
- Branch: `feature/menu-presentation`.
- Implementation source SHA: `b7fea416527b418d8fbe7125e646e3f904c0c360`.
- UX assessment head: `b98dad049157b6ba08d87ee4d09f871dc3ae8623`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image, Mesa software rendering, and Xvfb at 1920 by 1080.
- Viewport: 1920 by 1080 px.
- Runtime setup: one application session with saved people Ada, Bruno, Cora, and Diego; Ada and Bruno selected; persistent Elo and score data; Deathmatch; Assistance on; Quick Liquid on; Burnable Trees on; and Rounds `0`.
- Session workflow: the first observable menu render showed the responsive menu over the black loading background; F3 input produced the confirmation while asynchronous preparation completed, changing 985723 pixels from the initial capture. After dismissing with `N` and waiting for the selected background diagnostic, capture `MENU-01`, press F3 for `MENU-02`, dismiss with `N`, then open the console with an empty input for `CONS-01`, without restarting.
- Worktree state: clean at the implementation source SHA before capture; the ignored Docker-built runtime bundle was present.
- Session identifier: `MENU-PRESENTATION-20260826-B`.
- Selected asset filename: `forest-foundry.png`, recorded by the visible menu startup diagnostic in `CONS-01`.
- Runtime asset manifest revision: `81e364c8477c1b1cdbe90563812a90592d12cfea`.
- Capture artifacts: `MENU-01/default-scaled-background-1920x1080.png`, `MENU-02/confirmation-scaled-background-1920x1080.png`, and `CONS-01/open-scaled-background-1920x1080.png` under this directory.
- Assessment status: UX confirmed conformance for the then-current implementation. The `MENU-01`, `MENU-02`, and `CONS-01` artifacts are historical after the target four-action footer approval and are not evidence for issue #28.

The PR #52 Rounds evidence was reported with this setup:

- Pull request: `#52`.
- Branch: `feature/rounds-input`.
- Pull request head: `2af47861ce5f372a42620eb7045e680143acc518`.
- Implementation source SHA: `5020a173ab8b0c8e42fa5f7f7a7b43e1711d2ed5`.
- Screenshot artifact commit: `2af47861ce5f372a42620eb7045e680143acc518`.
- Worktree state: clean at the implementation source SHA before the documentation-only screenshot commit.
- Environment: Ubuntu 24.04 Docker, Xvfb, and software OpenGL rendering.
- Viewport: 1920 by 1080 px.
- Session workflow: apply Rounds `3`, enter gameplay, and return to the menu in the same application session.
- Expected state: the unfocused Rounds field shows `3`.
- Artifact path: `MENU-01/rounds-3-retained-after-gameplay-1920x1080.png` under this directory.
- Assessment source: exact pushed Git object `f62090d946a62270d6f2682290b2e6e3418bbf3d` at the screenshot artifact commit.
- Assessment status: UX confirmed conformance for AC-040–AC-043 on 2026-08-29.
- Scope note: the artifact shows the correct current three-action footer. The planned Network footer is outside PR #52.

The PR #55 round-summary progress artifact has this provenance:

- Pull request: `#55`.
- Branch: `feature/round-summary-progress`.
- Implementation source SHA: `4a831c52a62b64b9a47cab0fab067c173839a3ea`.
- Screenshot artifact and manifest head: `cd91df7a99c78a800ebda7eeb9342582cff76749`.
- Worktree state: clean at the implementation source SHA before capture; the ignored Docker-built runtime bundle was present.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Runtime setup: the deterministic tester-owned `RoundSummaryProgressBehaviorTests.sh` fixture seeded Predator, MarineA, and MarineB with two completed rounds, selected five-round Predator, set starting ammo to zero, disabled Quick Liquid, retained one dry start platform over water, and used the supplied Lua behavior so the marines entered the water and the predator won.
- Session workflow: resume the saved match, complete round 3 of 5, and capture the winner summary after the persisted played-round count becomes `3`.
- Historical expected and visible state: three ranked players, one winning predator, unobstructed centered `---SCORE---`, panel-top-right `Rounds: 3|5`, and finite-match progress above the arena.
- Artifact path: `OVER-02/resumed-round-3-of-5-1280x900.png` under this directory.
- Assessment status: UX confirmed conformance to the superseded PR #55 requirements at exact screenshot and manifest head `cd91df7a99c78a800ebda7eeb9342582cff76749` on 2026-08-31. Revised `UI-RND-001`–`UI-RND-009` and `AC-044` make this artifact stale historical evidence.

The revised PR #55 round-summary progress artifact has this provenance:

- Pull request: `#55`.
- Branch: `feature/round-summary-progress`.
- Implementation source SHA: `f64ac75a0f6842a4ff70b2221705b1cddcf91dad`.
- Screenshot artifact commit: `23293210831cac7e4b7e5fd30abd2281edaaa764`.
- Pull request and UX assessment head: `0e77f8849f2b707b556f5e056c1e338391cbd8f1`.
- Worktree state: clean at the implementation source SHA before capture; the ignored Docker-built runtime bundle was present.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Runtime setup: the deterministic tester-owned `RoundSummaryProgressBehaviorTests.sh` fixture seeded Predator, MarineA, and MarineB with two completed rounds, selected five-round Predator, set starting ammo to zero, disabled Quick Liquid, retained one dry start platform over water, and used the supplied Lua behavior so the marines entered the water and the predator won.
- Session workflow: resume the saved match, complete round 3 of 5, and capture the winner summary after the persisted played-round count becomes `3`.
- Historical expected and visible state: three ranked players, one winning predator, centered `Rounds: 3|5` in a dedicated row above the centered blue `---SCORE---` strip, aligned score table, and no top-center arena progress.
- Artifact path: `OVER-02/resumed-round-3-of-5-1280x900.png` under this directory.
- Assessment status: UX confirmed visual conformance for the superseded centered-label requirements at exact pull request head `0e77f8849f2b707b556f5e056c1e338391cbd8f1` on 2026-08-31. `UI-RND-010` and corrected `AC-044` make this artifact stale historical evidence. Temporal restoration of top-center progress in the next active round remains tester evidence and does not require another screenshot.

The corrected PR #55 round-summary progress artifact has this provenance:

- Pull request: `#55`.
- Branch: `feature/round-summary-progress`.
- Implementation source SHA: `ccf016bb7e8583f1bab37321f3d5bb0c2401a0e5`.
- Screenshot artifact commit: `b69f9d7c792a7a7c51b758fed21e2b0c9b40e4db`.
- Pull request and UX assessment head: `4e5c9e6945b33a4d5911fdb7ea944d544f31a9f8`.
- Worktree state: clean at the implementation source SHA before capture; ignored local build output was present outside the isolated capture container.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Runtime setup: the deterministic tester-owned `RoundSummaryProgressBehaviorTests.sh` fixture seeded Predator, MarineA, and MarineB with two completed rounds, selected five-round Predator, set starting ammo to zero, disabled Quick Liquid, retained one dry start platform over water, and used the supplied Lua behavior so the marines entered the water and the predator won.
- Session workflow: resume the saved match, complete round 3 of 5, and capture the winner summary after the persisted played-round count becomes `3`.
- Visible state: three ranked players, one winning predator, right-aligned `Rounds: 3|5` in a dedicated row above the centered blue `---SCORE---` strip, aligned score table, and no top-center arena progress.
- Artifact path: `OVER-02/resumed-round-3-of-5-1280x900.png` under this directory.
- Assessment status: UX confirmed visual conformance for corrected `UI-RND-010` and `AC-044` at exact pull request head `4e5c9e6945b33a4d5911fdb7ea944d544f31a9f8` on 2026-08-31. Temporal restoration of top-center progress in the next active round remains tester evidence and does not require another screenshot.

The historical PR #57 Team score-overview packet has this provenance:

This packet is superseded by the PR #69 replacement packet. Its `SS-010` and `SS-011` hashes and conformance assessment apply only to the historical PR #57 artifacts and are not current evidence.

- Pull request: `#57`.
- Branch: `feature/scoreboard-visual-improvement`.
- Rendering source SHA: `98b8f80aca8d15a66011a8f62cdf4862898e9df0`.
- Final screenshot packet and documentation head: `05a678bf36816979ae95bcc5e4b809afb23fb28d`.
- Historical freshness note: the final PR #57 documentation commit did not change the renderer from the rendering source SHA.
- Worktree state: tracked files were clean at the rendering source SHA before capture; ignored Docker build output and the local capture harness were present.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image `duel6r-build:local`, Mesa software rendering, Xvfb at 1280 by 900 by 24, and dummy SDL audio.
- Shared runtime setup: eight saved people `P01` through `P08` with non-zero persistent K, A, D, K/D, and PTS inputs; all eight selected; four-team Team deathmatch; two nested players in each of Alpha, Bravo, Charlie, and Delta; Quick Liquid off; configured five-round match resumed with two completed rounds; deterministic isolated-platform level and profile scripts that leave Alpha alive while the other teams enter the water.
- `SS-010` workflow: hold Tab during the active resumed round before a winner exists; retain the live arena progress and capture four ranked Team groups without a winner curtain.
- `SS-010` artifact: `OVER-01/shared-score-tab-1280x900.png`.
- `SS-010` SHA-256: `22f0adee73f69e198d20903d7feb716c7608c2c9fa162513fed933eb29a38b04`.
- `SS-011` workflow: release Tab, complete the resumed round as the non-final third round of five, and capture the interim Team summary after the persisted completed-round count becomes `3`.
- `SS-011` artifact: `OVER-02/team-interim-round-3-of-5-1280x900.png`.
- `SS-011` SHA-256: `1e09723595aa46cd93887b63930bb19710d50e29099ec9e647fd8e40adcbc640`.
- Automated capture assertions: both artifacts contain four distinct Team group rows and three separator rules with the documented table-width coverage and centered 2 px geometry.
- Assessment source: the two raw artifacts at final screenshot packet and documentation head `05a678bf36816979ae95bcc5e4b809afb23fb28d`.
- Visual assessment: both artifacts conform to `docs/design.md`, the applicable screen specifications, and the `OVER-01` and `OVER-02` wireframes. Each artifact shows three full-table-width 2 px white rules at 70% opacity, centered in separate 8 px bands with 3 px clear space above and below. Each artifact keeps every team row adjacent to its two player rows and omits a trailing separator. Score content remains readable and aligned. `SS-010` keeps active arena progress and has no winner curtain. `SS-011` keeps the dedicated right-aligned `Rounds: 3|5` row, hides arena progress, and keeps the winner curtain.
- Historical evidence status: both artifacts conformed and had complete required provenance at packet head `05a678bf36816979ae95bcc5e4b809afb23fb28d`.
- Historical visual gate: `Satisfied` at the PR #57 packet head. This status does not apply to the current matrix artifacts.

The PR #54 consolidated Teams evidence was assessed with this setup:

- Pull request: `#54`.
- Branch: `docs/consolidate-team-mode`.
- Implementation source SHA: `7de2b9b79523ad5f17e87c1f362819f6bd11ba43`.
- Screenshot artifact commit and head: `4117bb6634b78f914516ec949e26df5b1586f808`.
- Environment: Ubuntu 24.04 Docker image `duel6r-build:local`, Release, GL4, Lua enabled, Mesa software rendering, Xvfb at 1920 by 1080 by 24, and dummy SDL audio.
- Viewport: 1920 by 1080 px.
- Session workflow: one uninterrupted application process created and selected eight persons; selected Teams; set `Num. of Team` to `4`; set Friendly Fire on; captured Deathmatch after switching away; returned to Teams; applied Rounds `3`; entered gameplay; and captured Teams after returning to the menu.
- Session label: deterministic post-capture label `menu01-4117bb6-c829061a-7728d632`, anchored to screenshot commit `4117bb6634b78f914516ec949e26df5b1586f808` and both reported artifact hashes.
- Selected background: `forest-foundry.png`, verified by matching the screenshot artifact to the runtime resource; the ephemeral capture log did not retain the filename.
- Runtime asset Git tree: `399c8a8bdddd86e526e8811ae2da2461d9229866`.
- `SS-001` artifact: `MENU-01/non-team-settings-hidden-1920x1080.png`.
- `SS-001` reported SHA-256: `c829061ae118a597bb5fe144cd8aa6025883db4291a2f988091ab39d0e97a94c`.
- `SS-024` artifact: `MENU-01/teams-4-ff-on-retained-after-gameplay-1920x1080.png`.
- `SS-024` reported SHA-256: `7728d632245866857638a41766b680070ead5c603f13b0a405d0ae05a305c92e`.
- Assessment source: the two raw artifacts at screenshot artifact commit `4117bb6634b78f914516ec949e26df5b1586f808`.
- Visual assessment: both artifacts conform to their wireframes, the native menu design system, and AC-044–AC-051.
- Evidence status: both artifacts conform and have complete required provenance.
- Stitch qualification: the two stale `MENU-01` Stitch explorations are supplementary and do not block PR #54 visual acceptance because the authoritative local design documentation, wireframes, and implementation evidence conform.
- Current status: historical after the consolidated-person-list specification because both artifacts show two left-side lists.

The PR #58 consolidated-person-list packet has this provenance:

- Pull request: `#58`.
- Requirements baseline source SHA: `d0e78496f6adfc9cefc08a1b99a28d140ea7248f`.
- Implementation branch: `feature/consolidate-person-list`.
- Rendering source SHA: `0f198780ea685bf69aa1d78245d4a562d0a5a836`.
- Evidence commit and pull request head: `0e503e31b73a6c7cde002d85eb4458ae57cde3b3`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image `duel6r-build:local`, Mesa software rendering, Xvfb at 1920 by 1080 by 24, and dummy SDL audio.
- Worktree state: tracked implementation files were clean at the rendering source. Prior untracked destination artifacts and ignored Docker capture files were present. The capture replaced the destination artifacts.
- Session identifier: `menu01-0f19878-e9441ea9-29744655`.
- Selected background: `forest-foundry.png`.
- Runtime asset Git tree: `3ae377dc261e9a4e407dc75dd35dfd7911fb8e79`.
- Viewport: 1920 by 1080 px.
- Saved-person fixture: Alice and Bruno have Elo games and distinct Elo values; Cora, Diego, Erin, Farah, Gus, and Hana have zero Elo games in that person-record order.
- Roster fixture: all eight saved persons are selected players.
- Selection fixture: Cora is selected in the Persons list.
- Expected list order: Alice and Bruno appear first in descending Elo order; Cora through Hana follow in person-record order.
- Expected row content: Alice and Bruno show Rank, Name, Elo, and signed Trend; Cora through Hana show Name and empty Rank, Elo, and Trend cells.
- Session workflow: one application session used eight saved and selected persons. `SS-001` shows Deathmatch with Cora selected. `SS-024` shows Teams, four teams, Friendly Fire on, and Rounds `3` after gameplay return, with Cora selected.
- `SS-001` artifact: `MENU-01/non-team-consolidated-persons-1920x1080.png`.
- `SS-001` SHA-256: `e9441ea9addd2c25b5541f34d5ee285f12646e600ed3f53f5164c13782a1b0c9`.
- `SS-024` artifact: `MENU-01/teams-consolidated-persons-retained-1920x1080.png`.
- `SS-024` SHA-256: `29744655314db3de009ddd247d01fd0972f6a7e4e2185a530a976a883310827f`.
- Visual assessment: both artifacts conform to `docs/design.md`, the `MENU-01` specification, and the applicable wireframes. The screen keeps one large `PERSONS` list separate from Players and Game Settings. The headings, row alignment, clipping bounds, ranked and unranked treatment, Cora selection, roster retention, conditional Team settings, scale, keyline, background, and retro controls conform.
- Accessibility assessment: headings and numeric signs provide non-color meaning in Persons. The selected row uses a full-width blue fill with white text. Team roster identity still depends on color and position, as the canonical specification records.
- Evidence status: both artifacts conform and have complete required provenance.
- Current status: historical after the approved 50:50 Persons and Players split.

The PR #58 50:50 replacement packet has this provenance:

- Pull request: `#58`.
- Implementation branch: `feature/consolidate-person-list`.
- Rendering source SHA: `89904c876148e3e8bc2aba603a894db725e952db`.
- Evidence commit and exact pull request head: `cb9cc3ee3081b399c2d042ee62e1f7b0cc016ac0`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb at 1920 by 1080 by 24.
- Viewport: 1920 by 1080 px.
- Menu transform: scale `1.35` and recorded client origin `(386, 67)` with no internal reflow.
- Session identifier: `menu01-89904c8-b9171d37-7f5626df`.
- Selected background: `forest-foundry.png` from `resources/textures/menu-backgrounds/` at evidence head `cb9cc3ee3081b399c2d042ee62e1f7b0cc016ac0`.
- Runtime asset manifest revision: evidence head `cb9cc3ee3081b399c2d042ee62e1f7b0cc016ac0`, with the selected asset path recorded above.
- Shared workflow: use one application session with eight saved and selected persons. Keep Cora selected in Persons for both captures.
- `SS-001` state: show Deathmatch after retained four-team and Friendly Fire values become hidden.
- `SS-024` state: show Teams with four teams, Friendly Fire on, and unfocused Rounds `3` after gameplay returns.
- `SS-001` artifact: `MENU-01/non-team-consolidated-persons-1920x1080.png`.
- `SS-001` SHA-256: `b9171d3768b12eb247aac41482cf4b8af8d0b71525be661959b165a1ec098189`.
- `SS-024` artifact: `MENU-01/teams-consolidated-persons-retained-1920x1080.png`.
- `SS-024` SHA-256: `7f5626dfa6d05dfaceca70a19ad9d2fd403914f3672e8f16cc1f0a68e6af8a5e`.
- Geometry assessment: both artifacts show Persons at `x=10–324`, Players at `x=330–644`, Game Settings at `x=650–839`, and both 5 logical px gaps.
- Usability assessment: Rank, Name, Elo, and Trend remain legible. Player names, controller labels, spinner controls, and row `D` actions remain legible.
- Blocking finding: the person-name field keeps its former width. It extends beyond `x=324`, crosses the Persons-to-Players gap, and draws under the Players region in both artifacts.
- Unchanged presentation: the fixed canvas, uniform scaling, background, panel headers, list clipping, selected row, conditional Team controls, roster colors, score table, and three-action footer otherwise conform.
- Evidence status: both replacement artifacts have complete provenance but do not conform to the approved control-containment rule.
- Current status: historical after the person-name field containment fix.

The final PR #58 containment-fix packet has this provenance:

- Pull request: `#58`.
- Implementation branch: `feature/consolidate-person-list`.
- Rendering source SHA: `971157f2fc2dd7aecc581ac8be78c9dc7b11ce1d`.
- Evidence commit and exact final pull request head: `f294c742a08a90f03ea39b7ed0385d5b59ad684a`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, Xvfb at 1920 by 1080 by 24, and SDL dummy audio.
- Viewport: 1920 by 1080 px.
- Menu transform: fixed 850 by 700 logical canvas, scale `1.35`, recorded client origin `(386, 67)`, and no internal reflow.
- Session identifier: `menu01-971157f-9a7a49ad-d571ec67`.
- Selected background: `forest-foundry.png` from `resources/textures/menu-backgrounds/`.
- Runtime asset provenance: unchanged from the preceding PR #58 packet.
- Shared workflow: use one application session with eight saved and selected persons. Keep Cora selected in Persons for both captures.
- `SS-001` state: show Deathmatch after retained four-team and Friendly Fire values become hidden.
- `SS-024` state: show Teams with four teams, Friendly Fire on, and unfocused Rounds `3` after gameplay returns.
- `SS-001` artifact: `MENU-01/non-team-consolidated-persons-1920x1080.png`.
- `SS-001` SHA-256: `9a7a49ad360166759c549cb812d5b720f44b133668b4f537ebd5cce42f94a134`.
- `SS-024` artifact: `MENU-01/teams-consolidated-persons-retained-1920x1080.png`.
- `SS-024` SHA-256: `d571ec67bd5966b1a31fe93897515bb983ab83f433e96b68abfa1ed5bcbce6ee`.
- Geometry assessment: both artifacts show balanced 315-logical-pixel Persons and Players panels, the unchanged 190-logical-pixel Game Settings panel, and two clear 5-logical-pixel gaps.
- Containment assessment: the resized person-name field remains fully inside Persons. It does not draw into a gap or the Players region.
- Usability assessment: Rank, Name, Elo, and Trend remain aligned and legible. Player names, controller labels, spinner controls, and row `D` actions remain legible.
- Regression assessment: no clipping, overlap, panel, gap, or uniform-scaling regression is visible.
- Unchanged presentation: the background, keyline, panel headers, selected row, conditional Team controls, roster colors, score table, and three-action footer conform.
- Evidence status: both final artifacts conform and have complete required provenance.
- Current status: both artifacts are historical after the Equalize and Shuffle change because they show the obsolete `E` and `S` controls in non-Team and Teams states.

The PR #59 roster-order control packet has this provenance:

- Pull request: `#59`.
- Provenance reassessment and exact pull request head: `c31c28f577ac1388294462142b58633c0205beb5`.
- Implementation branch: `feature/equalize-shuffle-controls`.
- Implementation source SHA: `f68bb9c1c78f8d48e4ce316fa38d47dd4fd892a9`.
- Original screenshot artifact commit: `232f4c54b0d2bbaa40c6e5c0322e5811ee0f9d34`; the identified recapture produced byte-identical files.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image `duel6r-build:local`, Mesa software rendering, Xvfb, and SDL dummy audio.
- Viewport: 1920 by 1080 px.
- Selected background: `forest-foundry.png`.
- Runtime asset manifest revision: resource tree `3ae377dc261e9a4e407dc75dd35dfd7911fb8e79`.
- Session identifier: `MENU-01-PR59-20260902T120116Z-63cfbce6-823b-4b03-9775-afe3d04f5fb1`.
- Capture interval: 2026-09-02 12:02:11 through 12:02:28 UTC.
- Worktree state: the UX-owned manifest update was the only tracked modification; production, tests, and both screenshot files matched the pushed branch before recapture. Ignored build output supplied the binary from the implementation source SHA.
- Shared workflow: use the documented eight-person fixture and keep Cora selected in Persons.
- `SS-001` state: show Deathmatch after the retained four-team and Friendly Fire values become hidden.
- `SS-001` artifact: `MENU-01/deathmatch-roster-order-controls-hidden-1920x1080.png`.
- `SS-001` SHA-256: `817fc6a2c5c1e5fdc3d548cc43b77901c6575142917068bbab416e223a0e949a`.
- `SS-024` state: show Teams with four teams, Friendly Fire on, and unfocused Rounds `3` after gameplay returns.
- `SS-024` artifact: `MENU-01/teams-equalize-shuffle-visible-1920x1080.png`.
- `SS-024` SHA-256: `0919466ec83e94435e2a81336f55db4fa8215916a605dbc756ab531d9940b92c`.
- Visual assessment: both artifacts conform to `docs/design.md`, the `MENU-01` specification, and their conditional-layout wireframes.
- Conditional-layout assessment: `SS-001` has no visible `Equalize` or `Shuffle` control or residual button frame. `SS-024` shows separate, fully readable `Equalize` and `Shuffle` buttons.
- Hierarchy and containment assessment: both artifacts keep the approved three-panel hierarchy, balanced Persons and Players panels, clear panel gaps, and controls inside their panel bounds.
- Regression assessment: no clipping, overlap, label truncation, or inconsistent retro-control treatment is visible.
- Evidence boundary: the static artifacts do not prove random ordering, control-assignment preservation, immediate mode updates, or removal of hidden interaction targets.
- Evidence status: both artifacts conform for visual assessment and have complete required provenance.
- Current status: historical after PR #60 because both artifacts show the prior person-action arrangement.

The PR #60 person-action alignment artifacts have this provenance:

- Pull request: `#60`.
- Implementation branch: `feature/person-action-alignment`.
- Capture source head: `65555efa22b4f50c3cdf93c96014db213913eac9`.
- Fixed product baseline: `e75552f`.
- Implementation source SHA: `d783c2cf2224e5071d17566782ce843f50e5e49f`.
- Runtime asset Git tree: `399c8a8bdddd86e526e8811ae2da2461d9229866`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker image `duel6r-build:local`, Mesa software rendering, Xvfb at 1920 by 1080 by 24, and SDL dummy audio.
- Selected background: `forest-foundry.png` from `resources/textures/menu-backgrounds/`.
- Viewport: 1920 by 1080 px.
- Teams session identifier: deterministic post-capture identifier `MENU-01-PR60-TEAMS-65555ef-37bfd1c7`, anchored to the capture source head and artifact hash.
- Teams workflow and state: open the main menu in Teams mode with six selected players, two teams, Friendly Fire off, Rounds `0`, and visible `Equalize` and `Shuffle`.
- Teams artifact: `MENU-01/teams-person-action-aligned-1920x1080.png`.
- Teams SHA-256: `37bfd1c7fb6ca05616c6ea4fd6fd8c15d93fe5cdd74b2caba06b8c30d6f3b0d9`.
- Deathmatch session identifier: deterministic post-capture identifier `MENU-01-PR60-DEATHMATCH-65555ef-4494c9e5`, anchored to the capture source head and artifact hash.
- Deathmatch workflow and state: launch one application process with eight saved and selected persons in Deathmatch, Rounds `0`, and no `Equalize` or `Shuffle`; the person-action row remains at the same position used by Teams.
- Deathmatch artifact: `MENU-01/deathmatch-person-action-aligned-1920x1080.png`.
- Deathmatch SHA-256: `4494c9e5987beaf6d34e78a5156d52eb89cb8c6a502c6295ede2d0827c8acca3`.
- Capture worktree: production files and runtime resources matched implementation source `d783c2cf2224e5071d17566782ce843f50e5e49f`; the specialist-owned documentation and test changes were present for the Deathmatch capture and do not change the rendered production state.
- Visual assessment: the fixed canvas, three-panel hierarchy, 315-logical-pixel Persons and Players panels, 190-logical-pixel Game Settings panel, and clear panel gaps conform.
- Alignment assessment: `Remove`, `<<`, and `>>` form one contained row with the same visible centerline as `Equalize` and `Shuffle`.
- Add-placement assessment: the person-name field and `Add` form a separate row, and `Add` is to the right of the field.
- Containment assessment: all affected controls remain inside their panels without clipping or overlap.
- Evidence boundary: the static artifacts do not prove unchanged action behavior; the tester-owned behavioral coverage supplies the mode-position and interaction checks.
- Teams evidence status: `SS-024` conforms and has complete required provenance.
- Deathmatch evidence status: `SS-001` conforms and has complete required provenance.
- Focused UX assessment head: integrated PR #60 head `380d956932f675c9923b4e3836f15296b6956e52`.
- Visual assessment result: both artifacts conform to `SET-078`–`SET-083`, `AC-065`, `docs/design.md`, the `MENU-01` specification, and their applicable wireframes.
- Current status: historical after PR #62 because both artifacts show the shorter Persons list, higher person-name row, previous button geometry, and previous batch controller-detection caption.

The PR #62 person-list and action-button refinement artifacts have this provenance:

- Pull request: `#62`.
- Implementation branch: `feature/person-list-button-refinement`.
- Exact pushed artifact head: `316c43f99e2f0178b8581012393fd7ecd5353776`.
- Fixed product baseline: `88b72a6`.
- Production source SHA: `790c8b76f9e1f2c07260b466726db55bf83dc6db`.
- Environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, Xvfb at 1920 by 1080, and SDL dummy audio.
- Viewport: 1920 by 1080 px.
- Runtime asset Git tree: `399c8a8bdddd86e526e8811ae2da2461d9229866`.
- Selected background: `forest-foundry.png`.
- Selected background SHA-256: `fa3d12b5dac0508d44596671d54ecdd999b87772b4a58d743bbe25d335b1fef4`.
- Deathmatch session identifier: `MENU-01-PR62-DEATHMATCH-790c8b7-913cfae6`.
- Deathmatch state: show eight selected players in Deathmatch with no visible `Equalize` or `Shuffle` controls.
- Deathmatch artifact: `MENU-01/deathmatch-person-action-aligned-1920x1080.png`.
- Deathmatch SHA-256: `913cfae67e79e3ad41bd4e9c91d530749743acf364ef432670281fbf5edbc511`.
- Teams session identifier: `MENU-01-PR62-TEAMS-790c8b7-11c2b8f3`.
- Teams state: show six selected players, two teams, Friendly Fire off, Rounds `0`, and visible `Equalize` and `Shuffle` controls.
- Teams artifact: `MENU-01/teams-person-action-aligned-1920x1080.png`.
- Teams SHA-256: `11c2b8f35074a17e50395f0a55c64251a10c395cedcb06471a7902ef4e80edb3`.
- List assessment: both artifacts show one additional standard list row and put the person-name row one standard list row lower than the PR #60 layout.
- Button assessment: `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and `Detect All` use one visible height and keep clear caption space from every border.
- Naming assessment: both artifacts show `Detect All` for batch controller detection and retain `D` for every row-level action.
- Conditional-visibility assessment: Deathmatch hides `Equalize` and `Shuffle` without residual frames. Teams shows both controls with readable full captions.
- Hierarchy and containment assessment: both artifacts preserve the three-panel hierarchy, balanced Persons and Players widths, setup-panel gaps, control alignment, and containment without clipping or overlap.
- Evidence boundary: the static artifacts do not prove unchanged person-list content or action behavior. The visual assessment confirms that representative content and action affordances remain present.
- Evidence status: `SS-001` and `SS-024` conform to `SET-084`–`SET-091`, `AC-066`–`AC-069`, `docs/design.md`, the `MENU-01` specification, and both applicable wireframes.
- Visual gate: `Satisfied`.
- Current status: historical after PR #65 because both artifacts show `<<` in Persons.

The PR #65 person-card button-alignment evidence has this provenance:

- Pull request: `#65`.
- Implementation branch: `feature/person-card-button-alignment`.
- Final PR head and UX assessment head: `c2d72fb4d2c2bb27e6e525d88368543d710eb5b9`.
- Repository capture head and Teams screenshot artifact commit: `adf7293d14413e5987af09a166cad19a1db8662e`.
- Capture source state: `fe395f5998ed6eb5ca44a04db9b0cc861bb7639f`.
- Production source SHA: `8eeb60061c32d4ecf1088e5bbf710b691bb76fb1`.
- Historical freshness note: production implementation was unchanged between the production source SHA, capture source state, and final PR #65 head.
- Environment: Release, GL4, Ubuntu 24.04 Docker, Mesa software rendering, Xvfb, and SDL dummy audio.
- Viewport: 1920 by 1080 px.
- Teams state: show eight saved persons, six selected players, two teams, Friendly Fire off, Rounds `0`, visible `Equalize` and `Shuffle`, and Cora selected.
- Teams artifact: `MENU-01/teams-person-action-aligned-1920x1080.png`.
- Teams SHA-256: `0d41aab7d14301b11a52efff3c7149bb31f42a62001b376b036b09d74ff5e4b0`.
- Alignment assessment: Persons shows `Remove` at bottom-left and `>>` at bottom-right.
- Players alignment assessment: Players shows `<<` at bottom-left and `Detect All` at bottom-right.
- Group assessment: `Equalize` and `Shuffle` form one horizontal group with equal visible space on each side in the span between the Players edge controls.
- Hierarchy and containment assessment: all controls remain on one shared centerline inside their applicable panels without clipping or overlap.
- Legibility assessment: all captions retain clear border padding and the existing text contrast.
- Responsive assessment: the complete fixed-layout canvas is centered and uniformly scaled at the 135% cap.
- Accessibility assessment: the moved `<<` control is adjacent to Players and gives a clearer spatial cue for roster removal without changing the symbolic caption.
- Evidence boundary: the static artifact does not prove action behavior or the non-Team conditional state.
- Teams evidence status: `SS-024` conforms to the user-approved PR #65 placement requirement, `docs/design.md`, the `MENU-01` specification, and the Teams wireframe.
- Non-Team session identifier: `MENU-01-PR65-SS001-8eeb600-c170a844`.
- Non-Team workflow and state: use one application process with Alice through Hana, select all eight players, select Teams with four teams and Friendly Fire on, switch to Deathmatch, keep Cora selected, and keep Rounds `0`.
- Non-Team artifact: `MENU-01/deathmatch-person-action-aligned-1920x1080.png`.
- Non-Team SHA-256: `c170a844b1d2777ec605cdc375712cd83481da368d512a9da74af2fed0999273`.
- Non-Team alignment assessment: Persons shows `Remove` at bottom-left and `>>` at bottom-right, and Players shows `<<` at bottom-left and `Detect All` at bottom-right.
- Hidden-control assessment: no `Equalize` or `Shuffle` caption, frame, or other residual visible target appears in the Players action row.
- Centerline and containment assessment: both action rows use one centerline, and every affected control stays inside its panel without clipping or overlap.
- Non-Team legibility assessment: all four captions retain clear border padding and the existing contrast.
- Unchanged-presentation assessment: the fixed canvas, three-panel hierarchy, panel widths, panel gaps, expanded Persons list, lowered person-name row, row-level `D` controls, statistics table, scaled background, keyline, and three-action footer remain unchanged.
- Shared capture background: `forest-foundry.png`.
- Non-Team evidence status: `SS-001` conforms to the user-approved PR #65 placement requirement, `docs/design.md`, the `MENU-01` specification, and the non-Team wireframe.
- Evidence boundary: the non-Team static artifact confirms that no residual control is visible, but it does not prove removal of hidden interaction targets or unchanged action behavior.
- Historical evidence status: `SS-001` and `SS-024` conformed at final PR #65 head `c2d72fb4d2c2bb27e6e525d88368543d710eb5b9`. PR #69 replaces both destination artifacts.

The PR #69 canonical refresh uses these two provenance packets:

- Pull request: `#69`.
- Implementation branch: `docs/refresh-canonical-screenshots`.
- Final assessment head: `f4708d337bb82be55c553c64608bd75ccd64121f`.
- Current hash-verification head: `dee7e05660f448fc6a558c9f9c8b6009805c1e9f`.
- First screenshot artifact commit: `4517a3c`.
- First packet capture source SHA: `15606304305a7b43b2032f2d5c6c98f1c4245a7d`.
- First packet entries: `SS-001`, `SS-024`, `SS-003`, `SS-007`, `SS-008`, and `SS-014`.
- First packet environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, Xvfb, and SDL dummy audio.
- First packet viewports: 1920 by 1080 px for `SS-001` and `SS-024`; 1280 by 900 px for `SS-003`, `SS-007`, `SS-008`, and `SS-014`.
- Menu workflow: capture `SS-001` and `SS-024` in one uninterrupted application session.
- Menu session identifier: `MENU-01-REFRESH-1560630-eca1697b-b8e27a93`.
- Selected menu background: `forest-foundry.png`.
- Runtime asset Git tree: `399c8a8bdddd86e526e8811ae2da2461d9229866`.
- `SS-007` state: Bravo is the predator.
- `SS-001` SHA-256: `b8e27a9390294aeb1bb6c31d40843e680a0ac4a45ed9fa96fb4af5b48f0a604b`.
- `SS-024` SHA-256: `eca1697b5ea69f7d7034e0df1b8c25c48215f4434c90a5ea384bfd2ad454f182`.
- `SS-003` SHA-256: `4142e5432d29b78413a2febdc207f2b08f9c2f9d4b23cb43e89d3116cf1c39b2`.
- `SS-007` SHA-256: `cee68ed2c71fc5144c25f26eeb8d9192cde507ac3692e9c3f6d114241d965b5f`.
- `SS-008` SHA-256: `898e538e2c73aba9ec4758d69f0d0508149b0f6943b3f6d4ddcfcc7a842963b3`.
- `SS-014` SHA-256: `2aad38e15b0ed0057768320938e9ee7cdd4848d45dff7d4a36b7710a126849e5`.
- Final replacement artifact commit: `e8fd7a8`.
- Final replacement capture source SHA: `2475af83246a98ef2334f6d6fc90a9e943374eff`.
- Final replacement entries: `SS-009`, `SS-010`, `SS-011`, and `SS-012`.
- Final replacement capture time: 2026-09-03 at approximately 14:42 UTC.
- Final replacement environment: Release, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, Xvfb at 1280 by 900 by 24, SDL dummy audio, complete client area, no window chrome, and no retouching.
- `SS-009` SHA-256: `f8b67d047ab0fc1d23428ad32bddd6c9fe0977670c524a69bf09ad834c332c92`.
- `SS-010` SHA-256: `f916ef240e4ac39dff93abc7a2e742aa070ec7414f7e13f218f00f61bd2cc4bc`.
- `SS-011` SHA-256: `33847eebcc00b1e23ab0d7bb2a1eeff879a2575ee13e46401ec18a8292f8158e`.
- `SS-012` SHA-256: `e8981233c251b337f7ef53e2f4486b4c5e49d48b6c32cd83172c6cff928e9098`.
- Exact-byte verification: all ten recorded SHA-256 values match their matrix destination artifacts at current hash-verification head `dee7e05660f448fc6a558c9f9c8b6009805c1e9f`.
- Source correction `2475af83246a98ef2334f6d6fc90a9e943374eff` restored the then-required separator-free final-summary behavior.
- Tester behavior coverage is integrated at final assessment head `f4708d337bb82be55c553c64608bd75ccd64121f`.
- Historical assessment status: all ten implemented artifacts conformed to the canonical design system, screen specifications, and wireframes at the PR #69 assessment head.
- Current assessment status: `SS-012` is stale because it has no Team separator bands and no `End of Game` notice.

## Screenshot matrix

| ID | Screen ID | Wireframe | Route or workflow | Representative state and setup data | Viewport | Expected visible behavior | Destination path | Provenance and status |
|---|---|---|---|---|---:|---|---|---|
| <a id="ss-001"></a>`SS-001` | `MENU-01` | [Non-Team Game Settings](../screens/wireframes/menu-main.md#menu-01-a--non-team-state) | Start one application process with the shared consolidated-person fixture, select all eight persons, select `Teams`, set `Num. of Team` to `4`, turn Friendly Fire on, and then select `Deathmatch`. | Use Deathmatch with eight selected players. Alice and Bruno are ranked. Cora through Hana are unranked. Keep Cora selected and Rounds `0`. Retain the hidden four-team and Friendly Fire-on values. | 1920x1080 | Persons and Players must each use 315 logical px with a 5 px gap. Persons must show `Remove` at bottom-left and `>>` at bottom-right. Players must show `<<` at bottom-left and `Detect All` at bottom-right. All four buttons must use the common height with visible caption padding. Team settings, `Equalize`, and `Shuffle` must be absent without residual frames or interaction targets. Both action rows must keep the Teams-state centerline. Roster rows must use standard colors. All controls must remain inside their panel bounds without clipping or overlap. | `docs/screenshots/MENU-01/deathmatch-person-action-aligned-1920x1080.png` | PR #69 first packet. The artifact shows the current three-action footer and conforms. `Conforms`. |
| <a id="ss-002"></a>`SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | After issue #38 implements the Network footer, select Clear or press F3 from the populated menu. | Use four saved people, two selected players, Deathmatch, persistent score data, and documented default settings; show `Really delete? (Y/N)`. | 1920x1080 | The unchanged scaled target menu behind the strip must visibly include equal-width `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` footer actions. | `docs/screenshots/MENU-02/network-entry-confirmation-1920x1080.png` | `Planned` for downstream issue #38. The existing three-action-footer capture is not valid for this target wireframe. |
| <a id="ss-003"></a>`SS-003` | `PLAY-01` | [Shared arena play](../screens/wireframes/play-fullscreen.md) | Start a local Deathmatch and press F2 once during live play. | Use 15 living players, ranking on, a finite round limit, and a state after the start fade. | 1280x900 | One undivided arena must show the whole level and all 15 players. F2 must not change the view. Live ranking, round progress, event messages, and player status must remain available. | `docs/screenshots/PLAY-01/shared-15-player-1280x900.png` | PR #69 first packet. The actual 15-player Deathmatch keeps one complete shared arena, ranking, and round progress. `Conforms`. |
| <a id="ss-007"></a>`SS-007` | `MODE-01` | [Predator](../screens/wireframes/mode-predator.md) | Start Predator from the menu. | Use three living players, one predator, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show the faint predator body, visible predator weapon, opaque marines, live ranking, round progress, events, and status cues. | `docs/screenshots/MODE-01/shared-live-1280x900.png` | PR #69 first packet. Bravo is the predator. Identity, opacity contrast, ranking, and shared-arena presentation conform. `Conforms`. |
| <a id="ss-008"></a>`SS-008` | `MODE-02` | [Team mode](../screens/wireframes/mode-team.md) | Start `Team deathmatch (2 teams, FF: off)` from the menu. | Use two living players per team, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show all four players. Team apparel and named grouped ranking must remain visible with events, status cues, and round progress. | `docs/screenshots/MODE-02/shared-live-1280x900.png` | PR #69 first packet. Four players, named grouped ranking, team apparel, and round progress conform in one shared arena. `Conforms`. |
| <a id="ss-009"></a>`SS-009` | `PLAY-05` | [Sudden death](../screens/wireframes/play-sudden-death.md) | Start Deathmatch with Quick Liquid on and reach rising water. | Use two living players, ranking on, raised water in the safe arena, and one submerged player. | 1280x900 | One undivided arena must show both players, raised water, the air indicator, live ranking, status cues, events, and round progress. | `docs/screenshots/PLAY-05/shared-rising-water-1280x900.png` | PR #69 final replacement packet. P02 is submerged with a visible blue air bar. Raised water, both players, ranking, and round progress conform. `Conforms`. |
| <a id="ss-010"></a>`SS-010` | `OVER-01` | [Score tab](../screens/wireframes/overlay-score-tab.md) | Start a local four-team Team deathmatch and press Tab during an active round before a winner exists. | Use eight ranked players with two players in each of Alpha, Bravo, Charlie, and Delta. Use non-zero K, A, D, K/D, and PTS values. Keep the finite-round arena progress visible. | 1280x900 | The centered score panel must show four ranked team groups over one undivided live arena. Each team row must touch its two nested player rows. Each of the three adjacent group boundaries must use an 8 px band with a centered 2 px rule across the table width. Team names, colors, headings, values, ranking, columns, Tab behavior, and arena progress must remain unchanged. The winner curtain must not appear. | `docs/screenshots/OVER-01/shared-score-tab-1280x900.png` | PR #69 final replacement packet. Active round 3, persisted rounds 2, non-zero values, four contiguous groups, three separator rules, arena progress, and no curtain conform. `Conforms`. |
| <a id="ss-011"></a>`SS-011` | `OVER-02` | [Round over](../screens/wireframes/overlay-round-over.md) | Resume a five-round local four-team Team deathmatch with two completed rounds, then finish round 3 as a non-final round. | Use eight ranked players with two players in each of Alpha, Bravo, Charlie, and Delta. Use non-zero values and one winning team. Capture after the completed count becomes `3`. | 1280x900 | The winner-curtain sequence and centered score panel must appear over one undivided arena. The dedicated progress row must show exact `Rounds: 3|5` at its documented right alignment. The centered `---SCORE---` strip, values, ranking, columns, controls, and hidden arena progress must remain unchanged. Each team row must touch its two nested player rows. Each of the three adjacent group boundaries must use an 8 px band with a centered 2 px rule across the table width. | `docs/screenshots/OVER-02/team-interim-round-3-of-5-1280x900.png` | PR #69 final replacement packet. Persisted round 3, Alpha win, six opponent deaths, `Rounds: 3|5`, hidden arena progress, four contiguous groups, and three interim separator rules conform. The capture represents the opening phase of the animated curtain sequence. `Conforms`. |
| <a id="ss-012"></a>`SS-012` | `OVER-03` | [Game over](../screens/wireframes/overlay-game-over.md) | Start a one-round local four-team Team deathmatch and finish its only round. | Use eight ranked players with two players in each of Alpha, Bravo, Charlie, and Delta. Use non-zero K, A, D, K/D, and PTS values. Use one winning team. Capture while the final summary and curtain are visible. | 1280x900 | The final Team outcome, score panel, dark red curtain, and final round progress must appear over one undivided arena. Each team row must touch its nested player rows. Each of the three adjacent Team boundaries must match the 8 px band and centered 2 px rule used in non-final `OVER-02`. A horizontally centered blue notice at the bottom must show exact white text `End of Game`. The notice must keep its 16 px bottom inset, remain separate from the panel by at least 16 px, and must not obscure any score content. | `docs/screenshots/OVER-03/shared-game-over-1280x900.png` | `Pending`. The PR #69 artifact at this path is stale because it has no Team separators and no `End of Game` notice. Replace it after implementation and supply complete provenance. |
| <a id="ss-013"></a>`SS-013` | `CONS-01` | [Console over menu](../screens/wireframes/console-menu.md) | After issue #38 implements the Network footer, open the console over the populated local menu. | Use the planned four-action footer, recent startup output, and an empty input line. | 1920x1080 | The console spans the client width while the visible scaled menu below includes equal-width `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` actions. | `docs/screenshots/CONS-01/network-entry-open-1920x1080.png` | `Planned` for downstream issue #38. The existing three-action-footer capture is not valid for this target wireframe. |
| <a id="ss-014"></a>`SS-014` | `CONS-02` | [Console over play](../screens/wireframes/console-gameplay.md) | Open the console during active Deathmatch and enter or inspect any command that previously affected the gameplay view. | Use four living players, ranking on, a finite round limit, and recent game output. | 1280x900 | The console must overlay one undivided arena. The command must not create player-specific views. The simulation, ranking, round progress, events, and status state must remain present behind the console where visible. | `docs/screenshots/CONS-02/shared-open-1280x900.png` | PR #69 first packet. The full-width console overlays one shared arena, and the visible gameplay context remains undivided. `Conforms`. |
| <a id="ss-015"></a>`SS-015` | `NET-01` | [Network entry](../screens/wireframes/network-entry.md) | From `MENU-01`, activate `Network (F2)`. | Default target state with Host focused and direct same-machine/LAN scope visible. | 1920x1080 | The scaled retro canvas shows Host, Join, Back, supported platforms, direct endpoint scope, host-alone lobby 1–15, match 2–15, and no unsupported affordances. | `docs/screenshots/NET-01/default-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-016"></a>`SS-016` | `NET-02` | [Host setup](../screens/wireframes/network-host-setup.md) | Choose Host from `NET-01`. | Valid port and two configured host local players in editable host setup. | 1920x1080 | Host setup shows retained port/local players, capacity, Start session, Back, and validation. Pending variants expose the 10-second boundary and Cancel back to retained editable setup with no listener. | `docs/screenshots/NET-02/host-setup-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-017"></a>`SS-017` | `NET-03` | [Join connecting](../screens/wireframes/network-join.md) | Choose Join, enter `192.168.1.24:27015`, configure two local players, and activate Connect. | Connecting to `192.168.1.24:27015` with two local players and Cancel available. | 1920x1080 | Endpoint and local setup remain visible with truthful Connecting copy and the 10-second total boundary; Cancel retains setup and the screen does not claim admission. | `docs/screenshots/NET-03/connecting-two-local-players-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-018"></a>`SS-018` | `NET-04` | [Lobby](../screens/wireframes/network-lobby.md) | Host a session, admit two guests, and configure six total players. | Host waiting with 3 participants, 6 players, and one named unready guest. | 1920x1080 | Separate Role, Connection, and Readiness columns, ownership, settings, roster, disabled Start, and `Waiting for <participant> to be ready` are visible; Leave/End variants use confirmations. | `docs/screenshots/NET-04/host-waiting-one-unready-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-019"></a>`SS-019` | `NET-05` | [Network match](../screens/wireframes/network-match.md) | Start a LAN Deathmatch after all participants are ready. | Six living players across connected participants during active play. | 1280x900 | One undivided arena must preserve full level, six players, ranking, round progress, events, status, and compact truthful LAN session context. | `docs/screenshots/NET-05/six-player-lan-deathmatch-1280x900.png` | `Planned` for downstream issue #38; no playable network session exists. |
| <a id="ss-020"></a>`SS-020` | `NET-06` | [Final summary](../screens/wireframes/network-summary.md) | Complete a configured three-round network match. | Final authoritative three-round result for all connected participants. | 1280x900 | Summary shows totals, `Session only`, no persistence, applicable actions, retained/departed rows, clearing at new-match start, and discard on session end. | `docs/screenshots/NET-06/final-three-round-summary-1280x900.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-021"></a>`SS-021` | `NET-07` | [Guest reconnect](../screens/wireframes/network-reconnect.md) | Interrupt a guest connection during an active network match. | Reconnecting with 24 positive ceiling seconds remaining after host crash or another ambiguous transport failure. | 1280x900 | Endpoint, unchanged reservation deadline, no-input state, active simulation, retryable status, and `Leave session` consequence are visible; the state does not claim host end or player removal. | `docs/screenshots/NET-07/reconnecting-24s-1280x900.png` | `Planned` for downstream issue #38; reconnect is not implemented. |
| <a id="ss-022"></a>`SS-022` | `NET-08` | [Connection failure](../screens/wireframes/network-failure.md) | Attempt to join an unavailable direct endpoint. | Initial `Host unreachable.` with endpoint context, Retry, Edit setup, and Return to Network. | 1920x1080 | Initial admission uses fixed precedence; terminal reconnect disables Retry; expiry uses truthful copy; host-local service failure uses `Hosted session stopped unexpectedly.` only for the host. | `docs/screenshots/NET-08/host-unreachable-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-023"></a>`SS-023` | `NET-09` | [Host-ended session overlay](../screens/wireframes/network-host-ended.md) | Receive a valid intentional host End session notice during an active network match. | Host-ended outcome over the last authoritative arena context. | 1280x900 | A blocking `HOST ENDED SESSION` overlay states no migration/resume or persistence and can arise only from an End notice accepted through the current established session. | `docs/screenshots/NET-09/host-ended-1280x900.png` | `Planned` for downstream issue #38; host-ended UI is not implemented. |
| <a id="ss-024"></a>`SS-024` | `MENU-01` | [Teams Game Settings](../screens/wireframes/menu-main.md#menu-01-b--teams-state) | Open the main menu, select six players, and select `Teams`. | Use eight saved persons, six selected players, two teams, Friendly Fire off, Rounds `0`, visible `Equalize` and `Shuffle`, and Cora selected. | 1920x1080 | Persons must show `Remove` at bottom-left and `>>` at bottom-right. Players must show `<<` at bottom-left and `Detect All` at bottom-right. `Equalize` and `Shuffle` must form a centered group in the available span between the Players edge controls. All six buttons must use one common height with visible caption padding. The action rows must align and remain contained without overlap. | `docs/screenshots/MENU-01/teams-person-action-aligned-1920x1080.png` | PR #69 first packet. The current three-action footer, six-player two-team setup, controls, team colors, alignment, and containment conform. `Conforms`. |

## Coverage

- Required wireframes: 21.
- Required representative screenshot entries: 21.
- Fully conforming screenshot entries: 10.
- Non-conforming screenshots: 0.
- Planned screenshot entries awaiting downstream issue #38: 11.
- Pending screenshot assessments: 0.
- Screenshots per wireframe: exactly one.
- Retired screenshot matrix entries: `SS-004`, `SS-005`, and `SS-006`.
- Coverage status: all ten implemented entries conform. Eleven planned entries have no current artifacts.

The matrix uses one representative state for each wireframe.
The screen specifications and wireframes document other player counts, modes, interaction states, and accessibility limits without multiplying screenshot entries.

## Issue #30 operational evidence matrix

This matrix was prepared for issue #30 and draft PR #50 at reviewed head `4eb4291d3090e2a3b9de8fc83f130671d8b3eb5e`.
The final UX reassessment covers the implemented scope at exact head `af51bad5bab2cf02e657f3cd8aa71d76cabd9e27`.
The issue #30 graphical screenshot matrix contains exactly zero entries.
No affected wireframe has an implemented graphical state in issue #30.
Terminal or command-line output is operational evidence and is not an implementation screenshot.
Operational evidence must record the implementation branch, source SHA, environment, command or test scenario, exact machine outcome, exact visible output when applicable, and artifact path.
The implementation evidence sources are `source/network/SessionTransport.h`, `source/network/SessionTransport.cpp`, and `source/server/HeadlessServer.cpp`.
The registered operational evidence sources are `tests/AdmissionCompatibilityTests.cpp`, `tests/AdmissionProcessTests.py`, `tests/SessionTransportTests.cpp`, and `tests/SessionTransportCTestRegistration.cmake`.
The applicable workflow evidence sources are `.github/workflows/branch.yml`, `.github/workflows/native-windows-transport.yml`, and `docker/build-windows-native-transport.cmd`.

| Operational scenario | Required substitute evidence |
|---|---|
| Host-local invalid manifest | A test or recorded protocol/CLI artifact must show `host-gameplay-content-manifest-invalid`, the exact host-visible copy, no listener or session, and Retry-disabled semantics for the current application session. |
| Guest-local invalid manifest | A test or recorded protocol/CLI artifact must show `guest-gameplay-content-manifest-invalid`, the exact guest-visible copy, no resolution or connection, retained setup destinations, and Retry-disabled semantics until restart. |
| Initial admission precedence | Automated results must exercise all 11 ordered identifiers and must show that the first applicable complete host result wins. |
| Initial outcome copy | Automated results must assert every exact rejection string, including release, invalid manifest, content mismatch, match started, session full, and host policy outcomes. |
| Malformed host messages | Actual-process results must show that malformed, trailing, unexpected, or inconsistent complete host messages use `invalid-host-admission-message` and `Connection ended before admission completed.` without dynamic peer detail. |
| Atomic cancellation and acceptance gate | Automated results must show one total order for Cancel, acceptance enqueue, and outcome publication; Cancel before enqueue or publication must produce no visible host result or success. |
| Transport precedence | Automated results must distinguish name-resolution failure, unreachable or refusal, close before complete admission, and no complete result at the 10-second deadline. |
| Cancellation and destinations | Automated or integration results must show that Cancel and local validation win, retained setup is preserved, and no pre-admission identity or slot remains. |
| Reconnect compatibility handoff | Automated results must assert the exact release and content restoration copy and disabled reconnect Retry semantics. |
| Non-disclosure | Automated results must inject untrusted names, release IDs, capabilities, paths, hashes, counts, credentials, addresses, thresholds, and payloads and must show that user-visible output contains none of them. |
| Local Play preservation | A regression result must show that `Play (F1)` starts and completes without a listener, server, transport worker, client connection, or network availability. No fresh screenshot is required because Local Play visuals must not change. |

Issue #38 must provide exactly one representative graphical screenshot for each implemented affected wireframe when it implements the planned UI.

## Issue #31 operational evidence matrix

This matrix was prepared for issue #31 and draft PR #51 at reviewed head `7e6255ffb2a5c48a65c7666ed3afe1cd3450aebf`.
The final UX reassessment covers the implemented lifecycle scaffold at exact head `413d1c3d33812c0199757b0b496f7f6d4c8e254a`.
The issue #31 graphical screenshot matrix contains exactly zero entries.
Terminal output, automated results, and process-lifecycle records are operational substitutes and are not implementation screenshots.
Each substitute must record the implementation branch, source SHA, environment, command or test scenario, machine outcome, user-visible output when applicable, and artifact path.
The reviewed implementation evidence sources are `source/client/HostServiceSupervisor.h`, `source/client/HostServiceSupervisor.cpp`, `source/client/HostServiceProcess.cpp`, and `source/client/HostSupervisorMain.cpp`.
The reviewed operational test sources are `tests/HostServiceSupervisorTests.cpp`, `tests/HostServiceProcessTests.py`, `tests/HostServiceTestChild.cpp`, and `tests/SessionTransportCTestRegistration.cmake`.
The registered tests cover byte-exact identifiers and copy, strict startup timing, exit-sealed status draining and fixed outcome precedence, cancellation precedence, retained setup, cleanup-gated Retry and dismissal, post-readiness failure, the observable exactly-once intentional-end handoff boundary, handoff exclusion from all other outcomes, process cleanup, non-disclosing output, and scaffold-only scope truth.

| Operational scenario | Required substitute evidence |
|---|---|
| Starting and readiness | An automated or integration result must show one attempt, the 10-second deadline, no second Start, no readiness claim before all readiness conditions complete, and `NET-04` eligibility only strictly before the deadline. |
| Cancel with retained setup | An automated or integration result must show that accepted Cancel wins before a later terminal result, cleanup completes, no listener or service remains, and the endpoint and local-player setup are retained for editable `NET-02`. |
| Port unavailable | A result must assert `host-service-port-unavailable`, exact copy `The selected port is unavailable. Choose another port and try again.`, precedence before timeout, completed cleanup, and eligible Retry when retained setup remains valid. |
| Generic start failure | A result must assert `host-service-start-failed`, exact copy `Hosted session could not start.`, completed cleanup, and eligible Retry when retained setup remains valid. |
| Exited before readiness | A result must assert `host-service-exited-before-ready`, exact copy `Hosted session stopped before it was ready.`, completed cleanup, and eligible Retry when retained setup remains valid. |
| Startup timeout | A result must assert `host-service-startup-timed-out`, exact copy `Hosted session startup timed out.`, no readiness accepted at or after 10 seconds, no remaining listener or service, and eligible Retry when retained setup remains valid. |
| Post-readiness unexpected stop | A result must assert `host-service-stopped-unexpectedly`, exact host copy `Hosted session stopped unexpectedly.`, discarded session-only results, no automatic restart, disabled Retry, and retained `NET-02` destination through Edit setup. |
| Retry and destinations | Automated results must exercise every Retry eligibility condition, cleanup gating, the new 10-second deadline on eligible Retry, disabled Retry after an active session ends, Edit setup to retained `NET-02`, and Return to Network to `NET-01`. |
| End session and other termination | Integration results must distinguish confirmed End session from normal shutdown, crash, forced termination, and service failure. Only confirmed End session may emit the exactly-once `intentional-host-end` handoff. Issue #31 sends no guest notice and claims no `NET-09` route; issue #36 owns notice delivery and issue #38 owns presentation. |
| Non-disclosure | Automated results must inject endpoint, process, path, command, credential, payload, and operating-system error details and must show that host lifecycle user copy contains none of them. Trusted diagnostics must satisfy the trust policy. |
| Local Play preservation | A regression result must show that `Play (F1)` starts and completes without creating, adopting, starting, stopping, or supervising a hosted service and without network availability. No fresh screenshot is required because Local Play visuals must not change. |
| Scope truth | Build or test output must not claim graphical network UI, a playable network session, or release readiness from issue #31. |

Issue #38 must capture `NET-02`, `NET-08`, and `NET-09` only when their planned graphical wireframes become implemented and reachable.

## Issue #32 operational evidence matrix

This matrix was prepared for issue #32 and draft PR #53 at specification head `03fdc0c481b183b695eb937ea0c7b0e93b4e31b9`.
The final UX reassessment covers the merged headless scope at exact head `5c18155ef16ba5c1ffcbe71e4de433873ed0e072`.
The final tester execution provenance remains recorded against implementation base `d38ba491505d670e20c71162aa896e3f520750a5` in `tests/AuthoritativeMatchAcceptanceCoverage.md`.
The merged-head reassessment confirms that the reviewed copy, result, state, authority, session-only, non-disclosure, persistence, and scope contracts remain unchanged.
The issue #32 graphical screenshot matrix contains exactly zero capturable entries.
Terminal output, serialized results, automated results, and process-lifecycle records are operational substitutes and are not implementation screenshots.
Each substitute must record the implementation branch, source SHA, environment, command or test scenario, machine identifier, exact user-visible output when applicable, exit status when applicable, and artifact path.
The reviewed implementation sources are `source/server/AuthoritativeMatch.cpp`, `source/server/AuthoritativeMatchTypes.h`, `source/server/AuthoritativeMatchValidation.cpp`, `source/server/AuthoritativeHostedMatchController.cpp`, `source/server/AuthoritativeMatchSerialization.cpp`, and `source/server/AuthoritativeMatchCli.cpp`.
The reviewed operational evidence sources are `tests/AuthoritativeMatchBehaviorTests.cpp`, `tests/AuthoritativeMatchProcessTests.py`, and `tests/AuthoritativeMatchAcceptanceCoverage.md`.
The registered evidence covers fixed outcome copy, completed and interrupted results, ranking, host-only controls, session-only output, script exclusion, persistence-field exclusion, bounded non-disclosing failures, Local Play isolation, and scaffold-only scope truth.
No reviewed source adds graphical network UI, changes a Local Play visual requirement, or claims a playable network session or release readiness.

| Operational scenario | Required substitute evidence |
|---|---|
| Supported modes and settings | Automated results must cover Deathmatch, Predator, all six Team deathmatch variants, all three level plans, round limits 1 and 99, rejected limits, Assistance, Quick Liquid, and Burnable Trees. Results must show that unsupported and content-owned settings are rejected. |
| Six-second round end | A deterministic trace or automated result must show one second of normal updates, five seconds without normal round updates, automatic non-final advancement, and no advancement after the final round. |
| Host authority | Automated results must show host-only early advancement after an outcome, rejected guest and arbitrary-key advancement, rejected pre-winner Shift+F1 advancement, and host-only End session. |
| Approved terminal copy | Actual-process or automated results must assert `authoritative-match-completed` with `Authoritative match completed.`, `authoritative-match-interrupted-no-winner` with `Authoritative match ended without a winner.`, and `authoritative-match-ended-intentionally` with `Authoritative match ended by the host.`. |
| Completed and interrupted results | Serialized result fixtures and automated assertions must cover `Completed`, `Interrupted`, `No winner`, all match, round, player, and team fields, total-points arithmetic, player and team ranking precedence, and completed-round retention on interruption. |
| Session-only and scripts | Automated results must show exact `Session only` labeling, no writes to statistics, Elo, people, profiles, saves, or history, and no optional Lua or profile script loading or execution. |
| Invalid settings | An actual-process or automated result must assert `authoritative-match-settings-invalid`, exact copy, no round or result, cleared readiness, and corrected-settings eligibility for a later start. |
| Content unavailable | An actual-process or automated result must assert `authoritative-match-content-unavailable`, exact copy, no round or result, cleared readiness, blocked later start in that hosted session, and host End session availability. |
| Runtime failure | An actual-process or automated result must assert `authoritative-match-runtime-failed`, exact service copy, stopped progression, rejected later actions, discarded results, orderly shutdown, host mapping to `Hosted session stopped unexpectedly.`, and no intentional host-end implication. |
| Cleanup failure | An actual-process result must assert `authoritative-match-shutdown-failed`, exact copy, exit status 4, no partial published result, and replacement of an earlier process result. |
| Exit meanings and precedence | Automated results must cover exit statuses 0, 2, 3, and 4, permitted identifiers, start-time and in-match precedence, and the first established outcome rule. |
| Non-disclosure | Automated results must inject peer-supplied names, release values, capabilities, paths, hashes, counts, credentials, addresses, thresholds, payloads, and raw filesystem values and must show that user-facing output contains none of them. |
| Local Play preservation | A regression result must show unchanged Local Play unlimited-round behavior, advancement, optional scripting, persistence, and independence from renderer-free authoritative service startup. No fresh screenshot is required because Local Play visuals must not change. |
| Scope truth | Build, test, and process output must not claim graphical network UI, playable end-to-end networking, or release readiness from issue #32. |

Issue #38 must provide exactly one representative graphical screenshot for each implemented affected wireframe when it implements these planned states.
