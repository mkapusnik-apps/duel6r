# Workflow Overview

GitHub Actions separates pull-request validation, validation of `develop`, nightly publication, and release packaging. The [workflow files](workflows/) are the source of truth for triggers, job dependencies, permissions, and implementation details.

## Workflows

| Workflow | Trigger | Purpose and main elements |
| --- | --- | --- |
| [Feature - Sanity check](workflows/branch.yml) | Pull requests targeting `develop` | Validates the pull-request head with a Linux build and automated tests, plus native Windows transport tests. `Feature Ready` aggregates the two job results. |
| [Develop - Build Container Image](workflows/develop-build-image.yml) | Reusable workflow call or manual dispatch | Publishes Linux and Windows cross-compilation build images to GHCR. Commit-specific images connect validation and nightly packaging to the same source revision; `develop` image tags support consumers of the current development environment. |
| [Develop - Sanity](workflows/develop.yml) | Push to `develop` | Publishes build images, runs a Linux build with automated tests and a main-menu smoke check, and performs a Debug compilation as the lint-equivalent check. Success advances `sanity` and enables the nightly scheduler. |
| [Develop - Nightly Scheduler](workflows/develop-nightly-scheduler.yml) | Every four hours while enabled, or manual dispatch | Requests a nightly build from `sanity` and disables itself until a later successful develop validation enables it again. |
| [Develop - Nightly](workflows/develop-nightly.yml) | Dispatch from the `sanity` tag | Packages Linux and Windows runtime files from the captured validated commit using its matching build images, without rerunning application tests. Publishes the combined ZIP as the current `nightly` release. |
| [Release Artifact](workflows/master-release.yml) | Push to `master` or manual dispatch | Builds and packages a combined Linux and Windows runtime artifact using the `develop` build images. GitHub release asset publication is conditional on a tag-based invocation. |

## Pipeline concept

- Pull-request checks validate proposed changes before integration into `develop`.
- Develop validation establishes the `sanity` checkpoint. Nightly packaging uses that exact source revision and its build images, rather than whichever commit is newest when packaging runs.
- The scheduler separates successful validation from publication. The `nightly` tag and release represent the latest published nightly bundle, not a history of nightly releases. Replacement is non-transactional, so publication can temporarily leave the release unavailable.
- The `master` release-artifact path is separate from nightly publication. It produces a downloadable workflow artifact; a branch push does not itself publish a GitHub release.

## Basic elements and workspace context

- **Containerized execution:** Linux builds and Windows cross-compilation use Docker build environments. Native Windows transport checks exercise the Windows implementation in a native Windows container.
- **Runners and images:** Self-hosted runners handle pull-request Linux validation, develop builds, and nightly work. GitHub-hosted runners support image publication, native Windows checks, scheduling, tagging, and release-artifact builds. GHCR stores the reusable build images.
- **Artifacts and diagnostics:** Runtime archives carry packaged output between jobs or to users. Validation workflows retain available test diagnostics and smoke-check evidence separately from release bundles.
- **Self-hosted Docker workspace contract:** The runner checkout and Docker daemon can occupy different filesystem namespaces, so the checkout path is not assumed to exist on the daemon host. The [workspace helper](../docker/run-with-daemon-workspace.sh) transfers source and build output through the Docker API. This relies on Docker daemon access and storage for the transferred workspace and output; a shared host path is not required.

This overview describes the pipeline's responsibilities and relationships, not procedures for implementing individual steps.
