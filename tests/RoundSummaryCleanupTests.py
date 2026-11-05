#!/usr/bin/env python3
"""One non-game failure-path regression using the actual harness EXIT trap."""
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest


class CleanupTests(unittest.TestCase):
    def test_manifest_failure_cannot_skip_release_kill_reap_or_replace_status(self):
        workspace = Path(__file__).resolve().parents[1]
        script = (workspace / 'tests/RoundSummaryProgressBehaviorTests.sh').read_text()
        # Execute the real setup/functions/trap, stopping before any Xvfb/game
        # launch. The two owned children below are ordinary sleep processes.
        preamble, marker, _ = script.partition('\nXvfb "$display" -screen')
        self.assertTrue(marker)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'build').mkdir()
            binary = root / 'build/duel6r'
            binary.write_text('#!/bin/sh\nexit 99\n')
            binary.chmod(0o755)
            payload = r'''
events="$TEST_ROOT/order.log"
xdotool() { printf 'release %s\n' "$*" >>"$events"; }
kill() {
    if [[ "$1" != -0 ]]; then printf 'kill %s\n' "$*" >>"$events"; fi
    builtin kill "$@"
}
wait() {
    printf 'wait %s\n' "$*" >>"$events"
    builtin wait "$@"
    local result=$?
    printf 'reaped %s\n' "$*" >>"$events"
    return "$result"
}
convert() { printf 'diagnostic-convert\n' >>"$events"; command convert "$@"; }
/bin/sleep 60 >/dev/null 2>&1 & app_pid=$!
/bin/sleep 60 >/dev/null 2>&1 & xvfb_pid=$!
printf '%s\n%s\n' "$app_pid" "$xvfb_pid" >"$TEST_ROOT/children"
tab_held=true
mkdir "$TEST_ROOT/first"
printf 'P6\n1 1\n255\n\377\0\0' >"$TEST_ROOT/first/summary-candidate-01.ppm"
rm "$D6R_SCREENSHOT_MANIFEST"
mkdir "$D6R_SCREENSHOT_MANIFEST" # actual manifest append will fail, not convert
exit 37
'''
            env = dict(os.environ, WORKSPACE_DIR=str(workspace), BUILD_DIR=str(root / 'build'),
                       TEST_ROOT=str(root / 'diagnostic'), D6R_ROUND_SUMMARY_FIRST_ONLY='0')
            process = subprocess.Popen(['bash', '-c', preamble + payload], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
            try:
                stdout, stderr = process.communicate(timeout=10)
                self.assertEqual(37, process.returncode, (stdout, stderr))
                self.assertIn(b'Is a directory', stderr)
                self.assertIn(b'candidate preservation failed', stderr)
                app, display = (root / 'diagnostic/children').read_text().splitlines()
                self.assertEqual([
                    'release keyup Tab', 'kill ' + app, 'wait ' + app, 'reaped ' + app,
                    'kill ' + display, 'wait ' + display, 'reaped ' + display,
                    'diagnostic-convert'], (root / 'diagnostic/order.log').read_text().splitlines())
                self.assertTrue((root / 'diagnostic/first/summary-candidate-01.png').is_file())
                for pid in (app, display):
                    with self.assertRaises(ProcessLookupError): os.kill(int(pid), 0)
            finally:
                # Test-owned safety net only; a skipped production cleanup must
                # fail the assertions above, not leave these synthetic children.
                try: os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError: pass
                process.wait(timeout=2)


if __name__ == '__main__': unittest.main()
