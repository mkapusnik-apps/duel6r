# First-release network play target specification

## Status and authority

This document is the authoritative product target for issue [#28](https://github.com/mkapusnik-apps/duel6r/issues/28), a subtask of [#27](https://github.com/mkapusnik-apps/duel6r/issues/27). It defines approved first-release network-play scope and journeys, not implemented behavior. The current code remains an experimental scaffold with no playable network support, as documented in [`docs/networking.md`](networking.md).

The target network screens in [`docs/screens`](screens/README.md) implement this product specification. Existing local behavior remains governed by [`docs/features.md`](features.md), which intentionally makes no network-support claim.

## Terminology and clock

- **Participant:** One admitted game instance. The host is one participant; every other participant is a guest.
- **Connected participant:** An admitted participant with a currently accepted transport connection.
- **Local player:** A player controlled from a participant's machine with that participant's person, profile, and control assignment.
- **Roster player:** One combatant in the authoritative session roster. Every roster player belongs to exactly one participant until removal.
- **Host:** The participant that creates the session, owns the authoritative server process, configures the match, and controls session progression.
- **Guest:** A participant admitted through the host's direct endpoint.
- **Endpoint:** A directly entered hostname or IP address plus port.
- **Lobby:** The pre-match and between-match state showing admission, connection, ownership, roster, readiness, and host settings.
- **Ready:** A participant's confirmation of the current configuration. Clearing mutations invalidate every participant's readiness.
- **Session:** The period from confirmed host startup until host End session, unexpected host loss, or shutdown.
- **Host session clock:** The authoritative monotonic clock used for connection, disconnect, reservation, expiry, and same-instant ordering decisions.
- **Supported release/content:** The exact network release ID and exact canonical gameplay-content manifest required by the host.

All normative deadlines and precedence rules in this document are evaluated on the host session clock. Downstream issues own transport, clock synchronization presentation, and implementation mechanics; they may not change the user-visible boundaries.

## First-release scope matrix

| Dimension | Supported target | Explicitly unsupported |
|---|---|---|
| Platforms | Linux x86-64 and Windows x86-64 | Other operating systems and architectures |
| Cross-platform play | Linux and Windows x86-64 participants in one session | Other targets |
| Network environments | Separate instances on one machine; LAN direct connection | Internet support, NAT traversal, relays, public hosting claims |
| Connection method | Direct hostname or IP address plus port | Discovery, server browser, matchmaking |
| Hosting | Player-hosted authoritative session | Dedicated server deployment and host migration |
| Identity and access | Session-local participant identity | Accounts, passwords, cloud identity, ranked identity |
| Lobby cardinality | 1–15 admitted participants and 1–15 roster players; a host-alone lobby is valid | Empty or over-capacity admitted lobby |
| Match start | 2–15 connected participants and 2–15 roster players; each participant owns at least one player | Starting alone, with a disconnected guest, or with an ownerless participant |
| Degraded match | One connected host may continue while at least two roster players remain | Continuing after fewer than two roster players remain |
| Admission | Lobby admission before match start | Join-in-progress and spectators |
| Compatibility | Exact network release ID and canonical gameplay content | Cross-release or cross-content compatibility |
| Results | Session-only network results retained until replacement or session end | Local statistics or Elo writes |

Lobby invariants are `1 <= admitted participants <= roster players <= 15`. Match start invariants are `2 <= connected participants <= roster players <= 15`, with every participant owning at least one player. A started match may degrade to one connected host if reservations or retained roster ownership leave at least two roster players. Fewer than two roster players ends the match without a winner.

Same-machine support means separate running instances communicating through the production transport. It does not wrap or alter the local-only Play journey.

## Ownership and configuration

- The host starts and owns the player-hosted authoritative session and direct endpoint.
- The host configures mode, level or rotation, rounds, other approved match settings, and authoritative roster order.
- Each participant configures only its own local persons, profiles, and controls and must own at least one roster player while admitted.
- The lobby labels each participant's role, connection state, readiness, owned players, and authoritative roster positions separately.
- Host-owned settings are read-only for guests. Participant-owned player controls are editable only by that participant.
- Every gameplay action and state transition is validated and applied by the authoritative host simulation.

## Lobby, readiness, and admission

- A host-alone lobby with one or more host-owned players is valid, but Start is blocked until the match-start invariants are met.
- Every connected participant, including the host, must be ready before Start is enabled.
- Adding, removing, admitting, or expiring a participant; intentionally leaving; adding, removing, or editing a player; changing a person, profile, control, host match setting, or roster order clears every participant's readiness.
- An admitted guest declared disconnected remains admitted as `Reconnecting`. Its prior Ready value is retained, but Start is blocked with `Waiting for <participant> to reconnect`.
- A successful reconnect restores the retained Ready value unless another readiness-clearing mutation occurred after disconnect.
- Reconnect expiry or intentional Leave removes the participant and players and clears every remaining participant's readiness.
- No participant may be admitted after match start. A late attempt receives the explicit join-in-progress failure.

## Compatibility contract

### Network release ID

- Every host and guest provides a non-empty network release ID.
- Equality is exact and case-sensitive. Whitespace is content and is not trimmed or normalized.
- A missing or different value fails before lobby admission.
- Exact release mismatch copy is `Network release mismatch. Host requires "<host-release-id>"; this client is "<client-release-id>".`

### Canonical gameplay-content manifest

The compatibility manifest includes content that can alter authoritative simulation or match outcomes:

- gameplay levels and their gameplay metadata;
- gameplay configuration and data definitions;
- enabled, trusted gameplay scripts.

It excludes presentation-only and participant-local material:

- textures, sounds, fonts, and other cosmetic presentation;
- local people, profiles, statistics, Elo, saves, and other persistence;
- local control assignments and controller presets;
- documentation.

Each manifest entry uses a non-empty session-relative UTF-8 logical path with `/` separators. Paths are case-sensitive, have no leading `/`, empty segment, `.` segment, `..` segment, or duplicate logical path. Entries are sorted in ascending bytewise UTF-8 logical-path order. Equality compares logical paths and file content; timestamps, permissions, owners, archive order, and other filesystem or package metadata do not affect equality.

The exact content mismatch copy is `Gameplay content mismatch: <logical-path>. Use the host's exact gameplay content.` The path is the first differing logical path in canonical sorted order, including a path present on only one side. Invalid manifests fail with `Gameplay content manifest invalid: <logical-path>.`

The following synthetic fixtures are normative compatibility examples, not shipped content:

| Fixture | Host | Guest | Expected result |
|---|---|---|---|
| Release match | ID `release-fixture-a` | ID `release-fixture-a` | Continue to manifest comparison |
| Case mismatch | ID `release-fixture-a` | ID `Release-Fixture-A` | `Network release mismatch. Host requires "release-fixture-a"; this client is "Release-Fixture-A".` |
| Content match | `config/gameplay.json` containing `fixture-a`; `levels/arena.json` containing `fixture-b` | Same paths and exact contents in any source enumeration order | Compatible |
| Content mismatch | `levels/arena.json` containing `fixture-b` | `levels/arena.json` containing `fixture-c` | `Gameplay content mismatch: levels/arena.json. Use the host's exact gameplay content.` |
| Path mismatch | `levels/arena.json` | `Levels/arena.json` | `Gameplay content mismatch: Levels/arena.json. Use the host's exact gameplay content.` because uppercase `L` sorts first in the fixture's exact path ordering |
| Invalid path | Not applicable | `levels/../arena.json` | `Gameplay content manifest invalid: levels/../arena.json.` |

Issue #30 owns canonical serialization, content digest choice, exchange, comparison mechanics, and protocol enforcement. Those mechanics must produce the exact policy, fixture outcomes, and user-visible copy above.

## Timing, failures, and precedence

### Host startup

- Host startup has a 10-second deadline on the host session clock from Start session activation.
- Success requires confirmed listener/session readiness strictly before the deadline. At or after 10 seconds, startup fails and no listener may remain.
- Cancel before success returns to editable `NET-02`, retains endpoint and local-player setup, and leaves no listener or session.
- A specific confirmed startup failure is shown instead of a generic timeout when known before the deadline.

### Guest connection

- Hostname/address and port validation remains inline in editable `NET-03`; invalid input does not begin the connection clock.
- A connection attempt has one 10-second total deadline covering name resolution, transport connection, compatibility, capacity, host admission, and lobby confirmation.
- Success must be confirmed strictly before the deadline. At or after 10 seconds, the generic result is `Connection timed out` unless a more specific failure was confirmed first.
- Specific resolution, unreachable-host, rejection, capacity, join-in-progress, release, or content failures take precedence over generic timeout when confirmed before the deadline.
- Cancel returns to editable `NET-03` and retains endpoint and local-player setup.
- `NET-08` Retry repeats the same retained attempt when still valid; Edit setup returns to editable `NET-02` or `NET-03` with all setup retained; Return to Network goes to `NET-01`.

### Host connection loss

- A guest declares unexpected host loss when authoritative host contact has been continuously absent through the 5-second boundary on the session timing model.
- Host End session is a confirmed intentional terminal event and takes precedence over unexpected-loss or guest-removal handling at the same clock instant.
- Downstream issues own heartbeat, transport, and clock mechanics but must satisfy the 5-second user-visible boundary.

## User journeys and destinations

### Host and startup

1. `MENU-01` → `Network (F2)` → `NET-01` → Host → editable `NET-02`.
2. Start session shows truthful pending state for at most 10 seconds.
3. Cancel → editable `NET-02` with setup retained and no listener.
4. Confirmed success → `NET-04`; specific failure or timeout → `NET-08`.

### Join

1. `NET-01` → Join → editable `NET-03`.
2. Inline-invalid endpoint remains in `NET-03`; valid Connect starts the 10-second attempt.
3. Cancel → editable `NET-03` with endpoint and local-player setup retained.
4. Confirmed admission → `NET-04`; failure → `NET-08`; Retry repeats, Edit setup returns to `NET-03`, Return to Network → `NET-01`.

### Lobby, match, summary, and return

1. Valid admitted participants configure owned fields in `NET-04`; every clearing mutation clears readiness.
2. Start is enabled only when all match-start cardinality, ownership, connection, compatibility, and readiness requirements hold.
3. Start closes admission and enters authoritative shared-arena `NET-05`.
4. Normal completion enters `NET-06`; host Return to lobby moves connected participants to `NET-04` with readiness cleared.
5. Session-only result rows remain visible in summary and lobby until the next match replaces them or the session ends.

### Explicit cancel, leave, and end actions

- Guest `Leave` in `NET-04` requires confirmation: `Leave session? Your players will be removed and you will return to Network.` Confirm → guest `NET-01`; Cancel → current lobby.
- Guest `Leave session` during `NET-05` requires confirmation: `Leave session? Your players will be removed immediately and the match will continue without reconnect.` Confirm → guest `NET-01`; Cancel → active match.
- Guest `Leave` from `NET-06` uses the lobby consequence copy and destination.
- `NET-07` action is `Leave session`. It requires `Leave session? Your reserved players will be removed now and reconnect will stop.` Confirm → `NET-01`; Cancel → reconnect continues against the unchanged deadline.
- Host `End session` in `NET-04`, `NET-05`, or `NET-06` requires `End session for everyone?` Confirm → host `NET-01`; guests see the host-ended variant of blocking `NET-09`. Cancel → current state.
- Unexpected host loss shows the distinct host-loss variant of `NET-09`. Its Return to Network action → `NET-01`.
- Back from `NET-01` → `MENU-01` without changing local setup or starting a network service.

## Guest disconnect and reconnect

- The host declares a guest disconnected at host clock time `D`; the reservation begins at `D` and has deadline `D + 30 seconds`.
- A reconnect is accepted only when the host accepts restored identity and transport strictly before `D + 30 seconds`. At or after the deadline, the reservation expires.
- Displayed seconds are `ceil(deadline - now)` while positive. The active reconnect state therefore shows `30` through `1`, never active `0`.
- Repeated attempts use the original deadline and never extend or restart it.
- After a successful restore, a later newly declared disconnect creates a new reservation and new 30-second deadline.
- Successful reconnect restores only the current authoritative lobby, match, or summary state; it never rewinds or makes an older snapshot current.
- During an active round, simulation, timers, hazards, connected inputs, combat, scoring, winner rules, and round progression continue. Reserved players receive no input, remain valid targets, and count for winner conditions.
- Normal damage, death, score, and winner rules apply during reservation.

## Atomic removal and winner evaluation

- At each host session clock instant, all guest intentional leaves effective at that instant and all reservations whose deadlines are at or before that instant form one removal batch.
- The host removes all players in that batch atomically, adds no kill, death, assist, penalty, or other combat statistic for removal itself, then performs exactly one winner-condition evaluation after the batch.
- If at least two roster players remain, normal match and round progression continues, including a degraded match with only the connected host.
- If fewer than two roster players remain, the match ends without a winner and remaining connected participants return to `NET-04` with readiness cleared.
- Host End session or declared host loss at the same instant takes precedence: the session ends and no removal-derived winner outcome is produced.

## Session-only result lifecycle

- Authoritative network results are labeled `Session only` and never write local statistics, Elo, people, profiles, or saves.
- Completed-match result rows remain available in `NET-06` and the following `NET-04` lobby.
- A participant or player that leaves after results exist remains in those rows and is labeled `Departed`.
- Starting and completing the next match replaces the prior session result set; results are not accumulated as persistent history.
- Host End session, unexpected host loss, or application session shutdown discards the session result set.
- Interrupted matches do not create a persistent or locally recoverable result.

## Local-only preservation

- `Play (F1)` remains the existing local-only journey and starts no listener, server, client connection, or other network service.
- Local Play starts and completes without network availability.
- `Network (F2)` is distinct and must not replace, wrap, or redirect local Play.
- Network configuration, readiness, results, failures, and participant state do not mutate local persistent data.
- This specification does not add network behavior to [`docs/features.md`](features.md).

## Non-goals and explicit boundaries

- Internet play, NAT traversal, relays, firewall automation, discovery, server browsing, public listings, or matchmaking.
- Accounts, passwords, cloud identity, ranked networking, network Elo, or persistent network statistics.
- Dedicated-server operation or packaging, host migration, join-in-progress, or spectators.
- Cross-release or cross-content compatibility.
- Compatibility checks for presentation-only assets, local persistence, local controls, or documentation.
- Changes to existing local-only Play behavior.

## Acceptance criteria

- **NET-AC-001 — Platform:** Linux x86-64 and Windows x86-64 instances can participate together, and no other platform or architecture is claimed.
- **NET-AC-002 — Endpoints:** Separate instances connect on one machine or LAN through a directly entered hostname or IP address plus port, with no Internet, NAT, discovery, or matchmaking affordance.
- **NET-AC-003 — Host model:** The session is player-hosted and authoritative, with no dedicated-server product path or host migration.
- **NET-AC-004 — Lifecycle cardinality:** A lobby admits 1–15 participants and players including a valid host-alone lobby; Start requires 2–15 connected participants and players with at least one player each; a degraded match may continue with one connected host while at least two roster players remain; fewer than two ends without winner.
- **NET-AC-005 — Ownership:** The host controls match settings and roster order; each participant controls only its local persons, profiles, and controls; authoritative input and state ownership are enforced.
- **NET-AC-006 — Readiness:** Every participant must be connected, valid, and ready to start; clearing mutations clear all readiness; a disconnected admitted guest retains prior readiness as `Reconnecting` but blocks Start by name; reconnect restores retained readiness only when no later clearing mutation occurred.
- **NET-AC-007 — Admission:** Admission occurs only before match start, and late attempts fail with explicit join-in-progress-prohibited behavior.
- **NET-AC-008 — Compatibility:** Admission requires an exact case-sensitive non-empty network release ID and exact canonical gameplay-content manifest under the defined inclusion, exclusion, path, ordering, content, fixture, and mismatch-copy rules.
- **NET-AC-009 — Timing and failures:** Host startup and complete guest connection each satisfy their 10-second boundaries; host loss is declared by the 5-second boundary; inline validation, specific-failure precedence, retained data, Retry, Edit setup, and Return destinations match this specification.
- **NET-AC-010 — Authority:** Participants control only owned local players while the host owns canonical simulation, rounds, scoring, winner evaluation, and current state in one shared arena.
- **NET-AC-011 — Reconnect:** A reservation begins at host-declared disconnect, expires at `D + 30s`, accepts only strictly-before-deadline restoration, shows positive ceiling seconds without active zero, retains one deadline across attempts, and restores only current state; a later disconnect starts a new reservation.
- **NET-AC-012 — Active disconnect:** Active simulation, timers, hazards, connected input, combat, scoring, winner rules, and round progression continue while reserved players receive no input, remain targets, and count for winner conditions.
- **NET-AC-013 — Atomic removal:** Same-clock leaves and expiries are removed in one atomic batch without removal combat statistics and receive one post-batch winner evaluation; fewer than two players ends without winner; host end/loss takes precedence.
- **NET-AC-014 — Host end and loss:** Confirmed host End session returns the host to `NET-01` and guests to host-ended `NET-09`; unexpected host loss uses distinct `NET-09`; neither migrates, resumes, or persists results.
- **NET-AC-015 — Local independence:** `Play (F1)` starts and completes unchanged without starting or requiring any network service, while `Network (F2)` remains separate.
- **NET-AC-016 — Cancellation and leave:** Host startup Cancel, guest connection Cancel, guest lobby/match/summary Leave, reconnect Leave session, host End session, and their confirmations retain or discard data and reach exactly the specified destinations.
- **NET-AC-017 — Truthful UX:** Role, Connected/Reconnecting, readiness, pending, disabled, failure, timeout, endpoint, consequence, host-ended, and host-loss states use visible truthful text and never claim unsupported success or behavior.
- **NET-AC-018 — Session-only results:** Results are labeled `Session only`, retained through summary and following lobby with departed rows labeled, replaced by the next match, discarded on session end/loss, and never persisted locally or to Elo.
- **NET-AC-019 — Explicit boundaries:** UI, packaging, and release claims omit every non-goal and exclude presentation/cosmetic/local persistence/control/documentation material from gameplay compatibility.

## Exact downstream issue mapping

Each issue owns the listed criteria without changing their normative boundaries. Criteria are enumerated rather than represented by overlapping ranges.

| Issue | Responsibility | NET-AC criteria |
|---|---|---|
| [#29](https://github.com/mkapusnik-apps/duel6r/issues/29) | Session transport and connection lifecycle | `NET-AC-002`, `NET-AC-004`, `NET-AC-007`, `NET-AC-009`, `NET-AC-011`, `NET-AC-016` |
| [#30](https://github.com/mkapusnik-apps/duel6r/issues/30) | Protocol, release, capability, and content compatibility | `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-019` |
| [#31](https://github.com/mkapusnik-apps/duel6r/issues/31) | Player-hosted server lifecycle supervision | `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016` |
| [#32](https://github.com/mkapusnik-apps/duel6r/issues/32) | Authoritative headless match simulation | `NET-AC-004`, `NET-AC-010`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#33](https://github.com/mkapusnik-apps/duel6r/issues/33) | Local devices and authoritative remote input | `NET-AC-005`, `NET-AC-010`, `NET-AC-012` |
| [#34](https://github.com/mkapusnik-apps/duel6r/issues/34) | Canonical state replication and identities | `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#35](https://github.com/mkapusnik-apps/duel6r/issues/35) | Responsiveness and recovery budgets | `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012` |
| [#36](https://github.com/mkapusnik-apps/duel6r/issues/36) | Disconnect, reconnect, shutdown, and host loss | `NET-AC-004`, `NET-AC-006`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016` |
| [#37](https://github.com/mkapusnik-apps/duel6r/issues/37) | Network result and persistence enforcement | `NET-AC-013`, `NET-AC-014`, `NET-AC-018` |
| [#38](https://github.com/mkapusnik-apps/duel6r/issues/38) | Host, join, lobby, status, recovery, and error UX | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |
| [#39](https://github.com/mkapusnik-apps/duel6r/issues/39) | Trust boundaries and abuse limits | `NET-AC-002`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-010`, `NET-AC-011`, `NET-AC-019` |
| [#40](https://github.com/mkapusnik-apps/duel6r/issues/40) | Supported network packaging and deployment documentation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-008`, `NET-AC-015`, `NET-AC-019` |
| [#41](https://github.com/mkapusnik-apps/duel6r/issues/41) | Complete release-candidate validation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |

Issue #28 approves this target but does not satisfy parent issue #27's implementation or release evidence. In-process loopback, documentation, or planned screenshots are insufficient to claim playable networking.

## Evidence expectations

- Product review traces each downstream issue to the exact criteria above and confirms non-goals remain excluded.
- UX review traces `MENU-01`, `MENU-02`, `CONS-01`, and `NET-01`–`NET-09` to applicable criteria and assesses one representative wireframe per affected screen.
- Issue #38 supplies one implementation screenshot for each of the 12 planned entries in [`docs/screenshots/README.md`](screenshots/README.md). No current screenshot is valid for the changed target UI.
- Reviewer evidence checks lifecycle cardinality, timing boundaries, precedence, exact compatibility fixtures/copy, destinations, and local-only preservation.
- Tester evidence independently verifies applicable criteria at downstream implementation SHAs. Issue #28 itself is documentation-only and requires no automated test implementation.
- DevOps evidence confirms supported Linux and Windows x86-64 artifacts and hosted checks at the applicable release-candidate SHA.
- Issue #41 validates the complete production path before parent issue #27 or release text claims playable network support.
