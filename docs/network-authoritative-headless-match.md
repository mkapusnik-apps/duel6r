# Authoritative headless network match

## Status and authority

This document is the authoritative product specification for GitHub issue #32. It defines the first-release authoritative headless match behavior.

This document defines target behavior. It does not claim implemented or playable network support.

The approved network scope and journeys are in [`network-play-first-release.md`](network-play-first-release.md). Local gameplay behavior is in [`features.md`](features.md).

The current networking scaffold status is in [`networking.md`](networking.md). Compatibility and admission behavior is in [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md).

The player-hosted service lifecycle is in [`network-host-service-lifecycle.md`](network-host-service-lifecycle.md). This document must not change those lifecycle boundaries.

## Terms

- **Authoritative service:** The headless service that owns canonical match decisions and state.
- **Session playable level:** A playable level in the frozen, compatible gameplay content for the session.
- **Level plan:** The approved rule that selects the level for each round.
- **Authoritative action sequence:** The ordered valid gameplay actions that the authoritative service accepts.
- **Semantic determinism:** Equal gameplay decisions, state, progression, and results without a requirement for equal process memory or binary representation.
- **Completed result:** A result from a match that finishes its configured final round.
- **Interrupted result:** A valid no-winner result caused by an approved roster reduction below two players.

## Product goal

The authoritative service must run a complete supported match without renderer, audio, or local input-device initialization.

The authoritative service must own canonical world state, score, round state, winner decisions, random decisions, and match progression.

A client or participant must not directly change canonical match state.

## Supported modes

First-release network matches must support this complete matrix:

| Mode | Team count | Friendly fire |
|---|---:|---|
| Deathmatch | Not applicable | Not applicable |
| Predator | Not applicable | Not applicable |
| Team deathmatch | 2 | Off |
| Team deathmatch | 2 | On |
| Team deathmatch | 3 | Off |
| Team deathmatch | 3 | On |
| Team deathmatch | 4 | Off |
| Team deathmatch | 4 | On |

The authoritative service must apply the applicable mode requirements in [`features.md`](features.md).

These requirements include team assignment, friendly fire, scoring, assistance, winner selection, and no-winner behavior.

Team deathmatch must assign teams from roster position modulo the selected team count. Alpha, Bravo, Charlie, and Delta must keep their documented order.

The service must not require each configured team to contain a player. Local Play does not validate team population.

## Allowed match settings

The host may configure only these authoritative match settings:

- mode;
- level plan;
- round limit;
- Assistance;
- Quick Liquid;
- Burnable Trees.

The service must freeze these settings when the match starts. The service must keep them for all rounds in that match.

After a return to the lobby, the host may configure these settings for a later match.

Weapon enablement, ammunition ranges, level data, and gameplay definitions must come from the frozen supported gameplay content.

The product must not present those content values as match settings owned by this specification.

## Round limit

A first-release network match must use an integer round limit from 1 through 99.

The service must reject zero, an empty value, a negative value, a value above 99, or a non-integer value.

A network match must not be unlimited. Local Play must keep its existing zero and unlimited match behavior.

The match must finish after the configured final round completes.

## Level plans

The host must select exactly one of these level plans:

### Fixed level

The host must select one session playable level. Every round must use that level.

### Shuffle all levels

At match start, the service must randomly reorder all session playable levels.

Each round must use the next level in that order. After the last level, the next round must use the first level again.

The service must keep the same reordered list for the complete match.

### Random level

Each round must independently select one session playable level. Consecutive rounds may use the same level.

### Shared level behavior

The selected level plan must contain at least one session playable level.

Each round must independently select normal or mirrored orientation with equal probability.

All gameplay-affecting level and orientation choices must use the authoritative match seed.

Presentation-only background selection is not an authoritative level decision.

## Match start

The service may start a match only when the first-release match-start invariants are true.

The service must receive one frozen valid roster, one valid setting set, and one valid level plan before it creates the first round.

The service must clear a retained prior result before it starts the new match.

The service must not create a partial round or result when match-start validation fails.

## Round lifecycle and advancement

The authoritative service must apply the local round-start, world, combat, scoring, and winner rules.

When a mode produces a winner or no-winner outcome, the service must start one six-second round-end delay.

During the first second, the service must continue normal world and player updates.

