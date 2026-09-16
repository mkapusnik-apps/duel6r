# UX screenshot coverage and integration evidence

## Public-service extension — pending coverage

This section owns pending capture and assessment for the public-service extension only. The legacy manifest below retains authority for unchanged baseline artifacts. New captures must use the canonical destinations in this table and replace the same wireframe's artifact on later revisions. Do not create a new screenshot identity for a minor role, copy, or failure variant.

All entries below are **Pending**. No extension screenshots have been supplied or assessed. Historical LAN evidence does not establish public-service conformance. No image hashes are available for these pending entries.

There are no URL routes. Routes below are local UI workflows. The standard profile is the existing desktop retro presentation, full client capture, standard input and font settings. Use 1920 by 1080 for menu representatives and 1280 by 900 for arena-context representatives. Behavioral boundary checks must cover 850 by 700 and 1280 by 720; they do not require a second default representative per wireframe.

| Entry / screen / wireframe | Route and reproducible setup | Expected visible result | Canonical destination |
|---|---|---|---|
| SS-015 / NET-01 / NET-01 | Main menu → Network (F2); NET-01-PUBLIC-ENTRY | Public invite scope; Connect, Host private LAN, Back; visible initial focus | `docs/design/screenshots/NET-01/NET-01.png` |
| SS-017 / NET-03 / NET-03 | Network → Connect; NET-03-PUBLIC-EDIT; fresh production default; two local players; type a disposable test invitation without connecting | Explicit Public (encrypted), complete `duel.netusite.cz`, Port 26660, masked Invite, local controls, Connect/Back; no connected claim | `docs/design/screenshots/NET-03/NET-03.png` |
| SS-018 / NET-04 / NET-04 | First participant connects through public mode to the local trusted TLS fixture; NET-04-PUBLIC-CONTROLLER; admit two guests with two players each; leave one guest unready | Confirmed Host, actual fixture endpoint, separate role/connection/readiness columns; disabled Start reason; host consequence note | `docs/design/screenshots/NET-04/NET-04.png` |
| SS-025 / NET-04 / NET-04-R | Complete a public match; return to lobby with retained result using existing outcome-extreme reproduction | Public header does not cover historical results, scroll controls, owned-slot editors, or footer | `docs/design/screenshots/NET-04/NET-04-R.png` |
| SS-019 / NET-05 / NET-05 | Fixture public lobby → actual six-player match; NET-05-PUBLIC; use existing degraded representative procedure through the secure connection | `Public session`; undivided arena and readable status; preserve accepted orientation and Invisibility checks | `docs/design/screenshots/NET-05/NET-05.png` |
| SS-026 / NET-05 / NET-05-C | During the public match, open explicit guest Leave confirmation without accepting | Distinct guest consequence, contained modal, visible action and Cancel; no input leakage | `docs/design/screenshots/NET-05/NET-05-C.png` |
| SS-020 / NET-06 / NET-06 | Finish an actual limited public match with existing summary reproduction inputs | Correct completed result, session-only persistence notice, role-correct actions; no LAN-only copy | `docs/design/screenshots/NET-06/NET-06.png` |
| SS-021 / NET-07 / NET-07 | Disconnect the admitted public Host during an actual fixture match; NET-07-PUBLIC-RECONNECT; capture within the original 30-second reservation | Last confirmed arena; Host label, positive countdown, truthful consequence; End session available | `docs/design/screenshots/NET-07/NET-07.png` |
| SS-022 / NET-08 / NET-08 | Public Connect to a local TLS fixture presenting a trusted-chain certificate with the wrong endpoint identity; NET-08-PUBLIC-SECURITY | Exact TRU-PUB-016 message; Edit setup focused and Return to Network; no Retry or bypass; no invite disclosure | `docs/design/screenshots/NET-08/NET-08.png` |
| SS-023 / NET-09 / NET-09 | Guest in fixture public match receives an actual accepted intentional End session notice; NET-09-PUBLIC-CONTROLLER-END | Confirmed end panel over last arena; no resume; Return to Network | `docs/design/screenshots/NET-09/NET-09.png` |

