#!/usr/bin/env python3
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

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
            decoy = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(20)"])
            try:
                with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as runner:
                    deadline = time.monotonic() + 3
                    while not (root / "ready").exists() and time.monotonic() < deadline:
                        time.sleep(.01)
                    self.assertTrue((root / "ready").exists())
                    if mode == "cancel":
                        runner.send_signal(signal.SIGTERM)
                    out, err = runner.communicate(timeout=8)
                records = [json.loads(line) for line in log.read_text().splitlines()]
                result = records[-1]
                self.assertTrue(result["cleanup"], (out, err))
                self.assertEqual([], result["remaining"])
                self.assertEqual({"normal": "exited", "timeout": "timeout", "cancel": "cancelled",
                                  "exception": "exception"}[mode], result["phase"])
                for name in ("child", "grandchild"):
                    pid = int((root / name).read_text())
                    self.assertIn(str(pid), result["reaped"])
                    self.assertFalse(Path(f"/proc/{pid}").exists(), f"{name} still exists, including zombie")
                self.assertIsNone(decoy.poll(), "unrelated sibling was killed")
                self.assertEqual(mode == "normal", runner.returncode == 0)
            finally:
                decoy.terminate()
                decoy.wait(timeout=3)

    def test_normal_exit_reaps_leftover_descendants(self): self.scenario("normal")
    def test_timeout_forces_and_reaps_full_tree(self): self.scenario("timeout")
    def test_cancellation_forces_and_reaps_full_tree(self): self.scenario("cancel")
    def test_exception_forces_and_reaps_full_tree(self): self.scenario("exception")


if __name__ == "__main__":
    unittest.main()
