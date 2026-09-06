#!/usr/bin/env bash
set -euo pipefail

# Real SDL/OpenGL evidence harness for round-summary progress. It creates a
# deterministic one-platform arena: Predator receives zero starting ammo and
# stays on the platform, while both marines receive bonus ammo and walk into
# the water. The resulting summary therefore has three ranked players and one
# winning predator without changing application code.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${build_dir}}"
test_root="${TEST_ROOT:-${build_dir}/round-summary-progress}"
display="${DISPLAY:-:94}"

fail() {
    echo "round-summary-progress: FAIL: $*" >&2
    exit 1
}

for command in Xvfb xdotool import convert python3; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done
[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing"

rm -rf "$test_root"
mkdir -p "$test_root"
export DISPLAY="$display" SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1

xvfb_pid=""
app_pid=""
tab_held=false
cleanup() {
    if [[ "$tab_held" == true ]]; then
        xdotool keyup Tab >/dev/null 2>&1 || true
        tab_held=false
    fi
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

fail_if_app_exited() {
    local phase="$1"
    [[ -n "$app_pid" ]] || fail "cannot poll $phase without an application process"
    if ! kill -0 "$app_pid" 2>/dev/null; then
        set +e
        wait "$app_pid"
        local status="$?"
        set -e
        app_pid=""
        fail "application exited unexpectedly during $phase (status $status)"
    fi
}

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
        [[ -d "${resource_dir}/${resource}" ]] || fail "runtime resource is missing: $resource"
        cp -a "${resource_dir}/${resource}" "$runtime_dir/"
    done
    write_fixture "$runtime_dir" "$played"
    mkdir -p "${scenario_dir}/home"
    (
        export HOME="${scenario_dir}/home"
        export XDG_CACHE_HOME="$HOME/.cache" XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
        cd "$runtime_dir"
        ./duel6r "rounds $rounds" "start_ammo_range 0 0" \
            >"${scenario_dir}/app.stdout" 2>"${scenario_dir}/app.stderr"
    ) &
    app_pid="$!"
    window_id=""
    for _ in {1..300}; do
        fail_if_app_exited "$label window startup"
        window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        sleep 0.1
    done
    fail_if_app_exited "$label window startup"
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
    if [[ -n "$answer" ]]; then
        local prompt
        if [[ "$rounds" == 0 ]]; then
            prompt="Clear statistics? (Y/N)"
        else
            prompt="Resume previous game? (Y/N)"
        fi
        local prompt_ready=false
        for _ in {1..120}; do
            fail_if_app_exited "$label $prompt prompt"
            import -window root "${scenario_dir}/prompt-candidate.png"
            if python3 - "${workspace_dir}/tests/RoundSummaryProgressImageAssertions.py" \
                    "${scenario_dir}/prompt-candidate.png" "$prompt" \
                    >"${scenario_dir}/prompt-candidate-state.txt" <<'PY'
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("round_assertions", sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
image = module.pixels(sys.argv[2])
matched, diagnostic = module.menu_message_evidence(image, sys.argv[3])
print(diagnostic)
raise SystemExit(0 if matched else 1)
PY
            then
                prompt_ready=true
                break
            fi
            sleep 0.05
        done
        fail_if_app_exited "$label $prompt prompt"
        [[ "$prompt_ready" == true ]] || fail "$label did not render the expected $prompt prompt"
        xdotool key --window "$window_id" "$answer"
    fi
}

hold_active_tab() {
    sleep 0.5
    # Keep the live score state enabled across the winner transition. This is
    # intentionally keydown rather than a completed key press: the regression
    # only occurred when displayScoreTab was still true as the round ended.
    xdotool keydown --window "$window_id" Tab
    tab_held=true
    sleep 0.25
    import -window root "${scenario_dir}/active-tab-held.png"
    cp "${scenario_dir}/active-tab-held.png" "${scenario_dir}/active-tab.png"
}

release_active_tab() {
    xdotool keyup --window "$window_id" Tab
    tab_held=false
}

wait_for_summary() {
    local expected="$1" total="$2"
    # The real physics-driven deaths can take longer under software rendering.
    # Poll persisted application state rather than imposing a process lifetime.
    for _ in {1..300}; do
        fail_if_app_exited "${scenario_dir##*/} round-summary persistence"
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
            local tab_was_held="$tab_held"
            # Persistence is updated when the winner is established. Capture
            # several distinct render opportunities immediately, before the
            # deliberately expensive semantic classifier can consume the
            # six-second summary interval on a busy container.
            local candidate_paths=()
            for index in {1..6}; do
                fail_if_app_exited "${scenario_dir##*/} winner-summary observation"
                candidate_path="$(printf '%s/summary-candidate-%02d.ppm' "$scenario_dir" "$index")"
                # PPM avoids PNG compression consuming the live six-second
                # summary interval under software-rendered container load.
                import -window root "ppm:${candidate_path}"
                candidate_paths+=("$candidate_path")
                if (( index < 6 )); then
                    # Preserve the original observation spacing while keeping
                    # semantic analysis out of the application's live window.
                    sleep 0.15
                fi
            done

            # Complete input and next-round observation while the captured
            # winner state is still live. Delayed analysis can outlast the
            # automatic transition and would make Tab key-up toggle the next
            # round's score panel instead of being ignored by the winner state.
            if [[ "$tab_was_held" == true ]]; then
                release_active_tab
                capture_next_round
            fi

            if python3 - "${workspace_dir}/tests/RoundSummaryProgressImageAssertions.py" \
                    "$expected" "$total" "${resource_dir}/data/font.ttf" \
                    "${scenario_dir}/summary-candidate-state.txt" \
                    "${scenario_dir}/summary-matches.txt" "${candidate_paths[@]}" <<'PY'
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("round_assertions", sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
played, total, font_path = int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
state_path, matches_path = sys.argv[5], sys.argv[6]
candidates = sys.argv[7:]
matches = []
diagnostics = []
for index, path in enumerate(candidates):
    image = module.pixels(path)
    matched, diagnostic = module.summary_state_evidence(image, played, total, font_path)
    diagnostics.append(f"candidate-{index + 1:02d}: {diagnostic}")
    if matched:
        matches.append((index, path))

# Two distinct observations retain the original 0.15-second multi-frame
# persistence assertion without making analysis latency part of the behavior.
pair = next(
    ((first, second) for first in matches for second in matches if second[0] > first[0]),
    None,
)
with open(state_path, "w", encoding="utf-8") as output:
    output.write("\n".join(diagnostics) + "\n")
with open(matches_path, "w", encoding="utf-8") as output:
    if pair:
        output.write(pair[0][1] + "\n" + pair[1][1] + "\n")
raise SystemExit(0 if pair else 1)
PY
            then
                mapfile -t summary_matches <"${scenario_dir}/summary-matches.txt"
            else
                summary_matches=()
            fi
            (( ${#summary_matches[@]} == 2 )) || fail \
                "winner summary did not persist across two matching observations"
            convert "${summary_matches[0]}" "${scenario_dir}/summary-early.png"
            convert "${summary_matches[1]}" "${scenario_dir}/summary-late.png"
            cp "${scenario_dir}/summary-late.png" "${scenario_dir}/summary.png"
            if [[ "$tab_was_held" == true ]]; then
                cp "${scenario_dir}/summary-early.png" \
                    "${scenario_dir}/winner-while-tab-held-early.png"
                cp "${scenario_dir}/summary-late.png" \
                    "${scenario_dir}/winner-while-tab-held-late.png"
            fi
            return
        fi
        sleep 0.1
    done
    fail_if_app_exited "${scenario_dir##*/} round-summary persistence"
    import -window root "${scenario_dir}/summary-timeout.png" || true
    fail "summary for ${scenario_dir} did not reach played-round count $expected"
}

capture_next_round() {
    xdotool key --window "$window_id" F1
    local candidate_paths=()
    for index in {1..6}; do
        fail_if_app_exited "${scenario_dir##*/} next-round rendering"
        candidate_path="$(printf '%s/next-round-candidate-%02d.ppm' "$scenario_dir" "$index")"
        import -window root "ppm:${candidate_path}"
        candidate_paths+=("$candidate_path")
        if (( index < 6 )); then
            sleep 0.15
        fi
    done

    if python3 - "${workspace_dir}/tests/RoundSummaryProgressImageAssertions.py" \
            "${scenario_dir}/next-round-candidate-state.txt" \
            "${scenario_dir}/next-round-matches.txt" "${candidate_paths[@]}" <<'PY'
import importlib.util
import sys

spec = importlib.util.spec_from_file_location("round_assertions", sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
state_path, matches_path = sys.argv[2], sys.argv[3]
matches = []
diagnostics = []
for index, path in enumerate(sys.argv[4:]):
    image = module.pixels(path)
    shown, dark, glyphs = module.has_top_progress(image)
    bands = module.blue_strip_bands(image)
    diagnostics.append(
        f"candidate-{index + 1:02d}: top-progress={shown} dark={dark} glyphs={glyphs} bands={bands}"
    )
    if shown and not bands:
        matches.append((index, path))
pair = next(
    ((first, second) for first in matches for second in matches if second[0] > first[0]),
    None,
)
with open(state_path, "w", encoding="utf-8") as output:
    output.write("\n".join(diagnostics) + "\n")
with open(matches_path, "w", encoding="utf-8") as output:
    if pair:
        output.write(pair[0][1] + "\n" + pair[1][1] + "\n")
raise SystemExit(0 if pair else 1)
PY
    then
        mapfile -t next_round_matches <"${scenario_dir}/next-round-matches.txt"
    else
        next_round_matches=()
    fi
    fail_if_app_exited "${scenario_dir##*/} next-round rendering"
    (( ${#next_round_matches[@]} == 2 )) || fail "next round did not replace the summary frame"
    convert "${next_round_matches[0]}" "${scenario_dir}/next-round-first.png"
    convert "${next_round_matches[1]}" "${scenario_dir}/next-round-settled.png"
}

stop_scenario() {
    fail_if_app_exited "${scenario_dir##*/} shutdown"
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
hold_active_tab
wait_for_summary 1 5
stop_scenario

start_scenario resumed 2 5 y
wait_for_summary 3 5
stop_scenario

start_scenario penultimate 3 5 y
wait_for_summary 4 5
stop_scenario

start_scenario final 4 5 y
wait_for_summary 5 5
stop_scenario

start_scenario unlimited 2 0 n
wait_for_summary 3 0
stop_scenario

python3 "${workspace_dir}/tests/RoundSummaryProgressImageAssertions.py" "$test_root" \
    "${resource_dir}/data/font.ttf"

echo "Round-summary evidence captured at ${test_root}"
