# Public dedicated GUI capture handoff

## Status and provenance

Seven of ten requested canonical representatives were captured. Three remain pending a UX reproduction/scope decision; no substitute image was supplied for them. This report is developer capture evidence, not UX conformance or final security/Windows acceptance. Images remain uncommitted. The UX-owned screenshot manifest was not edited.

- Production source: `a985ada14a3f514c0158d85a9bc8946c1c00dc03`.
- Repository head during capture: `77f847ab70d53b8562efb56db0a52dc47e6884c8`, branch `feature/public-dedicated-server`.
- Worktree was not clean: four unrelated uncommitted tester paths existed before capture, followed by these image changes. No production source was changed. Captures use the separate frozen artifact, not a worktree build.
- Container: `duel6r-public-a985ada`, ID `5405fa422b9c3ec1161e0dff117431c5184c530fe583113c931cf38579dc6120`.
- Direct image: `sha256:ebbdd9376fc3ffefc833c7222f2975aa7774217e2600d08c5f4b36670e436b60`.
- Environment: Ubuntu 24.04 Docker, network mode `none`, Xvfb, Release/gl4/Lua ON, `LIBGL_ALWAYS_SOFTWARE=1`, `SDL_AUDIODRIVER=dummy`. Standard font/input presentation. Not physical-GPU or Windows evidence.
- Actual GUI SHA-256: `1e72ea10403d93b846dd0d3b9c91fbbc94664ac8d4a5b9e6aab290dd0870f80b`.
- Actual server SHA-256: `7793826160da97d9e08219df2fdf6de5b7ce5ee85f3c0df342278b4037f8bafa`.
- Tester runner SHA-256 matched supplied `e5572f9cdbc9e00168bd291a6ed2bd824234f550170ef077c3295dfafcb4fe6e` before use.
- Matching gameplay-content manifest retained at `/tmp/opencode/public-gui-content-manifest.json`, SHA-256 `eea315efabe105e8edac66900bb6c82da5d8156b4eb528af5d6cdacb8bbf7ee5`. All four fixture copies were checked against it before teardown.
- Full runtime asset manifest: container `/runtime/linux-x86_64.sha256sums`, SHA-256 `47b967fdb0bb12e97394564906b7c3c0548cfc257d7cf7fb724791270fdb6542`.

Captures are actual full-client ImageMagick `import -window root` PNGs from fullscreen SDL GUIs, copied unchanged with Docker. No cropping, retouching, compositing, fabricated overlays, forced outcomes, or injected application state was used. The temporary asset contact sheet was only used to identify backgrounds visually; it is not a GUI screenshot.

## Connection setup

Actual Public (encrypted) GUI connections used loopback `127.0.0.1`: Host route 26660, G guest route 26661, J guest route 26662; all reach the real dedicated backend on 26669 through mandatory PROXYv2. The negative route 26663 used a certificate signed by the same explicitly trusted disposable CA but with the wrong endpoint identity. Per-process CA paths were supplied by the tester launcher, with no system trust modification or verification bypass.

The invitation was supplied through the fixture's protected clipboard command and explicit Ctrl+V. No invitation/reconnect value was printed or included in metadata. Each GUI used two fixture persons (H1/H2, G1/G2, J1/J2), each assigned the actual `K1: Arrows` preset. Initial overly fast input batches missed UI transitions/modifier state; subsequent setup used deliberate pointer focus and held Control. Failed setup attempts were not represented as admission.

## Canonical captures

All times below are capture-start UTC on 2026-09-17. Paths are relative to `/devel/duel6r`.

