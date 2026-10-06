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

The directory deployment section below authorizes only the specified cloud directory environments and their required provisioning. It supersedes earlier blanket cloud-deployment prohibitions for these environments only. It does not authorize cloud gameplay hosting or player accounts. Local Play must not contact the directory.

## Directory deployment

This section owns the operational product contract for the staging and production directory. It records approved deployment constraints, not a claim that either environment is deployed or verified. Player-hosted gameplay, admission, directory behavior, and security requirements remain unchanged.

- **NET-DIR-DEP-001** The directory deployments must use Cloud Run in project `duel-6-reloaded` and region `europe-west1`.
- **NET-DIR-DEP-002** A successful nightly must deploy its directory image to `staging-directory`.
- **NET-DIR-DEP-003** A push to `master` must deploy the same immutable image from the last successfully verified staging deployment to the production directory.
- **NET-DIR-DEP-004** Production promotion must not rebuild the image.
- **NET-DIR-DEP-005** Both directory services must provide public HTTPS access without removing the existing listing-owner authorization requirements.
- **NET-DIR-DEP-006** Staging and production must use separate named Firestore databases and separate runtime identities.
- **NET-DIR-DEP-007** Each directory service must use a minimum of zero instances, a maximum of two instances, and a concurrency limit of 32.
- **NET-DIR-DEP-008** The staging directory must provide HTTPS directory access at `https://staging.duel.netusite.cz`.
- **NET-DIR-DEP-009** The production directory must provide HTTPS directory access at `https://duel.netusite.cz`.
- **NET-DIR-DEP-010** A network-capable nightly client distribution must use the compiled default origin `https://staging.duel.netusite.cz` when `D6R_DIRECTORY_URL` is absent.
- **NET-DIR-DEP-011** A network-capable release client distribution must use the compiled default origin `https://duel.netusite.cz` when `D6R_DIRECTORY_URL` is absent.
- **NET-DIR-DEP-012** The client must use a valid explicit `D6R_DIRECTORY_URL` setting instead of the compiled default. The client must treat a present empty or invalid setting as directory unavailable without fallback to the compiled default.
- **NET-DIR-DEP-013** Distribution workflows must select the compiled default origin by distribution channel independently of the CMake build optimization configuration. An unchannelled build must have no compiled default origin.

A failed or unverified staging deployment is not eligible for production promotion. If no successfully verified staging image exists, there is no eligible image to promote. Public HTTPS directory access does not establish public gameplay reachability or overall network-release readiness.

