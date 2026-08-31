#!/usr/bin/env bash
set -euo pipefail

# Runtime regression harness for the shared-arena multiplayer view. This test is
# intentionally run on demand because it starts the real SDL/OpenGL application
# eight times under Xvfb. It also exercises the consolidated mode selector and
# its conditional Team settings before each shared-arena match starts.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${build_dir}}"
test_root="${TEST_ROOT:-${build_dir}/shared-arena-behavior}"
display="${DISPLAY:-:98}"
image_assertions="${workspace_dir}/tests/SharedArenaImageAssertions.py"

fail() {
    echo "shared-arena-behavior: $*" >&2
    exit 1
}

for command in Xvfb xdotool import identify convert compare python3 timeout; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing: ${build_dir}/duel6r"
[[ -f "$image_assertions" ]] || fail "image assertion helper is missing: $image_assertions"
[[ -d "${resource_dir}/data" ]] || fail "application resources are missing: $resource_dir"

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

assert_images_differ() {
    local first="$1" second="$2" label="$3" distance
    distance="$(image_distance "$first" "$second")"
    python3 - "$distance" "$label" <<'PY'
import sys
if float(sys.argv[1]) < 0.01:
    raise SystemExit(f"{sys.argv[2]} did not visibly change ({sys.argv[1]})")
PY
}

assert_images_same() {
    local first="$1" second="$2" label="$3" distance
    distance="$(image_distance "$first" "$second")"
    python3 - "$distance" "$label" <<'PY'
import sys
if float(sys.argv[1]) != 0:
    raise SystemExit(f"{sys.argv[2]} changed unexpectedly ({sys.argv[1]})")
PY
}

crop_mode() {
    convert "$1" -crop 240x36+934+180 +repage "$2"
}

crop_team_settings() {
    # Includes Num. of Team, Friendly Fire, and the roster rows whose colors
    # must update immediately when the team count changes.
    convert "$1" -crop 600x190+585+180 +repage "$2"
}

crop_conditional_settings() {
    # Restrict mode-layout comparisons to the settings panel so scenarios with
    # different roster sizes remain directly comparable.
    convert "$1" -crop 240x110+930+235 +repage "$2"
}

