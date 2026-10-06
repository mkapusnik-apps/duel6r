# Experimental macOS local and network play

## Product scope

This document owns the macOS platform contract. [features.md](features.md) remains authoritative for local-game behavior. The macOS target is experimental implementation support, not verified platform support.

- **MAC-PLAT-001** The macOS application must target Apple Silicon on macOS 14 or later.
- **MAC-PLAT-002** The macOS application must provide local play with the existing two-through-15-player roster and all existing local game modes.
- **MAC-PLAT-003** The macOS application must use the existing `gl1` renderer.
- **MAC-PLAT-004** The macOS extension must preserve the behavior, packaging, and renderer defaults of other platforms.

Intel Macs, older macOS versions, IPv6 gameplay, dedicated hosting, new renderers, gameplay changes, visual redesign, Developer ID signed or notarized distribution, App Store distribution, and cross-platform save migration are outside this contract. The unsigned application retains ad-hoc integrity seals. Existing pointer mapping and full-drawable viewport behavior from PR #110 and PR #119 must be preserved.

## Application and distribution

- **MAC-DIST-001** The macOS distribution must provide an unsigned `.app` with its required runtime resources and dependencies.
- **MAC-DIST-002** The user must be able to launch the application from Finder without selecting a process working directory.
- **MAC-DIST-003** The nightly release must provide a separate macOS Apple Silicon artifact without replacing the existing platform artifacts.
- **MAC-DIST-004** The artifact documentation must identify the target, source revision, unsigned status, installation and launch procedure, and experimental verification status.
- **MAC-DIST-005** The launch instructions must explain the applicable macOS approval procedure without requiring the user to disable system-wide security protections.

## Saved data

- **MAC-SAVE-001** The macOS application must store `data/persons.json` under `~/Library/Application Support/Duel 6 Reloaded/` for the current user.
- **MAC-SAVE-002** The macOS application must create the required saved-data directories when they do not exist.
- **MAC-SAVE-003** The macOS application must preserve person records, roster membership, statistics, Elo data, and the played-round count across application restarts according to PER-002 through PER-005.
- **MAC-SAVE-004** The macOS application must not require write access to its `.app` to save person data.
- **MAC-SAVE-005** Replacing the macOS application with a nightly application must not replace or delete existing per-user person data.

MAC-SAVE-001 is the macOS-only location exception to PER-001. It does not change the saved-data format, save timing, missing-file behavior, or data locations on other platforms.

## Network entry

- **MAC-NET-001** The macOS application must provide the existing `Network (F2)` entry.
- **MAC-NET-002** Pointer activation of `Network (F2)` and the F2 key must enter `NET-01`.
- **MAC-NET-004** Enabling macOS networking must preserve Local Play and other-platform network entry.
- **MAC-NET-009** macOS must provide the existing Host, direct Join, and Browse contracts and their existing screen journeys.
- **MAC-NET-010** macOS must host and join sessions with other approved platforms using the same supported network release and gameplay content, preserving participant ownership, authoritative gameplay, and recovery contracts.
- **MAC-NET-011** Gameplay transport must remain IPv4, defaulting to loopback and permitting eligible assigned private or public unicast listening addresses under the existing interface and trust rules.
- **MAC-NET-012** The port must preserve the existing trust, password, admission, hosted-service lifecycle, and reconnect contracts, including containment, cancellation, secret protection, and bounded work.
- **MAC-NET-013** The application must detect usable hardware AES before any cryptographic context initialization, entropy polling, or cryptographic RNG initialization.
- **MAC-NET-014** AES key expansion and block operations must use hardware AES. Software AES and plaintext fallback are forbidden.
- **MAC-NET-015** Missing secure capability must fail closed for listener and connection startup while Local Play remains available.
- **MAC-NET-016** macOS distributions must use the existing directory-origin rules in NET-DIR-DEP-010 through NET-DIR-DEP-013.
- **MAC-NET-017** Network sessions must not write local person records, persistent statistics, Elo, or network results into Local Play saves.

