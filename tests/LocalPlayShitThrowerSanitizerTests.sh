#!/usr/bin/env bash
set -euo pipefail

# Focused non-visual Local Play sanitizer regression. Every supported roster size
# constructs all weapon resources (including the Shit Thrower Aseprite-backed
# temporary skin), starts a real match, accepts gameplay input, and exits cleanly.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${build_dir}}"
test_root="${TEST_ROOT:-${build_dir}/local-play-shit-thrower-sanitizer}"
display="${DISPLAY:-:97}"

fail() {
    printf 'local-play-shit-thrower-sanitizer: %s\n' "$*" >&2
    exit 1
}

for command in Xvfb xdotool python3 timeout; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done
[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing"

rm -rf "$test_root"
mkdir -p "$test_root"
export DISPLAY="$display"
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1

Xvfb "$display" -screen 0 1280x900x24 +extension GLX +render -noreset \
    >"${test_root}/xvfb.log" 2>&1 &
xvfb_pid="$!"
app_pid=""
cleanup() {
    if [[ -n "$app_pid" ]] && kill -0 "$app_pid" >/dev/null 2>&1; then
        kill "$app_pid" >/dev/null 2>&1 || true
        wait "$app_pid" >/dev/null 2>&1 || true
    fi
    kill "$xvfb_pid" >/dev/null 2>&1 || true
    wait "$xvfb_pid" >/dev/null 2>&1 || true
}
trap cleanup EXIT
for _ in {1..40}; do
    xdotool getdisplaygeometry >/dev/null 2>&1 && break
    sleep 0.25
done
xdotool getdisplaygeometry >/dev/null 2>&1 || fail "Xvfb did not become ready"

for player_count in $(seq 2 15); do
    scenario_dir="${test_root}/players-${player_count}"
    runtime_dir="${scenario_dir}/runtime"
    mkdir -p "${runtime_dir}/data" "${scenario_dir}/home"
    cp "${build_dir}/duel6r" "${runtime_dir}/duel6r"
    cp -R "${resource_dir}/data/." "${runtime_dir}/data/"
    cp -R "${resource_dir}/levels" "${resource_dir}/profiles" "${resource_dir}/shaders" \
        "${resource_dir}/sound" "${resource_dir}/textures" "$runtime_dir/"
    python3 - "${runtime_dir}/data/persons.json" "$player_count" <<'PY'
import json
import sys

path, count = sys.argv[1], int(sys.argv[2])
person = {
    "shots": 0, "hits": 0, "kills": 0, "deaths": 0, "assistances": 0,
    "wins": 0, "penalties": 0, "games": 0, "timeAlive": 0,
    "totalGameTime": 0, "totalDamage": 0, "assistedDamage": 0,
    "elo": 1000, "eloTrend": 0, "eloGames": 0,
}
names = [f"P{index:02d}" for index in range(1, count + 1)]
with open(path, "w", encoding="utf-8") as output:
    json.dump({"persons": [dict(person, name=name) for name in names],
               "playing": names, "rounds": 0}, output)
PY

    export HOME="${scenario_dir}/home"
    export XDG_CACHE_HOME="${HOME}/.cache"
    export XDG_CONFIG_HOME="${HOME}/.config"
    export XDG_DATA_HOME="${HOME}/.local/share"
    (
        cd "$runtime_dir"
        timeout --kill-after=5s 45s ./duel6r \
            >"${scenario_dir}/app.stdout" 2>"${scenario_dir}/app.stderr"
    ) &
    app_pid="$!"

    window_id=""
    for _ in {1..120}; do
        if ! kill -0 "$app_pid" >/dev/null 2>&1; then
            wait "$app_pid" || true
            fail "${player_count} players exited before opening a window"
        fi
        mapfile -t windows < <(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null || true)
        if (( ${#windows[@]} > 0 )); then window_id="${windows[0]}"; fi
        [[ -n "$window_id" ]] && break
        sleep 0.25
    done
    [[ -n "$window_id" ]] || fail "${player_count} players did not open a window"
    xdotool windowfocus "$window_id" windowactivate "$window_id" 2>/dev/null || true
    xdotool key --window "$window_id" F1
    sleep 0.5
    xdotool key --window "$window_id" n
    sleep 2
    kill -0 "$app_pid" >/dev/null 2>&1 || fail "${player_count} players exited during Local Play startup"

    # Exercise a press/release input cycle so initialized controller state cannot
    # retain a phantom action when the real local match updates.
    xdotool keydown --window "$window_id" Right
    sleep 0.2
    xdotool keyup --window "$window_id" Right
    sleep 0.2
    xdotool key --window "$window_id" Shift+Escape
    sleep 0.3
    xdotool key --window "$window_id" Escape
    set +e
    wait "$app_pid"
    app_status="$?"
    set -e
    app_pid=""
    (( app_status == 0 )) || fail "${player_count} players exited with status ${app_status}"
    [[ ! -s "${scenario_dir}/app.stderr" ]] \
        || fail "${player_count} players emitted sanitizer, SDL, icon, or runtime stderr"
    printf 'local-play-shit-thrower-sanitizer: players=%s passed\n' "$player_count"
done

printf 'local-play-shit-thrower-sanitizer: all roster sizes passed\n'
