# Workflow Lifecycle

## Feature sanity check

- `Feature - Sanity check` starts for a pull request that targets `develop`.
- The workflow checks out the pull request head commit on a self-hosted runner, with the event commit as a safe fallback outside pull request events.
- The workflow pulls the `develop` Linux build image from GHCR.
- The build compiles the game and runs the full configured `ctest` suite.
- The workflow verifies `build/duel6r` after the tests pass.
- The job needs `contents: read` and `packages: read` permissions.
- The job uploads CTest diagnostics only when the container preserves them after a test failure.
- GitHub cancels an older run for the same pull request when a new run starts.

## Develop sanity

- `Develop - Sanity` starts after a push to `develop`.
- The workflow calls `Develop - Build Container Image` before the sanity jobs start.
- The image workflow publishes Linux and Windows images with `sha-<full-commit-SHA>` tags.
- The image workflow also updates the `develop` image tags for a `develop` branch invocation.
- The sanity jobs use the exact Linux image for the pushed commit.
- The sanity job compiles the game and runs the full configured `ctest` suite.
- The sanity job verifies output and runs the main-menu smoke check after the tests pass.
- The sanity job uploads CTest diagnostics only when the container preserves them after a test failure.
- An artifact upload error does not replace the primary test failure.
- The lint-equivalent job performs a Debug compilation and verifies output.
- The tag job moves `sanity` after both build jobs succeed.
- The tag job needs the `PAT_ACTIONS` secret and `contents: write` permission.
- The tag job enables `develop-nightly-scheduler.yml` with the `PAT_ACTIONS` secret.

## Native Windows transport evidence

- `Evidence - Native Windows Transport` starts manually or for relevant pull request changes that target `develop`.
- A pull request run checks out the pull request head commit.
- A manual run checks out the selected workflow commit.
- GitHub runs the job on `windows-2025` with native `ltsc2025` Windows containers.
- The host selects the newest Visual Studio instance with an MSVC x64 toolchain, redistributable runtime, and compatible Windows SDK.
- The host mounts the toolchain, redistributable runtime, and SDK read-only in the container.
- The host does not compile, test, or run a project binary.
- The container uses MSVC x64 to build all targets in the transport-only configuration.
- These targets include the production transport, server, resolver, host supervisor, and registered test executables.
- The container verifies the build tools and required Visual C++ runtime libraries before CMake starts.
- The container puts the Visual C++ runtime libraries beside the native executables before CTest starts.
- The container runs all CTests that the transport-only configuration registers.
- The job needs `contents: read` permission.
- The job does not use repository secrets and does not create an artifact.
- This workflow provides issue acceptance evidence.
- Repository rules do not require this workflow unless an administrator changes those rules.
- GitHub cancels an older run for the same pull request when a new run starts.

## Self-hosted Docker workspace contract

- The self-hosted runner may run in a container that uses a Docker daemon on another filesystem namespace.
- The runner checkout path does not have to exist at the same path on the Docker daemon host.
- Self-hosted build steps must not bind mount `$PWD` or `GITHUB_WORKSPACE` into a build container.
- `docker/run-with-daemon-workspace.sh` transfers the checkout through the Docker API with `docker cp`.
- The helper confirms that the daemon-side container contains `/workspace/CMakeLists.txt` before it starts the build.
- The helper replaces `GITHUB_WORKSPACE/build` with `/workspace/build` after the container stops.
- The helper copies output to a new staging directory before it replaces `GITHUB_WORKSPACE/build`.
- A failed staging copy keeps the prior runner output.
- The helper returns the build container status when the container fails.
- The runner must provide the Docker CLI and access to a Docker daemon.
- The daemon must permit `docker create`, `docker cp`, `docker start`, and `docker rm` operations.
- The runner needs enough local storage for one checkout copy and returned build output.
- A same-path host bind mount is not required.
- Operators may instead use a bind mount only when the daemon can resolve the checkout path to the same repository content.

## Nightly and release paths

- `Develop - Nightly Scheduler` dispatches `develop-nightly.yml` from the `sanity` tag.
- The `sanity` tag identifies the exact source commit that passed `Develop - Sanity`.
- `Develop - Nightly` rejects an invocation when `github.ref` is not `refs/tags/sanity`.
- The workflow captures `github.sha` before a build job starts.
- A later movement of the `sanity` tag does not change the captured commit.
- `Develop - Nightly` builds Linux and Windows files on a self-hosted runner.
- Both nightly builds use the Docker API workspace transfer helper.
- The Windows build receives the Linux output and extends the shared bundle.
- The nightly workflow consumes the exact `sanity` commit and does not rerun application tests.
- Both nightly builds pull `sha-<captured-sanity-SHA>` images.
- The workflow stops before compilation when either exact image is unavailable.
- The workflow packages the shared Linux and Windows files as `duel6r-nightly.zip`.
- The ZIP root contains the files from `build` without a `build` directory.
- GitHub Actions uses a one-day transport artifact between the build and release jobs.
- The repository provides the stable `nightly` tag.
- The release job moves the `nightly` tag to the workflow commit.
- The release job creates the `nightly` release when it does not exist.
- The release job updates the existing `nightly` release in place when it exists.
- The release is a full release and is explicitly the latest repository release.
- The release job uploads only `duel6r-nightly.zip`.
- The release job overwrites an existing asset that has the same file name.
- Publication is non-transactional.
- A failure during asset replacement can leave the release without the new asset.
- Operators can rerun the workflow to recover from a partial publication.
- The release keeps the title `nightly` and does not create a nightly release history.
- A failed build does not change the prior successful nightly release.
- A package failure does not change the prior successful nightly release.
- Nightly runs use `${{ github.workflow }}` as the concurrency group.
- A newer dispatch cancels an active nightly run.
- Cancellation during asset replacement can leave the release without the new asset.
- The release job uses `GITHUB_TOKEN` with `contents: write` to update the release.
- `GITHUB_TOKEN` limits release access to the current repository.
- The release job uses `PAT_ACTIONS` only to move the `nightly` tag.
- The tag token can start workflows that listen for the tag update.
- `Release Artifact` builds release files after a push to `master` or a manual dispatch.
- GitHub-hosted jobs use direct bind mounts because their Docker daemon shares the runner host filesystem.
- Nightly and release publication need the permissions and secrets declared in their workflow files.

## Failure handling

- Check the native Windows job output for the discovered Visual Studio and Windows SDK versions.
- A missing tool path indicates that the `windows-2025` image does not contain a required host tool.
- Exit code `0xC0000135` indicates that Windows cannot load a required runtime library.
- A mount error indicates a Windows Docker bind-mount or path-access failure.
- Re-run a failed self-hosted job after Docker daemon access or storage is restored.
- Check for the daemon workspace confirmation before you investigate CMake failures.
- A missing confirmation indicates a checkout transfer or Docker API failure.
- A missing `build/duel6r` after a successful container run indicates an output transfer or packaging failure.
- A failed CTest run stores available CTest records, screenshots, and classifier or log diagnostics in `build/ci-diagnostics`.
- Diagnostic copy errors do not replace the saved CTest exit status.
