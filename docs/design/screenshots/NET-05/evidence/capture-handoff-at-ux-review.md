# PR #87 developer capture handoff

This explanatory handoff was reconstructed after the interrupted session. The original captures, JSON sidecars, and runtime asset manifest were recovered unchanged from the stopped capture container. This is provenance, not a UX conformance assessment or a second canonical screenshot specification.

## Frozen checkpoint

- Branch: `bugfix/network-arena-orientation`.
- Accepted, pushed head: `84f63bc6fdee75444b06723b3c5f542546e27b15`.
- Production-source head: `78d97e5c9c4791262a81ffd4b0823671034ec872`.
- PR: https://github.com/mkapusnik-apps/duel6r/pull/87, open draft, base `develop`.
- Worktree was clean during capture. Subsequent changes are only four replacement PNGs and three supplemental PNGs, intentionally uncommitted for UX.
- Application SHA-256, previously verified across all four execution copies: `394281ccbe19b207cdf97d9dd025bc95350a8a194651df9bb6061982e1274198`.
- QA image: `duel6r-orientation-qa:84f63bc`, ID `sha256:339cf595c6a1896ae9d92af760f7c577fa426d9c89495854b71e6b3ea9c5d1ef`.
- Restored archive: `/tmp/opencode/duel6r-orientation-84f63bc.tar.gz`, SHA-256 `bc7ae4b4c49f232d1d3a630c479e768d434aa7bdb87a638535eb885e548f9174`.
- `captures.json` SHA-256: `ac7bada9a5e00b252f7033179e97835cec4f197c880549d7ea44d4c81687d4bf`.
- `runtime-assets.sha256` records 464 unchanged deployed runtime-content files across all four clients. Informational top-level text files were not copied into execution roots.

## Environment and real-state workflow

Release, GL4, Lua enabled, Ubuntu 24.04 Docker, SDL dummy audio, separate Xvfb displays. The actual application reports Mesa llvmpipe (LLVM 20.1.2, 256 bits), OpenGL 4.5 Core, Mesa 25.2.8. `gl-info.png` records the environment. These are actual rendered OpenGL frames in the documented software-rendering environment, not physical-GPU evidence or recording-renderer images.

Host display :91, guest :92, and Local Play :94 are 1280x900. Second guest :93 is 1280x720. No runtime resizing. ImageMagick `import -window root` captured each full client without cropping, compositing, retouching, or synthesizing content.

Persons were created and selected using the actual UI. Host owns Alder and Birch; the 1280x900 guest owns Cedar and Dogwood; the 1280x720 guest owns Elm and Fir. All six used K2 WSAD controls. Host selected Deathmatch, fixed shipped `levels/duel_16.json`, one round, Assistance on, Quick Liquid off, and Burnable Trees on; every participant confirmed readiness.

The host listened at container-local 127.0.0.1:26660. Owned transparent proxies at :26661 and :26662 forwarded guest traffic. A passive observer decoded forwarded replication bytes with the accepted library. It never sent game-state bytes or altered payloads. Only owned-connection delay/closure was used for network conditions. No source edits, content edits, forced seeds, state injection, or physics changes occurred.

JSON sidecars contain capture-start timestamps, viewport, hashes, canonical destination, and before/after observed state. Ticks bound nearby forwarded state; they do not assert the exact GPU-consumed update. Local Play does not expose network identities or a canonical tick counter; none were invented.

## Required images

All four share session `11662738353544004403`, match `144115188075855873`, round `216172782113783809`, `levels/duel_16.json`, actual `mirrored=false`, and default network visuals. No named level background exists. Production fallback selection recomputed from observed identities and the unchanged eligible list `000.tga`–`013.tga` is `000.tga`.

