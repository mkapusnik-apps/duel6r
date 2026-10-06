# Darwin networking foundation checkpoint

This is a permanent platform/security/process slice, not completed Mac network
parity. Production host, resolver, TCP and manifest adapters now build on Darwin;
the graphical Mac target remains local-only pending native proof and bundle/GUI
integration. The owning
requirements are in `docs/macos.md` and the HSL-FAULT supervision boundary.

## Native execution

### Focused diagnostic checkpoint (not qualification)

After source review, the existing macOS job may invoke the helper with
`D6R_NATIVE_DIAGNOSTICS=ON`. This explicitly selects ten `native-diagnostic`
CTest entries using the existing test-name filters: irreversible anchor loss, observation invariance,
unaltered production refusal/queue cases, separately labelled raw-socket/writer
observations, and the four failing admission/input/summary cases. Empty filters
that match no cases fail rather than claiming success. TLS policy/counter tests
are unchanged; DevOps owns the parallel curl provenance investigation.

The mode builds only its required targets and never calls the package helper.
Even if its selected checks pass, it exits 78 with a `DIAGNOSTIC ONLY` message so
the current workflow cannot mistake this for Feature Ready or publish a package.
CTest failure returns its actual nonzero code. The existing failure-log upload
retains the selected LastTest.log. Default `OFF` keeps the normal full job.
No new workflow is introduced. Do not push/trigger the normal full job as a
substitute for the approved focused run; DevOps must select this mode explicitly.

The production queue-boundary case has no send/clock/wait injection. Supplemental
writer counters are fixed-size observation storage, never used by decisions, and
do not bypass native poll/send paths. Raw refusal controls use independent sockets;
they never consume SO_ERROR from a production socket. Runtime observations record
calibration stage/probe pairing and first close cause, and actual host wait/work,
tick debt, sampling counts and fixture-stop conditions. They do not alter clocks,
service a missing fixture probe, add catch-up ticks, or change deadlines.
Reported times are monotonic microseconds, not CPU usage; RTT observations do not
replace production calibration decisions. `first-terminal` maps to
`ObservedTerminal` in `RuntimeObservations.h`. Replication phase 2 remains
RoundSummary; the receipt separately labels the last active authoritative phase
and hosted stage. First and latest match starts are distinct so a following-lobby
restart cannot overwrite the timing origin of the result under investigation.

Lost ownership now latches before all subsequent PID operations in both child
modes. The native regression actually reaps a child unexpectedly, observes ECHILD,
then models PID reuse while asserting zero further wait/reap/signal operations,
sticky failure, a retained slot and survival of an unrelated real child. This
negative test is isolated in a subprocess that explicitly exits after disposing
its real fixtures; production has no quarantine-release escape hatch.

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
  unrelated-process survival, and listener release. The owner explicitly passes
  FD 4096 to the guardian; the worker checks the full native FD inventory before
  starting its monitor, not just descriptors below 1024. The boundary fixture
  positively observes the high FD in the guardian and deliberately leaks it in
  a negative control that must be rejected before worker initialization. No GUI
  or external service.
- `darwin-process-boundaries`: a separately compiled test guardian places bounded
  barriers before/after parent registration and worker spawn, then tests actual
  parent SIGKILL. Registration syscall failure prevents spawn. An exact-child
  reap makes the actual next `waitid` report ECHILD and verifies no subsequent
  PGID signal or cleanup-success event. Real half-frame partial status delivery
  must seal the channel at EOF with no later send or frame resynchronization;
  anchored cleanup must still complete. These observers and fault selectors are
  absent from the shipped guardian. The negative lost-anchor fixture explicitly
  disposes its own surviving descendant after verifying fail-closed behavior.
- `darwin-hardware-crypto`: existing AES-128/AES-256 key expansion and block
  known-answer tests with the private hardware-only configuration.
- `darwin-hardware-admission`: restricts real physical queries; observes actual
  production TLS/entropy/DRBG initialization sites. No positive capability can be
  fabricated. Every negative uses valid native sockets; supported positive
  controls in the same executable must reach all three initialization sites.
  Missing AES/SIMD, syscall failure and malformed results fail closed without
  socket-option failures concealing a removed capability gate.
- `darwin-secure-session`: the production SecureSession implementation on real
  native sockets, including password/unlocked application data, wrong-password,
  entropy/capability restrictions, modified ciphertext and record replay.
- `darwin-trust-policy`: existing trust-policy behavioral suite on Darwin.
  It links the complete production network library, including ConnectionPlan,
  TcpListener and TcpClient; no cases are removed to make a security-only link pass.
- `darwin-session-transport` and the existing admission, host-supervisor, lifecycle,
  authoritative-input, replication and responsiveness suites run against that
  complete library. `darwin-authoritative-fixtures` executes the existing CLI
  semantic fixture suite against the real headless server.
