#!/usr/bin/env bash
set -euo pipefail

# Black-box menu regression checks for the centered redesign. This complements
# the match-start and shared-arena tests with roster, persistence, confirmation,
# and overflow-list interaction coverage.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${workspace_dir}/resources}"
test_root="${TEST_ROOT:-${build_dir}/menu-redesign-behavior}"
display="${DISPLAY:-:96}"

fail() {
  echo "menu-redesign-behavior: FAIL: $*" >&2
  exit 1
}

for command in Xvfb xdotool import compare convert timeout python3; do
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
[[ "$(xdotool getdisplaygeometry)" == "1280 900" ]] || fail "Xvfb did not become ready"

new_scenario() {
  scenario_dir="${test_root}/$1"
  mkdir -p "$scenario_dir"
  cp "${build_dir}/duel6r" "$scenario_dir/duel6r"
  cp -a "${resource_dir}/." "$scenario_dir/"
  rm -f "${scenario_dir}/data/persons.json"
}

start_app() {
  local startup_command="${1:-}"
  mkdir -p "${scenario_dir}/home"
  (
    export HOME="${scenario_dir}/home"
    export XDG_CACHE_HOME="$HOME/.cache" XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
    cd "$scenario_dir"
    if [[ -n "$startup_command" ]]; then
      timeout --kill-after=3s 35s ./duel6r "$startup_command" >app.stdout 2>app.stderr
    else
      timeout --kill-after=3s 35s ./duel6r >app.stdout 2>app.stderr
    fi
  ) &
  app_pid="$!"
  window_id=""
  for _ in {1..100}; do
    window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
    [[ -n "$window_id" ]] && break
    sleep 0.1
  done
  [[ -n "$window_id" ]] || fail "application window not found in $scenario_dir"
  xdotool windowfocus "$window_id" windowactivate "$window_id" >/dev/null 2>&1 || true
  sleep 2
}

close_app() {
  xdotool key --window "$window_id" Escape
  set +e
  wait "$app_pid"
  status=$?
  set -e
  app_pid=""
  (( status == 0 )) || fail "application did not close cleanly: $status"
}

capture() {
  import -window root "$1"
}

# Release fills the 1280x900 Xvfb display. The 850x700 logical menu therefore
# uses scale 9/7 and origin (93, 0). Convert points from the former centered
# 1:1 canvas, and normalize captures when assertions operate on logical pixels.
menu_x() {
  printf '%s' $((93 + ($1 - 215) * 9 / 7))
}

menu_y() {
  printf '%s' $((($1 - 100) * 9 / 7))
}

normalize_menu() {
  convert "$1" -crop 1093x900+93+0 +repage -filter point -resize 850x700! "$2"
}

assert_changed() {
  local before="$1" after="$2" label="$3" output status pixels
  set +e
  output="$(compare -metric AE "$before" "$after" null: 2>&1)"
  status=$?
  set -e
  (( status <= 1 )) || fail "could not compare $label"
  pixels="${output%%.*}"
  [[ "$pixels" =~ ^[0-9]+$ ]] || fail "$label returned invalid difference: $output"
  (( pixels >= 10 )) || fail "$label did not visibly change ($pixels pixels)"
  echo "$label: changed-pixels=$pixels"
}

assert_same() {
  local before="$1" after="$2" label="$3" output status pixels
  set +e
  output="$(compare -metric AE "$before" "$after" null: 2>&1)"
  status=$?
  set -e
  (( status <= 1 )) || fail "could not compare $label"
  pixels="${output%%.*}"
  [[ "$pixels" =~ ^[0-9]+$ ]] || fail "$label returned invalid difference: $output"
  (( pixels == 0 )) || fail "$label changed $pixels pixels"
}

dump_rounds_setting() {
  local destination="$1"
  xdotool key --window "$window_id" grave
  sleep 0.15
  xdotool type --window "$window_id" --delay 5 rounds
  xdotool key --window "$window_id" Return
  xdotool type --window "$window_id" --delay 5 "dump $destination"
  xdotool key --window "$window_id" Return
  sleep 0.15
  xdotool key --window "$window_id" grave
}

