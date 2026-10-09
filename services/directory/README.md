# Directory container operations

Run these commands from the repository root. Docker transfers the build context
through its API. No checkout bind mount or shared daemon-host path is required.

## Backend verification

```sh
docker build --target verification -t duel6r-directory:verification services/directory
docker run --rm --network none duel6r-directory:verification
```

The verification target contains Node 24, Java 21, Firebase CLI 15.31.0, and the
Firestore emulator downloaded by that CLI. It runs `npm test` through
`firebase emulators:exec`. The CLI starts and stops Firestore and returns the
command result. Tests remain application-owned.

The project is `demo-duel6r-directory`. `NODE_ENV=test` and
`FIRESTORE_EMULATOR_HOST=127.0.0.1:8080` select the local emulator. `PORT=8081`
reserves a separate port for an application server. Do not supply credentials,
mount cloud configuration, or publish emulator ports. Emulator data and logs
are removed with the container. The caller owns container cleanup.
The verification container uses only loopback networking; external access is disabled.
The emulator uses permissive local rules. These checks exercise the server SDK,
not production Firestore Security Rules or IAM.

To check emulator startup without running application tests:

```sh
docker run --rm --network none duel6r-directory:verification 'node --version'
```

Initial image preparation needs access to the base-image registry, Debian and
npm repositories, and the Firebase emulator download service. Application
dependencies use `package-lock.json`; Firebase CLI is version-pinned, but its
transitive dependencies and the Node base tag can change on a fresh build.

## Production image

```sh
docker build --target production -t duel6r-directory:production services/directory
```

This target contains the backend and its production dependencies. It does not
contain Java, Firebase CLI, the emulator, or tests. It runs as the `node` user.
Set `GOOGLE_CLOUD_PROJECT` explicitly. The server listens on `0.0.0.0:$PORT`;
the default port is 8080. Cloud Run terminates HTTPS outside the container.
Use a service identity for cloud access, not embedded credential files.
`D6R_DIRECTORY_FIRESTORE_DATABASE` selects a named database; the default is
`(default)`. The approved cloud services must set their distinct named databases.

The [approved deployment contract](../../docs/network-host-directory.md#directory-deployment)
authorizes public HTTPS directory services in `duel-6-reloaded`, `europe-west1`.
The [deployment runbook](../../docs/directory-cloud-run.md) defines service identities,
named databases, TTL, immutable image promotion, bootstrap, and evidence boundaries.
Emulator success does not validate production IAM, HTTPS ingress, query indexes,
or all Firestore limits and transaction behavior.

Shared application quotas bound admitted operations, not total database costs.
Rejected requests can still incur database work. The approved services use minimum
0, maximum 2, and concurrency 32. These limits are not a hard spending cap or an
abuse shield. No budget alert amount or recipient was approved. Directory deployment
does not authorize gameplay hosting or prove Internet gameplay reachability.

References: [Cloud Run container contract](https://cloud.google.com/run/docs/container-contract),
[Firebase emulator lifecycle](https://firebase.google.com/docs/emulator-suite/install_and_configure),
[Firestore emulator configuration and limits](https://firebase.google.com/docs/emulator-suite/connect_firestore).
