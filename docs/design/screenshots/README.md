# Current game screenshots

The [screen inventory](../../screens/README.md) owns screen identities; the [design system](../../design.md) and linked wireframes own visual requirements. This catalog contains **30 of 35** current representatives. Five rows have no current image. The catalog records visible states only; it does not close behavioral, temporal, constrained-viewport, or native-platform acceptance.

## Capture standard

Keep one original, lossless PNG per current wireframe at `<screen-id>/<wireframe-id>.png`, always at **1280 × 900 actual client pixels**. Use a supported startup display at that size and verify the display, application client, and PNG dimensions agree. Capture the entire native client with no external chrome, cropping, resizing, retouching, compositing, or injected state. Use real reachable states and ordinary input.

Replace a representative at its stable path when its appearance or representative state materially changes. Record the immutable source revision, runtime identity and profile, actual state and role where applicable, acquisition UTC, and PNG SHA-256. Retain the true producer for unchanged images; leave an unavailable representative without an image rather than substitute a stale or wrong-state capture. This standard does not replace verification at other required sizes or on other platforms.

<a id="current-representative-originals"></a>
## Current representatives

**Source and profile.** Except for the Browse rows noted below, originals use clean source [`dabdc89774962fac3e4951f46a6c5cfc6a18475a`](https://github.com/mkapusnik-apps/duel6r/commit/dabdc89774962fac3e4951f46a6c5cfc6a18475a), Ubuntu 24.04.5 Linux x86-64 Docker Release GL4/Lua ON, shipped font and assets, private 24-bit Xvfb with software OpenGL, dummy audio, and native keyboard/pointer input. The network worker reported Mesa llvmpipe (LLVM 20.1.2), OpenGL 4.5 Core; this is not a per-frame measurement. Local and owned network scenarios used disposable persons and real game outcomes.

- Application SHA-256: `2e1c7285135df60b5c052b2c5509059c058665b835eb5fa8db253be387f610ef`.
- 470-file runtime manifest SHA-256: `6d66bf859fa990a089e6b276c5b41045362ba87b8c6637bb7f8c82d9f9202f5e`.

Unless otherwise stated, times are UTC on **2026-11-26** at PNG write completion, not GPU presentation. Where only whole seconds are shown, the export retained only whole-second filesystem time.

<a id="current-local-play-and-menu-originals"></a>

| Representative | Captured state | UTC | PNG SHA-256 |
|---|---|---|---|
| <a id="current-ss-001"></a>[MENU-01-A](MENU-01/MENU-01-A.png) · SS-001 | Deathmatch menu after the actual 1/1 final; Rounds 1 retained. Ben and Ada ranked, Cora and Drew unranked; Team controls absent. | 09:21:53.468093742 | `21098f4f0997cf7cbeb6fed87c3d6ded02ab77038c90374d438bd8544c34e51b` |
| <a id="current-ss-024"></a>[MENU-01-B](MENU-01/MENU-01-B.png) · SS-024 | Same menu switched to Teams: two teams, Friendly Fire off, Rounds 1, colored assignments and centered `Equalize` / `Shuffle`. | 09:21:55.962033988 | `ee550195ed5ec73b9073a5feb4ccbfd97eb582bcb1c3813b79d46ef14adf25f2` |
| <a id="current-ss-002"></a>[MENU-02](MENU-02/MENU-02.png) · SS-002 | F3 after the same match, before dismissal: `Really delete? (Y/N)` over the four-action menu. | 09:21:54.495544605 | `b19682bdc0056b1226d8acc906ff0cc803f0e999eab7b5596b6b2493eaea7aa6` |
| <a id="current-ss-007"></a>[MODE-01](MODE-01/MODE-01.png) · SS-007 | Active three-player Duel 04 Predator; faint Predator with visible held weapon, two opaque Marines, and live ranking. | 09:21:21.778757573 | `0dfad01638134235fce37d8155a366c6ad2196f1a85bed4095d37d66bc1d546e` |
| <a id="current-ss-008"></a>[MODE-02](MODE-02/MODE-02.png) · SS-008 | Active mirrored Duel 09 Team deathmatch; Ada/Cal in Alpha and Ben/Dee in Bravo, matching headbands and named live groups. | 09:53:07.341576602 | `79ebbb021b33bb1fa6427d1c340519ef159544d5452b0388992edebb3b2af48c` |
| <a id="current-ss-003"></a>[PLAY-01](PLAY-01/PLAY-01.png) · AP-L / SS-003 | Six living players in Local Deathmatch 1/1 after the start fade. The native console confirms unmirrored Duel 16; water, rooted trees, supports, elevator and HUD are visible. This is the Local Play reference, not a matched network comparison. | 11:12:04.163134070 | `02d188473d1659ae80759af7b7ca3946b957b38c5ec7213ca9798cf503f74bce` |
| <a id="current-ss-009"></a>[PLAY-05](PLAY-05/PLAY-05.png) · SS-009 | Mirrored Duel 04 Quick Liquid Deathmatch; raised water and submerged living Bea with a partial blue air bar. Cal and Dee remain above water. | 09:52:07.130491296 | `edefba21531c9277c2f914bdbadb1b5fa4b150fc6e102bf5de99ebec57d74d0c` |
| <a id="current-ss-010"></a>[OVER-01](OVER-01/OVER-01.png) · SS-010 | Tab before the first outcome of Duel 04 Teams, round 1/2. Four groups, three separators, actual zero scores, live arena and no curtain. | 09:23:40.809484435 | `9b2c843b03edd41d21492d9613f87e3c61441e5643b7ac3c6b0cf6982b303791` |
| <a id="current-ss-011"></a>[OVER-02](OVER-02/OVER-02.png) · SS-011 | Actual non-final Duel 04 Local Teams result; Charlie wins with Cal and Gil still alive. Four groups and eight player rows, three separators, red curtain, and one backed bottom-center `Rounds: 1\|2`. | 11:12:30.104381704 | `8907737cfba33e64a4bcc30a22abeb805d67199765fe39e1f172b1a83c0e5e4a` |
| <a id="current-ss-012"></a>[OVER-03](OVER-03/OVER-03.png) · FS-L / SS-012 | Duel 04 Local Deathmatch 1/1 after the final wait; Ben +1 and Ada −1 PTS. Compact score table, separate final counter and `End of Game`. | 09:21:51.954370159 | `b08aa169df79400565373538adf9b41640fa935f8e1c4b4ac690742905a6b5d9` |
| <a id="current-ss-013"></a>[CONS-01](CONS-01/CONS-01.png) · SS-013 | Startup console over populated menu, blank input and four footer actions. Actual startup output names `alpine-flood.png`. | 09:20:42.494713480 | `3e1a957647c63b8c03a64dd298821d700e6212ab5acb4eb9ef5f232ec3d63fca` |
| <a id="current-ss-014"></a>[CONS-02](CONS-02/CONS-02.png) · SS-014 | Console over live Local Deathmatch on mirrored Duel 16; actual round output, blank input, and one retained arena. This is not the AP-L comparator. | 09:24:54.018417684 | `3598aaccfbc0d0dd13f89f18fa53bf04588b16cdee66767f31756af23d7bd48f` |
| <a id="current-ss-016"></a>[NET-02](NET-02/NET-02.png) · SS-016 | Editable Host setup with Elm/K1, focused Port `26660`, wildcard listening, header count, Password `Configure`, and Start/Back. | 09:27:32.496358352 | `7bf0de272b54e76801ea29ab92f892737bf6a0599b69886a36b4e23a6e9ba733` |
| <a id="current-net-02-la"></a>[NET-02-LA](NET-02/NET-02-LA.png) | Wildcard listening popup; `Listen on all` focused and checked, both actual address rows checked and disabled, wildcard help and `Close`. | 09:27:57.760951801 | `89fbf37849fe9a0685888e61440fd640100a7b9414c31f0b1c96673b1f2fbac9` |
| <a id="current-net-02-le"></a>[NET-02-LE](NET-02/NET-02-LE.png) | Explicit listening popup; loopback selected, eligible private address unchecked and focused, matching summary, explicit help and `Close`. | 09:27:59.493533119 | `590c2f251199926ada00e022a834e8bf5fc704fee3c796b98f1f81ddbdd451cc` |
| <a id="current-net-02-p"></a>[NET-02-P](NET-02/NET-02-P.png) | Genuine wildcard Host startup for Oak/K1, before readiness: Starting, ten-second help and sole focused Cancel. | 09:50:47.787182608 | `7c8c85e54bc0592737e49f7c5ca804622530d9721cbf4139b3bf4dd3eef83f1d` |
| <a id="current-net-03-p"></a>[NET-03-P](NET-03/NET-03-P.png) | Independent Direct setup, Fir/K1; focused Address, default loopback endpoint, blank optional Password and no attempt. | 09:27:36.179615664 | `9a8441e8e4fe40f668996e1ba62b326657059eca5348d8235aba02796f146de4` |
| <a id="current-net-03-e"></a>[NET-03-E](NET-03/NET-03-E.png) | Protected ongoing player-hosted listing; Ivy/K1, locked binding, required focused mask, next-round and host-confirmation copy before Connect. | 09:39:32 | `fb9efdd702f092016bcbab090a4d000098eb7bb367083f8796835d7295b3c533` |
| <a id="current-net-03-b"></a>[NET-03-B](NET-03/NET-03-B.png) | Protected ready-idle dedicated listing at zero occupancy; locked binding and focused mask, before Connect or role assignment. | 09:33:37.072605372 | `5543328415a18ee65358b307cf9d039a51cc545899ecf286c355147aadfe74da` |
| <a id="current-ss-017"></a>[NET-03](NET-03/NET-03.png) · SS-017 | Direct Connecting with Fir/K1 and Hazel/K2 locked. The owned peer completed the common-channel handshake without returning admission; Cancel is the sole action and has focus. | 09:30:55.319112194 | `c893b43e414313ef4b6f3fb7b092f38b49d907d1de08f5a654edfd5f74dc761b` |
| <a id="current-ss-018"></a>[NET-04](NET-04/NET-04.png) · SS-018 | Actual dedicated controller lobby: Ada is the sole ready player; mixed Team settings and enabled, focused Start match. | 09:33:34.268572017 | `7338fca14e8e77910c4641d9955c97b8f2a03f13ded65091563051e9e4950fe9` |
| <a id="current-ss-025"></a>[NET-04-R](NET-04/NET-04-R.png) · SS-025 | Same dedicated controller after Return to lobby from NET-06; retained Completed history names Ben separately from current membership, readiness and settings. | 09:40:20.302260341 | `f434477e3c2e6c803797e18aaae07b96a662fb4885233792c5408dd46a1d87ab` |
| <a id="current-net-05-r"></a>[NET-05-R](NET-05/NET-05-R.png) | Player-hosted Duel 04 Team deathmatch, frozen non-final `1\|3` before Advance. `Round outcome: Team Bravo`; Dee yellow, Cal red, no `Dead` suffix; separate actions, status and counter. | 09:35:49 | `ba8496a6ac43987ea6f0a2a313c3e3c94a5c4115acaa5baebcb3d58a4f267f14` |
| <a id="current-net-05-w"></a>[NET-05-W](NET-05/NET-05-W.png) | Ivy/K1 admitted by Direct Join into player-hosted active round 2/9. The undimmed Guest view shows Connected, Ivy marked Waiting with no owned actor, `Your players join next round.`, and `Leave session`. | 11:55:19.178291586 | `a53f4852b78d0b8b1ca77f70faf09ab84f9016a11e829becb69988c13f8cb9ca` |
| <a id="current-ss-020"></a>[NET-06](NET-06/NET-06.png) · FS-N / SS-020 | Dedicated controller after the final wait in actual Duel 04 Deathmatch 1/1. Ben +1 and Ada −1 PTS; compact cumulative scores, session-only result copy and focused Return to lobby. | 09:37:38.905348685 | `c95225e9e62c7506efaaad8d48a1688f208d9817fb2375d201a275a9a9a340a2` |
| <a id="current-ss-021"></a>[NET-07](NET-07/NET-07.png) · SS-021 | Dedicated controller-only interruption during active play. Retained arena, 29 seconds remaining, both active-round notices, and focused End session. | 09:36:16.076879858 | `c27251ed5d0b3d4abb36d3bff3d628ac221168d68bd5670c9de5b4b42c330fdb` |
| <a id="net-08-browser-password-rejection"></a>[NET-08](NET-08/NET-08.png) | Real wrong Password against the bound dedicated listing; `Connection not authorized.`, focused Edit setup and Return to browser, no Retry. | 09:33:43.250030180 | `c8070b87642578b3d3d533e15f1d0c981b8f6a7705e2641436b9e5c337a54375` |
| <a id="current-ss-023"></a>[NET-09](NET-09/NET-09.png) · SS-023 | Dedicated Guest after an accepted intentional controller End; retained arena, no resume or local persistence, sole focused Return to Network. | 09:41:17.171980681 | `0dbcffd730e78195e1926bcf8f09768343f27d09926bd8cb7252325bade47d94` |
| <a id="current-net-10-e"></a>[NET-10-E](NET-10/NET-10-E.png) | Successful empty read before publishers, without continuation. All six revised headings and the complete inline message are visible; the reason is blank, no rows or selection appear, Refresh is focused, and Join is disabled. | 2026-11-27 00:36:01.241079272–00:36:01.492828669 | `846a46306c6ff1f94630b2e28b9039b60d9d2171e3d6a807ab501a67cabaca9b` |
| <a id="current-net-10-u"></a>[NET-10-U](NET-10/NET-10-U.png) | Browser-only HTTP 503 on Refresh after a populated selection. All six headings and the complete inline error and reason are visible; no cached rows or details appear, Refresh is focused, Join is disabled, and Direct connect remains available. | 2026-11-27 00:39:33.225188178–00:39:33.628669170 | `f0e05204391618efdd73edc463b423bd84732bc30db6421625a752424c1a5e96` |

**Browse assessment and provenance.** The retained empty and unavailable originals were accepted for the visible Mode and Access change at their original producer, `d0c41dc515cfddf6eb6d3df50be8cc2f2afeb797` on `feature/browse-session-mode-access`. Both cover the full 1280 × 900 native client on Ubuntu 24.04.5 Linux x86-64 Docker Release GL4/Lua ON with shipped font and assets, private Xvfb, Mesa llvmpipe/OpenGL 4.5 Core, and native keyboard/pointer input. Application SHA-256: `7b2dced7c08b77fc8881227dbb1c16a4800a88aa5da70477ab7a5440746beb06`; 475-file runtime identity SHA-256: `176a0fa4c7eef6e3eed141f54968f2ac27274d28df5bdd6a15db73c264eac392`. UTC intervals run from acquisition start through PNG write completion, not GPU presentation.

These Browse images show the absence of `LAN-first.` in their producer. They do not establish conformance with that retained copy expectation or verify that a client clears names after they were shown.

## Rows without a current image

| Representative | Disposition |
|---|---|
| <a id="current-ss-015"></a>NET-01 · SS-015 | The [pre-roster original][pre-roster] does not show the public-name notice. See the [native capture matrix](../screens/NET-10.md#minimum-native-capture-matrix). |
| <a id="current-net-10"></a>NET-10 | The [pre-roster original][pre-roster] does not show the two-column details or connected-player listbox. See the [native capture matrix](../screens/NET-10.md#minimum-native-capture-matrix). |
| <a id="current-ss-019"></a>NET-05 · AP-N / SS-019 | Six-player degraded-but-Connected Guest play was observed on mirrored Duel 16, unlike the unmirrored Local Play reference. No qualifying original shows authoritative Invisibility on both body and held weapon with the opaque comparison. |
| <a id="current-ss-026"></a>NET-05-C · SS-026 | Withheld: part of the underlying reconnect `End session` button remains visible and clipped below the active confirmation. |
| <a id="current-net-05-s"></a>NET-05-S | Withheld: the pre-winner score panel clips `Network connection degraded.` in the three-row Listed Host view. The two-row Direct Guest view does not cover that overlap. |

## Archived evidence

- [Pre-catalog evidence tree][prior-tree], [assessment and focused coverage][prior-manifest], and [legacy SS-* records][legacy]. Earlier findings and provenance apply only to the original bytes and stated scope. For artifacts overwritten before that tree, follow the matching earlier pins in those records.
- [AP-N role, temporal coverage and open comparisons][arena-matrix] and [FS-G and other final-result evidence limits][final-matrix]. A representative still does not replace these observations or the separate public, role, recovery, and extreme-result requirements.
- [Accepted public-pilot snapshot and checksums][public-pilot]. Use the pinned checkout with its original checksum paths, not the current catalog.
- [Original Pi 5 V3D observations][pi5-originals] and the [physical verification record](../../verification/raspberry-pi5-physical.md). Physical V3D coverage remains incomplete. Windows and macOS verification retain their existing timing and scope.

[prior-tree]: https://github.com/mkapusnik-apps/duel6r/tree/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots
[pre-roster]: https://github.com/mkapusnik-apps/duel6r/blob/e5eb6c87f126a9f0acd1c7422aba68c9f7d49095/docs/design/screenshots/README.md#current-representatives
[prior-manifest]: https://github.com/mkapusnik-apps/duel6r/blob/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots/README.md
[legacy]: https://github.com/mkapusnik-apps/duel6r/blob/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots/legacy.md
[arena-matrix]: https://github.com/mkapusnik-apps/duel6r/blob/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots/README.md#shared-arena-verification-follow-up
[final-matrix]: https://github.com/mkapusnik-apps/duel6r/blob/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots/README.md#arena-parity-and-shared-final-presentation
[public-pilot]: https://github.com/mkapusnik-apps/duel6r/blob/a55ae3f3624de56d73b48f91b417f28c5a6d4a07/docs/design/screenshots/evidence/public-pilot/README.md
[pi5-originals]: https://github.com/mkapusnik-apps/duel6r/blob/dabdc89774962fac3e4951f46a6c5cfc6a18475a/docs/design/screenshots/pi5/README.md