assert_dumped_rounds() {
  local dump_file="$1" expected="$2" label="$3"
  python3 - "$dump_file" "$expected" "$label" <<'PY'
import sys

path, expected, label = sys.argv[1:]
with open(path, encoding="utf-8") as source:
    lines = [line.strip() for line in source if line.startswith("Max rounds:")]
actual = lines[-1] if lines else "<missing>"
wanted = f"Max rounds: {expected}"
if actual != wanted:
    raise SystemExit(f"{label}: expected {wanted!r}, got {actual!r}")
PY
}

write_overflow_fixture() {
  python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
persons = []
for i in range(1, 21):
    persons.append({
        "name": f"Rank{i:02d}", "shots": 100 + i, "hits": 70 + i,
        "kills": 30 + i, "deaths": 10 + i, "assistances": 5 + i,
        "wins": i, "penalties": i % 3, "games": 20 + i,
        "timeAlive": 500 + i, "totalGameTime": 900 + i,
        "totalDamage": 2000 + i, "assistedDamage": 200 + i,
        "elo": 1000 + 10 * i, "eloTrend": i - 10, "eloGames": 5 + i,
    })
with open(sys.argv[1], "w", encoding="utf-8") as f:
    json.dump({"persons": persons, "playing": [p["name"] for p in persons[:15]], "rounds": 7}, f, indent=2)
PY
  cp "${scenario_dir}/data/persons.json" "${scenario_dir}/before.json"
}

write_elo_name_fixture() {
  python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
cases = [
    ("EightPos", 2000, 12), ("EightNeg", 1990, -12),
    ("NinePos09", 1980, 23), ("NineNeg09", 1970, -23),
    ("TenPos0010", 1960, 34), ("TenNeg0010", 1950, -34),
]
persons = []
for i, (name, elo, trend) in enumerate(cases):
    persons.append({
        "name": name, "shots": 10, "hits": 5, "kills": 2, "deaths": 1,
        "assistances": 0, "wins": 1, "penalties": 0, "games": 2,
        "timeAlive": 10, "totalGameTime": 20, "totalDamage": 30,
        "assistedDamage": 0, "elo": elo, "eloTrend": trend, "eloGames": 2,
    })
for i in range(14):
    persons.append(dict(persons[0], name=f"Extra{i:02d}", elo=1900 - i, eloTrend=i - 7))
with open(sys.argv[1], "w", encoding="utf-8") as f:
    json.dump({"persons": persons, "playing": [], "rounds": 0}, f, indent=2)
PY
}

echo "[RUN] complete consolidated-person rows for names and signed trends"
new_scenario elo-name-widths
write_elo_name_fixture
start_app
capture "${scenario_dir}/elo-name-widths-screen.png"
normalize_menu "${scenario_dir}/elo-name-widths-screen.png" "${scenario_dir}/elo-name-widths.png"
python3 - "${scenario_dir}/elo-name-widths.png" <<'PY'
import subprocess, sys

image = sys.argv[1]
left = 16
first_top = 165

def rgb_crop(x, y, width, height=18):
    return subprocess.check_output([
        "convert", image, "-crop", f"{width}x{height}+{x}+{y}",
        "+repage", "-alpha", "off", "-depth", "8", "rgb:-",
    ])

def dark_pixels(data):
    return sum(max(data[i:i + 3]) < 100 for i in range(0, len(data), 3))

lengths = (8, 8, 9, 9, 10, 10)
signs = []
for row, length in enumerate(lengths):
    top = first_top + 18 * row
    # Consolidated rows use Rank(5), Name(24), Elo(6), and Trend(7).
    final_name_x = left + (5 + length - 1) * 8
    if dark_pixels(rgb_crop(final_name_x, top, 8)) < 4:
        raise SystemExit(f"Elo row {row + 1}: final character of {length}-character name is not visible")
    # All fixture trends have two digits. Right alignment in the seven-character
    # Trend field puts the sign at character 39 and final digit at character 41.
    sign = rgb_crop(left + 39 * 8, top, 8)
    if dark_pixels(sign) < 3:
        raise SystemExit(f"Elo row {row + 1}: trend sign is not visible")
    if dark_pixels(rgb_crop(left + 41 * 8, top, 8)) < 4:
        raise SystemExit(f"Elo row {row + 1}: final trend digit is clipped")
    signs.append(sign)

