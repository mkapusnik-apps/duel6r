#!/usr/bin/env bash
set -euo pipefail

# Black-box application regression tests for AC-022 through AC-029.
# Run from the repository root after producing the Linux runtime bundle.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${workspace_dir}/resources}"
test_root="${TEST_ROOT:-/tmp/duel6r-safe-empty-match-start}"
display_number="${DISPLAY_NUMBER:-:97}"

fail() {
  if [[ -n "${scenario_dir:-}" && -d "${scenario_dir:-}" && -d "${build_dir:-}" ]]; then
    mkdir -p "${build_dir}/safe-empty-test-failure"
    cp -f "${scenario_dir}/"*.png "${scenario_dir}/"*.stdout "${scenario_dir}/"*.stderr \
      "${build_dir}/safe-empty-test-failure/" 2>/dev/null || true
  fi
  echo "safe-empty-match-start: FAIL: $*" >&2
  exit 1
}

for command in Xvfb xdotool import compare identify timeout python3; do
  command -v "$command" >/dev/null 2>&1 || fail "required command not found: ${command}"
done
[[ -x "${build_dir}/duel6r" ]] || fail "runtime bundle not found at ${build_dir}"
[[ -d "${resource_dir}/data" && -d "${resource_dir}/levels" ]] \
  || fail "runtime resources not found at ${resource_dir}"

rm -rf "$test_root"
mkdir -p "$test_root"
export DISPLAY="$display_number"
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1

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

Xvfb "$DISPLAY" -screen 0 1280x900x24 +extension GLX +render -noreset >"${test_root}/xvfb.log" 2>&1 &
xvfb_pid="$!"
for _ in {1..40}; do
  xdotool getdisplaygeometry >/dev/null 2>&1 && break
  sleep 0.25
done
[[ "$(xdotool getdisplaygeometry)" == "1280 900" ]] || fail "Xvfb did not become ready"

write_people_fixture() {
  local destination="$1"
  python3 - "$destination" <<'PY'
import json
import sys

def person(name, seed):
    return {
        "name": name, "shots": 100 + seed, "hits": 80 + seed,
        "kills": 30 + seed, "deaths": 20 + seed, "assistances": 10 + seed,
        "wins": 7 + seed, "penalties": 2 + seed, "games": 12 + seed,
        "timeAlive": 500 + seed, "totalGameTime": 900 + seed,
        "totalDamage": 2500 + seed, "assistedDamage": 300 + seed,
        "elo": 1200 + seed, "eloTrend": seed, "eloGames": 10 + seed,
    }

with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"persons": [person("Alpha", 1), person("Beta", 2)],
               "playing": ["Alpha", "Beta"], "rounds": 7}, output, indent=2)
PY
}

new_scenario() {
  local name="$1"
  scenario_dir="${test_root}/${name}"
  mkdir -p "$scenario_dir"
  cp "${build_dir}/duel6r" "$scenario_dir/duel6r"
  cp -a "${resource_dir}/." "$scenario_dir/"
  write_people_fixture "${scenario_dir}/data/persons.json"
  cp "${scenario_dir}/data/persons.json" "${scenario_dir}/people-before.json"
}

disable_all_weapons() {
  local config="$1"
  # Keep the first generated command separate even if the shipped script has no
  # trailing newline.
  printf '\n' >>"$config"
  for index in {0..16}; do
    printf 'gun %s false\n' "$index" >>"$config"
  done
}

start_app() {
  local directory="$1"
  shift
  mkdir -p "${directory}/home"
  (
    export HOME="${directory}/home"
    export XDG_CACHE_HOME="${directory}/home/.cache"
    export XDG_CONFIG_HOME="${directory}/home/.config"
    export XDG_DATA_HOME="${directory}/home/.local/share"
    cd "$directory"
    timeout --kill-after=3s 25s ./duel6r "$@" >app.stdout 2>app.stderr
  ) &
  app_pid="$!"
  window_id=""
  for _ in {1..100}; do
    if ! kill -0 "$app_pid" 2>/dev/null; then
      set +e
      wait "$app_pid"
      local status=$?
      set -e
      app_pid=""
      fail "application exited before its window appeared (status ${status}) in ${directory}"
    fi
    window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
    [[ -n "$window_id" ]] && break
    sleep 0.1
  done
  [[ -n "$window_id" ]] || fail "application window not found in ${directory}"
  xdotool windowfocus "$window_id" >/dev/null 2>&1 || true
  xdotool windowactivate "$window_id" >/dev/null 2>&1 || true
  sleep 3
}