During the final five seconds, the service must stop normal round updates.

After the delay, the service must start the next non-final round automatically.

Only the host may request advancement before the delay ends. The request is valid only after the mode produces a winner or no-winner outcome.

A guest must not advance a round. An arbitrary participant key press must not advance a network round.

Network play must not support the local Shift+F1 force-advance action before a winner exists.

The final round must not advance to another round. It must produce the completed session-only result.

## Host End session action

Only the host may use the existing End session action. This specification does not add a separate End match action.

An accepted End session action must stop match progression. It must not produce a match winner or an interrupted result.

The service must discard all session-only results when the host ends the session.

The service must report `authoritative-match-ended-intentionally` with exactly:

`Authoritative match ended by the host.`

Issue #36 owns intentional host-end notice behavior. Issue #38 owns its presentation.

## Authoritative seed and random decisions

Before the first round starts, the authoritative service must create one nonzero match seed.

The service must keep that seed for the complete match. It must include the seed in the session-only result.

The seed and authoritative action sequence must determine every gameplay-affecting random choice.

The scope includes:

- level shuffle order;
- random level selection;
- level orientation;
- random player starting state and placement choices;
- Predator selection;
- starting weapons;
- starting ammunition;
- pickup category and type;
- pickup position;
- bonus duration and amount;
- weapon or bonus behavior that uses randomness;
- each other random decision that can change gameplay state, score, progression, or results.

The service must not start a new time-based random sequence after the match starts.

Presentation-only choices do not use the authoritative seed. This exclusion includes cosmetic backgrounds and sounds.

## Linux and Windows semantic determinism

Linux x86-64 and Windows x86-64 hosts must provide semantic determinism.

The comparison inputs are:

- the same supported network release;
- the same gameplay content;
- the same settings;
- the same nonzero seed;
- the same authoritative roster order;
- the same valid authoritative action sequence.

For those inputs, both hosts must produce:

- the same gameplay-affecting random choices;
- the same accepted action order;
- the same damage, death, pickup, and scoring decisions;
- the same authoritative gameplay state at equivalent decision points;
- the same round winners and no-winner outcomes;
- the same round and match progression;
- the same final session-only result.

The product does not require equal process memory, diagnostic formatting, rendering, audio timing, or binary floating-point representation.

Platform differences must not change an authoritative gameplay decision or result.

## Network script policy

The authoritative service must disable all optional Lua and profile scripts for first-release network matches.

It must not load or execute:

- a participant profile script;
- a host profile script;
- another optional gameplay script.

The first-release gameplay-content manifest must contain no enabled optional gameplay script.

Local Play must keep the scripting behavior in [`features.md`](features.md).

## Session-only result

The authoritative service must create one session-only result after a completed match or approved interruption.

The result must not write local statistics, Elo, people, profiles, saves, or persistent history.

### Match fields

The result must contain:

- the exact label `Session only`;
- result state `Completed` or `Interrupted`;
- mode;
- team count when applicable;
- friendly-fire value when applicable;
- Assistance value;
- Quick Liquid value;
- Burnable Trees value;
- level plan;
- configured round limit;
- authoritative nonzero match seed;
- completed-round count;
- final-round winner identity, winning team, or `No winner`.

The final winner field records the final round outcome. It does not create a separate match-champion rule.

### Round fields

The result must contain one entry for each completed round. Each round entry must contain:

- round number;
- level logical identity;
- normal or mirrored orientation;
- round winner identities, winning team, or `No winner`;
- the authoritative roster order for that round.

### Player fields

Each player row must contain:

- stable player identity;
- stable owner-participant identity;
- session display name;
- team when applicable;
- `Departed` state when applicable;
- rounds played;
- shots;
- hits;
- kills;
- deaths;
- assists;
- wins;
- penalties;
- survival time;
- damage;
- assisted damage;
- total points.

Total points must equal kills plus wins plus assists minus penalties.

The result must contain per-round player values and cumulative match values.

### Player ranking

The service must rank player rows by this precedence:

1. total points, descending;
2. wins, descending;
3. damage, descending;
4. authoritative roster order.

### Team ranking

The service must total player points for each configured team.

It must rank team rows by this precedence:

1. team point total, descending;
2. Alpha, Bravo, Charlie, then Delta.

It must use the player ranking rules inside each team.

