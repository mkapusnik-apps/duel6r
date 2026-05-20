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
  ctest --test-dir "${tmp_build_dir}" --output-on-failure
fi

if [[ "${clean_output_dir}" == "ON" ]]; then
  rm -rf "${workspace_dir}/${output_dir}"
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
cp -R "${workspace_dir}/resources/." "${workspace_dir}/${output_dir}/"

echo "Linux runtime bundle written to ${workspace_dir}/${output_dir}"
