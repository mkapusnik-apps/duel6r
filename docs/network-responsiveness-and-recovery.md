# Network responsiveness and recovery

## Status and authority

This document is the authoritative product target for GitHub issue [#35](https://github.com/mkapusnik-apps/duel6r/issues/35), under [#27](https://github.com/mkapusnik-apps/duel6r/issues/27).

It defines target behavior. It does not claim implemented or playable network support.

The approved network scope is in [`network-play-first-release.md`](network-play-first-release.md). Canonical state replication is in [`network-state-replication.md`](network-state-replication.md).

Authoritative player input is in [`network-authoritative-player-input.md`](network-authoritative-player-input.md). Local gameplay behavior is in [`features.md`](features.md).

## Terms

- **Round-trip latency:** The elapsed time from one participant request until that participant receives the related service response.
- **Jitter:** The absolute difference between consecutive round-trip latency measurements.
- **Packet loss:** The percentage of network packets that the network path discards.
- **Current state age:** The elapsed time since the authoritative service produced the latest complete state that the client presents as current.
- **Local response time:** The elapsed time from local action sampling until the client first presents the applicable local-player response.
- **Correction time:** The elapsed time from acceptance of canonical state until the client finishes a visible correction to that state.
- **Recovery time:** The elapsed time from restored supported conditions until the client again presents current canonical state without a degraded indication.
- **Degraded indication:** The exact visible text `Network connection degraded.`

## Supported condition budgets

The budgets apply while the participant remains connected. They do not extend the fixed connection or reconnect deadlines.

| Environment | Round-trip latency | Jitter | Packet loss |
|---|---:|---:|---:|
| Separate instances on one machine | 20 ms or less | 5 ms or less | 0 percent |
| Direct private LAN | 100 ms or less | 30 ms or less | 1 percent or less |

- **NRP-BUD-001** Each supported network environment must meet all applicable values in the table.
- **NRP-BUD-002** The budgets must apply during a complete session with 2 through 15 roster players.
- **NRP-BUD-003** The service must provide at least 20 complete canonical state updates each second during an active round.
- **NRP-BUD-004** The service must provide a complete canonical state after each lobby, round, summary, and result transition.
- **NRP-BUD-005** During supported conditions, current state age must not exceed 150 ms for 95 percent of active-round observations.
- **NRP-BUD-006** During supported conditions, current state age must not exceed 250 ms for any continuous period longer than one second.
- **NRP-BUD-007** The client must not enter reconnect only because conditions remain inside all applicable budgets.

## Local and remote presentation

- **NRP-PRS-001** On the same machine, local response time must be 100 ms or less for 95 percent of sampled player actions.
- **NRP-PRS-002** On a private LAN, local response time must be 150 ms or less for 95 percent of sampled player actions.
- **NRP-PRS-003** The client must present remote movement as continuous motion between accepted canonical states during supported conditions.
- **NRP-PRS-004** A remote player must not visibly move backward and forward between two accepted canonical positions.
- **NRP-PRS-005** The client may predict movement and pose for a locally controlled living player.
- **NRP-PRS-006** Local prediction must not create or confirm a shot, hit, pickup, bonus, damage, death, score, winner, or progression outcome.
- **NRP-PRS-007** The client must present each authoritative outcome from NRP-PRS-006 without a predicted replacement.
- **NRP-PRS-008** The client must correct predicted movement to the latest accepted canonical state within 150 ms.
- **NRP-PRS-009** One correction must move toward one authoritative state. It must not alternate between an older state and a newer state.
- **NRP-PRS-010** A correction must not change another player's authoritative state or any authoritative outcome.
- **NRP-PRS-011** First release must not compensate for latency when the service evaluates a hit.

## Degraded state and recovery

- **NRP-REC-001** The client must show the degraded indication when current state age exceeds 250 ms for one continuous second.
- **NRP-REC-002** The client must also show the indication when measured network conditions exceed an applicable budget for three continuous seconds.
- **NRP-REC-003** The degraded indication must not state that the participant disconnected or that the host ended the session.
- **NRP-REC-004** While canonical updates continue, the client must continue to present the latest complete accepted state.
- **NRP-REC-005** The client must not present a partial state as recovery progress.
- **NRP-REC-006** The client must remove the indication after all applicable budgets remain satisfied for three continuous seconds.
- **NRP-REC-007** Recovery time must not exceed five seconds after network conditions return inside all applicable budgets.
- **NRP-REC-008** If incremental recovery is not safe, the client must use the full resynchronization behavior in REP-051 through REP-059.
- **NRP-REC-009** A recovery must not rewind canonical match time, duplicate an event, or restore a removed entity.
- **NRP-REC-010** If transport closes, the guest must use the existing `NET-07` reconnect journey.
- **NRP-REC-011** Conditions outside the supported budgets must not create a claim of supported quality.
- **NRP-REC-012** The application may recover from conditions outside the budgets when it can preserve canonical authority and state integrity.

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
- A playable-network or release-readiness claim.

## Acceptance criteria

- **NRP-AC-001 — Supported conditions:** Same-machine and private-LAN sessions meet their latency, jitter, loss, state-age, and update-rate budgets.
- **NRP-AC-002 — Scale:** NRP-AC-001 remains true for supported sessions with 2 through 15 roster players.
- **NRP-AC-003 — Local response:** Local player actions meet the applicable same-machine or private-LAN response budget.
- **NRP-AC-004 — Remote motion:** Remote movement remains continuous and does not alternate between older and newer accepted states during supported conditions.
- **NRP-AC-005 — Prediction boundary:** Prediction affects only local movement and pose. Canonical outcomes remain authoritative.
- **NRP-AC-006 — Correction:** Predicted movement converges to accepted canonical state within the correction budget and does not change another authoritative value.
- **NRP-AC-007 — Lag compensation:** Hit evaluation uses authoritative state without latency compensation.
- **NRP-AC-008 — Degraded indication:** A sustained budget breach shows the fixed degraded indication without a disconnect or host-end claim.
- **NRP-AC-009 — Recovery:** Restored supported conditions clear the indication and restore current canonical presentation within the recovery budget.
- **NRP-AC-010 — Safe resynchronization:** Recovery never presents partial state, rewinds match time, duplicates events, or restores removed entities.
- **NRP-AC-011 — Connection loss:** A closed guest connection uses `NET-07` and the existing fixed reconnect deadline.
- **NRP-AC-012 — Authority:** Smoothing, prediction, correction, degradation, and recovery never override authoritative outcomes or pause another participant's simulation.
- **NRP-AC-013 — Local independence:** Local Play remains unchanged and does not require network responsiveness behavior.
- **NRP-AC-014 — Scope truth:** Completion of issue #35 alone must not support a playable-network or release-readiness claim.

## Downstream boundaries

- Issue #36 owns disconnect detection, reconnect authorization, reservation lifecycle, and host-loss outcomes.
- Issue #38 owns the visual treatment and accessibility of the degraded indication in the existing `NET-05` state.
- Issue #41 owns complete release-candidate validation.

## Possible evidence

- Tester results can measure supported conditions, local response, current state age, correction time, and recovery time.
- Tester results can cover 2-player and 15-player sessions on the same machine and a private LAN.
- Tester results can cover sustained budget breaches, restored conditions, resynchronization, and connection loss.
- Developer recordings can show remote motion, local correction, the degraded indication, and recovery.
- UX can assess supplied recordings for understandable motion, correction, status, and accessibility.
- Reviewer analysis can assess authority boundaries and the absence of outcome prediction.

Issue #41 remains the complete network-play release gate.