for positive, negative in ((0, 1), (2, 3), (4, 5)):
    if signs[positive] == signs[negative]:
        raise SystemExit(f"Elo rows {positive + 1}/{negative + 1}: positive and negative signs are indistinguishable")
PY
close_app

echo "[RUN] overflowing Persons and persistent score lists: wheel, arrows, track/thumb"
new_scenario overflow-scroll
write_overflow_fixture
start_app
capture "${scenario_dir}/initial.png"

# Persons list (11 visible of 20): wheel, down arrow, track, then thumb drag.
xdotool mousemove "$(menu_x 315)" "$(menu_y 400)" click 5
sleep 0.2
capture "${scenario_dir}/elo-wheel.png"
assert_changed "${scenario_dir}/initial.png" "${scenario_dir}/elo-wheel.png" "Persons wheel"
xdotool mousemove "$(menu_x 589)" "$(menu_y 449)" mousedown 1 sleep 0.15 mouseup 1
sleep 0.2
capture "${scenario_dir}/elo-arrow.png"
assert_changed "${scenario_dir}/elo-wheel.png" "${scenario_dir}/elo-arrow.png" "Persons down arrow"
xdotool mousemove "$(menu_x 589)" "$(menu_y 320)" click 1
sleep 0.2
capture "${scenario_dir}/elo-track.png"
assert_changed "${scenario_dir}/elo-arrow.png" "${scenario_dir}/elo-track.png" "Persons track"
xdotool mousemove "$(menu_x 589)" "$(menu_y 320)" mousedown 1 \
  mousemove "$(menu_x 589)" "$(menu_y 420)" sleep 0.1 mouseup 1
sleep 0.2
capture "${scenario_dir}/elo-thumb.png"
assert_changed "${scenario_dir}/elo-track.png" "${scenario_dir}/elo-thumb.png" "Persons thumb drag"

# Persistent score list (8 visible of 20): wheel, down arrow, track/thumb.
xdotool mousemove "$(menu_x 700)" "$(menu_y 630)" click 5
sleep 0.2
capture "${scenario_dir}/score-wheel.png"
assert_changed "${scenario_dir}/elo-thumb.png" "${scenario_dir}/score-wheel.png" "score wheel"
xdotool mousemove "$(menu_x 1045)" "$(menu_y 702)" mousedown 1 sleep 0.15 mouseup 1
sleep 0.2
capture "${scenario_dir}/score-arrow.png"
assert_changed "${scenario_dir}/score-wheel.png" "${scenario_dir}/score-arrow.png" "score down arrow"
xdotool mousemove "$(menu_x 1045)" "$(menu_y 660)" click 1
sleep 0.2
capture "${scenario_dir}/score-track.png"
assert_changed "${scenario_dir}/score-arrow.png" "${scenario_dir}/score-track.png" "score track"
xdotool mousemove "$(menu_x 1045)" "$(menu_y 660)" mousedown 1 \
  mousemove "$(menu_x 1045)" "$(menu_y 610)" sleep 0.1 mouseup 1
sleep 0.2
capture "${scenario_dir}/score-thumb.png"
assert_changed "${scenario_dir}/score-track.png" "${scenario_dir}/score-thumb.png" "score thumb drag"

# F3 rejection must preserve all data.
xdotool key --window "$window_id" F3
sleep 0.2
xdotool key --window "$window_id" n
close_app
python3 - "${scenario_dir}/before.json" "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: before = json.load(f)
with open(sys.argv[2], encoding="utf-8") as f: after = json.load(f)
if before != after:
    raise SystemExit("F3 rejection changed persisted person data")
PY

# Clear button acceptance resets non-Elo stats but retains Elo fields.
start_app
xdotool mousemove "$(menu_x 640)" "$(menu_y 755)" click 1
sleep 0.2
xdotool key --window "$window_id" y
close_app
python3 - "${scenario_dir}/before.json" "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: before = json.load(f)
with open(sys.argv[2], encoding="utf-8") as f: after = json.load(f)
old = {p["name"]: p for p in before["persons"]}
elo = ("elo", "eloTrend", "eloGames")
non_elo = ("shots", "hits", "kills", "deaths", "assistances", "wins", "penalties",
           "games", "timeAlive", "totalGameTime", "totalDamage", "assistedDamage")
