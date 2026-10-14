# Corrected public GUI capture handoff

## Status

Six canonical representatives were captured from the accepted corrected production artifact. Four previously accepted representatives are reused unchanged with their original provenance. All images remain uncommitted for UX assessment. This report does not declare visual conformance, native Windows acceptance, live deployment, or Ready-for-review completion.

No source, test, fixture implementation, or UX-owned manifest was edited. No build, commit, push, cloud operation, or system trust modification occurred. Team has not yet conveyed the final `074bfe` infrastructure review PASS, so development push remains on hold.

## Immutable source and environment

- Production source: `a69a5adfd8fadb95e2024179bb5ecbd6ab3a62ad`.
- Accepted QA/integration checkpoint: `424c4bde8a945f010432b554ad6de4e8c1ddc422`.
- Capture checkout: `074bfe13ef214cea6a61833f2666759c9ab007f4`, branch `feature/public-dedicated-server`. The worktree contained the earlier uncommitted images and then the new captures; no uncommitted application changes were used.
- Container: `duel6r-public-424c4bd`, ID `f64212120718e33dc917325fed471768ff3ef0fa032f748333f9175d9fa73659`.
- Direct image: `sha256:fb07d2d66657b2d8b3e3a85e459cfff90d2075a35875dfb740cd3f38a07b7962`.
- Bundle: `/runtime-a69a5ad`, not the older `/runtime`.
- GUI SHA-256: `8d2522f14f083befdbef596a5ff2b4ca7cc44ad01446b50588800158dc22b947`.
- Server SHA-256: `7793826160da97d9e08219df2fdf6de5b7ce5ee85f3c0df342278b4037f8bafa`.
- Runtime archive SHA-256: `c827c836172824e4d6ef7a1e1008c7f329b1857257566e3a219c01579eb8caba`.
- Environment: Ubuntu 24.04 Docker, network `none`, Release/gl4/Lua ON, Xvfb fullscreen SDL clients, `LIBGL_ALWAYS_SOFTWARE=1`, dummy SDL audio, standard font/input presentation. This is software-rendered Linux evidence, not physical-GPU or Windows evidence.
- Tester runner: `/tmp/public-gui-fixture-424c4bd.py`, observed SHA-256 `fb81b130194cae162992e32270b8f2d810102470f26bec8bf20f267495bacd6f`.
- Per-process test CA was supplied by the launcher; certificate verification and invitation enforcement remained enabled. No private CA, key, invitation or reconnect credential is included in this report.
- Matching gameplay-content manifest retained at `/tmp/opencode/recapture-content-manifest.json`, SHA-256 `eea315efabe105e8edac66900bb6c82da5d8156b4eb528af5d6cdacb8bbf7ee5`. Actual copied files for server and all GUIs were verified against it before teardown.

Images were taken with `import -window root` and copied unchanged. They show full client areas without cropping, retouching, compositing, fabricated overlays, result injection or forced game-state edits. Normal person creation, setup, controls, gameplay and menu actions were used.

## Canonical captures

All capture-start times are UTC on 2026-09-17. Paths are relative to `/devel/duel6r`.