capture() {
  import -window root "$1"
  [[ "$(identify -format '%wx%h' "$1")" == "1280x900" ]] || fail "invalid screenshot $1"
}

changed_pixels() {
  local first="$1" second="$2" output="$3"
  local status metric
  set +e
  compare -metric AE "$first" "$second" "${output}.png" 2>"${output}.txt"
  status=$?
  set -e
  (( status <= 1 )) || fail "ImageMagick could not compare $first and $second"
  metric="$(<"${output}.txt")"
  metric="$(python3 - "$metric" <<'PY'
import sys
print(int(float(sys.argv[1])))
PY
)"
  [[ "$metric" =~ ^[0-9]+$ ]] || fail "non-numeric image difference: $metric"
  printf '%s' "$metric"
}

assert_changed() {
  local first="$1" second="$2" minimum="$3" label="$4" pixels
  pixels="$(changed_pixels "$first" "$second" "${second%.png}-diff")"
  (( pixels >= minimum )) || fail "$label did not change enough pixels (${pixels}, expected >= ${minimum})"
}

assert_same() {
  local first="$1" second="$2" label="$3" pixels
  pixels="$(changed_pixels "$first" "$second" "${second%.png}-diff")"
  (( pixels == 0 )) || fail "$label changed ${pixels} pixels"
}

