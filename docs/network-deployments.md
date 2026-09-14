# Network packages and deployments

## Authority and status

This document defines the product requirements for network packages, deployment instructions, reinstallation, and rollback. It does not establish implemented support or release readiness.

The following documents remain authoritative for their domains:

- The [first-release specification](network-play-first-release.md) owns the platform, hosting, network-environment, and user-journey contracts.
- The [compatibility contract](network-compatibility-and-admission.md) owns protocol, release, capability, and content checks and their exact outcomes.
- The [trust policy](network-trust-and-abuse-limits.md) owns exposure restrictions, diagnostic constraints, and secret handling.
- The [service lifecycle contract](network-host-service-lifecycle.md) owns readiness, shutdown, and cleanup.
- The [Local Play specification](features.md) owns existing local behavior.

This document does not redefine those contracts. The [current networking status](networking.md) remains subject to the experimental/prototype warning. Package availability, successful installation, and successful reinstallation do not prove playable end-to-end networking.

Requirement and acceptance criterion identifiers in this document are stable references.

## Terms

- **Complete package:** One target-platform package with the client, required player-hosted service components, runtime dependencies, and shipped resources.
- **Matching package set:** Complete Linux x86-64 and Windows x86-64 packages that satisfy the authoritative compatibility contract for one network release and gameplay-content set.
- **Local data backup:** A recoverable copy of the user's local people, statistics, profiles, and configuration taken before replacement.
- **Clean installation:** Installation into a location with no previous package files or restored user data.
- **Same-release reinstallation:** Offline replacement with a complete package of the same network release and shipped gameplay-content set on the same supported target platform.
- **Complete rollback:** Offline restoration of a saved previous complete package and that package's own pre-upgrade local data backup.

## Package and deployment requirements

- **NET-DEP-001** The release must provide a complete package for Linux x86-64 and Windows x86-64.
- **NET-DEP-002** Each package must provide all components required for the approved player-hosted deployment without a development checkout.
- **NET-DEP-003** Published instructions must identify each package's target platform, runtime prerequisites, and installation procedure.
- **NET-DEP-004** Published instructions must support separate instances on one trusted machine and direct connection on a trusted private LAN.
- **NET-DEP-005** Published instructions must cover Linux and Windows participants in the same session.
- **NET-DEP-006** Published instructions must identify server components as parts of player-hosted operation, not as a supported dedicated-server deployment.
- **NET-DEP-007** Published instructions must state the transport ports, default port, permitted configuration, and manual firewall requirements.
- **NET-DEP-008** Published instructions must state the trust policy's lack of authentication and encryption and its prohibited network exposures.
- **NET-DEP-009** Published instructions must explain host startup, guest connection, confirmed readiness, cancellation, shutdown, and cleanup under the service lifecycle contract.
- **NET-DEP-010** Published instructions must identify available logs and diagnostics and their safe use under the trust policy.
- **NET-DEP-011** Published instructions must distinguish transport connection, participant admission, and playable-session evidence.
- **NET-DEP-012** A packaged Local Play session must start and complete without requiring or starting a network service.

The supported deployment boundary is player-hosted operation between trusted instances on IPv4 loopback or a trusted private RFC1918 IPv4 LAN. The existing trust policy defines eligible listening addresses and explicit private-interface selection. Packaging must not weaken that boundary.

## Replacement and local data

- **NET-UPG-001** The user must end the network session before package replacement.
- **NET-UPG-002** All participants must stop their game instances and complete owned-service cleanup before package replacement.
- **NET-UPG-003** Each participant must replace the installation with a complete matching package and gameplay-content set before the group starts a new session.
- **NET-UPG-004** Published instructions must prohibit replacement of a running installation.
- **NET-UPG-005** Published instructions must prohibit mixing files from different package releases.
- **NET-UPG-006** Published instructions must require a local data backup before replacement.
- **NET-UPG-007** Same-release reinstallation must let the user restore local people, statistics, profiles, and configuration from that backup.
- **NET-UPG-008** Published instructions must identify the local data to back up and restore without overwriting the backup during replacement.
- **NET-UPG-009** Restored configuration that affects gameplay must satisfy the existing gameplay-content compatibility contract before admission.
- **NET-UPG-010** Published instructions must require verification of the installed package, restored local data, host readiness, guest admission, and clean shutdown after reinstallation.
- **NET-UPG-011** Published instructions must state that replacement starts a new session and does not restore an ended network session or session-only results.
- **NET-UPG-012** Rollback instructions must require the complete previous package and that package's own pre-upgrade local data backup.
- **NET-UPG-013** Rollback instructions must require all participants to stop before replacement and use a matching package set before they start a new session.
- **NET-UPG-014** Rollback instructions must not claim that a previous package can use local data changed by a newer package.
- **NET-UPG-015** Published instructions must state that older network prototypes have no supported migration path.

