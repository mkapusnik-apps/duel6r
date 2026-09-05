# First-release network play target specification

## Status and authority

This document is the authoritative product target for issue [#28](https://github.com/mkapusnik-apps/duel6r/issues/28), a subtask of [#27](https://github.com/mkapusnik-apps/duel6r/issues/27). It defines approved first-release network-play scope and journeys, not implemented behavior. The current code remains an experimental scaffold with no playable network support, as documented in [`docs/networking.md`](networking.md). The enforced trusted-loopback/private-LAN deployment boundary and abuse limits are defined in [`docs/network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md).

The target network screens in [`docs/screens`](screens/README.md) implement this product specification. Existing local behavior remains governed by [`docs/features.md`](features.md), which intentionally makes no network-support claim.

The authoritative player-hosted service lifecycle target is in [`network-host-service-lifecycle.md`](network-host-service-lifecycle.md).

The authoritative headless match target is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

The canonical state-replication target is in [`network-state-replication.md`](network-state-replication.md).

The authoritative player-input target is in [`network-authoritative-player-input.md`](network-authoritative-player-input.md).

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
- **Session:** The period from confirmed host startup until intentional host End session, host-local supervised service failure, or application shutdown.
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
| Results | Session-only network results retained until a new match starts or the session ends | Local statistics or Elo writes |

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
- User-visible release mismatch copy is exactly `Network release mismatch. Use the same supported game release as the host.` It never includes either peer-supplied release ID.

### Canonical gameplay-content manifest

The compatibility manifest includes content that can alter authoritative simulation or match outcomes:

- gameplay levels and their gameplay metadata;
- gameplay configuration and data definitions that control the approved built-in simulation.

First-release network matches disable all optional Lua, profile, and gameplay scripts. The authoritative policy is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

It excludes presentation-only and participant-local material:

- textures, sounds, fonts, and other cosmetic presentation;
- local people, profiles, statistics, Elo, saves, and other persistence;
- local control assignments and controller presets;
- documentation.

Each manifest entry uses a canonical session-relative logical path with all of these constraints:

- ASCII only, with total path length from 1 through 240 bytes;
- from 1 through 16 `/`-separated segments, with no leading or trailing slash;
- each segment is from 1 through 64 characters and matches `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`;
- no percent encoding, backslash, whitespace, control character, newline, Unicode, bidirectional-control character, raw filesystem path, empty segment, or duplicate case-sensitive logical path;
- unique entries sorted in ascending unsigned-ASCII byte order.

Equality compares canonical logical paths and file content. Timestamps, permissions, owners, archive order, and other filesystem or package metadata do not affect equality.

User-visible copy is fixed and non-disclosing:

- invalid manifest: `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.`
- content mismatch: `Gameplay content mismatch. Use the host's exact supported gameplay content.`

User-visible copy, including errors and disabled reasons, never includes a peer-supplied release ID, path, value, hash, or raw payload. Diagnostics may identify the first differing path only after that path has independently passed every canonical-path rule above. Diagnostics never include an invalid path, raw payload, or other peer-supplied value.

The following synthetic fixtures are normative compatibility examples, not shipped content:

| Fixture | Host | Guest | Expected result |
|---|---|---|---|
| Release match | ID `release-fixture-a` | ID `release-fixture-a` | Continue to manifest comparison |
| Case mismatch | ID `release-fixture-a` | ID `Release-Fixture-A` | `Network release mismatch. Use the same supported game release as the host.` |
| Content match | `config/gameplay.json` containing `fixture-a`; `levels/arena.json` containing `fixture-b` | Same paths and exact contents in any source enumeration order | Compatible |
| Content mismatch | `levels/arena.json` containing `fixture-b` | `levels/arena.json` containing `fixture-c` | `Gameplay content mismatch. Use the host's exact supported gameplay content.` |
| Path mismatch | `levels/arena.json` | `Levels/arena.json` | `Gameplay content mismatch. Use the host's exact supported gameplay content.`; diagnostics may use canonical path `Levels/arena.json` |
| Invalid path | Not applicable | a peer value containing a disallowed segment | `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.`; user copy and diagnostics omit the raw value |

Issue #30 owns canonical serialization, content digest choice, exchange, comparison mechanics, and protocol enforcement. The authoritative issue #30 target is in [`network-compatibility-and-admission.md`](network-compatibility-and-admission.md). Those mechanics must produce the exact policy, fixture outcomes, and user-visible copy above.

## Timing, failures, and precedence

### Host startup

- Host startup has a 10-second deadline on the host session clock from Start session activation.
- Success requires confirmed listener/session readiness strictly before the deadline. At or after 10 seconds, startup fails and no listener may remain.
- Cancel before success returns to editable `NET-02`, retains endpoint and local-player setup, and leaves no listener or session.
- A specific confirmed startup failure is shown instead of a generic timeout when known before the deadline.
- Issue #31 defines exact supervision states, startup outcomes, Retry rules, cleanup, and fixed copy without changing these boundaries.

### Guest connection

- Hostname/address and port validation remains inline in editable `NET-03`; invalid input does not begin the connection clock.
- A connection attempt has one 10-second total deadline covering name resolution, transport connection, compatibility, capacity, host admission, and lobby confirmation.
- Success must be confirmed strictly before the deadline. At or after 10 seconds, the generic result is `Connection timed out.` unless a higher-precedence result below was established first.
- Cancel returns to editable `NET-03` and retains endpoint and local-player setup.
- `NET-08` Retry repeats the same retained attempt when still valid; Edit setup returns to editable `NET-02` or `NET-03` with all setup retained; Return to Network goes to `NET-01`.

### Initial admission precedence and fixed copy

User Cancel and local inline validation take precedence before any host or transport result. Once a valid request reaches the host, the host evaluates the first applicable admission result in this fixed order:

1. malformed request — `Connection request rejected.`;
2. trust or authorization rejection — `Connection not authorized.`;
3. release mismatch — `Network release mismatch. Use the same supported game release as the host.`;
4. invalid gameplay-content manifest — `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.`;
5. gameplay-content mismatch — `Gameplay content mismatch. Use the host's exact supported gameplay content.`;
6. match already started — `Match already started. Join-in-progress is not supported.`;
7. capacity reached — `Session is full.`;
8. other host policy rejection — `Host rejected the connection.`;
9. success.

The host stops at the first applicable result. A complete valid host response accepted before the deadline takes precedence over a later generic transport symptom. Without a complete host response, initial connection outcomes use this order:

1. name-resolution failure — `Host name could not be resolved.`;
2. unreachable or refused connection — `Host unreachable.`;
3. reset or closed transport before a complete admission response — `Connection ended before admission completed.`;
4. no complete result at the deadline — `Connection timed out.`

All copy is fixed and non-disclosing. It never interpolates a peer-supplied release ID, manifest path, credential, policy value, payload, or other untrusted value.

### Reconnect outcome precedence

First release has no guest-observable unexpected host-termination signal. Loss of contact, silence, connection refusal, unreachable host, reset, timeout, host crash, host-machine loss, listener loss, temporary transport failure, or no response is ambiguous. None proves host end. Every such outcome keeps the guest in `NET-07` and permits retries against the original fixed 30-second deadline.

When reconnect outcomes compete, apply this fixed precedence:

1. valid intentional host End session notice accepted through the current established session → host-ended `NET-09`;
2. accepted reconnect strictly before the deadline → current authoritative prior state;
3. terminal reconnect rejection → `NET-08` with Retry disabled;
4. retryable ambiguous failure → remain in `NET-07` against the unchanged deadline;
5. deadline expiry → `NET-08` with `Reconnect time expired. The session could not be restored.` and Retry disabled.

Terminal reconnect rejection means an authoritative response establishes an invalid or expired reconnect credential, missing reservation or removed participant, or compatibility/trust rejection. Its fixed non-disclosing copy is one of:

- `Reconnect authorization failed. This session cannot be restored.`;
- `Reconnect reservation is no longer available. This session cannot be restored.`;
- `Network release mismatch. This session cannot be restored.`;
- `Gameplay content mismatch. This session cannot be restored.`

Reconnect authorization checks do not disclose whether a reservation, participant, or credential exists. A failed/wrong/all-zero credential or wrong session, participant, or reservation scope leaves the valid reservation and its original deadline unchanged; only the offending connection closes under rate policy. Successful consume invalidates before restoration and replay fails. Expiry, participant removal, session end, explicit cancellation, and correctly scoped replacement invalidate the credential; replacement invalidates the old credential before a replacement can become usable. Every credential/scope failure uses exactly `Reconnect authorization failed. This session cannot be restored.`

No isolated guest state may claim that the host ended the session or that players were removed without a valid host-end notice accepted through the current established session or an authoritative terminal rejection.

The host application's local supervisor may detect that its own hosted service stopped unexpectedly. That host-only observation routes the host application to `NET-08` with exactly `Hosted session stopped unexpectedly.` It is never transmitted, inferred, or reused as evidence of host end for guests; isolated guests remain in `NET-07` until terminal rejection or deadline expiry.

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
5. Session-only result rows remain visible in summary and lobby until a new match starts and clears them or the session ends.
6. An approved interruption returns connected participants directly to `NET-04`. It does not enter `NET-06`.

### Explicit cancel, leave, and end actions

- Guest `Leave` in `NET-04` requires confirmation: `Leave session? Your players will be removed and you will return to Network.` Confirm → guest `NET-01`; Cancel → current lobby.
- Guest `Leave session` during `NET-05` requires confirmation: `Leave session? Your players will be removed immediately and the match will continue without reconnect.` Confirm → guest `NET-01`; Cancel → active match.
- Guest `Leave` from `NET-06` uses the lobby consequence copy and destination.
- `NET-07` action is `Leave session`. It requires `Leave session? Your reserved players will be removed now and reconnect will stop.` Confirm → `NET-01`; Cancel → reconnect continues against the unchanged deadline.
- Host `End session` in `NET-04`, `NET-05`, or `NET-06` requires `End session for everyone?` Confirm → host `NET-01`; guests see the host-ended variant of blocking `NET-09`. Cancel → current state.
- Only a valid intentional host End session notice accepted through the current established session shows guest `NET-09`. Return to Network → `NET-01`.
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
- Retryable resolution, refusal, unreachable, reset, temporary transport, and no-response outcomes remain in `NET-07` for the full remaining reservation. Terminal outcomes follow the fixed precedence above.

## Lifecycle-specific atomic removal

At each host session clock instant, applicable confirmed intentional leaves and authoritative reservation expiries form one atomic removal batch. Removal itself adds no kill, death, assist, penalty, or other combat statistic. Intentional host End session takes precedence over every removal batch and discards session results. A host-local supervised service failure also ends host processing and discards the host's session result without creating a guest-observable host-end signal.

- **Lobby:** Remove the batch, label departed rows in any retained result, and clear every remaining participant's readiness. Do not evaluate a winner. The retained completed result remains until a new match starts or the session ends.
- **Active round:** Remove the batch, then perform exactly one winner-condition evaluation. With at least two roster players, normal progression continues, including one connected host. With fewer than two, produce the current result `Session only • Interrupted • No winner`, retain it, and return remaining connected participants to `NET-04` with readiness cleared.
- **Non-final round summary:** Preserve the completed round outcome. After the batch, continue to the next round when at least two roster players remain. Otherwise produce the current result `Session only • Interrupted • No winner`, retain the already completed round, and return remaining connected participants to `NET-04` with readiness cleared.
- **Final summary:** Never reevaluate or replace the completed match outcome because of departure. Remove the batch and retain the completed result with affected participant/player rows labeled `Departed`.

An isolated guest reaching its local deadline enters `NET-08`; it does not claim authoritative removal or host end. Only a running host applies authoritative expiry batching on its own session clock.

## Session-only result lifecycle

- Authoritative network results are labeled `Session only` and never write local statistics, Elo, people, profiles, or saves.
- Result state, match outcome, last completed-round outcome, and cumulative rankings are distinct values.
- For a completed result, the match outcome equals the configured final round outcome.
- For an interrupted result, the match outcome is `No winner`.
- An interrupted result keeps its last completed-round outcome when one exists.
- An interrupted result with no completed round has no last completed-round outcome.
- Cumulative rankings include only completed rounds. A ranking leader is not a match champion.
- Completed-match result rows remain available in `NET-06` and the following `NET-04` lobby.
- An interrupted result appears in the following `NET-04` lobby and does not appear in `NET-06`.
- The following lobby must retain each result value without deriving a champion from cumulative ranking.
- A participant or player that leaves after results exist remains in those rows and is labeled `Departed`.
- Starting a new match clears the prior retained result before the new match begins; results are not accumulated as persistent history.
- Intentional host End session, host-local supervised service failure, or application shutdown discards the host's session result set. An isolated guest does not infer that discard from transport failure.
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
- A guest-observable unexpected host-termination signal; only intentional End session notice is guest-observable in first release.
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
- **NET-AC-008 — Compatibility:** Admission requires an exact case-sensitive non-empty network release ID and exact gameplay-content manifest whose logical paths satisfy every ASCII length, segment, character, separator, uniqueness, and unsigned-order rule; fixed user copy discloses no peer release ID, path, value, or raw payload, and diagnostics name only independently validated canonical paths.
- **NET-AC-009 — Timing, admission, and host-local failure:** Host startup and complete initial guest connection satisfy their 10-second boundaries; user Cancel and local validation precede the fixed host admission order, complete host responses, precise transport outcomes, and generic timeout; the host application's local supervisor alone may route the host to `NET-08` with `Hosted session stopped unexpectedly.`, which is never guest evidence; retained data, fixed copy, Retry, Edit setup, and Return destinations match this specification.
- **NET-AC-010 — Authority:** Participants control only owned local players while the host owns canonical simulation, rounds, scoring, winner evaluation, and current state in one shared arena.
- **NET-AC-011 — Reconnect:** A reservation begins at host-declared disconnect, expires at `D + 30s`, accepts only strictly-before-deadline restoration, shows positive ceiling seconds without active zero, retains one deadline across attempts, and restores only current state; silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, and no response remain ambiguous in `NET-07`; only a valid host End notice accepted through the established session, accepted restore, terminal rejection, retryable failure, and expiry follow the fixed precedence.
- **NET-AC-012 — Active disconnect:** Active simulation, timers, hazards, connected input, combat, scoring, winner rules, and round progression continue while reserved players receive no input, remain targets, and count for winner conditions.
- **NET-AC-013 — Lifecycle-specific server-side removal:** A running host batches same-clock confirmed leaves and authoritative expiries without removal combat statistics. Lobby removal clears readiness without winner evaluation. Active-round removal performs one winner evaluation. An interruption preserves completed rounds and their last outcome. It discards the incomplete round and creates a `No winner` match outcome. It returns connected participants to `NET-04`, not `NET-06`. Non-final-summary removal preserves the completed round before continuation or interruption. Final-summary removal retains the completed match outcome and adds departed labels. An isolated guest deadline does not claim server-side removal.
- **NET-AC-014 — Intentional host end:** Only a valid intentional host End session notice accepted through the current established session returns the host to `NET-01` and sends guests to host-ended `NET-09`; first release has no guest-observable unexpected termination signal, and host crash, machine/listener loss, silence, reset, refusal, or timeout remains `NET-07` until terminal rejection or expiry.
- **NET-AC-015 — Local independence:** `Play (F1)` starts and completes unchanged without starting or requiring any network service, while `Network (F2)` remains separate.
- **NET-AC-016 — Cancellation and leave:** Host startup Cancel, guest connection Cancel, guest lobby/match/summary Leave, reconnect Leave session, host End session, and their confirmations retain or discard data and reach exactly the specified destinations.
- **NET-AC-017 — Truthful and non-disclosing UX:** Role, Connected/Reconnecting, readiness, pending, retryable ambiguity, terminal rejection, disabled Retry, expiry, host-local service failure, consequence, and host-ended states use fixed visible copy, disclose no untrusted peer value, and never claim guest-observed host end, player removal, or unexpected termination from isolation alone.
- **NET-AC-018 — Session-only results:** Result state, match outcome, last completed-round outcome, and cumulative rankings are distinct. Completed match outcome equals the final round outcome. Interrupted match outcome is `No winner`. Cumulative rankings include only completed rounds and define no champion. Completed results appear in `NET-06`. Completed and interrupted results remain through the following lobby with departed labels. A new match clears the retained result. Intentional host end, host-local service failure, or application shutdown discards it. An isolated guest expiry does not claim a server-side result transition. Results never persist locally or to Elo.
- **NET-AC-019 — Explicit boundaries:** UI, packaging, and release claims omit every non-goal and exclude presentation/cosmetic/local persistence/control/documentation material from gameplay compatibility.

## Exact downstream issue mapping

Each issue owns the listed criteria without changing their normative boundaries. Criteria are enumerated rather than represented by overlapping ranges.

| Issue | Responsibility | NET-AC criteria |
|---|---|---|
| [#29](https://github.com/mkapusnik-apps/duel6r/issues/29) | Session transport and connection lifecycle | `NET-AC-002`, `NET-AC-004`, `NET-AC-007`, `NET-AC-009`, `NET-AC-011`, `NET-AC-016` |
| [#30](https://github.com/mkapusnik-apps/duel6r/issues/30) | Protocol, release, capability, and content compatibility | `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-019` |
| [#31](https://github.com/mkapusnik-apps/duel6r/issues/31) | [Player-hosted service supervision and host-local failure](network-host-service-lifecycle.md) | `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016` |
| [#32](https://github.com/mkapusnik-apps/duel6r/issues/32) | Authoritative headless match simulation | `NET-AC-004`, `NET-AC-010`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#33](https://github.com/mkapusnik-apps/duel6r/issues/33) | Local devices and authoritative remote input | `NET-AC-005`, `NET-AC-010`, `NET-AC-012` |
| [#34](https://github.com/mkapusnik-apps/duel6r/issues/34) | Canonical state replication and identities | `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#35](https://github.com/mkapusnik-apps/duel6r/issues/35) | Responsiveness and recovery budgets | `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012` |
| [#36](https://github.com/mkapusnik-apps/duel6r/issues/36) | Disconnect, reconnect, shutdown, and intentional host end | `NET-AC-004`, `NET-AC-006`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016` |
| [#37](https://github.com/mkapusnik-apps/duel6r/issues/37) | Network result and persistence enforcement | `NET-AC-013`, `NET-AC-014`, `NET-AC-018` |
| [#38](https://github.com/mkapusnik-apps/duel6r/issues/38) | Host, join, lobby, status, recovery, and error UX | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |
| [#39](https://github.com/mkapusnik-apps/duel6r/issues/39) | Trust boundaries and abuse limits | `NET-AC-002`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-010`, `NET-AC-011`, `NET-AC-019` |
| [#40](https://github.com/mkapusnik-apps/duel6r/issues/40) | Supported network packaging and deployment documentation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-008`, `NET-AC-015`, `NET-AC-019` |
| [#41](https://github.com/mkapusnik-apps/duel6r/issues/41) | Complete release-candidate validation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |

Issue #28 approves this target but does not satisfy parent issue #27's implementation or release evidence. In-process loopback, documentation, or planned screenshots are insufficient to claim playable networking.

## Evidence expectations

- Product review traces each downstream issue to the exact criteria above and confirms non-goals remain excluded.
- UX review traces `MENU-01`, `MENU-02`, `CONS-01`, and `NET-01`–`NET-09` to applicable criteria and assesses one representative wireframe per affected screen.
- Issue #38 must supply one implementation screenshot for each of the 11 planned entries in [`docs/screenshots/README.md`](screenshots/README.md): `SS-002`, `SS-013`, and `SS-015`–`SS-023`. These entries remain planned, and no current screenshot is valid for the changed target UI.
- Reviewer evidence checks lifecycle cardinality, initial admission order, full-deadline ambiguity for every unexpected host failure, intentional-end-only `NET-09`, host-local-only supervision, reconnect precedence, lifecycle-specific removal, exact compatibility fixtures/copy, destinations, and local-only preservation.
- Tester evidence independently verifies at downstream implementation SHAs that every guest-observed host crash, machine/listener loss, silence, reset, refusal, timeout, and no-response case stays `NET-07` through the fixed deadline; that only an accepted intentional End notice enters guest `NET-09`; and that host-local supervision routes only the host to `NET-08`. Issue #28 itself is documentation-only and requires no automated test implementation.
- DevOps evidence confirms supported Linux and Windows x86-64 artifacts and hosted checks at the applicable release-candidate SHA.
- Issue #41 validates the complete production path before parent issue #27 or release text claims playable network support.
