# First-release network play target specification

## Status and authority

This document is the authoritative product target for issue [#28](https://github.com/mkapusnik-apps/duel6r/issues/28), a subtask of [#27](https://github.com/mkapusnik-apps/duel6r/issues/27). It defines the approved first-release network-play scope and user journeys. It does not describe implemented behavior: the current networking code remains an experimental scaffold with no playable network support, as documented in [`docs/networking.md`](networking.md).

The network screen specifications in [`docs/screens`](screens/README.md) define the corresponding target UX. Existing local behavior remains governed by [`docs/features.md`](features.md), which intentionally does not claim network support.

## Terminology

- **Participant:** One running game instance connected to the session. The host is one participant; every other participant is a guest.
- **Local player:** A player controlled from a participant's machine with that participant's selected person, profile, and control assignment.
- **Player:** One combatant in the authoritative match roster. Every player belongs to exactly one participant.
- **Host:** The participant that creates the session, owns the authoritative server process, configures the match, and controls session progression.
- **Guest:** A participant that joins the host's direct endpoint.
- **Endpoint:** A directly entered hostname or IP address plus port.
- **Lobby:** The pre-match and between-match session state where participants, players, readiness, connection state, roster order, and host-controlled settings are visible.
- **Ready:** A participant's confirmation that its current local-player configuration is complete. Any relevant configuration change clears readiness.
- **Session:** The period from successful host or guest lobby admission until leaving, host loss, session cancellation, or shutdown.
- **Supported release/content:** The exact application/protocol release and exact required content set accepted by the host.

## First-release scope matrix

| Dimension | Supported target | Explicitly unsupported |
|---|---|---|
| Platforms | Linux x86-64 and Windows x86-64 | Other operating systems and architectures |
| Cross-platform play | Linux and Windows x86-64 participants in one session | Compatibility with other targets |
| Network environments | Separate game instances on one machine; LAN direct connection | Internet support, NAT traversal, relays, public hosting claims |
| Connection method | Direct hostname or IP address plus port | Discovery, server browser, matchmaking |
| Hosting | Player-hosted session | Dedicated server deployment and host migration |
| Identity and access | Session-local participant identity | Accounts, passwords, cloud identity, ranked identity |
| Participants | 2–15 connected participants | One-participant network sessions or more than 15 participants |
| Players | 2–15 total players | Fewer than two or more than 15 players |
| Local/remote mix | Every participant owns at least one local player; total players are distributed across the 2–15 participants | Spectators or participants with no local player |
| Admission | Lobby admission before a match starts | Join-in-progress |
| Compatibility | Exact supported release and required content | Cross-version or cross-content compatibility |
| Results | Session-only network scores and summary | Writing network results to local statistics or Elo |

For every valid session, `2 <= participants <= players <= 15`. Same-machine support means separate running game instances communicating through the production transport; it does not mean adding networking to the existing local Play journey.

## Ownership and configuration

- The host starts and owns the player-hosted authoritative session and exposes its direct endpoint.
- The host configures mode, level or level rotation, round settings, other approved match settings, and authoritative roster order.
- Each participant configures only its own local persons, profiles, and controls.
- Every participant must contribute at least one local player. The combined roster must contain 2–15 players.
- The lobby must identify the host and each guest textually, group players under their owning participant, and show the authoritative roster order.
- Host-owned settings are read-only for guests. Participant-owned player configuration is editable only by that participant.
- Unsupported release/content is rejected before lobby admission with an actionable reason; first release does not negotiate compatibility.

## Lobby and readiness

- A participant may mark ready only after its local-player configuration is valid.
- All connected participants, including the host, must be ready before the host can start.
- Adding, removing, or editing a local player; changing a person, profile, or control; changing host match settings; changing roster order; or changing session membership clears every participant's readiness.
- The lobby must state why `Ready` or `Start match` is disabled, including invalid player count, missing local configuration, incompatible content, or named unready participants.
- The host alone can start or cancel the session. A guest can leave the session.
- No participant may join after the host starts the match.

## User journeys

### Host

1. From `MENU-01`, the user chooses `Network (F2)` without changing the independent local roster or local Play path.
2. `NET-01` offers Host or Join and states the direct LAN/same-machine scope.
3. Host opens `NET-02`, selects a valid port, configures at least one local player, and requests session creation.
4. While creation is pending, the UI says that the session is being started; it must not claim to be listening or ready before the runtime confirms it.
5. Success enters `NET-04` as host. Failure enters `NET-08` with the concrete reason and retry or return actions.
6. The host configures match settings and roster order, marks ready, waits for every participant to be ready, and starts the match.

### Join

1. From `NET-01`, Join opens `NET-03`.
2. The guest enters a direct hostname or address and port and configures one or more local players.
3. Connect shows the exact endpoint and a truthful connecting state. Cancel returns to editable join setup; it never implies successful admission.
4. Compatibility, capacity, endpoint, timeout, or host rejection enters `NET-08` with the exact available reason.
5. Successful admission enters `NET-04`; host settings and roster order are visible but not guest-editable.

### Match

1. The host can start from `NET-04` only when participant and player counts are valid and every participant is ready.
2. Start closes admission. Late connection attempts are rejected because join-in-progress is unsupported.
3. `NET-05` uses the existing undivided shared arena and authoritative match, round, scoring, and winner rules.
4. A participant controls only its local players. The server owns canonical simulation and results.
5. Session status must not hide the arena or imply that a disconnected player's simulation has paused.

### Final summary and return

1. Completing the configured match opens `NET-06` for every connected participant.
2. The summary shows final authoritative results and labels them `Session only`.
3. Network results are not written to local persistent statistics or Elo.
4. The host chooses Return to lobby, which moves connected participants to `NET-04` with readiness cleared for the next match, or ends the session.
5. Guests may leave to `NET-01`. Ending the session returns guests through a truthful session-ended state rather than implying host loss or migration.

### Cancellation and exit

- Back or Cancel before a host session exists returns to `NET-01` without starting a network service.
- Cancel during connection stops that attempt and restores editable `NET-03` state.
- A guest leaving lobby, match, or summary removes that participant according to the same immediate-removal outcome as an expired reconnect reservation; leaving is not offered as a pause.
- Host cancellation in the lobby or summary ends the session for all participants and returns them to `NET-01` with an explicit host-ended reason.
- Host exit during a match is host loss and follows `NET-09`.
- Returning from `NET-01` goes to `MENU-01`; it does not alter or start the local-only journey.

## Failures and truthful status

- The UI must distinguish validation, host startup, endpoint unreachable, timeout, capacity, exact-release mismatch, content mismatch, host rejection, transport disconnect, reconnect expiry, session ended, and host loss when the runtime can identify them.
- `Connecting`, `Starting session`, `Reconnecting`, `Disconnected`, and failure states must describe the confirmed runtime state and must not imply readiness early.
- Every failure state must preserve the actionable endpoint or session context needed for Retry when retry is valid.
- Disabled actions must show a visible textual reason.
- Unsupported environments or features must not be presented as available actions.

## Guest disconnect and reconnect

- An unintentionally disconnected guest receives one 30-second reconnect window. The host reserves that participant's player slots and session identity for the window.
- `NET-07` shows a seconds-remaining countdown and the state to which a successful reconnect will return.
- During an active round, disconnect does not pause the match: simulation, timers, hazards, connected-player inputs, scoring, and round progression continue.
- Reserved players receive no input, remain valid combat targets, and continue to count for winner conditions. Normal damage, death, score, and winner rules apply.
- A successful reconnect restores the guest to the current authoritative lobby, active match, or final summary state; it does not rewind missed events or restore an earlier snapshot as current truth.
- When the 30 seconds expire, the reserved players are removed without adding a kill, death, assist, penalty, or other combat statistic for the removal itself.
- After removal, the server reevaluates the current winner condition and continues normal round progression when at least two players remain.
- If fewer than two players remain, the match ends without a winner and all remaining connected participants return to `NET-04` with readiness cleared.
- A guest that intentionally leaves is removed immediately and does not receive the reconnect window.

## Host loss

- First release has no host migration.
- If the host process, transport, or host connection is lost, the authoritative session ends immediately.
- Guests see `NET-09` with `Host connection lost`, an explicit statement that the session ended and cannot be resumed, and a Return to Network action leading to `NET-01`.
- No network result from the interrupted session is persisted locally.

## Local-only preservation

- `Play (F1)` on `MENU-01` remains the existing local-only journey.
- Local Play must start and complete without starting, connecting to, or requiring a network service.
- `Network (F2)` is a distinct target action and must not replace, wrap, or silently redirect local Play.
- Network session settings, readiness, scores, failures, and participant state must not mutate local persistent people, statistics, Elo, or local setup state except through an independently approved local edit.
- This specification does not change current local gameplay requirements in [`docs/features.md`](features.md).

## Non-goals

- Internet play, NAT traversal, relays, or firewall automation.
- Discovery, server browsing, public listings, or matchmaking.
- Accounts, passwords, authentication services, cloud identity, ranked play, or network Elo.
- Dedicated-server operation or packaging.
- Join-in-progress, spectators, or host migration.
- Cross-version or cross-content compatibility.
- Persistent network match statistics.
- Changes to existing local-only Play behavior.

## Acceptance criteria

- **NET-AC-001 — Platform matrix:** The release supports Linux x86-64 and Windows x86-64 participants, including Linux/Windows cross-platform sessions, and claims no other platform or architecture.
- **NET-AC-002 — Direct environments:** Two separate instances can use a directly entered hostname or address plus port on one machine or a LAN; no Internet, NAT, discovery, or matchmaking affordance is presented.
- **NET-AC-003 — Player-hosted model:** A player host owns the authoritative session; no dedicated-server or host-migration behavior is offered.
- **NET-AC-004 — Participant and player limits:** Every admitted session satisfies `2 <= participants <= players <= 15`, and every participant owns at least one local player.
- **NET-AC-005 — Configuration ownership:** The host controls match settings and roster order, while each participant controls only its local persons, profiles, and controls.
- **NET-AC-006 — Readiness:** Start remains disabled until every participant is valid and ready, and a material configuration, roster, or membership change clears every participant's readiness with a visible reason.
- **NET-AC-007 — Admission:** Guests can join only before match start; attempts during a match are clearly rejected because join-in-progress is unsupported.
- **NET-AC-008 — Compatibility:** Admission requires the exact supported release and required content set; mismatches fail before lobby admission with a specific reason.
- **NET-AC-009 — Truthful lifecycle:** Host, join, connecting, lobby, match, summary, cancellation, failure, and return journeys expose confirmed state, valid actions, and disabled reasons without premature success claims.
- **NET-AC-010 — Authoritative match:** Connected participants complete the configured match in one shared arena while controlling only their owned local players and receiving authoritative state and outcomes.
- **NET-AC-011 — Reconnect reservation:** An unintentionally disconnected guest has 30 seconds to restore its reserved identity and current authoritative lobby, match, or summary state.
- **NET-AC-012 — Active-round disconnect:** During the reconnect window, active simulation and round progression continue; reserved players receive no input, remain valid targets, count for winner conditions, and follow normal combat outcomes.
- **NET-AC-013 — Reconnect expiry:** Expiry removes reserved players without removal-generated combat statistics, reevaluates the winner, and ends the match without a winner when fewer than two players remain before returning remaining participants to the lobby.
- **NET-AC-014 — Results and host loss:** Final results are labeled `Session only` and never update local statistics or Elo; host loss ends the session without migration or persistence and sends guests to the documented host-loss journey.
- **NET-AC-015 — Local independence:** `Play (F1)` starts and completes the existing local-only journey without starting or requiring a network service, while `Network (F2)` remains a separate target path.

## Issue traceability

| Requirement area | Acceptance criteria | UX screens | Downstream implementation issues |
|---|---|---|---|
| Entry, platforms, endpoint, hosting | `NET-AC-001`–`NET-AC-003`, `NET-AC-015` | `MENU-01`, `NET-01`–`NET-03` | #29, #31, #38, #40 |
| Capacity, ownership, lobby, readiness | `NET-AC-004`–`NET-AC-007` | `NET-02`–`NET-05` | #30–#33, #38 |
| Compatibility and truthful lifecycle | `NET-AC-008`–`NET-AC-010` | `NET-03`–`NET-06`, `NET-08` | #29–#35, #38–#40 |
| Reconnect and host loss | `NET-AC-011`–`NET-AC-014` | `NET-07`–`NET-09` | #34–#39 |
| Session-only results | `NET-AC-014` | `NET-05`, `NET-06`, `NET-09` | #32, #34, #37, #38 |
| End-to-end acceptance | All criteria | All network screens | #41 |

Issue #28 approves this scope and journey target but does not satisfy the implementation or release evidence required by parent issue #27. Downstream issues #29–#41 remain responsible for transport, trust, compatibility, server lifecycle and simulation, input, replication, responsiveness, recovery, persistence enforcement, UI, packaging, and complete release validation.

## Evidence expectations

- Product review must confirm that implementation acceptance criteria preserve this scope and its explicit non-goals.
- UX review must trace `MENU-01` and `NET-01`–`NET-09` to the applicable `NET-AC` criteria and assess one representative wireframe per screen.
- Issue #38 must provide one implementation screenshot for each planned matrix entry in [`docs/screenshots/README.md`](screenshots/README.md); no current application screenshot is valid evidence for the planned network UI.
- Reviewer evidence must trace downstream behavior and errors to the approved journeys without weakening local-only independence.
- Tester evidence must independently verify the criteria at the applicable downstream implementation SHA. Issue #28 itself is documentation-only and requires no automated test implementation.
- DevOps evidence must confirm supported Linux and Windows x86-64 artifacts and hosted checks at the applicable release-candidate SHA when downstream implementation reaches those gates.
- Parent issue #27 may claim playable network support only after issue #41 validates the complete production path; in-process loopback or documentation alone is insufficient.