### Excluded result data

The result must not contain:

- Elo, Elo trend, or Elo game count;
- a local person record;
- saved profile state;
- an endpoint;
- a credential;
- persistent history.

## Approved interruption

Issue #36 owns detection of leave, disconnect, reservation expiry, and roster removal.

The authoritative service must consume an approved authoritative removal outcome without redefining that lifecycle.

If fewer than two roster players remain, the service must end the match without a winner.

It must preserve completed rounds. It must discard an incomplete active round.

It must create a result with exactly:

`Session only • Interrupted • No winner`

This outcome must use machine identifier `authoritative-match-interrupted-no-winner` with exactly:

`Authoritative match ended without a winner.`

An approved interruption is not a runtime failure.

## Match completion

After the configured final round, the service must create the completed session-only result.

It must use machine identifier `authoritative-match-completed` with exactly:

`Authoritative match completed.`

A no-winner round inside a completed match must not make the match execution fail.

## Match-start validation outcomes

The service must validate settings before gameplay-content prerequisites.

### Invalid settings

Invalid settings include:

- an unsupported mode;
- an unsupported team count or friendly-fire combination;
- an invalid round limit;
- an unsupported setting;
- an invalid roster cardinality;
- an invalid level-plan value.

The service must use machine identifier `authoritative-match-settings-invalid` with exactly:

`Match settings are invalid. Correct the settings and try again.`

The service must not start the match or create a result. It must clear every participant's readiness.

The hosted session may accept corrected settings for a later start request.

### Gameplay content unavailable

This outcome applies when:

- the selected level plan contains no session playable level;
- the selected fixed level is unavailable;
- a required selected level cannot load;
- no weapon is enabled.

The service must use machine identifier `authoritative-match-content-unavailable` with exactly:

`The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.`

The service must not start the match or create a result. It must clear every participant's readiness.

The service must block another match start in that hosted session. The host may end the session.

A manifest failure before listener readiness must keep the issue #30 identifier and behavior.

## Runtime failure

An unrecoverable simulation error or authoritative-state invariant failure after match start is a runtime failure.

The service must use machine identifier `authoritative-match-runtime-failed` with exactly:

`The authoritative match stopped unexpectedly.`

The service must stop authoritative progression. It must reject later gameplay actions.

It must create no winner. It must discard the incomplete round and all session-only results.

It must start orderly shutdown. It must not restart or restore the failed match.

The host application must map the service stop to existing machine identifier `host-service-stopped-unexpectedly` with exactly:

`Hosted session stopped unexpectedly.`

The runtime failure must not produce or imply intentional host end for a guest.

## Terminal outcome precedence

The service must use this precedence before the first round:

1. an accepted host End session action;
2. `authoritative-match-settings-invalid`;
3. `authoritative-match-content-unavailable`;
4. successful match start.

During a match, the service must use this precedence when terminal outcomes compete:

1. an accepted host End session action;
2. `authoritative-match-runtime-failed`;
3. `authoritative-match-interrupted-no-winner`;
4. `authoritative-match-completed`.

The first established outcome in the applicable order must remain the match outcome.

A cleanup failure must replace the process result with `authoritative-match-shutdown-failed`. It must not create or restore a match result.

## Cleanup and result integrity

The authoritative service must release match resources during orderly shutdown.

It must not report successful cleanup while owned match resources remain active.

A completed result must become available only after all required final-round decisions are complete.

An interrupted result must contain only completed rounds and the approved interruption outcome.

Invalid setup must create no partial round or result.

Runtime failure must discard the incomplete round and all session-only results.

Host End session must discard all session-only results.

Cleanup failure must not publish a partial result as completed or interrupted.

## Process exit meanings

The headless process must use these exact exit statuses:

| Exit status | Meaning | Permitted machine identifiers |
|---:|---|---|
| `0` | The service reached an approved terminal outcome and released its resources. | `authoritative-match-completed`, `authoritative-match-interrupted-no-winner`, `authoritative-match-ended-intentionally` |
| `2` | Setup or gameplay-content validation blocked the match before the first round. The service released its resources. | `authoritative-match-settings-invalid`, `authoritative-match-content-unavailable` |
| `3` | The authoritative simulation failed after match start. The service released its resources. | `authoritative-match-runtime-failed` |
| `4` | The process could not confirm complete match-resource cleanup. | `authoritative-match-shutdown-failed` |

