# PR83 review regression verification

## Provenance and status

- Original review: [comment 5664146704](https://github.com/mkapusnik-apps/duel6r/pull/83#issuecomment-5664146704), findings 1–11. Its original reviewed head was `c63a61c52d84ed0d14a8db9c965d10e5f6d9170a`.
- Production checkpoint used for test authoring: `03b7d763e3ca9a31252417217bf1c6f475f27db9`.
- Specification baseline: `b4f0f6cea5732da2ecc4cb995cdc0135879032fd`. The relevant feature, network-play, authoritative-input, and trust specification files are unchanged between that baseline and the production checkpoint.
- **Diagnostic only.** No approved checkpoint artifact was supplied. These are uncommitted tester changes; integration and source approval are still required. They do not establish a final gate or product acceptance.
- No production files, Git metadata, infrastructure, hosted checks, or issues were modified. No acceptance screenshots were captured.

## Automated coverage mapping

Every added test name starts with `PR83`. Expected behavior is expressed in executable assertions, not source-text assertions.

| Finding | Requirements | Test file and scenario | Observable assertions |
|---|---|---|---|
| 1: End deadlock | NET-AC-014, NET-AC-016, NET-AC-018 | `NetworkSessionRuntimeTests.cpp`: real runtime End from lobby/active; separate naturally completed final-summary End | Actual `update()` draining returns; host becomes Inactive; guest receives HostEnded; host result disappears; supervisor confirms cleanup. Lobby/active cases restart the service on the same port. Final-summary case runs the real simulation on an independently staged small level. |
| 2: Return/repeat/modal | NET-AC-016, NET-AC-017 | `NetworkSessionRuntimeTests.cpp`: Application Return/repeat/Tab/modal | SDL events traverse Application. Return updates the status binding without opening a dialog. Repeat does not activate. Held fire/status cannot confirm an Escape-opened dialog. Modal input is zeroed. Release arms a fresh confirmation; Cancel and explicit Confirm remain functional for host and guest. |
| 3: Maximum host input rate | NET-AC-004, NET-AC-005, NET-AC-010 | `NetworkSessionRuntimeTests.cpp`: fourteen host slots plus guest | Real runtime/TCP/service with 15 independent controls; 24 alternating crouch press/release edges over approximately 7.2 seconds. Every slot, including the last host slots and guest, reaches the expected authoritative action mask within a 250 ms regression watchdog. Input production continues after early convergence. Simulation advances more than 300 ticks; no input policy violation occurs. |
| 4: Final-summary ghost members | NET-RES-AC-001, NET-OWN-AC-005, NET-AC-006, NET-AC-013, NET-AC-018 | `AdmissionCompatibilityTests.cpp`: final-summary Leave and expiry | Real HeadlessServer/TCP with a deterministic combat-completion seam. Both removal paths are observed before direct Return, with no intervening configuration/admission repair. Departed membership disappears, readiness clears, result/round winners and historical score rows remain, and the two surviving participants can ready and start a fresh match. Expiry advances only the host test clock after disconnect has been observed. |
| 5: Active round-end input | NIN-LIFE-001, NIN-LIFE-002 | `NetworkSessionRuntimeTests.cpp`: host input masks at countdown 360, 301, 300 and below; `AuthoritativeMatchBehaviorTests.cpp`: real canonical survivor input | Host runtime emits press/release changes during the active second, not the frozen remainder. A separate real canonical Game/Round/World run consumes survivor press, release, and changed movement masks after round end, then rejects input once frozen. |
| 6: Team preferences | NET-SET-001–003, NET-SET-AC-001, SET-042, SET-043 | `NetworkSessionRuntimeTests.cpp`: menu Team preferences and capacity scenario | Application-dispatched menu edits cycle Teams(4, FF on) → Deathmatch → Predator → Teams. Both host and guest apply zero teams/FF off in non-team modes, retain four teams/FF on on return, clear readiness, and accept a subsequent rounds edit. |
| 7: Tab dismissal | NET-AC-017 | `NetworkSessionRuntimeTests.cpp`: Application Return/repeat/Tab/modal | Production SDL/Application press/release dispatch opens, closes, and reopens the overlay. Repeated keydowns and key release do not toggle it. |
| 8: Leave after reconnect restore | NET-AC-011, NET-AC-016, NET-OWN-AC-005 | `AdmissionCompatibilityTests.cpp`: restore-before-response barrier | Real TCP adapter withholds the accepted reconnect response after the host has restored the binding. The actual guest state machine receives local Leave, emits an admitted Leave, exits successfully, and peers observe immediate removal of the participant and all its players, with cleared readiness rather than a new reservation. |
| 9: Shared non-input budget | Network trust bandwidth/action policy: 30/s, burst 60; offender-local enforcement | `AdmissionCompatibilityTests.cpp`: production ingress budget | A fixed injected host clock makes the burst boundary deterministic. Valid owned-person changes alternate with Ready actions. All 60 initial actions and 30 replenished actions apply; the next name mutation in each window does not. Two adjacent over-limit windows close the offender. A separate observer continues making valid changes. |
| 10: Weapon opacity | BON-013, NET-VIS-008, NET-VIS-AC-003 | `NetworkSessionRuntimeTests.cpp`: canonical held-weapon draws; `RecordingRenderer.h` | Calls the actual canonical presenter using loaded built-in weapon resources. Records material color, masking, and blend mode: normal and Predator-only weapons are opaque; Invisibility and combined states are alpha 51/255 with source-alpha blending. Expired Invisibility restores opacity; dead players draw no held weapon. |
| 11: Complete bounded outcomes | NET-RES-001, NET-RES-002, NET-RES-AC-001 | `StateReplicationTests.cpp`: retained outcome projection; `NetworkSessionRuntimeTests.cpp`: recorded summary/retained-result text draws | Projection preserves 14 winners across 99 rounds plus match outcome, complete 64-byte names, departure flags, and adjacent IDs near UINT64_MAX. Actual menu/Font draws keep outcome headers within the logical canvas. Keyboard horizontal scrolling and focused mouse-wheel vertical scrolling expose every full name and the exact identity associated with that name in both final summary and retained result. No live roster name lookup is available in that presentation fixture. |

### Deliberate test seams

- Existing test-only private access is used for fixture state and observations; no production diagnostic API was added.
- The reconnect adapter delegates to real TCP. HeadlessServer normally disables production replication whenever a client factory is injected; this test explicitly restores that setting on its test instance so the adapter does not select the legacy fake protocol.
- The final-summary membership test substitutes deterministic combat completion, not admission, removal, replication, or return-to-lobby logic. The separate End-from-summary test uses real canonical combat/hazards with a small temporary arena and waits for natural completion.
- Renderer recording checks submitted geometry/materials and Font cache text. It is not GPU pixel validation, an acceptance screenshot, or an accessibility certification.
- The 250 ms input-edge watchdog detects growing backlog; it is not a replacement for any approved responsiveness SLO or a queue-depth measurement.

## Diagnostic execution

Environment: Linux x86-64, Docker image `duel6r-build:local`, image ID `sha256:037910295700dc301b5197edea374b555948d6cf7bef5d8f6752c74b0cb0014b`, GNU C++ 13.3.0, Release, gl4, Lua ON, BUILD_TESTING ON. Runtime tests use Xvfb and `SDL_AUDIODRIVER=dummy` through their existing CTest registration.

The missing artifact required diagnostic compilation. A disposable container held an independent checkout copy at `/workspace` and build outputs at `/tmp/pr83-build`. The tester owns its cleanup; no runtime bundle or evidence image is copied into the repository.

Configuration command and consolidated equivalent of the required-target builds (the targets were built incrementally during authoring):

```sh
docker exec duel6r-pr83-tester cmake -S /workspace -B /tmp/pr83-build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DD6R_RENDERER=gl4 -DD6R_WITH_LUA=ON
docker exec duel6r-pr83-tester cmake --build /tmp/pr83-build --target \
  duel6r-network-session-runtime-tests duel6r-state-replication-tests \
  duel6r-admission-compatibility-tests duel6r-authoritative-match-behavior-tests -j 4
```

Focused checks used the existing `D6R_TEST_FILTER` substring convention, for example:

```sh
docker exec -e D6R_TEST_FILTER='PR83 production guest Leave' duel6r-pr83-tester \
  ctest --test-dir /tmp/pr83-build -R '^duel6r-admission-compatibility-tests$' --output-on-failure
docker exec -e D6R_TEST_FILTER='PR83 summary and retained' duel6r-pr83-tester \
  ctest --test-dir /tmp/pr83-build -R '^duel6r-network-session-runtime-tests$' --output-on-failure
docker exec -e D6R_TEST_FILTER='PR83 real canonical' duel6r-pr83-tester \
  ctest --test-dir /tmp/pr83-build -R '^duel6r-authoritative-match-behavior-tests$' --output-on-failure
docker exec -w /workspace/resources -e D6R_TEST_FILTER=PR83 duel6r-pr83-tester \
  /tmp/pr83-build/duel6r-state-replication-tests
```

All 12 added test cases passed in their latest affected-scenario diagnostics. They were not all rerun together after every edit. Unaffected evidence is preserved where only another independent test body changed; the weapon draw test was rerun after changing its shared recording renderer. No broad suite was run. Initial failures exposed incomplete test fixtures (missing admission content, protocol selection behind the reconnect adapter, unobserved menu entry, zero participant/seed identities), not confirmed production defects; these fixture defects were corrected and affected checks rerun. One attempted build used a nonexistent shortened admission target name, then used the registered compatibility target successfully. The authoritative target also emitted an existing signedness warning from an unchanged assertion through TestHarness.

## Required integrated-checkpoint verification

1. Developer integrates the exact handed-off test bytes and supplies the new tip SHA, source approval, artifact identity/hash, build configuration, and required test targets/resources. Do not use this mutable authoring checkout as gate evidence.
2. Confirm the artifact corresponds to that tip and the specification baseline above. If production or artifact bytes change during verification, stop and report stale evidence.
3. Run all `PR83` cases in the four targets on the approved artifact, Docker-only. Keep resources/binaries immutable and put CTest logs, temporary fixture resources, and any mutable application state in a separate verification area. Use the supplied artifact rather than rebuilding unless a required target is absent or incompatible.
4. Record tip SHA, specification baseline, requirement IDs, image/configuration, exact command, expected/actual result, and conclusion for each mapped gate. Final source approval and artifact verification are prerequisites for a pass recommendation.
5. Run the existing broader relevant suites once at the integrated tree if requested for regression closure. Do not rerun already-covered focused cases after a broad run unless diagnosing a failure.

## Remaining focused checks and gaps

- Linux gl4 was exercised; Windows, mixed-platform sessions, and other renderer backends were not. Verify those only when required artifacts are supplied.
- Test a real charged weapon release from both host and guest during the active first second, including resulting projectile/charge behavior. Current tests verify the release mask and real canonical input consumption, but do not assert a charged projectile from a guest in that interval.
- Extend the maximum-cardinality run when sustained performance evidence is required: exercise distinct movement/fire/pick edges, log accepted/outstanding commands without changing freshness limits, and measure the approved responsiveness budgets. The current run covers crouch edges, bounded observed latency, and continued guest/simulation progress, not all action families or long-duration performance.
- Validate body and weapon compositing together on the supported renderers and viewport sizes. Current alpha tests inspect the weapon draw; current text tests inspect the fixed logical canvas and reachable complete outcome text, not GPU clipping/scaling at every viewport.
- The reconnect race test begins in lobby. Repeat the same ordering in active match/final summary and cover Leave before any retry and expiry-boundary precedence using the existing lifecycle tests or a focused extension.
- For product acceptance screenshots or hosted checks, route the request through team. This test handoff supplies neither.