| Entry | State and actual setup | Viewport | Time | Path | SHA-256 |
|---|---|---|---|---|---|
| SS-015 | NET-01-PUBLIC-ENTRY: fresh Main menu -> F2; Connect initially focused, no connection | 1920x1080 | 08:50:15.734910045 | `docs/design/screenshots/NET-01/NET-01.png` | `49dbb5bb8f3c41287f99f7c98e96194c3cf90d89b3b1511985322528de461b2c` |
| SS-017 | NET-03-PUBLIC-EDIT: production default `duel.netusite.cz:26660`; two local players; protected invitation pasted, masked field focused; no Connect request | 1920x1080 | 08:50:18.169619369 | `docs/design/screenshots/NET-03/NET-03.png` | `e719c6f6b2c85f8c262af19511479357b77776ae64add0875a12ae3a36115a3a` |
| SS-018 | NET-04-PUBLIC-CONTROLLER: actual first-admitted Host on 26660, three participants/six players; Host and G guest Ready, J guest (Guest 2 in this session) not ready; Start disabled | 1920x1080 | 08:56:24.167670356 | `docs/design/screenshots/NET-04/NET-04.png` | `516323aa2e62df68f59153132f24710193c49864b4b23a8d3d1a23ae6b052904` |
| SS-026 | NET-05-C: actual G guest opens Leave confirmation in a six-player public Deathmatch; not accepted; later cancelled with Escape | 1280x900 | 09:02:27.894142552 | `docs/design/screenshots/NET-05/NET-05-C.png` | `4ce0bd1a31f2f7f51acd2f92c3e558598b307557e79d1b99fc8f057d6d62e5ab` |
| SS-021 | NET-07-PUBLIC-RECONNECT: drop admitted Host route during that match; real retained arena, Host label, 30 seconds remaining, End session focused; route resumed immediately afterward | 1280x900 | 09:02:30.477881444 | `docs/design/screenshots/NET-07/NET-07.png` | `bff26ca89595f06253d619093585a2552859952d01e2a59216356fc90fc9300d` |
| SS-022 | NET-08-PUBLIC-SECURITY: actual G GUI attempt to trusted-CA/wrong-identity route `127.0.0.1:26663`; exact secure-failure text, Edit setup initially focused, no Retry | 1920x1080 | 08:57:40.521644196 | `docs/design/screenshots/NET-08/NET-08.png` | `3038446b1c489cd14a7238ad6ad5b852d50a7ba56c86f02a4aed54d3873cc452` |
| SS-023 | NET-09-PUBLIC-CONTROLLER-END: connected G guest receives genuine intentional End after controller GUI End action and confirmation during the six-player match; retained arena and Return to Network | 1280x900 | 09:03:46.696959503 | `docs/design/screenshots/NET-09/NET-09.png` | `58c5197fed6ad2935b7d4f3302407a516ded3f41a4ea4787ae63cfbbc79fc4bc` |

The six-player arena session was created separately at the 1280x900 startup size, after intentionally ending the menu-only session. Settings selected through the controller GUI: Deathmatch, fixed `levels/duel_16.json`, one round, Assistance on, Quick Liquid off, Burnable Trees on. All participants used Ready before controller Start. No GUI was resized during this session. No exact canonical session ID/tick or mirroring trace was collected; do not infer these from the raster.

## Background and resource identity

- SS-015/017/018: `textures/menu-backgrounds/forest-foundry.png`, confirmed by the same GUI's console diagnostic. SHA-256 `fa3d12b5dac0508d44596671d54ecdd999b87772b4a58d743bbe25d335b1fef4`.
- SS-022: `textures/menu-backgrounds/alpine-flood.png`, confirmed by that GUI's console diagnostic. SHA-256 `45be6836764a5e1b0d36aa19abfb884335454a67760dd5b1f6272a56cb49984e`.
- SS-026/021/023 arena: visually matches `textures/backgrounds/006.tga`, SHA-256 `68ef999d2704ec08615b981327ba17a4d3e567f318415a4024bae2ac6925129e`. This is visual asset identification, not a runtime filename/identity trace.
- `levels/duel_16.json`: `8ba7a10e56537b76a7b7bf6ac2dff3eb82f892afb8377c4553f24e3fda5ec33c`.
- Font `data/font.ttf`: `a049191eea6f3c2427fb2f61906e59fd91c0735a677fb47c6d6db5ce37d028b8`.
- Boundary match uses fixed Duel 01 with four players. Its background visually matches `009.tga`, SHA-256 `e562a62d40cbcf051b05edf068ad1d2b5bddaa7b1af0050f374f63d8fc986de4`.

## Minimum-viewport observations

Fresh actual GUI processes ran at 850x700 (Host) and 1280x720 (G guest); these are not resized screenshots. Observations: explicit paste rendered a horizontally scrolled mask and focused underscore without exposing the invitation; Public/LAN switching removed the invitation row and Tab traversal skipped it; pointer endpoint/port edits and a fresh protected paste permitted actual admission. Public Host/Guest roles and settings stayed in their separate regions. Clicking guest host-settings did not grant focus/editability; four Tab steps from its first owned Person moved focus to Ready, skipping host controls. Both became Ready and the Host started an actual four-player match. Guest Leave and Host End confirmations showed different consequences, with visible Cancel/action controls. Escape cancelled confirmations. Dropping/resuming the Host showed real positive recovery; confirmed controller End produced the guest's host-ended panel. Both sizes displayed genuine wrong-identity security failure, with Edit setup focused and no Retry. Editing the endpoint after recovery to setup cleared the invitation; `staging.duel.netusite.cz` fit at 850x700 and Connect became disabled. No connection to staging was requested.

These are focused developer observations, not complete viewport or controller-device coverage. Maximum-player/extreme-result and maximum-length input coverage was not established by this capture session. UX owns conformance assessment, including spacing/focus outlines.

Supplement paths are under `/tmp/opencode/`. Times are PNG creation UTC on 2026-09-17.

