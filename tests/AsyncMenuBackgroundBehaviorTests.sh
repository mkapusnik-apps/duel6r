#!/usr/bin/env bash
set -euo pipefail

# Black-box regression coverage for asynchronous menu background preparation.
# A test-only preload shim delays the first background file open without
# delaying SDL window creation or the render/event thread.

workspace_dir="${WORKSPACE_DIR:-/workspace}"
build_dir="${BUILD_DIR:-${workspace_dir}/build}"
resource_dir="${RESOURCE_DIR:-${workspace_dir}/resources}"
test_root="${TEST_ROOT:-${build_dir}/async-menu-background-behavior}"
display="${DISPLAY:-:95}"

fail() {
    echo "async-menu-background-behavior: FAIL: $*" >&2
    exit 1
}

for command in Xvfb xdotool import compare convert identify timeout cc python3; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done
[[ -x "${build_dir}/duel6r" ]] || fail "application binary is missing"

rm -rf "$test_root"
mkdir -p "$test_root"

cat >"${test_root}/delay-background-open.c" <<'C'
#define _GNU_SOURCE
#include <dlfcn.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int delayed = 0;
static int background_open_seen = 0;
static int upload_failure_injected = 0;
static int pending_gl_error = 0;

static int prepare_background_open(const char *path) {
    if (path == NULL || strstr(path, "textures/menu-backgrounds/") == NULL) {
        return 0;
    }
    if (getenv("D6R_QA_BACKGROUND_FAIL_FIRST_UPLOAD") != NULL) {
        dprintf(STDERR_FILENO, "QA background candidate opened: %s\n", path);
    }
    if (__atomic_exchange_n(&delayed, 1, __ATOMIC_SEQ_CST)) return 0;
    __atomic_store_n(&background_open_seen, 1, __ATOMIC_SEQ_CST);
    if (getenv("D6R_QA_BACKGROUND_FAIL_FIRST") != NULL) {
        write(STDERR_FILENO, "QA first background open failed\n", 32);
        return 1;
    }
    const char *value = getenv("D6R_QA_BACKGROUND_DELAY_MS");
    long milliseconds = value == NULL ? 2500 : strtol(value, NULL, 10);
    struct timespec duration = {milliseconds / 1000, (milliseconds % 1000) * 1000000L};
    write(STDERR_FILENO, "QA background open delayed\n", 27);
    nanosleep(&duration, NULL);
    return 0;
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*real_fopen)(const char *, const char *) = NULL;
    if (real_fopen == NULL) real_fopen = dlsym(RTLD_NEXT, "fopen");
    if (prepare_background_open(path)) return NULL;
    return real_fopen(path, mode);
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*real_fopen64)(const char *, const char *) = NULL;
    if (real_fopen64 == NULL) real_fopen64 = dlsym(RTLD_NEXT, "fopen64");
    if (prepare_background_open(path)) return NULL;
    return real_fopen64(path, mode);
}

