#!/usr/bin/env python3
import json
import ctypes
import errno
import io
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from contextlib import nullcontext, redirect_stderr
from unittest.mock import patch
from OwnedProcessRunner import reap, signal_tree

RUNNER = Path(__file__).with_name("OwnedProcessRunner.py")

# A shell-like leader, a TERM-resistant child and a new-session grandchild.
# The readiness file is written only after both descendants install handlers.
FIXTURE = r'''
import os, signal, sys, time
from pathlib import Path
root = Path(sys.argv[1])
child = os.fork()
if child == 0:
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    grandchild = os.fork()
    if grandchild == 0:
        os.setsid()
        (root / "grandchild").write_text(str(os.getpid()))
        while True: signal.pause()
    (root / "child").write_text(str(os.getpid()))
    while True: signal.pause()
while not (root / "grandchild").exists(): time.sleep(.01)
(root / "ready").write_text(str(os.getpid()))
if sys.argv[2] == "normal": sys.exit(0)
while True: signal.pause()
'''


class OwnershipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # If the runner regresses, the test still owns its escaped fixtures.
        # Assertions below run before this emergency teardown, never credit it
        # as evidence that the runner performed cleanup.
        if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), "cannot enable fixture subreaper")

    def scenario(self, mode):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            log = root / "process.log"
            command = [sys.executable, str(RUNNER), "--timeout", "2", "--log", str(log),
                       "--", sys.executable, "-c", FIXTURE, str(root), mode]
            if mode == "exception":
                # Inject a monitor exception after child launch, not a launch error.
                command = [sys.executable, "-c", r'''
import sys, time
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, sys.argv[1])
from OwnedProcessRunner import run_owned
def broken(*args):
    end = time.monotonic() + 2
    while not (Path(sys.argv[2]) / "ready").exists() and time.monotonic() < end: time.sleep(.01)
    raise RuntimeError("injected monitor failure")
with patch("os.waitid", broken):
    run_owned(sys.argv[4:], 2, sys.argv[3])
''', str(RUNNER.parent), str(root), str(log), sys.executable, "-c", FIXTURE, str(root), mode]
            if mode.startswith("diagnostic-"):
                sink, event = mode.removeprefix("diagnostic-").split(":", 1)
                command = [sys.executable, "-c", r'''
import builtins, errno, json, os, sys
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, sys.argv[1])
from OwnedProcessRunner import run_owned
root, log, sink, event = Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4], sys.argv[5]
original_open, original_print, original_waitpid = Path.open, builtins.print, os.waitpid
injected, reaped = [], []
broken = False
def emit(payload, kind):
    global broken
    if kind != sink: return
    if json.loads(payload)["event"] == event: broken = True
    if broken:
        assert (root / "ready").exists(), "injection preceded child readiness"
        injected.append(kind)
        if kind == "log": raise OSError(errno.ENOSPC, "injected full diagnostic disk")
        raise BrokenPipeError(errno.EPIPE, "injected closed stdout")
class LogWriter:
    def __init__(self, stream): self.stream = stream
    def __enter__(self): return self
    def __exit__(self, *args): return self.stream.__exit__(*args)
    def write(self, payload):
        emit(payload, "log")
        return self.stream.write(payload)
def open_log(path, *args, **kwargs):
    stream = original_open(path, *args, **kwargs)
    return LogWriter(stream) if path == log else stream
def print_record(payload, **kwargs):
    emit(payload, "stdout")
    return original_print(payload, **kwargs)
def observe_reap(*args):
    pid, status = original_waitpid(*args)
    if pid: reaped.append(pid)
    return pid, status
with patch.object(Path, "open", open_log), patch("builtins.print", print_record), patch("os.waitpid", observe_reap):
    try:
        run_owned(sys.argv[6:], 2, log)
    except RuntimeError as error:
        assert "diagnostics failed after cleanup=True" in str(error), str(error)
    else:
        raise AssertionError("diagnostic failure was silently accepted")
try:
    original_waitpid(-1, os.WNOHANG)
except ChildProcessError:
    complete = True
else:
    complete = False
(root / "verification.json").write_text(json.dumps({"cleanup": complete, "remaining": [],
    "phase": "diagnostic-failure", "reaped": {pid: True for pid in reaped}, "injected": injected}))
sys.exit(1)  # diagnostic failure must remain a failing run even after safe cleanup
''', str(RUNNER.parent), str(root), str(log), sink, event,
                           sys.executable, "-c", FIXTURE, str(root), "normal"]
            if mode.startswith("stat-"):
                command = [sys.executable, "-c", r'''
import errno, json, sys
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, sys.argv[1])
from OwnedProcessRunner import run_owned
root, log, kind = Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4]
error = {"missing": FileNotFoundError(errno.ENOENT, "injected missing stat"),
         "gone": ProcessLookupError(errno.ESRCH, "injected exited process"),
         "denied": PermissionError(errno.EACCES, "injected denied stat"),
         "io": OSError(errno.EIO, "injected stat I/O failure")}[kind]
original_read = Path.read_text
injected = []
def read_stat(path, *args, **kwargs):
    if path.parent.parent == Path("/proc") and path.name == "stat":
        assert (root / "ready").exists(), "injection preceded child readiness"
        injected.append(int(path.parent.name))
        raise error
    return original_read(path, *args, **kwargs)
try:
    with patch.object(Path, "read_text", read_stat):
        run_owned(sys.argv[5:], 2, log)
except OSError as caught:
    assert caught is error, repr(caught)
    raise
finally:
    (root / "injected.json").write_text(json.dumps(injected))
''', str(RUNNER.parent), str(root), str(log), mode.removeprefix("stat-"),
                            sys.executable, "-c", FIXTURE, str(root), "normal"]
            if mode == "pidfd-error":
                command = [sys.executable, "-c", r'''
import errno, json, os, signal, sys
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, sys.argv[1])
from OwnedProcessRunner import run_owned
root, log = Path(sys.argv[2]), Path(sys.argv[3])
original_open, original_send = os.pidfd_open, signal.pidfd_send_signal
failure = OSError(errno.EINVAL, "injected unanchorable owned target")
injected, signalled, anchored = [], [], {}
def open_pidfd(pid, flags=0):
    target = int((root / "ready").read_text())
    if pid == target:
        injected.append(pid)
        raise failure
    fd = original_open(pid, flags)
    anchored[fd] = pid
    return fd
def send_pidfd(fd, sig):
    assert fd in anchored, "unanchored signal"
    signalled.append(anchored[fd])
    return original_send(fd, sig)
reported = False
with patch("os.pidfd_open", open_pidfd), patch("signal.pidfd_send_signal", send_pidfd):
    try:
        run_owned(sys.argv[4:], 2, log)
    except OSError as caught:
        assert caught is failure, "original pidfd failure was replaced"
        reported = True
    else:
        raise AssertionError("unclassified pidfd error became success")
records = [json.loads(line) for line in log.read_text().splitlines()]
result = records[-1]
result.update(reported=reported, injected=injected, signalled=signalled)
(root / "verification.json").write_text(json.dumps(result))
sys.exit(1)  # The error remains a failing outcome even when every child is reaped.
''', str(RUNNER.parent), str(root), str(log), sys.executable, "-c", FIXTURE, str(root), "normal"]
            decoy = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(20)"])
            try:
                with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as runner:
                    try:
                        deadline = time.monotonic() + 3
                        while not (root / "ready").exists() and time.monotonic() < deadline:
                            time.sleep(.01)
                        self.assertTrue((root / "ready").exists())
                        if mode == "cancel":
                            runner.send_signal(signal.SIGTERM)
                        out, err = runner.communicate(timeout=8)
                    finally:
                        if runner.poll() is None:
                            runner.kill()
                            runner.wait(timeout=3)
                if mode.startswith("diagnostic-") or mode == "pidfd-error":
                    result = json.loads((root / "verification.json").read_text())
                    if mode.startswith("diagnostic-"):
                        self.assertGreaterEqual(len(result["injected"]), 2 if event == "cleanup-begin" else 1)
                    else:
                        self.assertTrue(result["reported"])
                        self.assertTrue(result["injected"])
                        self.assertTrue(result["cleanup"], "pidfd error aborted the remaining owned-tree cleanup")
                        self.assertEqual([{"target_pid": int((root / "ready").read_text()), "errno": errno.EINVAL}],
                                         result["cleanup_errors"])
                        for name in ("child", "grandchild"):
                            self.assertIn(int((root / name).read_text()), result["signalled"])
                else:
                    records = [json.loads(line) for line in log.read_text().splitlines()]
                    result = records[-1]
                stat_success = mode in ("stat-missing", "stat-gone")
                if mode.startswith("stat-"):
                    injected = json.loads((root / "injected.json").read_text())
                    self.assertTrue(injected, "stat failure was not exercised")
                    self.assertEqual({}, result["process_state"])
                    self.assertEqual(stat_success, any(record["event"] == "cleanup-begin" for record in records))
                    if not stat_success:
                        self.assertIn("injected", err)
                self.assertTrue(result["cleanup"], (out, err))
                self.assertEqual([], result["remaining"])
                expected_phase = ("exited" if mode.startswith("stat-") or mode == "pidfd-error" else
                                  "diagnostic-failure" if mode.startswith("diagnostic-") else
                                  {"normal": "exited", "timeout": "timeout", "cancel": "cancelled",
                                   "exception": "exception"}[mode])
                self.assertEqual(expected_phase, result["phase"])
                for name in ("child", "grandchild"):
                    pid = int((root / name).read_text())
                    self.assertIn(str(pid), result["reaped"])
                    self.assertFalse(Path(f"/proc/{pid}").exists(), f"{name} still exists, including zombie")
                self.assertIsNone(decoy.poll(), "unrelated sibling was killed")
                self.assertEqual(mode == "normal" or stat_success, runner.returncode == 0)
            finally:
                decoy.terminate()
                decoy.wait(timeout=3)
                deadline = time.monotonic() + 3
                while True:
                    signal_tree(signal.SIGKILL)
                    complete, _ = reap()
                    if complete:
                        break
                    if time.monotonic() >= deadline:
                        self.fail("fixture emergency teardown could not reap all descendants")
                    time.sleep(.02)

    def test_normal_exit_reaps_leftover_descendants(self): self.scenario("normal")
    def test_timeout_forces_and_reaps_full_tree(self): self.scenario("timeout")
    def test_cancellation_forces_and_reaps_full_tree(self): self.scenario("cancel")
    def test_exception_forces_and_reaps_full_tree(self): self.scenario("exception")
    def test_full_log_disk_at_cleanup_start(self): self.scenario("diagnostic-log:cleanup-begin")
    def test_full_log_disk_at_cleanup_end(self): self.scenario("diagnostic-log:end")
    def test_broken_stdout_at_cleanup_start(self): self.scenario("diagnostic-stdout:cleanup-begin")
    def test_broken_stdout_at_cleanup_end(self): self.scenario("diagnostic-stdout:end")
    def test_missing_stat_still_reaps_full_tree(self): self.scenario("stat-missing")
    def test_exited_process_stat_still_reaps_full_tree(self): self.scenario("stat-gone")
    def test_stat_permission_failure_propagates_after_cleanup(self): self.scenario("stat-denied")
    def test_stat_io_failure_propagates_after_cleanup(self): self.scenario("stat-io")
    def test_pidfd_error_still_reaps_other_anchored_descendants(self): self.scenario("pidfd-error")