The nightly distribution path selects staging. The release distribution path selects production, including a manual invocation of the release workflow. `Release` optimization does not identify the release distribution channel. Network-capable macOS distributions follow these same channel rules under [MAC-NET-016](macos.md#network-entry); unchannelled builds have no default origin. This requirement does not add a macOS caller to the existing release workflow or authorize a new cloud deployment campaign.

The client must not switch between staging and production after a directory failure. Explicit overrides must retain the existing origin validation and HTTPS certificate-chain and hostname verification. The explicit loopback-HTTP development exception remains limited to local development. A missing origin in an unchannelled build means directory unavailable. These outcomes must preserve NET-DIR-015 and Local Play independence. Local Play must not contact the directory.

### Directory deployment acceptance criteria

- **NET-DIR-DEP-AC-001 — Staging:** A successful nightly deploys its identifiable immutable directory image to `staging-directory` in the approved project and region. A failed nightly does not qualify for this deployment path. This criterion covers NET-DIR-DEP-001 and NET-DIR-DEP-002.
- **NET-DIR-DEP-AC-002 — Promotion:** A push to `master` deploys the exact image digest from the last successfully verified staging deployment without a rebuild. Failed or unverified staging deployments cannot replace the eligible image. No promotion occurs when no eligible image exists. This criterion covers NET-DIR-DEP-003 and NET-DIR-DEP-004.
- **NET-DIR-DEP-AC-003 — Environment boundaries:** Both services provide public HTTPS directory access and retain listing-owner authorization. The services use separate named Firestore databases and runtime identities, with the instance and concurrency limits from NET-DIR-DEP-007. This criterion covers NET-DIR-DEP-001 and NET-DIR-DEP-005 through NET-DIR-DEP-007.
- **NET-DIR-DEP-AC-004 — Scope and independence:** Deployment instructions distinguish directory availability from gameplay reachability and network-release readiness. The domain and distribution-default changes preserve UI contracts, gameplay admission, direct joining, running hosted sessions, and platform evidence requirements. Local Play does not contact the directory. Experimental macOS follows the same channel rules; native automated directory/package verification is required before pre-review and merge under MAC-NET-AC-004, while actual Mac GUI and live cross-OS checks remain deferred after merge/nightly under MAC-NET-AC-006. No new cloud campaign is required by this platform extension.
- **NET-DIR-DEP-AC-005 — Domain access:** Each approved domain provides certificate-validated HTTPS directory access to its corresponding environment. Domain access retains listing-owner authorization and staging/production isolation. This criterion covers NET-DIR-DEP-008 and NET-DIR-DEP-009.
- **NET-DIR-DEP-AC-006 — Distribution defaults:** With `D6R_DIRECTORY_URL` absent, network-capable nightly packages select the compiled staging origin and release packages select the compiled production origin. Manual release-workflow invocation selects production. Distribution-channel selection remains independent of CMake build optimization configuration. Unchannelled builds have no compiled default origin. This criterion covers NET-DIR-DEP-010, NET-DIR-DEP-011, and NET-DIR-DEP-013.
- **NET-DIR-DEP-AC-007 — Explicit override:** A valid explicit `D6R_DIRECTORY_URL` setting selects the requested directory instead of the compiled default. A present empty or invalid setting makes the directory unavailable without fallback. Directory failure does not switch between staging and production. Existing secure-origin validation and the explicit local-development exception remain unchanged. This criterion covers NET-DIR-DEP-012.

Deployment evidence must identify the immutable source checkpoint, nightly or promotion event, image digest, deployed service revision, project, region, endpoint, and verification result. Promotion evidence must identify the eligible staging deployment and show digest equality with production. Hosted observations must establish HTTPS directory operation and retained listing-owner authorization; emulator results alone do not establish these outcomes. Configuration and access-policy evidence must establish environment separation and service limits. Team supplies this evidence; these criteria do not require a new gameplay or screenshot campaign.

Domain-access evidence must identify each approved origin, its environment and serving service revision, and its certificate-validated HTTPS directory and owner-authorization results. A configured domain mapping alone does not establish DNS resolution or public TLS readiness. Distribution-default and override evidence must identify the immutable client source checkpoint, package identity, distribution channel, build optimization configuration, override presence and validity, and selected origin or unavailable outcome. Relevant consumer results must establish selection behavior and independence; configuration review may support workflow channel selection. These criteria do not require another full gameplay or screenshot campaign.

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

Team must supply evidence tied to an immutable checkpoint and observation context. Independent QA must cover real browser-to-host and direct joins, round-one boundaries, directory failure/expiry, password rejection, and reconnect. Developer routine results may support deterministic lifecycle and limit cases. Reviewer evidence must address admission authority, secure mechanisms, secret handling, and listing ownership. UX assessment and implementation screenshots must cover the changed screen states. No production cloud deployment is required or implied by these criteria.

### Deferred nightly platform evidence

The user has approved deferring only the remaining interactive QA on distinct Linux and Windows LAN endpoints, with each operating system as host, to nightly testing. This evidence is not a prerequisite for feature PR readiness or feature-scope product acceptance. The evidence remains required for the applicable overall network-release and deployment claims. No functional acceptance criterion or supported-platform requirement is removed.

The user plans the later testing. Team and testing own execution and the evidence handoff. The handoff must identify the immutable nightly source checkpoint, artifact identities, endpoint environments, scenarios, and observations.

On two distinct LAN endpoints, testing must cover Linux host to Windows guest and Windows host to Linux guest. Each direction must demonstrate:

- Browser-selected password-protected admission to the intended host.
- Round-one arrival with the new player controllable and visible on both endpoints, without resetting existing gameplay.
- Direct joining through the secure admission path.

These checks complete the deferred platform evidence for NET-DIR-AC-003, NET-PASS-AC-001, and NET-ADM-AC-001. Existing invariant mode, timing, hazard, paging, and failure evidence need not be repeated unless the nightly implementation materially changes the relevant behavior. The deferred checks must remain recorded as not executed until results are supplied. Directory deployment authorization is separate and limited to the directory deployment section above.

### Current feature acceptance

Product acceptance is Accepted for the revised feature scope. This assessment uses the supplied evidence for source checkpoint `6809885ecfe5dc622f03a1bafec0580ba6157602`, its supplied documentation-only equivalent `4a0e5001ac8452afdfded5f9894ec6ad2987aecc`, independent Linux QA, reviewer approval, satisfied UX assessment, and reported exact-head CI results of 30 Linux, 12 backend, and 15 native Windows checks. Previously satisfied criterion assessments remain valid for unchanged behavior.

Distinct Linux/Windows interactive LAN checks in both host directions remain not executed and deferred to nightly testing. Feature acceptance does not mark them passed and does not establish overall network-release readiness. The existing recorded security-maintenance and unconfirmed scheduling risks remain follow-up items; this evidence deferral does not resolve them.
