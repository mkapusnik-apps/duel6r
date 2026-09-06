#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
smoke_dir="${SMOKE_DIR:-${build_dir}/smoke-main-menu}"
display="${DISPLAY:-:99}"
screen_size="${XVFB_SCREEN:-1280x900x24}"
app_timeout="${SMOKE_APP_TIMEOUT:-30s}"
expected_dimensions=""

fail() {
  echo "main-menu-smoke: $*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

parse_screen_dimensions() {
  local screen="$1"

  if [[ "$screen" =~ ^([0-9]+)x([0-9]+)x[0-9]+$ ]]; then
    expected_dimensions="${BASH_REMATCH[1]}x${BASH_REMATCH[2]}"
  else
    fail "XVFB_SCREEN must use WIDTHxHEIGHTxDEPTH format: $screen"
  fi
}

window_point() {
  local local_x="$1"
  local local_y="$2"
  local width="${expected_dimensions%x*}"
  local height="${expected_dimensions#*x}"

  awk -v width="$width" -v height="$height" -v local_x="$local_x" -v local_y="$local_y" '
    BEGIN {
      scale = width / 850
      if (height / 700 < scale) scale = height / 700
      if (1.35 < scale) scale = 1.35
      translation_x = int((width - 850 * scale) * 0.5)
      translation_y = int((height - 700 * scale) * 0.5)
      print int(translation_x + local_x * scale), int(height - translation_y - local_y * scale)
    }
  '
}

capture_root() {
  local path="$1"
  import -window root "$path"
}

check_image() {
  local path="$1"
  local label="$2"

  [[ -s "$path" ]] || fail "$label screenshot is missing or empty: $path"

  local dimensions colors
  dimensions="$(identify -format '%wx%h' "$path")"
  colors="$(identify -format '%k' "$path")"

  {
    echo "$label: dimensions=${dimensions} unique-colors=${colors}"
    identify -verbose "$path"
  } >>"${smoke_dir}/image-info.txt"

  [[ "$dimensions" == "$expected_dimensions" ]] || fail "$label screenshot has unexpected dimensions: $dimensions (expected ${expected_dimensions})"
  [[ "$colors" =~ ^[0-9]+$ ]] || fail "$label unique color count is not numeric: $colors"
  (( colors >= 16 )) || fail "$label screenshot appears blank or nearly uniform: ${colors} unique colors"
}

assert_image_changed() {
  local before="$1"
  local after="$2"
  local diff="$3"
  local label="$4"
  local minimum_pixels="$5"
  local compare_status
  local diff_pixels

  set +e
  compare -metric AE "$before" "$after" "$diff" 2>"${diff}.txt"
  compare_status=$?
  set -e

  if (( compare_status > 1 )); then
    fail "failed to compare $label screenshots"
  fi

  diff_pixels="$(<"${diff}.txt")"
  diff_pixels="${diff_pixels%%.*}"
  [[ "$diff_pixels" =~ ^[0-9]+$ ]] || fail "$label screenshot difference is not numeric: $diff_pixels"
  (( diff_pixels >= minimum_pixels )) || fail "$label did not visibly change the menu: ${diff_pixels} pixels changed"

  echo "$label: changed-pixels=${diff_pixels}" >>"${smoke_dir}/image-info.txt"
}

require_command Xvfb
require_command xdotool
require_command import
require_command identify
require_command compare
require_command timeout
require_command awk

parse_screen_dimensions "$screen_size"

[[ -x "${build_dir}/duel6r" ]] || fail "game executable not found or not executable: ${build_dir}/duel6r"

rm -rf "$smoke_dir"
mkdir -p "$smoke_dir"
mkdir -p "${smoke_dir}/home"
touch "${smoke_dir}/app.stdout" "${smoke_dir}/app.stderr" "${smoke_dir}/automation.log" "${smoke_dir}/xvfb.log"

export DISPLAY="$display"
export SDL_AUDIODRIVER=dummy
export LIBGL_ALWAYS_SOFTWARE=1
export HOME="${smoke_dir}/home"
export XDG_CACHE_HOME="${smoke_dir}/home/.cache"
export XDG_CONFIG_HOME="${smoke_dir}/home/.config"
export XDG_DATA_HOME="${smoke_dir}/home/.local/share"

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

Xvfb "$display" -screen 0 "$screen_size" +extension GLX +render -noreset >"${smoke_dir}/xvfb.log" 2>&1 &
xvfb_pid="$!"

for _ in {1..40}; do
  if xdotool getdisplaygeometry >"${smoke_dir}/display-geometry.txt" 2>>"${smoke_dir}/xvfb.log"; then
    break
  fi
  sleep 0.25
done

xdotool getdisplaygeometry >>"${smoke_dir}/display-geometry.txt" 2>>"${smoke_dir}/xvfb.log" \
  || fail "Xvfb did not become ready"
display_geometry="$(<"${smoke_dir}/display-geometry.txt")"
display_geometry="${display_geometry//$'\n'/ }"
[[ "$display_geometry" == *"${expected_dimensions%x*} ${expected_dimensions#*x}"* ]] \
  || fail "Xvfb display geometry does not match XVFB_SCREEN=${screen_size}: ${display_geometry}"

(
  cd "$build_dir"
  timeout --kill-after=5s "$app_timeout" ./duel6r >"${smoke_dir}/app.stdout" 2>"${smoke_dir}/app.stderr"
) &
app_pid="$!"

window_id=""
for _ in {1..80}; do
  if ! kill -0 "$app_pid" >/dev/null 2>&1; then
    wait "$app_pid" || fail "game exited before the main menu window appeared"
  fi

  mapfile -t windows < <(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null || true)
  if (( ${#windows[@]} > 0 )); then
    window_id="${windows[0]}"
    break
  fi

  sleep 0.25
done

[[ -n "$window_id" ]] || fail "main menu window was not found"

{
  echo "window-id=${window_id}"
  xdotool getwindowname "$window_id"
  xdotool getwindowgeometry "$window_id"
} >"${smoke_dir}/window-info.txt" 2>&1 || true

xdotool windowfocus "$window_id" 2>>"${smoke_dir}/automation.log" || true
xdotool windowactivate "$window_id" 2>>"${smoke_dir}/automation.log" || true
sleep 2

capture_root "${smoke_dir}/main-menu.png"
check_image "${smoke_dir}/main-menu.png" "main-menu"

read -r click_x click_y < <(window_point 662 472)
xdotool mousemove "$click_x" "$click_y" \
  mousedown 1 sleep 0.1 mouseup 1 2>>"${smoke_dir}/automation.log"
sleep 1
capture_root "${smoke_dir}/quick-liquid-toggled.png"
check_image "${smoke_dir}/quick-liquid-toggled.png" "quick-liquid-toggled"
assert_image_changed \
  "${smoke_dir}/main-menu.png" \
  "${smoke_dir}/quick-liquid-toggled.png" \
  "${smoke_dir}/quick-liquid-diff.png" \
  "quick-liquid toggle" \
  10

xdotool mousemove "$click_x" "$click_y" \
  mousedown 1 sleep 0.1 mouseup 1 2>>"${smoke_dir}/automation.log"
sleep 0.25

xdotool key --window "$window_id" grave
sleep 1
capture_root "${smoke_dir}/console-open.png"
check_image "${smoke_dir}/console-open.png" "console-open"
assert_image_changed \
  "${smoke_dir}/main-menu.png" \
  "${smoke_dir}/console-open.png" \
  "${smoke_dir}/console-diff.png" \
  "console toggle" \
  1000

xdotool key --window "$window_id" grave
sleep 0.25
xdotool key --window "$window_id" Escape

set +e
wait "$app_pid"
app_status=$?
set -e
app_pid=""

if (( app_status == 124 || app_status == 137 || app_status == 143 )); then
  fail "game did not exit cleanly after automated menu input before timeout (${app_timeout})"
fi

(( app_status == 0 )) || fail "game exited with status ${app_status}"

if grep -Eiq 'fatal|segmentation fault|core dumped|error occured|unable to|exception' "${smoke_dir}/app.stderr"; then
  fail "fatal-looking output was written to app.stderr"
fi

echo "Main menu smoke test passed. Artifacts: ${smoke_dir}"
