# Workflow Lifecycle

GitHub Actions is the authoritative hosted CI system. All compilation and application checks execute through the repository's Docker build images; `.travis.yml` is retained only as historical context.

## Feature compilation

`branch.yml` runs for pull requests targeting `develop`. It checks out the pull request merge revision, authenticates to GHCR, always pulls `ghcr.io/<repository>/build:develop`, and compiles the Linux game bundle with application test targets disabled. The container build script fails when compilation or runtime-bundle assembly fails, so no separate output-existence step is needed.

This workflow intentionally does not run the application test suite. Feature branches require successful game compilation; the required test execution remains in the develop sanity and nightly workflows.

## Develop validation and tags

`develop.yml` runs on every push to `develop` and has four responsibilities:

- **Sanity Build** builds the checked-out Linux image, performs a Release build, runs the existing `ctest` suite, and exercises the main menu under Xvfb. Smoke screenshots and logs are uploaded even when the smoke check fails.
- **Lint-equivalent Build** performs the repository's Debug compilation with test targets disabled. There is no independent lint executable in this codebase, so warning-enabled compilation is the current lint and type-safety gate.
- **Build images** runs only after both application gates pass. It publishes Linux and Windows cross-build images under the full `sha-<commit>` tag plus the convenience `develop` tag. Both images carry the source commit as an OCI revision label.
- **Tag - sanity** runs only after both gates and both image publications pass. It moves the `sanity` tag to the validated commit and enables the one-shot nightly scheduler. Thus every ordinary sanity commit has a complete pair of commit-bound images before it can be selected for nightly.

`develop-nightly-scheduler.yml` checks hourly while enabled. For each attempt it disables itself before dispatching `develop-nightly.yml` at the `sanity` tag. A failed or cancelled dispatch attempt re-enables the scheduler for retry. The dispatched nightly also re-enables the scheduler when any build, publication, or tag job fails or is cancelled, so a successfully dispatched but unsuccessful downstream run cannot disable scheduling indefinitely. Disabling first prevents the scheduler from overwriting a concurrent enable performed by a newer successful develop run; scheduler runs are not cancelled mid-transition.

## Nightly build

`develop-nightly.yml` is dispatch-only and is normally started by the scheduler at `sanity`. It resolves `build:sha-<sanity commit>` and `build-w64:sha-<sanity commit>`, checks each image's OCI revision, records its pulled SHA-256 digest, and runs only those digest references. The Linux build runs the existing automated tests; the Windows cross-build does not build or run test targets because that suite is already executed natively on Linux and cannot run in the cross-build container.

The two platform builds append their binaries and runtime dependencies to one `build/` bundle. A shared bundle-manifest check requires both executables and each non-empty runtime resource directory before packaging. The workflow creates commit-named archive and provenance assets. The JSON provenance binds repository, commit, tree, both exact image digests, archive name, and archive SHA-256. Generation immediately verifies the sidecar before upload.

Nightly publication stages these immutable, commit-named assets on the existing `nightly` pre-release without replacing the prior canonical assets, verifies their downloaded bytes, and only then moves the `nightly` tag. After the move succeeds, stale nightly assets are removed. A failed upload or tag move therefore leaves the previous tag and its matching assets intact; partial or extra assets remain detectably non-canonical because stable selection uses the current tag's commit-named pair and verifies its provenance. Nightly and stable promotion share one repository concurrency group and do not cancel in-progress transitions, so their canonical mutations cannot overlap.

## Build image publication

The image matrix is part of `develop.yml`, rather than a separate path-filtered workflow, so toolchain publication cannot race independently of sanity selection. Every successful develop commit publishes:

- `ghcr.io/<repository>/build`
- `ghcr.io/<repository>/build-w64`

The image jobs do not run application tests themselves; they depend on the develop sanity and lint-equivalent jobs. These current-repository paths are authoritative after the repository-owner migration; there is no fallback to the historical `ghcr.io/mkapusnik/...` packages. The mutable `develop` tags support feature compilation, while nightly uses only the full commit tags resolved to digests.