An exit status applies only when the headless process exits. A hosted service may remain active after a correctable settings failure.

If that service exits later, its final terminal reason must determine its exit status.

`authoritative-match-shutdown-failed` must use exactly:

`Authoritative match cleanup did not complete.`

An approved no-winner or interrupted outcome may use exit status `0`. It is not a process failure.

The process result must identify one permitted machine identifier. It must not claim transport, replication, graphical networking, or playable end-to-end network support.

## Local Play parity and exceptions

The authoritative service must preserve the applicable gameplay rules in [`features.md`](features.md).

Parity includes:

- all supported mode and team rules;
- combat, weapon, pickup, bonus, water, elevator, sudden-death, and Burnable Trees behavior;
- scoring, assistance, winner, and no-winner rules;
- level shuffle, random selection, mirroring, and round progression rules;
- authoritative in-match statistics.

First-release network play has these approved exceptions:

- the round limit must be from 1 through 99;
- an unlimited match is not supported;
- only the host may advance a completed round early;
- Shift+F1 force advancement is not supported;
- an arbitrary key press does not advance a round;
- all optional Lua and profile scripts are disabled;
- results remain session-only and do not update local statistics or Elo;
- renderer, audio, and local input-device initialization are absent from the authoritative service.

These exceptions must not change Local Play. Local Play must remain independent of every network service.

## Downstream boundaries

### Issue #34

Issue #34 owns stable world identities, snapshots, incremental state, recovery, and read-only replicated client state.

It must replicate the canonical decisions and result data from this specification. It must not redefine them.

### Issue #36

Issue #36 owns leave, disconnect, reservation, reconnect, expiry, removal, intentional host-end notice, and restoration behavior.

This specification owns only the authoritative match outcome after #36 supplies an approved removal or End session outcome.

### Issue #38

Issue #38 owns graphical presentation, navigation, visible status, and error screens.

It must use the exact identifiers, copy, outcomes, and result meaning in this specification. This document does not define layouts.

### Issue #40

Issue #40 owns artifacts, deployment instructions, operational configuration, and release-facing support claims.

It must document the process exit meanings without claiming playable networking before issue #41 completes.

## Non-goals

- Transport or framing behavior.
- State replication or client rendering.
- Remote-input transport, validation, or acknowledgment.
- Prediction, interpolation, reconciliation, or lag compensation.
- Disconnect detection, reconnect, or host migration.
- Graphical network UI or visual design.
- Persistent network statistics or Elo.
- Optional Lua or profile scripts in a network match.
- Unlimited network matches.
- Dedicated-server product support.
- A playable-network or release-readiness claim.

## Acceptance criteria