for person in after["persons"]:
    if any(person[k] != old[person["name"]][k] for k in elo):
        raise SystemExit(f"Elo changed for {person['name']}")
    if any(person[k] != 0 for k in non_elo):
        raise SystemExit(f"non-Elo stats were not cleared for {person['name']}")
PY

echo "[RUN] consolidated Persons add, transfer, duplicate, delete, selection, and restart behavior"
new_scenario consolidated-persons
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys

def person(name, elo=1000, trend=0, elo_games=0):
    return {"name": name, "shots": 3, "hits": 2, "kills": 1, "deaths": 1,
            "assistances": 0, "wins": 0, "penalties": 0, "games": 1,
            "timeAlive": 10, "totalGameTime": 20, "totalDamage": 30,
            "assistedDamage": 0, "elo": elo, "eloTrend": trend,
            "eloGames": elo_games}

# Person-record order deliberately differs from the required ranked ordering.
persons = [person("UnrankedRoster"), person("RankLow", 1100, -7, 2),
           person("UnrankedFree"), person("RankHigh", 1400, 12, 4),
           person("RankRoster", 1250, 0, 3)]
with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"persons": persons,
               "playing": ["RankRoster", "UnrankedRoster"], "rounds": 0}, output)
PY
start_app
capture "${scenario_dir}/initial.png"

# Ranked display order is RankHigh, RankRoster, RankLow; unranked rows then
# retain record order (UnrankedRoster, UnrankedFree). Select RankRoster and
# exercise no-op add/double-click/remove behavior for an existing roster member.
xdotool mousemove "$(menu_x 315)" "$(menu_y 292)" click 1
xdotool mousemove "$(menu_x 336)" "$(menu_y 511)" click 1
xdotool mousemove "$(menu_x 315)" "$(menu_y 292)" click --repeat 2 --delay 80 1
xdotool mousemove "$(menu_x 254)" "$(menu_y 511)" click 1

# A non-modal roster-member Remove leaves the UI interactive. Enter in the
# focused name field adds exactly one saved person and refreshes the list.
xdotool mousemove "$(menu_x 315)" "$(menu_y 490)" click 1
xdotool type --window "$window_id" --delay 5 EnterAdded
xdotool key --window "$window_id" Return
xdotool type --window "$window_id" --delay 5 EnterAdded
xdotool key --window "$window_id" Return
xdotool key --window "$window_id" BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace BackSpace

# Add UnrankedFree with >>. Refresh must retain its selected row; repeated >>
# and person-row double-click must not duplicate roster membership.
xdotool mousemove "$(menu_x 315)" "$(menu_y 346)" click 1
capture "${scenario_dir}/free-selected.png"
xdotool mousemove "$(menu_x 336)" "$(menu_y 511)" click 1
capture "${scenario_dir}/free-after-transfer.png"
normalize_menu "${scenario_dir}/free-selected.png" "${scenario_dir}/free-selected-normalized.png"
normalize_menu "${scenario_dir}/free-after-transfer.png" "${scenario_dir}/free-after-transfer-normalized.png"
convert "${scenario_dir}/free-selected-normalized.png" -crop 352x18+16+237 +repage "${scenario_dir}/free-selected-row.png"
convert "${scenario_dir}/free-after-transfer-normalized.png" -crop 352x18+16+237 +repage "${scenario_dir}/free-after-transfer-row.png"
assert_same "${scenario_dir}/free-selected-row.png" "${scenario_dir}/free-after-transfer-row.png" \
  "Persons selection after roster transfer"
xdotool mousemove "$(menu_x 336)" "$(menu_y 511)" click 1
xdotool mousemove "$(menu_x 315)" "$(menu_y 346)" click --repeat 2 --delay 80 1