| Entry | Actual state and role | Viewport | Capture start | Path | SHA-256 |
|---|---|---|---|---|---|
| SS-017 | NET-03-PUBLIC-EDIT; production default `duel.netusite.cz:26660`, two local players, masked protected invitation, Invite focused; no Connect request | 1920x1080 | 10:16:19.419089345 | `docs/design/screenshots/NET-03/NET-03.png` | `a27bc8dc14d3fe083360aab59dee893850fa595454a49ca67fcb248c2da35f44` |
| SS-022 | NET-08-PUBLIC-SECURITY; real trusted-CA/wrong-identity attempt to `127.0.0.1:26663`; Edit setup focused, no Retry and no generic host-running helper | 1920x1080 | 10:17:23.014955934 | `docs/design/screenshots/NET-08/NET-08.png` | `91c04703e12b15eaeaaee8725772821a8ca3e35a9e6b367d37902e9a8a8d6a1b` |
| SS-019 public profile | NET-05-PUBLIC; actual Host active Deathmatch, two participants/one player each, no modal; public connection and session-only notices visible | 1280x900 | 10:19:50.793742796 | `docs/design/screenshots/NET-05/NET-05.public.png` | `94f8c4c69ce0f75c055bbfa9209ada2780bd3164b0db2981c8cb025002ff5aca` |
| SS-020 public profile | NET-06-PUBLIC; actual completed one-round result, Host role, Host One the winner; outcome and no-persistence copy with Return to lobby/End actions | 1280x900 | 10:23:10.826953528 | `docs/design/screenshots/NET-06/NET-06.public.png` | `de64242e437288f3737f407067691078d5c480b20dbe61d09dc45fcf9f7a9e14` |
| SS-025 public profile | NET-04-R public; same Host/session/process/startup viewport after Return to lobby; completed result retained, both participants Not ready, outcome rows at top and scroll region focused | 1280x900 | 10:24:17.300284302 | `docs/design/screenshots/NET-04/NET-04-R.public.png` | `1d4a0f3fae2c4e4f70b860247577302f516c148428994cac2fc0292eb3968b77` |
| SS-021 | NET-07-PUBLIC-RECONNECT; admitted Host route actually dropped during a subsequent active match; 30 seconds displayed, both active-match notices, Host consequence and End action | 1280x900 | 10:25:47.082572966 | `docs/design/screenshots/NET-07/NET-07.png` | `50d933c5d6fe25a41700b573209060db3aa217a47f1f1912057d4fd8876dced7` |

SS-017 used the normal GUI Add action to select Host Two, assigned K2: WSAD, alongside Host One with K1: Arrows. This remained an unconnected production-default setup. It did not contact the public domain.

### Actual ordinary outcome sequence

After the setup/security captures, both GUIs were closed and the helper's ordinary local roster restored: Host One and Guest One, one selected player per participant. Fresh GUIs started at 1280x900. The intended Host connected first to `127.0.0.1:26660`, followed by Guest through `127.0.0.1:26661`. Both used real TLS/proxy admission and K1: Arrows controls on their separate displays.

The confirmed setup was Deathmatch, fixed shipped Duel 01, one round, Assistance off, Quick Liquid off, Burnable Trees on. Both became Ready; Host used Start. SS-019 was taken before any outcome. An initial degraded indicator was actually visible; this ordinary profile makes no legacy degraded/Invisibility claim.

Guest One moved Left for approximately 1.5 seconds, landed on a lower ice step, then moved Left for approximately 0.8 seconds into the water. The actual environmental death and normal round transition produced Host One's win. Neither disconnect nor End session was used to create the completed result. The visible completed result records seed `3355622747484268544`, round 1, Duel 01, Mirrored, Host One/player 2 as match and round winner. No exact canonical session ID/tick trace was collected.

SS-020 was captured at the initial result position. Keyboard scrolling reached rows 3-15/15 and columns 10-103/103 in the actual result. Host then used Return to lobby without restarting, resizing or beginning another match. The retained-result scroll was returned to rows 1-3/15 and columns 1-94/103 for SS-025, then exercised to rows 13-15/15 and the rightmost columns. This is ordinary completed-result evidence only; historical extreme outcomes remain separate and untouched.

Only after the summary/retained pair was finished did both participants become Ready for another actual match. Host route interruption supplied SS-021; it was resumed before expiry. No service cause was invented from socket closure.

## Reused accepted representatives

These four files were not recaptured or modified. Their source remains `a985ada14a3f514c0158d85a9bc8946c1c00dc03`, not a69a5ad. Their original metadata is in `/tmp/opencode/public-gui-capture-report.md`, SHA-256 `6b2f4736003d8d0d98cff81f55f6058622f7334142c773e7847946cf4ab5835d`, and the UX assessment in the repository. Hashes were rechecked unchanged.

