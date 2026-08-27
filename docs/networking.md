# Experimental Networking Scaffold

## Status

The networking code is an experimental, incomplete developer scaffold. It does not add network play to Duel 6 Reloaded. Normal `duel6r` startup and every existing local game journey remain independent of this code: the game does not connect to a server, start a server, or expose network controls.

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

## Honest runtime behavior

`duel6r-server` only validates its command-line configuration. It does not bind a socket or other transport, listen for clients, run a lobby, or simulate a match. After valid configuration it prints an explicit no-transport/unsupported result and exits with status `2`. It never reports that it is ready to accept clients.

For example:

```sh
./build/duel6r-server --local-only --port=26660
```

The local-launch helper constructs a prospective executable-and-argument vector; it does not start or supervise a process. The loopback helper calls the handshake scaffold directly in-process. A successful loopback handshake returns an accepted result with a nonzero client ID and reports that no local process was launched. This path does not use or emulate network transport.

Handshake requests must contain the current protocol version, the scaffold build version, and a non-empty client name. Missing or incompatible values are rejected. Resource entries, when supplied, must contain both a path and a hash, but the scaffold does not yet compare them with an authoritative server manifest.

Authentication is unsupported. Non-empty authentication tokens are rejected by serializers, connection planning, handshake validation, and server construction. `duel6r-server` rejects token command-line options so secrets cannot be exposed through generated process command lines or process listings.

## Prototype format and safeguards

The current line-oriented text format is prototype-only. It has no backward- or forward-compatibility promise and may be replaced without migration support.

Serialization is deterministic where practical because property keys use a stable order. Parsing rejects malformed escapes, missing or duplicate required properties, invalid numeric and Boolean values, non-finite floats, unsupported enum/action values, zero ports, and invalid required strings. Payloads, property counts, keys, values, lobby/snapshot player lists, and other collections have explicit bounds to prevent unbounded allocation in this scaffold.

These safeguards make the parser suitable for prototype development; they do not make it safe to expose to an untrusted network because no transport or complete protocol security design exists.

## Build and packaging

CMake builds `duel6r-network-scaffold` and `duel6r-server` independently from the normal game executable. The existing Linux and Windows runtime bundle scripts place the server scaffold beside `duel6r` because those are the repository's current coherent binary packaging paths.

There is no dedicated headless-server package. Existing runtime bundles still include all client resources, and no release-facing documentation advertises network support.

## Missing networking implementation

The following essential pieces are absent:

- a real network or IPC transport;
- playable remote or local client/server sessions;
- an authoritative simulation runtime;
- lobby and session lifecycle handling;
- complete world, score, round, and entity replication;
- server-side resource-manifest comparison;
- latency handling, interpolation, prediction, reconciliation, or lag compensation;
- connection supervision, process lifecycle management, shutdown, and reconnect;
- authentication and a secure secret-transfer mechanism;
- network-facing UI.

These items are tracked in [Complete end-to-end network-play support](https://github.com/mkapusnik-apps/duel6r/issues/27). Until they are implemented and independently validated, the scaffold must not be described as network support.