class PidfdDiagnosticTests(unittest.TestCase):
    def observe_failure(self, *, reads_fail=False, ancestry_fails=False, output_fails=False):
        target = os.getpid()
        failure = OSError(errno.EINVAL, "injected pidfd failure")
        output = io.StringIO()
        ownership = [{target}, PermissionError(errno.EACCES, "injected ancestry failure")] if ancestry_fails else None
        reads = patch.object(Path, "read_text", side_effect=PermissionError(errno.EACCES, "injected identity failure")) if reads_fail else nullcontext()
        writes = patch("builtins.print", side_effect=BrokenPipeError(errno.EPIPE, "injected output failure")) if output_fails else nullcontext()
        with patch("OwnedProcessRunner.descendants", side_effect=ownership, return_value={target}), \
                patch("os.pidfd_open", side_effect=failure) as opened, \
                patch("signal.pidfd_send_signal") as sent, redirect_stderr(output):
            with reads, writes, self.assertRaises(OSError) as caught:
                signal_tree(signal.SIGTERM)
            self.assertIs(caught.exception, failure)
            opened.assert_called_once_with(target, 0)
            sent.assert_not_called()
        return output.getvalue(), target

    def test_pidfd_failure_format_and_original_exception(self):
        output, target = self.observe_failure()
        record = json.loads(output)
        self.assertEqual("pidfd-open-failure", record["event"])
        self.assertEqual((target, 0, errno.EINVAL), (record["target_pid"], record["flags"], record["errno"]))
        self.assertTrue(record["owned_at_census"] and record["owned_at_recheck"])
        self.assertFalse(record["anchored"])
        self.assertEqual(target, record["identity"]["status"]["Tgid"])
        self.assertGreater(record["identity"]["stat"]["start_ticks"], 0)
        self.assertNotIn("Name", record["identity"]["status"])

    def test_unavailable_identity_does_not_replace_original_error(self):
        output, _ = self.observe_failure(reads_fail=True)
        record = json.loads(output)
        self.assertEqual({}, record["identity"])
        self.assertEqual({"status", "stat"}, {item["source"] for item in record["observation_errors"]})

    def test_unavailable_ancestry_is_not_reported_as_verified(self):
        output, _ = self.observe_failure(ancestry_fails=True)
        self.assertIsNone(json.loads(output)["owned_at_recheck"])

    def test_broken_diagnostic_output_preserves_original_error(self):
        output, _ = self.observe_failure(output_fails=True)
        self.assertEqual("", output)

    def test_already_gone_target_keeps_existing_nonfailure_path(self):
        with patch("OwnedProcessRunner.descendants", return_value={os.getpid()}), \
                patch("os.pidfd_open", side_effect=ProcessLookupError(errno.ESRCH, "gone")), \
                patch("signal.pidfd_send_signal") as sent, redirect_stderr(io.StringIO()) as output:
            signal_tree(signal.SIGTERM)
            self.assertEqual("", output.getvalue())
            sent.assert_not_called()


if __name__ == "__main__":
    unittest.main()
