# Packaged Linux deployment behavior tests

## Purpose and boundaries

`PackageDeploymentBehaviorTests.py` exercises the **packaged graphical executable**
and its own server, supervisor, and resolver. It follows the repository's existing
Python/shell application-behavior harness convention, but does not rebuild the
game or register infrastructure checks. The input archive is verified against a
digest supplied independently by the checkpoint owner. Every installation and
local-data backup is a private disposable copy; the original archive is not edited.

The tests cover NET-DEP-AC-001/002/003/004 and NET-UPG-AC-001/002/004 in
`docs/network-deployments.md`. Passing a scenario is not complete criterion or
release acceptance. New, unintegrated tests provide diagnostic evidence only.

`PackageRenderedTextObserver.py` is a narrowly scoped GDB assertion helper for the
Linux x86-64, libstdc++ C++11 ABI binary. It observes the existing `Font::print`
entry points and groups requested predicates at `SDL_GL_SwapWindow`. Only an
allowlisted predicate's index, the request identity, and frame number are saved.
It never records arbitrary UI text, raw traffic, reconnect credentials, a memory
dump, or a screenshot. Matching identity, ownership, and readiness rows must occur
in **one frame**, not accumulate across UI states. The actual inferior exit code
is checked separately from GDB's exit code.

This observes text submitted to the real rendering path, not final pixels. It is
not visual acceptance, an accessibility audit, or a portable accessibility API.
Software breakpoints perturb scheduling; these tests must not establish startup,
shutdown, reconnect, latency, or race deadline compliance. A stripped binary or a
different ABI needs a separately validated observer, not guessed offsets. Missing
symbols and observer failures are test-environment failures, not product failures.

## Execution environment

Follow `AGENTS.md`: **Docker-only execution**. Use
`docker/run-with-daemon-workspace.sh` from a disposable workspace, because that
wrapper copies its `build/` output back. Never run it against a frozen handoff's
build directory. Override the image entrypoint so the runtime does not rebuild.

Use the Linux runtime dependencies listed in `docs/network-package-operations.md`,
plus test tools `python3`, `unzip`, `xvfb`, `xdotool`, `xauth`, `gdb`, `procps`, and
`iproute2`. GDB needs container-local ptrace permission; the documented debugging
options are `--cap-add=SYS_PTRACE --security-opt seccomp=unconfined`. Do not attach
to any existing user's client. Displays `:91` and `:92` must be free in the
disposable container. No host display, physical input device, or screenshot is
required. Audio is dummy and rendering uses Mesa software GL.

For offline Local Play and replacement, install prerequisites first, then remove
the disposable container's external network attachment. Only loopback may remain.
The Local Play check rejects an environment with another interface. It uses stock
arenas, observes `End of Game`, leaves via Escape after the summary hold, checks
saved game/Elo/death data, and samples absence of service components and new TCP
listeners. Existing container OS/DNS listeners are recorded before client launch;
Docker may retain its embedded DNS listener after its network is detached.
The winner itself is not fixed: a valid simultaneous-death result is allowed.

## Single-container scenarios

Run these **inside the approved container**, with both Python files available:

```sh
python3 /tests/PackageDeploymentBehaviorTests.py /artifacts/package.zip \
  --sha256 CHECKPOINT_ARCHIVE_SHA256 --scenario session
```

Use one of these scenarios:

| Scenario | Assertions |
|---|---|
| `session` | Two graphical clients; four named players; both participant ownership rows; not-ready and ready states; actual gameplay; intentional End; guest Host Ended panel; client exit codes; owned-process/listener cleanup. |
| `local` | Offline complete one-round Local Play with stock content, final summary, and real persisted statistics. |
| `replacement` | Local Play first, then initial network session; fresh same-release reinstallation with own backup restoration; another real session; rollback from the saved complete archive plus the original backup; another real session. Later config/profile changes must not replace the prior backup. |
| `mismatch` | Different valid guest `config.script`; exact wrapped gameplay-content mismatch copy; host still has only its own participant; no silent guest-config rewrite. Exits through Edit setup and Back. |
| `invalid` | Symlinked guest gameplay file; exact local-invalid-content copy; Retry unavailable; host remains alone; cleanup. This is **local pre-admission invalidity**, not a remote invalid-manifest request. |
| `failure-return` | Same real mismatch, then pointer-click Return to Network; require Network entry, not Join setup. Keep this expectation even if the checkpoint fails it. |

People and statistics are created through the client. Config and custom profile
backups are byte-compared before and after the real replacement sessions. Each
fresh extraction passes the platform inventory **before** local data is restored.
Replacement never overlays old binaries or rewrites an inventory. No previous
network release, Windows data transfer, or cross-release migration is tested.

## Two-container private virtual LAN

The operator must create an isolated **internal** Docker network with a free
RFC1918 subnet, attach only the two disposable participant containers, and publish
no ports. Do not change host routes, host firewall policy, or shared networks.
Each participant must have loopback and exactly one private interface. This is
virtual-LAN evidence, not physical-LAN or Windows/cross-platform evidence.

Example roles, executed in their separate containers:

```sh
# Host, assigned 172.30.240.10 on the isolated internal network:
python3 /tests/PackageDeploymentBehaviorTests.py /artifacts/package.zip \
  --sha256 CHECKPOINT_ARCHIVE_SHA256 --scenario lan-host \
  --address 172.30.240.10 --peer-ready-file /tmp/qa40-peer-ready

# Guest, assigned a different address on that same isolated network:
python3 /tests/PackageDeploymentBehaviorTests.py /artifacts/package.zip \
  --sha256 CHECKPOINT_ARCHIVE_SHA256 --scenario lan-guest --address 172.30.240.10
```

Launch the guest after the host reports `LAN_HOST_READY`. The controller must
wait for **guest** output `LAN_PARTICIPANT_READY guest`, then create
`/tmp/qa40-peer-ready` in the host container. After **guest** output
`LAN_PARTICIPANT_MATCH guest`, create `/tmp/qa40-peer-ready.match` in the host
container. These are test-controller acknowledgments, not application state or
network messages. Fresh markers are required and consumed. They prevent the host
from advancing before the independent guest assertions have actually run.

For cancellation, use a known-unused peer address on that same isolated network:

```sh
python3 /tests/PackageDeploymentBehaviorTests.py /artifacts/package.zip \
  --sha256 CHECKPOINT_ARCHIVE_SHA256 --scenario cancel --address 172.30.240.99
```

This verifies guest connection Cancel while its UI is pending, editable setup,
and owned-process cleanup. It does not establish host-startup cancellation or a
deadline. Remove the owned containers and network after the tests.

## Remaining compatibility and platform coverage

Protocol/release/capability mismatches cannot be generated by two unchanged copies
of one matching release. Existing `AdmissionCompatibilityTests.cpp` cases provide
supplemental policy/code mapping checks without inventing a package mutation:

- `AC-005 AC-006 AC-007 exact compatibility checks are case and whitespace sensitive`
- `AC-017 admission outcomes retain approved precedence identifiers and fixed copy`

Run them with `D6R_TEST_FILTER` and `D6R_TEST_EXACT=1` in their existing test target.
They do **not** replace graphical incompatible-release or remote-invalid-manifest
observations. Windows desktop, both cross-platform host directions, physical LAN,
visual acceptance, host-startup cancellation, and uninstrumented timing remain
separate evidence requirements. Route screenshot and environment needs through
team/developer; do not substitute Wine or synthetic admission for those gates.
