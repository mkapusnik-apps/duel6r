# Workflow Lifecycle

## Feature sanity check

- `Feature - Sanity check` starts for a pull request that targets `develop`.
- The workflow checks out the pull request merge commit on a self-hosted runner.
- The workflow pulls the `develop` Linux build image from GHCR.
- The build compiles the game, runs configured `ctest` tests, and verifies `build/duel6r`.
- The job needs `contents: read` and `packages: read` permissions.
- The job does not create an artifact.
- GitHub cancels an older run for the same pull request when a new run starts.

## Develop sanity

- `Develop - Sanity` starts after a push to `develop`.
- Self-hosted jobs build the checked-out Linux image.
- The sanity job compiles the game, runs configured `ctest` tests, verifies output, and runs the main-menu smoke check.
- The lint-equivalent job performs a Debug compilation and verifies output.
- The tag job moves `sanity` after both build jobs succeed.
- The tag job needs the `PAT_ACTIONS` secret and `contents: write` permission.
- The tag job enables `develop-nightly-scheduler.yml` with the `PAT_ACTIONS` secret.

## Self-hosted Docker workspace contract

- The self-hosted runner may run in a container that uses a Docker daemon on another filesystem namespace.
- The runner checkout path does not have to exist at the same path on the Docker daemon host.
- Self-hosted build steps must not bind mount `$PWD` or `GITHUB_WORKSPACE` into a build container.
- `docker/run-with-daemon-workspace.sh` transfers the checkout through the Docker API with `docker cp`.
- The helper confirms that the daemon-side container contains `/workspace/CMakeLists.txt` before it starts the build.
- The helper copies `/workspace/build` back to `GITHUB_WORKSPACE/build` after the container stops.
- The runner must provide the Docker CLI and access to a Docker daemon.
- The daemon must permit `docker create`, `docker cp`, `docker start`, and `docker rm` operations.
- The runner needs enough local storage for one checkout copy and returned build output.
- A same-path host bind mount is not required.
- Operators may instead use a bind mount only when the daemon can resolve the checkout path to the same repository content.

## Nightly and release paths

- `Develop - Nightly Scheduler` dispatches `develop-nightly.yml` from the `sanity` tag.
- `Develop - Nightly` builds Linux and Windows files on a GitHub-hosted runner.
- The nightly workflow runs configured Linux `ctest` tests and publishes the `nightly` pre-release.
- The nightly tag job needs the `PAT_ACTIONS` secret with `contents: write` access.
- `Release Artifact` builds release files after a push to `master` or a manual dispatch.
- GitHub-hosted jobs use direct bind mounts because their Docker daemon shares the runner host filesystem.
- Nightly and release publication need the permissions and secrets declared in their workflow files.

## Failure handling

- Re-run a failed self-hosted job after Docker daemon access or storage is restored.
- Check for the daemon workspace confirmation before you investigate CMake failures.
- A missing confirmation indicates a checkout transfer or Docker API failure.
- A missing `build/duel6r` after a successful container run indicates an output transfer or packaging failure.
