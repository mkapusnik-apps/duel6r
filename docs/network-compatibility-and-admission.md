# Network compatibility and admission

## Status and authority

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

The admission protocol version is separate from transport framing version `1`. Equal numeric values do not make these versions the same field.

## First-release constants

First release must use these exact values:

| Field | Exact value |
|---|---|
| Admission protocol version | unsigned integer `1` |
| Network release ID | `duel6r-network-r1` |

The network release ID is the only build-compatibility value. The admission request must not use a separate build-version compatibility field.

Supported Linux and Windows artifacts must use the same network release ID. The value must be non-empty.

Network release ID equality must be exact and case-sensitive. Whitespace is content and must not be trimmed or normalized.

First release supports no protocol range, release range, or cross-release compatibility.

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

The host owns the required capability set and authoritative gameplay-content baseline. Each guest owns its compatibility claim.

Each participant must generate its manifest claim from its locally installed supported gameplay content. A guest claim must not select a host file.

The host must freeze its authoritative manifest before remote admission starts. The host must not change that baseline during the session.

## Canonical gameplay-content manifest

### Included content

The manifest must include content that can change authoritative simulation or match outcomes:

- every gameplay level that the host can select during the session;
- gameplay metadata for each included level;
- gameplay data referenced by an included level when that data can change simulation or outcomes;
- gameplay configuration and data definitions;
- host-installed and host-enabled gameplay scripts.

The level scope is not limited to the initial level or rotation. Lobby changes may select only levels in the frozen manifest.

### Excluded content

The manifest must exclude:

- textures, sounds, fonts, and other presentation-only assets;
- local people, profiles, statistics, Elo, saves, and other persistence;
- local control assignments and controller presets;
- documentation.

Participant profile data must not affect compatibility. The network session must not load or execute a participant profile script.

Only host-installed and host-enabled gameplay scripts may affect authoritative network play. Those scripts must be present in the manifest.

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

The host and guest manifest builder includes every regular file under `levels/`, `data/blocks.json`, `data/config.script`, and each explicitly enabled `scripts/` path supplied through `--gameplay-script=`. It does not enumerate `profiles/`, people, controls, saves, statistics, documentation, or presentation directories. A profile script therefore cannot enter or execute through admission.

The builder sorts logical paths before hashing and rejects symlinks or reparse points, hard-linked files, non-regular entries, root escape, cross-device or cross-volume traversal, unsafe aliases, duplicate or invalid paths, missing required content, more than 256 manifest entries, more than 256 traversed directories, more than 512 examined filesystem entries, an individual file above 64 MiB, or more than 256 MiB of included content. Native descriptor- or handle-based reads pin the root and every traversed parent without rename/delete sharing, use no-follow opens, compare final Windows paths with ordinal semantics, and revalidate file identity, size, and modification metadata after hashing. Mutation, replacement, unsafe alias, and read failure must fail closed without publishing a partial manifest. These bounds limit pre-listener and pre-connection hashing work; they do not change the 262,144-byte admission-payload bound.

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
6. The host must apply the trust and authorization decision from the trust policy.
7. The host must check protocol, release, required capabilities, manifest validity, and manifest equality.
8. The host must then check admission state and capacity.
9. On success, the host must atomically reserve provisional participant and player identities and return one complete `admission-offer` containing the nonzero participant identity, the requested player count, and the exact ordered player identities.
10. The guest must validate the participant identity, original requested count, exact player ordering, nonzero and unique values, and separation of participant and player identities. It must return exactly one `admission-acceptance` that repeats the complete offer before the single total deadline.
11. The host must reject an acceptance received at or after the total deadline or one that does not exactly match the offer. For one valid acceptance, it must atomically commit the participant identity, player identities, immutable connection binding, ownership, and slots.
12. After commit, the host must return one final `admitted` confirmation containing the exact committed participant identity, player count, and ordered player identities. The guest may report success and enter the downstream lobby only after validating that confirmation strictly before its total deadline.

A guest admission request must contain at least one local player. Before commit, a rejection, offer-send failure, invalid or missing acceptance, cancellation, timeout, or disconnect must roll back the reservation and allocate no participant, playable slot, ownership, or committed session identity. Provisional identities remain burned for the session so that a later entity never reuses them. Commit is the rollback boundary: failure or loss of the final confirmation does not undo host state, and the committed participant passes to the disconnect and reconnect lifecycle owned by issue #36 while the guest reports the applicable incomplete-admission close or timeout result.

### Invalid guest-local manifest

