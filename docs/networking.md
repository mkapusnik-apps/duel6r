# Production Session Transport and Experimental Networking Scaffold

## Status

The networking code now includes a production TCP session-transport layer under an experimental, incomplete developer scaffold. It does not add network play to Duel 6 Reloaded. Normal `duel6r` startup, Play, and every existing local game journey remain independent of this code: the game does not bind or connect a socket, start a transport worker or server, or expose network controls.

The approved future first-release scope and journeys are defined in [`docs/network-play-first-release.md`](network-play-first-release.md). That target specification does not change the scaffold's current status and must not be read as implemented or playable network behavior.

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

Only explicitly launching `duel6r-server` with `--transport` (or the diagnostic `--transport-echo`) starts a listener. Launching the executable without either flag preserves the no-transport scaffold result. Readiness is printed only after IPv4 bind/listen succeeds and the accept worker is active. `SIGINT` or `SIGTERM` requests bounded shutdown. The server still has no lobby, admission, compatibility, client identity, gameplay simulation, reconnect policy, or playable session behavior, and says so at startup.

For example:

```sh
./build/duel6r-server --transport --local-only --port=26660
```

For separate-process transport diagnostics only, `--transport-echo` returns each opaque application frame on the same connection. It never forwards a frame to another client and is not lobby admission or playable networking. The local-launch helper still only constructs a prospective executable-and-argument vector; it does not start or supervise a process. The legacy loopback helper still calls the handshake scaffold directly in-process and does not use the production transport.

## TCP envelope

The transport supports Linux x86-64 and Windows x86-64 over TCP/IPv4. Endpoints may be IPv4 literals or hostnames that resolve to IPv4, including loopback and directly reachable LAN addresses. IPv6, UDP, discovery, matchmaking, NAT traversal, and Internet or dedicated-product claims are outside this layer.

Every frame has this fixed 12-byte envelope followed by exactly `payload length` opaque bytes. All integers use unsigned network byte order (big-endian).

| Offset | Size | Field | Required value |
|---:|---:|---|---|
| 0 | 4 | framing identifier | `0x44365254` (`D6RT`) |
| 4 | 2 | framing version | `1` |
| 6 | 2 | frame kind | `0` application, `1` liveness ping, `2` liveness pong |
| 8 | 4 | opaque payload length | `0..1,048,576` for application; `0` for liveness |
| 12 | variable | opaque payload | exact bytes, including a valid zero-length payload |

The binary envelope intentionally replaces no part of the prototype text serializer: serialized handshake request and response bytes can be carried opaquely as application frames, but transport `Connected` never means the handshake was valid or the participant was admitted. The envelope has no compatibility promise with the earlier in-process prototype and may break prototype integrations without migration.

An unsupported identifier/version/kind, a nonzero liveness length, or a payload length above 1,048,576 closes only the offending connection before payload allocation. An incomplete header or body is never delivered. Each accepted complete application frame is delivered once, in TCP order, to only its connection. TCP provides reliable ordered bytes while connected; this layer does not claim delivery after a close, retry application frames, deduplicate application retries, or provide state semantics.

## Lifecycle, bounds, and deadlines

- Listener lifecycle is `NotStarted -> Starting -> Ready -> Stopping -> Stopped`, with mutually exclusive terminal startup outcomes `Failed`, `Cancelled`, or `TimedOut`. `Ready` is published only after bind/listen and active accept processing, strictly before one 10-second startup deadline. Cancellation cannot later become Ready.
- Client lifecycle is `NotStarted -> Resolving -> Connecting -> Connected -> Closing -> Closed`, with mutually exclusive attempt outcomes `Failed`, `Cancelled`, or `TimedOut`. IPv4 DNS resolution and every resolved-address TCP attempt share one total 10-second deadline. Cancellation cannot later become Connected.
- Bind, resolution, connection-refused, unreachable, protocol, peer-close, queue-stall, idle-timeout, and other system failures remain distinguishable through `TransportFailure`; a known result before the deadline is not rewritten as timeout.
- A listener accepts at most 15 simultaneous connections. Each connection has independent workers and queues, so malformed input, stalling, timeout, and close affect only that connection.
- In each direction, at most 256 application frames or 4 MiB of application payload may be queued, whichever limit is reached first. `send` immediately returns `Backpressure` without accepting, dropping, or reordering the new frame. Payloads above 1,048,576 immediately return `PayloadTooLarge`.
- No progress on a partial inbound frame, a full inbound queue, or socket output for five seconds closes only that connection. A 30-second receive-idle boundary is enforced; internal zero-payload ping/pong frames maintain healthy quiet connections and are never exposed as application payloads.
- Connection close rejects new sends, flushes already accepted output in order for at most two seconds, then force-closes. Listener shutdown stops accepts immediately and releases its listener, connections, queues, and transport workers within three seconds. Repeated close, shutdown, and cancel requests are safe and retain the original deadline.
- Startup/connect cancellation interrupts pending sockets without transferring their ownership, stops and joins the transport worker, and releases resolver resources within one second. Linux and Windows isolate blocking `getaddrinfo` work in the packaged `duel6r-resolver` helper process. The parent sends one bounded hostname request over inherited standard-input/output pipes, accepts only a bounded IPv4 response, and kills and reaps the helper on cancellation or deadline. Linux launches the helper with `posix_spawn`; Windows uses an explicit executable path and a restricted inherited-handle list. Neither path detaches a resolver thread, runs resolver code after returning, or can publish Ready or Connected after cancellation or at/after the shared deadline.

The transport API is `source/network/SessionTransport.h`. `SessionTransportDependencies` provides optional resolver, monotonic-clock, and connector operations so application tests can deterministically hold resolution, advance the shared resolution/connect deadline, return exact refusal/unreachable/timeout outcomes, and release on cancellation. Empty operations always select the real production implementations; injected operations receive the same absolute deadline and cancellation probe and must not retain workers after returning. The transport deliberately exposes opaque frames rather than release/content compatibility, admission, client IDs, reconnect reservations, or simulation messages; those remain downstream policy.

Handshake requests must contain the current protocol version, the scaffold build version, and a non-empty client name. Missing or incompatible values are rejected. Resource entries, when supplied, must contain both a path and a hash, but the scaffold does not yet compare them with an authoritative server manifest.

Authentication is unsupported. Non-empty authentication tokens are rejected by serializers, connection planning, handshake validation, and server construction. `duel6r-server` rejects token command-line options so secrets cannot be exposed through generated process command lines or process listings.

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
- server-side resource-manifest comparison;
- latency handling, interpolation, prediction, reconciliation, or lag compensation;
- local server process supervision and reconnect policy;
- authentication and a secure secret-transfer mechanism;
- network-facing UI.

These items are tracked in [Complete end-to-end network-play support](https://github.com/mkapusnik-apps/duel6r/issues/27). Until they are implemented and independently validated, the scaffold must not be described as network support.
