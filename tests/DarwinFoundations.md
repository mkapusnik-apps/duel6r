# Darwin networking foundation checkpoint

This document records the platform/security/process foundation and its application
integration, not completed Mac acceptance. The graphical Mac target now connects
the existing network journeys while retaining native startup, saves, pointer and
drawable handling. Complete native verification and independent QA remain gates.
The owning
requirements are in `docs/macos.md` and the HSL-FAULT supervision boundary.

## Native execution

### Diagnostic findings and normal qualification

The one approved focused run at `2e881c4` is complete. Its temporary workflow,
build-helper switch and filtered CTest registrations have been removed. The
ordinary full native suite remains mandatory; another run requires authorization.
Permanent test-name filters reject empty selections rather than claiming success.
TLS policy/counter assertions remain unchanged.

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
It builds the common pinned private Mbed TLS recipe and curl 8.21.0 for
arm64/macOS 14. Private curl uses OpenSSL with Apple SecTrust, a threaded resolver,
and no CA bundle, CA path or CA fallback. Its archive hash and generated settings
are checked; CMake explicitly selects the private library instead of Homebrew
curl. Packaging checks provenance and rejects substituted curl dependencies.
The network GUI consumes the private curl; packaging requires it in the actual
closure and records `local_only=false`. Source/configuration checks do not prove native trust.
The helper runs these ordinary CTest entries:

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

The focused `2e881c4` measurements identify the following corrections, still
requiring native confirmation:

- Both raw and production Darwin sockets remained Connecting at a bound but
  non-listening endpoint. The fixture now checks bounded cancellation in this OS
  case; the actual closed-listener refusal assertion remains unchanged.
- Darwin replaced a 4096-byte pre-connect receive-buffer hint with 326640 bytes.
  The fixture reasserts and verifies the small buffer after connect. Production
  queue limits and the five-second no-progress watchdog remain unchanged.
- Admission's first RTT was 45944 microseconds, over the unchanged 20 ms bound.
  The fake host answered only one probe then closed after two seconds. It now
  answers bounded retries within the original overall ten-second admission
  window; a forced slow-first-response regression requires real recalibration.
- Summary fixtures reached only 300/360 and 288/360 ticks; actual sleeps totalled
  9.48 and 8.16 seconds against requested totals of 1.58 and 1.475 seconds. Fixed
  tick pacing now permits one immediate successor, revisiting all ingress and
  cancellation work before each tick, then yielding. No ticks or debt are dropped.
  A rolling one-second cap of 90 ticks bounds catch-up below existing input limits;
  deterministic tests model the measured sleep overruns and second-boundary bursts.
- Installed curl enabled CA fallback: a successful public request bypassed
  SecTrust despite the negative control reaching it. The pinned private recipe
  removes that fallback; real positive and negative trust counters still decide
  native acceptance, not version strings or generated macros.

The focused input case passed once, which is not qualification. None of these
corrections relax calibration, progress, cleanup, sampling or admission bounds.

The subsequent integrated pacing regression runs the production host loop with
deterministic transport/world/clock seams. It retains several seconds of debt,
exhausts the real rolling cap and supplies continuous 50 Hz admitted input alongside
pending Join, reconnect and terminal/silent peers. At `cf6685d`, admitted input
progressed but the overdue-admission branch starved pending offers, reconnect and
cleanup. The correction services admitted peers and pending request/lifecycle
intake before one atomic lifecycle batch and at most one due tick. Only new
admission commits run after that tick. A cap-blocked due tick defers only
new admission commits; requests, reconnects and deadlines still run. The test
requires sub-second offer/reconnect progress, the unchanged first-request timeout,
disconnect cleanup, real applied input after cap release, due-winner rejection of
queued admission, and separate Cancel/End cleanup while the cap is exhausted.
These deterministic integration results do not replace native execution.

The completion-boundary regression additionally authenticates a reserved Leave
before the final completion tick. The earlier post-tick intake at `72d4895`
incorrectly retained a Completed result. Reserved and admitted Leave now enter
the same removal batch before completion; a terminal connection cannot stop the
admitted scan and hide another queued Leave. Coverage includes simultaneous
admitted/reserved departure below two players, the same batch under sustained
debt and cap exhaustion, and a no-Leave control that really completes on the
next tick. The interruption cases require one removal batch, no extra tick,
and an Interrupted following lobby without a final winner. No second lifecycle
batch or winner evaluation is added.

