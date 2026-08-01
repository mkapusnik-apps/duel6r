#!/usr/bin/env bash
set -euo pipefail

bundle_kind="${1:-all}"
bundle_dir="${2:-/workspace/build}"

case "${bundle_kind}" in
  linux)
    required_files=(duel6r)
    ;;
  windows)
    required_files=(duel6r.exe)
    ;;
  all)
    required_files=(duel6r duel6r.exe)
    ;;
  *)
    echo "Usage: $0 {linux|windows|all} [bundle-directory]" >&2
    exit 2
    ;;
esac

required_resource_dirs=(data levels profiles shaders sound textures)
missing=0

for file_name in "${required_files[@]}"; do
  file_path="${bundle_dir}/${file_name}"
  if [[ ! -f "${file_path}" ]]; then
    echo "Missing required bundle file: ${file_path}" >&2
    missing=1
  fi
done

if [[ "${bundle_kind}" != "windows" && -f "${bundle_dir}/duel6r" && ! -x "${bundle_dir}/duel6r" ]]; then
  echo "Linux bundle executable is not executable: ${bundle_dir}/duel6r" >&2
  missing=1
fi

for directory_name in "${required_resource_dirs[@]}"; do
  directory_path="${bundle_dir}/${directory_name}"
  if [[ ! -d "${directory_path}" ]]; then
    echo "Missing required resource directory: ${directory_path}" >&2
    missing=1
  elif [[ -z "$(find "${directory_path}" -type f -print -quit)" ]]; then
    echo "Required resource directory is empty: ${directory_path}" >&2
    missing=1
  fi
done

if [[ "${missing}" -ne 0 ]]; then
  exit 1
fi

echo "Validated ${bundle_kind} runtime bundle at ${bundle_dir}"
