# Raspberry Pi 5 (64-bit Linux)

## Scope and acceptance contract

The approved target is Raspberry Pi 5 running 64-bit Linux, with Mesa V3D,
an X11/XWayland desktop, audio and keyboard/controllers. Local play, hosted and
joined network play, the server component and all packaged helper processes are
in scope. This does not promote experimental standalone server commands into a
supported deployment. Existing gameplay, screen and network contracts remain unchanged.
This document owns the platform requirements below; the cryptographic policy
remains in [network-directory-implementation.md](network-directory-implementation.md#password-and-credential-protection).

- **PI5-001** Build the client, server, host supervisor and resolver for Linux
  AArch64 using the supported Docker build. Package resources and an architecture-
  specific SHA-256 inventory. Never label an ARM binary as x86-64.
- **PI5-002** Default Linux AArch64 to the existing `gl1` renderer (OpenGL 2.0
  compatibility), preserving its full rendering behavior including wireframe.
  Keep the x86-64 default `gl4` and explicit renderer overrides unchanged.
- **PI5-003** Permit secure sessions on Linux AArch64 only when the OS reports
  ASIMD and AES capabilities. Check before any TLS, entropy or DRBG initialization.
  Use upstream hardware AES for both key expansion and block operations. Preserve
  x86 AES-NI, protocol/ciphersuite, authentication, record limits and fail-closed
  behavior. Do not add software AES or plaintext fallback.
- **PI5-004** ARM peers must exchange authenticated encrypted application data
  with existing x86-64 peers. Wrong passwords, tampering, replay, missing entropy
  and missing CPU capabilities must retain existing rejection behavior.
- **PI5-005** On a physical Pi 5, the packaged application must complete the
  existing local-player loop and a cross-architecture network match: menu,
  profiles/Lua, controller discovery, movement/combat, water/elevators, round and
  final summaries, persistence and return to menu. Exercise Deathmatch, Predator,
  Teams/friendly fire and representative 2/15-player rosters. Confirm hardware V3D
  rather than llvmpipe, audible audio, and responsive input. Record resolution,
  OS/Mesa versions, frame timing and thermal conditions; no unmeasured FPS promise.

Non-goals: Raspberry Pi 4, 32-bit ARM, ARM Windows/macOS, new graphics backends,
repairing experimental ES2/ES3, UI redesign, security-protocol changes, cloud
deployment and automatic publication into the existing mixed x86 nightly ZIP.

Specification status: ready. Full platform acceptance requires PI5-001–005;
cross-compilation or software-rendered screenshots alone are insufficient.

## Build and runtime baseline

The build uses Ubuntu 24.04 userspace. Use a native ARM64 Docker daemon on Pi 5
(or another AES-capable AArch64 host). No `-march=native` or global crypto ISA
flags are needed. The build chooses `gl1` automatically on Linux AArch64.

```sh
docker build -t duel6r-build:pi5 .
bash docker/run-with-daemon-workspace.sh \
  -e BUILD_TYPE=Release -e BUILD_TESTING=ON -e RUN_TESTS=ON \
  -e CLEAN_OUTPUT_DIR=ON duel6r-build:pi5
```

The resulting `build/` is a **separate** ARM64 bundle. Do not overlay x86 Linux
binaries in that directory. Validate `linux-aarch64.sha256sums` from its root.
The native ARM CI workflow publishes a separate candidate artifact; its software
rendering checks are not physical Pi GPU certification.

Run through Docker as required by the repository. For a local X11/XWayland
desktop, pass the existing X authority cookie (do not disable access control),
DRM render device, audio socket/devices and controller devices using the host's
least-privilege permissions. Override the build image entrypoint, set the working
directory to the bundle and use `SDL_VIDEODRIVER=x11`. Do not set
`LIBGL_ALWAYS_SOFTWARE` for hardware acceptance. Runtime deployment and device
permissions must be verified on the actual target; headless CI uses dummy audio.
Native execution outside Docker is not part of the repository-supported workflow.

For an existing local X11/XWayland login with a PulseAudio-compatible server
(including PipeWire-Pulse), the following is the target-device launch recipe.
Set `XAUTHORITY` to that login's existing cookie file and select the actual V3D
render node; these paths are host-specific. Run it on the Pi, not through a
remote Docker daemon. This recipe still needs the physical-device acceptance
below; it is not a claim that host device access has been exercised in CI.

```sh
test -f "${XAUTHORITY:?Set XAUTHORITY to the existing desktop cookie file}"
RENDER_DEVICE=/dev/dri/renderD128
test -c "$RENDER_DEVICE"
test -S "${XDG_RUNTIME_DIR}/pulse/native"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --group-add "$(stat -c '%g' "$RENDER_DEVICE")" \
  --device "$RENDER_DEVICE" \
  -e DISPLAY -e SDL_VIDEODRIVER=x11 -e SDL_AUDIODRIVER=pulseaudio \
  -e XAUTHORITY=/tmp/desktop.xauth -e PULSE_SERVER=unix:/tmp/pulse-native \
  -v "$XAUTHORITY:/tmp/desktop.xauth:ro" \
  -v /tmp/.X11-unix:/tmp/.X11-unix:ro \
  -v "${XDG_RUNTIME_DIR}/pulse/native:/tmp/pulse-native" \
  -v "$PWD/build:/game" -w /game \
  --entrypoint /game/duel6r duel6r-build:pi5
```

The bundle must be writable by that user for `data/persons.json`. A controller
additionally needs its selected `/dev/input/eventN` (and, when applicable,
`/dev/input/jsN`) passed with `--device` and its numeric device group added with
`--group-add`; expose only intended controller devices. Do not use `--privileged`
or `xhost +`. Docker/SDL controller hotplug across replacement event nodes needs
target-device verification, not an assumption from keyboard-only smoke tests.

## Visual verification matrix

No layout, focus, pointer region or viewport rule changes. The unsupported-CPU
message on ARM is `Secure networking is unavailable. ARM AES and ASIMD are required.`;
the existing x86 message remains unchanged. Preserve
the [screen contracts](screens/README.md), [design rules](design.md) and their
existing stable wireframes. The new platform path potentially affects every
screen, so capture one representative per current wireframe at **1280×900** on
the Pi's V3D renderer after behavioral preflight and source review:

| Wireframes | Reachable representative |
| --- | --- |
| MENU-01-A, MENU-01-B | Non-Team / Teams setup, two assigned players |
| MENU-02 | Rejected one-player start |
| PLAY-01, MODE-01, MODE-02, PLAY-05 | Live Deathmatch, Predator, Teams and rising water |
| OVER-01, OVER-02, OVER-03 | Tab scores, non-final round summary, limited final summary |
| CONS-01, CONS-02 | Console over menu / live arena |
| NET-01, NET-02, NET-02-P | Network entry, editable host, pending host |
| NET-03-E, NET-03 | Editable join / connecting |
| NET-04, NET-04-R | Lobby / retained completed result |
| NET-05, NET-05-C, NET-05-S, NET-05-R | Live network arena, confirmation, status, resync |
| NET-06, NET-07, NET-08, NET-09 | Final summary, reconnect, failure, host ended |
| NET-10 | Loaded browser results |

Store unchanged captures under `docs/design/screenshots/pi5/` with source SHA,
binary hash, scenario, OS/Mesa/GPU, viewport, path and PNG hash. Missing captures
block the visual gate. No capture is claimed by this plan. Independent security
review must assess the new AES dispatch path; routine developer tests do not
satisfy that independent gate.

## Local implementation verification (2026-09-30)

These are historical pre-checkpoint developer observations, **not final checkpoint
acceptance**. They were collected on the worktree based on
`da6f4a067efb6f369143864cd4ed36ba9f9e98f3`, before commit/push/PR authorization.
That base SHA does not identify the changed application. The user subsequently
authorized a checkpoint, draft PR and physical verification over SSH on `rpi5.lan`.
Results below do not themselves claim hosted CI or physical Pi execution.

Builds and execution ran only in disposable Docker containers. Native tooling:
Ubuntu 24.04, GCC 13.3, base image
`sha256:604c41dbb788156bb580ae4eee88c7a00d6e06e9facf9cadccc1ba705e00d57e`.
The temporary cross image added GCC 13.3 AArch64, ARM64 library development
packages and QEMU 8.2, image
`sha256:ace79f7229348e3d575b3b535559e2c83365fc37fab68e2b692fdb3db3639975`.
The pinned Mbed TLS dependency was rebuilt from the changed configuration for
each architecture. No host SDK, global binfmt registration or privileged container
was installed. Temporary tooling, containers and candidate binaries are cleaned
up by the implementing agent; no binary from this run is a published release.

| Requirement / check | Command or scenario inside Docker | Observation |
| --- | --- | --- |
| PI5-001/002 | `cmake -S /workspace -B /tmp/pi5-arm -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_PREFIX_PATH=/opt/mbedtls-arm -DCMAKE_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu -DCMAKE_INCLUDE_PATH=/usr/include/aarch64-linux-gnu -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-aarch64-static`; `cmake --build /tmp/pi5-arm --parallel 4` | Complete ARM64 application/helper/test build passed; cache selected `gl1` without an override; ELF is AArch64. Native ARM bundle execution remains pending. |
| PI5-003 | `qemu-aarch64-static -cpu cortex-a76 /tmp/pi5-arm/duel6r-hardware-crypto-tests` | AES-128/256 encrypt/decrypt known-answer tests passed. Restriction-only link wrapper separately removed AES, ASIMD and all HWCAP bits; all denied sessions failed closed. This is not a physical non-AES CPU result. |
| PI5-004 | `D6R_TEST_FILTER=secure qemu-aarch64-static -cpu cortex-a76 /tmp/arm-execution/duel6r-session-transport-tests` | All five security scenarios passed: encrypted wire/replay, password/unlocked exchange, record/byte/time budgets, unsupported hardware/entropy, tampering/plaintext downgrade. |
| PI5-004 | `python3 tests/AdmissionProcessTests.py HOST_SERVER INTEROP_CLIENT_SERVER`, with ARM and x86 binaries in both role assignments | Both directions passed actual encrypted admission, stable assigned identities and rejection of mismatched gameplay content. Not a complete cross-machine graphical match. |
| ARM logic | Seven CTests under QEMU: core tests, hardware crypto, authoritative match, sparse-team Quick Liquid, player input, state replication, trust policy | 7/7 passed. The final crypto wrapper was rerun after its test-only addition. |
| PI5-002/005 preflight | `LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a qemu-aarch64-static -cpu cortex-a76 /tmp/pi5-arm/duel6r-renderer-batch-tests`, from resources | Exact scalar/batch pixels and GL error checks passed under ARM software rendering, not V3D. |
| Regression | Release x86 `gl1`, `ctest --test-dir /tmp/pi5-gl1 --output-on-failure` | 32/32 CTests passed, including shared arena, all mode/team variants, network runtime and renderer tests. The shell observer timed out at 600 seconds; the unchanged CTest process completed and its final log recorded all 32 passes. |
| Test-only changes | Rebuilt crypto test and `ctest --test-dir /tmp/pi5-gl1 --output-on-failure -R '^(duel6r-hardware-crypto-tests\|duel6r-admission-process-tests)$'` | 2/2 passed after final test-source changes; unaffected earlier results retained. |
| x86 default/package | `BUILD_TYPE=Release BUILD_TESTING=ON RUN_TESTS=OFF CLEAN_OUTPUT_DIR=ON bash docker/build.sh`; `sha256sum --quiet -c linux-x86_64.sha256sums` | Default `gl4` build/package and complete inventory validation passed. |
| Menu preflight | `bash docker/main-menu-smoke.sh` on x86 GL4 bundle and ARM GL1/QEMU fixture | Both passed menu, Quick Liquid toggle, console and clean exit. QEMU used the existing `SMOKE_APP_TIMEOUT=90s` override. |
| Declarative checks | Containerized `actionlint /arm64.yml`; `bash -n` on both changed shell scripts; `git diff --check` | Passed. Workflow has not been published or run on a hosted ARM runner. |

Initial setup failures were isolated to the disposable cross environment
(multiarch JACK selection and curl development-package conflict). Without global
binfmt, ARM helper `exec` initially failed resolution; explicit QEMU launcher
scripts around the unchanged ARM resolver/server binaries fixed the execution
fixture, not production code. The first ARM menu smoke captured a black window
before asset initialization finished. Bounded first-frame polling replaced the
fixed two-second sleep; all existing pixel-change and exit assertions remain.

Observed Release executable SHA-256 values (artifacts removed during cleanup):

| Artifact at verification time | SHA-256 |
| --- | --- |
| `/tmp/pi5-arm/duel6r` (GL1) | `fabff260cad76a59ed25bcc9e631f7b6d4f24e12c4384eb31890cec8fea8ba3f` |
| `/tmp/pi5-arm/duel6r-server` | `2c8a201747f39dbbac5415256c925f9bba7b39b93ba450930e8ad81447f72cbe` |
| `/tmp/pi5-arm/duel6r-host-supervisor` | `145300301d5110fb427a1407093d189c3e0208dec65faaa88f2ae882a51c932c` |
| `/tmp/pi5-arm/duel6r-resolver` | `9925ccf446adafe27c7d76bd4cfe7ec710f2eeec8fe1facb2bb2c69ac6dceaad` |
| `/tmp/pi5-gl1/duel6r` (x86 GL1) | `38d80a74624745dbd416657b439f20fe69639a2102c3493a8e26968144694b99` |
| `/workspace/build/duel6r` (x86 GL4) | `4c3d927aa9686f4ddb64e66a39a074b689bdd8994cbef70a32ec779d69d2b061` |

Static self-review covered the complete worktree change against the base:
architecture guards, pre-initialization denial, upstream AESCE targeting,
unchanged x86 behavior, renderer override precedence, manifest replacement,
archive execute permissions, tests and source/visual risks. The archive was
changed to tar before upload because raw Actions artifacts lose executable modes.
There is no claim of independent review.

At the initial implementation handoff: Specification status: ready.
Visual gate: blocked (physical Pi matrix missing).
Product acceptance: Blocked by missing evidence. Outstanding gates were: immutable
candidate checkpoint, native ARM CI/bundle consumption, Pi 5 V3D/peripherals and
complete cross-architecture gameplay, physical unsupported-CPU rejection, and
independent security review of ARM admission/key expansion/DRBG dispatch.