At `cf7f9d49`, the supplied native run passed 20 of 21 CTests with no skips,
including real positive SecTrust evaluation, negatives, crypto/process ownership,
66 admission cases, sustained input and semantic fixtures. Only two writer-stall
cases failed: native application output had progressed 2282 ms before observation,
so the unchanged five-second watchdog was not yet due. A 250 ms receiver plateau
was not proof of writer blockage. The revised real-socket fixture isolates its
writer observations, bounds setup at 12 seconds, requires outstanding output and
non-writable IO with stable native progress/receiver occupancy, and observes the
unchanged five-second watchdog with its existing two-second margin. Resumed
progress may invalidate a candidate only within the original setup deadline;
later changes fail setup. The whole observation is capped at 19 seconds. No
production send/poll/clock replacement, deadline relaxation or diagnostic workflow
is used. Both affected cases pass locally; native closure remains required.

## Integration root-fix map and acceptance evidence

The current implementation follows approved specification `bdf9a5e` and the exact
UX source at `d6308a2`. No normative document or wireframe is rewritten here.

| Cause / scope | Production or test correction | Evidence and outstanding gate |
| --- | --- | --- |
| Premature writer fixture readiness | Bounded real-socket setup described above; shared read-only observation helper | Local queue/writer cases pass; complete native transport gate required |
| Mac local-only build guard also owned startup/rendering | `D6_MACOS_PLATFORM` preserves startup/save/pointer/full-drawable code; obsolete Network notice removed; shared network UI enabled | Mac pointer/viewport model includes pointer and F2 entry, Host/Join/Browser transitions; native manual visuals deferred |
| Browse could reach HTTPS initialization without starting a gated gameplay socket | Darwin directory requests check physical secure capability before curl initialization | Fresh-process restricted AES/SIMD/sysctl cases require zero curl/TLS initialization; positive control calls real curl with an explicitly disabled directory origin |
| Host/CLI secure seed acquisition precedes socket construction | Darwin secure-seed consumers check physical secure capability before OS entropy calls | Restricted-hardware tests require zero actual OS entropy calls; separate OS-failure tests cover both host seeds and CLI world-construction failure |
| SDL bundle base names Resources, not executables | Absolute Darwin sibling helper path and cached canonical resource directory | Native packaged adapter runs from an unrelated cwd in an app path containing spaces |
| Missing helper closure and static TLS attribution | Regular server/guardian/resolver staging, all-helper ABI/minimum/signing checks, pinned static Mbed TLS notices/configuration/archive identities | Portable packaging regressions; actual native relocation/signature/package checks mandatory |
| Build-tree tests do not exercise packaged files | `MacPackagedTests.py` checks byte-identical relocated production files, isolated homes, actual service data, directory failure independence and bundled-curl identity/trust | Runs after packaging in normal native helper; no graphical app or public registration |

| Requirement | Existing/reused and added coverage | Acceptance status for integrated candidate |
| --- | --- | --- |
| MAC-NET-AC-001 | Native hardware admission/AES, secure exchange/tamper/replay; native SecTrust positive/negative; packaged curl probe requires actual bundled image | Prior foundation passes are supporting evidence; final native run pending |
| MAC-NET-AC-002 | Guardian/process boundaries, resolver retained ownership/cancellation/parent death, host supervisor lifecycle suites; packaged real service readiness/cancel/stop/port conflict and guarded no-Ready fixture using the original ten-second startup plus three-second cleanup bounds | Prior foundation passes; added timeout and packaged execution require final native results |
| MAC-NET-AC-003 | Actual eligible-interface encrypted socket exchange, stale/invalid bind rejection; test-only OS entropy restriction verifies credential, lifecycle, host session/match seed and CLI failures; existing unsafe-filesystem/admission suite | Added native cases require native execution; no fabricated positive entropy/capability |
| MAC-NET-AC-004 | Per-user save/resource alias and isolation tests; packaged helpers from space path/unrelated cwd with two test homes and unchanged save sentinels; four compiled-origin cases and actual directory-outage Host/protected Join/End | Portable checks plus new native packaged/directory checks; final native result pending |
| MAC-NET-AC-005 | Unchanged native authoritative golden fixtures, admission/input/lifecycle/replication suites; packaged server replays same golden fixtures | Prior native semantics passed; final Linux/Windows/Pi relevant evidence and native candidate comparison remain required |
| MAC-AC-003 / UX-NET-01 | Shared entry and journeys, exact approved NET-01 scope line; original canvas and glyph/layout rules retained; network world and menus reset actual drawable viewport | Automated source/model evidence, not native visual acceptance |

