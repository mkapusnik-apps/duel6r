# First-release network play target specification

## Status and authority

The product supports the current player-hosted network journey and an operator-configured encrypted dedicated extension with the same optional Password procedure. NET-PASS-001–018 own common input, operator configuration and secret handling; dedicated X.509 endpoint verification remains mandatory even without a password. NET-PUB replaces dedicated-hosting exclusions only for this extension. Player-hosted service ownership does not apply to dedicated participants. Both modes apply hosted solo and ongoing admission under NET-SOLO-001–006 and NET-ADM-001–016 while retaining compatibility, gameplay, ownership, readiness, results and recovery except for the explicit controller/service-lifecycle differences below. Local Play, platform contracts, directory channel origins and unrelated current-design structure remain unchanged. Target support does not establish implementation, live availability or release acceptance.

## Public dedicated pilot

A **session controller** is the first successfully admitted participant in a dedicated session. This participant has the existing host's match-control permissions but does not own the server process. A **pilot environment** is one independently configured operator service environment; it is not a required cloud staging or production deployment. The service clock supplies the existing host-session clock for dedicated sessions.

The dedicated service must support Linux x86-64 and the existing Linux ARM64 (AArch64) packaged target. Linux ARM64 support uses the package and runtime baseline in [raspberry-pi5.md](raspberry-pi5.md). Secure ARM64 operation must satisfy PI5-003. Dedicated client participation remains limited to Linux x86-64 and Windows x86-64. Linux ARM64 clients, including Raspberry Pi clients, must not join dedicated sessions. Dedicated macOS hosting and client participation remain excluded. Current Local Play, player-hosted behavior and platform acceptance boundaries remain unchanged.

The service is a headless publishing variant of a hosted session: it prepares one ready empty session before first admission and publishes that session under the [directory contract](network-host-directory.md). The client uses an operator-supplied endpoint through Direct connect or the selected dedicated listing's operator TLS frontend. No gameplay domain, compiled gameplay endpoint, cloud provider, provisioning, or billable activation is selected by this contract. Current directory origins remain directory endpoints, not dedicated-gameplay defaults.

- **NET-PUB-001** Each pilot environment must provide at most one active session.
- **NET-PUB-002** Before first admission or directory publication, the service must prepare exactly one ready empty session with a nonzero session identity. The first authorized, compatible committed admission must use that same prepared and advertised identity.
- **NET-PUB-003** The service must assign session-controller authority to exactly that first admitted participant.
- **NET-PUB-004** Concurrent admission attempts must not create multiple sessions or multiple controllers.
- **NET-PUB-005** Later admitted participants must join the existing session as guests in its current authoritative lobby, immediate-play or Connected-waiting match state under NET-ADM-001–016.
- **NET-PUB-006** The controller must have the existing host permissions for match settings, roster order, match start, return to lobby, and session end.
- **NET-PUB-007** A guest must not acquire controller authority through a request, reconnect, controller departure, or service restart.
- **NET-PUB-008** After first committed controller admission, dedicated sessions must apply the same hosted 1–15 connected/valid/ready participant and owned-player Start conditions, NET-SOLO-001–006 and NET-ADM-001–016 as player-hosted sessions, retaining exact compatibility, authoritative gameplay and session-only results. Dedicated controller End/Leave/expiry remains session-ending under NET-PUB/HSL-PUB and takes precedence over ordinary roster-reduction solo rules.
- **NET-PUB-009** A confirmed controller Leave or End session must end the session for every participant.
- **NET-PUB-010** An unintentional controller disconnect must use the existing fixed 30-second reconnect reservation without transferring controller authority.
- **NET-PUB-011** Controller reservation expiry must end the dedicated session instead of transferring control or continuing without a controller.
- **NET-PUB-012** Guest departure and reconnect must retain the existing guest lifecycle behavior.
- **NET-PUB-013** Session end must discard session-only results and invalidate all session identities and reconnect credentials.
- **NET-PUB-014** After session cleanup, the service must prepare a fresh distinct ready empty session and its listing for a new first authorized participant. The ended identity must be invalid immediately, without waiting for directory removal or expiry.
- **NET-PUB-015** A service restart must not restore an ended session or its controller authority.
- **NET-PUB-017** Dedicated Join setup must support an operator-supplied hostname or IPv4 endpoint and separate port through unbound Direct connect or a selected dedicated listing. Browser-selected setup must retain its hosting/security mode, operator frontend endpoint and expected nonzero session identity; it must not replace a selected endpoint or silently become a direct attempt.
- **NET-PUB-018** The application must not connect until the user requests Connect.
- **NET-PUB-019** The application must not fall back from an unavailable dedicated endpoint to another endpoint, player-hosted mode, or an unencrypted connection.
- **NET-PUB-020** Public connection, security checks, admission, clock calibration, and initial-state validation must share the existing single 10-second connection deadline.
- **NET-PUB-021** Dedicated session behavior must preserve offline Local Play and the current player-hosted journey, platform contracts, listening modes, directory origins, and host settings.
- **NET-PUB-023** Normal controller application shutdown must request session end; when the service cannot receive that request, the existing controller reservation expiry must end the session.
- **NET-PUB-024** A prepared empty dedicated session must have zero participants, zero players, no controller and no round. Its nonzero identity must remain the same through first committed admission, lobby, gameplay, results, reservations and same-session Return to lobby. Offers, listing metadata and client selection must grant no authority; exactly the first committed admission becomes controller and concurrent later committed admissions become guests.
- **NET-PUB-025** A prepared empty dedicated session with zero participants/players and no controller must not start a match. Only after committed controller admission may the service accept Start under the hosted connected, valid, ready and owned-player conditions in NET-SOLO-001. No extra solo confirmation is required.

End, controller expiry and restart must invalidate the old identity immediately. Cleanup precedes preparation/publication of a fresh distinct identity. Old and new directory rows may coexist after lost removal, but an old selected identity must be rejected before authority allocation, never redirected to the fresh session. An idle selected session may become lobby or first-round while setup is open; actual authority determines the admitted role and destination while the selected identity remains fixed. The service owns publication and its memory-only owner credential independently of the controller. Directory failure must not stop gameplay, direct joining or Local Play. The TLS frontend must be operator-configured valid DNS or concrete non-loopback unicast IPv4 with a valid separate port, never the private backend, wildcard, directory origin or fictional local interface. X.509 identity verification precedes common EC-JPAKE Password/No password operation without fallback. Outer TLS, protected PROXYv2 handoff and inner exchange share the single absolute 10-second attempt; no stage restarts the deadline and application framing always uses the inner channel. The controller cannot manage the operator password.

