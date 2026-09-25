# Directory and admission implementation notes

This document describes implementation choices for issue #98. The requirements in
[the directory contract](network-host-directory.md) and
[network play](network-play-first-release.md) remain authoritative. These notes are
not a release-readiness or deployment claim.

## Configuration and independence

`D6R_DIRECTORY_URL` selects the directory HTTPS origin. Certificate-chain and
hostname verification are mandatory; redirects, URL credentials, and implicit
proxy configuration are not accepted. No configured origin means directory
unavailable, not a networking or Local Play failure. There is no shipped production
origin or provisioned cloud environment.

For explicit local development only, `D6R_DIRECTORY_ALLOW_HTTP=1` permits an origin
beginning with `http://127.0.0.1:`. This exception is not for deployment or LAN
credentials. The client requires libcurl's asynchronous resolver support so DNS
does not defeat its request deadline.

Local Play does not construct a publisher or issue browser requests. Publication
starts only after supervised host readiness and a valid authoritative snapshot.
The publisher has one worker, coalesces state changes, attempts renewal every
20 seconds, and gives publication requests a three-second deadline. Directory
failure does not own the simulation or transport lifecycle. Shutdown attempts
removal with a one-second deadline; lease expiry handles missed removal.

## Directory contract

The service uses `/v1/listings`. Registration returns a random listing identifier
and a distinct 256-bit owner bearer credential. Firestore stores a SHA-256 digest
of that high-entropy credential, not a player password. Owner mutation requires
the credential and the current `If-Match` revision. Transactions prevent concurrent
writers from silently overwriting one another. Expired and incorrectly owned
records use the same rejection outcome.

The active query orders by the Firestore timestamp `leaseExpiry` and document ID,
excluding expired history before fetching at most 26 records for a 25-row page.
The bounded cursor contains that ordering tuple. Renewal can move a listing to a
later page: this is a live view, not a frozen traversal. Selection uses the stable
listing/session identity, not a row number. Reads recheck expiry after query
completion. Optional TTL cleanup on `leaseExpiry` is storage retention only; it is
not required for read correctness and has not been deployed.

Body size is limited to 2 KiB; HTTP header size to 4 KiB; per-instance concurrent
work to 32 and connections to 64. Responses time out after eight seconds without
releasing unfinished database work from the concurrency counter. Shared quotas
admit at most 30 registrations, 240 page reads, and 600 mutations per minute.
Rejected quota transactions still cost database work. Deployment ingress,
instance scaling, billing controls, and abuse monitoring remain operational
requirements, not guarantees supplied by these application quotas.

## Password and credential protection

The native channel uses Mbed TLS 3.6.7 LTS's built-in TLS EC-JPAKE implementation:
TLS 1.2, P-256/SHA-256 PAKE and `TLS_ECJPAKE_WITH_AES_128_CCM_8`. Upstream labels
this Thread ciphersuite experimental. The record authentication tag is **64 bits**;
it must not be described as 128-bit authentication. There is no custom PAKE,
password-hash challenge protocol, raw-password PSK substitution, certificate
fallback, plaintext fallback, session ticket, session-cache resumption,
renegotiation, or early-data path. Both platform artifacts require actual matching
and mismatching-password handshake checks, not symbol-presence checks.

The configured library must enable EC-JPAKE, TLS 1.2, client/server support, P-256,
SHA-256, AES-CCM, system entropy and CTR-DRBG. TLS 1.3 and PSA-backed TLS are disabled
for this private profile. Each connection owns its TLS, entropy and DRBG contexts;
record operations are serialized. Configured headers and compiled libraries must
match. The application uses the Apache-2.0 licensing option; distribution must
retain applicable notices. The 3.6 LTS branch's support ends in March 2027, requiring
a supported-version migration before then. The developer and repository maintainers
own this migration before March 2027.

Secure networking requires x86-64 AES-NI. The private library is built with
`MBEDTLS_AESNI_C` and `MBEDTLS_AES_USE_HARDWARE_ONLY`, excluding software table AES.
The application checks CPUID leaf 1's AES bit using the platform compiler's CPUID
intrinsic **before any Mbed context initialization, entropy polling or CTR-DRBG
seeding**. This covers both CCM record encryption and CTR-DRBG's AES use. There is
no software-AES fallback. Missing capability disables secure connection/listener
startup with fixed local feedback; Local Play is unaffected. Tests may restrict
hardware capability but cannot override a negative physical CPU check. This is
an AES dispatch guarantee, not a claim that every operation in the application
has been proven constant-time.