The replacement order is:

1. End the network session.
2. Stop every participant's game instance and complete owned-service cleanup.
3. Save each previous complete package and its local data backup.
4. Replace each installation with a complete package from the matching package set.
5. Restore applicable local data.
6. Verify the installed packages and restored data.
7. Start a new host session and admit matching guests.
8. Verify clean shutdown.

All installations must be stopped before replacement begins. Once they are stopped, host and guest packages may be replaced in either order. The group must complete replacement and verification before it starts a new session. There is no rolling upgrade.

Restoring local configuration does not exempt gameplay configuration from compatibility checks. A configuration difference can preserve local settings while correctly preventing network admission. Instructions must explain how users restore a common supported gameplay-content set before they connect. Network play must not modify local data to make a mismatch disappear.

The backup-and-restore policy does not authorize a data-format migration from an older network prototype.

## Source-to-target support matrix

The following matrix applies separately to Linux x86-64 and Windows x86-64.

Cross-platform play is supported by the first-release target. Transferring an installation or local data between operating systems is not an approved migration path in this document.

| Source | Target | Supported operation and boundary |
|---|---|---|
| No installation | Complete first-release target package | Clean installation; no prior data migration. |
| Complete target package with local data | Complete package of the same network release and shipped gameplay-content set | Offline same-release reinstallation with backup and restore. |
| Reinstalled target package | Complete saved pre-reinstallation package and its own pre-reinstallation data backup | Offline complete rollback; verify the restored package and data. |
| Older network prototype | First-release target package | No migration support; a clean installation is not evidence of prototype migration. |
| Different network release, including a skipped release | Target package | No approved cross-release upgrade path. |
| Newer package and newer data | Older package without its own pre-upgrade data backup | Unsupported downgrade; not the approved rollback procedure. |
| Running installation or mixed package files | Replacement or partial overlay | Unsupported procedure; stop all instances and use complete packages. |

- **NET-UPG-016** Published instructions must identify every supported source-to-target path from this matrix.
- **NET-UPG-017** Published instructions must explicitly identify unsupported in-place, downgrade, skipped-version, mixed-release, and mixed-content paths.
- **NET-UPG-018** Published instructions must state the enforced protocol version, network release ID, required capabilities, and gameplay-content policy by reference to the compatibility contract.
- **NET-UPG-019** Published instructions must identify compatible client/server combinations by exact network release and gameplay-content set, including both approved operating systems.
- **NET-UPG-020** An incompatible connection must fail with the applicable stable outcome from the compatibility contract.

An offline complete replacement at an existing installation location is not a live in-place update. The instructions must not permit a partial overlay or replacement while any participating instance remains active.

The approved rollback rule does not approve cross-release compatibility or data migration. First-release evidence can demonstrate rollback from same-release reinstallation. It must not claim a demonstrated upgrade from a previous network release that has no approved path.

## Client/server compatibility coverage

The compatibility contract remains the sole authority for compatibility values and checks. Its first-release constants specify admission protocol `1`, network release ID `duel6r-network-r1`, and these required capabilities:

- `d6r.compatibility-admission.v1`
- `d6r.gameplay-manifest.v1`
- `d6r.session-identity.v1`

The network release ID is the only build-compatibility value. No separate build-version check, protocol range, or release range is authorized.

The target supports these operating-system combinations when the client and player-hosted service satisfy that exact contract and have equal canonical gameplay content:

| Player-hosted service | Participant client |
|---|---|
| Linux x86-64 | Linux x86-64 |
| Linux x86-64 | Windows x86-64 |
| Windows x86-64 | Linux x86-64 |
| Windows x86-64 | Windows x86-64 |

A mixed Linux and Windows session may use the matching package set. It must not mix incompatible releases or gameplay content.

An additional capability does not itself cause rejection. Presentation-only assets, profiles, local people, statistics, saves, controls, and documentation remain excluded from gameplay-content compatibility as defined by the compatibility contract.

Unsupported installation procedures do not introduce a new protocol error. Runtime checks reject the incompatibilities they can observe under the existing contracts. This specification does not require an updater, an installation-history detector, or a fabricated downgrade error.

Published instructions must map observable protocol, release, required-capability, invalid-manifest, and gameplay-content failures to the existing stable outcomes. They must not imply that an unsupported overlay is safe merely because its files happen to pass admission checks.

## Release claims

- **NET-DEP-013** Release-facing documentation must limit deployment claims to verified platform, architecture, hosting, network-environment, and replacement paths.
- **NET-DEP-014** Release-facing documentation must retain the experimental/prototype warning until complete end-to-end release validation approves its removal.
- **NET-DEP-015** Release-facing documentation must not describe packaging acceptance as playable-networking or release-readiness acceptance.

