# Workflow Lifecycle

GitHub Actions is the authoritative hosted CI system. All compilation and application checks execute through the repository's Docker build images; `.travis.yml` is retained only as historical context.

## Feature compilation

`branch.yml` runs for pull requests targeting `develop`. It checks out the pull request merge revision, authenticates to GHCR, always pulls `ghcr.io/<repository>/build:develop`, and compiles the Linux game bundle with application test targets disabled. The container build script fails when compilation or runtime-bundle assembly fails, so no separate output-existence step is needed.

This workflow intentionally does not run the application test suite. Feature branches require successful game compilation; the required test execution remains in the develop sanity and nightly workflows.

## Develop validation and tags

`develop.yml` runs on every push to `develop` and has three responsibilities:

- **Sanity Build** builds the checked-out Linux image, performs a Release build, runs the existing `ctest` suite, and exercises the main menu under Xvfb. Smoke screenshots and logs are uploaded even when the smoke check fails.
- **Lint-equivalent Build** performs the repository's Debug compilation with test targets disabled. There is no independent lint executable in this codebase, so warning-enabled compilation is the current lint and type-safety gate.
- **Tag - sanity** runs only after both gates pass. It moves the `sanity` tag to the validated commit and enables the one-shot nightly scheduler.

`develop-nightly-scheduler.yml` checks hourly while enabled. It dispatches `develop-nightly.yml` at the `sanity` tag and then disables itself. Dispatch happens first so a transient dispatch failure leaves the scheduler enabled for the next attempt.

## Nightly build

`develop-nightly.yml` is dispatch-only and is normally started by the scheduler at `sanity`. It always pulls the current `develop` Linux and Windows cross-build images. The Linux build runs the existing automated tests; the Windows cross-build does not build or run test targets because that suite is already executed natively on Linux and cannot run in the cross-build container.

The two platform builds append their binaries and runtime dependencies to one `build/` bundle. The workflow creates `duel6r-nightly.zip`, uploads it as a workflow artifact, moves the `nightly` tag to the built commit, and updates the `nightly` GitHub pre-release with that exact archive.

## Build image publication

`develop-build-image.yml` runs when build-container inputs change on `develop`, or by manual dispatch. A two-entry matrix builds the Linux and Windows cross-build Dockerfiles and publishes both `develop` and commit-SHA tags:

- `ghcr.io/<repository>/build`
- `ghcr.io/<repository>/build-w64`

The image workflow does not run application tests; its purpose is to publish reproducible toolchains consumed by the other workflows.

## Stable release

`master-release.yml` builds the shared Linux and Windows runtime bundle on pushes to `master`, on the `released` tag, or by manual dispatch. All invocations retain the packaged zip as a workflow artifact. A `released` tag invocation additionally creates or updates the corresponding GitHub release and fails if the archive cannot be found. Release builds disable test targets because the same commit is expected to have passed develop and nightly validation before promotion.

## Credentials and permissions

- The automatic `GITHUB_TOKEN` reads source and GHCR build images, publishes build images from `develop-build-image.yml`, and writes GitHub releases where explicitly permitted.
- The repository secret `PAT` must be able to update Git refs and enable or dispatch Actions workflows. It is used for `sanity` and `nightly` tag movement and scheduler control because those operations may need to trigger downstream automation.
- `mkapusnik-apps/commons/tag@v1` is the shared tag action. The consuming repository must be allowed to resolve that action. In particular, a public repository cannot normally consume an action from a private repository without compatible organization access and Actions settings.

No deployment environment credentials are required.

## Artifacts

- `main-menu-smoke`: screenshots, logs, and image diagnostics from develop sanity runs; absent only when the smoke script cannot create its output directory.
- `duel6r-nightly.zip`: combined Linux and Windows nightly bundle, also attached to the `nightly` pre-release.
- `duel6r-<ref>.zip`: combined stable bundle from `master-release.yml`; attached to the GitHub release for `released` tag runs.

## Troubleshooting

- **Self-hosted jobs do not start or actions fail before a shell step:** confirm the runner is online, has Docker access, and is current enough for Node 24 actions. The checked-in action majors require a modern Actions runner.
- **GHCR pull fails:** confirm the package exists, the workflow has `packages: read`, and its visibility permits this repository's `GITHUB_TOKEN` to download it.
- **Tag or scheduler control fails:** verify `PAT` is present and has repository contents and Actions workflow permissions. Also verify the shared `mkapusnik-apps/commons/tag@v1` action is resolvable.
- **Nightly does not run after develop succeeds:** check that the sanity tag job enabled `develop-nightly-scheduler.yml`. The scheduler intentionally disables itself only after a successful dispatch.
- **Release has no archive:** inspect the build artifact upload and download steps. Release publication uses `fail_on_unmatched_files: true`, so it must fail rather than publish silently without the zip.
