# Darwin networking foundation checkpoint

This is the first permanent platform/security/process slice, not completed Mac
network parity. The graphical Mac target remains local-only until production
host, resolver, socket, manifest and bundle integration is complete. The owning
requirements are in `docs/macos.md` and the HSL-FAULT supervision boundary.

## Native execution

The existing macOS workflow invokes `macos/build.sh` at its immutable source SHA.
It builds the common pinned private Mbed TLS recipe for arm64/macOS 14, selects
the Homebrew curl prefix explicitly, and runs these ordinary CTest entries:

- `darwin-cleanup-state`: the production cleanup decision state, also exercised
  by Linux/Windows builds; this alone is not native process evidence.
- `darwin-process-containment`: the real guardian, independently monitored worker,
  uncooperative descendant, production Darwin process inspection and private IPC.
  Covers cancellation, application SIGKILL (including stopped worker/descendant),
  leader SIGKILL, guardian SIGKILL with runnable worker, real localhost resolution,
  incomplete-inspection retention, pre-spawn cancellation, restricted inheritance,
  unrelated-process survival, and listener release. No GUI or external service.
- `darwin-hardware-crypto`: existing AES-128/AES-256 key expansion and block
  known-answer tests with the private hardware-only configuration.
- `darwin-hardware-admission`: restricts real physical queries; observes actual
  production TLS/entropy/DRBG initialization sites. No positive capability can be
  fabricated. Missing AES/SIMD, syscall failure and malformed results fail closed.
- `darwin-secure-session`: the production SecureSession implementation on real
  native sockets, including password/unlocked application data, wrong-password,
  entropy/capability restrictions, modified ciphertext and record replay.
- `darwin-trust-policy`: existing trust-policy behavioral suite on Darwin.
- `darwin-system-trust`: approved bounded HEAD of `https://curl.se/`, no redirects,
  proxies, credentials or cookies; requires actual Apple trust evaluation, not a
  library version or symbol-presence assertion. The test-only dyld observer calls
  the real SecTrust evaluator and cannot change its result. No trust store changes.
- `darwin-trust-negatives`: isolated loopback HTTPS fixtures; default system trust
  rejects an untrusted certificate. A per-request test CA establishes a successful
  local positive control and a separate wrong-host rejection, not a SecTrust pass.

`native-secure-session` reuses the TLS tests on Linux. Neither that run nor the
Linux modelled pointer/viewport tests are native Mac evidence. Native test failures
are failures, not skips; hosted results remain owned by DevOps.

## Process interface and ownership

`DarwinProcess.h` exposes `runGuardian` and `ParentMonitor::start`. The guardian
entry receives a non-secret parent PID, sole-owner liveness read FD, status socket
FD, and absolute worker executable plus non-secret arguments. The owning GUI must
retain the sole liveness writer. The guardian registers parent observation before
worker spawn; its worker is a separate process-group leader and direct child.
Worker FD 3 is the guardian-liveness reader. The worker must install its monitor
before initialization or binding, and owned code must not escape the group.

The guardian's fixed local events are spawn notification (not Ready), leader exit,
positive cleanup and failure. An eventual host adapter must retain existing
service-status framing and cancellation/readiness precedence; these local events
must not bypass that protocol. Unknown group inspection retains the unreaped
leader and never emits positive cleanup. Loss of child ownership forbids further
numeric process-group signalling. Only the exact direct child is reaped; ordinary
orphan descendants are observed until the system actually removes them.

The guardian never performs blocking status writes, simulation, DNS or directory
requests. A runnable worker independently terminates its own group on guardian
loss. Process inspection is bounded; failed, full, inaccessible or inconsistent
inspection cannot establish tree-zero. Production has no unknown-inspection test
switch; the separately linked regression executable injects that failure.

## Remaining integration and proof

The production HostServiceProcess/HostedServiceChannel and ResolverMain adapters
are not yet connected. Existing service readiness, startup timeout, port-conflict,
late-Ready cancellation, and production resolver deadline/retained-cleanup tests
must be extended through those adapters. Native anchor-loss syscall fault coverage,
registration-race stress, production IPv4/manifest/random paths, full dependency
relocation/package checks, and deterministic cross-platform fixtures remain to be
completed. Foundation helper binaries are not yet shipped in the `.app`.

Do not enable GUI hosting on the strength of compilation or the portable decision
tests. Preserve the three-second cleanup contract, positive identity proof and
unconfirmed-cleanup ownership when connecting these primitives. Actual Mac GUI,
live cross-OS gameplay and visuals remain user-owned after merge/nightly.
