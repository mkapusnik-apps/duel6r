# Cloud Run directory operations

## Scope and resources

The approved [directory deployment contract](network-host-directory.md#directory-deployment)
owns NET-DIR-DEP-001 through NET-DIR-DEP-013 and their acceptance criteria.
These resources host the HTTP directory, not game sessions, a relay, or player accounts.
Network-capable nightly packages use `https://staging.duel.netusite.cz`; release
packages use `https://duel.netusite.cz`, including manual release-workflow runs.
The build option `D6R_DIRECTORY_DEFAULT_URL` selects this compiled default separately
from `CMAKE_BUILD_TYPE`. Unchannelled builds have an empty default.
An explicit `D6R_DIRECTORY_URL` overrides the compiled default. A present empty or
invalid override makes the directory unavailable, without fallback. A directory
failure must not switch environments. Local Play and direct joining remain
independent. The experimental macOS package remains local-only.

All regional resources use project `duel-6-reloaded` (number `987997960434`) and
`europe-west1`. Identifiers below are public configuration, not credentials.

| Resource | Staging | Production |
| --- | --- | --- |
| Cloud Run service | `staging-directory` | `directory` |
| Public directory origin | `https://staging.duel.netusite.cz` | `https://duel.netusite.cz` |
| Firestore Native database | `staging-directory` | `directory` |
| Runtime service account ID | `d6r-directory-staging-runtime` | `d6r-directory-runtime` |
| Deployment service account ID | `d6r-directory-staging-deploy` | `d6r-directory-prod-deploy` |
| WIF provider ID | `nightly` | `master` |

Service account emails use the suffix `@duel-6-reloaded.iam.gserviceaccount.com`.
The Artifact Registry Docker repository is `directory`; the image path is
`europe-west1-docker.pkg.dev/duel-6-reloaded/directory/directory`.
The global workload identity pool is `d6r-directory`.

Each service uses public HTTPS, port 8080, service minimum 0, service maximum 2,
revision minimum 0, revision maximum 2, and concurrency 32. Runtime environment:

```text
NODE_ENV=production
GOOGLE_CLOUD_PROJECT=duel-6-reloaded
D6R_DIRECTORY_FIRESTORE_DATABASE=<the matching named database>
```

Do not set `FIRESTORE_EMULATOR_HOST` or mount credentials. Cloud Run supplies runtime
credentials. No persistent filesystem volume is required. The databases preserve
leases, ownership hashes, revisions, and shared quotas across container restarts.
TTL is enabled on `directoryListings.leaseExpiry` in each database. TTL deletion
is asynchronous; the application excludes expired records without waiting for it.
Retain the `leaseExpiry` query index. Do not exempt it from indexing for TTL.

## Provisioning and access boundaries

Provisioning uses an authorized operator, not a CI deployment identity. The enabled
APIs are Cloud Run, Artifact Registry, Firestore, IAM, IAM Credentials, and Security
Token Service. No Cloud Build, Secret Manager, service-account key, VPC connector,
load balancer or budget alert is required by this setup. Existing Cloud Run domain
mappings provide the approved public origins. Routine deployment does not recreate
these mappings or change DNS, IAM, or enabled APIs.

The initial inventory after API enablement was empty for Cloud Run services,
registry repositories, Firestore databases, and WIF pools. API activation also
created a default Compute service account; it is not a directory identity.
Never delete or repurpose unrelated resources during provisioning.

| Identity | Grant and scope |
| --- | --- |
| Staging runtime | `roles/datastore.user` at project level with condition `resource.name == 'projects/duel-6-reloaded/databases/staging-directory'` |
| Production runtime | `roles/datastore.user` at project level with condition `resource.name == 'projects/duel-6-reloaded/databases/directory'` |
| Staging deployer | `roles/artifactregistry.writer` on repository `directory`; `roles/run.developer` on service `staging-directory`; `roles/iam.serviceAccountUser` on the staging runtime identity |
| Production deployer | `roles/artifactregistry.reader` on repository `directory`; `roles/run.developer` on service `directory`; `roles/iam.serviceAccountUser` on the production runtime identity |
| Public callers | `roles/run.invoker` on each service; no Firestore access |

CI identities cannot administer IAM, create databases, or enable APIs. Bootstrap
the services from the first approved immutable image before granting service-scoped
deployment access. Do not grant project-wide Cloud Run administration to avoid this step.

### GitHub federation

Both OIDC providers use issuer `https://token.actions.githubusercontent.com` and map
`google.subject=assertion.sub,attribute.ref=assertion.ref`. Both require numeric
repository ID `19414113` and owner ID `305877280` in their attribute conditions.
They also require the exact ref, workflow, and event below:

| Provider | Ref | `assertion.workflow_ref` | Event |
| --- | --- | --- | --- |
| `nightly` | `refs/tags/sanity` | `mkapusnik-apps/duel6r/.github/workflows/develop-nightly.yml@refs/tags/sanity` | `workflow_dispatch` |
| `master` | `refs/heads/master` | `mkapusnik-apps/duel6r/.github/workflows/directory-production.yml@refs/heads/master` | `push` |

The provider condition uses `assertion.repository_id`, `assertion.repository_owner_id`,
`assertion.ref`, `assertion.workflow_ref`, and `assertion.event_name`; all five must
match. Fork PRs and manual production dispatches do not qualify.

Each deployment identity grants `roles/iam.workloadIdentityUser` only to its pool
principal set ending in `attribute.ref/refs/tags/sanity` or
`attribute.ref/refs/heads/master`, respectively. The full prefix is
`principalSet://iam.googleapis.com/projects/987997960434/locations/global/workloadIdentityPools/d6r-directory/`.
Only deployment jobs receive GitHub `id-token: write`. No new GitHub secret or
variable is required. The existing nightly scheduler's `PAT_ACTIONS` is unchanged.

## Custom domains

Both mappings use automatic certificates in `europe-west1`:

| Hostname | Route target | Required DNS record |
| --- | --- | --- |
| `staging.duel.netusite.cz` | `staging-directory` | CNAME to `ghs.googlehosted.com.` |
| `duel.netusite.cz` | `directory` | CNAME to `ghs.googlehosted.com.` |

The operator owns DNS and certificate readiness. A mapping or DNS record alone
does not prove HTTPS access. Before distribution acceptance, confirm the mapping
target and ready certificate, public DNS resolution, and a certificate-validated
`GET /v1/listings` with a listings array through each approved origin. Record the
serving revision and owner-authorization evidence. Do not bypass TLS verification
or substitute a different origin when a domain is unavailable. Certificate issuance
can take up to 24 hours after DNS configuration.

[Google's domain-mapping documentation](https://docs.cloud.google.com/run/docs/mapping-custom-domains)
labels native Cloud Run domain mapping Preview and does not recommend it for
production because of latency issues. This is a retained production risk of the
approved existing mapping, not authorization to add a load balancer.

## Initial service bootstrap

Developer supplies a committed source SHA and a clean source export. Build only
that export with Docker, target `production`, platform `linux/amd64`. Label the
image with `org.opencontainers.image.revision=<source SHA>`. Push it to the registry
and capture its immutable digest. Never use an uncommitted checkout or deploy a
mutable image tag.

An existing approved image can be reused when its production inputs are unchanged.
Record its original build SHA, local image ID, published digest, and equivalent
integrated checkpoint. Do not rebuild only to add a cosmetic source label.

The authorized operator creates staging first with that `IMAGE@sha256:...`, the
runtime environment and limits above, and `--allow-unauthenticated`. Configure an
internal HTTP startup probe on `/healthz`, port 8080, period 10 seconds, timeout
3 seconds, and failure threshold 3. Verify revision startup health, the public HTTPS
listing query, and retained owner authorization. Only a successful
verification may establish `staging-success`. Use a distinct `manual-verified-...`
audit tag for manual bootstrap; do not name it as nightly event evidence.

Bootstrap production from the verified staging digest, with its own database and
runtime identity. Grant the service-level deployment roles from the table after
service creation. Example, with the full deployment account email as `DEPLOYER`:

```sh
gcloud run services add-iam-policy-binding staging-directory \
  --project=duel-6-reloaded --region=europe-west1 \
  --member="serviceAccount:${DEPLOYER}" --role=roles/run.developer
```

Use `directory` and the production deployer for the second grant. Public access is
set by the operator, not changed by routine CI. Bootstrap and manual promotion do
not satisfy the nightly-event or master-push acceptance criteria.

## Nightly staging and production promotion

The nightly directory job waits for successful release publication, including both
game package jobs. It checks out the captured `sanity_sha`, builds the production
target once, and pushes a run-specific tag. It deploys the build output digest to
the `candidate` Cloud Run traffic tag without replacing existing serving traffic.
Deployment waits for the internal `/healthz` startup probe. It checks the public
`/v1/listings` response, moves 100% traffic to that exact revision,
and checks `https://staging.duel.netusite.cz/v1/listings` with TLS verification.
These requests do not print listing contents. The domain check must succeed before
the workflow records staging eligibility.

The final step records `staging-verified-<run ID>-<attempt>` and then moves
`staging-success` to the same digest. The last write is the promotion commit point.
The marker is a mutable pointer, not a deployable image identity. The image content
is immutable. Keep both audit tags and images needed for current service revisions
and rollback. Existing GHCR cleanup does not manage this registry repository.

The production workflow runs only on push to `master`. It resolves `staging-success`
once, under the shared deployment lock, and deploys that digest without a build.
It follows the same candidate verification and traffic activation sequence with
production configuration. After traffic activation, it checks
`https://duel.netusite.cz/v1/listings` with TLS verification before reporting success.
Its `promotion-sha` label identifies the master trigger,
not the image source. The image's OCI revision label and staging run identify source.

Both paths use concurrency group `directory-deployment`, do not cancel an active
deployment, and use `queue: max`. GitHub queues up to 100 pending entries in admission
order, not necessarily event creation order. No queued operation may reread a moving
tag after it has captured its digest. Operators must not deploy concurrently with
this queue. The nightly workflow's existing outer concurrency remains unchanged.

### Failure and rollback

- Failed nightly publication skips the directory job.
- Failed builds, deployments, or verification do not advance `staging-success`.
- A missing marker fails production before deployment. There is no rebuild or fallback.
- Candidate verification failure preserves previous serving traffic. A failure
  after traffic activation can leave the new revision serving; a failed marker
  write leaves the previous image eligible. Inspect actual traffic before recovery.
- A completed marker write defines successful staging eligibility even if a later
  runner cleanup or connection failure changes the workflow conclusion.
- Failed production does not alter the staging marker. No automatic rollback runs.
- A failed candidate traffic-tag URL can remain public. It is not the service URL
  and does not qualify its image for promotion. Operators can remove its tag after
  diagnosis without deleting serving revisions or data.

For operator-approved rollback, direct traffic to a previously verified revision
with `gcloud run services update-traffic SERVICE --to-revisions=REVISION=100`, always
passing `--project=duel-6-reloaded --region=europe-west1`. Do not reset Firestore or
rewrite the eligibility marker merely to make a failed job green. Restaging an older
image must pass verification again before it becomes the next eligible image.

## Evidence, costs, and ownership

Record source SHA, trigger/run, image digest, registry audit tag, Cloud Run revision,
project, region, endpoint, limits, identity, and verification outcomes. Compare the
production image digest with the captured staging digest. GitHub job summaries and
Cloud Run revision metadata support this record. `/healthz` alone does not prove
Firestore IAM, indexes, or owner authorization. During bootstrap, both public
`run.app` URL forms returned a Google-front-end 404 for `/healthz`, while public
`/v1/listings` reached the service. Use the internal HTTP startup probe for process
health and the public listing query for HTTPS/database verification. Do not treat
the public health-path 404 as an application result or add retries to hide it.
Manual evidence is not event evidence.

NET-DIR-DEP-AC-001 and AC-002 need real nightly and master-push observations after
workflow integration. AC-003 also needs hosted listing/owner-authorization evidence
and configuration evidence for isolation and limits. AC-004 does not require a UI
change or new screenshot campaign. Historical deferred Linux/Windows gameplay
evidence remains not executed; directory availability does not establish gameplay
reachability, Internet safety, or overall network-release readiness.

Scale-to-zero can add cold-start latency. Shared quotas, concurrency, and instance
limits do not impose a hard spend ceiling or prevent public denial of service.
Cloud Run can temporarily exceed configured instance limits during platform events.
TTL deletes, Firestore work, registry storage, and logs can incur charges. No budget
amount or recipient was approved, so no budget alerts were created. Do not claim
that billing is capped. Cloud Run request logs can contain caller addresses and URL
paths; never log authorization headers, tokens, or request bodies in application
diagnostics or verification output.

The project operator owns permanent resources, access review, billing, and incident
response. Devops owns task bootstrap and temporary infrastructure cleanup. Team must
release all QA and acceptance consumers before task resources are removed. Do not
reset either database between consumers; use scenario-specific records and remove
only records owned by that scenario. No routine cleanup deletes either database,
service, or a digest needed for promotion or rollback.