```text
49dbb5bb8f3c41287f99f7c98e96194c3cf90d89b3b1511985322528de461b2c  docs/design/screenshots/NET-01/NET-01.png
516323aa2e62df68f59153132f24710193c49864b4b23a8d3d1a23ae6b052904  docs/design/screenshots/NET-04/NET-04.png
4ce0bd1a31f2f7f51acd2f92c3e558598b307557e79d1b99fc8f057d6d62e5ab  docs/design/screenshots/NET-05/NET-05-C.png
58c5197fed6ad2935b7d4f3302407a516ded3f41a4ea4787ae63cfbbc79fc4bc  docs/design/screenshots/NET-09/NET-09.png
```

## Minimum-viewport observations and supplemental inventory

`/tmp/opencode/public-gui-recapture-inventory.json` records each new canonical and all 38 supplemental images with its path, SHA-256, actual viewport, PNG creation UTC, and common source/environment. Inventory SHA-256: `c44e7677666b98f868e47675d314772496869e86cc7c544ff0fb1aa7ec70525f`. Supplements remain outside the repository pending UX disposition; they are not additional default wireframe representatives.

Fresh clients were started at 850x700 and 1280x720. No image was resized. All paths below use `/tmp/opencode/recapture-<size>-<suffix>.png`, where size is `850` or `1280` (1280 means 1280x720).

### NET-03 input and Persons states

Using the normal main-menu text field and Add button, six ordinary persons (Person03 through Person08) were created for each GUI, giving eight available persons while initially selecting only the original one. No persistence file was edited directly.

- `public-row`: Public edit, empty Invite, first Persons row focused; field boundaries/selector marker visible while unfocused.
- `scrolled`: Public edit, eighth person focused after seven Down steps; the six-row window shows Person03 through Person08. Focus frame and footer are distinct.
- `pointer`: Clicking the second visible row in that scrolled window selected Person04, not another person. The screenshot shows Person04 selected and assigned K2: WSAD. It was then removed through the normal Remove action to restore one local player.
- `lan-row`: Explicit Private LAN edit, first Persons row focused after three Tab steps. Invite is absent and skipped; the trusted-LAN note remains in its reserved region.
- `lan-scrolled`: Same eight-person list in LAN mode with the eighth person focused. These are not LAN connection attempts.
- `max-input`: Public edit with an entered 253-character ASCII hostname (label lengths 63/63/63/61) and 256 repetitions of a non-secret keyboard character in Invite. The mask/insertion indicator stays within the field and the setup shows Ready to connect. No Connect action or DNS resolution was requested for these dummy values.
- `overlength`: One additional keyboard character was rejected; the fixed input-validation message and disabled Connect are visible. No invitation value was printed or submitted.
- `empty-invite`: Editing Port to 65535 cleared the invitation and its validation state. The 253-character endpoint and five-digit port remain contained; the now-empty Invite has a visible boundary and Connect is disabled with Enter an invitation.

These checks were executed independently at both sizes with keyboard and pointer input. No physical controller-device coverage is claimed.

### Actual boundary sessions and recovery

Both roles were exercised at both sizes by using two distinct real sessions, not authority migration:

- Boundary session A: GUI `host` on :91 at 850x700 connected first and controlled Host One. GUI `guest1` on :92 at 1280x720 joined as Guest One. Each had one K1: Arrows player. The first `active`, `850-host-reconnect`, `850-isolated-end`, `1280-guest-reconnect`, `850-summary`, `1280-summary-guest`, `850-retained`, and `1280-retained-guest` files belong to this session. Recovery was resumed before expiry. Normal Host One movement into water produced Guest One's actual win; the result shows seed `12182773237827809280`, Duel 01 Normal, Guest One/player 4. Both summary and subsequent same-session retained lobby were captured at their unchanged startup sizes.
- That session was intentionally ended through its controller GUI. Boundary session B admitted GUI `guest1` first on route 26661, making it the new service-confirmed Host at 1280x720; GUI `host` then joined on route 26660 as Guest at 850x700. Names do not confer authority. The `1280-new-controller`, `1280-active-host`, `1280-host-reconnect`, `1280-isolated-end`, `850-guest-reconnect`, `1280-summary-host`, `850-summary-guest`, `1280-retained-host`, and `850-retained-guest` files belong to this new session. After recovery, ordinary Right movement by Host One (now a Guest) caused an environmental death. The actual completed result shows seed `10548663201201477632`, Duel 01 Normal, Guest One/player 2 as winner. Return to lobby came from the actual controller without restart/resize.

