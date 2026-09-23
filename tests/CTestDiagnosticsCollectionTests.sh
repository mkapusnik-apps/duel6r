#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${WORKSPACE_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
fixture_root="$(mktemp -d /tmp/duel6r-diagnostics-test-XXXXXX)"
trap 'rm -rf "$fixture_root"' EXIT

build_dir="${fixture_root}/build"
diagnostics_dir="${fixture_root}/diagnostics"
mkdir -p "${build_dir}/Testing/Temporary" \
    "${build_dir}/menu-redesign-behavior/person-list-refinement" \
    "${build_dir}/async-menu-background-behavior/delayed-success" \
    "${build_dir}/shared-arena-behavior/deathmatch-2"
printf 'complete CTest output\n' >"${build_dir}/Testing/Temporary/LastTest.log"
printf '1:menu-redesign-behavior\n2:async-menu-background-behavior\n' \
    >"${build_dir}/Testing/Temporary/LastTestsFailed.log"

menu_dir="${build_dir}/menu-redesign-behavior/person-list-refinement"
for filename in \
    before-detect-all.png controller-short.png detect-before-0.png detect-after-0.png \
    free-selected-row.png controller-short-right-arrow.png controller-short-value.png \
    dynamic-row-14.png; do
    printf '%s\n' "$filename" >"${menu_dir}/${filename}"
done
printf 'menu log\n' >"${menu_dir}/app.stderr"
cat >"${build_dir}/menu-redesign-behavior/.original-screenshots" <<'EOF'
person-list-refinement/before-detect-all.png
person-list-refinement/controller-short.png
EOF

async_dir="${build_dir}/async-menu-background-behavior/delayed-success"
printf 'initial\n' >"${async_dir}/initial.png"
printf 'derived\n' >"${async_dir}/initial-normalized.png"
printf 'delayed-success/initial.png\n' \
    >"${build_dir}/async-menu-background-behavior/.original-screenshots"

passing_dir="${build_dir}/shared-arena-behavior/deathmatch-2"
printf 'passing screenshot\n' >"${passing_dir}/menu.png"
printf 'passing log\n' >"${passing_dir}/app.stdout"
printf 'deathmatch-2/menu.png\n' >"${build_dir}/shared-arena-behavior/.original-screenshots"

"${workspace_dir}/docker/collect-ctest-diagnostics.sh" "$build_dir" "$diagnostics_dir"

python3 - "$diagnostics_dir" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
actual = sorted(path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file())
expected = [
    "Testing/Temporary/LastTest.log",
    "Testing/Temporary/LastTestsFailed.log",
    "async-menu-background-behavior/delayed-success/initial.png",
    "menu-redesign-behavior/person-list-refinement/app.stderr",
    "menu-redesign-behavior/person-list-refinement/before-detect-all.png",
    "menu-redesign-behavior/person-list-refinement/controller-short.png",
]
if actual != expected:
    raise SystemExit(f"unexpected retained diagnostics:\nactual={actual!r}\nexpected={expected!r}")
print("Retained diagnostic fixture files:")
for path in actual:
    print(path)
PY
