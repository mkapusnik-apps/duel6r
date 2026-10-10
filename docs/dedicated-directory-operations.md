# Dedicated directory publication implementation

This is an implementation reference for the approved [directory contract](network-host-directory.md), not an operator provisioning recipe or a claim of deployed service availability. The dedicated service remains Linux x86-64; dedicated admission remains Linux/Windows x86-64. No cloud gameplay endpoint or new deployment is selected here.

## Server inputs

The existing `--dedicated --transport --host=127.0.0.1 --port=BACKEND-PORT --resources=PATH --trusted-proxy-protocol=v2 --invite-file=PATH --readiness-socket=PATH` invocation retains its protected same-machine backend and invitation-file contract. Optional `--advertise-host=TLS-FRONTEND --advertise-port=TLS-PORT` inputs identify the operator's separate TLS frontend. Both must be supplied together. No advertised frontend is inferred from the backend listener or directory origin. Omitting them leaves direct dedicated admission available without publication. Invitations remain in the protected file, never in process arguments or public listing metadata.

The advertised host must be a valid DNS hostname or concrete non-loopback unicast IPv4 address, and the port must be in 1–65535. Wildcard, loopback, link-local, multicast, URLs and malformed names are rejected. The configured directory origin cannot also be the advertised gameplay origin. This validation does not provision or verify the external TLS frontend; actual clients must still validate its certificate and invitation.

`D6R_DIRECTORY_URL`, its existing explicit loopback-HTTP development exception, and the compiled distribution-channel default retain their existing meaning. Publication uses the shared non-GUI publisher with bounded HTTP requests, coalesced updates, renewal/retry and memory-only owner authority. Transition-only `Directory: Registering`, `Directory: Listed` and `Directory: Unavailable` output reports operator publication state without secrets or new controller actions.

## API and wire choices

- `/v1/listings` is the legacy player-hosted-only read contract. Dedicated registrations and mutations are not accepted through v1. Reads filter dedicated records inside bounded pages and preserve continuation over those pages, including an empty filtered page with a next cursor.
- `/v2/listings` explicitly opts into the mixed contract. Its response identifies `contract: mixed-v2`; every row carries `hosting`, `protection` and `admission`. The v2 cursor has an `m2_` prefix and cannot be used in v1, or vice versa. Neither endpoint changes owner Bearer/If-Match enforcement, ordering or lease expiry.
- Hosting values are `player-hosted` and `dedicated`; protection is `none`, `password` or `invite` with validated consistency. Admission is `open`, `full`, `closed` or `busy`, independently of occupancy and phase. Dedicated phases add `idle`, `reserved` and `cleanup`; zero occupancy is allowed only for a ready idle dedicated session.
- Existing private admission request/version/release defaults remain unchanged. Dedicated offers carry a nonzero session identity suffix, echoed by acceptance and confirmation. Selected dedicated clients validate that identity before accepting an offer and verify it again in the reconnect grant and initial snapshot. The fixed selected-session-change outcome requires browser refresh/reselection; it does not authorize a replacement or fallback.

The authority prepares a nonzero empty identity before readiness and publication, without a controller, players or a playable match. First committed admission creates the controller in that same identity. Same-session Return preserves it. End/expiry invalidates it before asynchronous directory removal; cleanup precedes fresh distinct empty preparation. Lost removal may leave historical rows until their lease expires, but those selections cannot allocate authority in the fresh session.

Local emulator and TLS-fixture verification establishes only its recorded routine-test scope. It does not establish independent QA, live directory deployment, external frontend operation, native Windows graphical acceptance, or deferred Mac/Pi GUI evidence.