assert_report_width() {
  local screenshot="$1" expected_message="$2" label="$3"
  local expected_width=$(( ${#expected_message} * 8 + 60 ))
  local left=$((640 - expected_width / 2))
  local right=$((left + expected_width - 1))
  local center_y=450
  local left_pixel right_pixel outside_left outside_right
  left_pixel="$(identify -format "%[pixel:p{${left},${center_y}}]" "$screenshot")"
  right_pixel="$(identify -format "%[pixel:p{${right},${center_y}}]" "$screenshot")"
  outside_left="$(identify -format "%[pixel:p{$((left - 2)),${center_y}}]" "$screenshot")"
  outside_right="$(identify -format "%[pixel:p{$((right + 2)),${center_y}}]" "$screenshot")"
  [[ "$left_pixel" == *"(0,0,0"* && "$right_pixel" == *"(0,0,0"* ]] \
    || fail "$label does not have the expected framed report width ${expected_width}"
  [[ "$outside_left" != "$left_pixel" && "$outside_right" != "$right_pixel" ]] \
    || fail "$label report extends outside expected width ${expected_width}"
}

close_cleanly() {
  # The first Escape may dismiss a blocking report or return from a game.
  # The second closes the recovered menu.
  xdotool key Escape
  sleep 0.5
  xdotool key Escape
  set +e
  wait "$app_pid"
  local status=$?
  set -e
  app_pid=""
  (( status == 0 )) || fail "application did not close cleanly (status ${status})"
}

stop_app() {
  kill "$app_pid" 2>/dev/null || true
  set +e
  wait "$app_pid"
  set -e
  app_pid=""
}

assert_people_preserved() {
  local directory="$1"
  python3 - "${directory}/people-before.json" "${directory}/data/persons.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as before_file:
    before = json.load(before_file)
with open(sys.argv[2], encoding="utf-8") as after_file:
    after = json.load(after_file)
if before != after:
    raise SystemExit(f"person state changed after rejected start:\nBEFORE={before!r}\nAFTER={after!r}")
PY
}

no_level_message='No usable levels loaded. Correct content/configuration, restart the application, then try again. Press any key.'
no_weapon_message='No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.'
both_message='No usable levels loaded. No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.'

echo "[RUN] normal Play: no levels, mouse blocking, keyboard consumption, cached correction, preservation"
new_scenario normal-no-levels
saved_level="$(find "${scenario_dir}/levels" -maxdepth 1 -type f | head -n 1)"
cp "$saved_level" "${scenario_dir}/saved-level.json"
rm -f "${scenario_dir}/levels/"*.json
start_app "$scenario_dir"
# Exercise preservation with non-default visible settings: toggle Quick Liquid
# and advance the selected mode once before attempting Play.
xdotool mousemove 817 332 click 1
xdotool mousemove 999 277 click 1
sleep 0.5
capture "${scenario_dir}/menu-before.png"
xdotool key F1
sleep 0.5
capture "${scenario_dir}/report.png"
assert_changed "${scenario_dir}/menu-before.png" "${scenario_dir}/report.png" 1000 "no-level report"
assert_report_width "${scenario_dir}/report.png" "$no_level_message" "no-level report"
xdotool click --window "$window_id" 1
sleep 0.25
capture "${scenario_dir}/after-mouse.png"
assert_same "${scenario_dir}/report.png" "${scenario_dir}/after-mouse.png" "mouse click should not dismiss report"
cp "${scenario_dir}/saved-level.json" "${scenario_dir}/levels/corrected.json"
xdotool key F3
sleep 0.5
capture "${scenario_dir}/recovered.png"
assert_same "${scenario_dir}/menu-before.png" "${scenario_dir}/recovered.png" "recovered menu and consumed F3"
xdotool key F1
sleep 0.5
capture "${scenario_dir}/cached-report.png"
assert_same "${scenario_dir}/report.png" "${scenario_dir}/cached-report.png" "level correction before restart"
close_cleanly
assert_people_preserved "$scenario_dir"

echo "[RUN] normal Play: no enabled weapons"
new_scenario normal-no-weapons
disable_all_weapons "${scenario_dir}/data/config.script"
start_app "$scenario_dir"
capture "${scenario_dir}/menu-before.png"
xdotool key F1
sleep 0.5
capture "${scenario_dir}/report.png"
assert_report_width "${scenario_dir}/report.png" "$no_weapon_message" "no-weapon report"
close_cleanly
assert_people_preserved "$scenario_dir"

echo "[RUN] normal Play: both prerequisites missing in one report before clear-statistics prompt"
new_scenario normal-both-missing
rm -f "${scenario_dir}/levels/"*.json
disable_all_weapons "${scenario_dir}/data/config.script"
start_app "$scenario_dir"
capture "${scenario_dir}/menu-before.png"
xdotool key F1
sleep 0.5
capture "${scenario_dir}/report.png"
assert_report_width "${scenario_dir}/report.png" "$both_message" "combined report"
printf '\ngun 0 true\n' >>"${scenario_dir}/data/config.script"
xdotool key F3
sleep 0.25
xdotool key F1
sleep 0.5
capture "${scenario_dir}/cached-report.png"
assert_same "${scenario_dir}/report.png" "${scenario_dir}/cached-report.png" "configuration correction before restart"
close_cleanly
assert_people_preserved "$scenario_dir"

echo "[RUN] selected-map start: same enabled-weapon prerequisite"
new_scenario selected-map-no-weapons
disable_all_weapons "${scenario_dir}/data/config.script"
start_app "$scenario_dir"
xdotool key grave
xdotool type --delay 20 'map 0'
xdotool key Return
sleep 0.5
capture "${scenario_dir}/report.png"
assert_report_width "${scenario_dir}/report.png" "$no_weapon_message" "selected-map no-weapon report"
close_cleanly
assert_people_preserved "$scenario_dir"

echo "[RUN] valid normal Play retains game entry"
new_scenario valid-normal
start_app "$scenario_dir"
capture "${scenario_dir}/menu.png"
xdotool key F1
sleep 0.5
capture "${scenario_dir}/prompt.png"
assert_report_width "${scenario_dir}/prompt.png" 'Clear statistics? (Y/N)' "valid normal existing prompt"
xdotool key n
sleep 2
capture "${scenario_dir}/game.png"
assert_changed "${scenario_dir}/menu.png" "${scenario_dir}/game.png" 100000 "valid normal game start"
stop_app

echo "[RUN] valid selected-map start retains game entry"
new_scenario valid-selected-map
start_app "$scenario_dir"
capture "${scenario_dir}/menu.png"
xdotool key grave
xdotool type --delay 20 'map 0'
xdotool key Return
sleep 0.5
capture "${scenario_dir}/prompt.png"
assert_report_width "${scenario_dir}/prompt.png" 'Clear statistics? (Y/N)' "valid selected-map existing prompt"
xdotool key n
sleep 2
capture "${scenario_dir}/game.png"
assert_changed "${scenario_dir}/menu.png" "${scenario_dir}/game.png" 100000 "valid selected-map game start"
stop_app

echo "[RUN] malformed required level JSON retains fatal startup behavior"
new_scenario malformed-level
rm -f "${scenario_dir}/levels/"*.json
printf '{ malformed json' >"${scenario_dir}/levels/01.json"
start_app "$scenario_dir"
xdotool key grave
xdotool type --delay 20 'map 0'
xdotool key Return
sleep 0.5
xdotool key n
set +e
wait "$app_pid"
malformed_status=$?
set -e
app_pid=""
(( malformed_status != 0 && malformed_status != 124 )) \
  || fail "malformed required JSON did not retain fatal startup behavior (status ${malformed_status})"
grep -Eiq 'error occured|exception|parse|json' "${scenario_dir}/app.stderr" \
  || fail "malformed required JSON failure was not reported"

echo "safe-empty-match-start: PASS"