# Remove on a roster member is a no-op. Double-click in Players and << both
# return the player while its consolidated Persons row remains present.
xdotool mousemove "$(menu_x 254)" "$(menu_y 511)" click 1
xdotool mousemove "$(menu_x 650)" "$(menu_y 292)" click --repeat 2 --delay 80 1
xdotool mousemove "$(menu_x 315)" "$(menu_y 346)" click 1
xdotool mousemove "$(menu_x 336)" "$(menu_y 511)" click 1
xdotool mousemove "$(menu_x 650)" "$(menu_y 292)" click 1
xdotool mousemove "$(menu_x 299)" "$(menu_y 511)" click 1

# Reject then accept deletion of the selected non-roster person. The refresh
# clears the deleted selection, so a following >> cannot change the roster.
xdotool mousemove "$(menu_x 315)" "$(menu_y 346)" click 1
xdotool mousemove "$(menu_x 254)" "$(menu_y 511)" click 1
sleep 0.2
xdotool key --window "$window_id" n
xdotool mousemove "$(menu_x 254)" "$(menu_y 511)" click 1
sleep 0.2
xdotool key --window "$window_id" y
xdotool mousemove "$(menu_x 336)" "$(menu_y 511)" click 1

# Enter adds another person; a person-row double-click adds it to Players.
xdotool mousemove "$(menu_x 315)" "$(menu_y 490)" click 1
xdotool type --window "$window_id" --delay 5 Transfer
xdotool key --window "$window_id" Return
xdotool mousemove "$(menu_x 315)" "$(menu_y 364)" click --repeat 2 --delay 80 1
close_app

python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as source:
    data = json.load(source)
names = [person["name"] for person in data["persons"]]
expected = ["UnrankedRoster", "RankLow", "RankHigh", "RankRoster",
            "EnterAdded", "Transfer"]
if names != expected:
    raise SystemExit(f"unexpected persisted person records: {names}")
if data["playing"] != ["RankRoster", "UnrankedRoster", "Transfer"]:
    raise SystemExit(f"unexpected persisted roster: {data['playing']}")
PY
cp "${scenario_dir}/data/persons.json" "${scenario_dir}/before-restart.json"
start_app
close_app
python3 - "${scenario_dir}/before-restart.json" "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as source: before = json.load(source)
with open(sys.argv[2], encoding="utf-8") as source: after = json.load(source)
if after != before:
    raise SystemExit("application restart changed persons, roster, statistics, or Elo data")
PY

# The Add button must apply the same focused-name behavior as Enter.
start_app
xdotool mousemove "$(menu_x 315)" "$(menu_y 490)" click 1
xdotool type --window "$window_id" --delay 5 ButtonAdd
xdotool mousemove "$(menu_x 381)" "$(menu_y 511)" click 1
close_app
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as source: data = json.load(source)
names = [person["name"] for person in data["persons"]]
if "ButtonAdd" not in names:
    raise SystemExit("Add button did not add the focused person name")
PY

echo "[RUN] one-player F1 validation and state preservation"
new_scenario one-player
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
p = {"name":"Solo", "shots":3, "hits":2, "kills":1, "deaths":1, "assistances":0,
     "wins":0, "penalties":0, "games":1, "timeAlive":10, "totalGameTime":20,
     "totalDamage":30, "assistedDamage":0, "elo":1010, "eloTrend":2, "eloGames":1}
with open(sys.argv[1], "w", encoding="utf-8") as f:
    json.dump({"persons":[p], "playing":["Solo"], "rounds":2}, f, indent=2)
PY
cp "${scenario_dir}/data/persons.json" "${scenario_dir}/before.json"
start_app
capture "${scenario_dir}/menu.png"
xdotool keydown --window "$window_id" F1
sleep 0.3
capture "${scenario_dir}/cant-play-alone.png"
assert_changed "${scenario_dir}/menu.png" "${scenario_dir}/cant-play-alone.png" "one-player report"
xdotool keyup --window "$window_id" F1
sleep 0.2
close_app
python3 - "${scenario_dir}/before.json" "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: before = json.load(f)
with open(sys.argv[2], encoding="utf-8") as f: after = json.load(f)
if before != after:
    raise SystemExit("one-player rejected start changed persisted state")
PY

