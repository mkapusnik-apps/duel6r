terraform {
  required_version = ">= 1.6, < 2.0"
  required_providers {
    google = { source = "hashicorp/google", version = "~> 7.0" }
  }
  backend "gcs" {}
}

provider "google" {
  project = "duel-6-reloaded"
  region  = "europe-north2"
  zone    = "europe-north2-a"
}

locals {
  environments = {
    staging    = { domain = "staging.duel.netusite.cz", branch = "develop", cidr = "10.61.0.0/24" }
    production = { domain = "duel.netusite.cz", branch = "master", cidr = "10.62.0.0/24" }
  }
  host_files = {
    "/usr/local/sbin/duel6r-deploy"      = "duel6r-deploy"
    "/usr/local/sbin/duel6r-start"       = "duel6r-start"
    "/usr/local/sbin/duel6r-certificate" = "duel6r-certificate"
  }
}

# APIs and billing are operator prerequisites. This root does not enable them.
resource "google_service_account" "runtime" {
  for_each   = local.environments
  account_id = "duel6r-${each.key}-runtime"
}
resource "google_service_account" "deploy" {
  for_each   = local.environments
  account_id = "duel6r-${each.key}-deploy"
}
resource "google_compute_network" "environment" {
  for_each                = local.environments
  name                    = "duel6r-${each.key}"
  auto_create_subnetworks = false
}
resource "google_compute_subnetwork" "environment" {
  for_each      = local.environments
  name          = "duel6r-${each.key}"
  network       = google_compute_network.environment[each.key].id
  ip_cidr_range = each.value.cidr
}
resource "google_compute_firewall" "public" {
  for_each                = local.environments
  name                    = "duel6r-${each.key}-public"
  network                 = google_compute_network.environment[each.key].name
  source_ranges           = ["0.0.0.0/0"]
  target_service_accounts = [google_service_account.runtime[each.key].email]
  allow {
    protocol = "tcp"
    ports    = ["80", "26660"]
  }
}
resource "google_compute_firewall" "iap" {
  for_each                = local.environments
  name                    = "duel6r-${each.key}-iap"
  network                 = google_compute_network.environment[each.key].name
  source_ranges           = ["35.235.240.0/20"]
  target_service_accounts = [google_service_account.runtime[each.key].email]
  allow {
    protocol = "tcp"
    ports    = ["22"]
  }
}
resource "google_compute_address" "environment" {
  for_each     = local.environments
  name         = "duel6r-${each.key}"
  network_tier = "STANDARD"
}
resource "google_artifact_registry_repository" "server" {
  for_each      = local.environments
  location      = "europe-north2"
  repository_id = each.key
  format        = "DOCKER"
}
resource "google_artifact_registry_repository_iam_member" "reader" {
  for_each   = local.environments
  location   = "europe-north2"
  repository = google_artifact_registry_repository.server[each.key].name
  role       = "roles/artifactregistry.reader"
  member     = "serviceAccount:${google_service_account.runtime[each.key].email}"
}
resource "google_artifact_registry_repository_iam_member" "writer" {
  for_each   = local.environments
  location   = "europe-north2"
  repository = google_artifact_registry_repository.server[each.key].name
  role       = "roles/artifactregistry.writer"
  member     = "serviceAccount:${google_service_account.deploy[each.key].email}"
}
resource "google_secret_manager_secret" "invite" {
  for_each  = local.environments
  secret_id = "duel6r-${each.key}-invite"
  replication {
    auto {}
  }
}
resource "google_secret_manager_secret_iam_member" "invite" {
  for_each  = local.environments
  secret_id = google_secret_manager_secret.invite[each.key].id
  role      = "roles/secretmanager.secretAccessor"
  member    = "serviceAccount:${google_service_account.runtime[each.key].email}"
}