The approved target support matrix and the demonstrated evidence matrix are distinct. Missing execution evidence does not remove an approved target from scope. It prevents an acceptance or release claim for the unverified behavior.

Internet support, NAT traversal, port-forwarded exposure, relays, public hosting, dedicated deployment, and changes to Local Play remain excluded. This specification defines no screen, functional state, or visual-design change.

## Acceptance criteria

| Criterion | Required outcome | Requirements |
|---|---|---|
| **NET-DEP-AC-001** | Each approved target has an identifiable complete package. A clean machine can install and start it with only the published prerequisites and instructions. | NET-DEP-001–003 |
| **NET-DEP-AC-002** | Published instructions produce confirmed player-hosted readiness, guest admission, and clean shutdown on each target over same-machine loopback and private LAN. Cross-platform LAN coverage includes each operating system as host. | NET-DEP-004–006, NET-DEP-009, NET-DEP-011 |
| **NET-DEP-AC-003** | Operational documentation matches actual ports, defaults, configuration, diagnostics, exposure restrictions, readiness, cancellation, shutdown, and cleanup. Server-component descriptions do not imply dedicated deployment. | NET-DEP-006–011 |
| **NET-DEP-AC-004** | Packaged Local Play starts and completes with network availability removed and no network service started. | NET-DEP-012 |
| **NET-UPG-AC-001** | On each target, same-release offline reinstallation preserves backed-up local people, statistics, profiles, and configuration after restoration. The group can establish a new session with matching gameplay content and stop cleanly. | NET-UPG-001–011 |
| **NET-UPG-AC-002** | On each target, complete rollback after same-release reinstallation restores the saved prior package and its own prior data backup. Verification confirms the restored data, readiness, admission, and cleanup. | NET-UPG-010–014 |
| **NET-UPG-AC-003** | Documentation enumerates every source-to-target row and exact compatible client/server combination without claiming prototype migration, cross-release upgrade, unsupported downgrade, or skipped-version support. | NET-UPG-015–019 |
| **NET-UPG-AC-004** | Protocol mismatch, release mismatch, missing required capability, invalid manifest, and unequal valid gameplay content produce their canonical stable rejection outcomes. Additional capabilities and excluded local or cosmetic data retain the existing compatibility behavior. Restored gameplay configuration cannot bypass admission checks. | NET-UPG-009, NET-UPG-018–020 |
| **NET-DEP-AC-005** | Package descriptions and release-facing instructions match the demonstrated support matrix and retain the experimental/prototype warning and final release-validation boundary. | NET-DEP-013–015 |

## Acceptance evidence

Every evidence item must identify the immutable checkpoint tip SHA, scenario, observation context, and observed result. Package evidence must also identify the exact artifact and target operating system and architecture.

Reinstallation and rollback evidence must identify source and target artifacts, gameplay-content sets, and backup provenance without disclosing private user data or secrets.

| Evidence owner | Required evidence |
|---|---|
| DevOps | Artifact identities, runtime prerequisites, clean-machine installation and deployment, same-release reinstallation, and complete rollback on both targets. |
| Tester | Independent deployment and replacement smoke results for the approved matrix, including each operating system as the cross-platform host. |
| Reviewer | Documentation-to-runtime traceability, compatibility-policy traceability, source-to-target coverage, and release-claim assessment. |
| Security reviewer | Assessment of defaults, exposed ports, firewall guidance, diagnostics, and secret handling. |
| Product | Criterion-by-criterion assessment of the evidence packet supplied through team. |

Evidence for unsupported upgrade paths must distinguish two cases:

- An observable compatibility violation must demonstrate the existing stable rejection outcome.
- An unsupported installation procedure must have an explicit documentation boundary and reviewer traceability; this specification does not invent a runtime detector for installation history.

Successful same-release reinstallation is not evidence of cross-release upgrade support. Successful rollback to the saved same-release package and its own backup is not evidence that older packages can read newer data.

Compilation, source inspection, transport echo, synthetic admission, and a branch name do not replace packaged production-path observations. Missing target execution evidence remains an acceptance gap. Product does not collect execution evidence.

## Packaging and final release gates

Packaging acceptance covers the acceptance criteria in this document and the applicable existing first-release criteria:

- `NET-AC-001` — approved platforms and cross-platform participation;
- `NET-AC-002` — supported endpoints and network environments;
- `NET-AC-003` — player-hosted deployment only;
- `NET-AC-008` — exact compatibility;
- `NET-AC-015` — Local Play independence;
- `NET-AC-019` — explicit scope and release-claim boundaries.

The existing service readiness, cleanup, trust, and compatibility contracts remain applicable when packaging evidence exercises those behaviors. Packaging work must not redefine them.

Acceptance of packages and operational instructions does not replace complete end-to-end release-candidate validation. The final release gate must approve the complete production path before release-facing text claims playable network support or removes the experimental/prototype warning.
