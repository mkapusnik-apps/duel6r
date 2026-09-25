# Screenshot coverage and assessment

## Host-directory current capture matrix

This section owns changed coverage for the host-directory extension. It supersedes the legacy representatives only for the affected wireframes below. Unaffected entries remain governed by the [legacy manifest](../../screenshots/README.md). The historical integration index below is not evidence for the extension. Existing artifacts must not be relabeled as new acceptance.

All entries below are **Planned / not assessed**. Developer owns capture and image files. UX has assessed no implementation artifacts for this extension. Product reserves NET-10 and its states in the [browser contract](../../screens/network-browser.md). Routes are native menu workflows, not URLs.

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
| NET-10 | Network → Browse sessions; register real controlled hosts in lobby, protected open round one, full, later round, and first-round outcome delay | NET-10-results; all listing phases coexist; protected open first-round row selected; Join selected available; independent state columns and selected identity/mode | `NET-10/NET-10.png` |

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

One representative is required per stable wireframe. Variant checks may use focused supplemental images or behavioral evidence; they do not create permanent matrix entries. No screenshot may substitute a fabricated table for a reachable implemented state. If the all-state controlled setup is unavailable, report the blocked capture instead of substituting a mockup.

### Evidence packet and acceptance limits

Each supplied artifact must include source checkpoint, capture state, native workflow, supported viewport, presentation profile, environment, exact artifact path, and SHA-256. Include the selected menu-background provenance required by the legacy manifest. Use disposable credentials and exclude secrets from all evidence metadata.

Visual assessment will compare structure with wireframes, interaction presentation with the owning UX specifications, and styling with `docs/design.md`. It will not demand pixel matching to the low-fidelity SVGs. Directory listing is not proof of reachability. Images cannot prove password enforcement, heartbeat expiry, admission race handling, round-one initialization, world continuity, or Predator retention. Behavioral QA owns those checks.

Current conformance: **not assessed; implementation evidence pending**. No prior accepted screenshot satisfies the changed coverage automatically.

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
