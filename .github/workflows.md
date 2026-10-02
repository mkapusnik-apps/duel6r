# Workflow Overview

GitHub Actions separates pull-request validation, validation of `develop`, nightly publication, release packaging, and storage cleanup. The [workflow files](workflows/) are the source of truth for triggers, job dependencies, permissions, and implementation details.

## Workflows

| Workflow | Trigger | Purpose and main elements |
| --- | --- | --- |
| [Feature - Sanity check](workflows/branch.yml) | Pull requests targeting `develop` | Validates the pull-request head with a Linux build and automated tests, containerized directory backend tests against the Firestore emulator, and native Windows transport tests. `Feature Ready` aggregates the two job results. |
| [Linux ARM64 - Pi 5 candidate](workflows/arm64.yml) | Pull requests and pushes to `develop`, or manual dispatch | Native ARM64 Docker compilation, hardware-AES known-answer and existing CTests using the default `gl1` renderer. Uploads a separate architecture-labelled candidate. Software-rendered CI does not certify Pi 5 V3D, peripherals, performance or independent crypto review. |
| [Develop - Build Container Image](workflows/develop-build-image.yml) | Reusable workflow call or manual dispatch | Publishes Linux and Windows cross-compilation build images to GHCR. Commit-specific images connect validation and nightly packaging to the same source revision; `develop` image tags support consumers of the current development environment. |
| [Develop - Sanity](workflows/develop.yml) | Push to `develop` | Publishes build images, runs a Linux build with automated tests and a main-menu smoke check, and performs a Debug compilation as the lint-equivalent check. Success advances `sanity`, enables the nightly scheduler, and permits the gated staging deployment path. |
| [Develop - Nightly Scheduler](workflows/develop-nightly-scheduler.yml) | Every four hours while enabled, or manual dispatch | Requests a nightly build from `sanity` and disables itself until a later successful develop validation enables it again. |
| [Develop - Nightly](workflows/develop-nightly.yml) | Dispatch from the `sanity` tag | Packages Linux and Windows runtime files from the captured validated commit using its matching build images, without rerunning application tests. Publishes the combined ZIP as the current `nightly` release. |
| [Release Artifact](workflows/master-release.yml) | Push to `master` or manual dispatch | Builds its Linux tool image from the checkout, runs the full Linux CTests, and packages Linux and Windows runtime files. Successful packaging permits the gated production deployment path on `master`. GitHub release asset publication is conditional on a tag-based invocation. |
| [Deploy dedicated pilot](workflows/deploy-server.yml) | Reusable workflow call or manual dispatch from `develop` or `master` | Prepares the authorized staging or production deployment. Requires explicit activation; production also requires environment approval. |
| [Storage Cleanup](workflows/storage-cleanup.yml) | Weekly schedule or manual dispatch | Retains current GHCR build images and removes eligible old versions. Manual runs can also remove exact-name legacy Actions artifacts. |

## Pipeline concept

- Pull-request checks validate proposed changes before integration into `develop`.
- Develop validation establishes the `sanity` checkpoint. Nightly packaging uses that exact source revision and its build images, rather than whichever commit is newest when packaging runs.
- The scheduler separates successful validation from publication. The `nightly` tag and release represent the latest published nightly bundle, not a history of nightly releases. Replacement is non-transactional, so publication can temporarily leave the release unavailable.
- The `master` release-artifact path is separate from nightly publication. It produces a downloadable workflow artifact; a branch push does not itself publish a GitHub release.

## Basic elements and workspace context

- **Containerized execution:** Linux builds and Windows cross-compilation use Docker build environments. Native Windows transport checks exercise the Windows implementation in a native Windows container.
- **Runners and images:** Self-hosted runners handle pull-request Linux validation, develop builds, and nightly work. GitHub-hosted runners support image publication, native Windows checks, scheduling, tagging, release-artifact builds, and storage cleanup. GHCR stores the reusable build images.
- **Artifacts and diagnostics:** Validation diagnostics, smoke evidence, and master transport artifacts are retained for seven days; nightly transport remains at one day. Global CTest logs are always preserved after a test failure. Per-test logs and screenshots are collected only for mapped failed tests, and graphical harnesses explicitly record their full-frame screenshot provenance. Only those recorded PNGs are retained, so comparison crops, normalized images, row/control extracts, and other generated derivatives are excluded regardless of filename.
- **Self-hosted Docker workspace contract:** The runner checkout and Docker daemon can occupy different filesystem namespaces, so the checkout path is not assumed to exist on the daemon host. The [workspace helper](../docker/run-with-daemon-workspace.sh) transfers source and build output through the Docker API. This relies on Docker daemon access and storage for the transferred workspace and output; a shared host path is not required.
- **Directory backend:** The Linux pull-request job builds the [verification container](../services/directory/README.md) from a Docker build context, without a checkout bind mount. Firebase CLI runs the application tests against the local Firestore emulator with a demo project and no cloud credentials. No deployment runs in this workflow.
- **Native dependencies:** Pull-request Linux validation builds its tool image from the checkout. Linux, MinGW and native MSVC use the same [pinned private Mbed TLS configuration](../docker/mbedtls/README.md). The native Windows container builds its static dependency after mounted MSVC setup; the existing native CTests remain required by the workflow. Directory HTTPS uses separate libcurl builds. Public TLS uses a separate OpenSSL dependency. Windows bundle packaging follows transitive DLL imports; Mbed TLS is static and its license is included in both bundles.
- **Native public TLS checks:** The native Windows container retains the existing disposable-container opt-in for public TLS CTests. It retains certificate-store guards and cleanup. No host trust store or profile is mounted. Transport checks do not replace an interactive Windows dedicated-session journey.

## Public dedicated pilot authorization

- See [pilot operations](../deploy/README.md) for prerequisites, identity, DNS, certificates, invitations, costs, rollback, and teardown. Repository reconciliation does not authorize live activation.
- Cloud deployment is skipped until an operator sets `PUBLIC_PILOT_ACTIVATED=true`. A skipped deployment is not public readiness.
- Develop calls the staging path after `tag` succeeds. Its runs are not cancelled during service replacement. Deployment jobs serialize each environment separately.
- Master calls the production path after tested release packaging succeeds. Production uses the protected `production` environment and refuses activation without a configured required-reviewer rule.
- Each environment uses its own WIF identity, invitation authority, and session state in project `duel-6-reloaded`. Production and staging remain separate.
- The dedicated image builds from the selected source, runs existing headless CTests, records its source SHA, and deploys by registry digest. Failed builds do not deploy.
- Backend readiness and verified TLS identity are separate checks. Neither proves invitation admission or gameplay.
- Staging starts for deployment and stops afterward, including on activation failure. Manual rollback requires a recorded digest/source pair and the same production approval. Replacement does not restore sessions.

## Storage cleanup safeguards

- Scheduled runs apply GHCR cleanup; manual runs default to a dry run. The workflow fully inventories both packages and resolves `develop`, `sanity`, and `nightly` before deleting anything, then keeps the newest 10 versions plus protected mutable or exact-SHA tags.
- GHCR cleanup needs `contents: read`, `packages: write`, and repository package Admin access to `duel6r/build` and `duel6r/build-w64`. Ref, inventory, metadata, authentication, or authorization failures stop cleanup.
- Legacy Actions artifact cleanup is manual-only, defaults to `skip`, requires `actions: write`, fully paginates the inventory, and deletes only the exact legacy names listed in the workflow. It does not delete current transport artifacts, diagnostics, smoke evidence, Docker build records, or release assets.

This overview describes the pipeline's responsibilities and relationships, not procedures for implementing individual steps.
