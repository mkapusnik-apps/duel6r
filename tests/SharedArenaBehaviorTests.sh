#!/usr/bin/env bash
set -euo pipefail

# Runtime regression harness for the shared-arena multiplayer view. This test is
# intentionally run on demand because it starts the real SDL/OpenGL application
# eight times under Xvfb.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
test_root="${TEST_ROOT:-${build_dir}/shared-arena-behavior}"
display="${DISPLAY:-:98}"

fail() {
    echo "shared-arena-behavior: $*" >&2
    exit 1
}

for command in Xvfb xdotool import compare python3 timeout; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing: ${build_dir}/duel6r"

rm -rf "$test_root"
mkdir -p "$test_root"

export DISPLAY="$display"
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1

xvfb_pid=""
app_pid=""
cleanup() {
    if [[ -n "$app_pid" ]] && kill -0 "$app_pid" >/dev/null 2>&1; then
        kill "$app_pid" >/dev/null 2>&1 || true
        wait "$app_pid" >/dev/null 2>&1 || true
    fi
    if [[ -n "$xvfb_pid" ]] && kill -0 "$xvfb_pid" >/dev/null 2>&1; then
        kill "$xvfb_pid" >/dev/null 2>&1 || true
        wait "$xvfb_pid" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

Xvfb "$display" -screen 0 1280x900x24 +extension GLX +render -noreset >"${test_root}/xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..40}; do
    xdotool getdisplaygeometry >/dev/null 2>&1 && break
    sleep 0.25
done
xdotool getdisplaygeometry >/dev/null 2>&1 || fail "Xvfb did not become ready"

image_distance() {
    local first="$1"
    local second="$2"
    local output
    set +e
    output="$(compare -metric RMSE "$first" "$second" null: 2>&1)"
    local status=$?
    set -e
    (( status <= 1 )) || fail "ImageMagick could not compare $first and $second"
    [[ "$output" =~ \(([0-9.]+)\) ]] || fail "unexpected ImageMagick RMSE output: $output"
    printf '%s' "${BASH_REMATCH[1]}"
}

seed_people() {
    local path="$1"
    local count="$2"
    python3 - "$path" "$count" <<'PY'
import json
import sys

path, count = sys.argv[1], int(sys.argv[2])
names = [f"P{i:02d}" for i in range(1, count + 1)]
person = {
    "shots": 0, "hits": 0, "kills": 0, "deaths": 0,
    "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
    "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
    "assistedDamage": 0, "elo": 1000, "eloTrend": 0, "eloGames": 0,
}
payload = {
    "persons": [dict(person, name=name) for name in names],
    "playing": names,
    "rounds": 0,
}
with open(path, "w", encoding="utf-8") as output:
    json.dump(payload, output)
PY
}

run_scenario() {
    local mode_index="$1"
    local player_count="$2"
    local label="$3"
    local scenario_dir="${test_root}/${label}"
    local runtime_dir="${scenario_dir}/runtime"
    mkdir -p "${runtime_dir}/data"
    cp "${build_dir}/duel6r" "${runtime_dir}/duel6r"
    cp -R "${build_dir}/data/." "${runtime_dir}/data/"
    cp -R "${build_dir}/levels" "${build_dir}/profiles" "${build_dir}/shaders" \
        "${build_dir}/sound" "${build_dir}/textures" "$runtime_dir/"
    seed_people "${runtime_dir}/data/persons.json" "$player_count"
    # Stale saved/startup settings must be inert rather than restoring a
    # player-specific view. They are deliberately unknown after removal.
    python3 - "${runtime_dir}/data/config.script" <<'PY'
import sys

with open(sys.argv[1], "a", encoding="utf-8") as config:
    config.write("\nscreen_mode split\nscreen_zoom 6\n")
PY

    export HOME="${scenario_dir}/home"
    export XDG_CACHE_HOME="${HOME}/.cache"
    export XDG_CONFIG_HOME="${HOME}/.config"
    export XDG_DATA_HOME="${HOME}/.local/share"
    mkdir -p "$HOME"

    (
        cd "$runtime_dir"
        timeout --kill-after=5s 35s ./duel6r "screen_mode split" "screen_zoom 6" \
            >"${scenario_dir}/app.stdout" 2>"${scenario_dir}/app.stderr"
    ) &
    app_pid="$!"

    local window_id=""
    for _ in {1..80}; do
        if ! kill -0 "$app_pid" >/dev/null 2>&1; then
            wait "$app_pid" || fail "$label exited before opening its window"
        fi
        mapfile -t windows < <(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null || true)
        if (( ${#windows[@]} > 0 )); then
            window_id="${windows[0]}"
        fi
        [[ -n "$window_id" ]] && break
        sleep 0.25
    done
    [[ -n "$window_id" ]] || fail "$label window was not found"
    xdotool windowfocus "$window_id" windowactivate "$window_id" 2>/dev/null || true
    sleep 1
    import -window root "${scenario_dir}/menu.png"

    # The mode spinner's right arrow is at GUI (784, 531), translated to the
    # centered 850x700 menu in the 1280x900 test viewport.
    for ((i = 0; i < mode_index; i++)); do
        xdotool mousemove 999 269 mousedown 1 sleep 0.08 mouseup 1
        sleep 0.1
    done

    xdotool key --window "$window_id" F1
    sleep 0.5
    xdotool key --window "$window_id" n
    sleep 5
    import -window root "${scenario_dir}/live-a.png"
    sleep 0.15
    import -window root "${scenario_dir}/live-b.png"

    xdotool key --window "$window_id" F2
    sleep 0.15
    import -window root "${scenario_dir}/after-f2.png"

    # Exercise the same stale activation attempt through the live console.
    xdotool key --window "$window_id" grave
    sleep 0.2
    xdotool type --window "$window_id" --delay 20 "screen_mode split"
    xdotool key --window "$window_id" Return
    sleep 0.2
    xdotool key --window "$window_id" grave
    sleep 0.15
    import -window root "${scenario_dir}/after-console-command.png"

    local baseline_delta f2_delta
    baseline_delta="$(image_distance "${scenario_dir}/live-a.png" "${scenario_dir}/live-b.png")"
    f2_delta="$(image_distance "${scenario_dir}/live-b.png" "${scenario_dir}/after-f2.png")"
    local console_delta
    console_delta="$(image_distance "${scenario_dir}/after-f2.png" "${scenario_dir}/after-console-command.png")"
    python3 - "$baseline_delta" "$f2_delta" "$console_delta" "$label" <<'PY'
import sys

baseline, after_f2, after_console, label = (
    float(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
)
# A split-layout switch repaints most of the frame. Normal simulation movement
# may change the frame, so allow generous drift over the adjacent-frame sample.
limit = min(0.20, max(0.12, baseline * 8 + 0.02))
if after_f2 > limit:
    raise SystemExit(
        f"{label}: F2 caused a layout-sized frame change: {after_f2:.6f} "
        f"(baseline {baseline:.6f}, limit {limit:.6f})"
    )
if after_console > limit:
    raise SystemExit(
        f"{label}: stale console view command caused a layout-sized frame change: "
        f"{after_console:.6f} (baseline {baseline:.6f}, limit {limit:.6f})"
    )
print(
    f"{label}: baseline-rmse={baseline:.6f} f2-rmse={after_f2:.6f} "
    f"console-rmse={after_console:.6f} limit={limit:.6f}"
)
PY

    # Ranking remains an active overlay and Tab still opens the detailed score.
    xdotool key --window "$window_id" F4
    sleep 0.2
    import -window root "${scenario_dir}/ranking-toggled.png"
    xdotool key --window "$window_id" Tab
    sleep 0.2
    import -window root "${scenario_dir}/score-tab.png"
    local ranking_delta score_delta
    ranking_delta="$(image_distance "${scenario_dir}/after-console-command.png" "${scenario_dir}/ranking-toggled.png")"
    score_delta="$(image_distance "${scenario_dir}/ranking-toggled.png" "${scenario_dir}/score-tab.png")"
    python3 - "$ranking_delta" "$score_delta" "$label" <<'PY'
import sys

ranking, score, label = float(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
if ranking < 0.002:
    raise SystemExit(f"{label}: F4 did not visibly toggle the ranking overlay ({ranking:.6f})")
if score < 0.02:
    raise SystemExit(f"{label}: Tab did not visibly open the score overlay ({score:.6f})")
print(f"{label}: ranking-rmse={ranking:.6f} score-tab-rmse={score:.6f}")
PY

    xdotool key --window "$window_id" Shift+Escape
    sleep 0.3
    xdotool key --window "$window_id" Escape
    set +e
    wait "$app_pid"
    local app_status=$?
    set -e
    app_pid=""
    (( app_status == 0 )) || fail "$label exited with status $app_status"
    if grep -Eiq 'fatal|segmentation fault|core dumped|error occured|unable to|exception' "${scenario_dir}/app.stderr"; then
        fail "$label emitted fatal-looking stderr"
    fi
}

# Every selectable mode is exercised. Counts cover the supported minimum,
# historical 2/3/4-way split layouts, intermediate teams, and maximum roster.
run_scenario 0 2 deathmatch-2
run_scenario 1 3 predator-3
run_scenario 2 4 team2-ff-off-4
run_scenario 3 4 team2-ff-on-4
run_scenario 4 6 team3-ff-off-6
run_scenario 5 6 team3-ff-on-6
run_scenario 6 8 team4-ff-off-8
run_scenario 7 15 team4-ff-on-15

echo "Shared arena behavior test passed. Artifacts: ${test_root}"