## Stable release

`master-release.yml` is manual-dispatch only and must be dispatched from `master`; users do not move `released` beforehand. It never rebuilds release binaries. The workflow resolves current `master` and `nightly`, requires their trees to match, downloads the commit-named archive and provenance selected by `nightly`, validates the archive digest and every provenance field, confirms that both recorded image digests carry the nightly commit revision, extracts with mode-preserving `unzip`, validates the complete bundle, and retains the exact pair as a workflow artifact. This permits a normal develop-to-master merge commit while preventing modified source or mutable build images from bypassing nightly assurance.

Stable publication uses the immutable tag `released-<master commit>`. A draft stages exactly one archive/provenance pair; downloaded asset bytes are compared with the validated nightly inputs before the draft is published. A retry deletes only an incomplete draft, while an already-published immutable release must match exactly and is never modified. Failures before or during staging therefore leave the prior canonical `released` ref and release untouched. A published candidate is unambiguously associated with its commit even if the final canonical ref update fails.

Immediately before canonical promotion, the workflow re-queries remote `master` and `nightly`, verifies the immutable release tag, and rejects any GitHub release attached directly to the moving `released` ref. Only then does it move `released` to the validated master commit as its final operation. The canonical stable release for a `released` ref is therefore deterministically `released-<released commit>`, with exactly one archive/provenance pair. Publication is serialized with nightly and does not cancel in progress. GitHub has no conditional ref-update transaction, so branch/tag protection remains the external control for the narrow race after final ref validation.

## Credentials and permissions

- The automatic `GITHUB_TOKEN` reads source and GHCR build images, publishes build images from `develop.yml`, and writes GitHub releases where explicitly permitted.
- The repository secret `PAT` must be able to update Git refs and enable or dispatch Actions workflows. It is used for `sanity` and `nightly` tag movement and scheduler control because those operations may need to trigger downstream automation.
- `mkapusnik-apps/commons/tag@v1` is the shared tag action. The consuming repository must be allowed to resolve that action. In particular, a public repository cannot normally consume an action from a private repository without compatible organization access and Actions settings.

No deployment environment credentials are required.

## Artifacts

- `main-menu-smoke`: screenshots, logs, and image diagnostics from develop sanity runs; absent only when the smoke script cannot create its output directory.
- `duel6r-nightly-<commit>.zip` and `.provenance.json`: combined Linux and Windows nightly bundle plus its source/image/archive binding. The nightly pair is promoted byte-for-byte to the immutable `released-<master commit>` release selected by the canonical `released` ref.

## Troubleshooting

- **Self-hosted jobs do not start or actions fail before a shell step:** confirm the runner is online, has Docker access, and is current enough for Node 24 actions. The checked-in action majors require a modern Actions runner.
- **GHCR pull fails:** confirm the package exists, the workflow has `packages: read`, and its visibility permits this repository's `GITHUB_TOKEN` to download it.
- **Tag or scheduler control fails:** verify `PAT` is present and has repository contents and Actions workflow permissions. Also verify the shared `mkapusnik-apps/commons/tag@v1` action is resolvable.
- **Nightly does not run after develop succeeds:** check that both build images published successfully and that the sanity tag job enabled `develop-nightly-scheduler.yml`. The scheduler restores itself when dispatch fails or when the dispatched nightly fails or is cancelled.
- **Feature compilation cannot pull a current-namespace build image:** confirm that at least one develop run completed the image matrix and that package visibility permits pull access. Nightly additionally requires the full-SHA image pair for its selected sanity commit.
- **A stable promotion is rejected:** dispatch `Release Artifact` from the current `master` branch after confirming that its tree is unchanged from the successfully published `nightly` tree. Do not move `released` manually. A legacy GitHub release attached directly to `released` must be removed or migrated before this workflow can promote safely.
- **Release has no archive:** inspect the nightly commit-named archive and provenance pair. Stable promotion fails closed when either asset is absent, the archive digest differs, any repository/source/image fact is malformed or mismatched, or the immutable stable release does not contain exactly the verified pair.
