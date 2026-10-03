output "environments" {
  value = {
    for name, settings in local.environments : name => {
      address                    = google_compute_address.environment[name].address
      hostname                   = settings.domain
      workload_identity_provider = google_iam_workload_identity_pool_provider.github[name].name
      service_account            = google_service_account.deploy[name].email
    }
  }
}
