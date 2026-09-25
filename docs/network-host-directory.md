# Host directory and password admission

## Authority and scope

This document owns central listing, browser, and optional password behavior. [Network play](network-play-first-release.md) owns gameplay admission and [trust policy](network-trust-and-abuse-limits.md) owns security boundaries. This is target behavior, not a release-readiness claim.

An active listing is an unexpired registration for a ready player-hosted session. All active listings means all such registrations in the configured directory, not every game running outside that directory. Listing visibility does not depend on LAN membership or join eligibility.

## Listing lifecycle

- **NET-DIR-001** A host must automatically register its session only after confirmed hosted-service readiness.
- **NET-DIR-002** The directory must include all active listings, including full, password-protected, first-round, later-round, and summary states.
- **NET-DIR-003** Each listing must identify its session, endpoint, mode, current player count, player capacity, password-required state, and admission phase. The admission phase must distinguish lobby, first round, and admission closed.
- **NET-DIR-004** The directory must expire a listing no later than 60 seconds after its last accepted registration or renewal. Reads must exclude expired listings without waiting for background deletion.
- **NET-DIR-005** A ready host must attempt renewal at least every 20 seconds while directory contact succeeds. A host must submit a changed admission phase, capacity, or password-required state within five seconds of the authoritative change while contact succeeds.
- **NET-DIR-006** An orderly host shutdown must attempt listing removal. A crash or failed removal must use the same expiry rule.
- **NET-DIR-007** Only the authorized listing owner must renew, change, or remove its listing. Restarted sessions must not inherit admission authority from an old listing.
- **NET-DIR-008** Registration, renewal, removal, and directory reads must use bounded fields, payloads, result pages, request rates, and work. Browsing must let the user reach every active listing through bounded results rather than silently limiting the directory to one page.

## Browse and connection outcomes

- **NET-DIR-009** `NET-10` must let the user refresh listings and select a session for join setup in `NET-03`. Opening the browser must request current listings.
- **NET-DIR-010** The browser must distinguish loading, available results, empty results, stale results, and directory unavailable. Each read attempt must finish or report unavailable within 10 seconds.
- **NET-DIR-011** While the browser is open, it must request a refresh at least every 20 seconds. A failed refresh or results older than 30 seconds must mark retained results stale. Stale results must not claim current admission eligibility.
- **NET-DIR-012** Full or admission-closed listings must remain visible but must not offer a join attempt from that listing. The browser must explain the applicable reason. Stale results must require a successful refresh before browser joining.
- **NET-DIR-013** A locked eligible listing must lead to password entry. Selection must not reserve a slot or guarantee reachability, compatibility, or admission. The actual host must enforce all admission conditions again.
- **NET-DIR-014** A stale, expired, changed, or unreachable selection must produce an actionable failure without joining a different advertised session silently. The user must be able to edit direct setup or return to the browser and refresh.
- **NET-DIR-015** Directory failure must not end a running hosted session or block direct joining or Local Play. The host must be able to see that publication failed and retry publication without restarting the match. Background retries must be bounded.
- **NET-DIR-016** The browser must state that listing visibility does not guarantee reachability. It must not represent another machine's loopback address or an overlapping private address as verified contact with the advertised host.

## Optional password

- **NET-PASS-001** Host setup must let the host choose no password or a non-empty session password before startup. No password must be the default. The password must remain fixed until that session ends.
- **NET-PASS-002** The actual host must enforce the password before committing initial admission for both direct and browser-selected connections. Missing or incorrect passwords must not allocate admitted players or grant participant authority.
- **NET-PASS-003** Join setup must let the guest enter or correct a password before Connect. It must retain the endpoint and local-player setup after password rejection. Password entry time must not consume the connection deadline.
- **NET-PASS-004** Password rejection must use `Connection not authorized.` without echoing the submitted secret. The user must be able to return to editable join setup.
- **NET-PASS-005** Passwords, reusable password proofs, and listing-owner or reconnect credentials must not appear in directory results, process arguments, logs, diagnostics, crash messages, or persistent player data.
- **NET-PASS-006** Initial password checks must retain bounded admission attempts and non-disclosing failures. A directory lock indication must not replace host enforcement. A password must not grant host authority or control of another participant's players.
- **NET-PASS-007** A valid reconnect must use existing reserved identity and state. It must not become a new password admission or a fresh player spawn.
- **NET-PASS-008** Password and credential exchanges must use maintained secure mechanisms that protect secrets against passive interception and reject replay.
- **NET-PASS-009** Password-protected admission must protect the password and reusable admission credentials against an active intermediary that does not know the password. A secure exchange must not silently fall back to unprotected password transmission.
- **NET-PASS-010** An unlocked direct connection with only an endpoint and no trusted host credential must encrypt credential exchange, but it need not authenticate first contact against an active intermediary. The product must disclose this limitation in its network security guidance. It must not describe this connection as authenticated host identity.

