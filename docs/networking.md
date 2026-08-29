# Production Session Transport and Experimental Networking Scaffold

## Status

The networking code now includes a production TCP session-transport layer and command-line compatibility admission under an experimental, incomplete developer scaffold. It does not add playable network support to Duel 6 Reloaded. Normal `duel6r` startup, Play, and every existing local game journey remain independent of this code: the game does not bind or connect a socket, start a transport worker or server, or expose network controls.

The approved future first-release scope and journeys are defined in [`docs/network-play-first-release.md`](network-play-first-release.md). The authoritative compatibility and admission target is in [`docs/network-compatibility-and-admission.md`](network-compatibility-and-admission.md). The player-hosted service lifecycle target is in [`docs/network-host-service-lifecycle.md`](network-host-service-lifecycle.md). Enforced exposure boundaries and reusable abuse controls are documented in [`docs/network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md). Those policies do not change the scaffold's current status and must not be read as implemented or playable network behavior.

The scaffold provides transport-neutral data transfer objects and a prototype text serializer for these message families:

- handshake request, acceptance, and rejection;
- lobby state;
- input commands;
- world snapshots;
- events;
- disconnect notifications;
- endpoints and client connection configuration.

It also provides connection-plan and command-construction helpers, an in-process loopback handshake helper, server configuration parsing, and the explicitly invoked `duel6r-server` executable.

## Explicit runtime exposure

Only explicitly launching `duel6r-server` with `--transport` (or the diagnostic `--transport-echo`) can start a listener. Bind defaults to loopback; exact `localhost` is accepted only through an assigned loopback result that passes local prefix policy. A LAN listener requires an explicit private RFC1918 IPv4 address confirmed by operating-system enumeration to be assigned unicast for a local interface prefix; wildcard, unspecified, unassigned private, network, directed/limited-broadcast, public, multicast, link-local, and other listener-hostname values are rejected before a listener exists. Linux validation consumes `ifa_netmask` and `ifa_broadaddr`; Windows consumes `OnLinkPrefixLength`. Prefixes through `/30` reject their network and broadcast endpoints, while RFC 3021 `/31` and host-route `/32` addresses have no such pair. The final octet alone does not determine directed-broadcast status: assigned `10.0.0.255/16` is valid and assigned `192.168.1.255/24` is rejected. Launching the executable without either flag preserves the no-transport scaffold result. A non-echo host validates and freezes gameplay compatibility before listener readiness. Readiness is printed only after IPv4 bind/listen succeeds and the accept worker is active. `SIGINT` or `SIGTERM` requests bounded shutdown. The server still has no lobby, gameplay simulation, reconnect exchange, or playable session behavior, and says so at startup.

For example:

```sh
./build/duel6r-server --transport --local-only --port=26660
```

For separate-process transport diagnostics only, `--transport-echo` returns each opaque application frame on the same connection. It never forwards a frame to another client and is not lobby admission or playable networking. The local-launch helper still only constructs a prospective executable-and-argument vector; it does not start or supervise a process. The legacy loopback helper still calls the handshake scaffold directly in-process and does not use the production transport.

## TCP envelope

The transport supports Linux x86-64 and Windows x86-64 over TCP/IPv4. Guest endpoints may be safe ASCII IPv4 literals or hostnames, but resolved targets are limited to loopback or private RFC1918 IPv4. Remote `.255` results are retained when they are in those ranges because the destination prefix is unknown. Hosts bind only loopback or an explicit assigned local private IPv4 literal. IPv6, UDP, discovery, matchmaking, NAT traversal, public Internet, and dedicated-product claims are outside this layer.

Every frame has this fixed 12-byte envelope followed by exactly `payload length` opaque bytes. All integers use unsigned network byte order (big-endian).

| Offset | Size | Field | Required value |
|---:|---:|---|---|
| 0 | 4 | framing identifier | `0x44365254` (`D6RT`) |
| 4 | 2 | framing version | `1` |
| 6 | 2 | frame kind | `0` application, `1` liveness ping, `2` liveness pong |
| 8 | 4 | opaque payload length | `0..1,048,576` for application; `0` for liveness |
| 12 | variable | opaque payload | exact bytes, including a valid zero-length payload |

The binary envelope intentionally replaces no part of the prototype text serializer: serialized handshake request and response bytes can be carried opaquely as application frames, but transport `Connected` never means the handshake was valid or the participant was admitted. The envelope has no compatibility promise with the earlier in-process prototype and may break prototype integrations without migration.

The implemented admission application payload is a bounded binary contract separate from framing version `1` and the prototype serializer. It carries admission protocol version `1`, network release ID `duel6r-network-r1`, capabilities, local-player count, sorted canonical paths, and raw 32-byte SHA-256 content identities. It contains no build-version field, client name, profile, token, or peer-provided rejection detail. Admission uses four messages: request, `admission-offer`, exact `admission-acceptance`, and final `admitted` confirmation. The offer, acceptance, and confirmation carry the same nonzero participant identity, exact player count, and ordered unique nonzero player identities. `D6RS` is rejection-only and rejects the `Admitted` enum value; only `D6RO` and `D6RC` encode network success stages. The host atomically commits immutable binding and ownership only after exact acceptance; every earlier incomplete path rolls the reservation back and burns its provisional identities. Confirmation loss after commit does not roll host state back, while the guest reports success only after exact confirmation. Guests derive visible copy locally from enumerated outcomes; a malformed or semantically inconsistent complete host message uses only `invalid-host-admission-message` and `Connection ended before admission completed.`.

An unsupported identifier/version/kind, a nonzero liveness length, or a payload length above 1,048,576 closes only the offending connection before payload allocation. An incomplete header or body is never delivered. Each accepted complete application frame is delivered once, in TCP order, to only its connection. TCP provides reliable ordered bytes while connected; this layer does not claim delivery after a close, retry application frames, deduplicate application retries, or provide state semantics.

## Lifecycle, bounds, and deadlines

- Listener lifecycle is `NotStarted -> Starting -> Ready -> Stopping -> Stopped`, with mutually exclusive terminal startup outcomes `Failed`, `Cancelled`, or `TimedOut`. `Ready` is published only after bind/listen and active accept processing, strictly before one 10-second startup deadline. Cancellation cannot later become Ready.
- Client lifecycle is `NotStarted -> Resolving -> Connecting -> Connected -> Closing -> Closed`, with mutually exclusive attempt outcomes `Failed`, `Cancelled`, or `TimedOut`. IPv4 DNS resolution and every resolved-address TCP attempt share one total 10-second deadline. Cancellation cannot later become Connected.
- Bind, resolution, connection-refused, unreachable, protocol, peer-close, queue-stall, idle-timeout, and other system failures remain distinguishable through `TransportFailure`; a known result before the deadline is not rewritten as timeout.
- A listener accepts at most 15 simultaneous connections. Before admission, process-wide pending capacity is 8, per-source-IPv4 pending capacity is 4, attempts are limited to 20 per rolling 60 seconds with burst 4, and the first admission request is due strictly within three seconds of the transport's recorded acceptance time. Manifest work and every admission stage share the guest's single 10-second Connect deadline; there is no separate offer timer. A successful offer permits exactly one matching full acceptance. Invalid, missing, late, cancelled, closed, or unsent acceptance rolls back the provisional reservation before commit. Successful commit releases source pending accounting so sequential same-IP participants remain possible and is not reversed if the final confirmation is lost. Each connection has independent workers and queues, so malformed input, stalling, timeout, and close affect only that connection.
- Guest Cancel is checked before every host message and immediately before acceptance enqueue and outcome publication. One attempt gate gives Cancel, acceptance enqueue, and terminal publication a mutex-serialized total order. The production conditional enqueue checks Cancel and the injected monotonic clock while holding that gate at the outbound queue insertion point. If Cancel or `now >= Connect-start + 10 seconds` wins first, no `D6RK` is queued; if a strictly pre-deadline enqueue wins first, later Cancel cannot retract that queued frame but may still win against a final outcome that has not published. Runtime connection fakes expose the same operation for deterministic pre-insertion clock and cancellation changes. At deadline or terminal selection, the connection atomically seals and drains already queued complete frames against the reader thread; authoritative `receivedAt` timestamps preserve a strictly pre-deadline valid rejection or exact confirmation over delayed polling or a later close, while late frames cannot create acceptance or success.
- In each direction, at most 256 application frames or 4 MiB of application payload may be queued, whichever limit is reached first. A process-wide 32 MiB application-payload queue budget applies in addition. `send` reserves both budgets atomically and immediately returns `Backpressure` without accepting, evicting, dropping, or reordering the new frame. Inbound aggregate exhaustion is bounded by the existing progress deadline and closes only the offender. Payloads above 1,048,576 immediately return `PayloadTooLarge`; a pre-admission first request is additionally limited to 262,144 bytes before allocation. Up to two zero-payload liveness controls use a separate priority queue. At each complete frame boundary the writer selects pending controls before another application frame, while a two-control burst limit guarantees progress for queued application output. Connected sockets require no-delay writes and request a 4 KiB kernel send buffer to limit write-ahead, but the effective buffer size remains operating-system controlled. Controls remain hidden from application consumers and do not weaken application backpressure.
- No progress on a partial inbound frame, a full inbound queue, or socket output for five seconds closes only that connection. Successful socket reads of application or internal frame bytes and successful application-frame socket writes share the 30-second transport-activity boundary. Only bytes accepted by the application-frame socket send operation count as outbound progress; enqueueing output, polling, blocked sends, and writing internal probes do not count. Recent outbound application progress prevents a healthy one-way transfer from depending on a probe that cannot interleave within its active frame. When a frame boundary is available, inbound-idle one-way output still schedules bounded probes ahead of the next application frame; when both inbound activity and outbound application progress are absent, the same internal ping/pong exchange maintains a healthy quiet connection or allows it to reach the idle timeout. Controls are never exposed as application payloads.
- Connection close rejects new sends, flushes already accepted output in order for at most two seconds, then force-closes. Listener shutdown stops accepts immediately and releases its listener, connections, queues, and transport workers within three seconds. Repeated close, shutdown, and cancel requests are safe and retain the original deadline.
- Startup/connect cancellation interrupts pending sockets without transferring their ownership and stops and joins the attempt worker within one second. Linux and Windows isolate blocking `getaddrinfo` work in the packaged `duel6r-resolver` helper process. The parent passes only a validated ASCII hostname and decimal service as direct process arguments, with no shell, and accepts only a bounded IPv4 response on standard output. Linux also passes its validated PID, uses `posix_spawn`, redirects standard input/error to `/dev/null`, and closes every unrelated descriptor. Before resolver work, the helper installs `PR_SET_PDEATHSIG` and verifies that both the parent observed at startup and the current parent still match that expected PID. Windows uses a Unicode executable path, a safely constructed command line from the restricted argument alphabet, and an explicit inherited-handle list.
- On Windows 10 and later, every helper is atomically assigned at creation to a process-global Job Object configured with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`; creation fails rather than starting a helper when that containment cannot be established. Active and delayed helpers are terminated by Windows when application exit closes the job, so no resolver work can run outside supervision.
- Cancellation or deadline requests termination and polls reap completion for a bounded interval. Normal OS termination is reaped immediately. If the OS refuses or delays termination/reap, a process-global supervisor retains the Linux PID or Windows process handle and retries asynchronously; attempt workers and sockets still return within their API bound. The supervisor owns at most 32 active or delayed helpers, and new real resolutions fail safely while that cap is exhausted. A persistently noncompliant OS may therefore retain up to that bounded set for the application lifetime; the transport does not claim a finite reap time in that exceptional condition. No per-attempt resolver thread or unowned helper remains, and delayed cleanup cannot later publish Ready or Connected.

The transport API is `source/network/SessionTransport.h`. `SessionTransportDependencies` provides optional resolver, monotonic-clock, and connector operations so application tests can deterministically hold resolution, advance the shared resolution/connect deadline, return exact refusal/unreachable/timeout outcomes, and release on cancellation. Connections expose their authoritative acceptance time, complete-frame receipt time, terminal time, conditional admission-acceptance enqueue, and a concrete atomic seal-and-drain snapshot. The seal takes the reader queue lock, prevents later application delivery, drains queued complete frames, and snapshots terminal state under the transport's fixed input-then-terminal lock order, so admission precedence does not depend on delayed polling or a reader race. The admission runtime separately exposes injectable client, connection, listener, clock, cancellation, wait, outbound, lifecycle, identity, work, and manifest seams. Empty operations always select the real production implementations; injected operations receive the same absolute deadline and cancellation probe, fail closed when they throw or report failure, and must not retain workers after returning. Generic bounded validators and the clarified fail-without-mutation reconnect credential primitive are documented in [`docs/network-trust-and-abuse-limits.md`](network-trust-and-abuse-limits.md). The transport deliberately exposes opaque frames rather than release/content compatibility, admission, client IDs, reconnect exchange, or simulation messages; those remain downstream policy.

The legacy in-process prototype handshake requests still contain their prototype protocol version, scaffold build version, and a non-empty client name so existing prototype helpers remain source compatible. They are not used by command-line compatibility admission and have no release-compatibility authority. The implemented replacement contract is defined in [`docs/network-compatibility-and-admission.md`](network-compatibility-and-admission.md).

Authentication and encryption are intentionally excluded from first release. Non-empty authentication tokens are rejected by serializers, connection planning, handshake validation, and server construction. `duel6r-server` rejects token command-line options so secrets cannot be exposed through generated process command lines or process listings. This is safe only within the supported trusted loopback/private-LAN boundary.

## Prototype serializer format and safeguards

The current line-oriented text format is prototype-only. It has no backward- or forward-compatibility promise and may be replaced without migration support.

Serialization is deterministic where practical because property keys use a stable order. Parsing rejects malformed escapes, missing or duplicate required properties, invalid numeric and Boolean values, non-finite floats, unsupported enum/action values, zero ports, and invalid required strings. Payloads, property counts, keys, values, lobby/snapshot player lists, and other collections have explicit bounds to prevent unbounded allocation in this scaffold.

These safeguards make the parser suitable for prototype development; the TCP bounds do not provide authentication, authorization, encryption, TLS, or a complete protocol security design. Do not expose the scaffold as an untrusted public service.

## Build and packaging

CMake builds `duel6r-network-scaffold`, including the transport API, `duel6r-server`, and the internal `duel6r-resolver` helper independently from the normal game executable. The transport uses standard OS sockets (`ws2_32` on Windows and POSIX sockets/threads/processes on Linux) with no new third-party dependency. Existing Linux and Windows runtime bundle scripts place the explicitly invoked server scaffold and required resolver helper beside `duel6r` because those are the repository's current coherent binary packaging paths. The helper is an internal transport component with no supported command-line interface.

There is no dedicated headless-server package. Existing runtime bundles still include all client resources, and no release-facing documentation advertises network support.

## Missing networking implementation

The following essential pieces are absent:

- playable remote or local client/server sessions;
- an authoritative simulation runtime;
- lobby and session lifecycle handling;
- complete world, score, round, and entity replication;
- lobby integration for admitted participants;
- latency handling, interpolation, prediction, reconciliation, or lag compensation;
- player-hosted service supervision and reconnect policy;
- reconnect compatibility exchange and reconnect credential exchange;
- network-facing UI.

These items are tracked in [Complete end-to-end network-play support](https://github.com/mkapusnik-apps/duel6r/issues/27). Until they are implemented and independently validated, the scaffold must not be described as network support.
