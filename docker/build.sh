#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${WORKSPACE_DIR:-/workspace}"
output_dir="build"
build_type="${BUILD_TYPE:-Release}"
renderer="${D6R_RENDERER:-gl4}"
with_lua="${D6R_WITH_LUA:-ON}"
build_testing="${BUILD_TESTING:-ON}"
run_tests="${RUN_TESTS:-OFF}"
clean_output_dir="${CLEAN_OUTPUT_DIR:-OFF}"

tmp_build_dir="$(mktemp -d /tmp/duel6r-build-XXXXXX)"
cleanup() {
  rm -rf "${tmp_build_dir}"
}
trap cleanup EXIT

cmake -S "${workspace_dir}" -B "${tmp_build_dir}" \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DBUILD_TESTING="${build_testing}" \
  -DD6R_RENDERER="${renderer}" \
  -DD6R_WITH_LUA="${with_lua}"

cmake --build "${tmp_build_dir}" -j"$(nproc)"

if [[ "${run_tests}" == "ON" ]]; then
  diagnostics_dir="${workspace_dir}/${output_dir}/ci-diagnostics"
  rm -rf "${diagnostics_dir}"
  mkdir -p "${diagnostics_dir}"

  test_status=0
  ctest --test-dir "${tmp_build_dir}" --output-on-failure || test_status=$?
  if [[ "${test_status}" -ne 0 ]]; then
    if [[ -d "${tmp_build_dir}/Testing" ]]; then
      cp -R "${tmp_build_dir}/Testing" "${diagnostics_dir}/Testing"
    fi

    for test_output_name in \
      shared-arena-behavior \
      async-menu-background-behavior \
      menu-redesign-behavior \
      round-summary-progress \
      safe-empty-match-start; do
      test_output_dir="${tmp_build_dir}/${test_output_name}"
      if [[ -d "${test_output_dir}" ]]; then
        cp -R "${test_output_dir}" "${diagnostics_dir}/${test_output_name}"
      fi
    done

    echo "CTest diagnostics written to ${diagnostics_dir}" >&2
    exit "${test_status}"
  fi

  rm -rf "${diagnostics_dir}"
fi

if [[ "${clean_output_dir}" == "ON" ]]; then
  rm -rf "${workspace_dir:?}/${output_dir}"
fi
mkdir -p "${workspace_dir}/${output_dir}"

source_binary="${tmp_build_dir}/duel6r"
if [[ ! -f "${source_binary}" && -f "${tmp_build_dir}/duel6rd" ]]; then
  source_binary="${tmp_build_dir}/duel6rd"
fi

if [[ ! -f "${source_binary}" ]]; then
  echo "Unable to find built application in ${tmp_build_dir}" >&2
  exit 1
fi

cp "${source_binary}" "${workspace_dir}/${output_dir}/duel6r"
if [[ ! -f "${tmp_build_dir}/duel6r-server" ]]; then
  echo "Unable to find built server scaffold in ${tmp_build_dir}" >&2
  exit 1
fi
cp "${tmp_build_dir}/duel6r-server" "${workspace_dir}/${output_dir}/duel6r-server"
if [[ ! -f "${tmp_build_dir}/duel6r-host-supervisor" ]]; then
  echo "Unable to find built host supervisor scaffold in ${tmp_build_dir}" >&2
  exit 1
fi
cp "${tmp_build_dir}/duel6r-host-supervisor" "${workspace_dir}/${output_dir}/duel6r-host-supervisor"
if [[ ! -f "${tmp_build_dir}/duel6r-resolver" ]]; then
  echo "Unable to find built resolver helper in ${tmp_build_dir}" >&2
  exit 1
fi
cp "${tmp_build_dir}/duel6r-resolver" "${workspace_dir}/${output_dir}/duel6r-resolver"
cp -R "${workspace_dir}/resources/." "${workspace_dir}/${output_dir}/"

echo "Linux runtime bundle written to ${workspace_dir}/${output_dir}"