echo "[RUN] random and Elo shuffles preserve player/control row pairs"
new_scenario shuffle-controls
write_overflow_fixture
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: data = json.load(f)
data["playing"] = [f"Rank{i:02d}" for i in range(1, 7)]
with open(sys.argv[1], "w", encoding="utf-8") as f: json.dump(data, f, indent=2)
PY
start_app
# Assign K1 through K6 to the six visible players.
for i in {0..5}; do
  for ((step = 0; step < i; step++)); do
    xdotool mousemove "$(menu_x 825)" "$(menu_y $((255 + 18 * i)))" mousedown 1 sleep 0.06 mouseup 1
    sleep 0.05
  done
done
capture "${scenario_dir}/before-shuffle.png"
xdotool mousemove "$(menu_x 636)" "$(menu_y 534)" click 1
sleep 0.3
capture "${scenario_dir}/after-random.png"
xdotool mousemove "$(menu_x 617)" "$(menu_y 534)" click 1
sleep 0.3
capture "${scenario_dir}/after-elo.png"

assert_row_pairs_preserved() {
  local before="$1" after="$2" label="$3" before_crop after_crop metric status matched
  normalize_menu "$before" "${scenario_dir}/${label}-before-normalized.png"
  normalize_menu "$after" "${scenario_dir}/${label}-after-normalized.png"
  for i in {0..5}; do
    before_crop="${scenario_dir}/${label}-before-${i}.png"
    convert "${scenario_dir}/${label}-before-normalized.png" \
      -crop "250x18+394+$((147 + 18 * i))" +repage "$before_crop"
    matched=false
    for j in {0..5}; do
      after_crop="${scenario_dir}/${label}-after-${j}.png"
      convert "${scenario_dir}/${label}-after-normalized.png" \
        -crop "250x18+394+$((147 + 18 * j))" +repage "$after_crop"
      set +e
      metric="$(compare -metric AE "$before_crop" "$after_crop" null: 2>&1)"
      status=$?
      set -e
      (( status <= 1 )) || fail "$label row comparison failed"
      # Scaled font rasterization differs by vertical screen position even for
      # the same row content. Matching pairs stay below 700 changed pixels;
      # distinct pairs in this fixture remain above 1000.
      if (( ${metric%%.*} < 700 )); then
        matched=true
        break
      fi
    done
    [[ "$matched" == true ]] || fail "$label detached a player from its selected control (source row $i)"
  done
}

# A random shuffle may legitimately produce the identity permutation. Verify
# the behavioral invariant directly without requiring the visible order to
# change: every player must remain paired with the same selected control.
assert_row_pairs_preserved "${scenario_dir}/before-shuffle.png" "${scenario_dir}/after-random.png" "random"
assert_changed "${scenario_dir}/after-random.png" "${scenario_dir}/after-elo.png" "Elo shuffle"
assert_row_pairs_preserved "${scenario_dir}/after-random.png" "${scenario_dir}/after-elo.png" "elo"
close_app
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f: data = json.load(f)
expected = [f"Rank{i:02d}" for i in range(6, 0, -1)]
if data["playing"] != expected:
    raise SystemExit(f"Elo shuffle did not persist descending Elo order: {data['playing']}")
PY

echo "[RUN] Burnable Trees default, round/menu retention, and application restart reset"
new_scenario burnable-trees-session
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys

def person(name):
    return {"name": name, "shots": 0, "hits": 0, "kills": 0, "deaths": 0,
            "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
            "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
            "assistedDamage": 0, "elo": 1000, "eloTrend": 0, "eloGames": 0}

with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"persons": [person("Alpha"), person("Beta")],
               "playing": ["Alpha", "Beta"], "rounds": 0}, output)
PY

crop_burnable_trees_control() {
  # The 850x700 client is centered in the 1280x900 Xvfb root. GUI control Y
  # coordinates originate at the lower edge of that client.
  local normalized="${2%.png}-normalized.png"
  normalize_menu "$1" "$normalized"
  convert "$normalized" -crop 180x24+650+248 +repage "$2"
}

start_app
capture "${scenario_dir}/burnable-default.png"
crop_burnable_trees_control "${scenario_dir}/burnable-default.png" "${scenario_dir}/burnable-default-crop.png"

