#!/usr/bin/env python3
import json
import ctypes
import errno
import io
import itertools
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from contextlib import nullcontext, redirect_stderr
from types import SimpleNamespace
from unittest.mock import patch
from OwnedProcessRunner import descendants, main, observed_disappearance, pidfd_failure_context, reap, run_owned, signal_tree

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

# Policy fixture, NOT a reproduction of a kernel EINVAL race. An owned parent
# acknowledges reaping its child before the injected pidfd_open failure returns.
DISAPPEARING_FIXTURE = r'''
import os, signal, sys, time
from pathlib import Path
root = Path(sys.argv[1])
child = os.fork()
if child == 0:
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    grandchild = os.fork()
    if grandchild == 0:
        os.setsid()
        read_fd, write_fd = os.pipe()
        departing = os.fork()
        if departing == 0:
            os.close(write_fd)
            os.read(read_fd, 1)
            os._exit(0)
        os.close(read_fd)
        (root / "grandchild").write_text(str(os.getpid()))
        (root / "departing").write_text(str(departing))
        while not (root / "release-departing").exists(): time.sleep(.005)
        os.write(write_fd, b"x")
        os.close(write_fd)
        assert os.waitpid(departing, 0) == (departing, 0)
        (root / "departing-reaped").write_text(str(departing))
        while True: signal.pause()
    (root / "child").write_text(str(os.getpid()))
    while True: signal.pause()
while not ((root / "departing").exists() and (root / "child").exists()): time.sleep(.005)
(root / "ready").write_text(str(os.getpid()))
'''


