# Network compatibility and admission

## Status and authority

The dedicated extension in [network-play-first-release.md](network-play-first-release.md) retains this document's compatibility checks, stable rejection outcomes, selected-session binding and atomic admission contract. Both hosting kinds use the common current player-hosted EC-JPAKE profile under [TRU-PUB](network-trust-and-abuse-limits.md) and NET-PASS-001–018 without mandatory gameplay certificate verification. No raw password or reusable proof enters an application admission frame. The pre-listener local-host admission sequence below applies only to player-hosted mode. Dedicated readiness follows HSL-PUB-002: a ready empty nonzero session is prepared before listing/first admission, without participants, controller or round. Exactly the first authorized compatible committed admission becomes controller in that same advertised session under NET-PUB-002 through NET-PUB-004/024–025; concurrent later commits are guests. No listing, client selection or provisional offer grants controller authority. Failed security or configured-password checks cannot allocate authority. Exact release/content checks remain required; the retired D6PI invitation envelope is neither password admission nor a fallback.

This document is the authoritative product target for GitHub issue #30. It defines first-release network compatibility and admission behavior.

The compatibility and admission contract is implemented in the explicit command-line networking scaffold. It does not provide a lobby, gameplay, graphical network UI, or playable networking. The current networking status remains documented in [`networking.md`](networking.md).

The approved network-play scope is in [`network-play-first-release.md`](network-play-first-release.md). The transport contract is in [`networking.md`](networking.md).

