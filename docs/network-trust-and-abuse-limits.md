# Network trust boundaries and abuse limits

## Status

This document defines the production trust and resource policy delivered for issue [#39](https://github.com/mkapusnik-apps/duel6r/issues/39), under [#27](https://github.com/mkapusnik-apps/duel6r/issues/27). It complements the approved [first-release network-play target](network-play-first-release.md) and the current [transport documentation](networking.md).

The code remains an experimental scaffold with no playable network session. These controls do not add a lobby, compatibility admission, simulation, reconnect exchange, or graphical network UI, and they must not be used to remove the experimental/no-playable warning.

## Supported trust boundary

First release has no initial-admission authentication, passwords, tokens, certificates, TLS, or encryption. It is supported only between trusted game instances:

- on one trusted machine through IPv4 loopback; or
- on a trusted private RFC1918 IPv4 LAN through an explicitly selected private interface address.

It is not safe or supported on a hostile multi-user machine, the public Internet, a public VPN, a relay, NAT or port-forwarded exposure, or any network where an untrusted party can reach the port. Default bind is `127.0.0.1`; the exact `localhost` name is also accepted only when it resolves to loopback. LAN hosting requires an explicit `10/8`, `172.16/12`, or `192.168/16` IPv4 literal. Listener startup rejects every other hostname, wildcard/unspecified address, public address, multicast, broadcast, and link-local address before creating a listener. Guest endpoint hostnames are ASCII and at most 253 bytes; resolution may return at most 64 IPv4 addresses and connection attempts retain only loopback or RFC1918 results.

The exact CLI exposure copy is:

```text
Network session is limited to this machine. No authentication or encryption is used.
Network session is limited to a private LAN. No authentication or encryption is used. Do not expose this port to the Internet.
Network session cannot use a public or wildcard address. Use loopback or a private LAN address.
```

An unsupported listener address emits only the applicable rejection line and exits before constructing a listener. Normal `duel6r` startup and local-only Play still start no network service.

## Assets, actors, and boundaries

Protected assets are host authority, participant and player-slot ownership, canonical future simulation state, session availability, process memory and CPU, local files and scripts, reconnect identity, and non-disclosing diagnostics.

Trust boundaries are:

- **Local host authority:** created only by trusted local session setup. It is never granted by a remote message.
- **Transport peer:** every remote connection, frame, message, count, string, name, profile field, and source address is untrusted until its applicable bounded validation succeeds.
- **Admission:** a transport connection has no participant identity, player slot, readiness, or host authority. It may submit exactly one bounded initial request within three seconds. A future compatibility policy hook decides admission.
- **Participant authority:** after admission, one immutable connection-to-participant binding controls only that participant's readiness, proposals, leave action, and owned player slots. Host-only actions require the locally created host participant. Disconnect removes the connection's authority while reservation ownership may remain for #36; intentional/expired participant removal clears ownership.
- **Content and scripting:** guest Lua, content files, profile files, and profile-selected scripts never load or execute. Only host-installed and host-enabled gameplay scripts may later enter the authoritative manifest owned by #30.
- **Resolver helper:** the packaged helper is started by an explicit executable path with direct arguments and no shell, bounded output, restricted inherited handles, fail-closed supervision, and the existing process-global cap of 32 active or delayed helpers.
- **Diagnostics:** peer-facing copy and trusted local diagnostics are separate. The diagnostic API accepts only a trusted timestamp, local connection number, enumerated stage/category/limit name, and bounded counters.

This model limits accidental exposure and straightforward resource abuse by reachable peers. It does not provide confidentiality, peer identity, anti-cheat, resistance to a malicious local administrator, or public-service hardening.

## Admission and connection quotas

| Limit | Policy |
|---|---:|
| Total transport connections | 15 |
| Pending pre-admission connections, process-wide | 8 |
| Pending pre-admission connections per source IPv4 | 4 |
| Admission attempts per source IPv4 | 20 in a rolling 60 seconds |
| Immediate admission-attempt burst | 4 |
| First admission request | exactly one per connection |
| First request deadline | 3 seconds |
| Concurrent manifest validations | 2 |

Pending accounting is keyed by source IPv4 and is released when admission succeeds or the connection closes. It deliberately permits sequential and concurrently admitted participants behind one trusted IPv4 address. Limits are temporary controls only: there is no persistent ban or source denylist.

Source-rate bookkeeping is itself bounded to 256 active/recent IPv4 records and expires idle records after the rolling window. Exhausting that internal bookkeeping capacity fails closed without exposing source values.

The current explicit server transport gives the diagnostic echo path a bounded transport-only success hook so #29 diagnostics continue to work without participant authority. Because that diagnostic is not a network session or admission path, it retains the transport's 15-connection diagnostic contract instead of consuming session pending-admission reservations. The normal scaffold session path enforces the pending/source quotas but has no #30 compatibility semantics and therefore returns the generic host-policy rejection rather than inventing release or manifest fields.

## Bounded validation

All checks occur before downstream policy may rely on a value. The reusable constants and validators enforce:

| Value | Maximum or rule |
|---|---|
| Initial admission payload | 262,144 bytes |
| Properties | 4,096 |
| Property key | 128 bytes |
| General string | 4,096 bytes |
| Hostname | 253 bytes, ASCII hostname syntax |
| Participant/player name | 64 UTF-8 bytes; valid UTF-8; no control, newline, NUL, or bidirectional-control code point |
| Reason | 256 bytes, printable ASCII only |
| General collection | 256 entries |
| Gameplay manifest | 256 entries |
| Canonical logical path | 240 ASCII bytes, 1–16 segments, using the exact first-release segment grammar |
| Roster/participants | 15 |
| Resolver result | 64 IPv4 addresses |

The generic validation API exposes allocation-free `string_view`/count checks for property count, property key, general string, ASCII reason, collection size, manifest entry count, hostname, participant/player name, and canonical path. Property keys are 1–128 ASCII bytes, begin with an ASCII letter or digit, and otherwise contain only ASCII letters, digits, `.`, `_`, or `-`. General strings are bounded to 4,096 bytes; a more specific validator must also be applied whenever the field is a hostname, participant name, reason, or path. The prototype property parser applies the generic key/value/count checks without adding #30 fields or compatibility meaning.

Malformed data fails closed. Count and `string_view` validators run before downstream allocation, logging, manifest comparison, or authority changes. Guest profile metadata is data only and cannot select or load a local file or script.

## Queues, bandwidth, and processing

The existing per-connection queue limits remain 256 frames and 4 MiB in each direction. A process-wide 32 MiB aggregate application-payload queue budget now applies in addition. Outbound reservation is atomic: failure returns backpressure without accepting, evicting, dropping, or reordering existing admitted output. Inbound payload reservation occurs before allocation; exhaustion waits only for the existing bounded progress interval and then closes the offending connection. Dequeue, completion, close, and failure release reservations safely.

Reusable monotonic token buckets define these later session-policy limits:

| Direction/action | Sustained limit | Burst |
|---|---:|---:|
| Guest to host bytes | 512 KiB/s | 1 MiB |
| Host to one guest bytes | 4 MiB/s | 4 MiB |
| Non-input guest actions | 30/s | 60 |
| Inputs per owned player slot | 120 per one-second window | — |
| Accepted inputs across the host | 1,800 per one-second window | — |

Bandwidth and non-input action limits use token buckets. Input limits use bounded one-second counters because no additional input burst was approved. At most one input may be applied for one player slot in one simulation tick. Two consecutive one-second over-limit windows close only the offender. The policy primitives are independent of gameplay; #33 and the authoritative simulation must consume them when those message/application paths exist.

## Authorization and outcomes

The authorization primitive models a locally created host, immutable connection-to-participant bindings, participant-owned player slots, host-only actions, and guest-own readiness/proposal/leave/input actions. Unauthorized authority action uses `session-policy-violation`, the fixed copy `Connection ended.`, and requires closing only the offender. No lobby or simulation behavior is implemented here.

Initial and session policy outcomes are fixed and non-disclosing:

| Code | Exact user copy |
|---|---|
| `malformed-request` | `Connection request rejected.` |
| `not-authorized` | `Connection not authorized.` |
| `host-policy-rejected` | `Host rejected the connection.` |
| `session-policy-violation` | `Connection ended.` |

Copy never includes peer values, names, release IDs, manifest counts, thresholds, source addresses, credentials, paths, hashes, or raw payload. Host/local diagnostic events likewise have no API field for peer text, unvalidated path, credential, hash, profile value, or source IP. Secrets are not accepted through CLI arguments, environment settings, files, logs, diagnostics, or crash messages.

## Reconnect credential primitive

The #36 handoff provides a memory-only opaque credential generated by the operating system CSPRNG (`BCryptGenRandom` on Windows and `getrandom` on Linux). It is 128 bits, scoped to one session, participant, and reservation, expires after 30 seconds, and uses constant-time credential comparison. Generation retries at most four times and fails closed if the random source fails or repeatedly returns all-zero bytes. An all-zero credential is never made active, returned, or accepted.

A failed or wrong credential, wrong session, wrong participant, wrong reservation, or presented all-zero value does not invalidate, change, cancel, extend, or shorten the valid reservation. Only the offending connection closes under the applicable rate policy. `authorizeAndConsume` reports every failure as a rate-policy failure with exactly `Reconnect authorization failed. This session cannot be restored.` and discloses neither reservation existence nor which check failed.

A successful consume invalidates and erases the credential while holding the reservation lock before returning success to the future restoration caller; replay therefore fails. Expiry invalidates on the first reservation operation at or after the deadline. Correctly scoped participant removal, session end, and explicit cancellation invalidate immediately. Replacement first invalidates the correctly scoped old credential, then generates a nonzero credential distinct from the old value; if generation fails or repeats the old value through all bounded attempts, neither old nor replacement is usable. Wrong-scope cancellation, removal, end, or replacement requests leave an unrelated valid reservation unchanged. A later disconnect can use replacement to create a new reservation credential and a fresh 30-second expiry.

This issue does not implement credential exchange, persistence, command-line transfer, logging, reconnect attempts, connection-close rate handling, or state restoration.

## Downstream handoff and non-goals

- #30 owns release ID, capabilities, canonical manifest serialization/digest/exchange/comparison, and full admission success. It must use the admission hook, two-validation work limit, bounded manifest/path policy, host-installed script rule, and fixed outcome precedence without weakening them.
- #36 owns disconnect reservation lifecycle, secure credential exchange over the approved future session channel, reconnect attempt state, closing only the offending failed-attempt connection under rate policy, restoration after successful invalidate-before-return, and replacement after later disconnect.
- #38 owns graphical network UI and must reuse the fixed copy. No graphical screens, wireframes, or screenshots are changed by #39.
- #33 and #32 own applying action/input/authority/rate decisions to authoritative gameplay.

Non-goals are authentication, encryption, Internet safety, accounts, public hosting, hostile-machine isolation, anti-cheat, public VPN/relay/NAT traversal, compatibility fields, lobby/simulation implementation, reconnect exchange, and graphical UI. Full network play remains downstream of #27.