MAC-NET-003 and MAC-NET-005 through MAC-NET-008 are retired historical requirements for the former local-only notice. `MENU-01-MAC-LOCAL` and `MENU-02-MAC-LOCAL` are historical-only states, not routes in a network-capable build. [The existing screen inventory](screens/README.md) remains authoritative; no new screen is added. [UX-NET-01-006](design/screens/NET-01.md#presentation-requirements) solely owns the replacement platform-scope copy.

The existing [network-play](network-play-first-release.md), [directory/password](network-host-directory.md), [trust](network-trust-and-abuse-limits.md), and [hosted-service lifecycle](network-host-service-lifecycle.md) contracts remain authoritative. LAN is supported; other valid IPv4 connections are permitted without a public reachability guarantee. No automatic NAT, firewall, or routing changes are permitted. Linux, Windows, and Raspberry Pi security must not be weakened.

## Acceptance boundaries

### Experimental implementation acceptance

- **MAC-AC-001** Build and package evidence identifies an Apple Silicon macOS 14-or-later target, `gl1`, and a complete unsigned `.app` at an immutable source revision.
- **MAC-AC-002** Packaging and launch-path review establish that application resources do not depend on the caller's working directory and that person data uses MAC-SAVE-001 rather than the application bundle.
- **MAC-AC-003** Automated behavioral results and source review establish pointer and F2 entry to NET-01, available Host/direct Join/Browse journeys, unchanged Local Play selection, and unaffected network entry on other platforms. These results do not substitute for deferred native GUI verification.
- **MAC-AC-004** Nightly publication configuration provides the separate macOS artifact and preserves existing artifacts; the published artifact must subsequently identify its immutable source revision and experimental status.
- **MAC-AC-005** Relevant regression results and review show that the extension preserves other platforms and unrelated functional requirements.

Declarative package and publication behavior may use source/configuration review and relevant build checks. These checks do not establish successful Finder launch, macOS input, audio, rendering, or gameplay.

### Mandatory native automated network acceptance

**MAC-NET-AC-001 through MAC-NET-AC-005 are required before pre-review and experimental implementation acceptance, and before merge.** They are not covered by the manual Mac deferral. Evidence must identify the immutable candidate, native Mac architecture/OS/toolchain, artifact and dependency identities, command, fixtures, and actual outcomes. Compilation, echoed expectations, symbol presence, skipped tests, or Linux-only execution do not establish native behavioral parity.

- **MAC-NET-AC-001 — Secure transport:** Native real secure handshakes and application-data exchange pass for matching-password and unlocked sessions. Wrong-password, tamper, and replay cases reject; entropy and capability failures fail closed. Source review and native proof establish hardware AES key expansion and block operations and capability gating before TLS/RNG initialization. Tests must never override a negative physical capability result.
- **MAC-NET-AC-002 — Process lifecycle:** Native separate-process tests cover readiness, startup cancellation, port conflict, timeout, normal shutdown, abrupt parent termination, unexpected server termination, and owned descendants. Listener release and absence of orphans require positive process-identity/ownership evidence, not elapsed time alone. Resolver tests cover resolution, cancellation, deadline, retained cleanup ownership during delayed termination, and parent death. Spawn inheritance must be restricted, and arguments must contain no secrets.
- **MAC-NET-AC-003 — Platform boundaries:** Native tests use production IPv4 sockets for eligible interfaces, allowed/rejected addresses, bind, stale addresses, ports, and cancellation; exercise production secure-random success and failure paths; and establish canonical manifest equality and unsafe-filesystem rejection. All existing resource, traversal, protocol, and validation bounds remain unchanged.
- **MAC-NET-AC-004 — Package and independence:** Native packaged-application tests exercise the service, supervisor, resolver, dependency closure, and helper/resource lookup independently of working directory, including application paths containing spaces. Verify directory-origin selection and failure independence, MAC-SAVE-001 through MAC-SAVE-005, and network non-persistence. Separate-instance tests must isolate save data rather than accidentally share a Local Play save.
- **MAC-NET-AC-005 — Semantic compatibility and regression:** Native Mac CLI tests consume the same deterministic compatibility, admission, and authoritative fixtures as Linux and Windows. Provenance must include release/content identities, acceptance and rejection outcomes, and semantically equal authoritative results under AHM-AC-018. Relevant Linux, Windows, and Raspberry Pi regressions remain required. This is native fixture/golden comparison, not live cross-OS gameplay evidence.

### Deferred macOS runtime verification

- **MAC-AC-006** On an approved Mac, the user can obtain the nightly artifact, approve and launch the unsigned application from Finder, reach the menu, quit, and relaunch without a resource-path failure.
- **MAC-AC-007** On that Mac, the existing local-player loop works with keyboard and supported controllers, audible sound, combat, pickups, water and elevators, round progression, summaries, and return to menu in Deathmatch, Predator, and Teams.
- **MAC-AC-008** On that Mac, person data survives restart and application replacement without requiring bundle write access.
- **MAC-AC-009** On that Mac, the existing menu, shared arena, and summary presentation remains usable and conforms to the approved UX specifications, preserving full-drawable rendering and pointer alignment.
- **MAC-NET-AC-006 — Deferred graphical and live interoperability:** Using the actual published nightly, the user verifies Mac Host, direct Join, Browse and password flows, owned-player input, round-one admission, progression, summaries, reconnect, and intentional End. Live combinations include Mac host/Linux guest, Linux host/Mac guest, Mac host/Windows guest, Windows host/Mac guest, and separate instances on the same Mac. Actual GUI and visual verification follows the existing UX matrix; separate instances must not accidentally share save data.

The user owns manual Mac verification after merge and the subsequent nightly publication. MAC-AC-006 through MAC-AC-009 and MAC-NET-AC-006, including actual Mac GUI, gameplay, live cross-OS participation, visual assessment, and manual QA, are **deferred, not passed**. Their absence does not block pre-review, experimental implementation acceptance, or merge readiness. Native automated MAC-NET-AC-001 through MAC-NET-AC-005 remain mandatory; other merge gates remain separate. Actual nightly publication evidence for MAC-AC-004 follows publication; pre-merge review assesses its configuration only. No pre-merge claim may state that the unpublished artifact was verified.

Runtime observations must identify the nightly artifact, immutable source revision, Mac architecture, macOS version, and relevant display and input context. Team coordinates any UX evidence within the existing UX-owned matrix. Existing wireframes remain specification inputs; this extension does not require a redesign or an independent duplicate screenshot campaign.

## Execution policy boundary

The approved macOS work permits a narrow native macOS execution exception for building, packaging, testing, and running this target where Docker cannot provide the required macOS environment. DevOps owns the corresponding repository instruction wording through team. The exception does not change the Docker-only policy for other platforms. Product acceptance does not authorize execution by the product role.
