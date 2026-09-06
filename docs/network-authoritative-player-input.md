# Authoritative network player input

## Status and authority

This document is the authoritative product target for GitHub issue [#33](https://github.com/mkapusnik-apps/duel6r/issues/33), under [#27](https://github.com/mkapusnik-apps/duel6r/issues/27).

It defines target behavior. It does not claim implemented or playable network support.

The approved network scope is in [`network-play-first-release.md`](network-play-first-release.md). Local controls are in [`features.md`](features.md).

The authoritative match is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md). Player identities are in [`network-state-replication.md`](network-state-replication.md).

Transport, admission, and trust limits are in [`networking.md`](networking.md), [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md), and [`network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md).

## Terms

- **Player input:** The complete pressed or released state of the seven player actions in `INP-012`.
- **Input command:** One participant request to apply player input for one owned player.
- **Input sequence:** A positive value that orders input commands for one player during one match.
- **Target tick:** The 60 Hz authoritative match tick requested by an input command.
- **Next unprocessed tick:** The earliest authoritative match tick that the service has not processed. This document calls it `N`.
- **Effective tick:** The tick when an accepted input can first affect canonical state.
- **Applied acknowledgment:** The authoritative confirmation that an input affected its effective tick.
- **Network input client:** The participant-side capability that creates input commands for owned players and receives authoritative input outcomes.
- **Graphical network-session composition:** The graphical application journey that connects local controls, network input, transport, replicated state, and presentation.

## Product goal

- **NIN-AUTH-001** Each participant must control only the roster players that the participant owns.
- **NIN-AUTH-002** The authoritative service must validate each gameplay input before that input can change canonical state.
- **NIN-AUTH-003** The authoritative service must apply accepted input in a deterministic order at 60 Hz.
- **NIN-AUTH-004** Rejected input must not change canonical state, score, progression, or another player's input.

## Local devices and player ownership

- **NIN-OWN-001** Each network input client must create commands only for players that its participant owns.
- **NIN-OWN-002** A participant may own and control more than one roster player.
- **NIN-OWN-003** The same local control preset may control multiple players owned by one participant, as permitted by `INP-007`.
- **NIN-OWN-004** Each owned player must have an independent input command and input sequence.
- **NIN-OWN-005** Host-player commands must follow the same validation and authoritative tick rules as guest-player commands.
- **NIN-OWN-006** Graphical network-session composition must preserve the actions and local device behavior in `INP-001` through `INP-016`.
- **NIN-OWN-007** Network input must not add a player action or change an existing action meaning.

## Runtime boundary

- **NIN-BOUND-001** Host and guest participants must use the common network input client behavior.
- **NIN-BOUND-002** The authoritative service must receive, validate, apply, and acknowledge input commands.
- **NIN-BOUND-003** Graphical network-session composition must connect each participant's selected local controls to its network input client.
- **NIN-BOUND-004** The authoritative service must not initialize local input devices.

## Input state and sequence

- **NIN-SEQ-001** An input command must identify one admitted participant and one stable player identity owned by that participant.
- **NIN-SEQ-002** An input command must contain one positive input sequence and one target tick.
- **NIN-SEQ-003** An input command must contain the complete state of all seven player actions.
- **NIN-SEQ-004** A state with no pressed action is valid. The service must use it to release held actions.
- **NIN-SEQ-005** The service must retain the last applied input state until a later input state applies or an input-clear rule occurs.
- **NIN-SEQ-006** Input sequences must increase for each player during one match.
- **NIN-SEQ-007** A sequence gap is valid. A sequence value must not wrap during one match.
- **NIN-SEQ-008** The service must reject a sequence that is equal to or lower than the highest accepted sequence for that player.
- **NIN-SEQ-009** A rejected command must not advance the highest accepted sequence.
- **NIN-SEQ-010** A command with an unknown action or an invalid value must not change the retained input state.

## Target-tick window and application

- **NIN-TICK-001** The authoritative match must process gameplay at 60 Hz.
- **NIN-TICK-002** At validation, the service must accept a target tick from `N - 2` through `N + 1`, inclusive.
- **NIN-TICK-003** Before tick `2`, NIN-TICK-002 includes only non-negative tick values.
- **NIN-TICK-004** The service must reject a target tick earlier than `N - 2` as stale.
- **NIN-TICK-005** The service must reject a target tick later than `N + 1` as future input.
- **NIN-TICK-006** For target tick `N - 2`, `N - 1`, or `N`, the effective tick must be `N`.
- **NIN-TICK-007** For target tick `N + 1`, the effective tick must be `N + 1`.
- **NIN-TICK-008** A late input must not change a processed tick or rewind canonical state.
- **NIN-TICK-009** The service must apply no more than one input state for one player in one authoritative tick.
- **NIN-TICK-010** Before an effective tick, a valid higher sequence for the same player and tick must replace a lower pending sequence.
- **NIN-TICK-011** A replaced pending command must not affect canonical state and must not receive an applied acknowledgment.
- **NIN-TICK-012** The service must apply the remaining pending input when it processes the effective tick.
- **NIN-TICK-013** The applied acknowledgment must identify the input command and its effective authoritative tick.
- **NIN-TICK-014** The service must not send an applied acknowledgment before it processes the effective tick.

## Phase, player state, and lifecycle

- **NIN-LIFE-001** The service may apply player input only during an active round or the first second of the round-end delay.
- **NIN-LIFE-002** The service must reject player input during the frozen round-end delay, lobby, final summary, ended state, or failed state.
- **NIN-LIFE-003** The service must reject input for a dead or removed player.
- **NIN-LIFE-004** The service must set each player's retained input to no pressed actions at each round start.
- **NIN-LIFE-005** When a participant disconnects, the service must immediately clear input for that participant's reserved players.
- **NIN-LIFE-006** During a disconnect reservation, the service must not accept input for a reserved player.
- **NIN-LIFE-007** Reconnect must not restore a held input state from before disconnect.
- **NIN-LIFE-008** After reconnect restoration, the participant may send new input only for its restored owned players.
- **NIN-LIFE-009** Intentional leave, reservation expiry, or player removal must permanently revoke input authority for the removed player identity.
- **NIN-LIFE-010** A later connection must not restore authority for a removed player identity.

## Validation, limits, and outcomes

- **NIN-VAL-001** The service must validate command structure, identity, ownership, sequence, target tick, phase, player state, action state, and limits.
- **NIN-VAL-002** The service must complete validation before it changes pending input, retained input, or canonical state.
- **NIN-VAL-003** Unauthorized input must use `session-policy-violation` with exact copy `Connection ended.`.
- **NIN-VAL-004** After NIN-VAL-003, the service must close only the offending connection.
- **NIN-VAL-005** Stale, duplicate, future, invalid, unavailable, superseded, and over-limit input must have distinct authoritative outcome categories.
- **NIN-VAL-006** An input outcome must not include a credential, endpoint, raw payload, or other untrusted peer value.
- **NIN-VAL-007** Each owned player may submit at most 120 input commands in one monotonic one-second window.
- **NIN-VAL-008** The service may accept at most 1,800 input commands across all players in one monotonic one-second window.
- **NIN-VAL-009** An input above an applicable rate limit must not change pending input or canonical state.
- **NIN-VAL-010** Two consecutive over-limit windows from one remote participant must close only that participant's connection.
- **NIN-VAL-011** An empty or skipped window must reset the consecutive over-limit count.
- **NIN-VAL-012** A backward change in the monotonic window clock must reset the consecutive over-limit count.
- **NIN-VAL-013** Input processing must keep the existing payload, queue, bandwidth, progress, and shutdown limits.

## Authority and state replication

- **NIN-STATE-001** Player input must request normal player actions only. It must not request damage, score, pickup, death, winner, or progression outcomes.
- **NIN-STATE-002** Only the authoritative service may derive gameplay outcomes from applied player input.
- **NIN-STATE-003** A client must present the resulting canonical player and world state through the approved replication behavior.
- **NIN-STATE-004** A client must not change replicated state directly from its local input.
- **NIN-STATE-005** Prediction or reconciliation must not be part of this specification.

## Local Play preservation

- **NIN-LOCAL-001** Local Play must not create, send, receive, or require network input commands.
- **NIN-LOCAL-002** This target must not change Local Play controls, device assignment, simulation, presentation, or persistence.

## Non-goals

- Device-remapping user experience beyond the established local controls.
- State-replication content or lifecycle design.
- Snapshot pacing, interpolation, prediction, reconciliation, or lag compensation.
- Graphical layout, visual design, or new user-visible copy.
- Disconnect detection, reconnect authorization, reservation expiry, or host migration.
- Join-in-progress, spectators, dedicated hosting, or public Internet support.
- Persistent network statistics or Elo.
- A playable-network or release-readiness claim.

## Acceptance criteria

- **NIN-AC-001 — Ownership:** Each network input client submits commands only for its participant's owned players. No command controls an unowned player.
- **NIN-AC-002 — Existing actions:** Input commands represent all seven documented actions without changing an action meaning.
- **NIN-AC-003 — Mixed participation:** Host and guest network input clients support multiple owned players through a complete authoritative round without ownership crossover.
- **NIN-AC-004 — Complete state:** A zero input releases held actions. The last applied complete state remains effective until replacement or an input-clear rule.
- **NIN-AC-005 — Sequence:** Positive increasing per-player sequences are accepted. Duplicate, lower, invalid, or wrapped sequences do not change input or canonical state.
- **NIN-AC-006 — Tick window:** Relative to `N`, target ticks `N - 2` through `N + 1` are eligible. Earlier and later target ticks are rejected.
- **NIN-AC-007 — Effective tick:** Eligible late or current input applies no earlier than `N`. Eligible `N + 1` input applies at `N + 1`.
- **NIN-AC-008 — One state per tick:** Only the highest valid pending sequence for one player and effective tick can affect that tick.
- **NIN-AC-009 — Acknowledgment:** Applied input is acknowledged only after its effective tick. The acknowledgment identifies the command and effective tick.
- **NIN-AC-010 — Phase and player state:** Input applies only in an updating round phase for a living, owned, present player.
- **NIN-AC-011 — Disconnect and revocation:** Disconnect clears held input. Reserved or removed players receive no input until approved authority is restored.
- **NIN-AC-012 — Unauthorized input:** Input for another participant's player changes no state and closes only the offending connection with the fixed policy outcome.
- **NIN-AC-013 — Rejected input:** Each stale, duplicate, future, invalid, unavailable, superseded, or over-limit input changes no pending or canonical state.
- **NIN-AC-014 — Rate limits:** Per-player and host-wide input limits apply. Two consecutive remote-participant over-limit windows close only the offender.
- **NIN-AC-015 — Canonical authority:** Input can cause gameplay outcomes only through normal authoritative simulation and replicated canonical state.
- **NIN-AC-016 — Local independence:** Local Play remains unchanged and does not require network input processing.
- **NIN-AC-017 — Scope truth:** Completion of issue #33 alone must not support a playable-network or release-readiness claim.

## Graphical network-session composition acceptance

- **NIN-COMP-AC-001 — Local controls:** Each supported keyboard or controller action must control only an owned player through the graphical network session.
- **NIN-COMP-AC-002 — Control parity:** The graphical network session must preserve `INP-001` through `INP-016` for host and guest participants.
- **NIN-COMP-AC-003 — Mixed journey:** A supported mixed local and remote roster must complete an authoritative round through graphical host and guest sessions without ownership crossover.
- **NIN-COMP-AC-004 — Headless boundary:** Graphical network-session composition must not require the authoritative service to initialize a renderer, audio, or local input devices.

## Downstream boundaries

- Issue #35 owns responsiveness, interpolation, prediction, reconciliation, and network-condition budgets.
- Issue #36 owns disconnect detection, reservation lifecycle, reconnect authorization, and restored connection authority.
- Issue #33 owns `NIN-BOUND-001` and `NIN-BOUND-002`. It does not own the graphical connection of local devices.
- Issue #38 owns `NIN-OWN-006`, `NIN-BOUND-003`, `NIN-COMP-AC-001` through `NIN-COMP-AC-004`, graphical presentation, and visual evidence.
- Issue #41 owns complete network-play release validation.

## Possible evidence

- Tester results can cover each input-command action for host and guest network input clients.
- Tester results can cover network input clients that own one player or multiple players in mixed sessions.
- Tester results can cover eligible boundary ticks, stale input, future input, and pending-input replacement.
- Tester results can cover zero-state release, round-start clear, disconnect clear, reconnect, and permanent revocation.
- Tester results can cover duplicate, lower, invalid, unavailable, unauthorized, and over-limit input.
- Tester results can verify applied acknowledgments against authoritative ticks and resulting replicated state.
- Reviewer analysis can assess ownership isolation, sequence ordering, authority, rate limits, and Local Play independence.
- DevOps evidence can cover supported Linux and Windows network input client and authoritative service paths when hosted evidence is applicable.
- Issue #38 evidence must cover actual keyboard and controller sampling through graphical host and guest sessions.

Issue #41 remains the complete network-play release gate.