An invalid guest manifest established strictly before the total attempt deadline must produce machine identifier `guest-gameplay-content-manifest-invalid` with exactly:

`Local gameplay content is invalid. Restore the supported gameplay content and restart the application.`

No resolution or transport connection may start. The guest must enter `NET-08`; Retry is disabled until application restart. Edit setup returns to `NET-03` with the endpoint and local players retained, and Return to Network enters `NET-01`. If local manifest work does not establish the specific result before the deadline, the attempt uses `Connection timed out.` instead.

## Identity and capacity invariants

Lobby admission must preserve this invariant:

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
| `match-already-started` | `Match already started. Join-in-progress is not supported.` |
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

The guest's 10-second deadline includes resolution, connection, compatibility, capacity, host admission, and lobby confirmation.

Success must be confirmed by the exact final `admitted` message strictly before the deadline. At or after 10 seconds, the generic result is `Connection timed out.`. The host must reject an acceptance received at or after its attempt boundary even when the bytes are otherwise valid.

A complete valid rejection received before the deadline, or a complete valid final confirmation received before the deadline, must take precedence over a later generic transport symptom. An offer alone is not a complete admission result.

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

This issue does not implement lobby UX, simulation, replication, readiness, scoring, or persistence. It does not add join-in-progress, spectators, or host migration.

Completion of issue #30 alone must not justify a claim that network play is available. It must not remove the experimental scaffold warning.

## Operational scaffold entry points

The explicit host validates and freezes its manifest, allocates its local host participant, and only then starts the listener:

```sh
./build/duel6r-server --transport --local-only --resources=resources --local-players=2
```

The explicit guest starts its 10-second attempt when invoked, builds its local claim within that deadline, and starts resolution only after the local claim is valid:

```sh
./build/duel6r-server --admission-client --host=127.0.0.1 --port=26660 --resources=resources --local-players=2
```

These commands provide process-level protocol evidence only. Successful output reports `admitted` and trusted session-local identities; rejected output reports only the fixed identifier and fixed copy. Neither command starts a lobby or gameplay session.

## Acceptance criteria

- **AC-001:** A host with a valid local manifest must complete local host admission with one stable nonzero participant identity.
- **AC-002:** A compatible guest must receive and validate a final confirmation containing stable nonzero participant and player identities before the admission deadline.
- **AC-003:** Admission must assign each identity once and must not reuse it during the session.
- **AC-004:** Missing, malformed, duplicate, excessive, or late admission data must fail before playable-slot allocation.
- **AC-005:** A protocol mismatch must fail with `protocol-incompatible` and the fixed release-mismatch copy.
- **AC-006:** A network release mismatch must fail under exact case-sensitive comparison.
- **AC-007:** A missing required capability must fail without degraded behavior.
- **AC-008:** Exact canonical manifest equality must succeed without regard to source enumeration order.
- **AC-009:** An invalid path, duplicate path, invalid order, missing field, or excessive manifest must fail as invalid manifest.
- **AC-010:** A changed, missing, additional, or case-different valid entry must fail as gameplay-content mismatch.
- **AC-011:** A changed gameplay level or level metadata file must fail as gameplay-content mismatch.
- **AC-012:** Cosmetic assets, profiles, people, controls, statistics, and saves must not affect compatibility.
- **AC-013:** The network session must not load or execute a participant profile script.
- **AC-014:** Only host-installed and host-enabled gameplay scripts may enter the authoritative manifest.
- **AC-015:** Admission must reject a request that would exceed 15 participants or 15 roster players.
- **AC-016:** Rejection or disconnect before commit must allocate no participant, player slot, ownership, or committed identity; a lost final confirmation after commit must not roll host state back.
- **AC-017:** The host must apply every admission outcome in the fixed precedence order.
- **AC-018:** User-visible rejection copy must contain no untrusted or compatibility-sensitive value.
- **AC-019:** Trust-policy quotas and concurrent-validation limits must remain effective during compatibility processing.
- **AC-020:** A complete valid rejection or final confirmation accepted before the deadline must take precedence over a later transport symptom; an offer alone must not report success.
- **AC-021:** No complete result at the 10-second deadline must produce `Connection timed out.`.
- **AC-022:** A transport close before complete admission must produce `Connection ended before admission completed.`.
- **AC-023:** An admission attempt after match start must receive the fixed join-in-progress rejection.
- **AC-024:** Local Play must start and complete without starting or requiring a network service.
- **AC-025:** Completion of issue #30 alone must not justify a claim that network play is available or ready for release.
