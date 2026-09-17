# Dedicated pilot operations

## Status and boundaries

This configuration implements the infrastructure path for the [public pilot](../docs/network-deployments.md).
It does not establish live availability. Cloud authorization, provisioning, invitation setup, DNS,
certificates, and hosted gameplay observations remain external acceptance work.

Use only project `duel-6-reloaded`. Production and staging each have one non-Spot
`e2-medium` VM in `europe-north2-a`, a separate VPC, static Standard Tier IPv4,
registry repository, runtime identity, deploy identity, and invitation secret.
Production runs continuously after activation. Staging stops after initial provisioning
and each automated deployment. The environments share project administration and quotas,
not invitations or session state. There is no availability SLA or spare VM.

The VM is a trusted single-purpose machine. Do not grant login to untrusted users or
run unrelated containers. Local users can reach the loopback proxy backend and the
VM metadata service. The deploy identity is trusted to replace its environment's
application; it must not be shared with contributors or client players.

## Ports and process contract

| Port/path | Access | Purpose |
| --- | --- | --- |
| TCP 26660 | Public IPv4 | HAProxy TLS 1.2 or later; native game protocol, not HTTP |
| TCP 80 | Public IPv4 | Certbot standalone HTTP-01 challenge only |
| TCP 22 | IAP range only | Authorized OS Login administration |
| 127.0.0.1:26661 | Trusted VM only | Dedicated backend; fixed IPv4 PROXY v2 header, no TLVs |
| `/run/duel6r/runtime/ready.sock` | Private runtime directory | Application readiness |

The server runs as UID/GID 10001 with a read-only root filesystem, no capabilities,
and no Docker socket. The image's default command is:

```text
duel6r-server --dedicated --transport --host=127.0.0.1 --port=26661
  --resources=/opt/duel6r/resources --trusted-proxy-protocol=v2
  --invite-file=/run/secrets/invite --readiness-socket=/run/duel6r/ready.sock
```

The host recreates the private runtime directory on every service replacement.
It supplies a regular, service-owned mode-0400 invitation file, without a newline.
The application owns invitation validation, participant admission, first-player
controller assignment, and all remote session-control authorization.

Readiness has separate gates:

1. `duel6r-server --check-ready=/run/duel6r/ready.sock` exits zero and returns `ready`.
2. The certificate chain, expiry, and configured hostname validate.
3. HAProxy starts and a verified local TLS handshake succeeds.
4. CI performs a verified handshake through the public DNS endpoint.

TLS handshakes do not prove invitation admission or gameplay. The live acceptance
owner must observe those separately with the actual clients.

`SIGTERM` stops the application; Compose allows 10 seconds before forced termination.
Deployment, rollback, invitation rotation, and VM stop end sessions. No session or
reconnect authority is restored. There is no Docker crash restart loop: a replacement
must recreate the socket directory. An operator restarts `duel6r.service` after a crash.

## External prerequisites — separate authorization required

Do not apply Terraform or execute cloud operations merely to validate a repository change.

An authorized project administrator must:

1. Confirm billing and E2/IPv4 capacity in Stockholm.
2. Enable Compute Engine, Artifact Registry, Secret Manager, IAP, OS Login, IAM,
   Service Account Credentials, Security Token Service, and Resource Manager APIs.
3. Provide a private, versioned GCS Terraform-state bucket with restricted operator access.
   CI does not need state access. No secret payload is managed by Terraform.
4. Supply the numeric GitHub repository and owner IDs as Terraform variables.
5. Review the Terraform plan. Its IAM is resource-scoped except for project/operation
   lookup permissions. It creates no project and does not enable APIs.
6. Apply the reviewed configuration. Wait for cloud-init to finish. Staging then powers off.
   Record the infrastructure source SHA and Terraform/provider lock file with the plan.
7. Configure GitHub environments `staging` and `production`. Restrict staging to `develop`
   and production to `master`. Require a production reviewer, prevent self-review, and
   disable administrator bypass. Confirm that the repository plan supports these protections.