GLboolean glIsTexture(GLuint texture) {
    static GLboolean (*real_gl_is_texture)(GLuint) = NULL;
    static void (*real_gl_get_tex_level_parameter)(GLenum, GLint, GLenum, GLint *) = NULL;
    static const GLubyte *(*real_gl_get_string)(GLenum) = NULL;
    if (real_gl_is_texture == NULL) real_gl_is_texture = dlsym(RTLD_NEXT, "glIsTexture");
    if (real_gl_get_tex_level_parameter == NULL) {
        real_gl_get_tex_level_parameter = dlsym(RTLD_NEXT, "glGetTexLevelParameteriv");
    }
    if (real_gl_get_string == NULL) real_gl_get_string = dlsym(RTLD_NEXT, "glGetString");
    GLboolean result = real_gl_is_texture(texture);
    GLint width = 0;
    GLint height = 0;
    real_gl_get_tex_level_parameter(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    real_gl_get_tex_level_parameter(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    const char *version = (const char *) real_gl_get_string(GL_VERSION);
    if ((width != 1280 || height != 900) &&
        (version == NULL || strstr(version, "OpenGL ES 2.") == NULL)) {
        real_gl_get_tex_level_parameter(0x8C1A /* GL_TEXTURE_2D_ARRAY */, 0, GL_TEXTURE_WIDTH, &width);
        real_gl_get_tex_level_parameter(0x8C1A /* GL_TEXTURE_2D_ARRAY */, 0, GL_TEXTURE_HEIGHT, &height);
    }
    if (result == GL_TRUE && getenv("D6R_QA_BACKGROUND_FAIL_FIRST_UPLOAD") != NULL &&
        __atomic_load_n(&background_open_seen, __ATOMIC_SEQ_CST) &&
        width == 1280 && height == 900 &&
        !__atomic_exchange_n(&upload_failure_injected, 1, __ATOMIC_SEQ_CST)) {
        __atomic_store_n(&pending_gl_error, 1, __ATOMIC_SEQ_CST);
        write(STDERR_FILENO, "QA first background upload failed\n", 34);
        return GL_FALSE;
    }
    return result;
}

GLenum glGetError(void) {
    static GLenum (*real_gl_get_error)(void) = NULL;
    if (real_gl_get_error == NULL) real_gl_get_error = dlsym(RTLD_NEXT, "glGetError");
    if (__atomic_exchange_n(&pending_gl_error, 0, __ATOMIC_SEQ_CST)) {
        return GL_OUT_OF_MEMORY;
    }
    return real_gl_get_error();
}
C
cc -shared -fPIC -o "${test_root}/delay-background-open.so" \
    "${test_root}/delay-background-open.c" -ldl

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
xdotool getdisplaygeometry >/dev/null 2>&1 || fail "Xvfb did not become ready"

new_scenario() {
    scenario_dir="${test_root}/$1"
    mkdir -p "$scenario_dir"
    cp "${build_dir}/duel6r" "$scenario_dir/duel6r"
    cp -a "${resource_dir}/." "$scenario_dir/"
    rm -f "${scenario_dir}/data/persons.json"
    mkdir -p "${scenario_dir}/home"
}

start_app() {
    local delay_ms="$1"
    local fail_first="${2:-}"
    local fail_first_upload="${3:-}"
    (
        cd "$scenario_dir"
        export HOME="${scenario_dir}/home"
        export XDG_CACHE_HOME="$HOME/.cache" XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
        export LD_PRELOAD="${test_root}/delay-background-open.so"
        export D6R_QA_BACKGROUND_DELAY_MS="$delay_ms"
        if [[ "$fail_first" == true ]]; then
            export D6R_QA_BACKGROUND_FAIL_FIRST=1
        else
            unset D6R_QA_BACKGROUND_FAIL_FIRST
        fi
        if [[ "$fail_first_upload" == true ]]; then
            export D6R_QA_BACKGROUND_FAIL_FIRST_UPLOAD=1
        else
            unset D6R_QA_BACKGROUND_FAIL_FIRST_UPLOAD
        fi
        timeout --kill-after=3s 20s ./duel6r >app.stdout 2>app.stderr
    ) &
    app_pid="$!"
    window_id=""
    for _ in {1..120}; do
        window_id="$(xdotool search --onlyvisible --name 'Duel 6 Reloaded' 2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        sleep 0.05
    done
    [[ -n "$window_id" ]] || fail "application window did not appear while background open was delayed"
    xdotool windowfocus "$window_id" windowactivate "$window_id" >/dev/null 2>&1 || true
}

changed_pixels() {
    local first="$1" second="$2" metric status
    set +e
    metric="$(compare -metric AE "$first" "$second" null: 2>&1)"
    status=$?
    set -e
    (( status <= 1 )) || fail "ImageMagick comparison failed"
    printf '%s' "${metric%%.*}"
}

background_strip_mean() {
    convert "$1" -crop 80x900+0+0 -format '%[fx:mean]' info:
}

wait_for_non_black_background() {
    local destination="$1"
    background_ready=false
    for _ in {1..120}; do
        sleep 0.1
        import -window root "$destination"
        background_mean="$(background_strip_mean "$destination")"
        if python3 - "$background_mean" <<'PY'
import sys
raise SystemExit(0 if float(sys.argv[1]) > 0.01 else 1)
PY
        then
            background_ready=true
            break
        fi
    done
    [[ "$background_ready" == true ]] || fail "background preparation did not reach a published state"
}

close_cleanly() {
    xdotool key --window "$window_id" Escape
    set +e
    wait "$app_pid"
    status=$?
    set -e
    app_pid=""
    (( status == 0 )) || fail "$1 did not close cleanly: $status"
}

echo "[RUN] delayed preparation leaves startup responsive and transitions from black"
new_scenario delayed-success
start_app 3000
import -window root "${scenario_dir}/initial.png"
corner_sum="$(identify -format '%[fx:int(255*r+255*g+255*b)]' "${scenario_dir}/initial.png")"
(( corner_sum == 0 )) || fail "initial delayed frame did not retain solid black"

menu_ready=false
for _ in {1..20}; do
    sleep 0.05
    import -window root "${scenario_dir}/menu-ready.png"
    menu_delta="$(changed_pixels "${scenario_dir}/initial.png" "${scenario_dir}/menu-ready.png")"
    if (( menu_delta >= 10000 )); then
        menu_ready=true
        break
    fi
done
[[ "$menu_ready" == true ]] || fail "menu controls did not render promptly during background preparation"

xdotool key --window "$window_id" grave
sleep 0.15
import -window root "${scenario_dir}/console.png"
console_delta="$(changed_pixels "${scenario_dir}/menu-ready.png" "${scenario_dir}/console.png")"
(( console_delta >= 1000 )) || fail "menu did not respond to console input during background preparation"
xdotool key --window "$window_id" grave

transitioned=false
for _ in {1..120}; do
    sleep 0.1
    import -window root "${scenario_dir}/prepared.png"
    background_delta="$(changed_pixels "${scenario_dir}/menu-ready.png" "${scenario_dir}/prepared.png")"
    if (( background_delta >= 100000 )); then
        transitioned=true
        break
    fi
done
[[ "$transitioned" == true ]] || fail "prepared background was not published after the delay"
grep -q 'QA background open delayed' "${scenario_dir}/app.stderr" \
    || fail "test delay was not exercised"
xdotool key --window "$window_id" Escape
set +e
wait "$app_pid"
status=$?
set -e
app_pid=""
(( status == 0 )) || fail "application did not close cleanly after background publication: $status"
echo "delayed-success: console-changed-pixels=${console_delta} background-changed-pixels=${background_delta}"

echo "[RUN] a failed decode retries another candidate and publishes it"
new_scenario retry-success
start_app 0 true
wait_for_non_black_background "${scenario_dir}/retry-prepared.png"
grep -q 'QA first background open failed' "${scenario_dir}/app.stderr" \
    || fail "forced first-candidate failure was not exercised"
close_cleanly "successful decode retry"
echo "retry-success: status=${status} background-strip-mean=${background_mean}"

echo "[RUN] an upload error is rejected and another candidate is published"
new_scenario upload-retry-success
start_app 0 false true
wait_for_non_black_background "${scenario_dir}/upload-retry-prepared.png"
grep -q 'QA first background upload failed' "${scenario_dir}/app.stderr" \
    || fail "forced first-upload GL error was not exercised"
upload_candidate_count="$(grep -c 'QA background candidate opened:' "${scenario_dir}/app.stderr")"
(( upload_candidate_count >= 2 )) \
    || fail "upload failure did not retry a second background candidate"
close_cleanly "successful upload retry"
echo "upload-retry-success: status=${status} candidates-opened=${upload_candidate_count} background-strip-mean=${background_mean}"

echo "[RUN] exhausted invalid candidates reach terminal black fallback"
new_scenario fallback-terminal
rm -f "${scenario_dir}/textures/menu-backgrounds/"*
printf 'not an image' >"${scenario_dir}/textures/menu-backgrounds/broken-first.png"
printf 'also not an image' >"${scenario_dir}/textures/menu-backgrounds/broken-second.jpg"
start_app 5000
import -window root "${scenario_dir}/fallback-initial.png"
fallback_menu_ready=false
for _ in {1..20}; do
    sleep 0.05
    import -window root "${scenario_dir}/fallback-menu-ready.png"
    fallback_menu_delta="$(changed_pixels "${scenario_dir}/fallback-initial.png" \
        "${scenario_dir}/fallback-menu-ready.png")"
    if (( fallback_menu_delta >= 10000 )); then
        fallback_menu_ready=true
        break
    fi
done
[[ "$fallback_menu_ready" == true ]] \
    || fail "fallback menu did not render while preparation remained pending"
xdotool key --window "$window_id" grave
sleep 0.15
import -window root "${scenario_dir}/fallback-pending-console.png"
pending_console_delta="$(changed_pixels "${scenario_dir}/fallback-menu-ready.png" \
    "${scenario_dir}/fallback-pending-console.png")"
(( pending_console_delta >= 1000 )) || fail "fallback pending console did not open"
fallback_terminal=false
for _ in {1..70}; do
    sleep 0.1
    import -window root "${scenario_dir}/fallback-terminal-console.png"
    diagnostic_delta="$(changed_pixels "${scenario_dir}/fallback-pending-console.png" \
        "${scenario_dir}/fallback-terminal-console.png")"
    if (( diagnostic_delta >= 500 )); then
        fallback_terminal=true
        break
    fi
done
[[ "$fallback_terminal" == true ]] \
    || fail "fallback did not publish its terminal console diagnostics"
xdotool key --window "$window_id" grave
sleep 0.15
import -window root "${scenario_dir}/fallback-terminal.png"
fallback_mean="$(background_strip_mean "${scenario_dir}/fallback-terminal.png")"
python3 - "$fallback_mean" <<'PY'
import sys
if float(sys.argv[1]) > 0.001:
    raise SystemExit("terminal fallback did not remain solid black")
PY
grep -q 'QA background open delayed' "${scenario_dir}/app.stderr" \
    || fail "fallback scenario did not observe pending preparation"
close_cleanly "terminal fallback"
echo "fallback-terminal: status=${status} diagnostic-changed-pixels=${diagnostic_delta} background-strip-mean=${fallback_mean}"

echo "[RUN] shutdown is safe while preparation remains active"
new_scenario delayed-shutdown
start_app 4000
start_ms="$(date +%s%3N)"
xdotool key --window "$window_id" Escape
set +e
wait "$app_pid"
status=$?
set -e
app_pid=""
elapsed_ms=$(( $(date +%s%3N) - start_ms ))
(( status == 0 )) || fail "application crashed or timed out during active preparation shutdown: $status"
(( elapsed_ms < 8000 )) || fail "active preparation shutdown did not complete safely: ${elapsed_ms}ms"
grep -q 'QA background open delayed' "${scenario_dir}/app.stderr" \
    || fail "shutdown scenario did not exercise active preparation"
echo "delayed-shutdown: status=${status} elapsed-ms=${elapsed_ms}"

echo "Async menu background behavior test passed. Artifacts: $test_root"