Each connection is limited to 2^20 TLS records per direction, 1 GiB of TLS
application plaintext per direction (including game framing), and 30 minutes from
creation, whichever is reached first. Record counts include handshake records and
are tracked across partial socket operations, not inferred from application calls.
The first authentication/decryption failure ends the channel. These caps limit key
exposure; they do not increase the tag length or provide public-service hardening.
Exhaustion closes the transport and an admitted guest must use the existing
reserved reconnect flow with a fresh PAKE/key exchange. It cannot silently reuse
keys, allocate new players, reset a world, or declare intentional host end.

A configured session password is domain-prefixed and SHA-256 mapped to 32 bytes
using Mbed TLS before being handed to the library's PAKE API. This preserves the
128-byte input limit with the Thread profile's 32-byte MPI bound. The digest is
only an in-memory PAKE input, never a transmitted challenge response, stored
verifier, or TLS PSK. No player account or username is created.
The no-password path instead uses a distinct, public protocol value. Knowledge
of that public value does not remove ephemeral key exchange or record encryption,
but it does not authenticate an unlocked host against an active intermediary.
Fresh library-generated ephemeral scalars and zero-knowledge-proof randomness are
created for every connection from the operating-system-seeded DRBG. Knowing the
public unlocked PAKE input does not reveal those scalars or the derived ephemeral
shared key to a passive observer. No PAKE state or TLS session is resumed or cached.
An entropy failure aborts setup; the only entropy test seam can force failure and
cannot substitute deterministic bytes for system entropy.
An unlocked endpoint-only first connection protects against passive observation,
**not active interception**. Session-ID comparison is not cryptographic host
identity. A person who knows a shared session password can impersonate another
password holder; passwords do not establish personal identity or prevent cheating.

Passwords remain application memory, travel to the supervised host only through
its existing private inherited control channel, and are not placed in process
arguments, environment variables, directory listings, saved persons, or diagnostics.
Entering a password requests password-authenticated transport; it does not fall
back to the unlocked value after failure. The host's admission transport uses the same enforcement for direct and browser
joins. Reconnect uses encrypted transport plus the existing scoped, expiring,
consume-once reconnect credential; it does not allocate new players.

## Live admission and bounds

The canonical network release identifier remains `duel6r-network-r1`; the encrypted
session transport does not provide a legacy plaintext fallback. The host's serial event loop
orders due simulation ticks before pending admission commits. The first outcome
closes new admission before the end delay; later rounds do not reopen it. Returning
to the lobby does. An offer alone reserves no participant authority.

Admission wire version 2 binds a browser-selected nonzero session ID inside the
encrypted `D6RA` request before reservation or commitment. The guest additionally
checks the confirmed snapshot and reconnect grant agree on session identity and
checks the exact ordered offered/accepted/confirmed participant/player identities.

Arrival initializes only the appended players, using the current world and normal
initial loadout. Team spawning retains the round's starting-area rotation. Predator
arrival adds a non-predator without replacing the current Predator. Existing
players, projectile owner pointers, world state, timers, and input sequence state
remain intact. The initial guest snapshot is the current calibrated authoritative
state, rather than an artificial waiting lobby.

At most 15 players remain admitted simultaneously. The in-round history has a
separate bound of 32 identities, including departed players, so references and
session-only result rows remain stable without unbounded allocation. Remaining
history space can reduce the advertised first-round capacity after repeated
departures and arrivals; exhausted capacity rejects before commitment rather than
resetting the match. This is a storage/work bound, not account tracking or a
person-name-based respawn restriction. The lobby starts a fresh match history.

## Verification status

### Final acceptance: blocked by environment evidence