8. In each environment, set `WORKLOAD_IDENTITY_PROVIDER` and `DEPLOY_SERVICE_ACCOUNT`
   to the corresponding Terraform outputs. These identifiers are not secrets.

Use a containerized Terraform CLI from the repository root. Example preparation:

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace/deploy/terraform \
  hashicorp/terraform:1.14 init -backend=false
docker run --rm -v "$PWD:/workspace" -w /workspace/deploy/terraform \
  hashicorp/terraform:1.14 validate
```

For an authorized apply, initialize the GCS backend with the approved bucket and
prefix, supply short-lived operator credentials, and review a saved plan before apply.
Do not commit credentials, state, plan files, or private variable files. Retain the
generated provider lock file with the operator's infrastructure release record.
The cloud-init metadata installs the host configuration once. Editing metadata does
not upgrade an existing host. A host-configuration change needs a separately reviewed
operator rollout, with the old configuration retained for rollback.

## Invitation provisioning and rotation

Generate a distinct high-entropy invitation for each environment outside CI. Use at
least 32 random bytes encoded as hexadecimal: 64 printable ASCII characters, no newline.
The application accepts 1–256 ASCII bytes in `!`–`~`; this syntax is not a strength guarantee.

Add the payload through an approved secret-input channel to
`duel6r-staging-invite` or `duel6r-production-invite`. Do not put it in shell history,
process arguments, metadata, GitHub variables, screenshots, or logs. Distribute it
to invitees through a private channel. The VM retrieves `versions/latest` with its
own identity and materializes the payload only under `/run`.

To rotate, add a new version, replace the service, and verify that the new invitation
works and the old invitation fails. This ends the session and invalidates reconnect
credentials. A disabled latest version fails closed; the service does not select an
older version. Rollback of application code does not roll back invitations.

## Manual domain and certificate activation

Initially neither domain is assumed to exist. No plaintext or self-signed fallback is
configured. Missing DNS/certificates must leave activation blocked.

1. Set `duel.netusite.cz` and `staging.duel.netusite.cz` A records to their respective
   Terraform address outputs. Do not publish AAAA records for this IPv4-only pilot.
2. Wait for DNS propagation and confirm any CAA policy permits Let's Encrypt.
3. Start staging if needed. On each VM, an authorized administrator runs Certbot with
   the environment's hostname and an operator-controlled renewal email:

   ```sh
   sudo certbot certonly --standalone --non-interactive --agree-tos \
     --email OPERATOR_EMAIL --cert-name ENVIRONMENT_HOSTNAME -d ENVIRONMENT_HOSTNAME
   ```

4. Keep `/etc/letsencrypt` private and persistent. Certbot's timer renews certificates;
   the deploy hook validates and installs the PEM bundle, then reloads an already
   running HAProxy. It does not start an inactive public listener.
5. Service startup also runs renewal before public activation, including after staging
   has been stopped long enough for a certificate to expire.
6. After cloud, invitation, DNS, certificate, and GitHub approval setup is complete,
   set repository variable `PUBLIC_PILOT_ACTIVATED=true`. This is explicit authorization
   for subsequent branch-driven billable deployment operations. Without it the deployment
   job is skipped, not successful public activation.

Never disable certificate verification to finish activation. An unreadable or invalid
certificate, invitation, image, or readiness socket must fail the deployment.

## CI deployment and rollback

`develop.yml` calls the deployment workflow after its tag job (and therefore after
sanity and Debug compilation). `master-release.yml` calls it after the release artifact
build, which now runs the existing full Linux CTests with a tool image built from that
source. The server image additionally builds and runs the existing headless CTests.
No floating build tag selects the deployed application source.

The production job waits for environment approval. It also refuses activation if the
environment has no required-reviewer rule. WIF limits each deploy identity to the
numeric repository/owner IDs, its branch, and its environment subject. No service-account
key or long-lived SSH key is stored in GitHub. PR jobs have no cloud credentials.

The receiver accepts only `sha256:<64 hex characters> <40-character source SHA>` on stdin.
It pulls only from its root-configured environment repository, checks the source label
and deployment-contract version `1`, and invokes the fixed service unit. The OS Login
user has no Docker-group access and can sudo only this no-argument receiver. CI cannot
upload or execute a privileged script or change root-owned host configuration.

Record the image digest, source SHA, workflow run, and receiver's host-configuration
hashes. `/etc/duel6r/image.env` records the selected candidate, even if activation fails;
it is not proof of success. `previous.env` is only a convenience copy of the preceding
selection, not a guaranteed last-known-good deployment. Use the successful deployment
record for rollback. Keep those image digests in Artifact Registry.

For rollback, manually dispatch `deploy-server.yml` from `master` or `develop`, with
the recorded digest and source revision. Select a previously successful contract-1
artifact from that environment. Production still requires approval. The receiver
replaces the container, secret file, and runtime directory; it does not run old repository
scripts. The existing contract-1 host configuration stays fixed. A rollback across host
contract versions requires the authorized operator to restore the matching recorded host
configuration first. A rollback creates a new session, not old state.

Staging starts for deployment and stops afterward, including when replacement or TLS
validation fails. Failure to stop is a job failure and requires operator action. Production
is not stopped by staging cleanup. A failed production replacement stays failed; there is
no automatic rollback or success claim. Inspect `journalctl -u duel6r -u haproxy` and
the bounded Docker logs without exporting credentials or raw payloads.

To use staging after a deployment, an authorized operator starts `duel6r-staging`
and confirms service/TLS readiness. Stop it after the pilot session. Never stop production
as a staging operation. Production starts the selected service again on VM boot.

## Cost and capacity

Public prices researched on 2026-09-17 selected Stockholm as the cheapest listed European
E2 region. `e2-medium` was $0.035180998/hour versus Belgium $0.036857730 and Frankfurt
$0.043167780. The 4 GiB shared-core VM has only one vCPU-equivalent sustained CPU across
two visible vCPUs. Full 60 Hz/15-player capacity is not established by choosing this size.
Measure pilot behavior; if necessary, approve `e2-standard-2` rather than depend on bursts.

At 730 production hours and 40 staging hours per month, budget approximately USD 36–37
before network traffic, taxes, currency conversion, CI usage, or optional paid services:
compute $27.09, two attached static IPv4 addresses $7.30, two 20 GiB standard disks $1.60,
plus small registry/secret charges. Staging stopped all month still costs approximately
$4.45 for its address and disk. Do not detach a retained address: unused reserved IPv4
costs more. Production remains non-Spot and continuously running.

Standard Tier currently includes 200 GiB/month per billing account, then $0.085/GiB
in the first paid tier. Other projects may use that allowance. Registry storage is
$0.10/GiB-month after its account allowance. No log exporter is installed; Docker logs
are capped at three 10 MiB files. Review and remove obsolete images manually, retaining
deployed and rollback digests. Budget alerts are not spending caps.

Sources: [compute](https://cloud.google.com/products/compute/pricing/general-purpose),
[network/IP](https://cloud.google.com/vpc/network-pricing),
[disks](https://cloud.google.com/compute/disks-image-pricing),
[registry](https://cloud.google.com/artifact-registry/pricing),
[secrets](https://cloud.google.com/secret-manager/pricing).

## Local verification and teardown

Use Docker only for application builds and checks:

```sh
docker build -f Dockerfile.server --build-arg SOURCE_REVISION=FULL_SOURCE_SHA \
  -t duel6r-server:pilot .
docker compose -f deploy/compose.yaml config --no-interpolate
```

The Docker build runs the existing 18 headless CTests at the application checkpoint.
Use existing actionlint, ShellCheck, HAProxy configuration checks, and Terraform
`fmt -check`/`validate` in containers. Backend readiness and a TLS handshake do not
replace encrypted admission, gameplay, isolation, interruption, or rollback observations.

For authorized teardown, first disable `PUBLIC_PILOT_ACTIVATED`, remove DNS records,
stop services, and save required deployment records. Review removal of VM deletion
protection, then review Terraform destroy. This removes disks, static addresses,
repositories and invitation secrets; it does not remove the existing project or operator
state bucket. Delete state only after the authorized teardown and retention review.