The trust and resource policy is in [`network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md). This document must not weaken those contracts.

Local Play remains subject to [`features.md`](features.md). This document does not change local Play behavior.

## Terms

- **Admission protocol version:** The version of the initial compatibility and admission contract.
- **Network release ID:** The exact build-compatibility value for network admission.
- **Capability:** A fixed identifier for one required part of the admission contract.
- **Gameplay-content manifest:** The canonical list of gameplay-content paths and their exact content identities.
- **Content identity:** A value that identifies the exact file contents for equality comparison.
- **Pending connection:** A transport connection that has not completed admission.
- **Complete valid production admission result:** One valid rejection, or the complete set of validated success inputs required before the production guest connects.

The admission protocol version is separate from transport framing version `1`. Equal numeric values do not make these versions the same field.

Admission success has no `D6RS` result encoding. `D6RS` carries only rejection codes `malformed-request` through `host-policy-rejected`, with zero participant identity and no player identities. Serializing or parsing `AdmissionResultCode::Admitted` as `D6RS` is invalid. The only network success stages are the provisional `D6RO` offer and the final `D6RC` confirmation; `D6RK` is the guest's exact acceptance echo.

## Admission version and release contract

The current admission schema includes explicit hosting kind in the encrypted identity transaction. It must use a strict admission-version advance from version `2`; version `2` and earlier schemas are not compatible with this transaction. Developer owns the exact version constant and encoding and must align the published protocol reference with the implementation. This does not change transport framing version `1` unless its own format changes.

| Field | Required value policy |
|---|---|
| Admission protocol version | One exact current version that includes the mandatory hosting-kind transaction; no version range or missing-field default |
| Network release ID | One exact non-empty value shared by the coordinated matching client/service package set |

The network release ID is the only build-compatibility value. The admission request must not use a separate build-version compatibility field.

Developer owns the necessary release-ID transition and must record its exact value in the distribution/protocol reference. A version or release change must not enable cross-release admission, a legacy schema adapter, or a second secure-channel profile.

All supported client and service artifacts in a matching package set must use the same network release ID. The value must be non-empty.

Network release ID equality must be exact and case-sensitive. Whitespace is content and must not be trimmed or normalized.

First release supports no protocol range, release range, or cross-release compatibility.

### Common secure-channel compatibility

- **CMP-CHANNEL-001** Clients and services must use only the configured common EC-JPAKE profile for either hosting kind.
- **CMP-CHANNEL-002** A failed or incompatible channel must close without probing or retrying the legacy dedicated outer-TLS profile, plaintext, or an unlocked password value.
- **CMP-CHANNEL-003** A channel mismatch must not allocate participant authority or report successful admission.
- **CMP-CHANNEL-004** Published package and endpoint instructions must identify the matching client/service channel expectation before users replace packages or change the listening endpoint.
- **CMP-CHANNEL-005** A client must not claim a confirmed release or admission-version mismatch unless it received and validated that result through the established secure channel.

The former dedicated outer-certificate handshake is not a supported alternate connection profile. Its mismatch can occur before admission, so an admission version or release-ID check alone cannot establish secure-channel compatibility. Channel separation and any required release transition must preserve exact equality and fail-closed behavior; no compatibility adapter or automatic fallback is required. Hosting kind must not select a weaker or legacy profile. An unauthenticated discriminator may reject incompatible input but must not establish server trust or authorize another profile.

**CMP-CHANNEL-AC-001:** Matching current clients and services establish the common encrypted channel in both hosting kinds. Tests with common clients against legacy dedicated endpoints and legacy dedicated clients against common endpoints fail closed before authority allocation, without a second-profile or unlocked retry. Pre-admission failure uses the applicable fixed security, transport-close, or timeout outcome, not an invented confirmed release mismatch. Package/endpoint guidance identifies coordinated replacement without claiming old-profile interoperability. Evidence includes immutable client/service identities, observed channel behavior, and reviewer assessment. This criterion covers CMP-CHANNEL-001–005; developer owns the necessary wire/version mechanisms.

### Hosting kind and lifecycle binding

- **CMP-HOSTING-001** The service must provide exactly one bounded hosting kind, `player-hosted` or `dedicated`, from its own configuration through the encrypted admission transaction.
- **CMP-HOSTING-002** The offer, exact guest acceptance, and final confirmation must agree on hosting kind as well as the existing offered identities and session binding.
- **CMP-HOSTING-003** Missing, unknown, trailing, or inconsistent hosting-kind data must reject admission without a default or committed participant authority.
- **CMP-HOSTING-004** A browser-selected attempt must verify its immutable expected hosting kind, endpoint, and session identity before provisional reservation or authority allocation.
- **CMP-HOSTING-005** An unbound Direct client must publish hosting-kind lifecycle behavior only after complete validated admission, including the existing final confirmation, clock calibration, and current snapshot.
- **CMP-HOSTING-006** An admitted client must retain the confirmed hosting kind with its session and scoped reconnect identity and reject a changed kind during restoration.

Direct setup has no hosting-kind expectation and must not infer kind from an address, port, first-controller status, or failed handshake. An offer may supply provisional kind for validation and exact acceptance, but it must not activate session/controller behavior. Hosting kind alone grants no authority; the service assigns controller or guest permissions only at admission commit under NET-PUB. A reconnect grant, when issued, must remain consistent with the confirmed kind and session; it is not a new initial admission or a new fourth success input. These checks bind lifecycle semantics, not cryptographically authenticated server identity. NET-PASS-010's unlocked first-contact limitation remains applicable.

**CMP-HOSTING-AC-001:** Direct joins to both hosting kinds, including first and later dedicated admissions, publish the correct lifecycle and committed role only after complete admission. Offers, client kind claims, rejected attempts, and cancelled attempts grant no authority. Missing/unknown/inconsistent kind and old admission schemas fail closed without a default. Browser hosting-kind, endpoint, and session mismatch rejects before provisional authority allocation; exact offer/acceptance/confirmation equality is retained. Reconnect preserves kind and scoped identity without role migration. Existing controller End/Leave/expiry and player-hosted ownership outcomes remain distinct. Relevant transaction/runtime behavioral evidence and source review establish these outcomes within the existing single QA campaign. This criterion covers CMP-HOSTING-001–006.

## Required capabilities

The first-release required capability set contains exactly these case-sensitive identifiers:

- `d6r.compatibility-admission.v1`
- `d6r.gameplay-manifest.v1`
- `d6r.session-identity.v1`

The host must require all three capabilities. Each guest must advertise support for all three capabilities.

An additional guest capability must not cause rejection. It must not grant authority or enable different session behavior.

A missing required capability must stop admission. The product must not select degraded behavior.

## Compatibility claim and ownership

Each participant compatibility claim must contain:

- the admission protocol version;
- the network release ID;
- the supported capability identifiers;
- the canonical gameplay-content manifest.

The executable owns the protocol version and supported capability set. The supported release artifact owns the network release ID.

Selected-session binding uses an expected session identity inside the encrypted admission request. Explicit Direct joining uses zero; browser-selected
joining, including dedicated selection, uses the advertised nonzero identity. This value travels inside the secure
channel. Hosting kind and gameplay endpoint are also bound to selected setup. A mismatch, ended identity or restarted/fresh replacement is an authorization rejection before any reservation,
authority allocation or commit. The offer must remain associated with the selected session; final confirmation, initial snapshot and reconnect grant must identify that same session.
Session-ID equality is not cryptographic host authentication for unlocked first contact in either hosting kind.

The prepared dedicated identity is unchanged by first admission, idle-to-lobby/first-round transitions, results, reservations or same-session Return to lobby. End/expiry/restart invalidates it immediately before listing removal, and a fresh distinct identity follows cleanup. An old selection must never be redirected to the fresh session. During open setup, phase may change while the bound identity remains the same; actual authority decides current capacity, role and lobby/live-round destination. Credential correction and eligible Retry retain selected hosting kind/endpoint/identity. An endpoint/selection change requires browser refresh; explicit unbound Direct transition clears binding and secrets. Hosting-kind schema/version changes follow the contract above; this does not require a new directory capability spelling.

The host owns the required capability set and authoritative gameplay-content baseline. Each guest owns its compatibility claim.

Each participant must generate its manifest claim from its locally installed supported gameplay content. A guest claim must not select a host file.

The host must freeze its authoritative manifest before remote admission starts. The host must not change that baseline during the session.

## Canonical gameplay-content manifest

### Included content

The manifest must include content that can change authoritative simulation or match outcomes:

- every gameplay level that the host can select during the session;
- gameplay metadata for each included level;
- gameplay data referenced by an included level when that data can change simulation or outcomes;
- gameplay configuration and data definitions.

The level scope is not limited to the initial level or rotation. Lobby changes may select only levels in the frozen manifest.

### Excluded content

The manifest must exclude:

- textures, sounds, fonts, and other presentation-only assets;
- local people, profiles, statistics, Elo, saves, and other persistence;
- local control assignments and controller presets;
- documentation.

Participant profile data must not affect compatibility. The network session must not load or execute a participant profile script.

- **CMP-VIS-001** Admission must not compare a profile, skin, animation resource, visual resource, or other cosmetic asset.
- **CMP-VIS-002** A missing, changed, or additional profile or cosmetic asset must not cause an admission rejection.
- **CMP-VIS-003** A supported client must use the built-in default network visual set after admission.
- **CMP-VIS-004** A client must not obtain a profile or cosmetic fallback from a peer.

First-release network matches must not enable or execute an optional gameplay script. The authoritative policy is in [`network-authoritative-headless-match.md`](network-authoritative-headless-match.md).

### Entry fields and canonical paths

Each manifest entry must contain one canonical logical path and one content identity. The content identity must represent the exact file contents.

Each logical path must meet all these requirements:

- It must contain from 1 through 240 ASCII bytes.
- It must contain from 1 through 16 `/`-separated segments.
- It must not have a leading or trailing slash.
- Each segment must contain from 1 through 64 characters.
- Each segment must match `[A-Za-z0-9][A-Za-z0-9._-]{0,63}`.
- It must not contain whitespace, a control character, a newline, or a backslash.
- It must not contain Unicode, percent encoding, or a bidirectional-control character.
- It must not contain an empty segment or a raw filesystem path.

The manifest must contain no duplicate case-sensitive logical path. It must contain no more than 256 entries.

Entries must use ascending unsigned-ASCII byte order. Source enumeration order must not affect the canonical result.

Equality must compare canonical logical paths and exact file contents. Filesystem and package metadata must not affect equality.

Filesystem metadata includes timestamps, permissions, owners, and archive order.

An invalid, duplicate, unsorted, or incomplete entry makes the manifest invalid. An excessive entry count also makes the manifest invalid.

A missing, additional, changed, or case-different valid entry causes a gameplay-content mismatch. The same rule applies to levels and level metadata.

### Implemented content identity and filesystem boundary

The scaffold uses SHA-256 over each file's exact bytes as the 32-byte cross-platform content identity. The wire representation carries those 32 bytes directly and does not compare textual hash encodings.

The current scaffold manifest builder includes every regular file under `levels/`, `data/blocks.json`, and `data/config.script`. It can also include an explicitly enabled `scripts/` path from its diagnostic script option. That option is not part of the supported first-release match path. The supported path must not supply that option. It must not enumerate `profiles/`, optional gameplay scripts, people, controls, saves, statistics, documentation, or presentation directories. A profile or optional gameplay script therefore cannot enter or execute through a supported first-release network match.

The builder sorts logical paths before hashing and rejects symlinks or reparse points, hard-linked files, non-regular entries, root escape, cross-device or cross-volume traversal, unsafe aliases, duplicate or invalid paths, missing required content, more than 256 manifest entries, more than 256 traversed directories, more than 512 examined filesystem entries, an individual file above 64 MiB, or more than 256 MiB of included content. Native descriptor- or handle-based reads use no-follow opens and revalidate file identity, size, and modification metadata after hashing. On Windows, the builder uses Win32 wide APIs throughout: it converts the configured UTF-8 root strictly, resolves an absolute requested path with `GetFullPathNameW`, opens and identifies the selected root object, obtains its normalized extended final handle path with `GetFinalPathNameByHandleW`, and pins both the complete requested and resolved ancestor chains through that same root identity. It does not pass extended Win32, volume, or UNC final-handle paths through `std::filesystem` canonicalization. Every pinned directory denies delete/rename sharing and must remain a non-reparse, non-device directory on the same volume with the same file identity and ordinal final path. Enumeration and file opens remain wide under the pinned final path, revalidate the root and ancestor chain before every pathname operation, compare normalized paths with `CompareStringOrdinal`, and convert bounded logical relative names to ASCII only after safe wide enumeration. Parent junctions, ancestor or root rename/replacement, root-identity changes, invalid UTF-8/UTF-16, case/Unicode alias mismatch, volume escape, non-regular content, mutation, and read failure therefore fail closed or remain bound to the originally pinned root object without publishing a partial manifest. These bounds limit pre-listener and pre-connection hashing work; they do not change the 262,144-byte admission-payload bound.

## Host admission flow

1. Start session must begin the existing 10-second host startup deadline.
2. The host must create one local host participant with from 1 through 15 local players.
3. The host must validate its protocol, release, required capabilities, and gameplay-content manifest.
4. The host must complete validation before listener readiness.
5. Successful host admission must assign the host and its players stable session identities.
6. A valid host-alone lobby may then accept remote admission.

Remote data must never create or acquire host authority.

### Invalid host manifest

An invalid host manifest must produce machine identifier `host-gameplay-content-manifest-invalid`.

The exact host-visible copy is:

`Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.`

The host must enter `NET-08`. No listener or session may remain.

Retry must be disabled for the current application session. Edit setup must return to `NET-02` with endpoint and local-player setup retained.

Return to Network must enter `NET-01`. A confirmed invalid-manifest result before the deadline must take precedence over a later startup timeout.

## Guest admission flow

1. Local endpoint and setup validation must occur before the connection attempt.
2. Connect activation must start one total 10-second deadline. Building and validating the guest's local gameplay manifest shares that deadline with resolution, transport connection, compatibility, capacity, host admission, and lobby confirmation.
3. A pending connection may send exactly one admission request.
4. The host must receive that request within three seconds of transport connection.
5. The host must validate bounds before allocation, comparison, logging, or authority changes.
6. The host/service must apply the trust and authorization decision from the trust policy. A browser-selected attempt must validate its bound hosting kind, endpoint and expected nonzero session identity before provisional reservation or authority allocation, including a first-controller dedicated attempt. Direct admission remains explicitly unbound and requires no hosting-kind selection. The common channel must not determine controller authority; actual admission must confirm the role.
7. The host must check protocol, release, required capabilities, manifest validity, and manifest equality.
8. The host must then check admission state and capacity.
9. On success, the host must atomically reserve provisional participant and player identities and return one complete `admission-offer` containing the nonzero participant identity, the requested player count, and the exact ordered player identities.
10. The guest must validate the participant identity, original requested count, exact player ordering, nonzero and unique values, and separation of participant and player identities. It must return exactly one `admission-acceptance` that repeats the complete offer before the single total deadline.
11. The host must reject an acceptance received at or after the total deadline or one that does not exactly match the offer. For one valid acceptance, it must atomically commit the participant identity, player identities, immutable connection binding, ownership, and slots.
12. After commit, the host must return one final `admitted` confirmation. It must contain the exact committed participant identity, player count, and ordered player identities.
13. The production flow must start host-clock calibration before it waits for the final confirmation or initial snapshot.
14. A production success result must include three inputs: the exact final confirmation, one valid host-clock calibration result, and one complete valid initial full snapshot.
15. Each success input must have an authoritative receipt time strictly before the total deadline.
16. The snapshot production time must be valid under the calibration result. Its participant identity and ordered owned-player identities must exactly match the final confirmation.
17. The guest may report success and enter the current authoritative lobby or match (including waiting-for-next-round) only after it validates all three inputs.

A guest admission request must contain at least one local player. Before commit, a rejection, offer-send failure, invalid or missing acceptance, cancellation, timeout, or disconnect must roll back the reservation and allocate no participant, playable slot, ownership, or committed session identity. Provisional identities remain burned for the session so that a later entity never reuses them. Commit is the rollback boundary: failure or loss of the final confirmation does not undo host state, and the committed participant passes to the disconnect and reconnect lifecycle owned by issue #36 while the guest reports the applicable incomplete-admission close or timeout result.

- **ADM-OWN-001** Admission commit must fix one participant's exact ordered player identities and ownership.
- **ADM-OWN-002** An admitted participant must not request an additional player slot or removal of one owned player slot.
- **ADM-OWN-003** A person or local-control change for an existing player slot must not change the committed player identity or ownership.
- **ADM-OWN-004** An authoritative roster-order change must not change a committed player identity or ownership.
- **ADM-OWN-005** Participant Leave, reservation expiry, or session end may remove committed player slots.
- **ADM-OWN-006** The service must not reuse an identity from a removed player slot during the session.

The guest must check Cancel before parsing every offer, rejection, or confirmation, immediately before acceptance enqueue, and immediately before publishing rejection, invalid-host, or success. One admission-attempt gate serializes Cancel, acceptance enqueue, and terminal publication. The concrete acceptance enqueue holds that gate through the outbound queue insertion and checks Cancel plus the injected monotonic clock at the insertion point. Cancel wins when it linearizes first; no `D6RK` is queued in that case. A strictly pre-deadline enqueue that linearizes first remains accepted and cannot be retroactively removed by a later Cancel; that later Cancel may still win against a not-yet-published final confirmation. The guest must never enqueue acceptance at or after the original Connect-start deadline. Runtime fakes expose the same conditional-enqueue operation so a test can change Cancel or time immediately before insertion. The host independently retains its transport-acceptance-time plus 10-second hard bound and rejects acceptance at or after that boundary. This client check prevents avoidable late asymmetric commit; only acceptance already sent before the guest deadline followed by confirmation loss retains the documented post-commit issue #36 handoff.

### Invalid guest-local manifest

An invalid guest manifest established strictly before the total attempt deadline must produce machine identifier `guest-gameplay-content-manifest-invalid` with exactly:

`Local gameplay content is invalid. Restore the supported gameplay content and restart the application.`

No resolution or transport connection may start. The guest must enter `NET-08`; Retry is disabled until application restart. Edit setup returns to `NET-03` with the endpoint and local players retained, and Return to Network enters `NET-01`. If local manifest work does not establish the specific result before the deadline, the attempt uses `Connection timed out.` instead.

## Identity and capacity invariants

Lobby and ongoing admission must preserve this invariant:

`1 <= admitted participants <= roster players <= 15`

The host counts as one admitted participant. Each admitted participant must own at least one roster player.

An admission that would exceed 15 participants or 15 roster players must fail as `session-full`.

Each admitted participant identity and player identity must be nonzero. Each identity must be unique within the session.

An identity must remain stable for its assigned entity. The host must not reuse an assigned identity during the same session.

A transport connection has no committed participant identity, player identity, ownership, readiness, or host authority before successful admission. A provisional reservation is private host state and is not a playable entity or an authority binding.

## Initial outcome identifiers and copy

The following identifiers are exact, case-sensitive protocol values. They must not contain dynamic text or peer data.

| Identifier | Exact user-visible copy |
|---|---|
| `malformed-request` | `Connection request rejected.` |
| `not-authorized` | `Connection not authorized.` |
| `protocol-incompatible` | `Network release mismatch. Use the same supported game release as the host.` |
| `network-release-mismatch` | `Network release mismatch. Use the same supported game release as the host.` |
| `required-capability-unsupported` | `Network release mismatch. Use the same supported game release as the host.` |
| `gameplay-content-manifest-invalid` | `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.` |
| `gameplay-content-mismatch` | `Gameplay content mismatch. Use the host's exact supported gameplay content.` |
| `match-already-started` | `Match completed. Join when the host returns to the lobby.` |
| `session-full` | `Session is full.` |
| `host-policy-rejected` | `Host rejected the connection.` |
| `admitted` | No rejection copy |

`admitted` is a success identifier. It is not a rejection identifier.

Any malformed, trailing, unexpected, or semantically inconsistent complete host offer, rejection, or final confirmation must close the connection and use machine identifier `invalid-host-admission-message` with exactly `Connection ended before admission completed.`. No peer value or dynamic detail may be logged or displayed. A partial transport frame is not a complete host message and remains subject to close or the total deadline.

### Host evaluation precedence

After local validation and user Cancel, the host must stop at the first applicable result in this order:

1. `malformed-request`
2. `not-authorized`
3. `protocol-incompatible`
4. `network-release-mismatch`
5. `required-capability-unsupported`
6. `gameplay-content-manifest-invalid`
7. `gameplay-content-mismatch`
8. `match-already-started`
9. `session-full`
10. `host-policy-rejected`
11. `admitted`

User-visible copy must not include a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, or payload.

Trusted diagnostics may identify one differing path only after that path independently passes every canonical-path rule. Diagnostics must not include an invalid path or raw payload.

## Quota and work-limit mapping

Issue #30 must keep all applicable limits from the trust and transport contracts. These limits include:

- 15 total transport connections;
- 8 process-wide pending connections;
- 4 pending connections for one source IPv4 address;
- 20 admission attempts for one source IPv4 address in 60 seconds;
- an immediate burst of 4 attempts;
- one admission request for each connection;
- a three-second first-request deadline;
- exactly one admission acceptance for each successful offer;
- one total 10-second Connect deadline covering manifest work and every admission message, with no separate offer timer;
- 2 concurrent manifest validations;
- a 262,144-byte initial admission payload;
- 4,096 properties;
- 256 manifest entries;
- a 240-byte canonical logical path;
- 15 participants and 15 roster players.

A policy limit that can return a complete response must use `host-policy-rejected`. This rule applies to source-rate and pending-admission quota exhaustion.

The rule also applies to the concurrent manifest-validation limit. The host must reject a third concurrent manifest validation immediately and must not queue it.

Participant or player capacity must use `session-full`, not `host-policy-rejected`.

If transport cannot accept a connection or return a complete response, the applicable transport outcome remains authoritative.

Missing the three-second request deadline must close the connection without admission. Without a complete response, the guest must use the incomplete-admission transport outcome.

## Timing, cancellation, and incomplete admission

The admission window is defined by `NET-ADM-001` through `NET-ADM-016` in [network play](network-play-first-release.md). Lobby, active rounds, and non-final round-end delays permit eligible admission. Commit strictly before the first-round outcome spawns immediately; other ongoing commits wait for the first round start strictly after admission. Outcome and round-start decisions take precedence at the same host-clock instant. Final outcome closes admission before final delay and summary. Initial snapshots mean current authoritative lobby or match state, including explicit waiting ownership without an actor. All ownership, calibration, compatibility, password, capacity, and connection-deadline checks remain required. The `match-already-started` identifier remains stable for final admission closure and must not imply a first-round cutoff. Both hosting kinds use NET-PASS-001–018 and the common TRU-PUB channel before initial commitment within the one absolute 10-second deadline. Zeroizable channel Password cannot replace the scoped reconnect credential, its original 30-second reservation, or become fresh admission/migration.

The guest's 10-second deadline includes resolution, connection, compatibility, capacity, host admission, clock calibration, and current initial snapshot validation. Waiting for an eligible spawn after complete admission is not an extension of that connection attempt or a connection failure.

For production networking, a final `admitted` message alone is not a complete valid admission result. The result must include all three success inputs from the guest admission flow.

The production guest must receive all three success inputs strictly before the deadline. It may process those queued inputs after the deadline during the sealed drain.

The sealed drain must not start or await a new calibration exchange. If calibration is unavailable at the deadline, predeadline confirmation and snapshot inputs must produce `Connection timed out.`.

An initial snapshot beyond the calibrated future bound is not a valid success input. It must change no canonical or presentation state.

The production flow may request a replacement while the original deadline remains open. A replacement request must not extend or restart that deadline.

The host must reject an acceptance received at or after its attempt boundary even when the bytes are otherwise valid.

A complete valid rejection received before the deadline must take precedence over a later generic transport symptom. A complete valid production success received before the deadline has the same precedence.

An offer alone is not a complete admission result. A final confirmation without a valid calibration result and a valid initial snapshot is also incomplete.

At deadline or terminal-state selection, the guest transport must atomically seal application input against the reader. It must drain complete queued frames with the terminal state.

The guest must use each frame's authoritative `receivedAt`. A complete valid result received strictly before the deadline retains precedence when polling resumes after the deadline.

A later recorded close must not replace that result. Invalid complete messages received strictly before the deadline use `invalid-host-admission-message`.

The guest must exclude a frame received at or after the deadline from admission, calibration, and initial-snapshot semantic validation. A late malformed, mismatched, or unexpected frame must not replace timeout or a complete valid predeadline result.

After the sealed drain, no racing reader enqueue may change the selected close or timeout outcome.

Without a complete host response, initial transport outcomes must use this order and copy:

1. name-resolution failure — `Host name could not be resolved.`
2. unreachable or refused connection — `Host unreachable.`
3. reset or close before complete admission — `Connection ended before admission completed.`
4. no complete result at the deadline — `Connection timed out.`

User Cancel and local inline validation must take precedence before host or transport outcomes. Cancel must preserve endpoint and local-player setup.

Cancel, timeout, and disconnect before commit must allocate no participant, player slot, ownership, or committed identity. Every provisional reservation is rolled back; its burned identity values are never reused. Loss of the final confirmation after atomic commit does not roll host state back.

A disconnect after complete admission is subject to issue #36. It is not an initial-admission failure.

## Reconnect compatibility handoff

Issue #36 must reuse the compatibility checks in this document. The following reconnect mappings are terminal for the guest reconnect state:

| Condition | Machine identifier | Exact user-visible copy |
|---|---|---|
| Protocol mismatch | `reconnect-protocol-incompatible` | `Network release mismatch. This session cannot be restored.` |
| Network release mismatch | `reconnect-network-release-mismatch` | `Network release mismatch. This session cannot be restored.` |
| Missing required capability | `reconnect-required-capability-unsupported` | `Network release mismatch. This session cannot be restored.` |
| Invalid manifest | `reconnect-gameplay-content-invalid` | `Gameplay content mismatch. This session cannot be restored.` |
| Content mismatch | `reconnect-gameplay-content-mismatch` | `Gameplay content mismatch. This session cannot be restored.` |

Retry must be disabled after one of these outcomes. The host must close only the offending connection.

A failed compatibility attempt must not disclose or change the valid reservation. Issue #36 owns reservation expiry, invalidation, removal, and state restoration.

## Local Play independence

`Play (F1)` must remain the existing local-only journey. It must not start a listener, server, transport worker, or client connection.

Local Play must not require network availability. Network compatibility failures, manifests, identities, and quotas must not change local Play.

Local Play must keep the local profile and scripting behavior in [`features.md`](features.md).

## Downstream boundaries

- Issue #29 owns transport framing, connection lifecycle, and transport outcomes.
- Issue #34 may use identities assigned by admission. It must not redefine their admission invariants.
- Issue #36 owns reconnect credentials, reservations, retries, expiry, removal, and restoration.
- Issue #38 owns graphical presentation, navigation, disabled reasons, and screenshots for these outcomes.
- Issue #39 owns trust boundaries, authorization primitives, quotas, validation bounds, and redaction rules.
- Issue #40 owns supported artifact and deployment documentation.
- Issue #41 owns complete release-candidate validation.

This compatibility contract does not own lobby UX, simulation, replication, readiness, scoring, or persistence. It consumes ongoing admission from the network-play contract without adding spectators or host migration.

Completion of issue #30 alone must not justify a claim that network play is available. It must not remove the experimental scaffold warning.

## Operational scaffold entry points

The explicit host validates and freezes its manifest, allocates its local host participant, and only then starts the listener:

```sh
./build/duel6r --server --transport --local-only --resources=resources --local-players=2
```

The explicit guest starts its 10-second attempt when invoked, builds its local claim within that deadline, and starts resolution only after the local claim is valid:

```sh
./build/duel6r --server --admission-client --host=127.0.0.1 --port=26660 --resources=resources --local-players=2
```

These commands provide process-level protocol evidence only. Successful output reports `admitted` and trusted session-local identities; rejected output reports only the fixed identifier and fixed copy. Neither command starts a lobby or gameplay session.

## Acceptance criteria

- **AC-001:** A host with a valid local manifest must complete local host admission with one stable nonzero participant identity.
- **AC-002:** Before the admission deadline, a compatible production guest must receive the final confirmation, valid clock calibration, and a complete valid initial snapshot. The confirmed identities and snapshot ownership must match exactly.
- **AC-003:** Admission must assign each identity once and must not reuse it during the session.
- **AC-004:** Missing, malformed, duplicate, excessive, or late admission data must fail before playable-slot allocation.
- **AC-005:** A protocol mismatch must fail with `protocol-incompatible` and the fixed release-mismatch copy.
- **AC-006:** A network release mismatch must fail under exact case-sensitive comparison.
- **AC-007:** A missing required capability must fail without degraded behavior.
- **AC-008:** Exact canonical manifest equality must succeed without regard to source enumeration order.
- **AC-009:** An invalid path, duplicate path, invalid order, missing field, or excessive manifest must fail as invalid manifest.
- **AC-010:** A changed, missing, additional, or case-different valid entry must fail as gameplay-content mismatch.
- **AC-011:** A changed gameplay level or level metadata file must fail as gameplay-content mismatch.
- **AC-012:** Cosmetic assets, profiles, people, controls, statistics, and saves must not affect compatibility. Missing, changed, or additional profile or cosmetic material must not cause an admission rejection.
- **AC-013:** The network session must not load or execute a participant profile script.
- **AC-014:** An optional gameplay script must not enter or execute through a supported first-release network match.
- **AC-015:** Admission must reject a request that would exceed 15 participants or 15 roster players.
- **AC-016:** Rejection or disconnect before commit must allocate no participant, player slot, ownership, or committed identity; a lost final confirmation after commit must not roll host state back.
- **AC-017:** The host must apply every admission outcome in the fixed precedence order.
- **AC-018:** User-visible rejection copy must contain no untrusted or compatibility-sensitive value.
- **AC-019:** Trust-policy quotas and concurrent-validation limits must remain effective during compatibility processing.
- **AC-020:** A complete valid rejection or production success received before the deadline must take precedence over a later transport symptom. An offer or final confirmation alone must not report success.
- **AC-021:** Missing calibration or another required success input at the 10-second deadline must produce `Connection timed out.`. Frames received at or after the deadline must not replace that outcome.
- **AC-022:** A transport close before complete admission must produce `Connection ended before admission completed.`.
- **AC-023:** New admission follows NET-AC-007 and NET-ADM-AC-003: the first outcome changes spawn eligibility, not non-final admission availability; equal-time outcomes and starts precede admission; final outcome rejects uncommitted offers. Waiting ownership, exact-once eligible spawn, reconnect, removal, and lobby retention follow NET-ADM-013 through NET-ADM-016.
- **AC-024:** Local Play must start and complete without starting or requiring a network service.
- **AC-025:** Completion of issue #30 alone must not justify a claim that network play is available or ready for release.
- **CMP-VIS-AC-001:** Admission ignores profile, skin, animation-resource, visual-resource, and cosmetic-asset differences. After admission, each supported client uses the built-in default network visual set without loading peer content as a fallback.
- **ADM-OWN-AC-001:** Admission confirms one immutable set of ordered player identities and ownership for each participant. Lobby person, control, and roster-order changes preserve that set. Participant removal removes all owned slots, and the service does not reuse their identities during the session.
