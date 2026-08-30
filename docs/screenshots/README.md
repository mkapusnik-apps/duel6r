# Implementation screenshot manifest

## Assessment status

This manifest requires exactly one representative screenshot entry for each wireframe. An entry is `Planned` until its target UI exists and is captured; planned paths are not implementation evidence.
The 2026-08-26 approved menu presentation affects `MENU-01`, `MENU-02`, and `CONS-01`.
It preserves the retro logical layout while adding uniform responsive scaling, a session-persistent blurred and scrimmed gameplay still, and a canvas keyline.
The previously conforming `MENU-01/default-1706x938.png`, `MENU-02/confirmation-1706x938.png`, and `CONS-01/open-1706x938.png` artifacts are now stale historical evidence and are not representative matrix artifacts.
The prior `SS-002` and `SS-013` artifacts remain valid historical evidence for the current three-action menu, but they are not evidence for the changed target footer visible behind those overlays.
The other eight existing representative screenshots remain current because their target presentation is unchanged.
Issue #28 changes target `MENU-01`, its visible background in `MENU-02` and `CONS-01`, and adds `NET-01`–`NET-09`. Exactly 12 planned entries cover those 12 affected wireframes. No current application screenshot is valid for those changed targets because the Network action and network screens are not implemented; downstream issue #38 owns implementation and recapture.
Issue #30 defines protocol, command-line, or scaffold outcomes only.
Issue #30 has zero capturable graphical entries because it must not implement the planned network screens.
The existing 12 planned issue #38 entries remain unchanged and must not be used as issue #30 evidence.
Issue #31 defines future visual states and copy but remains headless or scaffold-only at final reviewed implementation head `413d1c3d33812c0199757b0b496f7f6d4c8e254a`.
Issue #31 has exactly zero capturable graphical entries because it must not implement `NET-02`, `NET-08`, `NET-09`, or another network screen.
The issue #31 visual specification changes no representative layout and adds no wireframe.
The 20-entry wireframe-based screenshot matrix therefore remains unchanged with exactly one representative entry per wireframe.
The existing 12 planned issue #38 entries must not be used as issue #31 evidence.
The obsolete `PLAY-02`, `PLAY-03`, and `PLAY-04` artifacts are not evidence and are not in this manifest.

## Provenance requirements

Each supplied artifact must include the implementation branch, source SHA, environment, route or workflow, state, viewport, destination path, selected background filename, runtime asset manifest revision, and session identifier when the menu background is visible.
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
- Screenshot artifact commit: `1fe5b617ef88d0e66a781f11db7757155a1e8364`.
- Environment: Debug, GL4, Lua enabled, Ubuntu 24.04 Docker, Mesa software rendering, and Xvfb.
- Viewport: 1280 by 900 px.
- Worktree state: the application binary was built from the implementation source SHA; the UX-owned manifest, tester-owned regression harness, and changelog integration changes were present while the replacement artifacts were captured.
- Runtime setup: normal menu controls and public Lua player controls drove the documented match states; no application source or saved score data was altered to fabricate outcomes.

## Screenshot matrix