# Toggle only the Burnable Trees control off.
xdotool mousemove "$(menu_x 875)" "$(menu_y 355)" click 1
sleep 0.25
capture "${scenario_dir}/burnable-disabled.png"
crop_burnable_trees_control "${scenario_dir}/burnable-disabled.png" "${scenario_dir}/burnable-disabled-crop.png"
assert_changed "${scenario_dir}/burnable-default-crop.png" "${scenario_dir}/burnable-disabled-crop.png" \
  "Burnable Trees checkbox"

# Apply the selection to a match, cross two round boundaries, and return to the
# same menu. Unlimited-round games ask whether to clear statistics first.
xdotool key --window "$window_id" F1
sleep 0.4
xdotool key --window "$window_id" n
sleep 3
xdotool key --window "$window_id" Shift+F1
sleep 2
xdotool key --window "$window_id" Shift+F1
sleep 2
xdotool key --window "$window_id" Shift+Escape
sleep 2
capture "${scenario_dir}/burnable-disabled-after-rounds.png"
crop_burnable_trees_control "${scenario_dir}/burnable-disabled-after-rounds.png" \
  "${scenario_dir}/burnable-disabled-after-rounds-crop.png"
assert_same "${scenario_dir}/burnable-disabled-crop.png" \
  "${scenario_dir}/burnable-disabled-after-rounds-crop.png" \
  "Burnable Trees disabled selection after rounds and menu return"

close_app
start_app
capture "${scenario_dir}/burnable-restarted.png"
crop_burnable_trees_control "${scenario_dir}/burnable-restarted.png" "${scenario_dir}/burnable-restarted-crop.png"
assert_same "${scenario_dir}/burnable-default-crop.png" "${scenario_dir}/burnable-restarted-crop.png" \
  "Burnable Trees fresh-start enabled state"
close_app

echo "[RUN] Rounds focus, apply, session retention, restart, and startup override"
new_scenario rounds-session
python3 - "${scenario_dir}/data/persons.json" <<'PY'
import json, sys

def person(name):
    return {"name": name, "shots": 0, "hits": 0, "kills": 0, "deaths": 0,
            "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
            "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
            "assistedDamage": 0, "elo": 1000, "eloTrend": 0, "eloGames": 0}

with open(sys.argv[1], "w", encoding="utf-8") as output:
    json.dump({"persons": [person("Alpha"), person("Beta")],
               "playing": ["Alpha", "Beta"], "rounds": 0}, output)
PY

crop_rounds_control() {
  local normalized="${2%.png}-normalized.png"
  normalize_menu "$1" "$normalized"
  convert "$normalized" -crop 96x24+740+268 +repage "$2"
}

# A startup console setting is reflected in the menu. Editing that positive
# value and leaving the field does not apply it until Enter is pressed.
start_app "rounds 7"
capture "${scenario_dir}/startup-override-7.png"
crop_rounds_control "${scenario_dir}/startup-override-7.png" "${scenario_dir}/startup-override-7-crop.png"
dump_rounds_setting rounds-startup.txt
assert_dumped_rounds "${scenario_dir}/rounds-startup.txt" 7 "startup override"

xdotool mousemove "$(menu_x 1025)" "$(menu_y 376)" click 1
xdotool key --window "$window_id" BackSpace
xdotool type --window "$window_id" --delay 5 "x3y"
xdotool mousemove "$(menu_x 900)" "$(menu_y 450)" click 1
sleep 0.2
capture "${scenario_dir}/edited-3-blurred.png"
crop_rounds_control "${scenario_dir}/edited-3-blurred.png" "${scenario_dir}/edited-3-blurred-crop.png"
assert_changed "${scenario_dir}/startup-override-7-crop.png" "${scenario_dir}/edited-3-blurred-crop.png" \
  "Rounds digit-only edit"
dump_rounds_setting rounds-before-enter.txt
assert_dumped_rounds "${scenario_dir}/rounds-before-enter.txt" 7 "non-empty blur"