NET-PUB-016 and NET-PUB-022 are retired; no hard-coded gameplay hostname or production prefill is required. Retired identifiers must not be reused. The public security policy remains authoritative in [network-trust-and-abuse-limits.md](network-trust-and-abuse-limits.md). Dedicated service and confirmed interruption behavior remains authoritative in [network-host-service-lifecycle.md](network-host-service-lifecycle.md). [Network deployments](network-deployments.md) distinguishes this repository extension from live operation and the separately approved directory deployment.

### Public pilot acceptance criteria

| Criterion | Required outcome | Requirements |
|---|---|---|
| **NET-PUB-AC-001** | A ready empty session has one advertised nonzero identity and zero participants/players, no controller and no round. Concurrent authorized first joins preserve that identity and produce exactly one controller at first committed admission; later joins enter the current lobby, immediate-play or Connected-waiting match as guests under NET-ADM-001–016. Offers, selection, rejected and cancelled admission grant no authority. The empty session cannot start a match. | NET-PUB-001–005, NET-PUB-024–025 |
| **NET-PUB-AC-002** | Only the controller can change host-owned settings, order the roster, start, return to lobby, or end the session. Hosted 1–15 validity/ownership/readiness, NET-SOLO opposition/outcome and NET-ADM ongoing/waiting/final-closure rules apply after committed controller admission; dedicated End/Leave/expiry retains precedence. | NET-PUB-006–008, NET-SOLO-001–006, NET-ADM-001–016 |
| **NET-PUB-AC-003** | Controller Leave ends the session. Normal controller application shutdown requests session end. Controller contact loss permits the existing 30-second restore to the same authority; expiry ends the session without migration. Guest departure retains existing behavior. | NET-PUB-009–012, NET-PUB-023 |
| **NET-PUB-AC-004** | Ending, controller expiry or restart immediately invalidates the old session/result/authority before directory removal completes. After cleanup, a fresh distinct empty session/listing is prepared. Stale old selections reject before authority allocation without redirection. First admission, completed summary and same-session Return preserve the current session identity. | NET-PUB-013–015, NET-PUB-024 |
| **NET-PUB-AC-005** | Direct dedicated setup remains unbound and accepts the operator endpoint. Selected dedicated setup binds hosting/security mode, frontend endpoint and expected nonzero identity through offer, confirmation, snapshot and reconnect grant. Credential correction or eligible Retry preserves that binding; endpoint changes require refreshed selection rather than overwriting it. Explicit Direct transition clears binding and secrets. No implicit connection, fallback or change to directory origin occurs; complete admission retains the single 10-second boundary. | NET-PUB-017–020, NET-PUB-024 |
| **NET-PUB-AC-006** | Linux x86-64 and Windows x86-64 clients must support the same dedicated journey with Linux x86-64 and the existing Linux ARM64 packaged service. Clients Browse to the service's ready empty session, complete first-controller admission, and exercise eligible solo Start, ongoing immediate/waiting admission, strict eligible spawn and final closure through lobby/match/completed summary, same-session Return and fresh reuse after End/cleanup. Linux ARM64 service startup must reach actual secure readiness under HSL-PUB-002 and NET-EXEC-AC-002; help success alone is insufficient. Current Local Play and player-hosted behavior remain available under their platform contracts. Linux ARM64 and Mac clients must not join dedicated sessions. Updated Mac/Pi clients retain inspectable dedicated rows with Join disabled under NET-DIR-032. | NET-PUB-008, NET-PUB-021, NET-PUB-024–025, NET-ADM-001–016, NET-SOLO-001–006; PI5-003; HSL-PUB-002; NET-EXEC-AC-002 |