Both boundary sessions used actual two-player Deathmatch, one round, fixed Duel 01, Assistance off, Quick Liquid off, Burnable Trees on. `host-reconnect` and `guest-reconnect` are real active-round transport-loss states with positive countdowns and both required notices. Host also retains its control-loss consequence and End action. `isolated-end` is the actual disconnected Host's confirmation, with the request-or-expiry consequence. Escape cancelled each confirmation before resuming the route. The underlying recovery action remains partly visible below the confirmation; UX should explicitly assess that layered presentation. No conformance is asserted here.

Finally, each size exercised a real wrong-identity attempt on 26663. `security` shows the fixed public security outcome, focused Edit setup/Return actions, no Retry and no generic host-running helper. No new live endpoint or public deployment was involved.

The two unnumbered supplements `recapture-summary-scroll.png` and `recapture-retained-scroll.png` belong to the canonical 1280x900 Host sequence and show actual result access at the bottom/right limits.

## Background and runtime-asset identity

Menu backgrounds were identified from the actual GUI console diagnostic in the corresponding process:

- SS-017: `forest-foundry.png`, SHA-256 `fa3d12b5dac0508d44596671d54ecdd999b87772b4a58d743bbe25d335b1fef4`.
- SS-022 and the 1280x900 ordinary Host summary/retained pair: `jungle-channel.png`, SHA-256 `9f3f57ed5e8b68f50a791fdacfc2a650f9cbcbf62746237586ba50d07f1d6769`.
- Both boundary processes: `alpine-flood.png`, SHA-256 `45be6836764a5e1b0d36aa19abfb884335454a67760dd5b1f6272a56cb49984e` (covered by the full menu canvas at 850x700).

Arena asset identification is visual, not an instrumented runtime filename trace:

- Canonical NET-05 public: `004.tga`, `3e35720a667bd551847ed2f8f34066c75ac0feea2731b211c8f1fd7bc8ba4d6b`.
- Canonical NET-07 subsequent match: `011.tga`, `96ffc0be34cb8688ed1b6fce19f2e0bff214cc27ab26dea796f62348b6fffbba`.
- Boundary session A: `007.tga`, `44de842cb6eef7bbd36fc7f343e382fe035f9c5faa271e0e997982a1bd3e38f2`.
- Boundary session B: `003.tga`, `026f467a8e6de4ba28091333e523ca3716b27e8397832909acdfb37f81372e65`.

Shipped `levels/duel_01.json`: `71fe679c5008df120a700e687c0a4506cf16c6f09bb9d7232800e6814e1bcc2b`.
Font `data/font.ttf`: `a049191eea6f3c2427fb2f61906e59fd91c0735a677fb47c6d6db5ce37d028b8`.
Full runtime asset manifest `/runtime-a69a5ad/linux-x86_64.sha256sums`: `4c7ec3c33a0230e681d4c5a7e40ae081355374cdd89f502fbc57a9a50e5ea402`.

## Teardown and remaining gates

The fixture's stop command completed. Its control socket disappeared, all six fixture TCP ports (26660-26664 and 26669) were checked closed, and its temporary root and runner were removed. This removes disposable invitations, CA/private keys, copied GUI/server content, GUI/display and clipboard processes. Frozen `/artifact`, `/runtime-a69a5ad`, archive and container were preserved. Both binary and production hash checks still passed after teardown.

Developer owns temporary capture-evidence cleanup after UX consumes it. Tester may remove its outside-repository helper copies now that teardown is confirmed. The six new representatives and four reused files remain uncommitted. Legacy extreme-scenario images and the UX-owned screenshot manifest were not changed. Team's reported Linux tester/reviewer passes are separate evidence; this capture report does not establish native Windows execution or hosted deployment. Await UX assessment and the explicit final infrastructure review handoff before any development push.