| File | Viewport | UTC | SHA-256 |
|---|---|---|---|
| `public-boundary-850-join.png` | 850x700 | 09:05:25 | `8acebb15119585606699964cd8a8c763051eb62108863c7efbd957b985342532` |
| `public-boundary-850-lan.png` | 850x700 | 09:06:28 | `f82813a66326e664ae01a3820e9b7072934a055db57bcf97040b72c6402052a2` |
| `public-boundary-850-lobby.png` | 850x700 | 09:07:39 | `4ef2aa3fb891d07cddf61c229da9708eff4877250acc81936cdf02cdf9bb88bf` |
| `public-boundary-850-reconnect.png` | 850x700 | 09:08:41 | `71ec552c5fad0e6949bd6796442a7bc87fbe6fd8d3a3c79124e5d1eab17c8e59` |
| `public-boundary-850-end.png` | 850x700 | 09:12:11 | `9af0de09ec7cbc8470c1ecfafb44c3d7794dc7768f581a887c8e64df0c42056d` |
| `public-boundary-850-security.png` | 850x700 | 09:13:11 | `58fee0dba286714539322bb332561f7fed03a47d4171a9064594b329e0b67f93` |
| `public-boundary-850-edit.png` | 850x700 | 09:14:52 | `28cdc233b93570387599c5d482dfe3522c9203aef61dda10c998df1e752823cc` |
| `public-boundary-1280-join.png` | 1280x720 | 09:05:26 | `944deb22ea56eee80fecdafb116231644d26786120b454398b9bb0d94e420214` |
| `public-boundary-1280-lan.png` | 1280x720 | 09:06:30 | `e864f1cbbac16180eb728299540cb25a3fbb0e7e8827fab8d976624be1dcf345` |
| `public-boundary-1280-lobby.png` | 1280x720 | 09:07:40 | `d5f7815fbd420b5cb7795947aa4c1033e5cef67f9d879670331bba47cac7371c` |
| `public-boundary-1280-readonly.png` | 1280x720 | 09:08:35 | `90c5918cd59cbc5c6c7ecf84a05a950f18d1a1e1ce3cb8805ccb80ff21f99c26` |
| `public-boundary-1280-confirm.png` | 1280x720 | 09:08:39 | `5548f2b68ce1624e45c7bcb00a46df5daa0734fce0e19d548248227be767aaf5` |
| `public-boundary-1280-ended.png` | 1280x720 | 09:13:01 | `143de1b3536b6ed0fd788744c13c571dca67767ae0b265a06a2375da19ef0fec` |
| `public-boundary-1280-security.png` | 1280x720 | 09:14:50 | `a639984c5de3b6e24459fe56762ee7a97409cdee160afbe662cb45b5309149d5` |

## Pending UX decisions

- SS-019 / NET-05: six-player real play was reached, but an actual Invisibility pickup with held weapon during degraded recovery was not reproduced. No passive canonical-state observer was provided to verify the required bonus/weapon/alpha combination. No canonical NET-05 image was supplied.
- SS-020 / NET-06 and SS-025 / NET-04-R: the real fourteen-winner, maximum-name result followed by five departed winning guest players was not reproduced. The handoff supplies the extreme roster, but not a reproducible gameplay-input sequence to obtain that result. No simpler summary or retained lobby was substituted. Existing historical files remain untouched.
- Decision requested: provide a reproducible normal-gameplay procedure and required observation support, or have UX approve a revised representative before capture. No implementation/specification change is proposed by developer.

## Uncommitted tester files: provenance not accepted

The four pre-existing tester changes remain unmodified and uncommitted by developer. They add portable TLS/native trust tests and public authorization regressions. The Windows CTest registration supplies `--allow-current-user-test-root`; the new test source adds and removes a disposable CurrentUser ROOT certificate. This platform-sensitive behavior was not executed and needs tester/team provenance and safety confirmation.

Observed hashes (identification only, not approved integration):

```text
81ae6859547566733d03020c69ca6205b09f44778ce52632d2d2471117076745  tests/PublicDedicatedProcessTests.py
4444469434905f70573b7d3f7204eb73a1f02655eb7fb50f39b6b02edade6848  tests/PublicDedicatedRuntimeTests.cpp
e9ca319da15c48ba2efe0cefd693ec8817d9754c6ebd0f64b097f5c523acd603  tests/SessionTransportCTestRegistration.cmake
fd1b606a034e1157379dc9c1e17b56820f86850a2b00bb1776c2e7c92da10cf0  tests/PortableTlsTests.cpp
```

## Teardown and next gate

Fixture `ctl stop` completed. The control socket disappeared; the temporary fixture root and runner were removed, including invitations, CA/private keys, copied GUI/server content, display and clipboard setup. All six fixture TCP ports (26660-26664 and 26669) were checked closed. Frozen `/artifact`, `/runtime`, runtime archive, and retained container remain available; the original binary manifest still passes. No cloud operation, build, source edit, test integration, commit, or push occurred.

Developer retains cleanup ownership of temporary capture evidence after UX consumption. Tester may clean its outside-repository helper copies now that fixture teardown is confirmed. Security evidence and real native-Windows TLS execution remain open readiness blockers; screenshot observations do not resolve them.