This document is the authoritative product target for issue [#28](https://github.com/mkapusnik-apps/duel6r/issues/28), a subtask of [#27](https://github.com/mkapusnik-apps/duel6r/issues/27). It defines approved first-release network-play scope and journeys, not implemented behavior. The current code remains an experimental scaffold with no playable network support, as documented in [`docs/networking.md`](networking.md). The enforced trusted-loopback/private-LAN deployment boundary and abuse limits are defined in [`docs/network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md).

The target network screens in [`docs/screens`](screens/README.md) implement this product specification. Existing local behavior remains governed by [`docs/features.md`](features.md), which intentionally makes no network-support claim.

The authoritative player-hosted service lifecycle target is in [`network-host-service-lifecycle.md`](network-host-service-lifecycle.md).

The authoritative headless match target is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

The canonical state-replication target is in [`network-state-replication.md`](network-state-replication.md).

The authoritative player-input target is in [`network-authoritative-player-input.md`](network-authoritative-player-input.md).

The authoritative responsiveness and recovery target is in [`network-responsiveness-and-recovery.md`](network-responsiveness-and-recovery.md).

The authoritative package, deployment, reinstallation, and rollback requirements are in [`network-deployments.md`](network-deployments.md).

## Terminology and clock

- **Participant:** One admitted game instance. The host is one participant; every other participant is a guest.
- **Connected participant:** An admitted participant with a currently accepted transport connection.
- **Local player:** A player controlled from a participant's machine with that participant's person and control assignment.
- **Roster player:** One combatant in the authoritative session roster. Every roster player belongs to exactly one participant until removal.
- **Host:** The participant that creates the session, owns the authoritative server process, configures the match, and controls session progression.
- **Guest:** A participant admitted through the host's direct endpoint.
- **Endpoint:** A host address plus port. A guest directly enters a connectable host address or hostname.
- **Listening mode:** `Listen on all` binds IPv4 wildcard `0.0.0.0` on the selected port. Explicit mode binds each selected eligible local IPv4 address on that port.
- **Individual listening choice:** One eligible assigned IPv4 address on a local interface. An interface with multiple eligible addresses has multiple choices.
- **Lobby:** The pre-match and between-match state showing admission, connection, ownership, roster, readiness, and host settings.
- **Ready:** A participant's confirmation of the current configuration. Clearing mutations invalidate every participant's readiness.
- **Session:** The period from confirmed host startup until intentional host End session, host-local supervised service failure, or application shutdown.
- **Host session clock:** The authoritative monotonic clock used for connection, disconnect, reservation, expiry, and same-instant ordering decisions.
- **Supported release/content:** The exact network release ID and exact canonical gameplay-content manifest required by the host.

All normative deadlines and precedence rules in this document are evaluated on the host session clock. Downstream issues own transport, clock synchronization presentation, and implementation mechanics; they may not change the user-visible boundaries.

## Directory and admission authority

The [host directory contract](network-host-directory.md) owns listing and browsing for both hosting modes, their consistent Password/No password metadata and common optional password procedure. NET-ADM-001–016 below own ongoing admission and spawn eligibility in both modes; phase and admission availability remain independent. These requirements replace earlier blanket exclusions of browsing, passwords, public IPv4 endpoints and join-in-progress in downstream documents. LAN is the supported player-hosted environment; other valid IPv4 connections are permitted without an Internet reachability or performance guarantee. Dedicated trusted server-identity verification remains mandatory, including No password. The invitation concept, option and wire envelope are retired under NET-PASS-018. Directory metadata and listing-owner authorization grant neither gameplay endpoint trust nor controller/operator authority.

## First-release scope matrix

| Dimension | Supported target | Explicitly unsupported |
|---|---|---|
| Platforms | Current player-hosted platforms under their contracts; dedicated Linux x86-64 and existing Linux ARM64 packaged service targets with Linux x86-64 and Windows x86-64 clients | ARM64 dedicated clients; macOS dedicated hosting or clients; other dedicated targets; blanket hardware or verified-platform claims |
| Cross-platform play | Approved platforms using the same supported release and gameplay content; platform-specific acceptance boundaries apply | Other targets; blanket claims of verified interoperability |
| Network environments | Same-machine and LAN support; other valid IPv4 connections permitted; encrypted dedicated pilot | Guaranteed Internet reachability or performance, NAT traversal, relays, public-service safety claims |
| Connection method | IPv4 wildcard or explicit listening addresses; direct join; central mixed player-hosted/dedicated browser; operator-endpoint dedicated direct or bound join | Automatic LAN discovery, matchmaking |
| Hosting | Player-hosted authoritative session; public dedicated pilot under NET-PUB | Other dedicated deployment and host migration |
| Identity and access | Session-local participant identity; common optional Password in both hosting modes; mandatory dedicated X.509 endpoint verification | Accounts, cloud identity, ranked identity; retired invitation admission |
| Lobby cardinality | 1–15 admitted participants and 1–15 roster players; a host-alone lobby is valid | Empty or over-capacity admitted lobby |
| Match start | 1–15 connected participants and 1–15 roster players; each participant owns at least one player; existing readiness and validity checks | Starting with a disconnected guest or an ownerless participant |
| Degraded match | One remaining roster player continues under hosted solo-round rules | Replacing an established outcome because of departure |
| Admission | Lobby, active rounds, and non-final round-end delays; immediate first-round spawn only strictly before its outcome, otherwise next eligible round | New admission during final round-end delay or final summary; spectators |
| Compatibility | Exact network release ID and canonical gameplay content | Cross-release or cross-content compatibility |
| Player visuals | Deterministic built-in default network visuals | Selected-profile appearance and remote profile appearance parity |
| Results | Session-only network results retained until a new match starts or the session ends | Local statistics or Elo writes |

Lobby invariants are `1 <= admitted participants <= roster players <= 15`. Match start invariants are `1 <= connected participants <= roster players <= 15`, with every participant owning at least one player and every participant connected, valid, and ready. A started match continues with one remaining roster player under NET-SOLO-001 through NET-SOLO-006; that reduction alone is not an interruption.

These hosted rules apply to player-hosted sessions and dedicated sessions after committed controller admission. Prepared dedicated idle remains zero-player/controllerless with no Start. Dedicated controller End, Leave and expiry override ordinary solo continuation. Local Play SET-006/SET-007 and its two-player minimum remain unchanged.

Same-machine support means separate running instances communicating through the production transport. It does not wrap or alter the local-only Play journey.

## Host listening address

- **NET-HOST-IF-001** `NET-02` must provide a control labeled `Listening interface` that opens a multiple-choice popup.
- **NET-HOST-IF-002** The popup must list IPv4 loopback and eligible assigned private or public unicast IPv4 addresses that are available on local interfaces.
- **NET-HOST-IF-003** Individual choices must not include wildcard, unspecified, multicast, link-local, unassigned, network, or broadcast addresses.
- **NET-HOST-IF-004** Each opening of the Network journey from Local Play must enable `Listen on all` by default.
- **NET-HOST-IF-005** The application must retain the listening mode and explicit selected set with the other host setup during the Network journey.
- **NET-HOST-IF-006** Explicit mode must use the selected set without substituting another address or falling back to wildcard mode.
- **NET-HOST-IF-007** Before explicit startup, the application must validate every selected address against current local interface information.
- **NET-HOST-IF-008** If an explicit selected address is no longer eligible, the application must keep `NET-02` editable and require the host to correct the selection.
- **NET-HOST-IF-009** Selecting a listening address must not change a host interface, route, firewall, Docker network, port forwarding, or other network infrastructure.
- **NET-HOST-IF-010** The Port field must keep initial focus in editable `NET-02`.
- **NET-HOST-IF-011** `Listening interface` must follow Port in the keyboard and controller focus order.
- **NET-HOST-IF-012** Local interface enumeration must not discover or list another host or session.

- **NET-HOST-IF-013** The popup must provide a checkbox labeled `Listen on all` and a checkbox for each individual choice.
- **NET-HOST-IF-014** While `Listen on all` is enabled, individual choices must appear checked and disabled.
- **NET-HOST-IF-015** Wildcard startup must delegate interface coverage to the operating system and must not depend on successful individual-address enumeration.
- **NET-HOST-IF-016** The first switch to explicit mode in a Network journey must select every currently eligible individual choice.
- **NET-HOST-IF-017** Explicit mode must let the host independently select or clear each individual choice.
- **NET-HOST-IF-018** An empty explicit selected set must block Start session in editable `NET-02`.
- **NET-HOST-IF-019** Returning from wildcard mode to explicit mode must restore the explicit selected set after its first initialization.
- **NET-HOST-IF-020** After explicit-set initialization, a newly available address must remain unselected until the host selects it.
- **NET-HOST-IF-021** An unavailable selected address must remain identifiable as an invalid selection until the host clears it or it becomes eligible again.
- **NET-HOST-IF-022** Reopening the Network journey from Local Play must discard the prior listening mode and explicit selected set.
- **NET-HOST-IF-023** Changing a popup checkbox must update retained setup without starting a service.
- **NET-HOST-IF-024** Closing the popup must retain checkbox changes.

Wildcard mode covers current and later IPv4 interfaces according to operating-system behavior. Individual checkmarks in wildcard mode show coverage, not an explicit bind set. Enumeration failure must not disable `Listen on all`. Explicit mode has no automatic expansion after initialization. The selected port applies to every selected address. [HSL-IF-004](network-host-service-lifecycle.md) owns all-or-nothing startup. [NET-DIR-017 through NET-DIR-020](network-host-directory.md) own the advertised connectable endpoint. Neither mode guarantees Internet reachability or security.

## Ownership and configuration

- The host starts and owns the player-hosted authoritative session and direct endpoint.
- The host configures mode, level or rotation, rounds, other approved match settings, and authoritative roster order.
- Each participant configures only its own local persons and controls and must own at least one roster player while admitted.
- The lobby labels each participant's role, connection state, readiness, owned players, and authoritative roster positions separately.
- Host-owned settings are read-only for guests. Participant-owned player controls are editable only by that participant.
- Every gameplay action and state transition is validated and applied by the authoritative host simulation.

- **NET-OWN-001** Each participant must select its local player count before host startup or guest connection begins.
- **NET-OWN-002** Admission must commit the participant's exact ordered player identities and ownership.
- **NET-OWN-003** The set of player identities owned by an admitted participant must remain unchanged while that participant remains admitted.
- **NET-OWN-004** An admitted participant must not add, remove, or transfer an individual player slot in `NET-04`.
- **NET-OWN-005** An admitted participant may change the person or local control assigned to one of its existing player slots in `NET-04`.
- **NET-OWN-006** A change under NET-OWN-005 must not change that slot's player identity or owner.
- **NET-OWN-007** A host roster-order change must not change a player identity or owner.
- **NET-OWN-008** Intentional participant Leave or authoritative reservation expiry must remove all player slots owned by that participant.
- **NET-OWN-009** A removed player identity must not be reused during the session.

### Team setting preferences

- **NET-SET-001** The network lobby must preserve the host's Team setting preferences across mode changes according to `SET-042` and `SET-043` in [`features.md`](features.md).
- **NET-SET-002** When Deathmatch or Predator is selected, the authoritative match settings must apply no teams and no Friendly Fire.
- **NET-SET-003** When the host selects Team deathmatch again, the authoritative match settings must apply the retained Team setting preferences.

Preference retention does not change host-only setting permissions or readiness invalidation.

## Lobby, readiness, and admission

- A host-alone lobby with one or more host-owned players is valid and may Start when the existing validity and readiness checks are satisfied; no solo confirmation is required.
- Every connected participant, including the host, must be ready before Start is enabled.
- In the lobby, admitting, intentionally removing, or expiring a participant clears every participant's readiness.
- Changing a person, control, host match setting, or roster order in `NET-04` clears every participant's readiness.
- A participant must add or remove local player slots before host startup or guest connection begins.
- An admitted guest declared disconnected remains admitted as `Reconnecting`. Its prior Ready value is retained, but Start is blocked with `Waiting for <participant> to reconnect`.
- A successful reconnect restores the retained Ready value unless another readiness-clearing mutation occurred after disconnect.
- Reconnect expiry or intentional Leave removes the participant and players and clears every remaining participant's readiness.
- New admission must follow NET-ADM-001 through NET-ADM-016.

### Ongoing admission and spawn eligibility

Match phase and admission availability are independent. A started match may remain Open; a final outcome closes admission before its final delay and summary.

- **NET-ADM-001** The host must permit new admission in the lobby, all active rounds, and non-final round-end delays, subject to existing password, compatibility, capacity, and ownership checks.
- **NET-ADM-002** The host must close new admission when it establishes the final-round winner or no-winner outcome, before the final round-end delay.
- **NET-ADM-003** An admission must commit strictly before the final outcome to succeed. At an equal host-clock instant, an outcome or round start takes precedence over admission. At the first non-final outcome, admission remains available but immediate first-round spawn is no longer eligible.
- **NET-ADM-004** The host must keep admission closed through the final round-end delay and final summary, including a one-round match. Later active rounds and non-final delays remain open.
- **NET-ADM-005** Returning to the lobby, including after an approved interruption, must reopen admission under the existing lobby checks.
- **NET-ADM-006** An ongoing admission must append all selected players as one admission without exceeding 15 participants or 15 roster players. It must preserve existing player identities, ownership, roster order, teams, and state.
- **NET-ADM-007** Admission committed strictly before the first-round outcome must spawn each new player immediately with normal initial life, air, random enabled weapon, ammo, protection, and spawn indicators. Other ongoing admissions must use that normal initialization exactly once at their eligible round start under NET-ADM-013. Placement must use the applicable arena and Team starting area; it does not guarantee safety from hazards.
- **NET-ADM-008** Immediate first-round arrival must keep the existing Predator; each arriving Predator-mode player enters as a non-predator with normal non-predator initial ammo. New Team players use appended roster positions without changing existing teams. At an eligible new-round start, the normal mode initialization applies to the participating roster.
- **NET-ADM-009** Admission must not restart the round, reset other players, rewind world state, or restart water or sudden-death timers. Newly spawned players participate in combat and winner evaluation under NET-SOLO-001 through NET-SOLO-006. An established sudden-death state remains active; waiting players must not affect it.
- **NET-ADM-010** New players receive statistics only for rounds they actually participate in. A completed round counts as played only after that player spawns in it; survival time begins at spawn. Waiting accrues no statistics or results. Never-spawned players must have no match-participation result rows. Results retain stable identities and actual round membership.
- **NET-ADM-011** A new guest must receive confirmed ownership, valid clock calibration, and a complete current authoritative snapshot within the existing connection deadline. Admission during gameplay enters `NET-05`, either immediately participating or `NET-05-waiting-for-next-round`, not a false lobby or spectator state. Lobby admission enters `NET-04`. No second match-start readiness vote may pause or restart gameplay.
- **NET-ADM-012** Reconnect preserves current reserved state and spawn eligibility; it must not spawn a waiting player early or duplicate a spawn. New admission must not grant control of reserved or existing identities. This release does not add accounts or cross-connection person authentication.
- **NET-ADM-013** An admission committed at or after the first-round outcome waits for the first round start strictly after its admission. Admission during a later active round or non-final delay uses the same rule. A round start at the same instant is not eligible.
- **NET-ADM-014** Waiting players retain admitted identity, ownership, and capacity but have no active body, gameplay input, damage, pickups, winner eligibility, survival time, or results in the current round. Replication must distinguish Waiting from living, dead, and removed players.
- **NET-ADM-015** At the eligible round start, each nonremoved waiting player must spawn exactly once through normal round initialization with all pending and retained input cleared. A reserved disconnected player spawns under existing no-input reservation rules. Leave or expiry cancels pending spawn; reconnect cannot bypass eligibility.
- **NET-ADM-016** Admission during a final active round after the immediate first-round window may succeed as waiting, but must not create an extra round. Final outcome closes further admission. Never-spawned players preserve their admitted ownership into the following lobby without match-participation results.

### Hosted solo-round rules

- **NET-SOLO-001** Hosted Start accepts 1–15 connected participants and roster players under the existing ownership, validity, and readiness checks. Local Play's minimum-player rule is unchanged.
- **NET-SOLO-002** Opposition uses spawned, nonremoved current-round membership, including dead players and disconnected reserved players. Deathmatch requires at least two such players; Predator requires the Predator and at least one non-predator; Team deathmatch requires at least two represented teams. Waiting and removed players are excluded.
- **NET-SOLO-003** Without opposition, the round stays playable and must not automatically award a victory to the surviving player or team. Normal simulation, hazards, timers, and input continue. When opposition exists, normal mode winner rules apply.
- **NET-SOLO-004** With no living current-round players, the service establishes a completed `No winner` round without victory, including when Leave or expiry removes the last living player. Waiting players do not prevent that outcome. The normal round-end delay and configured round limit apply.
- **NET-SOLO-005** Before an outcome is established, removal that eliminates opposition returns the current round to solo rules. Reducing the roster to one does not interrupt the match. An already established outcome is immutable.
- **NET-SOLO-006** Every new round reevaluates opposition from its spawned nonremoved membership; eligible waiting arrivals can establish opposition. Admission alone must not count as participation before spawn.

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

## Default network visuals

The **default network visual set** is the built-in player skin, animation mapping, entity-resource mapping, and background-selection behavior for first-release network play.

The **eligible background list** is the stable ordered list of locally available built-in backgrounds that can satisfy the existing fallback behavior in `LIF-009`.

- **NET-VIS-001** First-release network setup must let each participant select local persons and controls.
- **NET-VIS-002** First-release network setup must not offer profile selection for a network player.
- **NET-VIS-003** Each client must use the default network visual set for every player in a network match.
- **NET-VIS-004** A client must derive each player animation and entity visual from replicated canonical state and the default network visual set.
- **NET-VIS-005** A client must not run a second gameplay simulation to derive network presentation.
- **NET-VIS-006** The same supported release and replicated canonical state must select the same player animation and entity visual on each client.
- **NET-VIS-007** A local or remote profile must not change a network player's skin, animation, or visual resource.
- **NET-VIS-008** The default network visual set must apply authoritative Team colors, Predator opacity, invisibility, and other replicated visual gameplay states.
- **NET-VIS-009** A missing, changed, or additional profile or cosmetic asset must not block admission.
- **NET-VIS-010** A client must not load a peer-selected profile, file, or script as a network-visual fallback.
- **NET-VIS-011** If a client cannot load a required default network visual resource, the client must not start network play and must retain the existing required-resource failure behavior.
- **NET-VIS-012** A client must use the level's named background when that background is locally usable.
- **NET-VIS-013** When the level has no locally usable named background, the client must select one background from the eligible background list by a deterministic pseudo-random mapping.
- **NET-VIS-014** The mapping in `NET-VIS-013` must use only the replicated session, match, round, and level logical identities and the eligible background list.
- **NET-VIS-015** Equal identities and equal eligible background lists must select the same fallback background on each client of the supported release.
- **NET-VIS-016** The mapping must give each background in an eligible background list a selection opportunity as the identity inputs change.
- **NET-VIS-017** The fallback selection must remain presentation-only and must not use the authoritative gameplay seed, change canonical state, require a replicated background field, or load peer content.

Selected-profile appearance parity between network participants is outside first-release scope.

## Timing, failures, and precedence

### Host startup

- Host startup has a 10-second deadline on the host session clock from Start session activation.
- Success requires confirmed listener/session readiness strictly before the deadline. At or after 10 seconds, startup fails and no listener may remain.
- Cancel before success returns to editable `NET-02`, retains endpoint and local-player setup, and leaves no listener or session.
- A specific confirmed startup failure is shown instead of a generic timeout when known before the deadline.
- Issue #31 defines exact supervision states, startup outcomes, Retry rules, cleanup, and fixed copy without changing these boundaries.

### Guest connection

Initial snapshot references mean the current authoritative lobby or match state under NET-ADM-011. Dedicated certificate verification, PROXYv2 and common inner password exchange share the same absolute connection deadline under NET-PUB-020; no stage restarts it.

- Hostname/address and port validation remains inline in editable `NET-03`; invalid input does not begin the connection clock.
- A connection attempt has one 10-second total deadline. It covers resolution, transport connection, compatibility, capacity, host admission, clock calibration, and current initial snapshot validation.
- Production success must include exact final confirmation, valid host-clock calibration, and one complete current lobby or match snapshot, including waiting ownership when applicable.
- The guest must receive all three success inputs strictly before the deadline. It may validate queued predeadline inputs after the deadline without starting or awaiting a new exchange.
- Missing calibration or another success input at the deadline must produce `Connection timed out.` unless a higher-precedence result was established first.
- A frame received at or after the deadline cannot replace timeout or a complete valid predeadline result.
- Cancel returns to editable `NET-03` and retains endpoint and local-player setup.
- `NET-08` Retry repeats the same retained attempt when still valid; Edit setup returns to editable `NET-02` or `NET-03` with all setup retained; Return to Network goes to `NET-01`.

### Initial admission precedence and fixed copy

User Cancel and local inline validation take precedence before any host or transport result. Once a valid request reaches the host, the host evaluates the first applicable admission result in this fixed order:

1. malformed request — `Connection request rejected.`;
2. trust or authorization rejection — `Connection not authorized.`;
3. release mismatch — `Network release mismatch. Use the same supported game release as the host.`;
4. invalid gameplay-content manifest — `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.`;
5. gameplay-content mismatch — `Gameplay content mismatch. Use the host's exact supported gameplay content.`;
6. new admission closed under NET-ADM-002 — use the fixed final-admission-closed reason owned by [compatibility and admission](network-compatibility-and-admission.md#initial-outcome-identifiers-and-copy), without a first-round cutoff claim;
7. capacity reached — `Session is full.`;
8. other host policy rejection — `Host rejected the connection.`;
9. success.

The host stops at the first applicable result. A complete valid rejection received before the deadline takes precedence over a later generic transport symptom.

A complete valid production success received before the deadline has the same precedence. Final confirmation alone is not complete production success.

Without a complete result, initial connection outcomes use this order:

1. name-resolution failure — `Host name could not be resolved.`;
2. unreachable or refused connection — `Host unreachable.`;
3. reset or closed transport before a complete admission response — `Connection ended before admission completed.`;
4. no complete result at the deadline — `Connection timed out.`

All copy is fixed and non-disclosing. It never interpolates a peer-supplied release ID, manifest path, credential, policy value, payload, or other untrusted value.

### Reconnect outcome precedence

Player-hosted sessions have no guest-observable unexpected host-termination signal. Dedicated terminal notices follow HSL-PUB. In either mode, loss of contact, silence, connection refusal, unreachable host, reset, timeout, host crash, host-machine loss, listener loss, temporary transport failure, or no response is ambiguous. None proves session end. Every such outcome keeps the participant in `NET-07` and permits retries against the original fixed 30-second deadline.

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
4. Confirmed admission → current authoritative `NET-04` or `NET-05` under NET-ADM-011; failure → `NET-08`; Retry repeats, Edit setup returns to `NET-03`, Return to Network → `NET-01`.

### Lobby, match, summary, and return

1. Valid admitted participants may change the person or control for an existing owned player slot in `NET-04`. They must not add, remove, or transfer an individual player slot. Every permitted configuration change clears readiness.
2. Start is enabled only when all match-start cardinality, ownership, connection, compatibility, and readiness requirements hold.
3. Start enters authoritative shared-arena `NET-05`; NET-ADM-001 through NET-ADM-005 control admission.
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
- During an active round, simulation, timers, hazards, connected inputs, combat, scoring, solo/winner rules, and round progression continue. Spawned reserved players receive no input, remain valid targets, and count as current-round membership. Reserved waiting players remain waiting until their eligible start and have no body or current-round winner role.
- Normal damage, death, score, and winner rules apply during reservation.
- Retryable resolution, refusal, unreachable, reset, temporary transport, and no-response outcomes remain in `NET-07` for the full remaining reservation. Terminal outcomes follow the fixed precedence above.

## Lifecycle-specific atomic removal

At each host session clock instant, applicable confirmed intentional leaves and authoritative reservation expiries form one atomic removal batch. Removal itself adds no kill, death, assist, penalty, or other combat statistic. Intentional host End session takes precedence over every removal batch and discards session results. A host-local supervised service failure also ends host processing and discards the host's session result without creating a guest-observable host-end signal.

- **Lobby:** Remove the batch, label departed rows in any retained result, and clear every remaining participant's readiness. Do not evaluate a winner. The retained completed result remains until a new match starts or the session ends.
- **Active round:** Remove the batch, then perform exactly one evaluation under NET-SOLO-002 through NET-SOLO-005 if no outcome exists. One remaining roster player continues under solo rules; waiting players do not supply opposition. An established outcome remains unchanged.
- **Non-final round summary:** Preserve the completed round outcome. After the batch, continue to the next round with one or more roster players and reevaluate opposition at its start. A reduction to one does not produce an interrupted result.
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
- **NET-RES-001** Each retained round outcome and match outcome must preserve its complete winner identities after participant or player removal.
- **NET-RES-002** Participants must be able to read every winner's complete display name and identity for each available outcome in `NET-06` and the retained `NET-04` result, including supported maximum-length names and multiwinner outcomes.
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

- Guaranteed Internet play, NAT traversal, relays, firewall automation, automatic LAN discovery, or matchmaking.
- Accounts, cloud identity, ranked networking, network Elo, or persistent network statistics.
- Dedicated-server operation outside the approved pilot, host migration, new admission after the final-round outcome, or spectators.
- A guest-observable unexpected player-host termination signal. Dedicated terminal notices follow HSL-PUB; transport loss alone remains ambiguous in both modes.
- Cross-release or cross-content compatibility.
- Compatibility checks for presentation-only assets, local persistence, local controls, or documentation.
- Selected-profile appearance in a network match or remote profile appearance parity.
- Changes to existing local-only Play behavior.

## Acceptance criteria

- **NET-SET-AC-001 — Mode changes:** Switching from Team deathmatch with Friendly Fire on to Deathmatch and then Predator must apply valid non-team settings on host and guests. A subsequent setting edit must apply normally. Each configuration change must clear readiness. Returning to Team deathmatch must restore both Team preferences.
- **NET-RES-AC-001 — Complete retained winners:** After Leave or reservation expiry during final summary, each completed outcome must preserve its complete winner identities. Affected result rows must show `Departed`. Returning directly to the lobby must exclude removed participants and their players from membership and readiness. Participants must be able to read every winner's complete display name and identity in the summary and retained lobby result, including multiwinner outcomes with supported maximum-length names.
- **NET-AC-001 — Platform:** Linux x86-64, Windows x86-64, Raspberry Pi 5 Linux 64-bit under [its contract](raspberry-pi5.md), and Apple Silicon macOS 14+ under [its experimental contract](macos.md) are approved participation targets using the same supported release and gameplay content. This is not a blanket verified-platform claim. Native Mac automated acceptance is required after integration in `Develop - Sanity` and gates `sanity` and nightly publication, not pre-review or merge. Actual Mac GUI and live cross-OS gameplay are deferred to the user after merge/nightly under MAC-NET-AC-006. Dedicated participation remains limited to NET-PUB-AC-006.
- **NET-AC-002 — Endpoints:** A host can use IPv4 wildcard mode or an explicit selected set of eligible assigned loopback, private, or public unicast IPv4 addresses. Direct and browser joining use concrete connectable endpoints and preserve LAN support without promising Internet reachability. Concrete-address exclusions and no network-infrastructure automation remain enforced.
- **NET-AC-003 — Host model:** A player-hosted session remains authoritative and has no host migration. The separate dedicated-pilot controller and service model must follow NET-PUB and HSL-PUB.
- **NET-AC-004 — Lifecycle cardinality:** A lobby and hosted Start accept 1–15 participants and roster players, with all participants connected, valid, ready, and owning at least one player at Start. One remaining roster player continues under solo rules without interruption. Local Play is unchanged.
- **NET-AC-005 — Ownership:** The host controls match settings and roster order. Each participant controls only the persons and controls assigned to its immutable admitted player slots. A person, control, or roster-order change does not change player identity or ownership. Authoritative input and state ownership are enforced.
- **NET-AC-006 — Readiness:** Every participant must be connected, valid, and ready for initial Start, including solo Start without a new confirmation. Lobby admission/removal and permitted configuration changes clear all readiness. A disconnected guest retains prior readiness as `Reconnecting` but blocks Start by name. Reconnect restores it only absent later clearing mutations. Ongoing admission needs no second match-start vote.
- **NET-AC-007 — Admission:** Lobby, active-round, and non-final-delay admission succeeds when eligible. Immediate spawn is restricted to commit strictly before the first-round outcome; later admission waits for a strictly later round start. Final outcome closes admission through final delay and summary; lobby return reopens it.
- **NET-ADM-AC-001 — Live and waiting arrival:** Admission committed strictly before the first-round outcome immediately adds exact selected players with normal initial state and usable owned controls on every client, without resetting world, mode, existing players, or timers. At/after that outcome and in later rounds/non-final delays, eligible admission confirms waiting ownership and spawns only at the first strictly later round start. Capacity remains bounded.
- **NET-ADM-AC-002 — Modes and results:** Immediate arrival preserves Predator identity and existing Team assignments. Waiting players have no body, input, combat, pickup, winner, survival, or result effects; eligible spawn uses normal mode initialization exactly once. Statistics count actual participation only, and never-spawned players have no match-participation result rows.
- **NET-ADM-AC-003 — Boundary and recovery:** Evidence covers first outcome and round-start equality precedence, later active and non-final-delay admission, final active waiting without an extra round, final-outcome rejection, lobby ownership retention, reserved eligible spawn without input, Leave/expiry cancellation, and reconnect without early or duplicate spawn or identity takeover.
- **NET-SOLO-AC-001 — Start and opposition:** Solo hosted Start is available with valid ready setup and no extra confirmation. Mode-specific opposition includes dead/reserved spawned membership but excludes waiting/removed players. Normal winner evaluation applies when opposition exists; Local Play remains unchanged.
- **NET-SOLO-AC-002 — Solo lifecycle:** No-opposition play does not auto-win. No living current-round players completes a no-winner round without victory, including last-living death, Leave, or expiry; waiting does not prevent completion. Normal delay and limit apply. Departures to one continue; each new round reevaluates opposition.
- **NET-SOLO-AC-003 — Outcome integrity:** Pre-outcome removal can restore solo rules, while established outcomes remain immutable. Waiting admission cannot establish opposition or fabricate participation/results.
- **NET-AC-008 — Compatibility:** Admission requires an exact case-sensitive non-empty network release ID and exact gameplay-content manifest whose logical paths satisfy every ASCII length, segment, character, separator, uniqueness, and unsigned-order rule; fixed user copy discloses no peer release ID, path, value, or raw payload, and diagnostics name only independently validated canonical paths.
- **NET-AC-009 — Timing, admission, and host-local failure:** Host startup and complete initial guest connection must satisfy their 10-second boundaries. Production connection success must include predeadline final confirmation, valid clock calibration, and a complete valid initial snapshot. Missing calibration must produce timeout. Late initial-connection frames must not replace timeout or predeadline success. User Cancel and local validation must keep their defined precedence. The host application's local supervisor alone may route the host to `NET-08` with `Hosted session stopped unexpectedly.` This outcome is never guest evidence. Retained data, fixed copy, Retry, Edit setup, and Return destinations must match this specification.
- **NET-AC-010 — Authority:** Participants control only owned local players while the host owns canonical simulation, rounds, scoring, winner evaluation, and current state in one shared arena.
- **NET-AC-011 — Reconnect:** A reservation begins at host-declared disconnect, expires at `D + 30s`, accepts only strictly-before-deadline restoration, shows positive ceiling seconds without active zero, retains one deadline across attempts, and restores only current state; silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, and no response remain ambiguous in `NET-07`; only a valid host End notice accepted through the established session, accepted restore, terminal rejection, retryable failure, and expiry follow the fixed precedence.
- **NET-AC-012 — Active disconnect:** Simulation and progression continue. Spawned reserved players receive no input; living reserved players remain valid targets. Dead and living spawned membership counts for opposition until removal. Waiting reserved players have no current body or winner role and spawn only at their eligible start under no-input rules.
- **NET-AC-013 — Lifecycle-specific server-side removal:** Same-clock leaves and expiries are one batch without combat statistics. Lobby removal clears readiness without winner evaluation. Active pre-outcome removal evaluates once under solo/opposition rules; one remaining roster player continues. Established outcomes, including non-final-delay and final-summary outcomes, remain unchanged. Waiting removal cancels pending spawn. An isolated guest deadline does not claim server-side removal.
- **NET-AC-014 — Intentional host end:** Only a valid intentional host End session notice accepted through the current established session returns the host to `NET-01` and sends guests to host-ended `NET-09`; first release has no guest-observable unexpected termination signal, and host crash, machine/listener loss, silence, reset, refusal, or timeout remains `NET-07` until terminal rejection or expiry.
- **NET-AC-015 — Local independence:** `Play (F1)` starts and completes unchanged without starting or requiring any network service, while `Network (F2)` remains separate.
- **NET-AC-016 — Cancellation and leave:** Host startup Cancel, guest connection Cancel, guest lobby/match/summary Leave, reconnect Leave session, host End session, and their confirmations retain or discard data and reach exactly the specified destinations.
- **NET-AC-017 — Truthful and non-disclosing UX:** Role, Connected/Reconnecting, readiness, pending, retryable ambiguity, terminal rejection, disabled Retry, expiry, host-local service failure, consequence, and host-ended states use fixed visible copy, disclose no untrusted peer value, and never claim guest-observed host end, player removal, or unexpected termination from isolation alone.
- **NET-AC-018 — Session-only results:** Result state, match outcome, last completed-round outcome, and cumulative rankings are distinct. Completed match outcome equals the final round outcome. Interrupted match outcome is `No winner`. Cumulative rankings include only completed rounds and define no champion. Completed results appear in `NET-06`. Completed and interrupted results remain through the following lobby with departed labels. A new match clears the retained result. Intentional host end, host-local service failure, or application shutdown discards it. An isolated guest expiry does not claim a server-side result transition. Results never persist locally or to Elo.
- **NET-AC-019 — Explicit boundaries:** UI, packaging, and release claims omit every non-goal. Gameplay compatibility excludes presentation, cosmetic, profile, local persistence, local control, and documentation material. First-release network play does not claim selected-profile appearance parity.
- **NET-VIS-AC-001 — Setup:** Network setup provides person and control selection without profile selection.
- **NET-VIS-AC-002 — Deterministic presentation:** Each client presents the same replicated canonical state with the same default player skin, player animation, and entity visual selection without a second gameplay simulation.
- **NET-VIS-AC-003 — Gameplay states:** Default network visuals preserve each authoritative Team color, Predator opacity, invisibility state, and other replicated visual gameplay state.
- **NET-VIS-AC-004 — Profile exclusion:** Local and remote profiles do not change network visuals, do not affect admission, and do not provide a presentation fallback.
- **NET-VIS-AC-005 — Required-resource failure:** A client with an unavailable required default network visual resource does not start network play and does not load peer content as a fallback.
- **NET-VIS-AC-006 — Named background precedence:** Every client uses a locally usable named level background before it considers fallback selection.
- **NET-VIS-AC-007 — Deterministic background fallback:** For a level without a locally usable named background, two clients of the supported release select the same fallback when their replicated session, match, round, and level logical identities and their eligible background lists are equal. A full snapshot, equivalent incremental state, reconnect, and resynchronization produce that same selection. Controlled varied identity inputs demonstrate that every item in an eligible background list can be selected.
- **NET-VIS-AC-008 — Presentation-only background:** Fallback background selection uses no authoritative gameplay seed, canonical background field, second gameplay simulation, profile fallback, or peer content. It does not change canonical state or match outcomes.
- **NET-OWN-AC-001 — Pre-admission player count:** A host or guest can add or remove local player slots only before its admission process begins.
- **NET-OWN-AC-002 — Immutable admitted ownership:** Admission fixes each participant's exact player identities and ownership until that participant leaves, expires, or the session ends.
- **NET-OWN-AC-003 — Lobby edits:** In `NET-04`, a participant can change the person or control for an existing owned slot. The change clears all readiness and preserves the slot's identity and owner.
- **NET-OWN-AC-004 — Roster order:** A host roster-order change clears all readiness and preserves every player identity and owner.
- **NET-OWN-AC-005 — Removal:** Participant Leave or expiry removes all of that participant's slots and permanently revokes their identities for the session.
- **NET-HOST-IF-AC-001 — Default wildcard:** Reopening the Network journey enables `Listen on all` and shows individual choices checked and disabled. Successful wildcard bind permits startup even when individual enumeration fails.
- **NET-HOST-IF-AC-002 — Explicit subset:** First disabling `Listen on all` selects all currently eligible choices. The host can remove choices and start one session on every remaining selected address without accepting connections on unselected addresses.
- **NET-HOST-IF-AC-003 — Address exclusion:** Individual choices omit wildcard, unspecified, multicast, link-local, unassigned, network, and broadcast addresses. `0.0.0.0` is permitted only as the wildcard listening mode, not as a concrete guest or directory endpoint.
- **NET-HOST-IF-AC-004 — Empty and stale selection:** Empty explicit selection blocks Start. An unavailable selected address remains identifiable and blocks explicit startup until corrected. A newly available address remains unchecked after explicit-set initialization. Wildcard startup does not use these enumeration checks.
- **NET-HOST-IF-AC-005 — Focus and retention:** Port has initial focus. `Listening interface` follows Port in focus order. Popup closure, Cancel, Edit setup, and eligible Retry retain the mode and explicit set. Switching modes restores the initialized explicit set. Reopening the Network journey resets it.
- **NET-HOST-IF-AC-006 — Scope:** Address selection does not reconfigure host or Docker networking and does not provide discovery, public Internet, NAT traversal, or port forwarding.
- **NET-HOST-IF-AC-007 — Atomic startup:** A failure on any explicit selected bind prevents readiness and leaves no session or listener after cleanup. The host sees the applicable existing startup error and can edit retained setup.

## Exact downstream issue mapping

Each issue owns the listed criteria without changing their normative boundaries. Criteria are enumerated rather than represented by overlapping ranges.

| Issue | Responsibility | NET-AC criteria |
|---|---|---|
| [#29](https://github.com/mkapusnik-apps/duel6r/issues/29) | Session transport and connection lifecycle | `NET-AC-002`, `NET-AC-004`, `NET-AC-007`, `NET-AC-009`, `NET-AC-011`, `NET-AC-016` |
| [#30](https://github.com/mkapusnik-apps/duel6r/issues/30) | Protocol, release, capability, and content compatibility | `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-019` |
| [#31](https://github.com/mkapusnik-apps/duel6r/issues/31) | [Player-hosted service supervision and host-local failure](network-host-service-lifecycle.md) | `NET-AC-003`, `NET-AC-009`, `NET-AC-014`, `NET-AC-016` |
| [#32](https://github.com/mkapusnik-apps/duel6r/issues/32) | Authoritative headless match simulation | `NET-AC-004`, `NET-AC-010`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#33](https://github.com/mkapusnik-apps/duel6r/issues/33) | Network input client behavior and authoritative input processing | `NET-AC-005`, `NET-AC-010`, `NET-AC-012` |
| [#34](https://github.com/mkapusnik-apps/duel6r/issues/34) | Canonical state replication and identities | `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-018` |
| [#35](https://github.com/mkapusnik-apps/duel6r/issues/35) | [Responsiveness and recovery budgets](network-responsiveness-and-recovery.md) | `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012` |
| [#36](https://github.com/mkapusnik-apps/duel6r/issues/36) | Disconnect, reconnect, shutdown, and intentional host end | `NET-AC-004`, `NET-AC-006`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016` |
| [#37](https://github.com/mkapusnik-apps/duel6r/issues/37) | Network result and persistence enforcement | `NET-AC-013`, `NET-AC-014`, `NET-AC-018` |
| [#38](https://github.com/mkapusnik-apps/duel6r/issues/38) | Graphical network-session composition and host, join, lobby, status, recovery, and error UX | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |
| [#39](https://github.com/mkapusnik-apps/duel6r/issues/39) | Trust boundaries and abuse limits | `NET-AC-002`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-010`, `NET-AC-011`, `NET-AC-019` |
| [#40](https://github.com/mkapusnik-apps/duel6r/issues/40) | Supported network packaging and deployment documentation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-008`, `NET-AC-015`, `NET-AC-019` |
| [#41](https://github.com/mkapusnik-apps/duel6r/issues/41) | Complete release-candidate validation | `NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-006`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-018`, `NET-AC-019` |

The graphical host journey consumes `NET-VIS-001` through `NET-VIS-017`, `NET-VIS-AC-001` through `NET-VIS-AC-008`, `NET-OWN-001` through `NET-OWN-009`, `NET-OWN-AC-001` through `NET-OWN-AC-005`, `NET-HOST-IF-001` through `NET-HOST-IF-024`, and `NET-HOST-IF-AC-001` through `NET-HOST-IF-AC-007`. Issue #38 owns its existing graphical journey. Issue #41 owns complete network-release validation.

Issue #28 approves this target but does not satisfy parent issue #27's implementation or release evidence. In-process loopback, documentation, or planned screenshots are insufficient to claim playable networking.

## Evidence expectations

- Product review traces each downstream issue to the exact criteria above and confirms non-goals remain excluded.
- UX review traces `MENU-01`, `MENU-02`, `CONS-01`, and `NET-01`–`NET-09` to applicable criteria and assesses one representative wireframe per affected screen.
- Issue #38 must supply one implementation screenshot for each of the 11 planned entries in [`docs/screenshots/README.md`](screenshots/README.md): `SS-002`, `SS-013`, and `SS-015`–`SS-023`. These entries remain planned, and no current screenshot is valid for the changed target UI.
- Issue #38 evidence must show default network visuals without profile selection or selected-profile appearance parity.
- Issue #38 evidence must show named-background precedence and deterministic fallback convergence for equal eligible background lists.
- Reviewer evidence checks lifecycle cardinality, initial admission order, full-deadline ambiguity for every unexpected host failure, intentional-end-only `NET-09`, host-local-only supervision, reconnect precedence, lifecycle-specific removal, exact compatibility fixtures/copy, destinations, and local-only preservation.
- Tester evidence independently verifies at downstream implementation SHAs that every guest-observed host crash, machine/listener loss, silence, reset, refusal, timeout, and no-response case stays `NET-07` through the fixed deadline; that only an accepted intentional End notice enters guest `NET-09`; and that host-local supervision routes only the host to `NET-08`. Issue #28 itself is documentation-only and requires no automated test implementation.
- DevOps evidence confirms supported Linux and Windows x86-64 artifacts and hosted checks at the applicable release-candidate SHA.
- Issue #41 validates the complete production path before parent issue #27 or release text claims playable network support.