The matrix invalidates only affected public representations. NET-02 / SS-016 and Local Play remain unchanged and retain their existing coverage unless presentation source changes. NET-05, NET-06, NET-04-R, and NET-05-C need replacement for their public context; unchanged layouts do not justify new wireframe IDs. SS-020 uses NET-06-PUBLIC. SS-025 and SS-026 use the retained-result and confirmation variants within their public screen states. All connected rows may use the same local fixture session sequence where their setup permits it.

### Capture gates and supplemental checks

1. Use the reconciled [functional references](../README.md#reconciled-functional-references) and exact product-owned state mappings. Presentation has no outstanding product dependency.
2. Developer must supply a functioning public transport/admission path and security-check evidence. A local trusted TLS fixture is sufficient for UI capture; live DNS, cloud provisioning, and public deployment are not visual-gate prerequisites.
3. The fixture must use the production client path with Public (encrypted) selected. Use a custom loopback endpoint such as `127.0.0.1`, a recorded port, and a valid certificate with the matching IP identity chained to an explicitly trusted test authority in the isolated test environment. Certificate validation, invitation enforcement, and real authoritative admission must remain enabled. A plaintext or validation-bypass fixture is not acceptable.
4. Record fixture identity and trust setup without secrets. Developer owns temporary fixture cleanup. Do not relabel local evidence as production or staging deployment evidence. Production-default SS-017 needs no connection; custom secure fixtures exercise the subsequent states. Cloud deployment acceptance remains a separate gate.
5. Use short-lived test invites. Capture masks as rendered; do not retouch images. Evidence must contain no reusable invite or reconnect secret. Empty invite entry may represent SS-017 when no safe masked test setup is available; record that state precisely.
6. Record source checkpoint, exact state, role, viewport, presentation profile, environment, reproduction inputs, file path, and SHA-256 for every artifact. Record selected menu background and relevant runtime asset identity under the existing evidence contract.

QA must also supply observations for custom private-LAN connection, staging address entry without requiring live DNS, long valid endpoint/invite input, invalid invite, explicit paste, invitation clearing/retention, Connecting/Cancel, keyboard/controller/pointer focus, guest read-only controls, Host and Guest reconnect, host departure, deployment termination, and ended-session recovery. Exercise NET-03-PUBLIC-CONNECTING, NET-04-PUBLIC-GUEST, NET-08-PUBLIC-MAINTENANCE, and NET-08-PUBLIC-CONTROLLER-EXPIRED through their actual authenticated paths. Test abrupt service loss separately from an authenticated maintenance notice. Do not fabricate an unreachable state or assume that a socket close proves its cause. Minor role and outcome variants require behavioral observations, not extra default screenshot entries.

### Visual acceptance

- Structure must conform to the stable wireframes, not their pixel appearance.
- Styling must conform to the existing design system; no shared-token redesign is authorized.
- Endpoint, invite, local setup, role, failure reason, and primary action must remain clear at the supported minimum viewport.
- No text, focus outline, pointer region, or action may overlap an adjacent region.
- Roles, public/private context, readiness, and disconnect causes must remain understandable without color.
- The accepted capture packet must contain every affected representative and its required metadata and hashes.
- Supplied behavior evidence must establish transitions that still images cannot prove.

Visual gate: blocked

Reason: current implementation artifacts and behavioral evidence have not been supplied. Functional reconciliation is complete; DNS and cloud availability do not block local visual verification. Exact-hash integration requires no second assessment unless presentation source, artifacts, or UX-owned content changes.

## Integrated PR83 visual evidence

This is an integration index for the authoritative [screenshot manifest](../../screenshots/README.md), not a second specification or assessment. The original integrated manifest had SHA-256 `90f02bf755ecc4c54142d5555aa8e34a644515ee69415a539a62cf23066173b7`; that hash is historical after the current scoped orientation update.

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
