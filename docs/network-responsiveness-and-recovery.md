# Network responsiveness and recovery

## Status and authority

This document is the authoritative product target for GitHub issue [#35](https://github.com/mkapusnik-apps/duel6r/issues/35), under [#27](https://github.com/mkapusnik-apps/duel6r/issues/27).

It defines target behavior. It does not claim implemented or playable network support.

The approved network scope is in [`network-play-first-release.md`](network-play-first-release.md). Canonical state replication is in [`network-state-replication.md`](network-state-replication.md).

Authoritative player input is in [`network-authoritative-player-input.md`](network-authoritative-player-input.md). Local gameplay behavior is in [`features.md`](features.md).

## Terms

- **Round-trip latency:** The elapsed time from one participant request until that participant receives the related service response.
- **Jitter:** The absolute difference between consecutive round-trip latency measurements.
- **Packet loss:** The percentage of quality probes that do not receive a valid response before their response deadline.
- **Current state age:** The elapsed time since the authoritative service produced the latest complete state that the client identifies as current.
- **Presentation handoff:** Production guest output that supplies presentation-ready state and status to a downstream graphical consumer.
- **Local response time:** The elapsed time from local action sampling until the presentation handoff first supplies the applicable local-player response.
- **Correction time:** The elapsed time from canonical-state acceptance until the presentation handoff finishes a correction to that state.
- **Recovery time:** The elapsed time from restored supported conditions until the presentation handoff supplies current canonical state without degraded status.
- **Degraded status:** The status that carries the exact user-visible text `Network connection degraded.`
- **Host-clock calibration:** A bounded mapping from the client clock to the authoritative host session clock, established by a matched request and response.
- **Calibration uncertainty:** The bounded possible difference between the calibrated host time and the actual host session time.

## Supported condition budgets

The budgets apply while the participant remains connected. They do not extend the fixed connection or reconnect deadlines.

| Environment | Round-trip latency | Jitter | Packet loss |
|---|---:|---:|---:|
| Separate instances on one machine | 20 ms or less | 5 ms or less | 0 percent |
| Direct private LAN | 100 ms or less | 30 ms or less | 1 percent or less |

- **NRP-BUD-001** Each supported network environment must meet all applicable values in the table.
- **NRP-BUD-002** The budgets must apply during a complete session with 2 through 15 roster players.
- **NRP-BUD-003** At the fixed 60 Hz authoritative tick rate, the service must provide at least 20 complete canonical state updates each second during an active round.
- **NRP-BUD-004** The service must provide a complete canonical state after each lobby, round, summary, and result transition.
- **NRP-BUD-005** During supported conditions, current state age must not exceed 150 ms for 95 percent of active-round observations.
- **NRP-BUD-006** During supported conditions, current state age must not exceed 250 ms for any continuous period longer than one second.
- **NRP-BUD-007** The client must not enter reconnect only because conditions remain inside all applicable budgets.
- **NRP-BUD-008** Each complete canonical state update must identify its production time on the authoritative host session clock. A client must have valid host-clock calibration before it applies the update.
- **NRP-BUD-009** The client must compare production time with calibrated host time at frame receipt. Production time may equal calibrated host time plus calibration uncertainty. A later value must not change canonical or presentation state.
- **NRP-BUD-010** The client must not use half the measured round-trip latency as the authoritative production time.
- **NRP-BUD-011** Production quality measurement must include probes that receive no response after their applicable response deadline.
- **NRP-BUD-012** Successful probe responses alone must not establish zero packet loss.
- **NRP-BUD-013** Supported network matches must use the fixed 60 Hz authoritative tick rate.
- **NRP-BUD-014** The responsiveness budgets do not apply to a configurable alternative tick rate.
- **NRP-BUD-015** Production quality measurement must give each probe a bounded response deadline.

## Local and remote presentation

- **NRP-PRS-001** On the same machine, local response time must be 100 ms or less for 95 percent of sampled player actions.
- **NRP-PRS-002** On a private LAN, local response time must be 150 ms or less for 95 percent of sampled player actions.
- **NRP-PRS-003** The presentation handoff must supply continuous remote motion between accepted canonical states during supported conditions.
- **NRP-PRS-004** The presentation handoff must not move a remote player backward and forward between two accepted canonical positions.
- **NRP-PRS-005** The client may predict movement and pose for a locally controlled living player.
- **NRP-PRS-006** Local prediction must not create or confirm a shot, hit, pickup, bonus, damage, death, score, winner, or progression outcome.
- **NRP-PRS-007** The presentation handoff must supply each authoritative outcome from NRP-PRS-006 without a predicted replacement.
- **NRP-PRS-008** The presentation handoff must correct predicted movement to the latest accepted canonical state within 150 ms.
- **NRP-PRS-009** One correction must move toward one authoritative state. It must not alternate between an older state and a newer state.
- **NRP-PRS-010** A correction must not change another player's authoritative state or any authoritative outcome.
- **NRP-PRS-011** First release must not compensate for latency when the service evaluates a hit.
- **NRP-PRS-012** The production guest replication path must publish presentation-ready state and status through the presentation handoff.
- **NRP-PRS-013** Issue #35 does not require a graphical consumer for the presentation handoff.

## Degraded state and recovery

- **NRP-REC-001** The presentation handoff must publish degraded status when current state age exceeds 250 ms for one continuous second.
- **NRP-REC-002** The presentation handoff must also publish degraded status when measured network conditions exceed an applicable budget for three continuous seconds.
- **NRP-REC-003** Degraded status must not state that the participant disconnected or that the host ended the session.
- **NRP-REC-004** While canonical updates continue, the presentation handoff must continue to supply the latest complete accepted state.
- **NRP-REC-005** The presentation handoff must not identify a partial state as recovery progress.
- **NRP-REC-006** The presentation handoff must remove degraded status after all applicable budgets remain satisfied for three continuous seconds.
- **NRP-REC-007** Recovery time must not exceed five seconds after network conditions return inside all applicable budgets.
- **NRP-REC-008** If incremental recovery is not safe, the client must use the full resynchronization behavior in REP-051 through REP-059.
- **NRP-REC-009** A recovery must not rewind canonical match time, duplicate an event, or restore a removed entity.
- **NRP-REC-010** If transport closes, the presentation handoff must publish reconnecting status for the downstream `NET-07` journey.
- **NRP-REC-011** Conditions outside the supported budgets must not create a claim of supported quality.
- **NRP-REC-012** The application may recover from conditions outside the budgets when it can preserve canonical authority and state integrity.
- **NRP-REC-013** During resynchronization, the presentation handoff must retain the last complete accepted state as non-current context.
- **NRP-REC-014** During resynchronization, the presentation handoff must publish synchronizing status with the retained context.
- **NRP-REC-015** After transport closes, the presentation handoff must retain the last complete accepted state as non-current context.
- **NRP-REC-016** After transport closes, the presentation handoff must publish reconnecting status with the retained context.
- **NRP-REC-017** Retained context must not be identified as live or current authoritative state.

## Authority and lifecycle boundaries

- **NRP-AUT-001** The authoritative service must remain the only source of canonical gameplay and result state.
- **NRP-AUT-002** Presentation smoothing, prediction, correction, and recovery must not change canonical state.
- **NRP-AUT-003** A participant must not delay another participant's authoritative simulation during degradation or recovery.
- **NRP-AUT-004** The fixed host startup, initial connection, reconnect, progress, and shutdown deadlines remain unchanged.
- **NRP-AUT-005** Local Play must not use or require network responsiveness, prediction, correction, or recovery behavior.
- **NRP-AUT-006** This target must not change Local Play controls, simulation, presentation, persistence, or error behavior.

## Non-goals

- Public Internet, NAT traversal, relays, discovery, or matchmaking.
- Host migration, join-in-progress, spectators, or dedicated hosting.
- Lag compensation for hit evaluation.
- Prediction of authoritative gameplay outcomes.
- A new graphical screen or a visual-layout decision.
- Graphical consumption or rendering of presentation-ready state and status.
- Invocation, authorization, retry, or completion of the `NET-07` reconnect journey.
- Product support for an authoritative tick rate other than 60 Hz.
- A playable-network or release-readiness claim.

## Acceptance criteria

- **NRP-AC-001 — Supported conditions:** At the fixed 60 Hz authoritative tick rate, same-machine and private-LAN sessions meet their latency, jitter, loss, state-age, and update-rate budgets.
- **NRP-AC-002 — Scale:** NRP-AC-001 remains true for supported sessions with 2 through 15 roster players.
- **NRP-AC-003 — Local response:** The presentation handoff supplies local-player responses within the applicable same-machine or private-LAN response budget.
- **NRP-AC-004 — Remote motion:** The presentation handoff supplies continuous remote movement without alternation between older and newer accepted states during supported conditions.
- **NRP-AC-005 — Prediction boundary:** Prediction affects only local movement and pose. Canonical outcomes remain authoritative.
- **NRP-AC-006 — Correction:** The presentation handoff converges predicted movement to accepted canonical state within the correction budget. It does not change another authoritative value.
- **NRP-AC-007 — Lag compensation:** Hit evaluation uses authoritative state without latency compensation.
- **NRP-AC-008 — Degraded status:** A sustained budget breach publishes the fixed degraded status without a disconnect or host-end claim.
- **NRP-AC-009 — Recovery:** Restored supported conditions clear degraded status and restore current canonical presentation-ready state within the recovery budget.
- **NRP-AC-010 — Safe resynchronization:** The presentation handoff never identifies partial state as current. Recovery never rewinds match time, duplicates events, or restores removed entities.
- **NRP-AC-011 — Connection loss handoff:** A closed guest connection publishes reconnecting status and retained non-current context for the downstream `NET-07` journey.
- **NRP-AC-012 — Authority:** Smoothing, prediction, correction, degradation, and recovery never override authoritative outcomes or pause another participant's simulation.
- **NRP-AC-013 — Local independence:** Local Play remains unchanged and does not require network responsiveness behavior.
- **NRP-AC-014 — Scope truth:** Completion of issue #35 alone must not support a playable-network or release-readiness claim.
- **NRP-AC-015 — Measurement validity:** State-age measurement uses authoritative production time under valid host-clock calibration. An update beyond the calibrated future bound changes no canonical or presentation state. Loss measurement counts applicable unanswered probes.
- **NRP-AC-016 — Presentation handoff:** Production replication publishes presentation-ready state, synchronizing status, degraded status, reconnecting status, and applicable retained non-current context.

## Downstream boundaries

- Issue #36 owns disconnect detection, reconnect authorization, reservation lifecycle, and host-loss outcomes.
- Issue #36 owns invocation and completion of the `NET-07` reconnect journey.
- Issue #38 owns graphical consumption of presentation-ready state and status.
- Issue #38 owns the visual treatment and accessibility of degraded status in the existing `NET-05` state.
- Issue #41 owns complete release-candidate validation.

## Possible evidence

- Tester results can measure supported conditions, presentation-handoff response, current state age, correction time, and recovery time.
- Tester results can cover 2-player and 15-player sessions on the same machine and a private LAN.
- Tester results can cover unanswered quality probes, sustained budget breaches, restored conditions, resynchronization, and connection loss.
- Tester results can verify synchronizing and reconnecting status with retained non-current context.
- Reviewer analysis can assess authoritative production time, authority boundaries, downstream handoffs, and the absence of outcome prediction.
- Issue #38 evidence can show and assess graphical motion, correction, status, recovery, and accessibility.

Issue #41 remains the complete network-play release gate.
