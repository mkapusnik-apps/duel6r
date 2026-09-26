# Screenshot coverage and assessment

## Host-directory current capture matrix

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
