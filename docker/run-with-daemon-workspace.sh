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
copy_dir=""
cleanup() {
  if [[ -n "${validation_dir}" ]]; then
    rm -rf "${validation_dir}"
  fi
  if [[ -n "${copy_dir}" ]]; then
    rm -rf "${copy_dir}"
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

copy_status=0
if copy_dir="$(mktemp -d "${workspace_dir}/.daemon-build-copy-XXXXXX")"; then
  docker cp "${container_id}:/workspace/build" "${copy_dir}/" || copy_status=$?
else
  copy_status=$?
  echo "Unable to create a staging directory for returned build output." >&2
fi

if [[ "${copy_status}" -eq 0 && ! -d "${copy_dir}/build" ]]; then
  echo "Docker copied /workspace/build without creating the expected staging directory." >&2
  copy_status=1
fi

if [[ "${copy_status}" -eq 0 ]]; then
  rm -rf "${workspace_dir:?}/build" || copy_status=$?
  if [[ "${copy_status}" -eq 0 ]]; then
    mv "${copy_dir}/build" "${workspace_dir}/build" || copy_status=$?
  fi
  if [[ "${copy_status}" -ne 0 ]]; then
    echo "Unable to replace the runner build directory with staged container output." >&2
  fi
fi

if [[ "${run_status}" -ne 0 ]]; then
  if [[ "${copy_status}" -ne 0 ]]; then
    echo "Warning: docker cp could not preserve build output from the failed container." >&2
  fi
  exit "${run_status}"
fi

if [[ "${copy_status}" -ne 0 ]]; then
  echo "Failed to copy /workspace/build from the successful build container." >&2
  exit "${copy_status}"
fi
