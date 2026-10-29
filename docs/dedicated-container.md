# Dedicated game server container

This image runs the actual game executable in Linux dedicated mode. Networking
is experimental; packaging does not establish end-to-end release acceptance.
See the [dedicated inputs and trust limits](dedicated-directory-operations.md#server-inputs).

## Image and channels

Use `ghcr.io/mkapusnik-apps/duel6r/dedicated-server`. The image is **Linux amd64
only** and needs hardware AES. No ARM container is published. It contains the
existing Linux runtime bundle and its libraries, not a compiler. No display,
GPU mount, or audio device is needed.

| Tag | Publication |
| --- | --- |
| `nightly`, `nightly-<full-source-SHA>` | After the existing nightly GitHub release is published from the captured `sanity` source. Uses the staging directory default. |
| `release`, `release-<full-source-SHA>` | After GitHub release assets are published by a tag-invoked `Release Artifact` run. Uses the production directory default. A push to `master` alone does not publish this image. |

There is no `latest` tag. Source tags identify game source, not an immutable
image: rerunning publication can refresh OS libraries. Pin `D6R_SERVER_IMAGE`
to `ghcr.io/mkapusnik-apps/duel6r/dedicated-server@sha256:...` for an exact image.
Use matching client packages and resources. The `build`, `build-arm64`, and
`build-w64` packages are toolchains, not game servers. The directory backend is
a separate service; it does not run matches.

Publication uses the existing workflow token with package write permission.
An operator must permit repository access to the new GHCR package. To allow
anonymous pulls, set the package visibility to public after first publication.
Do not put registry credentials in Compose files. This configuration does not
deploy a gameplay service or change firewall rules.

## Start with Compose

Use a Linux x86-64 Docker Engine with Compose v2. Copy
[`docker-compose.server.yml`](../docker-compose.server.yml) to an operator
directory, or run these commands from the repository root:

```sh
docker compose -f docker-compose.server.yml config
docker compose -f docker-compose.server.yml pull
docker compose -f docker-compose.server.yml up -d --wait
docker compose -f docker-compose.server.yml logs server
```

The default publishes **TCP 127.0.0.1:26660 only**, with no password and no
directory listing. Join directly at `127.0.0.1:26660` from the Docker host.
Other containers on the same Compose network can reach the listener; do not
attach untrusted workloads to that network.

Set these non-secret values in the shell or an operator-owned `.env` beside
the Compose file:

```dotenv
D6R_SERVER_IMAGE=ghcr.io/mkapusnik-apps/duel6r/dedicated-server:nightly
D6R_SERVER_NAME=My dedicated server
D6R_BIND_IP=127.0.0.1
D6R_HOST_PORT=26660
```

For LAN access, set `D6R_BIND_IP` to the Docker host's assigned LAN IPv4, then
run `up -d --wait` again. Guests use that IPv4 and `D6R_HOST_PORT`. The container
always listens at `0.0.0.0:26660`; the published host binding controls external
exposure. An authorized operator must configure any required routing/firewall
rules. Do not use `0.0.0.0` as the host binding unless all-interface exposure is
intended. Docker port publication can bypass some host firewall rules.

The image runs as UID/GID `10001:10001`. Compose mounts `/run/duel6r` as private
temporary memory, cleared when the container stops. Do not replace it with a
persistent volume: the server deliberately refuses an existing readiness
socket. Health uses the application's `--server --check-ready` handshake, not
mere socket-file existence. The healthcheck does not prove external reachability
or directory registration, and it does not restart an unhealthy service.
No session persistence or automatic restart is configured.

## Optional password

Create the password file outside the checkout/image context with a trusted
secret editor. Preserve the exact UTF-8 bytes (at most 128); do not append a
newline unless it belongs to the password. The file must be regular, not a
symlink, owned by the container's effective UID, and inaccessible to group and
other users. For a normal rootful Linux daemon, set numeric owner `10001:10001`
and mode `0600`. With rootless Docker or user-namespace remapping, use the host
UID that maps to container UID 10001. Do not weaken the application's checks.

Uncomment the `--password-file` argument and the read-only bind mount in the
Compose example. Set `source` to the protected file on the **Docker daemon
host**. `create_host_path: false` prevents a missing file from becoming a
directory. Local Compose file-backed secrets do not reliably remap UID/mode;
do not substitute their default world-readable mount. Never put the password
in command arguments, environment variables, image layers, or logs.

An absent file option selects No password; an invalid supplied file fails
startup. A valid empty file also selects No password. Restart/recreate the
container to load a changed password. After an atomic file replacement, use
`up -d --force-recreate --wait` so the bind mount uses the new file.

## Optional directory listing

Uncomment **both** `--advertise-host` and `--advertise-port` arguments. Supply
the real client-reachable gameplay DNS name or eligible non-loopback IPv4 and
the externally reachable port (including any NAT mapping). Do not advertise
the container's private IP, a wildcard, loopback, a URL, or the directory's
domain. Advertisement does not publish a port or configure forwarding.

Without those arguments the service remains Direct only. The compiled directory
channel comes from the bundled game. To override it, uncomment
`D6R_DIRECTORY_URL` and use the intended HTTPS directory origin. Keep certificate
and hostname checks enabled. A listing does not authenticate server identity;
an unlocked connection does not resist active MITM, and a shared password does
not distinguish password holders. Directory failure does not end direct play.

## Inspect, update, and stop

```sh
docker compose -f docker-compose.server.yml ps
docker compose -f docker-compose.server.yml exec server /opt/duel6r/duel6r --server --check-ready=/run/duel6r/ready.sock
docker compose -f docker-compose.server.yml down
```

To update, stop at an operator-approved time, select a matching image/client
version, then `pull` and `up -d --wait`. Active sessions are not retained. Inspect
logs after a failure; no rollback or recovery is automatic.

For a local image, first obtain the existing complete Linux amd64 bundle at
`build/` through the repository's Docker build procedure, then run:

```sh
docker build --platform linux/amd64 -f docker/dedicated/Dockerfile -t duel6r-dedicated:local build
D6R_SERVER_IMAGE=duel6r-dedicated:local docker compose -f docker-compose.server.yml up -d --wait --pull never
```

The Dockerfile consumes that bundle as its build context. It does not rebuild
the game or include Windows binaries. When using `docker run` instead of
Compose, also supply `--init` and
`--tmpfs /run/duel6r:uid=10001,gid=10001,mode=0700` to retain the lifecycle behavior.