class DescendantDiscoveryTests(unittest.TestCase):
    def test_exited_task_children_read_preserves_other_owned_branches(self):
        for error in (FileNotFoundError(errno.ENOENT, "vanished task"),
                      ProcessLookupError(errno.ESRCH, "exited task")):
            with self.subTest(errno=error.errno):
                def tasks(path):
                    return [path / path.parent.name]
                def read_children(path):
                    pid = int(path.parent.name)
                    if pid == 300:
                        raise error
                    return {100: "200 300", 200: "400", 400: ""}[pid]
                with patch.object(Path, "iterdir", tasks), patch.object(Path, "read_text", read_children):
                    # Retain even the discovered vanished PID for the existing
                    # pidfd/reaping path; continue enumerating the live branch.
                    self.assertEqual({200, 300, 400}, descendants(100))

    def test_other_children_read_errors_are_not_treated_as_disappearance(self):
        for error in (PermissionError(errno.EACCES, "denied task children"),
                      PermissionError(errno.EPERM, "forbidden task children"),
                      OSError(errno.EIO, "task children I/O failure")):
            with self.subTest(errno=error.errno), \
                    patch.object(Path, "iterdir", return_value=[Path("/proc/100/task/100")]), \
                    patch.object(Path, "read_text", side_effect=error):
                with self.assertRaises(OSError) as caught:
                    descendants(100)
                self.assertIs(error, caught.exception)


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
            if mode.startswith("pidfd-disappear"):
                command = [sys.executable, "-c", r'''
import errno, json, os, signal, sys, time
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, sys.argv[1])
import OwnedProcessRunner as supervisor
root, log, mode = Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4]
original_open, original_send = os.pidfd_open, signal.pidfd_send_signal
original_read, original_reap = Path.read_text, supervisor.reap
failure = OSError(errno.EINVAL, "injected EINVAL after acknowledged real child reaping")
injected, signalled, anchored, final_identity = [], [], {}, []
terminal = False
def open_pidfd(pid, flags=0):
    target = int((root / "departing").read_text())
    if pid == target:
        assert not injected, "policy fixture unexpectedly retried the vanished target"
        (root / "release-departing").touch()
        deadline = time.monotonic() + 2
        while not (root / "departing-reaped").exists() and time.monotonic() < deadline: time.sleep(.005)
        assert (root / "departing-reaped").read_text() == str(target)
        for source in ("status", "stat"):
            try: original_read(Path(f"/proc/{target}/{source}"))
            except FileNotFoundError as error: assert error.errno == errno.ENOENT
            else: raise AssertionError("departing process still exists")
        injected.append(pid)
        raise failure
    fd = original_open(pid, flags)
    anchored[fd] = pid
    return fd
def send_pidfd(fd, sig):
    assert fd in anchored, "unanchored signal"
    signalled.append(anchored[fd])
    return original_send(fd, sig)
def reap():
    global terminal
    complete, statuses = original_reap()
    if complete: terminal = True
    return complete, statuses
def read_identity(path, *args, **kwargs):
    if not terminal or mode != "pidfd-disappear-reused":
        return original_read(path, *args, **kwargs)
    target = int(original_read(root / "departing"))
    if path.parent == Path(f"/proc/{target}"):
        # Deterministic replacement identity after ECHILD; no PID allocator manipulation.
        final_identity.append(path.name)
        if path.name == "status": return f"Pid:\t{target}\nTgid:\t{target}\nPPid:\t1\nState:\tS (replacement)\n"
        if path.name == "stat": return f"{target} (replacement) S 1 " + "0 " * 17 + "987654321"
    return original_read(path, *args, **kwargs)
def no_numeric_signal(*args, **kwargs):
    raise AssertionError("numeric PID/PGID signal fallback")
reported = False
with patch("os.pidfd_open", open_pidfd), patch("signal.pidfd_send_signal", send_pidfd), \
        patch.object(Path, "read_text", read_identity), patch.object(supervisor, "reap", reap), \
        patch("os.kill", no_numeric_signal), patch("os.killpg", no_numeric_signal):
    try:
        supervisor.run_owned(sys.argv[5:], 2, log)
    except OSError as caught:
        assert caught is failure, "original pidfd failure was replaced"
        reported = True
records = [json.loads(line) for line in log.read_text().splitlines()]
result = records[-1]
result.update(reported=reported, injected=injected, signalled=signalled, final_identity=final_identity)
(root / "verification.json").write_text(json.dumps(result))
sys.exit(1 if reported else 0)
''', str(RUNNER.parent), str(root), str(log), mode,
                           sys.executable, "-c", DISAPPEARING_FIXTURE, str(root)]
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
                if mode.startswith("diagnostic-") or mode.startswith("pidfd-"):
                    result = json.loads((root / "verification.json").read_text())
                    if mode.startswith("diagnostic-"):
                        self.assertGreaterEqual(len(result["injected"]), 2 if event == "cleanup-begin" else 1)
                    elif mode.startswith("pidfd-disappear"):
                        target = int((root / "departing").read_text())
                        rejected = mode == "pidfd-disappear-reused"
                        self.assertEqual([target], result["injected"])
                        self.assertNotIn(target, result["signalled"])
                        self.assertEqual(rejected, result["reported"])
                        self.assertEqual([{"target_pid": target, "errno": errno.EINVAL,
                            "classification": "unverified-disappearance" if rejected else "verified-disappearance"}],
                            result["pidfd_disappearances"])
                        if rejected:
                            self.assertEqual([{"target_pid": target, "errno": errno.EINVAL}], result["cleanup_errors"])
                            self.assertEqual(["status", "stat"], result["final_identity"])
                        else:
                            self.assertNotIn("cleanup_errors", result)
                            self.assertEqual([], result["final_identity"])
                        self.assertFalse(Path(f"/proc/{target}").exists())
                        contexts = [json.loads(line) for line in err.splitlines() if line.startswith("{")]
                        target_contexts = [item for item in contexts if item["target_pid"] == target]
                        self.assertEqual(["after-failed-syscall", "final-cleanup"],
                                         [item["observation"] for item in target_contexts])
                        self.assertEqual("provisional-disappearance", target_contexts[0]["classification"])
                        self.assertEqual("unclassified" if rejected else "provisional-disappearance",
                                         target_contexts[1]["classification"])
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
                expected_phase = ("exited" if mode.startswith("stat-") or mode.startswith("pidfd-") else
                                  "diagnostic-failure" if mode.startswith("diagnostic-") else
                                  {"normal": "exited", "timeout": "timeout", "cancel": "cancelled",
                                   "exception": "exception"}[mode])
                self.assertEqual(expected_phase, result["phase"])
                for name in ("child", "grandchild"):
                    pid = int((root / name).read_text())
                    self.assertIn(str(pid), result["reaped"])
                    self.assertFalse(Path(f"/proc/{pid}").exists(), f"{name} still exists, including zombie")
                self.assertIsNone(decoy.poll(), "unrelated sibling was killed")
                self.assertEqual(mode in ("normal", "pidfd-disappear") or stat_success, runner.returncode == 0)
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

    def test_real_disappearance_requires_verified_whole_tree_cleanup(self): self.scenario("pidfd-disappear")
    def test_disappearance_with_contradictory_final_identity_fails(self): self.scenario("pidfd-disappear-reused")


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