The directory QA interface is the existing
`duel6r-directory-client-integration-tests <server-executable> <resource-root>`
with an explicitly isolated emulator-backed `D6R_DIRECTORY_URL` and
`D6R_DIRECTORY_ALLOW_HTTP=1`. DevOps owns that backend and transport preparation.
Native build CI uses only loopback outage fixtures and the approved bounded
read-only curl.se trust request; it never creates a public listing.

Routine local verification for this integration uses Docker Release/GL1/Lua ON:
the application build, core/resource/save tests, complete transport and admission
suites, network-session runtime suite, directory-outage independence, pointer
tests and Mac drawable/network-world model pass. The final writer cases also pass
with native socket operations and no injected clocks. All 20 portable packaging
tests, Python/shell syntax checks and available portable C++ test syntax checks
pass. Windows Release/GL4 application and transport-test cross-compilation also
passes using the existing `duel6r-build-w64:ecjpake` image; no Windows runtime pass
is inferred. These results are not native Mac or Raspberry Pi execution evidence.

The native build profile is arm64/macOS 14+, Release, GL1, Lua ON, pinned private
Mbed TLS 3.6.7 and curl 8.21.0/OpenSSL/SecTrust. App, server, guardian and resolver
are the shipped executable targets. Headless adapter, randomness, capability,
trust, directory-independence and directory-client integration targets are test
outputs, not shipped app code. The normal helper writes the app/ZIP/checksum and
build metadata; successful packaged probes append their exact archive/file
identities and non-GUI results to the external `build-info.json` already retained
by the existing workflow. There is no alternate diagnostic publication path.

No candidate is frozen or user-ready here. Source review, every required final
native gate, cross-platform evidence and the independent high-risk QA campaign
remain separate. Actual Mac GUI, live cross-OS gameplay and visuals remain
user-owned after merge/nightly. Keep the original three-second cleanup and
positive identity/ownership requirements throughout final integration.

## Consolidated pre-freeze corrections after `0a1981c`

The supplied native job log SHA-256 is
`c6539938a4f48c45c8c7e33ace98437f2d133c8733ed2c48a7146230233c992a`.
Its 20 packaging tests passed, but compilation stopped before CTest: the hardware
admission target compiled `DirectoryOrigin.cpp` without its required definition.
No native CTest or packaged-runtime pass is inferred from that run.

| Finding | Correction | Focused evidence |
| --- | --- | --- |
| Origin implementation compiled outside its configured target | Hardware admission links the existing `duel6r-directory-origin` library; no fallback definition is introduced | Configured, unchannelled, nightly and release selector tests pass locally; native link rerun required |
| Native interface fixtures used an unrestricted listener | Positive encrypted and negative listeners enable production session policy; stale addresses require BindFailed and wildcard/broadcast/multicast/IPv6 require InvalidEndpoint, never Ready | Matching real-socket portable regression passes; native adapter cases require execution |
| CLI gate ran only during generated seed acquisition, after hashing | Darwin CLI entry checks physical support before manifest construction, independently of seed; the existing manifest filesystem observer is carried through CLI dependencies | Native cases cover generated/explicit seeds under all capability restrictions, zero filesystem/hash/entropy/world activity on denial, and real supported controls reaching hashing/world start; native execution pending |
| Apple ARM failure copy named x86 AES-NI | Apple AArch64 reuses the approved ARM AES/ASIMD copy; x86 wording is unchanged | Portable unsupported-hardware case checks the platform copy; native ARM assertion added |

Local focused verification passes the four origin suites, complete transport
suite (including the new policy/classification case), authoritative CLI process
fixtures, 20 packaging tests, application/server compilation and portable adapter
syntax. Windows Release/GL4 application, server and transport-test cross-compilation
also passes; no Windows execution is inferred. This is non-GUI routine evidence. Existing source/UX alignment and the
pending post-closure SS-015 capture are unchanged. The DevOps-prepared directory
backend at `0a1981c` is unchanged and remains reusable; this batch does not
provision, reset or reconfigure it. Copied local build workspaces are candidate
verification outputs, not a frozen independent-QA release artifact.

## Package-source retrieval after `9442099`

The supplied native checkpoint passed all 27 CTests without skips and all 20
then-current packaging unit tests. Its CTest log SHA-256 is
`b21434e283795def0914fdba297473b97071d9fb8d5c1aa80b81beafea69ffff`;
job log SHA-256 is
`d35437cfe24723b62b617039a9381c9ae4bc5624287bd8dfa4d1043d0c9bcf64`.
Packaging then failed after four connection timeouts fetching the pinned GNU
gettext 1.0 source archive. No application ZIP or post-package pass existed.