- `darwin-production-adapters` exercises encrypted TCP via actual guarded DNS,
  repeated cancellation followed by usable resolution, positive failed-spawn
  cleanup, real hosted readiness, normal shutdown, startup cancellation and port
  conflict. These are native headless tests, not Mac GUI or live cross-OS evidence.
- `darwin-system-trust`: approved bounded HEAD of `https://curl.se/`, no redirects,
  proxies, credentials or cookies; requires actual Apple trust evaluation, not a
  library version or symbol-presence assertion. The test-only dyld observer calls
  the real SecTrust evaluator and cannot change its result. No trust store changes.
  The observer is a separate test-only shared image, covers modern and legacy
  APIs, and has a real untrusted-certificate control from the executable. Output
  identifies the actual libcurl image/backend and both counters. Native-CA
  selection is explicit with file/path CA defaults cleared; zero native evaluations
  still fails. Fixture controls aggregate failures instead of aborting before
  the wrong-host case.
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
The production adapter additionally supplies private output/input sockets mapped
to worker FDs 4/5. Only their exact descriptors are inherited; payloads never pass
through the guardian event loop or argv. The resolver writes its bounded response
to FD 4. The server uses FDs 4/5 for the existing hosted-service protocol.

`GuardedChild` owns the sole application-side liveness writer and the direct
guardian child. Spawn, exit and cleanup are distinct from service readiness.
Guardian cancellation closes liveness, not a numeric PID. A bounded 32-slot custodian
retains abandoned resolver cleanup until a valid cleanup event, clean guardian
exit, channel EOF and exact guardian reap all agree. Unknown/failed ownership is
not released as success. Active callers retain first-exit observation ownership;
the custodian only polls handles no longer held by an active caller.
Leader exit carries monotonic nanoseconds sampled at the guardian's first terminal
`waitid` observation. The receiver checks attempt bounds, PID/phase, ordering and
future timestamps; GUI consumption does not resample it. Local guardian-loss time
is separate and cannot invent earlier worker-exit evidence. Portable codec tests
and a native delayed-GUI test cross the original ten-second startup deadline.

Numeric IPv4 endpoints require no resolver process. Standalone client hostnames
use the guarded resolver. An already guarded service uses an independently
cancellable resolver subprocess in the service's existing process group. No
`getaddrinfo` runs on the attempt thread. This leaf uses self-only parent-death
termination; the outer guardian can still kill it when stopped. Its exact parent
retains the PID until reaping, and never signals after ownership loss. The bounded
custodian has an owned, joined thread rather than a detached resolver/reaper.

`darwin-resolver-ownership` requires each actual helper to publish its entered
barrier and matching group identity before cancellation. A test-only observation
delay retains 32 exact children/slots, rejects a 33rd allocation, then requires
positive reap/release and reuse. This replaces the immediate-cancel loop as the
cleanup-ownership proof; that loop remains only a transport cancellation smoke.

The guardian's fixed 16-byte local events are spawn notification (not Ready), leader exit,
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

Supplied run 37436857635 / artifact 11400195128 at 56e1416 reported 14 passing and
six failing CTests. The host-adapter test incorrectly treated ApplicationExit intent
as completed cleanup; it now requires both within the original three-second bound.
The manifest fixture now creates a checked, representable UTF-8 filename forbidden
by the ASCII contract, with raw invalid-byte rejection asserted at the validator.

The stopped-process fixture failed before parent-kill injection, not during the
later guardian-loss case. Identity absence is explicit, and a failed task-info
query is not accepted as disappearance without BSD snapshot/ESRCH confirmation.
These observations must distinguish an inspection issue from premature exit;
neither is labelled passed without native evidence.

The two transport failures and four remaining admission/replication failures lack
sufficient actual values in the supplied log to assign a root cause. Their original
assertions and deadlines remain. Permanent diagnostics record refusal controls,
actual native writer progress, sampling counts, canonical phases/readiness and
fixed fixture outcomes. Do not label these additions as resolved behavior or
loosen calibration, progress or admission bounds.

The production HostServiceProcess/HostedServiceChannel and ResolverMain adapters
are connected without enabling the GUI. Native execution of the complete linked
suites, boundary faults and adapter regressions remains required. Deeper adapter
timeout/late-Ready/delayed-cleanup fault cases, native random failure paths,
full dependency relocation/package checks and cross-platform provenance comparison
remain to be completed. Foundation/network helper binaries are not yet shipped in
the `.app`; headless CLI targets are build outputs only at this stage.

Do not enable GUI hosting on the strength of compilation or the portable decision
tests. Preserve the three-second cleanup contract, positive identity proof and
unconfirmed-cleanup ownership when connecting these primitives. Actual Mac GUI,
live cross-OS gameplay and visuals remain user-owned after merge/nightly.