| ID | Screen ID | Wireframe | Route or workflow | Representative state and setup data | Viewport | Expected visible behavior | Destination path | Provenance and status |
|---|---|---|---|---|---:|---|---|---|
| <a id="ss-001"></a>`SS-001` | `MENU-01` | [Main menu](../screens/wireframes/menu-main.md) | Launch the local application to the setup menu after issue #38 implements the target Network entry. | Use four saved people, two selected local players, Deathmatch, persistent local score data, Assistance on, Quick Liquid on, Burnable Trees on, and Rounds `0`. | 1920x1080 | The scaled retro menu must show distinct `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` actions. Play remains local-only. | `docs/screenshots/MENU-01/network-entry-1920x1080.png` | `Planned` for downstream issue #38. No current screenshot is valid for this target wireframe. |
| <a id="ss-002"></a>`SS-002` | `MENU-02` | [Menu message](../screens/wireframes/menu-message.md) | After issue #38 implements the Network footer, select Clear or press F3 from the populated menu. | Use four saved people, two selected players, Deathmatch, persistent score data, and documented default settings; show `Really delete? (Y/N)`. | 1920x1080 | The unchanged scaled target menu behind the strip must visibly include equal-width `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` footer actions. | `docs/screenshots/MENU-02/network-entry-confirmation-1920x1080.png` | `Planned` for downstream issue #38. The existing three-action-footer capture is not valid for this target wireframe. |
| <a id="ss-003"></a>`SS-003` | `PLAY-01` | [Shared arena play](../screens/wireframes/play-fullscreen.md) | Start a local Deathmatch and press F2 once during live play. | Use 15 living players, ranking on, a finite round limit, and a state after the start fade. | 1280x900 | One undivided arena must show the whole level and all 15 players. F2 must not change the view. Live ranking, round progress, event messages, and player status must remain available. | `docs/screenshots/PLAY-01/shared-15-player-1280x900.png` | Replacement packet provenance above. Captured from an actual 15-player Deathmatch with ranking enabled and three rounds after pressing F2 once. `Conforms`. |
| <a id="ss-007"></a>`SS-007` | `MODE-01` | [Predator](../screens/wireframes/mode-predator.md) | Start Predator from the menu. | Use three living players, one predator, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show the faint predator body, visible predator weapon, opaque marines, live ranking, round progress, events, and status cues. | `docs/screenshots/MODE-01/shared-live-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |
| <a id="ss-008"></a>`SS-008` | `MODE-02` | [Team mode](../screens/wireframes/mode-team.md) | Start `Team deathmatch (2 teams, FF: off)` from the menu. | Use two living players per team, ranking on, and a finite round limit. | 1280x900 | One undivided arena must show all four players. Team apparel and named grouped ranking must remain visible with events, status cues, and round progress. | `docs/screenshots/MODE-02/shared-live-1280x900.png` | Replacement packet provenance above. Captured from an actual four-player `Team deathmatch (2 teams, FF: off)` with two players per team, grouped live ranking, and three rounds. `Conforms`. |
| <a id="ss-009"></a>`SS-009` | `PLAY-05` | [Sudden death](../screens/wireframes/play-sudden-death.md) | Start Deathmatch with Quick Liquid on and reach rising water. | Use two living players, ranking on, raised water in the safe arena, and one submerged player. | 1280x900 | One undivided arena must show both players, raised water, the air indicator, live ranking, status cues, events, and round progress. | `docs/screenshots/PLAY-05/shared-rising-water-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |
| <a id="ss-010"></a>`SS-010` | `OVER-01` | [Score tab](../screens/wireframes/overlay-score-tab.md) | Press Tab during a live Team deathmatch round. | Use four players in two teams with non-zero K, A, D, K/D, and PTS values. | 1280x900 | The centered score panel must show grouped team rows over one undivided live arena. The winner curtain must not appear. | `docs/screenshots/OVER-01/shared-score-tab-1280x900.png` | Replacement packet provenance above. Captured by pressing Tab during round 5 of an actual four-player Team deathmatch after normal runtime play produced grouped team rows and non-zero K, A, D, K/D, and PTS values; no winner curtain is present. `Conforms`. |
| <a id="ss-011"></a>`SS-011` | `OVER-02` | [Round over](../screens/wireframes/overlay-round-over.md) | Finish round 1 of a two-round Predator match. | Use one winning predator and three ranked players. | 1280x900 | The Predator outcome message, dark red curtain, round counter, and centered score panel must appear over one undivided arena. | `docs/screenshots/OVER-02/shared-round-over-1280x900.png` | Replacement packet provenance above. Captured after an actual predator won round 1 of a two-round, three-player Predator match; the runtime emitted `Predator won!` and opened the score panel and curtain. `Conforms`. |
| <a id="ss-012"></a>`SS-012` | `OVER-03` | [Game over](../screens/wireframes/overlay-game-over.md) | Finish the only round of a one-round Team deathmatch. | Use two teams, four ranked players, and one winning team. | 1280x900 | The Team outcome, final score panel, dark red curtain, and final round progress must appear over one undivided arena. The state must not add an exit label. | `docs/screenshots/OVER-03/shared-game-over-1280x900.png` | Replacement packet provenance above. Captured after Team Bravo won the only round of an actual four-player Team deathmatch; the final grouped score panel, dark red curtain, and `1 | 1` round progress are visible without an exit label. `Conforms`. |
| <a id="ss-013"></a>`SS-013` | `CONS-01` | [Console over menu](../screens/wireframes/console-menu.md) | After issue #38 implements the Network footer, open the console over the populated local menu. | Use the planned four-action footer, recent startup output, and an empty input line. | 1920x1080 | The console spans the client width while the visible scaled menu below includes equal-width `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)` actions. | `docs/screenshots/CONS-01/network-entry-open-1920x1080.png` | `Planned` for downstream issue #38. The existing three-action-footer capture is not valid for this target wireframe. |
| <a id="ss-014"></a>`SS-014` | `CONS-02` | [Console over play](../screens/wireframes/console-gameplay.md) | Open the console during active Deathmatch and enter or inspect any command that previously affected the gameplay view. | Use four living players, ranking on, a finite round limit, and recent game output. | 1280x900 | The console must overlay one undivided arena. The command must not create player-specific views. The simulation, ranking, round progress, events, and status state must remain present behind the console where visible. | `docs/screenshots/CONS-02/shared-open-1280x900.png` | Fresh shared-arena packet provenance above. `Conforms`. |
| <a id="ss-015"></a>`SS-015` | `NET-01` | [Network entry](../screens/wireframes/network-entry.md) | From `MENU-01`, activate `Network (F2)`. | Default target state with Host focused and direct same-machine/LAN scope visible. | 1920x1080 | The scaled retro canvas shows Host, Join, Back, supported platforms, direct endpoint scope, host-alone lobby 1–15, match 2–15, and no unsupported affordances. | `docs/screenshots/NET-01/default-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-016"></a>`SS-016` | `NET-02` | [Host setup](../screens/wireframes/network-host-setup.md) | Choose Host from `NET-01`. | Valid port and two configured host local players in editable host setup. | 1920x1080 | Host setup shows retained port/local players, capacity, Start session, Back, and validation. Pending variants expose the 10-second boundary and Cancel back to retained editable setup with no listener. | `docs/screenshots/NET-02/host-setup-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-017"></a>`SS-017` | `NET-03` | [Join connecting](../screens/wireframes/network-join.md) | Choose Join, enter `192.168.1.24:27015`, configure two local players, and activate Connect. | Connecting to `192.168.1.24:27015` with two local players and Cancel available. | 1920x1080 | Endpoint and local setup remain visible with truthful Connecting copy and the 10-second total boundary; Cancel retains setup and the screen does not claim admission. | `docs/screenshots/NET-03/connecting-two-local-players-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-018"></a>`SS-018` | `NET-04` | [Lobby](../screens/wireframes/network-lobby.md) | Host a session, admit two guests, and configure six total players. | Host waiting with 3 participants, 6 players, and one named unready guest. | 1920x1080 | Separate Role, Connection, and Readiness columns, ownership, settings, roster, disabled Start, and `Waiting for <participant> to be ready` are visible; Leave/End variants use confirmations. | `docs/screenshots/NET-04/host-waiting-one-unready-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-019"></a>`SS-019` | `NET-05` | [Network match](../screens/wireframes/network-match.md) | Start a LAN Deathmatch after all participants are ready. | Six living players across connected participants during active play. | 1280x900 | One undivided arena must preserve full level, six players, ranking, round progress, events, status, and compact truthful LAN session context. | `docs/screenshots/NET-05/six-player-lan-deathmatch-1280x900.png` | `Planned` for downstream issue #38; no playable network session exists. |
| <a id="ss-020"></a>`SS-020` | `NET-06` | [Final summary](../screens/wireframes/network-summary.md) | Complete a configured three-round network match. | Final authoritative three-round result for all connected participants. | 1280x900 | Summary shows totals, `Session only`, no persistence, applicable actions, retained/departed rows, clearing at new-match start, and discard on session end. | `docs/screenshots/NET-06/final-three-round-summary-1280x900.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-021"></a>`SS-021` | `NET-07` | [Guest reconnect](../screens/wireframes/network-reconnect.md) | Interrupt a guest connection during an active network match. | Reconnecting with 24 positive ceiling seconds remaining after host crash or another ambiguous transport failure. | 1280x900 | Endpoint, unchanged reservation deadline, no-input state, active simulation, retryable status, and `Leave session` consequence are visible; the state does not claim host end or player removal. | `docs/screenshots/NET-07/reconnecting-24s-1280x900.png` | `Planned` for downstream issue #38; reconnect is not implemented. |
| <a id="ss-022"></a>`SS-022` | `NET-08` | [Connection failure](../screens/wireframes/network-failure.md) | Attempt to join an unavailable direct endpoint. | Initial `Host unreachable.` with endpoint context, Retry, Edit setup, and Return to Network. | 1920x1080 | Initial admission uses fixed precedence; terminal reconnect disables Retry; expiry uses truthful copy; host-local service failure uses `Hosted session stopped unexpectedly.` only for the host. | `docs/screenshots/NET-08/host-unreachable-1920x1080.png` | `Planned` for downstream issue #38; no current network UI exists. |
| <a id="ss-023"></a>`SS-023` | `NET-09` | [Host-ended session overlay](../screens/wireframes/network-host-ended.md) | Receive a valid intentional host End session notice during an active network match. | Host-ended outcome over the last authoritative arena context. | 1280x900 | A blocking `HOST ENDED SESSION` overlay states no migration/resume or persistence and can arise only from an End notice accepted through the current established session. | `docs/screenshots/NET-09/host-ended-1280x900.png` | `Planned` for downstream issue #38; host-ended UI is not implemented. |

## Coverage

- Required wireframes: 20.
- Required representative screenshot entries: 20.
- Conforming screenshots: 8.
- Non-conforming screenshots: 0.
- Planned screenshot entries awaiting downstream issue #38: 12.
- Pending screenshot assessments: 0; planned entries cannot be assessed before implementation and capture.
- Screenshots per wireframe: exactly one.
- Retired screenshot matrix entries: `SS-004`, `SS-005`, and `SS-006`.
- Coverage status: eight unchanged implementation artifacts conform; the 12-entry issue #28 affected-wireframe matrix is planned and has no valid current artifacts.

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