The retrieval correction changes package inputs only: one explicit HTTPS
kernel.org GNU fallback, unchanged archive SHA-256, bounded attempts, mandatory
checksum validation before cache promotion, and truthful retrieval provenance.
The real mirror returned HTTP 200 and 32694085 bytes with the existing digest
`85d99b79c981a404874c02e0342176cf75c7698e2b51fe41031cf6526d974f1a`.
Regression coverage distinguishes transport failures from integrity failures,
retains the full source/notice packet, checks endpoint exhaustion and cache reuse,
and records the actual requested/effective mirror URLs for fresh downloads.
All 24 portable packaging tests and Python syntax checks pass in Docker. The
downloaded real archive also contains the required 26419-byte COPYING.LIB notice.
Application sources and native test inputs are unchanged, so the 27 native passes
remain reusable evidence. Artifact identity is not frozen: package completion and
actual post-package execution remain mandatory gates.

## Maintained native transport observation coverage

Production `4df66053ef0457de70062f051019b13325b13433` and its accepted artifact
remain frozen. The required native check at evidence-only `9dca8fc` subsequently
failed two test fixtures despite identical production inputs. Supplied CTest log
SHA-256: `26adc8e796145770fbc00abd1c164d6b39c8e25184fcaa9ad0c3e4ed0aeb7427`;
job log: `5f30b00069d3a73e62d1047c2ed573c5ea5479c13c48d184846d8abd03a50582`.
The original production queue, deterministic deadline, lifecycle and cancellation
cases passed in that run. The failing supplemental receipt did not record its
terminal/progress interval, so its terminal classification alone is not a pass.

The owner-approved correction is test-only and supersedes the receiver-occupancy
readiness assumption described earlier:

- `SO_RCVBUF` remains a checked positive socket hint, not a required 1x–2x size
  relationship. One-way liveness still verifies real ordered payloads, initial and
  sustained backpressure, 10/20/30-second delivery checkpoints, the full 34-second
  observation, ping/pong handling and complete delivery within the existing
  45-second drain budget. Requested/reported buffer values are recorded.
- A native stall candidate depends on producer progress, outstanding output and
  non-writable IO. Receiver occupancy is diagnostic only and cannot reset the
  producer's quiet interval. An already-published terminal receipt is evaluated
  before any live-observer bookkeeping, without requiring an earlier poll to have
  recognized a quiet state. Setup remains 12 seconds and total observation 19.
- Acceptance requires bounded accepted output/backpressure, actual native calls
  and bytes, outstanding output, blocked IO, valid producer/terminal timestamps,
  `TimedOut` plus `OutboundStalled`, a quiet interval within setup, and the existing
  4.5–7-second native observation guard. That guard is not the production timer:
  the exact five-second deterministic deadline tests remain unchanged.
- The former supplemental writer case is now `native writer expiry survives
  delayed observation`: real sockets run until terminal publication before the
  helper first observes them. Embedded test-side receipt controls reject early,
  missing, future/reversed, wrong-cause, completed-output, missing-blockage and
  setup/overall-budget-invalid evidence. Synthetic receipts test only this
  classifier; they do not replace the actual native socket scenario.
- The former raw-refusal diagnostic now asserts raw and production outcomes for
  stopped listeners and bound-but-not-listening endpoints, including bounded
  pending cancellation or correctly classified startup expiry. It never consumes
  `SO_ERROR` from the production socket.

All 26 transport cases remain; no failing case is removed or skipped. The owner
explicitly adopts existing `NativeWriteObservations` as permanent application
regression support for native queue/progress evidence. Its fixed-size read-only
counters are unchanged, affect no production decisions, and have no runtime flag
or debug service. No production observer code, limits, workflow, specification,
UX artifact or frozen application input changes in this correction. This is
focused invalidated-test revalidation, not a new independent QA campaign. Source
reassessment precedes push; the required final-head native check must still pass.

Routine local revalidation: Docker image
`sha256:604c41dbb788156bb580ae4eee88c7a00d6e06e9facf9cadccc1ba705e00d57e`,
Release, `D6R_TRANSPORT_ONLY=ON`, `BUILD_TESTING=ON`. All four focused cases pass,
followed by the complete 26-case transport suite (96.42 seconds, no failures).
The focused delayed-observer receipt reports a 5006 ms progress-to-terminal
interval with real native calls, outstanding output and non-writable evidence.
These are local Linux results, not a replacement for the failed native CI check.
