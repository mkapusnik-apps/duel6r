# Dedicated directory publication implementation

This is an implementation reference, not a provisioning or deployment claim. Linux x86-64 and Linux AArch64 packaged services accept the same common encrypted channel from Linux x86-64, Windows x86-64, Linux AArch64/Pi 5 and experimental Apple Silicon macOS 14+ and Intel x86_64 macOS 13 Ventura+ clients under their platform contracts. Dedicated hosting platforms are unchanged; no cloud endpoint is added.

## Server inputs

The shared application executable selects headless operation explicitly under [NET-EXEC](network-deployments.md#application-executable-and-mode-selection). Launch the operator-configured dedicated service with:

```text
duel6r --server --dedicated --transport --port=GAMEPLAY-PORT --resources=PATH --readiness-socket=PATH [--host=ASSIGNED-IPV4] [--password-file=PATH] [--advertise-host=GAMEPLAY-HOST --advertise-port=GAMEPLAY-PORT]
```

Use `duel6r --server --check-ready=PATH` for the readiness probe: confirmed readiness prints `ready` and exits `0`; otherwise it exits `1`. `--server` alone does not enable transport or configure a dedicated service. Headless operation does not initialize client presentation or require a display/audio device; the shared file still requires its runtime libraries.

The service listens directly using the current player-hosted Mbed TLS EC-JPAKE profile. Default bind is `0.0.0.0`; explicit eligible assigned IPv4 must bind without broadening or substitution. The retired `--trusted-proxy-protocol=v2` and `--invite-file` options reject. No certificate frontend/proxy handoff is required. `--password-file=PATH` retains NET-PASS-014–016: absent/valid empty selects No password; missing, unreadable, insecure or invalid supplied files fail before readiness/publication. Password loads once and persists across fresh sessions in the process; restart applies replacement. Values never enter arguments or listings.

Advertise inputs must be paired and are independent of listening coverage. Omitting them leaves Direct available without publication; no endpoint is inferred from wildcard or directory origin. Passwords preserve exact UTF-8, case and spaces within 128 bytes; controllers cannot manage them. Source quotas use the actual accepted TCP peer, never a PROXY/client-provided address.

The advertised host remains valid DNS or concrete non-loopback unicast IPv4, port 1–65535. Wildcard, loopback, link-local, multicast, URLs, malformed names and the directory origin reject. Same-machine Direct may use loopback. Neither endpoint nor listing label authenticates server identity. Unlocked first contact in both kinds is encrypted but does not resist active MITM; shared passwords do not establish personal identity. Directory HTTPS verification remains required.

`D6R_DIRECTORY_URL`, its existing explicit loopback-HTTP development exception, and the compiled distribution-channel default retain their existing meaning. Publication uses the shared non-GUI publisher with bounded HTTP requests, coalesced updates, renewal/retry and memory-only owner authority. Transition-only `Directory: Registering`, `Directory: Listed` and `Directory: Unavailable` output reports operator publication state without secrets or new controller actions.

## API and wire choices

- `/v1/listings` is the legacy player-hosted-only read contract. Dedicated registrations and mutations are not accepted through v1. Reads filter dedicated records inside bounded pages and preserve continuation over those pages, including an empty filtered page with a next cursor.
- `/v2/listings` explicitly opts into the mixed contract. Its response identifies `contract: mixed-v2`; every row carries `hosting`, `protection` and `admission`. The v2 cursor has an `m2_` prefix and cannot be used in v1, or vice versa. Neither endpoint changes owner Bearer/If-Match enforcement, ordering or lease expiry.
- Hosting values are `player-hosted` and `dedicated`; protection is `none` or `password`, consistent with `passwordRequired` in both modes. Retired `invite` records are safely excluded or fail closed, never reinterpreted as password admission. The atomic unreleased-v2 update uses existing 60-second lease expiry without a running database migration or new v3 adapter. Admission remains `open`, `full`, `closed` or `busy`, independently of occupancy and phase. Dedicated phases retain `idle`, `reserved` and `cleanup`; zero occupancy is allowed only for a ready idle dedicated session.
- Matching packages use admission **3**, release **`duel6r-network-r2`**, framing **1** and the unchanged three required capability names. Encrypted offers/acceptance/confirmation carry explicit hosting kind with exact identities/session binding; the [protocol reference](network-directory-implementation.md#live-admission-and-bounds) records encoding. Selected kind/session mismatch rejects before reservation; Direct publishes kind only after complete admission; reconnect retains confirmed kind.

The authority prepares a nonzero empty identity before readiness and publication, without a controller, players or a playable match. First committed admission creates the controller in that same identity. Same-session Return preserves it. End/expiry invalidates it before asynchronous directory removal; cleanup precedes fresh distinct empty preparation. Lost removal may leave historical rows until their lease expires, but those selections cannot allocate authority in the fresh session.

Replace clients/services with coordinated matching packages and point clients at the direct common-channel listener. Legacy dedicated outer-TLS frontends and PROXY backends are not alternate profiles: mismatch closes/times out without second-engine, plaintext or unlocked retry. A confirmed release mismatch requires a validated secure response. Local fixtures establish only recorded routine-test scope, not independent QA, deployment, native Windows GUI or deferred Mac/Pi acceptance.