resource "google_compute_instance" "server" {
  for_each            = local.environments
  name                = "duel6r-${each.key}"
  machine_type        = "e2-micro"
  deletion_protection = true
  boot_disk {
    initialize_params {
      image = "ubuntu-os-cloud/ubuntu-2404-lts-amd64"
      size  = 20
      type  = "pd-standard"
    }
  }
  network_interface {
    subnetwork = google_compute_subnetwork.environment[each.key].id
    access_config {
      nat_ip       = google_compute_address.environment[each.key].address
      network_tier = "STANDARD"
    }
  }
  service_account {
    email  = google_service_account.runtime[each.key].email
    scopes = ["cloud-platform"]
  }
  shielded_instance_config { enable_secure_boot = true }
  metadata = {
    enable-oslogin         = "TRUE"
    block-project-ssh-keys = "TRUE"
    serial-port-enable     = "FALSE"
    user-data = "#cloud-config\n${yamlencode(merge({
      package_update = true
      packages       = ["docker.io", "docker-compose-v2", "haproxy", "certbot", "curl", "jq", "openssl"]
      bootcmd        = [["cloud-init-per", "once", "mask-haproxy", "systemctl", "mask", "haproxy.service"]]
      write_files = concat([
        for destination, source in local.host_files : {
          path    = destination, permissions = "0755", owner = "root:root",
          content = file("${path.module}/../host/${source}")
        }
        ], [
        { path = "/etc/duel6r/compose.yaml", permissions = "0644", content = file("${path.module}/../compose.yaml") },
        { path = "/etc/haproxy/haproxy.cfg", permissions = "0644", defer = true, content = file("${path.module}/../haproxy.cfg") },
        { path = "/etc/systemd/system/duel6r.service", permissions = "0644", content = file("${path.module}/../host/duel6r.service") },
        { path = "/etc/duel6r/environment", permissions = "0600", content = "ENVIRONMENT=${each.key}\nHOSTNAME_TLS=${each.value.domain}\n" },
        { path = "/etc/sudoers.d/duel6r-deploy", permissions = "0440", content = "sa_${google_service_account.deploy[each.key].unique_id} ALL=(root) NOPASSWD: /usr/local/sbin/duel6r-deploy \"\"\n" },
        { path = "/etc/letsencrypt/renewal-hooks/deploy/duel6r", permissions = "0755", content = "#!/bin/sh\nexec /usr/local/sbin/duel6r-certificate\n" }
      ])
      runcmd = [
        "set -eu",
        "visudo -cf /etc/sudoers.d/duel6r-deploy",
        "systemctl unmask haproxy.service",
        "systemctl disable --now haproxy.service",
        "systemctl daemon-reload",
        "systemctl enable --now docker.service",
        "systemctl enable duel6r.service",
        "systemctl enable --now certbot.timer"
      ]
    }, each.key == "staging" ? { power_state = { mode = "poweroff", delay = "now", condition = true } } : {}))}"
  }
}

resource "google_iam_workload_identity_pool" "github" {
  workload_identity_pool_id = "duel6r-github"
}
resource "google_iam_workload_identity_pool_provider" "github" {
  for_each                           = local.environments
  workload_identity_pool_id          = google_iam_workload_identity_pool.github.workload_identity_pool_id
  workload_identity_pool_provider_id = each.key
  attribute_mapping                  = { "google.subject" = "assertion.sub" }
  attribute_condition                = "assertion.repository_id == '${var.github_repository_id}' && assertion.repository_owner_id == '${var.github_owner_id}' && assertion.ref == 'refs/heads/${each.value.branch}' && assertion.sub == 'repo:mkapusnik-apps/duel6r:environment:${each.key}'"
  oidc { issuer_uri = "https://token.actions.githubusercontent.com" }
}
resource "google_service_account_iam_member" "federation" {
  for_each           = local.environments
  service_account_id = google_service_account.deploy[each.key].name
  role               = "roles/iam.workloadIdentityUser"
  member             = "principal://iam.googleapis.com/${google_iam_workload_identity_pool.github.name}/subject/repo:mkapusnik-apps/duel6r:environment:${each.key}"
}
resource "google_compute_instance_iam_member" "login" {
  for_each      = local.environments
  instance_name = google_compute_instance.server[each.key].name
  zone          = "europe-north2-a"
  role          = "roles/compute.osLogin"
  member        = "serviceAccount:${google_service_account.deploy[each.key].email}"
}
resource "google_iap_tunnel_instance_iam_member" "ssh" {
  for_each = local.environments
  instance = google_compute_instance.server[each.key].name
  zone     = "europe-north2-a"
  role     = "roles/iap.tunnelResourceAccessor"
  member   = "serviceAccount:${google_service_account.deploy[each.key].email}"
  condition {
    title      = "ssh-only"
    expression = "destination.port == 22"
  }
}
resource "google_service_account_iam_member" "login_runtime" {
  for_each           = local.environments
  service_account_id = google_service_account.runtime[each.key].name
  role               = "roles/iam.serviceAccountUser"
  member             = "serviceAccount:${google_service_account.deploy[each.key].email}"
}
resource "google_project_iam_custom_role" "instance_operation" {
  for_each    = local.environments
  role_id     = "duel6r_${each.key}_operation"
  title       = "Duel ${each.key} instance operation"
  permissions = each.key == "staging" ? ["compute.instances.get", "compute.instances.start", "compute.instances.stop"] : ["compute.instances.get"]
}
resource "google_compute_instance_iam_member" "operation" {
  for_each      = local.environments
  instance_name = google_compute_instance.server[each.key].name
  zone          = "europe-north2-a"
  role          = google_project_iam_custom_role.instance_operation[each.key].name
  member        = "serviceAccount:${google_service_account.deploy[each.key].email}"
}
resource "google_project_iam_custom_role" "lookup" {
  role_id     = "duel6rDeploymentLookup"
  title       = "Duel deployment project and operation lookup"
  permissions = ["compute.projects.get", "compute.zones.get", "compute.zoneOperations.get", "resourcemanager.projects.get"]
}
resource "google_project_iam_member" "lookup" {
  for_each = local.environments
  project  = "duel-6-reloaded"
  role     = google_project_iam_custom_role.lookup.name
  member   = "serviceAccount:${google_service_account.deploy[each.key].email}"
}
