# Physical Raspberry Pi 5 verification

## Checkpoint, environment and review

Implementation checkpoint: `5cd69ff0c588656053994de9a084f18a8073f90d`, on
`feature/raspberry-pi5-arm64`, [draft PR #104](https://github.com/mkapusnik-apps/duel6r/pull/104),
based on `develop` at `f2cfe66c1ad6ccf5a4427d44c1dc9d00ab9ecaac`.
The complete baseline-to-checkpoint diff was statically self-reviewed for
architecture guards, hardware-only AES, pre-initialization admission, x86
regressions, renderer defaults/overrides, package identities and tests before
physical behavioral evidence was collected. No independent review is claimed.

The authorized SSH target `rpi5.lan` identified itself as Raspberry Pi 5 Model B
Rev 1.0, 8 GB, four ARM64 cores, Debian 12 Bookworm, kernel
`6.12.47+rpt-rpi-2712`. CPU capabilities include ASIMD, AES and PMULL. Docker
Engine is 29.8.0 (`linux/arm64`). Both HDMI outputs were disconnected; no desktop,
audio server or physical controller was available. Existing host configuration
and workloads were not modified.

The exact Git archive was transferred into `/tmp/duel6r-pi5-5cd69ff/source`.
The canonical Dockerfile built natively on the Pi as `duel6r-pi5:5cd69ff`, image
`sha256:8941f6fdc5c0dbce2eeb52d9d6b1c336f888f38e366692543c4ab5434b9ed5e8`.
Configuration: Release, BUILD_TESTING=ON, RUN_TESTS=ON, Lua ON, no renderer
override (ARM selected GL1). The normal build entrypoint compiled the client,
server, resolver, supervisor and test targets. The build directory
`/tmp/duel6r-build-5Qea0J` was copied before the entrypoint's cleanup so focused
checks could reuse the same compiled artifacts without rebuilding.

## Native results and test corrections

Command on the Pi (all application work stayed in Docker):

```sh
docker run --init --name duel6r-pi5-build-5cd69ff --network none \
  -e BUILD_TYPE=Release -e BUILD_TESTING=ON -e RUN_TESTS=ON \
  -e CLEAN_OUTPUT_DIR=ON \
  -v /tmp/duel6r-pi5-5cd69ff/source:/workspace duel6r-pi5:5cd69ff
```

The complete CTest run passed **30/32** in 688.99 seconds. Both failures are
retained in this assessment, not erased by later success:

- `shared-arena-behavior` compared a completely black first menu baseline with
  a ready Predator menu (`hidden Team controls changed unexpectedly`). Its
  one-second wait began when SDL created a window, before asset initialization.
- `round-summary-progress-behavior` captured the preceding arena frame 250 ms
  after holding Tab and later rejected the absent SCORE strip.

The follow-up changes affect test synchronization only; application binaries,
resources, image assertion predicates and expected states are unchanged:

1. Both harnesses now boundedly await a non-blank first menu before input.
2. The round-summary harness collects eight early held-Tab PPM observations
   and defers their semantic classification until after the winner/continuation
   evidence is captured. It still requires the same SCORE strip and top-progress
   predicate, and the original final assertions still run. No image is altered
   to meet an assertion. An intermediate attempt to classify synchronously was
   rejected because it delayed observation past the first round.

Focused reruns used the retained Pi build at its original absolute path and a
separate source copy containing only these test corrections:

```sh
ctest --test-dir /tmp/duel6r-build-5Qea0J --output-on-failure \
  -R '^(shared-arena-behavior|round-summary-progress-behavior)$'
ctest --test-dir /tmp/duel6r-build-5Qea0J --output-on-failure \
  -R '^round-summary-progress-behavior$'
```

`shared-arena-behavior` passed in 270.89 seconds, including all eight mode/team
scenarios up to 15 players. The final corrected round-summary test passed in
91.53 seconds, including held Tab, non-final/final/unlimited and resumed matches.
Final tested shell blob IDs are `2d3b49e63e571a0c48416ffcfe7a79659764d571`
(shared arena) and `857ecc82fdb60b9300875a44d6ff135bb8a1e50b`
(round summary). These are
**30 original passes plus two focused corrected passes**, not a new uninterrupted
32/32 full run. Unaffected evidence is reused. The failed full entrypoint correctly
did not publish a Pi-built runtime bundle.

Native AES-128/256 key expansion/encrypt/decrypt vectors passed, as did separate
restriction-only removals of AES, ASIMD and all HWCAP bits. Secure transport,
password/replay/tampering/downgrade, admission, authoritative gameplay, replication,
session lifecycle and network runtime tests passed in the original native run.
Missing-capability injection on this AES-capable Pi is not a physical non-AES CPU
test or independent security assessment.

## Real GPU and CI bundle consumption

An isolated, unprivileged-user graphics container exposed only the existing
`/dev/dri/renderD128`, with render group 105. Its tool image added Weston 13,
Xwayland and mesa-utils to the build image:
`sha256:4080a10a799be383ce896211cf24f744b7390c8b107618b50c6bcbc10b9c77c0`.
The headless GL compositor used kiosk shell at 1280×900. `glxinfo -B` reported
**Broadcom V3D 7.1.7.0, accelerated: yes, Mesa 25.2.8-0ubuntu0.24.04.3**.
The application independently confirmed the same renderer through `gl_info`.
This is real VideoCore rendering with a virtual output, not HDMI verification.

The retained `duel6r-renderer-batch-tests` passed directly on V3D, without
`LIBGL_ALWAYS_SOFTWARE`: exact scalar/batch pixels, blending/depth/intervening
draws and GL error checks. Broader pointer automation could not be accepted on
the seatless rootless Xwayland: XTEST motion crashed that server; synthetic core
events did not reliably activate the mode selector. A desktop-shell attempt also
produced clipped window captures, corrected by using kiosk shell. These are
explicit limitations of the disposable headless fixture, not claimed full V3D
mode coverage. An optional Xephyr setup was abandoned after package downloads
stalled; no host display/input configuration was changed.

The successful native ARM [CI run 36772825547](https://github.com/mkapusnik-apps/duel6r/actions/runs/36772825547)
provided the candidate archive for the exact implementation checkpoint:

- Archive: `duel6r-linux-aarch64.tar.gz`.
- SHA-256: `642cbe96c4fabfe51b76c57669c4db7a76b8d4763c583f42ff5c189e84e132ab`.
- Game: `1da2c6401c6ba0be423c7de9325cac36831a874dfd6e044018b990f05a55a1ad`.
- Server: `2c5fa183ac4b56bc25073368e2722153fdf8eb229757f213c0908f8c0d2e095c`.
- Supervisor: `909290b42993c8ab791ca0ff5f97b761654f8cc52a736323551ca3d2ed9d16a2`.
- Resolver: `716c4e073a7725e8ab0d4944330a4807aa9ff11a3be11aca1c24514f69015961`.

On the physical Pi, both the archive checksum and **every entry** of
`linux-aarch64.sha256sums` passed. No x86 Linux inventory was present. The
unchanged bundle passed `duel6r-main-menu-smoke` in an isolated execution copy
under Xvfb/dummy audio. The same CI game binary then ran on V3D: two-player
Deathmatch setup, console, live arena, held-Tab score, return to menu and clean
exit, with empty stderr. Existing arena/score image assertions passed.
[Four original V3D captures and provenance](../design/screenshots/pi5/README.md)
are retained. They do not complete the full platform visual matrix.

Temperature observations ranged from 60.9 to 74.1 °C; sampled `get_throttled`
values were all `0x0`. The gameplay capture's FPS overlay showed 30 at 1280×900
with two players under the virtual compositor. This single observation is not
a sustained performance benchmark or a 15-player/HDMI frame-time guarantee.

## Hosted checks, gates and cleanup

At implementation SHA `5cd69ff0c588656053994de9a084f18a8073f90d`, native ARM CI,
Linux CI, native MSVC transport CI and `Feature Ready` all succeeded. The active
develop ruleset requires an up-to-date branch and `Feature Ready` for **Merge**.
The follow-up test/evidence commit needs its own hosted results; earlier green
checks must not be presented as results for a later head.

Specification status: ready. Visual gate: blocked (partial matrix only).
Product acceptance: Blocked by missing evidence. Ready-for-review gaps remain:
complete Pi V3D screen/interaction coverage, real audio and physical controllers,
full cross-machine graphical gameplay, and independent security review. The user
explicitly requested a draft; the PR must remain draft. No issue is closed and
no release or deployment is promoted.

Cleanup owner: implementing agent. All task-specific remote containers, tagged
tool images, temporary source/build copies and local scratch files are removed
after collecting this evidence. Shared Docker build caches are not pruned, and
unrelated host workloads, device permissions and configuration are preserved.