| Entry | Capture start UTC | Ticks before–after | Packet file | Canonical destination |
|---|---|---|---|---|
| SS-019 | 2026-09-14 23:53:53.078073 | 24156–24204, live | `ss019-final.png` | `docs/screenshots/NET-05/six-player-lan-degraded-1280x900.png` |
| SS-026 | 2026-09-14 23:54:42.931218 | 27150–27207, live | `ss026-final.png` | `docs/design/screenshots/NET-05/NET-05-C.png` |
| SS-021 | 2026-09-14 23:54:50.784866 | 27237, retained | `ss021-final.png` | `docs/screenshots/NET-07/reconnecting-24s-1280x900.png` |
| SS-023 | 2026-09-14 23:55:50.890231 | 31206, retained | `ss023-final.png` | `docs/screenshots/NET-09/host-ended-1280x900.png` |

SS-019: Elm, player 8, acquired naturally spawned Invisibility through actual movement/jump while holding a shotgun. The before record shows active Invisibility, remaining canonical duration 822532, and body alpha 51/255. Normal forwarding resumed after the owned delay at 23:53:52.199724 UTC; capture began 0.878 seconds later, with complete updates advancing and the degraded indicator visible. Actual Show status input was sent to the clients. Numerical alpha and temporal motion acceptance belong to independent QA, not screenshot inference.

SS-026: Escape opened the actual focused guest Leave confirmation, then Escape cancelled it without departure.

SS-021: The owned guest connection was interrupted at 23:54:44.384225 UTC. The screenshot visibly shows 24 seconds remaining over the retained arena. Contact was restored at 23:54:51.695144, before expiry. `reconnect-restored.png` at 23:54:54.695859 shows active connected play.

SS-023: Following restoration, host Escape opened the explicit End session confirmation (`host-end-confirm.png`); Return intentionally confirmed it. The guest received the host-ended panel over retained arena state. No crash or timeout was substituted.

## Actual comparison images

- SS-019 is also the unmirrored network representative; no duplicate entry was created.
- `network-x-mirrored.png`, 2026-09-14 23:58:45.715733 UTC: actual six-player session `9745887772574650472`, match `144115188075855873`, round `216172782113783809`, ticks 216–264, shipped `duel_16`, `mirrored=true`, fallback `009.tga`. Normal session attempts continued until the authoritative random choice selected a mirror; no seed was forced. Destination: `docs/design/screenshots/NET-05/orientation-network-x-mirrored.png`.
- `local-round-a.png`, 2026-09-15 00:01:40.147197 UTC: actual Local Play, Hazel and Ivy, K1 Arrows, `map 0` selecting shipped `duel_16`; `local-console-a.png` records `mirror: false`. Destination: `docs/design/screenshots/NET-05/orientation-local-unmirrored.png`. Background visually matches unchanged `000.tga`.
- `local-round-f.png`, 2026-09-15 00:05:58.517477 UTC: same Local Play process and map; `local-console-f.png` records `mirror: true`. Destination: `docs/design/screenshots/NET-05/orientation-local-x-mirrored.png`. Background visually matches unchanged `007.tga`.

All comparisons are 1280x900. Local Play used normal progression/Shift+F1 to obtain the mirror states. Its exact numeric round/tick was not exposed in this uninstrumented capture. Local profile appearance and perspective side faces are existing behavior, not claims of network profile parity.

The long upper platform and lava shelf change horizontal side under the actual mirror while vertical orientation stays upright. Trees, player feet/supporting surfaces, readable names, weapons, and bottom water context are available for UX inspection. Static frames do not prove movement, culling invariants, or numeric opacity; independent QA supplies those gates.

## Ownership and remaining work

Capture container `duel6r-orientation-capture` is stopped. Temporary helpers, candidates, and execution copies inside it remain developer-owned for finalization cleanup. Preserve this selected packet and the accepted artifact through their final consumers. The capture session was not restarted on resume.

The original host-side prose README was lost when temporary exports disappeared; this file explicitly reconstructs its handoff information. Binary captures, JSON provenance, and the accepted archive were recovered from existing containers and their recorded hashes verified.

UX assessment and finalization remain pending. No screenshot manifest or production/test source was changed during capture. Historical PR #83 provenance sidecars have not been overwritten; finalization must reconcile them with the corrected packet. No ready transition is authorized yet.
