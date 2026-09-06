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
  test_status=0
  ctest --test-dir "${tmp_build_dir}" --output-on-failure || test_status=$?
  if [[ "${test_status}" -ne 0 ]]; then
    diagnostics_ready=true
    if ! rm -rf "${diagnostics_dir}" || ! mkdir -p "${diagnostics_dir}"; then
      echo "Warning: unable to prepare CTest diagnostics directory: ${diagnostics_dir}" >&2
      diagnostics_ready=false
    fi

    if [[ "${diagnostics_ready}" == true ]]; then
      if [[ -d "${tmp_build_dir}/Testing" ]] \
          && ! cp -R "${tmp_build_dir}/Testing" "${diagnostics_dir}/Testing"; then
        echo "Warning: unable to preserve CTest records." >&2
      fi

      shopt -s globstar nullglob
      for test_output_name in \
        shared-arena-behavior \
        async-menu-background-behavior \
        menu-redesign-behavior \
        round-summary-progress \
        safe-empty-match-start \
        safe-empty-test-failure; do
        test_output_dir="${tmp_build_dir}/${test_output_name}"
        [[ -d "${test_output_dir}" ]] || continue

        for diagnostic_file in \
          "${test_output_dir}"/**/*.png \
          "${test_output_dir}"/**/*.stdout \
          "${test_output_dir}"/**/*.stderr \
          "${test_output_dir}"/**/*.log \
          "${test_output_dir}"/**/*-state.txt \
          "${test_output_dir}"/**/*classifier*.txt \
          "${test_output_dir}"/**/*classification*.txt; do
          relative_file="${diagnostic_file#"${tmp_build_dir}/"}"
          destination_file="${diagnostics_dir}/${relative_file}"
          if ! mkdir -p "$(dirname "${destination_file}")" \
              || ! cp "${diagnostic_file}" "${destination_file}"; then
            echo "Warning: unable to preserve diagnostic file: ${relative_file}" >&2
          fi
        done
      done
      shopt -u globstar nullglob
      echo "Available CTest diagnostics written to ${diagnostics_dir}" >&2
    fi
    exit "${test_status}"
  fi

  rm -rf "${diagnostics_dir}" \
    || echo "Warning: unable to remove stale CTest diagnostics: ${diagnostics_dir}" >&2
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