class DisappearancePolicyTests(unittest.TestCase):
    # Deterministic syscall/proc-observation policy tests, not kernel-race claims.
    target = 424242

    def absent_context(self):
        return {"target_pid": self.target, "flags": 0, "errno": errno.EINVAL,
                "owned_at_census": True, "owned_at_recheck": False, "identity": {},
                "observation_errors": [{"source": source, "errno": errno.ENOENT}
                                       for source in ("status", "stat")]}

    def observe(self, *, reads=None, ancestry=None, flags=0, pid=None, owned=True,
                error_number=errno.EINVAL, broken_output=False):
        if reads is None:
            reads = {source: FileNotFoundError(errno.ENOENT, "missing identity") for source in ("status", "stat")}
        def read_identity(path, *args, **kwargs):
            value = reads[path.name]
            if isinstance(value, BaseException):
                raise value
            return value
        ancestry_patch = (patch("OwnedProcessRunner.descendants", side_effect=ancestry)
                          if isinstance(ancestry, BaseException) else
                          patch("OwnedProcessRunner.descendants", return_value=set() if ancestry is None else ancestry))
        output_patch = (patch("builtins.print", side_effect=BrokenPipeError(errno.EPIPE, "closed diagnostic sink"))
                        if broken_output else nullcontext())
        with ancestry_patch, patch.object(Path, "read_text", read_identity), \
                redirect_stderr(io.StringIO()), output_patch:
            return pidfd_failure_context(self.target if pid is None else pid, flags,
                                         OSError(error_number, "injected pidfd failure"), owned)

    def identity(self, state="S"):
        return {"status": f"Pid:\t{self.target}\nTgid:\t{self.target}\nPPid:\t1\nState:\t{state} (policy target)\n",
                "stat": f"{self.target} (policy target) {state} 1 " + "0 " * 17 + "12345"}

    def test_joint_enoent_and_successfully_absent_ancestry_is_only_provisional(self):
        context = self.observe()
        self.assertTrue(observed_disappearance(context))
        self.assertEqual("provisional-disappearance", context["classification"])
        self.assertFalse(context["anchored"])

    def test_live_and_zombie_identity_never_qualify(self):
        for state in ("S", "Z"):
            with self.subTest(state=state):
                context = self.observe(reads=self.identity(state))
                self.assertFalse(observed_disappearance(context))
                self.assertEqual(state, context["identity"]["status"]["state"])
                failure = OSError(errno.EINVAL, "live or zombie target")
                errors, candidates = [], []
                with patch("OwnedProcessRunner.descendants", return_value={self.target}), \
                        patch("OwnedProcessRunner.pidfd_failure_context", return_value=context), \
                        patch("os.pidfd_open", side_effect=failure), patch("signal.pidfd_send_signal") as sent:
                    signal_tree(signal.SIGTERM, errors, candidates)
                self.assertEqual([(self.target, failure)], errors)
                self.assertEqual([], candidates)
                sent.assert_not_called()

    def test_partial_missing_unreadable_and_malformed_identity_fail_closed(self):
        for source in ("status", "stat"):
            for replacement in (self.identity()[source], "malformed", "",
                                PermissionError(errno.EACCES, "denied"), OSError(errno.EIO, "I/O failure")):
                with self.subTest(source=source, replacement=repr(replacement)):
                    reads = {name: FileNotFoundError(errno.ENOENT, "missing identity") for name in ("status", "stat")}
                    reads[source] = replacement
                    self.assertFalse(observed_disappearance(self.observe(reads=reads)))

    def test_owned_unreadable_or_uncertain_ancestry_fail_closed(self):
        for ancestry in ({self.target}, PermissionError(errno.EACCES, "denied ancestry"),
                         RuntimeError("unexpected collection failure")):
            with self.subTest(ancestry=repr(ancestry)):
                self.assertFalse(observed_disappearance(self.observe(ancestry=ancestry)))
        reads = {name: RuntimeError("unexpected identity failure") for name in ("status", "stat")}
        self.assertIsNone(self.observe(reads=reads))

    def test_invalid_pid_flags_census_and_other_errors_never_qualify(self):
        for options in ({"pid": 0}, {"pid": -1}, {"flags": 1}, {"owned": False},
                        {"error_number": errno.EPERM}, {"error_number": errno.EMFILE},
                        {"error_number": errno.EIO}):
            with self.subTest(options=options):
                self.assertFalse(observed_disappearance(self.observe(**options)))

    def test_output_failure_cannot_replace_or_manufacture_observations(self):
        self.assertTrue(observed_disappearance(self.observe(broken_output=True)))
        self.assertFalse(observed_disappearance(self.observe(reads=self.identity(), broken_output=True)))

    def test_standalone_disappearance_still_raises_original_error(self):
        failure = OSError(errno.EINVAL, "standalone cannot certify cleanup")
        for errors in (None, []):
            with self.subTest(collecting=errors is not None), \
                    patch("OwnedProcessRunner.descendants", return_value={self.target}), \
                    patch("OwnedProcessRunner.pidfd_failure_context", return_value=self.absent_context()), \
                    patch("os.pidfd_open", side_effect=failure), patch("signal.pidfd_send_signal") as sent:
                if errors is None:
                    with self.assertRaises(OSError) as caught:
                        signal_tree(signal.SIGTERM)
                    self.assertIs(failure, caught.exception)
                else:
                    signal_tree(signal.SIGTERM, errors)
                    self.assertEqual([(self.target, failure)], errors)
                sent.assert_not_called()

    def test_every_failure_is_classified_before_deduplication(self):
        live = dict(self.absent_context(), identity={"stat": {"start_ticks": 12345}})
        for observations in ((self.absent_context(), live), (live, self.absent_context())):
            with self.subTest(absent_first=not observations[0]["identity"]):
                failures = [OSError(errno.EINVAL, "first"), OSError(errno.EINVAL, "second")]
                errors, candidates = [], []
                with patch("OwnedProcessRunner.descendants", return_value={self.target}), \
                        patch("OwnedProcessRunner.pidfd_failure_context", side_effect=observations) as observed, \
                        patch("os.pidfd_open", side_effect=failures), patch("signal.pidfd_send_signal") as sent:
                    signal_tree(signal.SIGTERM, errors, candidates)
                    signal_tree(signal.SIGKILL, errors, candidates)
                self.assertEqual(2, observed.call_count)
                live_index = 1 if not observations[0]["identity"] else 0
                self.assertEqual([(self.target, failures[live_index])], errors)
                self.assertEqual([(self.target, failures[1 - live_index])], candidates)
                sent.assert_not_called()

    def test_replacement_identity_after_failed_open_is_fatal(self):
        context = self.observe(reads=self.identity(), ancestry=set())
        failure = OSError(errno.EINVAL, "PID now names an unrelated replacement")
        errors, candidates = [], []
        with patch("OwnedProcessRunner.descendants", return_value={self.target}), \
                patch("OwnedProcessRunner.pidfd_failure_context", return_value=context), \
                patch("os.pidfd_open", side_effect=failure), patch("signal.pidfd_send_signal") as sent, \
                patch("os.kill") as numeric_pid, patch("os.killpg") as numeric_group:
            signal_tree(signal.SIGTERM, errors, candidates)
        self.assertEqual([(self.target, failure)], errors)
        self.assertEqual([], candidates)
        sent.assert_not_called()
        numeric_pid.assert_not_called()
        numeric_group.assert_not_called()

    def test_successful_handle_is_closed_without_signalling_stale_decoy(self):
        with patch("OwnedProcessRunner.descendants", side_effect=[{self.target}, set()]), \
                patch("os.pidfd_open", return_value=91) as opened, \
                patch("signal.pidfd_send_signal") as sent, patch("os.close") as closed, \
                patch("os.kill") as numeric_pid, patch("os.killpg") as numeric_group:
            signal_tree(signal.SIGTERM)
        opened.assert_called_once_with(self.target, 0)
        sent.assert_not_called()
        closed.assert_called_once_with(91)
        numeric_pid.assert_not_called()
        numeric_group.assert_not_called()

    def terminal_policy(self, *, complete=True, final_census=None, final_context=...,
                        fatal_first=False, command_exit=0):
        failure = OSError(errno.EINVAL, "original provisional failure")
        leader = self.target - 1
        signals = []
        def collect(sig, errors, candidates):
            signals.append(sig)
            if not candidates:
                candidates.append((self.target, failure))
            if fatal_first and not errors:
                errors.append((self.target, failure))
        ticks = itertools.count()
        final_context = self.absent_context() if final_context is Ellipsis else final_context
        census = set() if final_census is None else final_census
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "terminal-policy.jsonl"
            with patch("OwnedProcessRunner.children", return_value=[]), \
                    patch("ctypes.CDLL", return_value=SimpleNamespace(prctl=lambda *args: 0)), \
                    patch("subprocess.Popen", return_value=SimpleNamespace(pid=leader, returncode=None)), \
                    patch("os.waitid", return_value=SimpleNamespace(si_status=command_exit, si_code=os.CLD_EXITED)), \
                    patch("OwnedProcessRunner.signal_tree", side_effect=collect), \
                    patch("OwnedProcessRunner.reap", return_value=(complete, {leader: command_exit} if complete else {})), \
                    patch("OwnedProcessRunner.descendants", side_effect=[set(), census]), \
                    patch("OwnedProcessRunner.pidfd_failure_context", return_value=final_context) as observed, \
                    patch("time.monotonic", side_effect=lambda: next(ticks)), patch("time.sleep"), \
                    patch("builtins.print"):
                caught = None
                try:
                    run_owned(["policy-test-no-real-command"], 60, log)
                except OSError as error:
                    caught = error
                observed.assert_called_once_with(self.target, 0, failure, True, observation="final-cleanup")
            result = json.loads(log.read_text().splitlines()[-1])
        return failure, caught, result, signals

    def test_no_echild_never_resolves_a_disappearance(self):
        original, caught, result, signals = self.terminal_policy(complete=False)
        self.assertIs(original, caught)
        self.assertFalse(result["cleanup"])
        self.assertEqual("unverified-disappearance", result["pidfd_disappearances"][0]["classification"])
        self.assertIn(signal.SIGTERM, signals)
        self.assertIn(signal.SIGKILL, signals)

    def test_nonempty_final_census_never_resolves_a_disappearance(self):
        original, caught, result, _ = self.terminal_policy(final_census={self.target + 1})
        self.assertIs(original, caught)
        self.assertFalse(result["cleanup"])
        self.assertEqual([self.target + 1], result["remaining"])

    def test_unreadable_final_census_preserves_original_failure(self):
        for error in (PermissionError(errno.EACCES, "final ancestry denied"), ValueError("malformed census")):
            with self.subTest(error=repr(error)):
                original, caught, result, _ = self.terminal_policy(final_census=error)
                self.assertIs(original, caught)
                self.assertFalse(result["cleanup"])
                self.assertIsNone(result["remaining"])
                self.assertEqual("final-ancestry", result["cleanup_observation_errors"][0]["source"])

    def test_uncertain_repeat_identity_cannot_resolve_a_disappearance(self):
        contexts = [None, self.observe(ancestry=PermissionError(errno.EACCES, "final ancestry denied"))]
        for source in ("status", "stat"):
            reads = {name: FileNotFoundError(errno.ENOENT, "gone") for name in ("status", "stat")}
            reads[source] = PermissionError(errno.EACCES, "final identity denied")
            contexts.append(self.observe(reads=reads))
        for index, context in enumerate(contexts):
            with self.subTest(observation=index):
                original, caught, result, _ = self.terminal_policy(final_context=context)
                self.assertIs(original, caught)
                self.assertEqual("unverified-disappearance", result["pidfd_disappearances"][0]["classification"])

    def test_late_absence_never_erases_an_earlier_fatal_observation(self):
        original, caught, result, _ = self.terminal_policy(fatal_first=True)
        self.assertIs(original, caught)
        self.assertTrue(result["cleanup"])
        self.assertEqual([{"target_pid": self.target, "errno": errno.EINVAL}], result["cleanup_errors"])

    def test_verified_disappearance_preserves_wrapped_command_failure(self):
        _, caught, result, _ = self.terminal_policy(command_exit=7)
        self.assertIsNone(caught)
        self.assertTrue(result["cleanup"])
        self.assertEqual(7, result["exit"])
        with patch("OwnedProcessRunner.run_owned", return_value=result), \
                patch.object(sys, "argv", [str(RUNNER), "--timeout", "2", "--log", "unused", "--", "fixture"]):
            self.assertEqual(7, main())


if __name__ == "__main__":
    unittest.main()
