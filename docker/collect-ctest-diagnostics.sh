#!/usr/bin/env bash
set -u

if (( $# != 2 )); then
    echo "Usage: $0 <CTest build directory> <diagnostics directory>" >&2
    exit 2
fi

tmp_build_dir="$1"
diagnostics_dir="$2"
mkdir -p "$diagnostics_dir"
shopt -s globstar nullglob

copy_diagnostic() {
    local diagnostic_file="$1" description="$2"
    local relative_file="${diagnostic_file#"${tmp_build_dir}/"}"
    local destination_file="${diagnostics_dir}/${relative_file}"
    if ! mkdir -p "$(dirname "$destination_file")" \
        || ! cp "$diagnostic_file" "$destination_file"; then
        echo "Warning: unable to preserve ${description}: ${relative_file}" >&2
    fi
}

for diagnostic_file in \
    "${tmp_build_dir}"/Testing/Temporary/LastTest.log \
    "${tmp_build_dir}"/Testing/Temporary/LastTestsFailed.log; do
    [[ -f "$diagnostic_file" ]] && copy_diagnostic "$diagnostic_file" "CTest record"
done

declare -A failed_tests=()
failed_tests_file="${tmp_build_dir}/Testing/Temporary/LastTestsFailed.log"
if [[ -f "$failed_tests_file" ]]; then
    while IFS=: read -r _ test_name; do
        test_name="${test_name%$'\r'}"
        [[ -n "$test_name" ]] && failed_tests["$test_name"]=1
    done <"$failed_tests_file"
else
    echo "Warning: CTest failure list is unavailable; per-test diagnostics cannot be selected safely." >&2
fi

for test_output in \
    shared-arena-behavior:shared-arena-behavior \
    async-menu-background-behavior:async-menu-background-behavior \
    menu-redesign-behavior:menu-redesign-behavior \
    round-summary-progress-behavior:round-summary-progress \
    safe-empty-match-start:safe-empty-match-start \
    safe-empty-match-start:safe-empty-test-failure \
    final-team-summary-behavior:final-team-summary \
    duel6r-local-play-shit-thrower-sanitizer-tests:local-play-shit-thrower-sanitizer; do
    test_name="${test_output%%:*}"
    test_output_name="${test_output#*:}"
    [[ -n "${failed_tests[${test_name}]:-}" ]] || continue

    test_output_dir="${tmp_build_dir}/${test_output_name}"
    [[ -d "$test_output_dir" ]] || continue

    for diagnostic_file in \
        "${test_output_dir}"/**/*.stdout \
        "${test_output_dir}"/**/*.stderr \
        "${test_output_dir}"/**/*.log \
        "${test_output_dir}"/**/*-state.txt \
        "${test_output_dir}"/**/*classifier*.txt \
        "${test_output_dir}"/**/*classification*.txt; do
        copy_diagnostic "$diagnostic_file" "diagnostic file"
    done

    screenshot_manifest="${test_output_dir}/.original-screenshots"
    [[ -f "$screenshot_manifest" ]] || continue
    declare -A recorded_screenshots=()
    while IFS= read -r relative_screenshot; do
        relative_screenshot="${relative_screenshot%$'\r'}"
        case "$relative_screenshot" in
            ""|/*|..|../*|*/../*|*/..) continue ;;
            *.png) ;;
            *) continue ;;
        esac
        [[ -z "${recorded_screenshots[${relative_screenshot}]:-}" ]] || continue
        recorded_screenshots["$relative_screenshot"]=1
        diagnostic_file="${test_output_dir}/${relative_screenshot}"
        [[ -f "$diagnostic_file" ]] || continue
        copy_diagnostic "$diagnostic_file" "full screenshot"
    done <"$screenshot_manifest"
    unset recorded_screenshots
done

shopt -u globstar nullglob
