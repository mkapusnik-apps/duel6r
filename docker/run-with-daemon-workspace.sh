#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${GITHUB_WORKSPACE:-${PWD}}"

if [[ ! -f "${workspace_dir}/CMakeLists.txt" ]]; then
  echo "CMakeLists.txt is missing from runner workspace: ${workspace_dir}" >&2
  exit 1
fi

if [[ "$#" -lt 1 ]]; then
  echo "Usage: $0 [docker create options] IMAGE [COMMAND] [ARG...]" >&2
  exit 2
fi

container_id="$(docker create "$@")"
validation_dir=""
cleanup() {
  if [[ -n "${validation_dir}" ]]; then
    rm -rf "${validation_dir}"
  fi
  docker rm -f "${container_id}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# docker cp transfers through the Docker API. It does not require the daemon to
# resolve the runner container's checkout path on the daemon host.
docker cp "${workspace_dir}/." "${container_id}:/workspace"

validation_dir="$(mktemp -d)"
docker cp "${container_id}:/workspace/CMakeLists.txt" "${validation_dir}/CMakeLists.txt"
test -f "${validation_dir}/CMakeLists.txt"
rm -rf "${validation_dir}"
validation_dir=""
echo "Docker daemon workspace contains /workspace/CMakeLists.txt"

run_status=0
docker start --attach "${container_id}" || run_status=$?

mkdir -p "${workspace_dir}/build"
copy_status=0
docker cp "${container_id}:/workspace/build/." "${workspace_dir}/build" || copy_status=$?

if [[ "${run_status}" -ne 0 ]]; then
  exit "${run_status}"
fi

if [[ "${copy_status}" -ne 0 ]]; then
  echo "Build container did not return /workspace/build" >&2
  exit "${copy_status}"
fi
