#!/usr/bin/env bash
set -euo pipefail

# Real SDL/OpenGL evidence harness for round-summary progress. It creates a
# deterministic one-platform arena: Predator receives zero starting ammo and
# stays on the platform, while both marines receive bonus ammo and walk into
# the water. The resulting summary therefore has three ranked players and one
# winning predator without changing application code.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
test_root="${TEST_ROOT:-${build_dir}/round-summary-progress}"
display="${DISPLAY:-:94}"

fail() {
    echo "round-summary-progress: FAIL: $*" >&2
    exit 1
}

for command in Xvfb xdotool import convert timeout python3; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done
[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing"

rm -rf "$test_root"
mkdir -p "$test_root"
export DISPLAY="$display" SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1

xvfb_pid=""
app_pid=""
cleanup() {
    if [[ -n "$app_pid" ]] && kill -0 "$app_pid" 2>/dev/null; then
        kill "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
    fi
    if [[ -n "$xvfb_pid" ]] && kill -0 "$xvfb_pid" 2>/dev/null; then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

Xvfb "$display" -screen 0 1280x900x24 +extension GLX +render -noreset >"${test_root}/xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..40}; do
    xdotool getdisplaygeometry >/dev/null 2>&1 && break
    sleep 0.25
done
[[ "$(xdotool getdisplaygeometry)" == "1280 900" ]] || fail "Xvfb did not become ready at 1280x900"

write_fixture() {
    local runtime_dir="$1" played="$2"
    python3 - "$runtime_dir" "$played" <<'PY'
import json
import os
import shutil
import sys

root, played = sys.argv[1], int(sys.argv[2])
person = {
    "shots": 0, "hits": 0, "kills": 0, "deaths": 0,
    "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
    "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
    "assistedDamage": 0, "elo": 1000, "eloTrend": 0, "eloGames": 0,
}
names = ["Predator", "MarineA", "MarineB"]
with open(os.path.join(root, "data", "persons.json"), "w", encoding="utf-8") as output:
    json.dump({
        "persons": [dict(person, name=name) for name in names],
        "playing": names,
        "rounds": played,
    }, output, indent=2)

# JSON is stored top row first. Only x=6,y=4 is empty above a wall, so every
# player starts on the same platform. Water is well below the platform so the
# zero-ammo predator remains dry while the moving marines fall into it.
width, height = 13, 9
blocks = []
for stored_y in range(height):
    y = height - stored_y - 1
    row = []
    for x in range(width):
        if y <= 1:
            block = 4
        elif y == 4 and x == 6:
            block = 1
        else:
            block = 0
        row.append(block)
    blocks.extend(row)
levels = os.path.join(root, "levels")
for filename in os.listdir(levels):
    if filename.endswith(".json"):
        os.remove(os.path.join(levels, filename))
with open(os.path.join(levels, "qa_round_summary.json"), "w", encoding="utf-8") as output:
    json.dump({"width": width, "height": height, "blocks": blocks, "elevators": []}, output)

sample = os.path.join(root, "profiles", "sample")
script = '''function roundStart(context) end
function roundUpdate(context, roundTime)
    if context.player.ammo > 0 then context.player.pressLeft() end
end
function roundEnd(context, roundTime) end
'''
for name in names:
    profile = os.path.join(root, "profiles", name)
    os.makedirs(profile, exist_ok=True)
    shutil.copy(os.path.join(sample, "skin.json"), os.path.join(profile, "skin.json"))
    shutil.copy(os.path.join(sample, "sounds.json"), os.path.join(profile, "sounds.json"))
    with open(os.path.join(profile, "script.lua"), "w", encoding="utf-8") as output:
        output.write(script)
shutil.rmtree(sample)
PY
}

start_scenario() {
    local label="$1" played="$2" rounds="$3" answer="$4"
    scenario_dir="${test_root}/${label}"
    runtime_dir="${scenario_dir}/runtime"
    mkdir -p "$runtime_dir"
    cp "${build_dir}/duel6r" "$runtime_dir/"
    for resource in data levels profiles shaders sound textures; do
        [[ -d "${build_dir}/${resource}" ]] || fail "runtime resource is missing: $resource"
        cp -a "${build_dir}/${resource}" "$runtime_dir/"
    done
    write_fixture "$runtime_dir" "$played"
    mkdir -p "${scenario_dir}/home"
    (
        export HOME="${scenario_dir}/home"
        export XDG_CACHE_HOME="$HOME/.cache" XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
        cd "$runtime_dir"
        timeout --kill-after=3s 45s ./duel6r "rounds $rounds" "start_ammo_range 0 0" \
            >"${scenario_dir}/app.stdout" 2>"${scenario_dir}/app.stderr"
    ) &
    app_pid="$!"
    window_id=""
    for _ in {1..300}; do
        window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        sleep 0.1
    done
    [[ -n "$window_id" ]] || fail "$label application window not found"
    xdotool windowfocus "$window_id" windowactivate "$window_id" >/dev/null 2>&1 || true
    sleep 1

    # Select Predator (the second mode), start all maps, and answer the resume
    # or clear-statistics prompt when this scenario has one.
    xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
    sleep 0.2
    # Keep the water below the survivor's platform after the first marine dies.
    xdotool mousemove 973 298 mousedown 1 sleep 0.08 mouseup 1
    sleep 0.2
    xdotool key --window "$window_id" F1
    sleep 0.5
    if [[ -n "$answer" ]]; then
        xdotool key --window "$window_id" "$answer"
    fi
}

capture_active_tab() {
    sleep 0.5
    xdotool key --window "$window_id" Tab
    sleep 0.25
    import -window root "${scenario_dir}/active-tab.png"
    xdotool key --window "$window_id" Tab
}

wait_for_summary() {
    local expected="$1"
    for _ in {1..100}; do
        actual="$(python3 - "${runtime_dir}/data/persons.json" <<'PY'
import json, sys
try:
    with open(sys.argv[1], encoding="utf-8") as source:
        print(json.load(source).get("rounds", -1))
except (OSError, ValueError):
    print(-1)
PY
)"
        if [[ "$actual" == "$expected" ]]; then
            sleep 0.2
            import -window root "${scenario_dir}/summary-early.png"
            sleep 0.8
            import -window root "${scenario_dir}/summary-late.png"
            cp "${scenario_dir}/summary-late.png" "${scenario_dir}/summary.png"
            return
        fi
        sleep 0.1
    done
    import -window root "${scenario_dir}/summary-timeout.png" || true
    fail "summary for ${scenario_dir} did not reach played-round count $expected"
}

capture_next_round() {
    xdotool key --window "$window_id" F1
    import -window root "${scenario_dir}/next-round-first.png"
    sleep 0.2
    import -window root "${scenario_dir}/next-round-settled.png"
}

stop_scenario() {
    xdotool key --window "$window_id" Shift+Escape
    sleep 0.2
    xdotool key --window "$window_id" Escape
    set +e
    wait "$app_pid"
    status="$?"
    set -e
    app_pid=""
    (( status == 0 )) || fail "scenario exited with status $status"
}

start_scenario first 0 5 ""
capture_active_tab
wait_for_summary 1
capture_next_round
stop_scenario

start_scenario resumed 2 5 y
wait_for_summary 3
stop_scenario

start_scenario penultimate 3 5 y
wait_for_summary 4
stop_scenario

start_scenario final 4 5 y
wait_for_summary 5
stop_scenario

start_scenario unlimited 2 0 n
wait_for_summary 3
stop_scenario

python3 "${workspace_dir}/tests/RoundSummaryProgressImageAssertions.py" "$test_root" \
    "${build_dir}/data/font.ttf"

echo "Round-summary evidence captured at ${test_root}"
