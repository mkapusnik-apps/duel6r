# UX screenshot coverage and integration evidence

<a id="public-service-extension--pending-coverage"></a>
## Public-service extension — current coverage

This section owns capture and assessment for the public-service extension only. The legacy manifest below retains authority for unchanged baseline artifacts. Later recaptures must replace the same public-profile artifact at the destinations below. Do not create a new screenshot identity for a minor role, copy, or failure variant.

The approved [NET-PUB-VIS-AC-001–004 scope](../../screens/README.md#public-pilot-visual-evidence-scope) permits ordinary actual public representatives for NET-05, NET-06, and NET-04-R. Their rows below are public-extension profiles of the existing IDs, not replacements for legacy extreme-scenario definitions. The `.public.png` destinations keep the public artifacts separate from historical legacy files. No new screen or wireframe is introduced.

All ten public representatives conform within NET-PUB-VIS-AC-001–004: six corrected/new captures plus four unchanged previously assessed captures. UX inspected all six new canonical images and all 38 new supplements. The four reused representatives retain their original provenance and assessment. No required public representative is missing. Historical LAN evidence does not establish public-service conformance; public evidence does not establish legacy extreme-state regression acceptance.

There are no URL routes. Routes below are local UI workflows. The standard profile is the existing desktop retro presentation, full client capture, standard input and font settings. Use 1920 by 1080 for menu representatives and 1280 by 900 for arena-context representatives. Behavioral boundary checks must cover 850 by 700 and 1280 by 720; they do not require a second default representative per wireframe.

| Entry / screen / wireframe | Route and reproducible setup | Expected visible result | Canonical destination |
|---|---|---|---|
| SS-015 / NET-01 / NET-01 | Main menu → Network (F2); NET-01-PUBLIC-ENTRY | Public invite scope; Connect, Host private LAN, Back; visible initial focus | `docs/design/screenshots/NET-01/NET-01.png` |
| SS-017 / NET-03 / NET-03 | Network → Connect; NET-03-PUBLIC-EDIT; fresh production default; two local players; type a disposable test invitation without connecting | Explicit Public (encrypted), complete `duel.netusite.cz`, Port 26660, masked Invite, local controls, Connect/Back; no connected claim | `docs/design/screenshots/NET-03/NET-03.png` |
| SS-018 / NET-04 / NET-04 | First participant connects through public mode to the local trusted TLS fixture; NET-04-PUBLIC-CONTROLLER; admit two guests with two players each; leave one guest unready | Confirmed Host, actual fixture endpoint, separate role/connection/readiness columns; disabled Start reason; host consequence note | `docs/design/screenshots/NET-04/NET-04.png` |
| SS-025 public profile / NET-04 / NET-04-R | Continue the ordinary public SS-020 summary on the same Host at 1280x900; select Return to lobby without starting a new match | Public Host header, cleared readiness, retained completed result and outcome labels; readable results, owned-slot editors, scroll controls when needed, and footer | `docs/design/screenshots/NET-04/NET-04-R.public.png` |
| SS-019 public profile / NET-05 / NET-05 | Trusted TLS fixture lobby → actual two-participant, two-player Deathmatch, one round; NET-05-PUBLIC; capture Host active play at 1280x900 before an outcome, with no modal | Host, Public session, Connected; undivided arena, readable status/ranking/progress, session-only score and script notices | `docs/design/screenshots/NET-05/NET-05.public.png` |
| SS-026 / NET-05 / NET-05-C | During the public match, open explicit guest Leave confirmation without accepting | Distinct guest consequence, contained modal, visible action and Cancel; no input leakage | `docs/design/screenshots/NET-05/NET-05-C.png` |
| SS-020 public profile / NET-06 / NET-06 | Complete the same one-round public Deathmatch through ordinary gameplay; NET-06-PUBLIC; capture Host at 1280x900 | Completed, separate match/last-round outcomes, Session only and no-persistence notice, role-correct actions, readable result viewport | `docs/design/screenshots/NET-06/NET-06.public.png` |
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

Visual gate: satisfied

PUI-01–03 are resolved, PUI-04 is resolved, and the three ordinary public representatives are assessed. No further capture is required for this visual scope. Exact-hash integration requires no second assessment unless presentation source, artifacts, or UX-owned content changes. Security, native Windows, functional regression, infrastructure review, and live deployment remain separate team gates.

### Current artifact assessment

Current handoff: `/tmp/opencode/public-gui-recapture-report.md`, supplied SHA-256 `c56bab000d1d9b0a9c4008130f2d11e055ca1bcbef929ac5cb430e9d4a7c0713`. Full timestamp/viewport inventory: `/tmp/opencode/public-gui-recapture-inventory.json`, supplied SHA-256 `c44e7677666b98f868e47675d314772496869e86cc7c544ff0fb1aa7ec70525f`. All hashes here are supplied by developer, not independently recomputed by UX.

The six new artifacts use production source `a69a5adfd8fadb95e2024179bb5ecbd6ab3a62ad`, accepted QA checkpoint `424c4bde8a945f010432b554ad6de4e8c1ddc422`, and capture checkout `074bfe13ef214cea6a61833f2666759c9ab007f4` with unchanged production source. GUI SHA-256: `8d2522f14f083befdbef596a5ff2b4ca7cc44ad01446b50588800158dc22b947`. Environment: Ubuntu 24.04 Docker, network isolation, Xvfb fullscreen, Release GL4, Lua ON, software rendering, dummy audio, standard font/input. Source review and affected QA PASS are supplied team evidence, not tests run by UX.

SS-015, SS-018, SS-026, and SS-023 reuse unchanged artifacts from `a985ada14a3f514c0158d85a9bc8946c1c00dc03`. Their original handoff is `/tmp/opencode/public-gui-capture-report.md`, SHA-256 `6b2f4736003d8d0d98cff81f55f6058622f7334142c773e7847946cf4ab5835d`. They must not be relabeled as captures from a69a5ad. Earlier SS-017/021/022 hashes are superseded by the table below.

All connected captures use actual admission through trusted local TLS with identity validation and invitation enforcement. The canonical ordinary sequence uses two participants with one player each, Deathmatch, fixed Duel 01, one round, Assistance off, Quick Liquid off, Burnable Trees on, and 1280x900 clients. Guest movement into water produced Host One's real win; the visible result records seed `3355622747484268544`, Duel 01 Mirrored, player 2 as winner. SS-020 and SS-025 are the same session, Host process, and startup viewport with Return to lobby between them. SS-021 is a subsequent active match interrupted and restored before expiry. No fabricated results, forced outcomes, or cloud readiness are claimed. SS-017 and SS-022 use 1920x1080.

| Entry | Current conformance | Supplied image SHA-256 |
|---|---|---|
| SS-015 | Conforms for entry hierarchy, scope, complete action captions, initial Connect focus, and containment. | `49dbb5bb8f3c41287f99f7c98e96194c3cf90d89b3b1511985322528de461b2c` |
| SS-017 | Conforms at a69a5ad: selector marker and unfocused field boundaries are visible; empty and masked Invite, row focus, scrolling, validation and footer are contained in boundary supplements. | `a27bc8dc14d3fe083360aab59dee893850fa595454a49ca67fcb248c2da35f44` |
| SS-018 | Conforms for confirmed Host, endpoint separation, role/connection/readiness columns, ownership, unready participant reason, and actions. Guest supplements retain read-only explanation and owned-control focus. No maximum-roster claim. | `516323aa2e62df68f59153132f24710193c49864b4b23a8d3d1a23ae6b052904` |
| SS-026 | Conforms for explicit guest consequence, wrapped prompt, focused action/Cancel separation, and public arena context. Host confirmation supplement has the distinct everyone consequence. Still images do not prove input suppression. | `4ce0bd1a31f2f7f51acd2f92c3e558598b307557e79d1b99fc8f057d6d62e5ab` |
| SS-021 | Conforms at a69a5ad: both active-match notices, Host consequence, positive countdown, last-confirmed context and End action remain readable; both roles and minimum viewports assessed. | `50d933c5d6fe25a41700b573209060db3aa217a47f1f1912057d4fd8876dced7` |
| SS-022 | Conforms at a69a5ad: fixed security failure, bounded endpoint, Edit setup focus, Return to Network, no Retry/bypass, no generic host-running helper. | `91c04703e12b15eaeaaee8725772821a8ca3e35a9e6b367d37902e9a8a8d6a1b` |
| SS-023 | Conforms for intentional host-end message, no-resume and no-persistence copy, retained-state context and Return to Network. This is not maintenance or expiry evidence. | `58c5197fed6ad2935b7d4f3302407a516ded3f41a4ea4787ae63cfbbc79fc4bc` |
| SS-019 public | Conforms at a69a5ad: unobstructed actual public match, Host/Connected status, script/score notices and actual degraded text readable. No Invisibility-combination claim. | `94f8c4c69ce0f75c055bbfa9209ada2780bd3164b0db2981c8cb025002ff5aca` |
| SS-020 public | Conforms at a69a5ad: Completed, separate outcomes, Session only/no-persistence notice, actual result access, and role-correct actions. Summary has no endpoint/status region to relabel; no new result field is required. | `de64242e437288f3737f407067691078d5c480b20dbe61d09dc45fcf9f7a9e14` |
| SS-025 public | Conforms at a69a5ad: same-session retained Completed result, public role/context, cleared readiness, fixed outcome labels/footer and bounded scrollable result region. | `1d4a0f3fae2c4e4f70b860247577302f516c148428994cac2fc0292eb3968b77` |

Current menu backgrounds are `forest-foundry.png` for SS-017 (also reused SS-015/018), `jungle-channel.png` for new SS-022 and the ordinary summary/retained pair, and `alpine-flood.png` for boundary clients. Their respective hashes are `fa3d12b5dac0508d44596671d54ecdd999b87772b4a58d743bbe25d335b1fef4`, `9f3f57ed5e8b68f50a791fdacfc2a650f9cbcbf62746237586ba50d07f1d6769`, and `45be6836764a5e1b0d36aa19abfb884335454a67760dd5b1f6272a56cb49984e`. New runtime asset manifest SHA-256 is `4c7ec3c33a0230e681d4c5a7e40ae081355374cdd89f502fbc57a9a50e5ea402`. Arena asset identification is visual rather than an instrumented fallback trace. No deterministic fallback or full orientation regression claim is made.

### Findings — resolved

- **PUI-01 — Resolved:** Persons row focus no longer crosses neighboring text at either minimum viewport. First, last visible, scrolled and pointer-selected rows fit above the fixed status/footer.
- **PUI-02 — Resolved:** Public/private selector has an identifiable marker; address, five-digit Port and empty/masked Invite keep visible unfocused boundaries. Maximum-input and overlength-feedback images remain contained. No credential is exposed.
- **PUI-03 — Resolved:** Both roles show `Match continues while you reconnect` and `Reserved players receive no input and remain in play`. Host consequence, positive countdown and action fit at 850x700 and 1280x720.
- **PUI-04 — Resolved:** The public security outcome no longer shows the generic player-hosted helper; fixed failure copy and permitted actions remain.

**Isolated End layering:** Both minimum-size images show part of the inactive recovery End action behind the foreground confirmation. This is retained modal background, not a second foreground action. The complete foreground consequence, End session and Cancel remain readable, separated and unobscured. The request-or-expiry copy does not claim delivery to an unreachable service. With supplied modal input QA and observed Escape cancellation, this is not a visual blocker. This assessment does not require hiding or restyling the background or another capture; it does not authorize underlying pointer/keyboard activation while modal.

Developer observations and supplied affected-state QA support input/focus, protected paste, real outcomes, result scrolling, and recovery/confirmation transitions. UX did not run these tests. The screenshots establish observed presentation, not maximum-player/extreme-result behavior, physical-controller coverage, native Windows security, all terminal notices, or full functional regression. Team retains those separate gates.

### Remaining representative scope and reproduction

Product approved the evidence-only scope in NET-PUB-VIS-AC-001–004. All three ordinary public rows are now captured and conform. The following sequence remains the reproduction recipe, not a request for another capture. No new product decision or visual correction remains.

Practical public capture sequence:

1. Start a Host GUI at 1280x900 and a separate Guest through the trusted local TLS fixture. Admit one ordinary short-named player per participant with independent controls. Select Deathmatch, one round, a shipped fixed level, and Quick Liquid off; record the actual settings. Both participants become Ready; Host starts the match.
2. Capture the Host's unobstructed active arena as `NET-05.public.png`. No Invisibility pickup, held-weapon/degraded combination, maximum roster, or special outcome is needed for this public profile.
3. Use ordinary movement/shooting to finish the round. A normal elimination or environmental death may produce the actual result. Do not disconnect a participant to force completion, inject results, or substitute an interrupted lobby for NET-06. Capture the completed Host summary as `NET-06.public.png` with outcome labels, persistence notice, and actions visible. Scroll through real result content for the accompanying access observation.
4. On that same Host, select Return to lobby. Do not restart, resize, or start another match before capturing `NET-04-R.public.png`. Show the retained completed result, public role/context, cleared readiness, and available actions. Guest departure is not required for this public profile.
5. Assess the same ordinary states at 850x700 and 1280x720 using actual supported client presentations. Supply focused supplementary captures for readability/containment, not additional default representatives. Separate normal sessions at those startup sizes are acceptable; record which session each image represents. Every artifact needs immutable source checkpoint, state, role, viewport/profile, environment, path, and SHA-256. These steps are a capture plan, not a claim that UX executed it.

The completed-summary screen must show public context only in its existing presentation regions; do not add an endpoint to result data or new result columns for evidence. Functional result and action contracts remain unchanged.

#### Preserved legacy extreme coverage

The following scenario definitions, original checkpoints, hashes, and assessment status remain governed by the [legacy manifest](../../screenshots/README.md). This update neither accepts nor invalidates them. No legacy image is renamed, overwritten, or claimed as current public evidence.

| Legacy entry | Preserved scenario | Existing artifact, relative to repository root |
|---|---|---|
| [SS-019](../../screenshots/README.md#ss-019) | Six-player degraded recovery with actual Invisibility on body and held weapon; current orientation assessment remains at the legacy manifest's override | `docs/screenshots/NET-05/six-player-lan-degraded-1280x900.png` |
| [SS-020](../../screenshots/README.md#ss-020) | Fourteen Predator winners with maximum-length names and departed-winner access | `docs/screenshots/NET-06/final-three-round-summary-1280x900.png` |
| SS-025, [NET-04-R contract](../../screens/network-lobby.md) | Same-host continuation after a five-player winning guest leaves; ten current players distinct from fourteen retained winners | `docs/design/screenshots/NET-04/NET-04-R.png` |

Ordinary public screenshots do not verify those extremes or establish current functional regression acceptance. Changes beyond public copy/role presentation require team impact assessment and replacement evidence for any affected legacy criteria under NET-PUB-VIS-AC-004. Functional, security, Windows, and deployment gates remain separate.

### Current supplemental artifact assessment

All 38 images below were read and assessed. Paths are relative to `/tmp/opencode/`; all use a69a5ad and the current report's environment. `850` means 850x700; `1280` means 1280x720; the two scroll-only files are 1280x900. Inventory timestamps and session reproduction inputs remain in the hashed recapture handoff. These are supplements, not additional default wireframes.

Input rows conform for field affordances, non-overlapping focus, list containment, mask clipping and fixed validation/footer placement. Recovery rows conform for positive countdowns, both active-match notices and role-specific consequences. Active-match rows preserve readable public status. Summary and retained rows conform for distinct outcomes, no-persistence copy, fixed actions/headings, and contained actual scroll access. Security rows preserve the exact failure and permitted actions. No observed blocking defect remains.

Boundary session A has Host at 850x700 and Guest at 1280x720, seed `12182773237827809280`. Boundary session B reverses roles through a new admission, not migration, seed `10548663201201477632`. Each summary/retained pair is the actual same-session result at unchanged startup sizes. Player display names such as Host One or Guest One do not grant roles. The isolated-End rows have the accepted background layering described above.

| Artifact | Assessed state | Supplied SHA-256 |
|---|---|---|
| `recapture-850-public-row.png` | Public first-row focus | `540b6bfefabda5bf7da33ccc12acc489ec88bc70ebbfe28488c9a82bad1c7cdd` |
| `recapture-1280-public-row.png` | Public first-row focus | `d11db38ff5635cef707a8ab8d329bc7800ec3292950dcca7d6f3344585ba9d17` |
| `recapture-850-scrolled.png` | Public last visible row | `1474e4c3a8dafb2b70a6a2f83100750640208f66f5c1964ea85bf3e8aa7bcee4` |
| `recapture-1280-scrolled.png` | Public last visible row | `cc4c4b30502f8f6f61bc8dd38ccae919a6525cfe98736add6975155bc3357210` |
| `recapture-850-pointer.png` | Selected Person04 and local assignment | `fe28b0751039c7cb5c2004451e18c13eaf8758d933ed72fdf78e46aa23ae47ec` |
| `recapture-1280-pointer.png` | Selected Person04 and local assignment | `244eb06c071ac5a7b24dd12901ae07d2ebf94cdfe67f8ba4a2459b096e0a6c9e` |
| `recapture-850-lan-row.png` | LAN first-row focus, no Invite | `ec854f8775514b2fb2434892150cd48e1fa4322aa375b3970be502ec50f6a19d` |
| `recapture-1280-lan-row.png` | LAN first-row focus, no Invite | `1a0e9a4bc8391c2d8937807d36786f4040e2b27e3d26b76688d48f18e7ccce27` |
| `recapture-850-lan-scrolled.png` | LAN last visible row | `ef2d565f89cda5cc18cd84e9af965ef32cff43ee27d5f6f55968d1ce26b333c9` |
| `recapture-1280-lan-scrolled.png` | LAN last visible row | `8e6e426f045f7853b27a1ea500af936cde90bfec651eb4df3e1a86c9a03a2413` |
| `recapture-850-max-input.png` | 253-character address and 256-character mask | `b35a304692ff3f8ac5d3c713e3ea6dc5416d7d5016dac9748ebb7713ea34757a` |
| `recapture-1280-max-input.png` | 253-character address and 256-character mask | `2d0ab3fedbb205dae59b21badb93a783c0de3230442465959200178195e39eb8` |
| `recapture-850-overlength.png` | Rejected extra character feedback | `27d3b7b22a29af238434c6fa64ef720e052160882ac3dc91fbf2a6e19d38c793` |
| `recapture-1280-overlength.png` | Rejected extra character feedback | `bae8f40edc9290b9c1721dc331b56fbdacd7a44b44512f3003f11102b8684820` |
| `recapture-850-empty-invite.png` | Port edit clears Invite; disabled reason | `e9b91869ebf405798d6a70aa26ca7da53dc6ab5584bb81620795556e8b047959` |
| `recapture-1280-empty-invite.png` | Port edit clears Invite; disabled reason | `539a80a779e1184dfa27f2a020e5d410f565ac0316c1b53568bf78506d4352fe` |
| `recapture-850-active.png` | Session A Host active match | `b7be7743774ec8d0028e8e05703d2123aaa0b466762b7e83d897fe8f47af375a` |
| `recapture-1280-active.png` | Session A Guest active match | `2a7991a4df6b467ae9db5aca1eb6d38e755344dc9566b83f4ca0dda7da534bf5` |
| `recapture-1280-active-host.png` | Session B Host active match | `1f8a8041e155f2392d6f6509221631716b3bb6fd5f844fb8ec17f9905777dffc` |
| `recapture-1280-new-controller.png` | Session B fresh confirmed Host | `d56ea6fe5c97b7af2eaf6b0b863cede0ad5c8ecbd14f777f9fa34ca229f4410a` |
| `recapture-850-host-reconnect.png` | Session A Host recovery | `f01cbcfcdc706937a2312a37a5631b5e2d63140f15f80d26ac44bd2b810d9f92` |
| `recapture-1280-host-reconnect.png` | Session B Host recovery | `715b70696827e2e0344cb58e5b4bd0fcaf399327c954800c8c24eb7d81a42bf0` |
| `recapture-850-guest-reconnect.png` | Session B Guest recovery | `620de1a89389bbf1a5337dd4596e1f4ec01de8a3b07e94ac93596a8de170952f` |
| `recapture-1280-guest-reconnect.png` | Session A Guest recovery | `ede267c0296e9743fbeceb05ffc2235d58f366fcab37a30f83b5bd2086419ac4` |
| `recapture-850-isolated-end.png` | Session A isolated Host confirmation | `6007734e7e0a39fb7514eea7e32382db50245256d068591a8fd682ca8ad1503d` |
| `recapture-1280-isolated-end.png` | Session B isolated Host confirmation | `26547d15124cc93788e39fffb35a8f32180329698aba09f134f0821abc8df773` |
| `recapture-850-summary.png` | Session A Host completed summary | `5151f22b3c527abcceecaf4b7526a8c05b45bc83a7f9671ad4ef72b1a91c61b0` |
| `recapture-1280-summary-guest.png` | Session A Guest completed summary | `5bd0acb6efe5210ed4cf8a2707dc39413931330545d7ddb3f880d0a1badcd7cf` |
| `recapture-1280-summary-host.png` | Session B Host completed summary | `eada822dcba8107c16afd9a0f0df1394fc90eb340ed8466b058f837af8490d87` |
| `recapture-850-summary-guest.png` | Session B Guest completed summary | `05315ec3ebdb57c2c87cb2c633b4d87616028ecfa12e9d33435032f68a460ff3` |
| `recapture-850-retained.png` | Session A Host retained result | `df66313ef84012c489e293a0cf2d2a83e71bb5ef418f76aa79f32ea4ee86a64b` |
| `recapture-1280-retained-guest.png` | Session A Guest retained result | `5fbbf0d1d6770606ee87a564cd80e2ee04db31c400086b8dd99b690dcf17ac47` |
| `recapture-1280-retained-host.png` | Session B Host retained result | `6c8c7f7eba7f9ae23fe0ea9f03558ec530a750a03c48fa49b8ffec700715f453` |
| `recapture-850-retained-guest.png` | Session B Guest retained result | `bd416a8a48826c5b05283b4a72625a36c7447885d0008c4f18130964fdf3754c` |
| `recapture-850-security.png` | Wrong endpoint identity | `d7445e133de15054f9ba9257b7dca91de65a5f5eecc79b30a16b0548c6151751` |
| `recapture-1280-security.png` | Wrong endpoint identity | `7f22e1044e5707818643480a7286026da1a59549a288a3f2163278bd230fc36f` |
| `recapture-summary-scroll.png` | Canonical summary bottom/right access | `e23327e66dbcf3e4fc74f589044a559f04684eeb94ea14669946b8e3ccaa208e` |
| `recapture-retained-scroll.png` | Canonical retained-result bottom/right access | `812b417f68e83acc917f12167beb10f2e7ed6518f2922a7ac7615eac601967f5` |

Developer must preserve exact hashes when integrating the canonical artifacts and any required supplemental evidence. Local evidence is sufficient for this assessment; a push or commit is not required. Temporary report/inventory and supplemental cleanup remains developer-owned after evidence integration. No retouching or replacement state is authorized.

### Earlier supplemental artifact assessment — superseded for corrections

The fourteen images below were assessed at a985ada. Their defect findings are historical and resolved by the current recapture; do not use them as evidence of the corrected controls or notices. Paths are relative to `/tmp/opencode/`. The current 38-image assessment is above. Developer owns evidence retention/integration and cleanup.

| Artifact | Assessment | Supplied SHA-256 |
|---|---|---|
| `public-boundary-850-join.png` | Mask contained; controls affected by PUI-02. | `8acebb15119585606699964cd8a8c763051eb62108863c7efbd957b985342532` |
| `public-boundary-850-lan.png` | PUI-01: focus outline crosses H2 text; mode and trusted-LAN note readable. | `f82813a66326e664ae01a3820e9b7072934a055db57bcf97040b72c6402052a2` |
| `public-boundary-850-lobby.png` | Host role, Person focus, status and footer contained. | `4ef2aa3fb891d07cddf61c229da9708eff4877250acc81936cdf02cdf9bb88bf` |
| `public-boundary-850-reconnect.png` | PUI-03; countdown/action contained; retained-state status readable. | `71ec552c5fad0e6949bd6796442a7bc87fbe6fd8d3a3c79124e5d1eab17c8e59` |
| `public-boundary-850-end.png` | Connected Host End confirmation and two actions fit. Not isolated End evidence. | `9af0de09ec7cbc8470c1ecfafb44c3d7794dc7768f581a887c8e64df0c42056d` |
| `public-boundary-850-security.png` | Fixed reason and recovery actions fit; PUI-04 advisory. | `58fee0dba286714539322bb332561f7fed03a47d4171a9064594b329e0b67f93` |
| `public-boundary-850-edit.png` | Staging address, empty-invite reason and disabled Connect fit; empty field affected by PUI-02. | `28cdc233b93570387599c5d482dfe3522c9203aef61dda10c998df1e752823cc` |
| `public-boundary-1280-join.png` | Mask contained; controls affected by PUI-02. | `944deb22ea56eee80fecdafb116231644d26786120b454398b9bb0d94e420214` |
| `public-boundary-1280-lan.png` | PUI-01: focus outline crosses G2 text; mode and trusted-LAN note readable. | `e864f1cbbac16180eb728299540cb25a3fbb0e7e8827fab8d976624be1dcf345` |
| `public-boundary-1280-lobby.png` | Guest Person focus and read-only explanation are separate and contained. | `d5f7815fbd420b5cb7795947aa4c1033e5cef67f9d879670331bba47cac7371c` |
| `public-boundary-1280-readonly.png` | Ready focus and read-only settings/status are contained; raster alone does not prove traversal. | `90c5918cd59cbc5c6c7ecf84a05a950f18d1a1e1ce3cb8805ccb80ff21f99c26` |
| `public-boundary-1280-confirm.png` | Guest consequence and action/Cancel fit; degraded text remains readable below modal. | `5548f2b68ce1624e45c7bcb00a46df5daa0734fce0e19d548248227be767aaf5` |
| `public-boundary-1280-ended.png` | End reason, no-resume/persistence text and Return fit. | `143de1b3536b6ed0fd788744c13c571dca67767ae0b265a06a2375da19ef0fec` |
| `public-boundary-1280-security.png` | Fixed reason and recovery actions fit; PUI-04 advisory. | `a639984c5de3b6e24459fe56762ee7a97409cdee160afbe662cb45b5309149d5` |

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
