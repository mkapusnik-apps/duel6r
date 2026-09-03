# Canonical network state replication

## Status and authority

This document is the authoritative product target for GitHub issue [#34](https://github.com/mkapusnik-apps/duel6r/issues/34), under [#27](https://github.com/mkapusnik-apps/duel6r/issues/27).

It defines target behavior. It does not claim implemented or playable network support.

The approved network scope and journeys are in [`network-play-first-release.md`](network-play-first-release.md). The authoritative match behavior is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

Compatibility and admitted identities are in [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md). Transport and resource limits are in [`networking.md`](networking.md) and [`network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md).

Local gameplay behavior is in [`features.md`](features.md). This document must not change Local Play.

## Terms

- **Canonical state:** The current match and world state that the authoritative service owns.
- **Replicated state:** A read-only client copy of canonical state.
- **State version:** A session-scoped value that identifies one ordered canonical state.
- **Baseline:** A complete accepted state version that an incremental update uses as its starting state.
- **Full snapshot:** One complete canonical state at one state version.
- **Incremental update:** The ordered changes from one stated baseline to one later state version.
- **Resynchronization:** Replacement of replicated state with a new full snapshot after the client cannot safely apply incremental updates.
- **Presentation event:** An authoritative occurrence that a client can present once, such as a hit, death, pickup, or round outcome.

## Product goal

- **REP-001** Each connected participant must receive sufficient canonical state to follow the approved lobby, match, round, summary, and result journeys.
- **REP-002** The authoritative service must remain the only owner of canonical gameplay and result state.
- **REP-003** A client must use replicated state as read-only input for presentation.
- **REP-004** Replication must not add client authority over scoring, hits, pickups, deaths, winners, random decisions, or round progression.

## Stable identity rules

- **REP-005** Each session, match, round, participant, player, replicated world entity, and presentation event must have a stable session-scoped identity.
- **REP-006** An identity must be nonzero and unique in its applicable identity category during the session.
- **REP-007** The authoritative service must not reuse a removed identity during the same session.
- **REP-008** Participant and player identities must use the admitted identities from [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md).
- **REP-009** A player identity must remain unchanged across rounds until the authoritative service removes that player from the session.
- **REP-010** Each round must have a new identity. Round-bound entities must not continue into the next round.
- **REP-011** A removed entity that later returns as a new entity must receive a new identity.
- **REP-012** A presentation event identity must identify one occurrence. A client must not present that occurrence more than once.

## Replicated state coverage

### Session, lobby, and match state

- **REP-013** Replicated session state must identify the host, admitted participants, connection state, readiness, player ownership, and authoritative roster order.
- **REP-014** Replicated match state must include the selected mode, applicable team settings, level plan, round limit, Assistance, Quick Liquid, and Burnable Trees.
- **REP-015** Replicated match state must identify the current phase as lobby, active round, round summary, final summary, or ended.
- **REP-016** Replicated progression must include the current round number, completed-round count, phase timing, and applicable round-end countdown.
- **REP-017** Replicated result state must keep result state, match outcome, last completed-round outcome, and cumulative rankings as distinct values. It must include all session-only result data required by [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

### Round and world state

- **REP-018** Replicated round state must include the round identity, level logical identity, orientation, authoritative roster order, and that round's outcome. A round outcome must not become a cumulative-ranking champion.
- **REP-019** The replicated world must include each current player, shot, projectile, weapon pickup, bonus pickup, elevator, hazard, water state, applicable tree state, fire, and explosion.
- **REP-020** A replicated player must include identity, owner, roster position, display name, team, life state, position, movement state, facing, life, air, held weapon, ammunition, and applicable action state.
- **REP-021** A replicated player must include active bonus state, applicable remaining durations, invulnerability, visibility, reload, charge, and temporary movement effects.
- **REP-022** Each replicated shot or projectile must include its identity, owner, weapon type, position, movement, and current lifecycle state.
- **REP-023** Each replicated pickup must include its identity, category, applicable type, position, availability, and authoritative contents.
- **REP-024** Each replicated elevator, hazard, water state, tree, fire, and explosion must include the current state needed to present its authoritative gameplay effect.
- **REP-025** Replicated score state must include current-round values, cumulative values, ranking order, and applicable team totals. It must replicate match outcome separately. A cumulative ranking leader must not become a match champion.
- **REP-026** Replicated state must include current player indicators, live ranking state, round progress, score-summary state, status messages, and event messages.

### Presentation events

- **REP-027** The authoritative service must identify events that represent round start, round outcome, shot, hit, damage, death, kill, assist, weapon change, pickup, bonus, water entry, hazard, explosion, fire, and result transitions.
- **REP-028** A client may select presentation-only sound, animation, and background details. Those choices must not change canonical state.
- **REP-029** A full snapshot must include each current continuing effect and its current remaining state.
- **REP-030** A full snapshot must not cause a client to replay an expired presentation event.

## Entity lifecycle

- **REP-031** A full snapshot must declare the complete set of current replicated entities at its state version.
- **REP-032** An incremental update must identify each entity creation, update, and removal that applies after its baseline.
- **REP-033** A client must not apply an update or removal for an identity that is absent from the applicable baseline.
- **REP-034** A removal must be terminal for that identity.
- **REP-035** A client must not apply a later creation or update for a removed identity.
- **REP-036** After a client accepts a full snapshot, an entity absent from that snapshot must not remain in replicated state.
- **REP-037** A round transition must remove prior round-bound entities before the client presents the new round as current.

## Full snapshot behavior

- **REP-038** The authoritative service must provide a full snapshot after initial admission and before the participant enters the lobby as connected.
- **REP-039** The authoritative service must provide a full snapshot after a successful reconnect and before restoration completes.
- **REP-040** The authoritative service must provide a full snapshot when a connected client requires resynchronization.
- **REP-041** A full snapshot must contain all applicable state in REP-013 through REP-030 at one state version. Its result values must follow the completed or interrupted semantics in REP-017.
- **REP-042** A client must validate a complete full snapshot before it replaces the prior replicated state.
- **REP-043** A client must not present a partial snapshot as current authoritative state.
- **REP-044** A successfully restored client must present the current authoritative state. It must not rewind the match or make an older snapshot current.

## Incremental state behavior

- **REP-045** Each incremental update must identify exactly one accepted baseline and one later state version.
- **REP-046** A client must apply incremental updates only in authoritative state-version order.
- **REP-047** A client must not apply an incremental update when its baseline does not equal the client's current accepted state version.
- **REP-048** A duplicate, stale, out-of-order, incomplete, malformed, or internally inconsistent incremental update must not change replicated state.
- **REP-049** The client must apply one valid incremental update as one complete state change. It must not present a mixture of its old and new versions.
- **REP-050** Incremental state must produce the same current replicated state as a full snapshot of the same authoritative state version. This equality includes each distinct result value in REP-017.

## Baseline loss and resynchronization

- **REP-051** When REP-047 or REP-048 occurs, the client must require full resynchronization.
- **REP-052** During resynchronization, the client must not apply later incremental updates to the invalid baseline.
- **REP-053** During resynchronization, the client may retain the last complete accepted state as non-current context. It must not describe that state as live.
- **REP-054** Only the latest complete valid full snapshot may finish one resynchronization attempt.
- **REP-055** A client must have no more than one active resynchronization attempt for one connection.
- **REP-056** Resynchronization must use the existing transport payload, queue, bandwidth, progress, and shutdown limits.
- **REP-057** If resynchronization cannot complete before the transport closes, the guest must enter `NET-07` under the existing reconnect rules.
- **REP-058** A resynchronization failure must not pause the authoritative match or change another participant's state.
- **REP-059** A successful resynchronization must converge to the current authoritative state without replaying removed entities or expired events.

## Transport conditions and convergence

- **REP-060** While connected, replication may rely on the production transport's reliable, ordered delivery of complete application frames.
- **REP-061** Replication must not claim support for application-frame loss, duplication, or reordering that the production transport does not deliver.
- **REP-062** A connection loss may lose undelivered incremental state. Reconnect restoration must use REP-039 and REP-044.
- **REP-063** Delay or backpressure must not cause the client to apply state out of order or present partial state.
- **REP-064** Issue #35 owns snapshot pacing, latency, jitter, interpolation, prediction, reconciliation, and responsiveness targets.

## Validation, authority, and failure behavior

- **REP-065** A client must reject replicated data that exceeds applicable transport or trust limits.
- **REP-066** Invalid replicated data from a host must not change the client's last complete accepted state.
- **REP-067** After invalid host replication data ends the connection, the guest must follow `NET-07` without claiming that the host ended the session.
- **REP-068** A client message that attempts to change canonical replicated state must have no gameplay effect.
- **REP-069** An unauthorized state-change attempt must use the existing `session-policy-violation` outcome and must close only the offending connection.
- **REP-070** Replicated state and diagnostics must not disclose credentials, raw filesystem paths, peer payloads, or excluded local persistence data.

## Local Play preservation

- **REP-071** Local Play must not create, consume, or require replicated network state.
- **REP-072** Replication must not change Local Play controls, simulation, presentation, persistence, or error behavior.

## Non-goals

- Remote-input submission, validation, sequencing, or acknowledgment.
- Snapshot pacing or a responsiveness target.
- Interpolation, prediction, reconciliation, or lag compensation.
- Disconnect detection, reconnect credential exchange, reservation expiry, or host migration.
- Graphical screen layout or visual design.
- Join-in-progress or spectator state.
- Persistent network statistics or Elo.
- A playable-network or release-readiness claim.

## Acceptance criteria

- **REP-AC-001 — Stable identities:** Every replicated match, round, player, world entity, and event follows REP-005 through REP-012.
- **REP-AC-002 — Complete state:** A full snapshot contains all applicable session, lobby, match, round, world, score, message, effect, and result state. It keeps the result values in REP-017 distinct.
- **REP-AC-003 — Initial construction:** After admission, a client constructs one complete lobby state before it is shown as connected.
- **REP-AC-004 — Reconnect restoration:** A reconnected client receives the current complete lobby, match, round-summary, or final-summary state without rewind.
- **REP-AC-005 — Lifecycle:** Entity creation, update, removal, round transition, and full-snapshot replacement follow REP-031 through REP-037.
- **REP-AC-006 — Ordered updates:** Valid incremental updates apply atomically in baseline order. Each update equals the corresponding full snapshot, including all distinct result values.
- **REP-AC-007 — Invalid updates:** Duplicate, stale, out-of-order, incomplete, malformed, or inconsistent updates do not change replicated state.
- **REP-AC-008 — Recovery:** Baseline loss starts one bounded full resynchronization that converges without stale entities or repeated expired events.
- **REP-AC-009 — Recovery failure:** A failed resynchronization affects only that client and follows the existing reconnect journey without pausing the match.
- **REP-AC-010 — Read-only authority:** A client cannot change canonical score, hit, pickup, death, winner, random, or progression state through replication.
- **REP-AC-011 — Transport truth:** Replication uses the approved reliable ordered transport and makes no unsupported loss, duplication, or reordering claim.
- **REP-AC-012 — Presentation continuity:** Replicated state can present the shared arena, live status, score overlays, round transitions, final summary, and retained lobby result. It must not invent a cumulative-ranking champion.
- **REP-AC-013 — Safety:** Replication respects existing payload, queue, bandwidth, progress, redaction, and connection-isolation limits.
- **REP-AC-014 — Local independence:** Local Play remains unchanged and does not start or require replication.
- **REP-AC-015 — Scope truth:** Completion of issue #34 alone must not support a playable-network or release-readiness claim.

## Downstream boundaries

- Issue #33 may consume replicated player identities. It owns remote-input behavior and must not let input mutate replicated client state directly.
- Issue #35 owns responsiveness and presentation smoothing. It must not replace authoritative outcomes.
- Issue #36 owns disconnect, reconnect authorization, and lifecycle removal. It must restore the current state through a full snapshot.
- Issue #38 owns graphical presentation and visual evidence for replicated states.
- Issue #41 owns complete release-candidate validation.

## Possible evidence

- Tester results can compare a full snapshot with incremental convergence at the same state version.
- Tester results can cover initial lobby state and reconnect restoration during each supported phase.
- Tester results can cover duplicate, stale, out-of-order, incomplete, malformed, and inconsistent update rejection.
- Tester results can cover removal, round transition, continuing effects, and event deduplication.
- Reviewer analysis can assess complete state coverage, identity stability, read-only authority, bounds, and redaction.
- Developer screenshots or recordings can show remote shared-arena, score, round-summary, final-summary, and reconnect-restored states.
- UX can assess supplied visual artifacts against the existing target screens.

Issue #41 remains the complete network-play release gate.