assert_roster_team_colors() {
    local screenshot="$1" team_count="$2" player_count="$3" label="$4"
    local normalized="${screenshot%.png}-normalized.png"
    convert "$screenshot" -crop 1093x900+93+0 +repage -filter point -resize 850x700! "$normalized"
    python3 - "$normalized" "$team_count" "$player_count" "$label" <<'PY'
import subprocess
import sys

image, team_count, player_count, label = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
expected = [(255, 0, 0), (0, 255, 0), (255, 255, 0), (255, 0, 255)]
for index in range(player_count):
    # Sample clear of the player name in the center of each 18-pixel row.
    x, y = 470, 157 + index * 18
    pixel = subprocess.check_output([
        "convert", image, "-crop", f"1x1+{x}+{y}", "+repage",
        "-alpha", "off", "-depth", "8", "rgb:-",
    ])
    actual = tuple(pixel)
    wanted = expected[index % team_count]
    if actual != wanted:
        raise SystemExit(
            f"{label}: roster row {index} expected team color {wanted}, got {actual}"
        )
print(f"{label}: {player_count} roster rows follow index modulo {team_count}")
PY
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

primary_mode_crops=()
teams_mode_crop=""
non_team_settings_crop=""

run_scenario() {
    local mode_index="$1"
    local player_count="$2"
    local team_count="$3"
    local friendly_fire="$4"
    local label="$5"
    local scenario_dir="${test_root}/${label}"
    local runtime_dir="${scenario_dir}/runtime"
    mkdir -p "${runtime_dir}/data"
    cp "${build_dir}/duel6r" "${runtime_dir}/duel6r"
    cp -R "${resource_dir}/data/." "${runtime_dir}/data/"
    cp -R "${resource_dir}/levels" "${resource_dir}/profiles" "${resource_dir}/shaders" \
        "${resource_dir}/sound" "${resource_dir}/textures" "$runtime_dir/"
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

    # The logical mode spinner is scaled by 9/7 and centered at X=93 in this
    # 1280x900 Release viewport. Exercise the rendered arrow's aligned hitbox.
    for ((i = 0; i < mode_index; i++)); do
        import -window root "${scenario_dir}/mode-step-before.png"
        crop_mode "${scenario_dir}/mode-step-before.png" "${scenario_dir}/mode-step-before-crop.png"
        local mode_advanced=false
        for _ in {1..3}; do
            xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
            sleep 0.2
            import -window root "${scenario_dir}/mode-step-after.png"
            crop_mode "${scenario_dir}/mode-step-after.png" "${scenario_dir}/mode-step-after-crop.png"
            step_delta="$(image_distance "${scenario_dir}/mode-step-before-crop.png" \
                "${scenario_dir}/mode-step-after-crop.png")"
            if python3 - "$step_delta" <<'PY'
import sys
raise SystemExit(0 if float(sys.argv[1]) >= 0.01 else 1)
PY
            then
                mode_advanced=true
                break
            fi
        done
        [[ "$mode_advanced" == true ]] || fail "$label mode selector ignored repeated pointer clicks"
    done

    import -window root "${scenario_dir}/primary-mode-selected.png"
    crop_mode "${scenario_dir}/primary-mode-selected.png" "${scenario_dir}/primary-mode-crop.png"
    crop_conditional_settings "${scenario_dir}/primary-mode-selected.png" \
        "${scenario_dir}/settings-crop.png"

    if (( mode_index < 2 )); then
        for previous_crop in "${primary_mode_crops[@]}"; do
            assert_images_differ "$previous_crop" "${scenario_dir}/primary-mode-crop.png" \
                "$label primary mode"
        done
        primary_mode_crops+=("${scenario_dir}/primary-mode-crop.png")
        if [[ -z "$non_team_settings_crop" ]]; then
            non_team_settings_crop="${scenario_dir}/settings-crop.png"
        else
            assert_images_same "$non_team_settings_crop" "${scenario_dir}/settings-crop.png" \
                "$label hidden Team controls"
        fi
    else
        if [[ -z "$teams_mode_crop" ]]; then
            teams_mode_crop="${scenario_dir}/primary-mode-crop.png"
            assert_images_differ "$non_team_settings_crop" "${scenario_dir}/settings-crop.png" \
                "Teams controls become visible"

            # Teams is the third and final primary option: another right-arrow
            # activation must not expose one of the obsolete six entries.
            xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
            sleep 0.2
            import -window root "${scenario_dir}/past-teams.png"
            crop_mode "${scenario_dir}/past-teams.png" "${scenario_dir}/past-teams-crop.png"
            assert_images_same "$teams_mode_crop" "${scenario_dir}/past-teams-crop.png" \
                "Teams is the final primary mode"
        else
            assert_images_same "$teams_mode_crop" "${scenario_dir}/primary-mode-crop.png" \
                "$label uses the single Teams primary option"
        fi

        # Defaults are two teams and Friendly Fire off. Advancing from those
        # defaults produces all requested 2/3/4 x off/on combinations.
        for ((i = 2; i < team_count; i++)); do
            import -window root "${scenario_dir}/team-count-before-${i}.png"
            xdotool mousemove 1157 244 mousedown 1 sleep 0.08 mouseup 1
            sleep 0.2
            import -window root "${scenario_dir}/team-count-after-${i}.png"
            crop_team_settings "${scenario_dir}/team-count-before-${i}.png" \
                "${scenario_dir}/team-count-before-${i}-crop.png"
            crop_team_settings "${scenario_dir}/team-count-after-${i}.png" \
                "${scenario_dir}/team-count-after-${i}-crop.png"
            assert_images_differ "${scenario_dir}/team-count-before-${i}-crop.png" \
                "${scenario_dir}/team-count-after-${i}-crop.png" "$label team-count step"
        done
        if [[ "$friendly_fire" == "on" ]]; then
            import -window root "${scenario_dir}/friendly-fire-before.png"
            xdotool mousemove 950 283 click 1
            sleep 0.2
            import -window root "${scenario_dir}/friendly-fire-after.png"
            crop_team_settings "${scenario_dir}/friendly-fire-before.png" \
                "${scenario_dir}/friendly-fire-before-crop.png"
            crop_team_settings "${scenario_dir}/friendly-fire-after.png" \
                "${scenario_dir}/friendly-fire-after-crop.png"
            assert_images_differ "${scenario_dir}/friendly-fire-before-crop.png" \
                "${scenario_dir}/friendly-fire-after-crop.png" "$label Friendly Fire on"
        fi

        import -window root "${scenario_dir}/team-settings-selected.png"
        crop_team_settings "${scenario_dir}/team-settings-selected.png" \
            "${scenario_dir}/team-settings-selected-crop.png"
        crop_conditional_settings "${scenario_dir}/team-settings-selected.png" \
            "${scenario_dir}/team-settings-selected-conditional-crop.png"
        assert_roster_team_colors "${scenario_dir}/team-settings-selected.png" "$team_count" \
            "$player_count" "$label immediate roster colors"

        if [[ "$label" == "team4-ff-on-15" ]]; then
            # Switch away, prove the conditional controls disappear, then
            # click both hidden controls at their former hit locations. The
            # selected values and roster coloring must survive those clicks.
            xdotool mousemove 949 217 mousedown 1 sleep 0.08 mouseup 1
            sleep 0.2
            import -window root "${scenario_dir}/teams-hidden.png"
            crop_conditional_settings "${scenario_dir}/teams-hidden.png" \
                "${scenario_dir}/teams-hidden-crop.png"
            assert_images_same "$non_team_settings_crop" "${scenario_dir}/teams-hidden-crop.png" \
                "$label conditional Team controls hidden"
            xdotool mousemove 1157 244 mousedown 1 sleep 0.08 mouseup 1
            xdotool mousemove 950 283 click 1
            xdotool mousemove 1157 217 mousedown 1 sleep 0.08 mouseup 1
            sleep 0.2
            import -window root "${scenario_dir}/teams-restored.png"
            crop_team_settings "${scenario_dir}/teams-restored.png" \
                "${scenario_dir}/teams-restored-crop.png"
            crop_conditional_settings "${scenario_dir}/teams-restored.png" \
                "${scenario_dir}/teams-restored-conditional-crop.png"
            assert_images_same "${scenario_dir}/team-settings-selected-conditional-crop.png" \
                "${scenario_dir}/teams-restored-conditional-crop.png" \
                "$label retained values and hidden-control input exclusion"
            assert_roster_team_colors "${scenario_dir}/teams-restored.png" "$team_count" \
                "$player_count" "$label retained roster colors"
        fi
    fi

    # Disable Quick Liquid so the test can inspect live rankings and open the
    # score overlay before an unattended match reaches sudden death.
    # Click the label area, clear of the per-player controller-detect buttons
    # that overlap the checkbox's left edge for larger rosters.
    local quick_liquid_y=298
    if (( mode_index == 2 )); then
        quick_liquid_y=367
    fi
    xdotool mousemove 973 "$quick_liquid_y" mousedown 1 sleep 0.08 mouseup 1
    sleep 0.2
    import -window root "${scenario_dir}/mode-selected.png"
    crop_mode "${scenario_dir}/mode-selected.png" "${scenario_dir}/mode-crop.png"

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

    python3 "$image_assertions" "${scenario_dir}/after-f2.png" "$label-after-f2" \
        "$player_count" "$team_count" --viewport-only
    python3 "$image_assertions" "${scenario_dir}/after-console-command.png" "$label-after-console" \
        "$player_count" "$team_count" --viewport-only

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
    python3 "$image_assertions" "${scenario_dir}/after-console-command.png" "$label-live-ranking" \
        "$player_count" "$team_count" --without-ranking "${scenario_dir}/ranking-toggled.png"
    xdotool key --window "$window_id" Tab
    score_assertion=""
    score_ready=false
    for _ in {1..30}; do
        sleep 0.1
        import -window root "${scenario_dir}/score-tab.png"
        if score_assertion="$(python3 "$image_assertions" "${scenario_dir}/score-tab.png" \
                "$label-score-tab" "$player_count" "$team_count" --score 2>&1)"; then
            score_ready=true
            printf '%s\n' "$score_assertion"
            break
        fi
    done
    [[ "$score_ready" == true ]] || fail "$score_assertion"
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
    sleep 1
    if (( mode_index == 2 )); then
        import -window root "${scenario_dir}/after-gameplay-return.png"
        crop_team_settings "${scenario_dir}/after-gameplay-return.png" \
            "${scenario_dir}/after-gameplay-return-crop.png"
        crop_conditional_settings "${scenario_dir}/after-gameplay-return.png" \
            "${scenario_dir}/after-gameplay-return-conditional-crop.png"
        assert_images_same "${scenario_dir}/team-settings-selected-conditional-crop.png" \
            "${scenario_dir}/after-gameplay-return-conditional-crop.png" \
            "$label Team settings after gameplay return"
        assert_roster_team_colors "${scenario_dir}/after-gameplay-return.png" "$team_count" \
            "$player_count" "$label roster colors after gameplay return"
        crop_mode "${scenario_dir}/after-gameplay-return.png" \
            "${scenario_dir}/after-gameplay-return-mode-crop.png"
        assert_images_same "$teams_mode_crop" "${scenario_dir}/after-gameplay-return-mode-crop.png" \
            "$label Teams mode after gameplay return"
    fi
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
run_scenario 0 2 0 off deathmatch-2
run_scenario 1 3 0 off predator-3
run_scenario 2 4 2 off team2-ff-off-4
run_scenario 2 4 2 on  team2-ff-on-4
run_scenario 2 6 3 off team3-ff-off-6
run_scenario 2 6 3 on  team3-ff-on-6
run_scenario 2 8 4 off team4-ff-off-8
run_scenario 2 15 4 on team4-ff-on-15

echo "Shared arena behavior test passed. Artifacts: ${test_root}"
