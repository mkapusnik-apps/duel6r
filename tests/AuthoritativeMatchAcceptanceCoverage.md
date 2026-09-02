# Authoritative Headless Match Acceptance Coverage

This matrix maps `AHM-AC-001` through `AHM-AC-034` to tester-owned application evidence. It does not make a product acceptance claim.

Abbreviations:

- **Behavior**: `duel6r-authoritative-match-behavior-tests`.
- **Process**: `duel6r-authoritative-match-process-tests` using the real canonical headless runtime.
- **Golden**: canonical-default and explicit compact-combat Linux golden files.
- **Local sanitizer**: `LocalPlayShitThrowerSanitizerTests.sh` under ASan, UBSan, and leak detection.
- **Local animation**: `duel6r-local-play-pick-animation-tests` using the shipped Local Play Aseprite animation and production sprite update behavior.

| Criterion | Evidence | Local conclusion |
|---|---|---|
| AHM-AC-001 | Process starts and completes the canonical headless match without application renderer, audio, or device initialization. | Covered. |
| AHM-AC-002 | Behavior `strict tick ordering ownership values bounds and advancement authority`, unauthorized/malformed 130-action flood, accepted/rejected counters, and per-owner sequence maxima; Process canonical events, checkpoints, and forbidden-access counters. | Covered. |
| AHM-AC-003 | Behavior `mode matrix completes with documented winner and team assignment` exercises Deathmatch, Predator, and all six team variants. | Covered. |
| AHM-AC-004 | Behavior mode matrix, no-winner/assistance/friendly-fire, and opponent/environmental/self/team death cases; Process three-round team world. | Covered. |
| AHM-AC-005 | Behavior `validates settings roster levels weapons scripts and round bounds before content`; Process settings and content correction outcomes. | Covered. |
| AHM-AC-006 | Behavior completed results retain their starting configuration across rounds; no runtime configuration mutation API exists. | Covered. |
| AHM-AC-007 | Behavior accepts 1 and 99 and rejects 0 and 100; Process rejects `--rounds=0`. | Covered. |
| AHM-AC-008 | Behavior fixed-plan six-round replay and explicit fixed-level selection while the complete frozen set remains configured; Process fixed `duel_01` result and Golden. | Covered. |
| AHM-AC-009 | Behavior shuffle-all permutation, full frozen-set validation, cycle assertions, deterministic replay, and subset rejection; Process rejects the legacy subset flag. | Covered. |
| AHM-AC-010 | Behavior random-plan choices use the complete frozen set, remain playable and deterministic for equal seeds, and reject subsets; Process rejects the legacy subset flag. | Covered. |
| AHM-AC-011 | Golden random trace contains the seeded round orientation decision; replay stress compares exact output across 128 seeds. | Covered locally; native Windows comparison remains required. |
| AHM-AC-012 | Behavior `round-end boundaries update exactly one second then freeze five seconds`. | Covered. |
| AHM-AC-013 | Behavior accepts host advancement during the allowed round-end phase. | Covered. |
| AHM-AC-014 | Behavior rejects guest and pre-winner advancement; action validation rejects unsupported values and phases. | Covered. |
| AHM-AC-015 | Behavior direct and hosted-controller End authorization, including successful host End after a guest accepts sequence `UINT64_MAX`; Process exact intentional-end output and absence of a session result. | Covered. |
| AHM-AC-016 | Behavior deterministic seeded replay; Process result and diagnostics retain the requested nonzero seed. | Covered. |
| AHM-AC-017 | Golden random trace covers orientation, spawn order, weapons, ammo, pickups, bonuses, and duration; forbidden global RNG count is zero; 128-seed exact replay stress. | Covered. |
| AHM-AC-018 | Debug and Release Linux semantics and repeated exact replay are equal; MinGW compiles all contracts and application targets. | Partial: native Windows execution/golden comparison is external evidence. |
| AHM-AC-019 | Behavior rejects optional-script settings and script content; Process result reports scripts disabled. | Covered. |
| AHM-AC-020 | Behavior complete result assertions and canonical JSON bounds; Process verifies complete session, round, player, team, and statistics output. | Covered. |
| AHM-AC-021 | Behavior and Process verify winner/player ordering and statistics precedence inputs. | Covered. |
| AHM-AC-022 | Behavior team results verify fixed team order and points-bearing team rows. | Covered. |
| AHM-AC-023 | Behavior canonical JSON excludes persistence fields; Process runs in isolated headless resources and excludes Elo/history/profile data. | Covered. |
| AHM-AC-024 | Behavior terminal interruption preserves the no-winner result; Process exact interrupted identifier, copy, departure, and completed-round fields. | Covered. |
| AHM-AC-025 | Behavior settings-before-content precedence; Process exact settings failure identifier, copy, and exit status. | Covered. |
| AHM-AC-026 | Behavior frozen-content validation and malformed first/later-plan level blocking before world start, with readiness clearing, retry blocking, and permitted host End; Process production preflight parses malformed first and later-plan frozen levels before any round/result and reports exact unavailable-content output; admission correction tests cover exact frozen bytes. | Covered. |
| AHM-AC-027 | Behavior runtime failure publishes no result; Process exact identifier, copy, and exit status 3. | Covered. |
| AHM-AC-028 | Behavior invalid settings precede missing content; terminal-state tests prevent later outcomes replacing the selected result. | Covered. |
| AHM-AC-029 | Behavior settings failure, malformed-level preflight failure, runtime failure, End, and cleanup failure publish no partial result; Process validates result-line presence by outcome. | Covered. |
| AHM-AC-030 | Behavior cleanup controls resource-release truth; Process requires `cleanupConfirmed=true` and exact cleanup-failure output. | Covered. |
| AHM-AC-031 | Process asserts exact meanings and identifiers for statuses 0, 2, 3, and 4, including bounded parse failures. | Covered. |
| AHM-AC-032 | Release shared-arena application test passes; Local sanitizer starts real Local Play for every roster size 2 through 15, exercises press/release input, and exits without sanitizer, SDL, icon, or leak output. Local animation verifies the shipped ping-pong Pick animation remains locked through tick 65 and completes at tick 66, while Process verifies canonical headless pickup remains locked at tick 29, unlocks at tick 30, and accepts the held Pick action on tick 31. | Covered. |
| AHM-AC-033 | Tests consume public authoritative-match, hosted-controller, process, and serialization boundaries without implementing downstream lobby/client behavior. | Scope boundary evidenced; downstream issue verification remains separate. |
| AHM-AC-034 | The suites test authoritative-match behavior only and make no playable-network or release-readiness claim. | Product/scope decision, not an application pass criterion. |

## Canonical subsystem evidence

The Process suite additionally exercises all 13 weapons enabled by shipped content, a frozen-content-only Shit Thrower scenario, projectile ownership, ammo/reload, Bow charge/release, exact 30-tick canonical weapon-pick lock boundaries, weapon pickups and swaps, timed bonuses and expiry, liquids and drowning state, elevators, hazards, sudden death, trees and fire, spawn identities, orientations, fixed/team multi-round plans, zero input, held-input release, departures, cleanup, and forbidden random/time access counters.