## Support boundaries

LAN is the supported gameplay environment. Valid public IPv4 endpoints are permitted, but the product does not guarantee Internet connectivity, latency, NAT traversal, or protection against every Internet threat. Private-address filtering is not a substitute for password and credential protection. Passwords do not establish personal identity or provide anti-cheat guarantees.

Unlocked endpoint-only direct joining protects against passive interception, not an active intermediary during first contact. A session identity check does not establish cryptographic host identity. This limitation does not waive listing-owner authorization, protected directory access, password-protected admission, or reconnect scope and replay checks. An authenticated directory may supply host-authentication information for browser joining, but this contract does not require a particular mechanism. Accounts, manual fingerprint comparison, and out-of-band key verification are not required user journeys.

This contract does not authorize cloud deployment, provisioning, accounts, billing, or infrastructure changes. It does not select a cloud provider, database, wire protocol, or cryptographic implementation. Local Play must not contact the directory.

## Acceptance criteria

- **NET-DIR-AC-001 — Complete listing lifecycle:** Only ready hosts register. Full, locked, running, and summary sessions remain listed. Changed status is submitted within five seconds. Shutdown removes a listing when delivery succeeds; missed removal and crashes expire within 60 seconds without background deletion.
- **NET-DIR-AC-002 — Browser freshness:** All active listings are reachable through bounded browsing. Opening, periodic refresh, manual refresh, empty, failed, and stale states follow the 10/20/30-second bounds. Full, closed, and stale listings cannot start browser joins.
- **NET-DIR-AC-003 — Real joining:** An eligible selection reaches actual host admission and the correct lobby or live-round destination. Unreachable, expired, wrong-session, incompatible, full, and cutoff-race selections fail truthfully without silent substitution.
- **NET-DIR-AC-004 — Independence:** A directory outage preserves an active match, direct joining, and offline Local Play. Publication can recover without match restart.
- **NET-DIR-AC-005 — Ownership and limits:** Unauthorized registration-lifecycle changes fail. Malformed, oversized, and excessive requests fail within documented implementation limits without compromising valid session authority.
- **NET-PASS-AC-001 — Host enforcement:** No-password admission and correct-password admission succeed when otherwise eligible. Missing and wrong passwords fail on direct and browser paths before commitment. Correction retains non-secret setup.
- **NET-PASS-AC-002 — Secret protection:** Reviewer evidence establishes maintained secure mechanisms, encrypted secret exchange, replay rejection, and host enforcement. For password-protected admission, evidence must establish protection against an active intermediary without the password and no unprotected fallback. For unlocked endpoint-only direct first contact, evidence must establish passive-interception protection and truthful disclosure of the active-intermediary limitation; authenticated first contact is not required. Behavioral evidence covers rejected replay and bounded guessing. Listings, generated arguments, diagnostics, and persistence disclose no secrets.
- **NET-PASS-AC-003 — Reconnect:** A password-protected session restores only the reserved participant and current player state through the existing reconnect rules.

## Required acceptance evidence

Team must supply evidence tied to an immutable checkpoint and observation context. Independent QA must cover real browser-to-host and direct joins, round-one boundaries, directory failure/expiry, password rejection, and reconnect. Developer routine results may support deterministic lifecycle and limit cases. Reviewer evidence must address admission authority, secure mechanisms, secret handling, and listing ownership. Linux and Windows LAN evidence must cover each host direction where platform behavior differs. UX assessment and implementation screenshots must cover the changed screen states. No production cloud deployment is required or implied by these criteria.
