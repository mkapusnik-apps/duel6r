#!/usr/bin/env bash
set -euo pipefail

# Real SDL/OpenGL regression coverage for the limited Team game-over summary.
# Each roster has one stationary Alpha team; every other profile walks off its
# isolated spawn block into water, producing a deterministic one-round result.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${build_dir}}"
test_root="${TEST_ROOT:-${build_dir}/final-team-summary}"
display="${DISPLAY:-:93}"
assertions="${workspace_dir}/tests/SharedArenaImageAssertions.py"

fail() { echo "final-team-summary: FAIL: $*" >&2; exit 1; }
for command in Xvfb xdotool import python3 timeout; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done
[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing"
[[ -f "$assertions" ]] || fail "image assertion helper is missing"

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

Xvfb "$display" -screen 0 1280x900x24 +extension GLX +render -noreset \
    >"${test_root}/xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..40}; do
    xdotool getdisplaygeometry >/dev/null 2>&1 && break
    sleep 0.25
done
[[ "$(xdotool getdisplaygeometry)" == "1280 900" ]] || fail "Xvfb was not ready"

write_fixture() {
    local runtime_dir="$1" player_count="$2" team_count="$3"
    python3 - "$runtime_dir" "$player_count" "$team_count" <<'PY'
import json
import os
import shutil
import sys

root, player_count, team_count = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
names = [f"P{i:02d}" for i in range(player_count)]
person = {
    "shots": 0, "hits": 0, "kills": 0, "deaths": 0,
    "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
    "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
    "assistedDamage": 0, "elo": 1000, "eloTrend": 0, "eloGames": 0,
}
with open(os.path.join(root, "data", "persons.json"), "w", encoding="utf-8") as output:
    json.dump({
        "persons": [dict(person, name=name) for name in names],
        "playing": names,
        "rounds": 0,
    }, output)

# Four isolated one-block platforms provide one deterministic spawn layer per
# possible team. Water below kills moving profiles quickly; Alpha profiles stay.
width, height = 13, 7
platform_x = (1, 4, 7, 10)
blocks = []
for stored_y in range(height):
    y = height - stored_y - 1
    for x in range(width):
        if y <= 1:
            blocks.append(4)
        elif y == 3 and x in platform_x[:team_count]:
            blocks.append(1)
        else:
            blocks.append(0)
levels = os.path.join(root, "levels")
for filename in os.listdir(levels):
    if filename.endswith(".json"):
        os.remove(os.path.join(levels, filename))
with open(os.path.join(levels, "qa_final_team.json"), "w", encoding="utf-8") as output:
    json.dump({"width": width, "height": height, "blocks": blocks, "elevators": []}, output)

sample = os.path.join(root, "profiles", "sample")
for index, name in enumerate(names):
    profile = os.path.join(root, "profiles", name)
    os.makedirs(profile, exist_ok=True)
    shutil.copy(os.path.join(sample, "skin.json"), os.path.join(profile, "skin.json"))
    shutil.copy(os.path.join(sample, "sounds.json"), os.path.join(profile, "sounds.json"))
    movement = "" if index % team_count == 0 else "context.player.pressLeft()"
    with open(os.path.join(profile, "script.lua"), "w", encoding="utf-8") as output:
        output.write(
            "function roundStart(context) end\n"
            f"function roundUpdate(context, roundTime) {movement} end\n"
            "function roundEnd(context, roundTime) end\n"
        )
shutil.rmtree(sample)
PY
}

run_scenario() {
    local team_count="$1" player_count="$2"
    local label="team${team_count}-${player_count}"
    local scenario_dir="${test_root}/${label}"
    local runtime_dir="${scenario_dir}/runtime"
    mkdir -p "$runtime_dir"
    cp "${build_dir}/duel6r" "$runtime_dir/"
    for resource in data levels profiles shaders sound textures; do
        cp -a "${resource_dir}/${resource}" "$runtime_dir/"
    done
    write_fixture "$runtime_dir" "$player_count" "$team_count"
    mkdir -p "${scenario_dir}/home"
    (
        export HOME="${scenario_dir}/home"
        export XDG_CACHE_HOME="$HOME/.cache" XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
        cd "$runtime_dir"
        timeout --kill-after=3s 60s ./duel6r "rounds 1" "start_ammo_range 0 0" \
            >"${scenario_dir}/app.stdout" 2>"${scenario_dir}/app.stderr"
    ) &
    app_pid="$!"

    local window_id=""
    for _ in {1..200}; do
        window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        sleep 0.1
    done
    [[ -n "$window_id" ]] || fail "$label application window not found"
    xdotool windowfocus "$window_id" windowactivate "$window_id" >/dev/null 2>&1 || true
    sleep 1

    # Teams is the third primary mode. Advance the team count from its default 2.
    xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
    xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
    for ((count = 2; count < team_count; count++)); do
        xdotool mousemove 1157 244 mousedown 1 sleep 0.08 mouseup 1
    done
    xdotool key --window "$window_id" F1

    local assertion="" ready=false
    for _ in {1..300}; do
        sleep 0.1
        import -window root "${scenario_dir}/final-candidate.png"
        if assertion="$(python3 "$assertions" "${scenario_dir}/final-candidate.png" "$label-final" \
                "$player_count" "$team_count" --final-score \
                --font "${resource_dir}/data/font.ttf" 2>&1)"; then
            cp "${scenario_dir}/final-candidate.png" "${scenario_dir}/final.png"
            printf '%s\n' "$assertion"
            ready=true
            break
        fi
        kill -0 "$app_pid" 2>/dev/null || break
    done
    [[ "$ready" == true ]] || fail "$label final summary assertion failed: $assertion"

    xdotool key --window "$window_id" Shift+Escape
    sleep 1
    xdotool key --window "$window_id" Escape
    set +e
    wait "$app_pid"
    local status="$?"
    set -e
    app_pid=""
    (( status == 0 )) || fail "$label exited with status $status"
    if grep -Eiq 'fatal|segmentation fault|core dumped|error occured|unable to|exception' \
            "${scenario_dir}/app.stderr"; then
        fail "$label emitted fatal-looking stderr"
    fi
}

run_scenario 2 4
run_scenario 3 6
run_scenario 4 15

echo "Final Team summary behavior test passed. Artifacts: ${test_root}"