Product's final assessment is **Blocked**, solely because feature-required
Linux/Windows interactive LAN evidence is missing. Source review approved
`6809885ecfe5dc622f03a1bafec0580ba6157602`; independent local QA passed, and
[UX accepted all eight supplied representatives](design/screenshots/README.md#current-host-directory-visual-assessment)
with their original capture lineage and disclosed fixture limits. Product assessed
the other criteria as satisfied in the observed environments. This does not waive
the remaining platform gate or certify environments that were not exercised.

The [required acceptance evidence](network-host-directory.md#required-acceptance-evidence)
for NET-DIR-AC-003, NET-PASS-AC-001 and
[NET-ADM-AC-001](network-play-first-release.md) still needs **two distinct authorized
Linux and Windows LAN endpoints, tested in both host directions**. For each
direction, use a non-production directory and demonstrate a real browser-selected
password-protected join during round one, visible and controllable arrival without
resetting the existing world/players, plus a direct secure join. No authorized
endpoints are currently available. Same-daemon Linux containers, Windows
cross-compilation and native MSVC unit tests do not substitute for this evidence.

PR #99 must remain draft and issue #98 open until this evidence is supplied and
the responsible owners approve readiness. No production cloud deployment is
required or authorized. Configuration is described above; backend/emulator and
future deployment prerequisites are in
[Directory container operations](../services/directory/README.md).

### Checkpoint evidence and retained artifacts

The reviewed production candidate is
[`6809885`](https://github.com/mkapusnik-apps/duel6r/commit/6809885ecfe5dc622f03a1bafec0580ba6157602).
Its [hosted verification run](https://github.com/mkapusnik-apps/duel6r/actions/runs/36192067073)
passed the Linux check, native MSVC check and Feature Ready aggregate, as reported
by Devops. Local backend evidence is 12/12 emulator tests; Linux full-suite
evidence is 30/30 at the applicable earlier checkpoints, supplemented by focused
checks for subsequent changes. The latest complete network-session-runtime target
passed in 88.01 seconds. Deterministic admission tests cover outstanding offers
crossing the outcome boundary, including equal-clock precedence. These local
results support, but do not replace, independent QA or hosted verification.

This final documentation/image integration changes no executable behavior.
The final documentation commit still requires a new Devops hosted-check assessment;
candidate checks must not be presented as checks of that later SHA.

The following complete Release GL4/Lua-ON bundles both represent source 6809885.
Manifest records cover their executables, resources, dependencies where applicable,
and licenses, and have been checked after packaging. The developer owns retention
and cleanup after the remaining QA consumer; do not rebuild merely for transfer.

| Runtime | Retained Docker-API location | Manifest SHA-256 |
| --- | --- | --- |
| Linux x86-64 | `d6r99-6809885-linux:/workspace/build` | `976f9802b292715e8861570fb36d417a54304f5fb1aa5c872c7b51296fb0c8e5` |
| Windows x86-64 cross bundle | `d6r99-6809885-w64-artifact:/workspace/build` | `1d11b21eb334335dbca02e31647cd20da2a275b311cfc6b9b0c08b5d5277111f` |

The manifest filenames are `linux-x86_64.sha256sums` and
`windows-x86_64.sha256sums`. Copy a bundle into an independent consumer directory
and validate its manifest before launch. The Linux tool image is
`sha256:d6e97be33c3c44744ba057147f5b6dae1c1534273b490edc0ef573b8ab1c1720`;
the Windows cross image is
`sha256:6e3bb669d7564791d3d35d9578e515111ba88d0820fee833ee7b96c2084aa278`.
The Windows bundle passed dependency validation, not interactive Windows QA.

### Nonblocking risks and follow-ups

- The experimental TLS EC-JPAKE profile, 64-bit CCM tag, AES-NI requirement and
  unlocked active-intermediary limitation described above remain explicit risks.
  Developer/repository maintainers own supported-library maintenance before March 2027.
- An original Linux full run passed 29/30 with a charged-bow timing failure while
  a cross-build ran concurrently. The case and runtime target passed unchanged
  afterward; subsequent isolated full runs passed 30/30. Contention is suspected,
  not proven. Developer owns investigation of any recurrence.
- A capture-session voluntary Leave briefly displayed Reconnecting. Four
  independent tester checks did not reproduce it; no concrete defect is established.
  This is an unconfirmed, nonblocking developer-owned follow-up, not a reason to
  invent a failure or claim the observation resolved.

### Emulator-backed native composition

The `duel6r-directory-client-integration-tests` target consumes a real local
emulator-backed directory. Run it inside the native build container with the test
executable **beside the runtime's resolver and server executables**. The resolver
is intentionally resolved relative to the executable, not from an arbitrary path.
The backend container can use `--network none`; the native test container shares
that container's network namespace. Do not publish emulator ports or mount cloud
credentials. With the backend listening on port 8081 in that isolated namespace:

```sh
docker exec -w /workspace/build \
  -e D6R_DIRECTORY_URL=http://127.0.0.1:8081 \
  -e D6R_DIRECTORY_ALLOW_HTTP=1 \
  "$NATIVE_TEST_CONTAINER" \
  /workspace/build/duel6r-directory-client-integration-tests \
  /workspace/build/duel6r-server /workspace/build
```

This checks native publication and browsing against the real service, then real
host readiness, protected directory-selected lobby and live-round joins, and
shutdown removal. The integration test owns its synthetic host processes; the
caller owns the temporary service and native test containers.

Developer routine tests cover the real Firestore emulator, secure transport,
browser eligibility, authoritative arrival, and cutoff behavior. Their existence
does not imply that all tests or supported platform gates have passed. Use the
checkpoint-specific handoff for actual results. Independent QA, security review,
Windows runtime evidence, and UX assessment remain separate gates. No cloud
provisioning, deployment, routing, firewall, relay, or NAT setup is authorized.
