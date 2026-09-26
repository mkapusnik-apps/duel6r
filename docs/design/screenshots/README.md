# Screenshot coverage and assessment

## Network presentation current capture matrix

This section is the sole current capture matrix for the NET-01 through NET-10 presentation unification. It supersedes every earlier network conformance claim in this file and the [legacy manifest](../../screenshots/README.md) for these affected wireframes. The older packets below remain historical references with their original hashes. They do not prove the unified presentation. No local-play representative is invalidated unless implementation changes its presentation source.

**Current conformance: all 16 representatives conform for their supplied states; V-01 through V-05 are closed.** The [current assessment](#current-native-visual-assessment) combines five new or replacement representatives from source `a740254ee6ceaf5b55914abdf695e76f4766c1fd` with eleven unchanged accepted representatives from `e5f5517a56c426104f4cfb73c19058844db0d874`. Their original capture lineage is retained. The approved [shared visual baseline](../../design.md#unified-network-presentation), [screen index](../README.md#screen-specifications), and linked wireframes remain fixed. Existing functional contracts remain unchanged. NET-02-P, NET-05-S, and NET-05-R cover existing materially different layouts, not new functional states. All screen and wireframe identities remain unchanged.

### Fixed criteria and scoped correction

The approved scope correction fixes three presentation criteria: NET-05-S must not advertise unsupported scrolling; NET-04-R must identify host reordering without adding an always-clickable control; and baseline disabled-but-focused controls must retain visible focus without traversal changes. These criteria and the representative profiles below are now fixed.

Only prior criteria or evidence that require enabled NET-05-S scroll arrows, a permanently actionable NET-04-R reorder control, or hidden focus on a baseline-focused disabled control are invalidated by this correction. Such evidence cannot establish those corrected presentation details. Exact historical capture-profile mandates are superseded by the profiles below; otherwise conforming native evidence is not invalidated merely because its incidental player count, round count, or countdown differs. The 16 representatives were already Pending; this correction adds no entries and does not reset unrelated checks. Working navigation in NET-04-R, NET-06, and NET-05-R remains required. Unchanged world, gameplay, and unrelated interaction evidence retains its original scope and provenance.

### Presentation and reproduction profile

- All entries use the actual native desktop client, standard input profile, existing font and default network visuals, and complete client area without external chrome.
- Menu entries use the existing scaled 850 by 700 logical canvas inside a 1280 by 900 client.
- Arena entries use a 1280 by 900 client except NET-05-C, which uses 1280 by 720.
- Preferred reproducible environment is Release GL4, Lua ON, Linux Docker, software OpenGL, Xvfb, and dummy audio, with actual environment versions recorded.
- One representative is required per wireframe; focused edge-state checks below do not add permanent matrix rows.
- Existing legacy artifact destinations remain unchanged because no migration is authorized.
- Each new wireframe uses `docs/design/screenshots/<screen-id>/<wireframe-id>.png`.
- A destination is a planned replacement path, not evidence that the artifact has been captured.
- Legacy filenames remain unchanged even when their historical player count, round count, degraded state, or countdown differs from the replacement's actual state.
- The manifest and evidence packet must describe the actual captured state rather than infer it from a filename or an older diagram example.

### Matrix

This matrix specifies reproduction targets; the current assessment below owns each row's conformance and actual artifact hash. Paths are repository-relative. A stated test endpoint is an input for an authorized owned session, not a public service or a claim of reachability.

| Entry / screen / wireframe | Native route and reproduction inputs | Required captured state and visible expectation | Client viewport | Artifact destination |
|---|---|---|---|---|
| SS-015 / NET-01 / [NET-01](../wireframes/NET-01/NET-01.svg) | Start client → MENU-01 → Network (F2). | Initial Host focus; four framed actions, scope text, title strip, banner/version. | 1280 × 900 | `docs/design/screenshots/NET-01/NET-01.png` |
| SS-016 / NET-02 / [NET-02](../wireframes/NET-02/NET-02.svg) | Network → Host; select an assigned private interface; Port 26660; select two valid local persons/controls; enter a disposable optional password. | NET-02-password; Password focused and masked; full selected IPv4; framed endpoint and setup regions; valid Start and Back visible. | 1280 × 900 | `docs/design/screenshots/NET-02/NET-02.png` |
| NET-02-P / NET-02 / [NET-02-P](../wireframes/NET-02/NET-02-P.svg) | From valid NET-02, activate Start session; capture any actual pending-startup frame before confirmed readiness, including an unaltered full-client frame selected from native recording. | Starting session and timing help; Cancel focused; no editable setup, second Start, or success claim. No minimum display duration is required for the image. | 1280 × 900 | `docs/design/screenshots/NET-02/NET-02-P.png` |
| NET-03-E / NET-03 / [NET-03-E](../wireframes/NET-03/NET-03-E.svg) | Network → Browse sessions → select an actual protected open first-round host → Join selected; retain one valid local person/control. | NET-03-password; required-password focus, masked input when entered, endpoint, first-round consequence, Connect and Back. | 1280 × 900 | `docs/design/screenshots/NET-03/NET-03-E.png` |
| SS-017 / NET-03 / [NET-03](../../screens/wireframes/network-join.md) | Network → Direct connect; Address 127.0.0.1, Port 26660, two local persons/controls → Connect to an authorized owned listener that accepts transport but withholds a complete admission result. | Actual Connecting before its deadline; retained endpoint; two locked slot/person/control rows; truthful status and focused Cancel. | 1280 × 900 | `docs/design/screenshots/NET-03/NET-03.png` |
| SS-018 / NET-04 / [NET-04](../../screens/wireframes/network-lobby.md) | Host plus guest, one slot each; valid Deathmatch/two-round setup; leave the guest unready; make only the authorized test directory unavailable while transport remains active. | Host lobby; two players/two participants; publication failure and Retry publication; separate participant columns; owned and remote values; disabled Start reason; framed settings/actions. | 1280 × 900 | `docs/design/screenshots/NET-04/NET-04.png` |
| SS-025 / NET-04 / [NET-04-R](../wireframes/NET-04/NET-04-R.svg) | Complete the NET-06 representative's actual two-round match; host Return to lobby in the same session; retain the result and confirmed directory state; keep focus outside Reorder. | Completed retained result; current membership distinct from history; fixed outcome labels; working result navigation and position feedback; readiness/footer; plain host reorder capability and access-path indication without an always-clickable target. | 1280 × 900 | `docs/design/screenshots/NET-04/NET-04-R.png` |
| SS-019 / NET-05 / [NET-05](../../screens/wireframes/network-match.md) | Host plus guest, one slot each; actual Deathmatch on any supported shipped level; capture guest during active Connected play. The same two-round match may supply NET-05-S/R and NET-06. | Two-player complete shared arena; truthful Connected status; unchanged gameplay HUD; framed Leave session action. No mandatory Invisibility, degraded state, level, or mirror value. Record the actual state despite the historical filename. | 1280 × 900 | `docs/screenshots/NET-05/six-player-lan-degraded-1280x900.png` |
| SS-026 / NET-05 / [NET-05-C](../wireframes/NET-05/NET-05-C.svg) | In actual guest active play, open Leave session with Escape; capture before acceptance; cancel after capture. | Bounded confirmation over arena; full consequence; Leave session focused before Cancel; both framed controls visible. | 1280 × 720 | `docs/design/screenshots/NET-05/NET-05-C.png` |
| NET-05-S / NET-05 / [NET-05-S](../wireframes/NET-05/NET-05-S.svg) | Host plus guest, one slot each, in actual Deathmatch; before the first round has a winner, press Tab once. The common two-round setup may be reused. | Real pre-winner overlay; Session only/In progress and actual available values; fixed labels/body; optional truthful non-actionable position text; no enabled scroll-arrow affordance, new shortcut, focus target, or fabricated completed row. | 1280 × 900 | `docs/design/screenshots/NET-05/NET-05-S.png` |
| NET-05-R / NET-05 / [NET-05-R](../wireframes/NET-05/NET-05-R.svg) | Two-player, two-round Deathmatch; finish round one by normal play; capture host after the first active second and before automatic advance, without activating Advance round. | Frozen non-final result; Rounds 1\|2, actual outcome, any positive next-round countdown, ranking with working navigation, framed Advance round and End session. | 1280 × 900 | `docs/design/screenshots/NET-05/NET-05-R.png` |
| SS-020 / NET-06 / [NET-06](../../screens/wireframes/network-summary.md) | Host plus guest, one slot each; complete both Deathmatch rounds through actual gameplay; capture host before Return to lobby. | Genuine Completed summary in existing menu context; Session only and no-persistence copy; separate match/last-round outcomes; actual rows; working result navigation, fixed position feedback, and host actions. Record two completed rounds despite the historical filename. | 1280 × 900 | `docs/screenshots/NET-06/final-three-round-summary-1280x900.png` |
| SS-021 / NET-07 / [NET-07](../../screens/wireframes/network-reconnect.md) | Interrupt an authorized owned guest connection during active play; capture any actual positive two-digit seconds-remaining value, such as 23; restore before expiry after capture. | Actual reconnect; Last confirmed state context; countdown/reservation/continuation text; grey framed panel and focused Leave session. Record the exact displayed value rather than claim 24 from the historical filename. | 1280 × 900 | `docs/screenshots/NET-07/reconnecting-24s-1280x900.png` |
| SS-022 / NET-08 / [NET-08](../../screens/wireframes/network-failure.md) | Browse an actual protected host → Join selected → enter an incorrect disposable password → Connect. | NET-08-password-rejected; exact non-disclosing failure; Edit setup focused; actual Retry eligibility and reason; browser return control; no password echo. | 1280 × 900 | `docs/design/screenshots/NET-08/NET-08.png` |
| SS-023 / NET-09 / [NET-09](../../screens/wireframes/network-host-ended.md) | During a real match, host opens and intentionally confirms End session; capture the receiving guest. | Accepted intentional host-end notice over retained arena; fixed terminal/no-resume/no-persistence copy; focused Return to Network. | 1280 × 900 | `docs/screenshots/NET-09/host-ended-1280x900.png` |
| NET-10 / NET-10 / [NET-10](../wireframes/NET-10/NET-10.svg) | Network → Browse sessions through the authorized isolated backend; use at least two actual owned listings, one protected joinable session and one admission-closed session; select the protected row. | Populated NET-10-results; all independent columns; white inset table, grey headings, blue selected row plus non-color focus/selection; details; actual current page without invented total; all footer buttons. Loading/unavailable alone cannot substitute. | 1280 × 900 | `docs/design/screenshots/NET-10/NET-10.png` |

The default style sequence uses two participants with one player each and a two-round Deathmatch. A completed final result and a non-final round result remain distinct required states. Historical six-player, Invisibility, three-round, exact-24-second, and simultaneous multi-listing combinations are not prerequisites for these new representatives. Names, valid endpoint literals, and menu backgrounds need not match historical images; the packet must record actual inputs. Browser-origin setup and rejection must still use real browser workflows, not relabeled direct attempts.

NET-06 and NET-04-R use ordinary completed results for default style coverage; fourteen-winner Predator, departed identities, and maximum-result checks remain required below. NET-05-S must show whatever data the actual pre-winner state provides, including an empty body when applicable. Capture must not manufacture populated rows to resemble a diagram. The NET-10 diagram illustrates additional supported variants but does not require all of them in one image. Legacy diagram numbers are examples, not a reason to mislabel the approved current capture.

### Review and capture preflight

1. Team and developer must review implementation against the owning `UX-NET-*` requirements before capture.
2. Developer must check the proposed source checkpoint for consistent shared controls, clipping, focus bounds, and role-specific actions.
3. Developer must verify each planned route reaches the required state through the native application.
4. Developer must report an unreachable state before attempting a substitute or changing the capture setup.
5. Developer must capture only after review and reachability preflight pass.

NET-02-P must use actual host startup. An unaltered full-client frame from native capture recording is acceptable; the packet must identify the recording and frame time. A short capture window is not permission to inject a runtime state, add a production delay, freeze the application into a substitute state, or show a mock screen. If the pending state cannot be captured, report the limitation before considering a team-approved controlled startup boundary. NET-03 permits the existing owned pending-listener setup because the actual client is genuinely awaiting admission. It is not successful-admission evidence. No network infrastructure change is implied.

NET-10 defaults to actual owned sessions. If mixed real-host states cannot be sustained, developer must request explicit team approval for disclosed native-rendered directory metadata fixtures. The earlier packet's fixture approval does not automatically authorize this capture. A permitted metadata fixture proves layout only, not live reachability, host lifecycle, or admission timing. No fabricated UI, retouched screenshot, composed pixels, or fallback image is acceptable.

### Focused edge-state checks

These checks need a source-tied developer/tester report. Use supplemental native images only when required to demonstrate a disputed visual state. Keep temporary supplements outside the repository unless their durable retention is separately approved.

| Surface | Required focused checks without new representative rows |
|---|---|
| Shared desktop presentation | All changed menu layouts at 850 × 700 and 1280 × 720; 1920 × 1080 scale-cap/pointer alignment; fields and buttons focused, unfocused, pressed, disabled, and read-only; no font reduction or clipped focus outlines. |
| Input preservation | Keyboard/controller traversal, reverse traversal, pointer bounds, existing press-versus-release activation, repeat behavior, baseline disabled-focus behavior, visible disabled-but-focused state without activation, focus retention, and opening-input protection. |
| NET-01–03 | Empty available-person list; maximum supported slots; long names/control labels; valid and invalid endpoint; selector open/scroll/Back; stale interface; direct versus browser password focus; Starting/Cancelling; Cancel retention. |
| NET-04 / NET-04-R | Host/guest permissions; 15 participants/players; Team settings shown versus absent; readiness clearing; publication retry; completed/interrupted result; departed identities; full 64 UTF-8-byte names; working vertical/horizontal result navigation; host reorder indication before focus and baseline action/target after focus; no new clickable help region. |
| NET-05 / NET-05-C/S/R | Normal/degraded/resynchronizing status; all three modes; first-second active versus frozen phase; guest absence of Advance; long outcome and maximum ranking; preserved NET-05-R navigation; Tab toggling; NET-05-S has no enabled scroll arrows, new focus targets, shortcuts, or handlers; no hidden action activation; confirmation over arena/lobby/summary/reconnect; first-round arrival. |
| NET-06 | Host/guest footer; 14 Predator winners including departed winners; complete outcome identities; all result sections and scroll extremes; non-winning rank leader receives no champion treatment. |
| NET-07 / NET-09 | Lobby and summary retained backgrounds; long reconnect endpoint; positive countdown; Leave/Cancel; host end with and without match activity; no unexpected-failure substitute for intentional end. |
| NET-08 | Enabled Retry; cleanup-disabled Retry; invalid setup; restart-required result; port conflict; expired reconnect; closed admission; longest canonical reason; browser versus direct recovery destinations. |
| NET-10 | Loading, results, empty, stale, unavailable; open lobby, open first round, full, closed, and protected selection; disabled reasons; both page directions and current page; removed selection; refresh focus retention; long endpoints; independent Direct connect. |

The new appearance must not alter behavior to make a check pass. Team routes any functional discrepancy to product and behavioral QA. Static images do not prove network timing, input behavior, authoritative ownership, or gameplay continuity.

The simpler NET-05 image is Connected-state styling evidence only. Degraded and resynchronizing containment remain focused requirements and need not coincide with Invisibility. If status allocation, adjacent action bounds, or consuming shared rendering changes, developer must supply fresh targeted evidence for the affected status layout. Unchanged world rendering, Invisibility, orientation, and gameplay may retain prior scoped evidence with source/QA confirmation of unchanged behavior; the new image does not re-prove those properties. No supported player count, result extreme, or functional state is removed by the representative simplifications.

### Evidence and assessment

For every representative or supplied supplement, developer must provide source checkpoint, branch, capture state, exact native workflow and relevant inputs, supported viewport/profile, environment, artifact path, and SHA-256. Identify uncommitted presentation-source differences. Menu evidence must include the selected background filename, runtime asset manifest identity, and application-session identifier. Arena evidence must record actual level/mirror state, session/match/round identity and nearby accepted tick where available. Use disposable credentials and do not include secret values or unredacted logs.

UX assesses structure against the owning wireframe, component appearance against `docs/design.md`, and interaction presentation against the owning UX section. Wireframes are low fidelity; pixel matching is not required. Each row remains Pending until supplied evidence is inspected. Exact-hash integration without presentation-source or assessed-content changes does not require another assessment. A presentation correction invalidates only its affected consumers; a changed shared presentation primitive requires reassessment of every affected consumer. Unrelated local evidence is retained unless its presentation changes.

### Current native visual assessment

Current source-reviewed checkpoint: `a740254ee6ceaf5b55914abdf695e76f4766c1fd`, branch `feature/unify-network-menu-design`. UX inspected the five new/replacement canonical PNGs and all ten focused supplements in `/tmp/opencode/pr100-a740254-capture/supplements/`. The eleven earlier accepted rows retain their original `e5f5517a56c426104f4cfb73c19058844db0d874` hashes, captures, and assessment; developer reports hash verification and reviewer maps their presentation as unaffected. They are not relabeled as a740254 captures or a new uninterrupted session. No implementation, product requirement, wireframe, or image was changed by this assessment.

Evidence packets contain `README.md` and `manifest.json` at these paths:

- Current focused packet: `/tmp/opencode/pr100-a740254-capture/`; supplied manifest SHA-256 `99f03d3c5ddd8865725b900571343df741695671dfb0902652f45259b235ac16`.
- Original packet: `/tmp/opencode/pr100-e5f5517-capture/`; supplied manifest SHA-256 `72cc41f6450c5a33ad5721384cd59aad56ef527965cd366ed38b971e1e718004`.

All hashes in this assessment are developer-supplied; UX has no hashing tool and did not independently recompute them. The manifests record per-image application sessions, capture times, viewport, workflow, source freeze, runtime/asset identity, and menu-background diagnostics. Developer must preserve the packets or equivalent provenance handoffs through integration and cleanup. The original rejected NET-02, NET-03-E, NET-04, and NET-05-R hashes remain in the original packet; their canonical destinations now contain the accepted replacements below.

The new client SHA-256 is `4180e9f6d2e14eb23cb424b4fe2ed454773f1273e1bb6875a0aabca07b4272cd`; server SHA-256 is `8e44c16f00991974e40c4603000de0210b283f186ed9ee5ed814d59aca3e7008`; runtime manifest SHA-256 is `3e40bb93c01eae24a01978a7b579c9d9494afd4b8c54079c22ff865f1c60c054`. The eleven reused rows retain their original runtime and asset identities rather than these new values.

Both packets use actual Release GL4, Lua ON, Ubuntu 24.04 Docker, SDL 2.30.0, Mesa llvmpipe LLVM 20.1.2, OpenGL 4.5 Core Mesa 25.2.8, dummy audio, and Xvfb. Representatives are complete 1280 × 900 client areas except the reused NET-05-C at 1280 × 720. New NET-02 and NET-04 floor supplements are 850 × 700. The packets report no crop, retouch, composition, or source/runtime mutation during capture. Directory listings and completed results are real, not synthetic metadata or injected outcomes. Startup uses the explicitly authorized real-child-only boundary described below.

#### Representative results

Artifact paths are exactly the canonical destinations in the matrix. `Conforms` is limited to the supplied state, viewport, and this styling scope; it is not certification of uncaptured functional or platform variants. Source key **a740254** applies only to NET-02, NET-02-P, NET-03-E, NET-04, and NET-05-R. All other rows retain source **e5f5517** and their original assessment.

| Wireframe / entry | Supplied SHA-256 | Result and observed presentation |
|---|---|---|
| NET-01 / SS-015 | `cc928a6b53b8af656bf3d1c283d88932c5a9529ab09ad1e552717328a6d26667` | Conforms. Four recognizable beveled actions, Host focus, scope copy, banner/version, and title strip are contained. |
| NET-02 / SS-016 | `ad6e6c5a875b2da3a128ac55da7d626a83790110b1cbc4c45179854524354b67` | Conforms / a740254. Corrected endpoint separation, complete private address, masked focused password, two owned slots, setup panels, and actions remain contained. |
| NET-02-P | `dd3f0527910b2e9ef1063418a2afc184cb7a82be7f91c9fafc0749b6124a1d92` | Conforms / a740254. Genuine Starting session status, ten-second help, and sole focused Cancel are visible without editable setup or a readiness claim. |
| NET-03-E | `f6cd05be569c20840d8fa856943bd206aa3842982446b6f5894971c9f1c83b1d` | Conforms / a740254. Password required is outside the editable field; corrected gaps, masked focus, actual protected first-round context, owned Elm slot, Connect, and Back remain readable. |
| NET-03 / SS-017 | `26e92e082eef97d94a79dfbec8e9033212137b5fabc64db65c7e470173d949cb` | Conforms. Retained direct endpoint, two locked rows, connecting/deadline text, and focused Cancel are separate and contained. This is pending transport/admission, not successful admission. |
| NET-04 / SS-018 | `4ad94bfe4ff24316006d70a9357309b9a4d8187f30f574fa5837fb17fa4ff24b` | Conforms / a740254. Complete inset Order heading and corrected roster/Ready separation; Maple ready/Oak unready, publication failure, ownership, settings, and disabled Start reason remain clear. |
| NET-04-R / SS-025 | `089ef802a1dbbc0c807bcd8ce9264fe4b07017ca098f9b4dfe682ff18c3c9e55` | Conforms. Plain host reorder access help is visible with person focus outside Reorder; completed outcome history, current membership, settings, result navigation, readiness, and footer remain separate. |
| NET-05 / SS-019 | `6dc432ef9dd4663d7d098f493ebae2bc9f7ab79f265382d8eee78196a9393c12` | Conforms for two-player Connected plus actual degraded indication. Complete shared arena, framed Leave action, and both status groups remain visible. This is not historical six-player or Invisibility evidence. |
| NET-05-C / SS-026 | `ddf95f1a5d608d843c6d77f667c1c8801eecdf24c74f1ecdd5821dd53b5b61eb` | Conforms at 1280 × 720. Centered bounded panel, full wrapped consequence, initial Leave focus, separate Cancel, and retained arena/status remain readable. |
| NET-05-S | `3cef766f269ef447a8512fe58cadf3a2ef04973aefe2b28507f900f40ad0beb8` | Conforms. Genuine In progress/Pending state, empty white body, fixed labels, and plain zero-row/column information; no enabled scroll arrows or shortcut promise. |
| NET-05-R | `4ac2ee8dd3ff0f1dcee7e395ac44c51601d41180121a80c9bdf99e00908ac250` | Conforms / a740254. Actual first completed frozen result shows Rounds 1\|2, Maple winner, and next round in 4s. Progress, score, ranking, outcome, phase, status, and host actions remain separate. |
| NET-06 / SS-020 | `3906d240091f9497b2339540b2037068f6e9ca0439f79b22cdc90a8f89f27be5` | Conforms for actual two-round completion. Session-only/no-persistence labels, separate outcomes, three outcome rows, settings, round history, scroll feedback, and focused Return action are contained. |
| NET-07 / SS-021 | `7a5640f8e10bf79b7b3e5c83a2448f236dea7c90caec6734530243fde3abd3e2` | Conforms. Actual 26 seconds remaining, reservation and continuation text, Last confirmed state, and focused Leave action; filename does not imply 24 seconds. |
| NET-08 / SS-022 | `fefcc954c6b03ccb3960fd3d7160a1c3d05d5cde45ccb039d0a13c453918811c` | Conforms for browser-origin password rejection. Exact non-disclosing reason, endpoint/instructions, three framed recovery actions, and Edit setup focus are clear; no secret shown. |
| NET-09 / SS-023 | `b6cd0c0091a0e2c9a32d884cbebac7e7e9bc0100838131d9847288d53bd792b6` | Conforms. Intentional host-end panel retains arena context and shows terminal/no-resume/no-persistence copy with focused Return to Network. |
| NET-10 | `44b69e55b8952c481c82d1a57b63ebc67447c873d988016d5995929ccff4d57b` | Conforms for actual populated results. Protected open row and closed row, independent columns, blue selection plus non-color marker/focus, selected details, Page 1, disabled paging, and two-row footer are contained. |

#### Closed findings

All five findings are closed against the fixed requirements. No requirement or acceptance threshold was weakened. Source areas refer to the localized correction, not an instruction to make further changes.

| Finding | Affected row / source area | Closure evidence |
|---|---|---|
| V-01 / CAP-01 | NET-04; `source/NetworkMenu.cpp`, `drawLobby`; UX-NET-019, UX-NET-AC-03, UX-NET-04-003 | Closed. The replacement contains the complete Order heading with right clearance. The 850 × 700 guest-lobby supplement confirms heading containment at the floor. |
| V-02 / CAP-02 | NET-05-R; `drawRoundSummary`; UX-NET-05-010 and fixed matrix state | Closed. Product approved the localized counter-presentation correction; no scope decision remains pending. The genuine first frozen result now shows 1\|2. Same-recording supplements show the first active delay at 1\|2, subsequent active Round 2/2, and later frozen 2\|2; normal automatic advancement is reported without Advance input. |
| V-03 / CAP-03 | NET-02-P; UX-NET-02-010–011, UX-NET-AC-08 | Closed. The native Starting representative uses the team-authorized external real-child-only boundary with unmodified GUI/deadline. Ready and Cancel preflight observations accompany it. |
| V-04 | NET-03-E; editable setup password rendering; UX-NET-03-003 | Closed. Password required is plain help outside the white value region; mask, focus, first-round context, and actions remain contained. |
| V-05 | NET-02, NET-03-E, normal NET-04; endpoint geometry and roster/Ready allocation; UX-NET-017 | Closed. The source-reviewed 8-logical-pixel geometry is visible in the replacements and 850 × 700 supplements. Disabled focus remains visible; supplied pointer-strip/gap and blocked-activation observations support the unchanged input behavior. |

#### Authorized startup evidence

The accepted NET-02-P frame was captured while the exact owned server child was stopped before any listener/readiness. The supplied `barrier-capture.json` records child PID 311 under GUI PID 29, stable process start identity, matching server SHA-256, no listener while stopped, and GUI running/sleeping rather than stopped. The measured hold was 0.908619 seconds. Capture from 14:29:54.931196 to 14:29:55.306286 UTC completed before release at 14:29:55.416675 UTC on 2026-09-26. The same server PID/executable subsequently became listening, as reported by developer and team.

The external pidfd SIGSTOP/SIGCONT mechanism was explicitly authorized by team. A separate guard bounded release; readiness and Cancel preflights were reported successful. Neither the GUI nor its ten-second deadline was paused or altered. No preload, replacement server, status injection, memory write, or production instrumentation was used. UX read the capture report and inspected the native Starting, readiness-lobby, and returned-setup images; UX did not execute the helper or independently rerun lifecycle QA. This is genuine pending-state presentation evidence under a disclosed controlled environment, not a claim about ordinary startup duration.

#### New focused supplements

All paths below are relative to `/tmp/opencode/pr100-a740254-capture/supplements/`. All use a740254 and the Linux software-GL environment above. Each image was inspected. These supplements add no stable matrix entries and do not replace the eleven retained e5f5517 representatives.

| Inspected artifact | Viewport | Supplied SHA-256 | Assessment and limit |
|---|---|---|---|
| `barrier-readiness-lobby.png` | 1280 × 900 | `143454f80ae030f18200e1308e1df0b1f6e79607d36bb2b4b304d37216c89f26` | Actual established lobby after the readiness preflight; heading and Ready separation are contained. Process continuity comes from the supplied reports, not pixels alone. |
| `barrier-cancel-setup.png` | 1280 × 900 | `2b36ff1a2938753d8c783b67edbd035388b5875fd18f3a53fe155155b3eff615` | Returned editable setup retains two local slots, private interface, and Port focus with corrected spacing. Child reaping is developer evidence. |
| `NET-04-floor-guest.png` | 850 × 700 | `5d364db0b20eb4adb8252e422c6eda95e21a7117343dd86bc76407f8cabcfe94` | Complete Order heading, roster/Ready separation, owned field focus, read-only remote settings, and Leave remain contained. |
| `NET-04-disabled-focus.png` | 1280 × 900 | `bbc229e3f2c848ad504d1f8d00fad4a0a205e0092aac35c7ee02a38d96ad1b5a` | Disabled Start retains its focus keyline and unready reason with corrected normal-lobby geometry. |
| `round1-active-delay.png` | 1280 × 900 | `00775e34a467d5d604bcc205b4f3a87b918c0221c2c5a3243cd72c76b0514320` | Maple outcome, 1\|2 progress, and World active 1s are separate and readable before the frozen phase. |
| `normal-advance-round2.png` | 1280 × 900 | `c5579ce6f178caa500849f4125415da1dea45cd1a8d46abcedeedf4725ad3fc1` | Active Round 2/2 returns to the normal arena progress region; supplied recording lineage reports automatic advance without an Advance input. |
| `round2-frozen.png` | 1280 × 900 | `580daf5a90dbecf4704633fb4e1f2cbc1c276bc1f3ef0f7830c3a49a1cda3392` | Later progress correctly shows 2\|2. This is counter-bound evidence, not a new assessment of unchanged final-delay action/copy semantics. |
| `completed-match-proof.png` | 1280 × 900 | `61864e2edb747c145dd9ea0d0c7dd25aea6a7326a3564acba0900e0853dcaf49` | Actual Completed two-round result identifies Maple as winner on normal Duel 16 and mirrored Duel 28; supports new NET-05-R provenance, not replacement of the retained NET-06 image. |
| `NET-02-floor-disabled.png` | 850 × 700 | `32dc506d47f00c90e91f2c2d6672c53e37d11871d1abb1489b1284e59724e472` | Correct endpoint gaps, empty lists, persistent no-player reason, disabled focused Start, and Back fit without overlap. |
| `protected-first-round.png` | 1280 × 900 | `739126b2d5f94d6ddfcc8868f8b7e5ef45e0cf78fa4068b1e419a1ce3ca38e50` | Actual active first-round host context supports the new browser-origin NET-03-E state; no new admission-timing or world-parity claim. |

#### Earlier supplemental observations

All paths below are relative to `/tmp/opencode/pr100-e5f5517-capture/supplements/`. All retain source e5f5517; viewport is 1280 × 900 except the stated floor image. None creates another stable matrix entry. Their observations below describe the original bytes: V-01/V-05 geometry failures and missing-startup references are historical and resolved by the a740254 evidence above. Unchanged result-scroll, reorder, and arena observations retain their earlier limited scope.

| Inspected artifact | Supplied SHA-256 | Assessment and limit |
|---|---|---|
| `host-a-lobby-inspect.png` | `b25d77c6cc4202a957ed468855008aa593d4419f123fae2fea6658cafaca7a12` | Confirms V-01 and V-05 in a host-alone normal lobby; owned-field focus and disabled Start reason remain visible. |
| `NET-06-scroll-edge.png` | `18e626f9e65149f7f146ade63b01f8839caf42f792658146e25dd2ce91918c0a` | Conforms for supplied scroll state: columns 10–103/103 and rows 7–19/19 remain contained; fixed identity and actions remain visible. Leading clipped text is the intentional horizontal offset, not a new clipping failure. Does not establish every content extreme. |
| `NET-04-R-reorder-focused.png` | `b645e6d7c40625642d5b3d1acb1262f7eed39eb911ed7265653e7e9171d8fc51` | Conforms for focused Reorder presentation, identifying position 1 and Ivy while results/actions remain contained. Together with NET-04-R it shows help before focus and the baseline action after focus; it does not independently prove absence of an invisible pointer target. |
| `NET-04-disabled-focus.png` | `56003e7ae3306546e4bfee439b9b3279bb77916c1eade9e9939a6c1c7225ee50` | Disabled Start visibly retains focus and readable reason. This corrected focus detail conforms; normal-lobby V-01/V-05 remain visible. Reported blocked activation belongs to developer/behavioral evidence. |
| `guest-a-restored.png` | `668424d063223770f06467b3c7a3d91c09dfe0a844a8a61e286f57c725e10e1b` | Connected plus degraded text, arena, and framed Leave action remain contained after reported restoration. A still does not prove deadline or temporal recovery behavior. |
| `arena-a-inspect.png` | `8c3b69f9575e68b12c8b0199806779063b9ef6db1db559146ae95a4e018ba73d` | Player indicators, full arena, degraded status, and Leave action remain visibly separate. Not new numerical opacity or deterministic-background evidence. |
| `NET-02-floor-disabled.png` | `6d057a69b3e731a0d4de2a5ac0153d2bb021e5d0ba8105d111bfa1310326cecd` | At 850 × 700, empty lists, no-player reason, disabled focused Start, and Back remain contained. Focus detail conforms; V-05 endpoint spacing remains nonconforming. |
| `startup-after-final-attempt.png` | `b7c6530182fdef00f7280545b514e42d1e72c57e0fcd9f4adfc8e9502685a6a4` | Shows the established lobby, not Starting; cannot satisfy NET-02-P. Also reproduces the normal-lobby heading/spacing findings. |

#### Provenance and acceptance limits

Source, capture state, viewport/profile, environment, exact paths, and supplied image hashes are sufficient to assess the visible styling of these supplied states. Source approval and frozen-runtime integrity are team/developer assertions; UX did not rerun those checks. The actual 26-second reconnect, two-round results, and Connected plus degraded frame are recorded truthfully rather than inferred from legacy filenames.

Missing canonical match/round IDs, near-frame ticks, Session A level/mirror identity, and arena-background filename prevent new claims about authoritative timing, deterministic background selection, orientation parity, or uninterrupted world continuity. They do not prevent assessment of the visible panel/control styling. Menu-background diagnostics are not evidence of an arena background. Session B level/mirror and outcomes are visible in the supplied final result; no unsupported identity or tick is inferred.

The earlier eight supplements and the ten new focused supplements were inspected in their respective assessments. Recording archives were reported but were not unpacked or played by UX; the supplied extracted frames and their per-frame metadata were inspected. V-02 is now a product-approved localized correction with accepted replacement evidence, not a pending scope decision. The new first-round counter and subsequent visible states agree with the supplied normal-progression report. Independent QA and source approval remain supporting team evidence, not tests executed by UX.

The accepted NET-05 legacy filename still contains a two-player Connected/degraded frame, NET-06 still contains the original two-round Ivy/Juniper completion, and NET-07 still shows 26 seconds. No file was renamed or relabeled to imply a different actual state. Missing physical-controller, maximum-content, and other viewport/platform exercises are not declared passed by these images. Linux software-GL evidence does not establish distinct-machine Windows/Linux LAN or physical-GPU coverage. These are unchanged scope limits, not new capture mandates or unresolved V-01–V-05 blockers.

No new requirement or capture-profile change is introduced. All 16 stable representatives now conform for their supplied states, with five a740254 captures and eleven unchanged e5f5517 captures. No additional visual recapture is requested. Exact-hash integration without further presentation-source, artifact, or assessed UX-content changes needs no second assessment. Reassess only changed presentation consumers or changed evidence.

Visual gate: satisfied

## Host-directory current capture matrix

Historical for the network presentation unification above. All conformance and gate statements in the following prior packets apply only to their original presentation baseline.

This section owns changed coverage for the host-directory extension. It supersedes the legacy representatives only for the affected wireframes below. Unaffected entries remain governed by the [legacy manifest](../../screenshots/README.md). The historical integration index below is not evidence for the extension. Existing artifacts must not be relabeled as new acceptance.

All eight representatives conform for their supplied states at 1280 × 900. The replacement NET-10 and its focused paging supplements resolve the browser visual finding. The other seven retain their previous assessments and original capture lineage. Developer owns capture and image files. Product reserves NET-10 and its states in the [browser contract](../../screens/network-browser.md). Routes are native menu workflows, not URLs. This assessment does not certify uncaptured variants or cross-platform behavior.

Default presentation: existing retro desktop canvas, normal input profile, complete client area, no accessibility-mode claims. Menu representatives use 1280 × 900; retained-result NET-04-R uses the same viewport. Each destination below is relative to `docs/design/screenshots/`. Replace the representative at the same destination on later capture. Do not add a dated screenshot series. Existing representative IDs are retained where available; new rows use wireframe IDs as unambiguous keys until product integrates inventory numbering.

| Entry / wireframe | Route and reproducible setup | Representative state and expectation | Destination |
|---|---|---|---|
| SS-015 / NET-01 | MENU-01 → Network | Host focused; Browse sessions and Direct connect visible; Back available | `NET-01/NET-01.png` |
| SS-016 / NET-02 | Network → Host; choose an assigned private LAN interface; enter a disposable test password; retain valid local player | NET-02-password; masked password and full selected address; no registration success claim | `NET-02/NET-02.png` |
| NET-03-E | Network → Browse sessions → select protected open round-one host → Join selected | NET-03-password; Password focused; one local player; endpoint, first-round consequence, Connect and Back visible | `NET-03/NET-03-E.png` |
| SS-017 / NET-03 | From NET-03-E start a real controlled pending admission attempt | Locked local slots; masked or absent password; truthful Connecting and Cancel; no premature lobby claim | `NET-03/NET-03.png` |
| SS-018 / NET-04 | Network → Host → confirmed running lobby; make the authorized test directory unavailable without stopping host transport | Host directory warning and Retry publication; direct endpoint and existing lobby actions remain usable; no modal | `NET-04/NET-04.png` |
| SS-025 / NET-04-R | Complete a real match → return to lobby; keep retained result and confirmed directory state | Listing feedback fits without covering retained outcome rows or actions | `NET-04/NET-04-R.png` |
| SS-022 / NET-08 | From the browser attempt protected host admission with an incorrect disposable password | NET-08-password-rejected; `Connection not authorized.`; Edit setup focused; Return to browser available; no password echo | `NET-08/NET-08.png` |
| NET-10 | Network → Browse sessions; one real protected first-round host plus four explicitly authorized, leased synthetic metadata records: open lobby, full first round, and two closed records | NET-10-results; protected real row selected; Join selected available; independent state columns and selected identity/mode; closed rows both render Started/Closed, not distinct later-round or outcome-delay proof | `NET-10/NET-10.png` |

The baseline SS-019 / NET-05 representative remains owned by the legacy manifest because no arena layout changes. Add a focused arrival supplement to the evidence packet, not another stable matrix row. Capture a real guest joining first-round Predator at 1280 × 900 after complete admission, with the original Predator and current world visible. Store temporary supplements outside the repository unless developer and team approve durable evidence destinations. The final assessment must identify every supplied artifact by exact path and hash.

### Variant checks and minimum evidence

- Developer must demonstrate NET-10-loading, NET-10-results, NET-10-empty, NET-10-stale, and NET-10-unavailable, including failed refresh and expired selected-row removal.
- Developer must demonstrate that NET-10-stale disables every browser join path until successful refresh under NET-DIR-012.
- Developer must demonstrate age-triggered stale presentation under NET-DIR-011 and independent Direct connect without stale selection transfer.
- Developer must demonstrate bounded paging that reaches all active listings under NET-DIR-008.
- Developer must demonstrate publication retry without a session restart under NET-DIR-015.
- Developer must demonstrate a selected full session and a selected closed session with readable disabled reasons.
- Developer must demonstrate that password protection alone does not disable Join selected.
- Developer must demonstrate a closed first-round outcome-delay listing before the six-second delay finishes.
- Developer must demonstrate that a return to lobby restores the advertised admission state.
- Developer must demonstrate NET-03-password through directory and direct paths, password correction, pending admission, and Cancel states.
- Developer must demonstrate NET-03-live-admission entering the current NET-05 arena without a waiting lobby.
- Developer must demonstrate NET-08-admission-closed with exact canonical copy and browser-origin return to NET-10 for refresh.
- Developer must demonstrate a directory-selected unreachable host without a reachability promise.
- Developer must check the affected menu layouts at 850 × 700 and 1280 × 720 for containment and at 1920 × 1080 for scaled pointer alignment.
- Developer must check maximum supported local-player counts and long valid endpoint/person values without footer overlap.
- Developer must check keyboard and controller traversal, list scrolling, selection preservation, disabled focus exclusion, and recovery without a mouse.

One representative is required per stable wireframe. Variant checks may use focused supplemental images or behavioral evidence; they do not create permanent matrix entries. No screenshot may use fabricated pixels or a replacement UI. The current handoff explicitly authorizes synthetic directory metadata rendered by the actual native client. This scoped fixture is accepted for list layout and status presentation only. It does not satisfy real-host lifecycle, outcome-delay timing, or multi-host admission evidence. The five-live-host reproduction previously requested is not claimed by this packet.

### Evidence packet and acceptance limits

Each supplied artifact must include source checkpoint, capture state, native workflow, supported viewport, presentation profile, environment, exact artifact path, and SHA-256. Include the selected menu-background provenance required by the legacy manifest. Use disposable credentials and exclude secrets from all evidence metadata.

Visual assessment will compare structure with wireframes, interaction presentation with the owning UX specifications, and styling with `docs/design.md`. It will not demand pixel matching to the low-fidelity SVGs. Directory listing is not proof of reachability. Images cannot prove password enforcement, heartbeat expiry, admission race handling, round-one initialization, world continuity, or Predator retention. Behavioral QA owns those checks.

## Current host-directory visual assessment

The original assessment inspected the eight actual canonical PNG files and two arrival supplements. Original packet: `/tmp/opencode/pr99-0849-ux-capture-packet.md`, supplied SHA-256 `a66762bc3b09c68418ff1f3fee60d7a800842861a5bb61ccdd27e613738440ca`. The focused reassessment inspected only the replacement NET-10 and four paging supplements from `/tmp/opencode/pr99-6809885-artifact-and-net10-packet.md`, supplied SHA-256 `8b622440709817926781c9df385d26ef4b9126ea22b653997570ac77c4bcd602`. Hashes in this section are developer-supplied, not independently recomputed by UX; no hashing tool is available in this invocation.

Environment for every inspected image: native Release GL4, Lua ON, Ubuntu 24.04 Linux x86-64 Docker, SDL 2.30.0, Xvfb 1280 × 900 × 24, software GL and dummy audio. Profile: normal input, complete 1280 × 900 client area. No crop, rescale, retouch, or composed pixels are reported. This is same-daemon container composition, not two-machine LAN evidence.

Source keys below identify actual capture source, not a relabeled final head:

- `a633`: `a6337b5c2ce9e3eaaabc871c4cba4ef8a846b6b6`; NET-01, NET-02, and NET-04 rendering is reported unchanged by subsequent production changes. First launch of `d6r99-capture-ui`, background `textures/backgrounds/jungle-channel.png`, runtime asset manifest SHA-256 `99a64c7b4ad0e86df4956a2b6a0e18ca3ae6130d6cd1ca0f22a7d4b11c543ac4`.
- `0849`: approved production source `0849b8c759286e7a43fd24a5459fc34680038d2c`; first launches of `d6r99-0849-capture-host`, `d6r99-0849-capture-guest`, and `d6r99-0849-capture-arrival`; runtime asset manifest SHA-256 `f22f673bf8d8f4717ac06a12ff01ea62c2a9a127262da2904be84045a5b6fa4a`. Host and guest menu background: `textures/backgrounds/alpine-flood.png`; arrival menu background: `textures/backgrounds/jungle-channel.png`.

- `6809885`: approved NET-10 source `6809885ecfe5dc622f03a1bafec0580ba6157602`; canonical capture at `2026-09-25T21:37:13.252425247Z`; first native launch in `d6r99-680-capture-browser`, Xvfb :121; background `textures/backgrounds/forest-foundry.png`, observed through native console dump. Runtime asset manifest SHA-256 `976f9802b292715e8861570fb36d417a54304f5fb1aa5c872c7b51296fb0c8e5`; client SHA-256 `3e891e60c0a0d84ee95e7446ea7aa68bdaad312fd505774c82913a7c3936d307`. The shared environment and profile above apply.

Current reviewed head is `6809885ecfe5dc622f03a1bafec0580ba6157602`. The seven retained image hashes were reported rechecked unchanged; they are not relabeled as 6809885 captures. Source approval and prior independent tester reports supplied by team are supporting evidence; UX did not rerun those checks. Independent focused interaction QA for the latest delta remains separate and in progress. Complete capture lineage remains in the packets.

### Representative findings and artifact identities

Paths in this table are relative to this manifest's directory and match the capture matrix above.

| Wireframe / source | Artifact | SHA-256 | Current assessment |
|---|---|---|---|
| NET-01 / a633 | `NET-01/NET-01.png` | `307b7d5c4d0a18a8ef1469f383c8d628c4efd03041e890b3daf90d9c78cc8ba8` | Conforms for captured state. All four actions and LAN-first scope are readable; Host focus is explicit; no layout collision. |
| NET-02 / a633 | `NET-02/NET-02.png` | `649a3bc69f235ebcc309c0d300e236481b1b8510f4f74e6135a2fc6741ad32ef` | Conforms for captured state. Complete private endpoint, masked optional password, empty-password help, separate person/control regions, count, disclaimer, and actions fit. Password is focused after entry; this is not an initial-focus capture. |
| NET-03-E / 0849 | `NET-03/NET-03-E.png` | `074ac49371e201887f816eb7129fcc3b2e8aef0eda604946c6b3ef1809ea3f23` | Conforms for captured state. Real protected first-round selection, required-password cue and focus, owned local setup, immediate-play consequence, host-validation caveat, Connect and Back remain legible. |
| NET-03 / 0849 | `NET-03/NET-03.png` | `fda1be32c67e8bf0334d859389e6f11e1b5a3c82bc6c931613bed0a9a79e7a80` | Conforms for captured state. Real bounded pending attempt; locked/read-only roster and endpoint remain visible; Connecting does not claim admission; Cancel has clear focus. |
| NET-04 / a633 | `NET-04/NET-04.png` | `12470734e4ffa1a98a6fc2a0f1ce77405da880dd33e471c2e18cbe72d436063b` | Conforms for captured state. Publication failure and Retry publication are separated from the running session and disabled Start match reason; roster, settings, and End session remain unobscured. |
| NET-04-R / 0849 | `NET-04/NET-04-R.png` | `0f8394559fd622c7af079c5d8f70ec4fec571fac07eb2cdac863a32e47fd7f60` | Conforms for captured state. Listed status fits above current membership; retained Completed state, distinct match/round outcomes, stable winner identity, scroll-position feedback, readiness, and footer remain readable. Actual completed match, not injected result. |
| NET-08 / 0849 | `NET-08/NET-08.png` | `2667132a4f037352605cd5e6439dde447f42bb729dba591871f0d7db4eacb64c` | Conforms for captured browser-origin rejection. Exact non-disclosing authorization failure, endpoint, focused Edit setup, and Return to browser are clear and contained. No password echo. |
| NET-10 / 6809885 | `NET-10/NET-10.png` | `a36c9a9a49f98b3a39fa4d0fe1cd52007de56c85226896e1fe85dd865aaeade6` | Conforms for captured state. Page 1 is visible without an invented total; paging precedes the primary footer. Columns, independent protection/admission cues, selection keyline and marker, full selected identity/endpoint/mode, and reachability disclaimer remain legible and contained. |

The retained-result representative replaces the prior image at the same NET-04-R destination. The historical hash below is not the identity of this new artifact.

### NET-10 paging conformance

The previous paging finding is resolved. The replacement satisfies the existing [NET-10 allocation and input](../screens/NET-10.md#allocation-overflow-and-input) requirements: visible current page, paging above primary footer actions, and legible control focus. It preserves the table, selection, state cues, detail region, and reachability disclaimer without overlap. No design exception or new requirement is needed.

The four actual supplemental images below show Page 1 → Page 2 → Page 1, visible Next/Previous focus, and footer focus after paging. Their supplied input sequence reports Tab from the row to enabled paging, skipping disabled Next on Page 2, then Join selected, with reverse traversal back to Previous. These are developer observations corroborated by inspected focus states, not a claim that UX ran the controls or that independent tester QA has completed.

| Inspected focused supplement | Supplied SHA-256 | Observed presentation |
|---|---|---|
| `/tmp/opencode/pr99-680-page1-next-focus.png` | `750ffe292f73d25e6e47dd52e214e92ae94432025795f7bb6af798558bb345b7` | Page 1; Next focused in the paging row above the primary actions. |
| `/tmp/opencode/pr99-680-page2-previous-focus.png` | `adb2f2e992b41bfa28e984bc0ea423b9bb3dcc4928facca64850679143d47adf` | Page 2; Previous focused; Next visibly disabled; row and endpoint details remain contained. |
| `/tmp/opencode/pr99-680-page2-footer-focus.png` | `445bad91e469cee6e61b1f7ffe26c61f3617eb69b4212b70cc6d1be7ecb0eec3` | Page 2; Join selected focused below the paging row. |
| `/tmp/opencode/pr99-680-page1-return.png` | `3d4a82a9f0f0c17c131d7e97afe2369d3da02c0fe560acbeaba085170626532e` | Returned Page 1; previous-page selection cleared; Select a session reason and disabled Join selected are visible; Direct connect has focus. |

All four supplements use 6809885, the same 1280 × 900 environment, background, and process session as the replacement. The paging setup adds 26 disclosed synthetic leased records, not live hosts, and reports no synthetic endpoint connection in this observation. These files remain outside the repository and add no stable screenshot entries.

No visual blocker or additional capture request remains for this correction. Exact-hash integration requires no second assessment if developer commits the assessed artifacts and unchanged UX content without further presentation changes.

### Fixture and supplement assessment limits

The replacement NET-10 real selected row is the protected Predator host at `172.17.0.13:26660`, session `000000000000000041b71c6aede40420`, listing `5c27124786f79e0e8e4f146ee7974cc6`. Four synthetic leased records at ports 26661–26664 supply the other visible metadata. Their listing API responses are genuinely rendered by the native client, but those four rows have no backing live hosts. Both closed fixtures display Started/Closed; neither proves an actual six-second outcome-delay capture. Their visual contribution is accepted only within that disclosed scope. The unchanged 0849 arrival supplements below belong to their original session, not this replacement host.

| Inspected supplement | Supplied SHA-256 | Assessment limit |
|---|---|---|
| `/tmp/opencode/pr99-before-arrival.png` | `e0b8d7c019a60f3cdb77db7cdd346724d77dd7bbcaabfc14b69b275f618e7cca` | Host-side shared arena, round 1/1, two listed players, directory and session status legible. |
| `/tmp/opencode/pr99-predator-arrival.png` | `cb45ed86439aebb1e50e815728ed30fa818884c40998a3f53cb9fec7ca0c1fc5` | Guest-side active shared arena, round 1/1, arrival present in three-player ranking; no waiting-lobby or spectator panel. |

Both supplements use 0849 and the shared environment above. They are not NET-05 replacements. Missing exported match/round identities, tick, mirror flag, and arena-background provenance prevent complete identity/timing or Predator-retention proof from these images. They do not prove a world reset or its absence across the intervening play interval.

Team reports prior independent scroll and password-focus correction checks and protected browser live admission passed. The latest independent paging-interaction delta remains with tester. This assessment does not infer remaining freshness, controller, content-extreme, smaller-viewport, or lifecycle results from still images. Windows/second-machine interactive QA and final product acceptance remain separate gates; visual acceptance does not close them.

The capture-session observation that intentional Leave briefly appeared as Reconnecting remains with tester for diagnosis. It is not adjudicated by this visual assessment, and the later Completed result does not establish correct Leave behavior.

Current conformance: **all eight representatives conform for their supplied states, with original mixed capture lineage and disclosed fixture limits retained**.

Visual gate: satisfied

## Historical integrated PR83 visual evidence

This historical section is an integration index for the legacy [screenshot manifest](../../screenshots/README.md), not a second assessment. The host-directory matrix above owns the newly affected coverage. The original integrated manifest had SHA-256 `90f02bf755ecc4c54142d5555aa8e34a644515ee69415a539a62cf23066173b7`; that hash is historical after the scoped orientation update.

The [current coverage and assessment](../../screenshots/README.md#network-arena-orientation--current-coverage) accepts the PR #87 replacements for `SS-019`, `SS-021`, `SS-023`, and `SS-026` and three orientation comparison supplements. It is the sole current source for their paths, hashes, reproduction inputs, and assessment limits. Earlier acceptance below for the arena representatives and confirmation supplement is superseded for orientation. Their recorded hashes and PR #83 state/background sidecars identify historical evidence, not the replacement artifacts. Other entries remain unchanged.

The manifest's temporary paths record where UX originally assessed the files. The accepted images and supporting observations now have durable repository locations below. Verification does not depend on those temporary paths. The four superseded guest-defect supplements are intentionally excluded.

## Prior five-representative packet

This packet used capture checkpoint `24d244bee7edd5523c5791297cabe1b00c01fd7e` and byte-unchanged production source `f7730db176f48bfd6436b634b88a2d18310bd221`. `SS-019` and `SS-026` below are historical hashes; their linked destinations now contain the accepted PR #87 replacements. The guest-status correction at `6aebaf22d73a3d9409111714d3ebb7c2cf6a12ef` did not execute in these representative states.

| Entry | Image | SHA-256 |
|---|---|---|
| SS-018 | [Host lobby](../../screenshots/NET-04/host-waiting-one-unready-1920x1080.png) | `d9aa586e3c2d2d95ffdea39bfd5076811013f8e32c33dc5635c1f862eb75e1a1` |
| SS-019 | [Actual Invisibility](../../screenshots/NET-05/six-player-lan-degraded-1280x900.png) | `8e91fa6a693ff328271289d3198cf3d55c7f4812d8e98efe06abd2c05c110154` |
| SS-020 | [Final summary](../../screenshots/NET-06/final-three-round-summary-1280x900.png) | `6fa429de1cf74a74cad867b4817bdcdd06b08211476072bc2bb8fc0e1a8c1518` |
| SS-025 | [Retained result](NET-04/NET-04-R.png) | `dba2ee0762f48d9061fae5e375dd41a43bbaff9b5c2a1e9fabb40cabbba4ea77` |
| SS-026 | [Explicit confirmation](NET-05/NET-05-C.png) | `29b60a28f470556f15adcba8a2165df7f529b613cb17dcfa74ed637d27d56744` |

Capture times on 2026-09-14 were 19:20:31, 19:26:03, 19:39:02, 19:40:18 and 19:21:28 UTC, respectively. Environment: Ubuntu 24.04 Docker, Release GL4, Lua ON, Xvfb 24-bit desktops, software rendering and dummy audio. Runtime artifact SHA-256: `fb4aa847ec02f893095c1236aa1e439ca9258c15901e07200ea39f64e11c9496`.

All images show complete actual client areas without cropping, rescaling, retouching or fabricated state. Flat runtime directories were used without a nested-resources workaround. Valid 64-byte personal names were loaded through normal persistence; no match outcome was supplied. Normal gameplay produced the fourteen-winner result, and the same host continued from SS-020 to SS-025 at 1280x900 without restart. SS-019 is an actual pickup in the degraded recovery interval after an owned-server stall, with canonical updates resumed; it does not claim uninterrupted simulation during the preceding stall. The authoritative manifest records the full workflow and acceptance limits.

## Four corrected guest supplements

Owned by [NET-04](../../screens/network-lobby.md), captured at `6aebaf22d73a3d9409111714d3ebb7c2cf6a12ef` on 2026-09-14 between 20:37:17 and 20:37:19 UTC. These are real three-player retained-result bounds/focus evidence, not additional matrix entries or replacements for the fourteen-winner representative. Long UTF-8 status bounds use the accepted source/QA evidence; no synthetic long status was inserted.

| Image | SHA-256 |
|---|---|
| [Person, 850x700](NET-04/retained-person-focus-850x700.png) | `7e7d9f75d877b85ebc416aeed4a32ae7956f9bca9ed73176c5458a33dc69eed2` |
| [Control, 850x700](NET-04/retained-control-focus-850x700.png) | `309b03cf4f2e6e13a72e0a267a01e425bd5090255a0d880467db5aa3dd665db6` |
| [Person, 1280x720](NET-04/retained-person-focus-1280x720.png) | `1ae5fc1da532839ddcbd6ea156e9443e1e423758def4cb425c681d3df9114251` |
| [Control, 1280x720](NET-04/retained-control-focus-1280x720.png) | `fb0e2446343e6b6b84edfdfb99ff733393b00819db5f739c35c369b07d772439` |

## Eight unchanged accepted supplements

These retain `24d244b` capture provenance. Their placement follows the owning NET-04, NET-05 and NET-06 screen documents; they do not add representative entries.

| Image | SHA-256 |
|---|---|
| [Lobby, 1280x720](NET-04/lobby-1280x720.png) | `c4108cad49c27745bcef28b0d69af20dc663defc9bb2b55a7b7ecc85c1664b5d` |
| [Team preferences](NET-04/preferences.png) | `e54edf134fe76164ac3ce1dbaea33bd96d067f6989104ded0f8fa5002c560a79` |
| [Retained horizontal endings](NET-04/retained-horizontal-end.png) | `fc5e4c9c48630a46797b81eed20ded16ff04d16f2ba5c7febdde5a461f998c92` |
| [Confirmation, 1280x900](NET-05/confirmation-1280x900.png) | `b2cc3b4ce04c49877fe1f6074bca13dfc1c72880549cebed38325ec04413aebe` |
| [Summary, 1280x720](NET-06/summary-1280x720.png) | `7c2b050ed9af37f840d9fdfb33e3c1f6fa365c020f9eacc5e7a7db6cf8f62fd9` |
| [Summary horizontal, 1280x720](NET-06/summary-1280x720-horizontal.png) | `35982e11e54fcbcda9b0024c970c0c337ef7b241be0423175e8f4cfc1ce77144` |
| [Summary, 850x700](NET-06/summary-850x700.png) | `0415331a0d9c0f9eb50fe923ee07bd591d231471c3b65156303e86cd8026bbf7` |
| [Summary horizontal endings](NET-06/summary-horizontal-end.png) | `6ea8c6a5898863347fe134cd7a5b673bfc5737594709450347af4a69d0269227` |

## Supporting non-executable observations

These secret-free, read-only capture observations are not fixtures or a verification harness. Their hashes match the accepted packets.

| Evidence | SHA-256 |
|---|---|
| [SS-019 canonical state](NET-05/SS-019-state.txt) | `0ade896d7752643055aa430a7fa1b9a1e2dcf4c7b445a0bda8a4c27c5c8fae07` |
| [Background mapping inputs](NET-05/background-provenance.json) | `9175987ce32ccc411f1ed3a93ac5ac869c730291b0a4f5437cb88c5bd80f5cfe` |
| [SS-020 canonical state](NET-06/SS-020-state.txt) | `c93bfe6b2b491e324ade29ecc917b28d0d7c1607179006f0219215655db672f5` |
| [SS-025 canonical state](NET-04/SS-025-state.txt) | `c6d18db2148037dd181ceadc509c2092d71bafbe8a538d537efc50b2e7f9fc2f` |
| [Corrected guest state](NET-04/retained-state.txt) | `a9b5ef6fc3557629cf874cb66878dd0b0495135c86cd7876ca3053d6ccb0c27c` |

Product acceptance and source approval cover `6aebaf2`. Behavioral acceptance uses the supplied 317 distinct cross-checkpoint cases plus four focused localized checks; these are not summed into a new distinct-case count. Exact-final-head hosted checks and authorization to leave draft remain separate gates.

## Durable delivery record — PR #100

This developer integration record preserves the reproducible provenance and acceptance scope for [PR #100](https://github.com/mkapusnik-apps/duel6r/pull/100). It does not revise the UX assessment above. The preceding UX-authored manifest was integrated verbatim from intake SHA-256 `2038a8467ce830ef9e4c313b53f01dc8151cb521d6b32c3a36235b64b6def8c0`; `docs/design.md` had intake SHA-256 `67033e57b67c77a848d0c24e0db313f42f03490f9e8faaba08cc18026bacc000`. Only this technical delivery appendix was added to the assessed manifest text.

### Permanent evidence and retired temporary inputs

The sixteen canonical PNG destinations in the current matrix are the permanent image evidence. Final integration verifies their exact assessed SHA-256 values from the representative-results table. Five originate from `a740254ee6ceaf5b55914abdf695e76f4766c1fd` (**A**); eleven retain `e5f5517a56c426104f4cfb73c19058844db0d874` (**E**) provenance. Neither source is relabeled as the later documentation/evidence-only delivery commit.

The `/tmp/opencode/pr100-*` packet paths in the assessment are historical inspection locations, not permanent links or runtime dependencies. Those task-local packets, supplement image files, recordings, helper scripts and containers are retired after exact integration and completed assessment. Their relevant identities, observed states, hashes, capture lineage and limits are preserved here and in the assessment tables above. No temporary recording, helper, test infrastructure, or undocumented supplemental-image collection is committed. The supplement filenames above identify the inspected inputs; they do **not** claim that those PNGs are shipped in the repository. Preexisting historical committed evidence elsewhere in this manifest is unchanged.

### Runtime identity and reproduction configuration

Both capture lineages used Linux x86-64, Ubuntu 24.04, Release GL4, Lua enabled, `BUILD_TESTING=ON`, GNU 13.3.0, SDL 2.30.0, SDL_mixer 2.8.0, SDL_ttf 2.22.0 and SDL_image 2.8.2. Actual rendering reported Mesa llvmpipe (LLVM 20.1.2, 256 bits), OpenGL 4.5 Core, Mesa 25.2.8-0ubuntu0.24.04.2. Xvfb supplied separate 24-bit displays; SDL dummy audio was used. The shared build image identity was `sha256:d6e97be33c3c44744ba057147f5b6dae1c1534273b490edc0ef573b8ab1c1720`. The runtime banner reported version 5.5.0; the source SHA, not that banner, identifies these captures.

| Runtime identity | E capture lineage | A capture lineage |
|---|---|---|
| Client SHA-256 | `d932fea087a6bd66a4db0053634bfdab3d19fced439914f55968f3b76b40da39` | `4180e9f6d2e14eb23cb424b4fe2ed454773f1273e1bb6875a0aabca07b4272cd` |
| Linux runtime manifest SHA-256 | `a466b57b87f0e2025f6d95e659bd75509e0da53aa1b19ba5d2293069f9a0c678` | `3e40bb93c01eae24a01978a7b579c9d9494afd4b8c54079c22ff865f1c60c054` |
| Server SHA-256 | `8e44c16f00991974e40c4603000de0210b283f186ed9ee5ed814d59aca3e7008` | Same unchanged server |
| Resolver SHA-256 | `0cba785c55988a06fa3da5921fa89a4f19317f2a70d713459b11a5959455759f` | Same unchanged resolver |
| Host supervisor SHA-256 | `d7d30e9df7abe4b565d46161ebf28fbf0c92c1a4c39de5c1ba54ef895caf80da` | Same unchanged supervisor |

The A runtime archive had SHA-256 `26f317a7628ad076d8429be85aa1c5d28af429b9be5285cc1485f9abaf2f56be`. Archives are not permanent deliverables of this evidence-only integration. Capture consumers used the actual flat runtime at `/workspace/runtime`, with sibling server, resolver and supervisor executables and `data`, `levels`, `profiles`, `shaders`, `sound` and `textures` directories. Each application ran from its own disposable copy and private display. The source/runtime were immutable during capture; the worktree contained pending PNG changes but no uncommitted application, test or build-source changes.

Rebuild and verify through Docker only, following the repository workspace-transfer contract. The captured configuration was generated inside the build container with:

```sh
cmake -S /workspace -B /workspace/build-verify -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DD6R_RENDERER=gl4 -DD6R_WITH_LUA=ON
cmake --build /workspace/build-verify --parallel 12
```

Both consumers and the master runtime passed their Linux manifest integrity checks after capture. No build was repeated merely for final documentation/image integration.

### Application-session and background provenance

Application-session UUIDs below are capture-harness launch identifiers, **not** canonical network session IDs. All dates and times in this appendix are UTC on **2026-09-26**. Background filenames are under `textures/menu-backgrounds/`, observed from the actual startup console diagnostic and retained for that application session; no background override was used. For arena captures the listed menu image is session provenance, not a claim about the visible arena background.

| Key | Source / application-session UUID | Application start UTC | Selected menu background |
|---|---|---|---|
| E-HA | E / `79e87f9a-b67d-4837-95ed-81cc8c7aade6` | 12:54:09.240437 | `jungle-channel.png` |
| E-GA | E / `1a9b7780-655a-4e03-8970-302cabcb6866` | 12:55:24.239577 | `jungle-channel.png` |
| E-PJ | E / `83974d48-40d4-4e88-8dd1-d92ae79bebd5` | 13:02:20.509289 | `alpine-flood.png` |
| E-HB | E / `9abfced9-7666-4bc6-81cc-cc506cc9d3bc` | 13:02:20.772771 | `jungle-channel.png` |
| E-GB | E / `ad614f8e-5f0d-43bf-bc7a-2652c4136d18` | 13:04:32.850669 | `alpine-flood.png` |
| E-V | E / `9dbc2505-d80c-437b-88bb-0c767c3df22b` | 12:58:54.727153 | `forest-foundry.png` |
| A-PH | A / `fcfc009d-1200-4f9e-9e0d-3a8591ae2237` | 14:25:00.679659 | `alpine-flood.png` |
| A-V | A / `1f20c2ad-33ef-459a-bbfe-5e51ba19f1ac` | 14:27:55.151574 | `alpine-flood.png` |
| A-MH | A / `3d3f958c-c777-4aff-816f-a4ec10dd1321` | 14:32:40.403206 | `forest-foundry.png` |

### Exact representative lineage

Paths and PNG hashes are the current matrix and representative-results table above. Each interval records capture-command start and completion, not a GPU-consumed canonical tick. Frames are complete native client images, not crops or composites.

| Wireframe | Session key | Viewport | Capture interval UTC | Actual route and state |
|---|---|---|---|---|
| NET-01 | E-HA | 1280x900 | 12:54:36.647541–12:54:37.002716 | MENU-01 → F2; initial Host focus and four network actions. |
| NET-02 | A-PH | 1280x900 | 14:25:08.348763–14:25:08.711292 | Host setup; Cedar/Dogwood, K1 Arrows, assigned private `172.17.0.5:26660`, focused masked optional password. |
| NET-02-P | A-PH | 1280x900 | 14:29:54.931196–14:29:55.306286 | Genuine Starting and focused Cancel under the authorized child-only pre-readiness barrier below. |
| NET-03-E | A-V | 1280x900 | 14:29:54.860540–14:29:55.336989 | Browse → actual protected open first-round host `127.0.0.1:29200` → Join selected; Elm/K1 Arrows, focused mask, external Password required cue. |
| NET-03 | E-PJ | 1280x900 | 13:02:27.633117–13:02:28.061142 | Direct connect to owned withholding listener `127.0.0.1:26660`; Fir/Hazel locked, K1 Arrows, genuine pending admission and Cancel. |
| NET-04 | A-MH | 1280x900 | 14:32:54.271531–14:32:54.731217 | Two-round Deathmatch lobby at `127.0.0.1:29220`; Maple ready/Oak unready, one slot each, separate unavailable directory endpoint. |
| NET-04-R | E-HB | 1280x900 | 13:13:52.635723–13:13:53.442657 | Host Return to lobby after actual two-round completion; Listed, first result row/column, person focus outside Reorder. |
| NET-05 | E-GA | 1280x900 | 13:00:12.449335–13:00:13.286218 | Birch guest, Alder host, one slot each; actual active round 1/2, Connected plus degraded indication. No forced bonus. |
| NET-05-C | E-GB | 1280x720 | 13:05:47.264769–13:05:48.251292 | Juniper guest opened Leave with Escape during active round 1/2; initial Leave focus, captured before acceptance, then cancelled. |
| NET-05-S | E-HA | 1280x900 | 13:00:14.047677–13:00:14.794753 | Alder host pressed Tab once before a winner; genuine In progress/Pending result with empty body and informational position text. |
| NET-05-R | A-MH | 1280x900 | 14:38:50.003359–14:38:50.714457 | Second ordinary match in this session, first non-final win by Maple; normal Duel 16, Rounds 1\|2, frozen countdown 4s. |
| NET-06 | E-HB | 1280x900 | 13:09:29.297630–13:09:30.187333 | Ivy host, Juniper guest; genuine Completed two-round Deathmatch, before Return to lobby. |
| NET-07 | E-GA | 1280x900 | 13:02:29.533996–13:02:30.563110 | Owned opaque forwarding interrupted; actual 26s reservation countdown and last-confirmed arena; restored before expiry. |
| NET-08 | E-V | 1280x900 | 13:02:24.347076–13:02:24.877354 | Real browser-origin incorrect-password rejection from protected `127.0.0.1:29100`; Edit setup focused, no secret displayed. |
| NET-09 | E-GA | 1280x900 | 13:11:26.484074–13:11:28.284433 | Guest received intentional host End during active play after reconnect restoration; retained arena and terminal panel. |
| NET-10 | E-V | 1280x900 | 13:09:33.474298–13:09:34.150068 | Actual protected joinable first-round listing `127.0.0.1:29100` selected, plus closed completed session `127.0.0.1:29130`; Page 1, real backend. |

The E Alder/Birch session used Deathmatch, two rounds, Assistance and Burnable Trees on, Quick Liquid off, and Random level. The guest connected through an owned opaque TCP forwarder at `127.0.0.1:29110` to the host at port 29100. Its real directory session ID was `0000000000000000398ed76900046220`; the same target supplied E-V browser/rejection evidence. No stream contents were decoded or retained. The interruption closed only owned forwarding links, and restoration preserved the active session.

The E Ivy/Juniper session at port 29130 used two rounds with Quick Liquid on. Actual final results identify mirrored Duel 16 won by Juniper, then mirrored Duel 28 won by Ivy; final winner Ivy. Its real directory session ID was `0000000000000000394d7ec80ac35a0d`. These results supply NET-06 and the same-session NET-04-R; they are not replaced by the newer Maple/Oak match.

The A protected first-round host used Alder/Birch at port 29200 and real directory session ID `000000000000000082c04051978e29cd`. A-V selected that actual listing; it was not a relabeled direct attempt. Directory consumers shared the devops-owned isolated backend network namespace, with `D6R_DIRECTORY_URL=http://127.0.0.1:8081` and `D6R_DIRECTORY_ALLOW_HTTP=1`. No synthetic listing, cloud credentials, deployment or shared-backend shutdown was used. A-MH instead used its own unavailable directory endpoint `http://127.0.0.1:1`, as authorized, without stopping gameplay transport or the shared backend.

The A Maple/Oak session used one K1 Arrows slot per process, Deathmatch, two rounds, Random level, Assistance/Quick Liquid/Burnable Trees on. The first ordinary match had no first-round winner and was not substituted for the requested win. The second match was started through native Return to lobby, Ready and Start. Oak then held Right for three seconds as ordinary gameplay. Maple won on normal/unmirrored Duel 16 and then mirrored Duel 28. No outcome injection, diagnostic level, simulation control or manual Advance was used. NET-05-R is the unchanged recorded frame `round-win-progression-0018.png`; the inspected sequence also showed active end-delay 1\|2 at 14:38:47.807177, normal active Round 2/2 at 14:38:55.087438, later frozen 2\|2 at 14:39:07.459223, and genuine completion at 14:41:56.322885. The recording archive identity was `7b72cb5213396e0bb7082d00c2eb1c44a23553705767bee6a57f781cca00cb64`; the archive is retired, not a committed artifact.

Canonical near-frame match/round IDs and ticks were not exported, and the unregistered Maple/Oak session has no recorded directory ID. E session A's precise random level/mirror and arena-background filename were not independently identified. No missing value is inferred from legacy filenames or generated for this record. The visible styling acceptance does not establish additional deterministic-world, timing, physical-controller, physical-GPU or distinct-machine Windows/Linux LAN claims.

### Reproducible child-barrier procedure and measurements

The actual mechanism was external Linux pidfd signaling, not the earlier proposed preload approach. No helper code was loaded into the GUI or server. For an owned GUI launch, an external watcher identified a child by exact executable path, expected GUI parent and process start identity, opened a pidfd and revalidated identity. Before SIGSTOP, it transferred a duplicate pidfd to a separate release guard. After the stop it verified state `T`, the unchanged server ELF hash and absence of a listening socket; a missed pre-readiness boundary would be rejected and immediately released rather than used as capture evidence. The GUI was only observed and remained in `R`/`S` states.

The watcher targeted a 0.9s hold, independent of image capture. The guard had a fixed 1.25s release deadline and would SIGCONT the same process if the watcher failed. No GUI pause, deadline override, protocol/status write, process-memory write, replacement service or outcome injection was used. Native Start, Cancel and subsequent readiness were handled by the unchanged application. Fallback failure injection was not claimed. Reproduction must remain in a disposable, owned test environment; this is not ordinary startup-latency evidence or permanent instrumentation.

| Attempt | GUI PID / start ticks | Child PID / start ticks | Stop request UTC | Release UTC | Measured hold | Observed result |
|---|---|---|---|---|---|---|
| Readiness | 29 / 9808229 | 164 / 9815793 | 14:26:16.714128 | 14:26:17.615566 | 0.901469139s | Same child/executable listened at `172.17.0.5:26660`; native lobby showed the two local players. |
| Cancel | 29 / 9808229 | 236 / 9825619 | 14:27:54.978074 | 14:27:55.881498 | 0.903459405s | Cancel input completed at 14:27:55.572160 while held; subsequent child inspection was empty and editable setup retained both players. |
| Representative | 29 / 9808229 | 311 / 9837572 | 14:29:54.508092 | 14:29:55.416675 | 0.908618680s | NET-02-P capture completed during the hold; the same child/start identity/executable subsequently listened normally. |

All holds remained below two seconds and within the original ten-second GUI startup deadline. The owned executable was `duel6r-server` in the private runtime copy, with SHA-256 `8e44c16f00991974e40c4603000de0210b283f186ed9ee5ed814d59aca3e7008` before and while stopped. The temporary watcher source identity was `c7ab61d782a2dc7159ba73b5acedbc01239ecfeb25a7d6c1160d701b6a4fc5f7`; it is not committed. PIDs and start ticks are historical observations, never reusable process-control targets. Ready and Cancel reports and their native images were assessed before cleanup; their accepted observations and image hashes remain in the current assessment above.

### Behavioral evidence and acceptance carryforward

The following are separate evidence classes; image inspection is not represented as execution of behavioral tests.

| Evidence / gate | Immutable source and accepted result |
|---|---|
| Source review | Team reports approved at A, no remaining source blockers. |
| Independent QA | Team reports **SATISFIED**, scoped diagnostic coverage plus triangulation of 23 native images, with no broad remaining behavioral blockers. This is independent-team evidence, not developer-authored QA; no unsupported test-count total is invented. |
| UX | **Visual gate satisfied, 16/16** for the exact mixed-source hashes and limits above. V-01 through V-05 closed; no further recapture requested. |
| Product | Team reports **ACCEPTED at A**, with documentation/evidence-only carryforward authorized. |
| Ready for review | Team explicitly confirmed all Ready gates satisfied and authorized the transition after exact final-head/target verification. Hosted Ready is **N/A**. |
| Merge | Hosted checks remain **Merge-only**, owned by devops after the final evidence push. This manifest does not claim their result or a merge authorization. |

Developer verification at A: complete Docker build returned exit status 0; twelve distinct selected cases passed across the following literal `D6R_TEST_FILTER` values. Each filter ran the registered `duel6r-network-session-runtime-tests` CTest with `LIBGL_ALWAYS_SOFTWARE=1`, `SDL_AUDIODRIVER=dummy`, `--output-on-failure -V` and the real flat-bundle harness. No broad application suite was repeated for evidence-only finalization.

| Filter | Distinct cases | Local result |
|---|---:|---|
| `UX-NET` | 7 | Passed, 8.19s |
| `NET-DIR NET-PASS` | 2 | Passed, 2.54s |
| `NET-DIR reviewed menu dispatch` | 1 | Passed, 1.34s |
| `PR83 capture flat bundle` | 1 | Passed, 1.93s |
| `PR83 summary and retained result` | 1 | Passed, 2.78s |

New diagnostics covered field gaps and external password help, 850x700/1280x720/1920x1080 coordinate transforms and hit bounds, inert gaps/release, selector rows, caption inset, moved Ready and unchanged retained Ready. Round tests used current/completed values 1/0 and 2/1 in active/frozen end-delay phases, correct denominators, exactly one progress label, unchanged producer fields, and unchanged final/retained/Tab semantics. Their recording renderer is a draw-submission diagnostic, not screenshot evidence. Earlier exact-E full network-runtime/unit/font CTests passed 3/3 in 94.92s and retain their unaffected scope.

A separate native A smoke used two actual clients at 1280x900, selected Password before clicking the new Host Port top stripe at logical y=514 or Join Port at y=484, and verified the resulting real listener/connection. Ready was activated at y=166, below the old target, followed by successful normal Start and active-context End; the owned server was reaped and both clients exited cleanly. The native report identity was `230b205fcf1153fa15d0cec23956a46dad68978a1f5276daf561011288dea05b`; the selected-case log identity was `e66c8dd8566e15930a088374dafa85e2dba81a814ac4acb0bf2cfb79b86e892d`. These temporary files are retired after their results are preserved here. Missing-binding controller diagnostics do not claim physical hotplug coverage. Existing non-fatal compiler warnings were not expanded into unrelated fixes.

The accepted supplement observations additionally cover native disabled focus/blocked activation, the 850x700 endpoint gap remaining non-interactive, current normal-lobby containment, retained-result scroll extremes, baseline focused Reorder, protected first-round context, child-barrier readiness/Cancel, and normal round progression. Their source/viewport/hash/state limits are preserved in the assessment's supplemental tables. Temporary supplemental pixels and recordings are intentionally not permanent repository dependencies.

Final integration is restricted to these documents and the sixteen assessed PNGs. Application, test, resource and build trees must remain identical to A; matching source review, QA and product acceptance therefore carry forward without an application rebuild. Capture consumers, dedicated build containers and task-local scripts/recordings are developer cleanup responsibilities after durable integration. Shared/preexisting Docker images, resources, worktrees and unrelated artifacts are not cleanup targets. The directory backend was separately owned and removed by devops. No merge, force push, amended commit or skipped check is authorized by this record.