# Focusing the positive edit keeps it. Enter applies it and removes focus.
xdotool mousemove "$(menu_x 1025)" "$(menu_y 376)" click 1
sleep 0.15
capture "${scenario_dir}/focused-positive-3.png"
crop_rounds_control "${scenario_dir}/focused-positive-3.png" "${scenario_dir}/focused-positive-3-crop.png"
assert_changed "${scenario_dir}/edited-3-blurred-crop.png" "${scenario_dir}/focused-positive-3-crop.png" \
  "Rounds positive focus indicator"
xdotool key --window "$window_id" Return
sleep 0.15
capture "${scenario_dir}/applied-3.png"
crop_rounds_control "${scenario_dir}/applied-3.png" "${scenario_dir}/applied-3-crop.png"
assert_same "${scenario_dir}/edited-3-blurred-crop.png" "${scenario_dir}/applied-3-crop.png" \
  "Rounds positive value after Enter"
dump_rounds_setting rounds-after-enter.txt
assert_dumped_rounds "${scenario_dir}/rounds-after-enter.txt" 3 "Enter apply"

# Enter gameplay and return in the same process. The applied value remains.
xdotool key --window "$window_id" F1
sleep 3
xdotool key --window "$window_id" Shift+Escape
sleep 2
capture "${scenario_dir}/after-gameplay-3.png"
crop_rounds_control "${scenario_dir}/after-gameplay-3.png" "${scenario_dir}/after-gameplay-3-crop.png"
assert_same "${scenario_dir}/applied-3-crop.png" "${scenario_dir}/after-gameplay-3-crop.png" \
  "Rounds applied value after gameplay return"
close_app

# A fresh process does not restore the prior session's max-round setting.
start_app
capture "${scenario_dir}/restart-0.png"
crop_rounds_control "${scenario_dir}/restart-0.png" "${scenario_dir}/restart-0-crop.png"
assert_changed "${scenario_dir}/after-gameplay-3-crop.png" "${scenario_dir}/restart-0-crop.png" \
  "Rounds restart reset"
dump_rounds_setting rounds-after-restart.txt
assert_dumped_rounds "${scenario_dir}/rounds-after-restart.txt" 0 "restart reset"

# Focusing exactly zero clears it. Empty focus loss restores zero and applies
# unlimited-round semantics immediately.
xdotool mousemove "$(menu_x 1025)" "$(menu_y 376)" click 1
sleep 0.15
capture "${scenario_dir}/focused-empty.png"
crop_rounds_control "${scenario_dir}/focused-empty.png" "${scenario_dir}/focused-empty-crop.png"
assert_changed "${scenario_dir}/restart-0-crop.png" "${scenario_dir}/focused-empty-crop.png" \
  "Rounds exact-zero focus clear"
xdotool mousemove "$(menu_x 900)" "$(menu_y 450)" click 1
sleep 0.15
capture "${scenario_dir}/empty-blur-0.png"
crop_rounds_control "${scenario_dir}/empty-blur-0.png" "${scenario_dir}/empty-blur-0-crop.png"
assert_same "${scenario_dir}/restart-0-crop.png" "${scenario_dir}/empty-blur-0-crop.png" \
  "Rounds empty blur zero restore"
dump_rounds_setting rounds-after-empty-blur.txt
assert_dumped_rounds "${scenario_dir}/rounds-after-empty-blur.txt" 0 "empty blur unlimited"
close_app

echo "[RUN] invalid background load retry and solid-black fallback"
new_scenario background-fallback
python3 - "${scenario_dir}/textures/menu-backgrounds" <<'PY'
import os
import sys

directory = sys.argv[1]
for name in os.listdir(directory):
    path = os.path.join(directory, name)
    if os.path.isfile(path):
        os.remove(path)
for name in ("broken-first.png", "broken-second.JPG"):
    with open(os.path.join(directory, name), "wb") as output:
        output.write(b"not an image")
with open(os.path.join(directory, "ignored.txt"), "w", encoding="utf-8") as output:
    output.write("not eligible")
PY
start_app
capture "${scenario_dir}/fallback.png"
corner="$(identify -format '%[pixel:p{0,0}]' "${scenario_dir}/fallback.png")"
[[ "$corner" == *"(0,0,0"* ]] || fail "background fallback is not solid black at viewport edge"
close_app

echo "Menu redesign behavior test passed. Artifacts: $test_root"