- **AHM-AC-001 — Headless authority:** The authoritative service must complete a match without renderer, audio, or local input-device initialization.
- **AHM-AC-002 — Canonical ownership:** Only the authoritative service must change canonical world, score, round, winner, random, and progression state.
- **AHM-AC-003 — Mode matrix:** The service must support Deathmatch, Predator, and all six approved Team deathmatch variants.
- **AHM-AC-004 — Mode parity:** Each mode must apply the applicable local team, friendly-fire, scoring, assistance, winner, and no-winner rules.
- **AHM-AC-005 — Settings:** The host may configure only mode, level plan, round limit, Assistance, Quick Liquid, and Burnable Trees.
- **AHM-AC-006 — Frozen settings:** The service must keep match settings unchanged from match start through the final result.
- **AHM-AC-007 — Round limit:** A network match must accept only an integer round limit from 1 through 99 and must not support unlimited play.
- **AHM-AC-008 — Fixed level:** A fixed-level plan must use the selected session playable level for every round.
- **AHM-AC-009 — Shuffle levels:** A shuffle plan must reorder all session playable levels once and must cycle through that order.
- **AHM-AC-010 — Random level:** A random plan must select independently for each round and may repeat a level.
- **AHM-AC-011 — Orientation:** Each round must select normal or mirrored orientation with equal probability from the match seed.
- **AHM-AC-012 — Round timing:** A round end must use the six-second update, freeze, and automatic-advance behavior in this specification.
- **AHM-AC-013 — Advancement authority:** Only the host may advance after a round outcome and before automatic advancement.
- **AHM-AC-014 — Unsupported advancement:** Guest advancement, arbitrary-key advancement, and pre-winner Shift+F1 advancement must not change the round.
- **AHM-AC-015 — Host End:** Only accepted host End session must produce `authoritative-match-ended-intentionally` and discard session results.
- **AHM-AC-016 — Seed:** Each match must use one nonzero seed that remains fixed and appears in its session-only result.
- **AHM-AC-017 — Random scope:** The seed and authoritative actions must determine every listed gameplay-affecting random choice.
- **AHM-AC-018 — Cross-platform determinism:** Equal approved inputs on Linux and Windows must produce semantically equal authoritative decisions, progression, and results.
- **AHM-AC-019 — Script exclusion:** A network match must not load or execute an optional Lua, profile, or gameplay script.
- **AHM-AC-020 — Completed result:** A completed match must produce the complete session-only match, round, player, and team data in this specification.
- **AHM-AC-021 — Player ranking:** Player rows must follow points, wins, damage, and roster-order precedence.
- **AHM-AC-022 — Team ranking:** Team rows must follow team points and fixed team-order precedence.
- **AHM-AC-023 — No persistence:** A network result must not change local statistics, Elo, people, profiles, saves, or history.
- **AHM-AC-024 — Approved interruption:** A roster reduction below two players must preserve completed rounds and produce the exact interrupted no-winner outcome.
- **AHM-AC-025 — Settings failure:** Invalid settings must use the exact identifier, copy, readiness behavior, and retry boundary in this specification.
- **AHM-AC-026 — Content failure:** Unavailable gameplay prerequisites must use the exact identifier, copy, readiness behavior, and restart boundary in this specification.
- **AHM-AC-027 — Runtime failure:** An authoritative runtime failure must stop progression, discard all results, and use the exact failure identifier and copy.
- **AHM-AC-028 — Outcome precedence:** Competing start and match outcomes must follow the applicable precedence order.
- **AHM-AC-029 — Result integrity:** Invalid setup, runtime failure, Host End session, and cleanup failure must not publish a partial result.
- **AHM-AC-030 — Cleanup:** The process must report successful cleanup only after it releases all owned match resources.
- **AHM-AC-031 — Exit status:** Exit statuses `0`, `2`, `3`, and `4` must have only the meanings and identifiers in this specification.
- **AHM-AC-032 — Local independence:** The network exceptions must not change Local Play behavior or require a network service for Local Play.
- **AHM-AC-033 — Downstream boundaries:** Issues #34, #36, #38, and #40 must consume this behavior without redefining it.
- **AHM-AC-034 — Scope truth:** Completion of issue #32 alone must not support a playable-network or release-readiness claim.

## Future evidence expectations

When `team` requests product acceptance, the evidence packet must identify the implementation state, environment, scenario, and observation.

### Required evidence

- Tester results must cover each supported mode and friendly-fire variant through final results.
- Tester results must cover each level plan, each Boolean setting, round limits `1` and `99`, and rejected limits.
- Tester results must cover host advancement, automatic advancement, and rejected guest or pre-winner advancement.
- Tester results must cover the complete random-decision scope with repeated fixed-seed scenarios.
- Tester results must compare semantic decisions and results on Linux x86-64 and Windows x86-64.
- Tester results must confirm that network matches do not load or execute optional scripts.
- Tester results must confirm completed, interrupted, invalid-settings, unavailable-content, runtime-failure, End session, and cleanup-failure outcomes.
- Tester results must confirm every exact identifier, fixed copy, result field, ranking rule, and process exit meaning.
- Reviewer evidence must assess authoritative ownership, result integrity, deterministic decision boundaries, script exclusion, and Local Play isolation.
- DevOps evidence must identify supported Linux and Windows build and headless execution results when hosted evidence is applicable.
- The evidence packet must show that completion does not enable a playable-network claim.

### Optional supporting evidence

- A fixed-seed authoritative decision trace may support the semantic determinism comparison.
- A sample completed result and interrupted result may support schema review.
- Bounded shutdown records may support cleanup conclusions.
- Resource-lifecycle diagnostics may support the no-owned-resource conclusion.

Screenshots and recordings are not required for this headless issue. They do not prove authoritative behavior or cleanup.

Issue #41 remains the complete network-play release gate.
