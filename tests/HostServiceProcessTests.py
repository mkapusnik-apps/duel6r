#!/usr/bin/env python3
"""Bounded black-box lifecycle checks for the host supervisor and its owned child."""

import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time
import unittest


SUPERVISOR = pathlib.Path(sys.argv[1]).resolve()
CHILD = pathlib.Path(sys.argv[2]).resolve()
EXTRA_ARGUMENTS = sys.argv[3:]
sys.argv = [sys.argv[0]]


class HostServiceProcesses(unittest.TestCase):
    maxDiff = None

    def run_case(self, mode, *extra, timeout=5):
        command = [str(SUPERVISOR), f"--server={CHILD}", f"--resources={mode}", *extra]
        return subprocess.run(command, cwd=str(CHILD.parent), text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=timeout, check=False,
                              env={"PATH": "/test/path-that-must-not-be-used"})

    @staticmethod
    def windows_process_active(pid):
        import ctypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = kernel32.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE
        if not handle:
            return False
        try:
            return kernel32.WaitForSingleObject(handle, 0) == 0x102  # WAIT_TIMEOUT
        finally:
            kernel32.CloseHandle(handle)

    def test_ready_then_confirmed_end_is_clean_and_truthful(self):
        result = self.run_case("ready", "--end-after-ready")
        self.assertEqual(0, result.returncode, result)
        self.assertEqual("host-service-active\n"
                         "scaffold only: no graphical network UI or playable network session is implemented.\n",
                         result.stdout)
        self.assertEqual("", result.stderr)

    def test_cancel_before_readiness_is_not_reported_as_failure(self):
        result = self.run_case("timeout", "--cancel-immediately")
        self.assertEqual((0, "", ""), (result.returncode, result.stdout, result.stderr))

    def test_specific_startup_process_outcomes_are_exact_and_redacted(self):
        cases = {
            "port-unavailable": ("host-service-port-unavailable",
                                 "The selected port is unavailable. Choose another port and try again."),
            "start-failed": ("host-service-start-failed", "Hosted session could not start."),
            "early": ("host-service-exited-before-ready", "Hosted session stopped before it was ready."),
        }
        forbidden = [str(CHILD), "--server", "--resources", "PATH", "peer", "credential", "hash", "reconnect"]
        for mode, expected in cases.items():
            with self.subTest(mode=mode):
                result = self.run_case(mode)
                self.assertEqual(2, result.returncode, result)
                self.assertEqual(expected[0] + "\n" + expected[1] + "\n", result.stdout)
                self.assertEqual("", result.stderr)
                for value in forbidden:
                    self.assertNotIn(value, result.stdout + result.stderr)

    def test_missing_trusted_absolute_executable_is_generic_start_failure(self):
        missing = CHILD.parent / "definitely-missing-host-service-test-child"
        result = subprocess.run([str(SUPERVISOR), f"--server={missing}", "--resources=ready"],
                                cwd=str(CHILD.parent), text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=5, check=False, env={})
        self.assertEqual(2, result.returncode, result)
        self.assertEqual("host-service-start-failed\nHosted session could not start.\n", result.stdout)
        self.assertEqual("", result.stderr)

    def test_unexpected_post_ready_stop_is_not_end_notice_or_retry(self):
        result = self.run_case("unexpected-stop")
        self.assertEqual(2, result.returncode, result)
        self.assertEqual("host-service-active\n"
                         "scaffold only: no graphical network UI or playable network session is implemented.\n"
                         "host-service-stopped-unexpectedly\nHosted session stopped unexpectedly.\n",
                         result.stdout)
        self.assertNotIn("NET-09", result.stdout + result.stderr)
        self.assertNotIn("host-end", result.stdout + result.stderr)

    def test_normal_shutdown_after_ready_cleans_without_host_end_claim(self):
        result = self.run_case("ready", "--exit-after-ready")
        self.assertEqual(0, result.returncode, result)
        self.assertNotIn("host-end", result.stdout + result.stderr)
        self.assertNotIn("NET-09", result.stdout + result.stderr)

    def test_startup_timeout_uses_strict_fixed_outcome(self):
        result = self.run_case("timeout", timeout=14)
        self.assertEqual(2, result.returncode, result)
        self.assertEqual("host-service-startup-timed-out\nHosted session startup timed out.\n", result.stdout)
        self.assertEqual("", result.stderr)

    def test_direct_unowned_ipc_attempt_is_rejected(self):
        result = subprocess.run([str(CHILD), "--host-service-ipc",
                                 f"--host-service-parent={os.getpid()}", "--resources=ready"],
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=3, check=False, env={})
        self.assertEqual(70, result.returncode, result)
        self.assertEqual(("", ""), (result.stdout, result.stderr))

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux descriptor and environment contract")
    def test_child_gets_empty_environment_and_no_unrelated_file_descriptors(self):
        result = self.run_case("secure-ready", "--end-after-ready")
        self.assertEqual(0, result.returncode, result)
        self.assertIn("host-service-active\n", result.stdout)

    def test_strict_argv_bounds_fail_before_process_creation(self):
        scripts = [f"--gameplay-script=safe-{index}.lua" for index in range(17)]
        result = self.run_case("ready", *scripts)
        self.assertEqual(2, result.returncode, result)
        self.assertEqual("host-service-start-failed\nHosted session could not start.\n", result.stdout)

    def test_inherited_channel_rejects_actual_parent_identity_mismatch(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-host-parent-") as directory:
            marker = pathlib.Path(directory) / "parent-check.txt"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}",
                                        "--resources=parent-mismatch", f"--gameplay-script={marker}"],
                                       cwd=str(CHILD.parent), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       text=True, env={})
            try:
                deadline = time.monotonic() + 4
                while not marker.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(marker.exists(), "hosted child did not complete parent validation probe")
                self.assertEqual("rejected\n", marker.read_text(encoding="ascii"))
            finally:
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
                process.wait(timeout=5)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux PDEATHSIG process observation")
    def test_parent_sigkill_does_not_orphan_owned_child(self):
        process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=timeout"],
                                   cwd=str(CHILD.parent), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, env={})
        child_pid = None
        deadline = time.monotonic() + 3
        children_file = pathlib.Path(f"/proc/{process.pid}/task/{process.pid}/children")
        while time.monotonic() < deadline:
            try:
                values = children_file.read_text(encoding="ascii").split()
                if values:
                    child_pid = int(values[0])
                    break
            except (FileNotFoundError, ProcessLookupError):
                pass
            time.sleep(0.01)
        self.assertIsNotNone(child_pid, "supervisor did not create the owned child")
        os.kill(process.pid, signal.SIGKILL)
        process.wait(timeout=3)
        child_path = pathlib.Path(f"/proc/{child_pid}")
        deadline = time.monotonic() + 3
        while child_path.exists() and time.monotonic() < deadline:
            # A PDEATHSIG-killed child can briefly remain as an init-owned zombie.
            try:
                state = (child_path / "stat").read_text(encoding="ascii").split()[2]
                if state == "Z":
                    break
            except (FileNotFoundError, ProcessLookupError):
                break
            time.sleep(0.01)
        if child_path.exists():
            state = (child_path / "stat").read_text(encoding="ascii").split()[2]
            self.assertEqual("Z", state, f"owned child {child_pid} remained active after parent SIGKILL")

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux process-group descendant observation")
    def test_parent_sigkill_does_not_orphan_owned_descendant(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-host-tree-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            deadline = time.monotonic() + 3
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists(), "owned child did not create its descendant")
            descendant = int(marker.read_text(encoding="ascii").strip())
            try:
                os.kill(process.pid, signal.SIGKILL)
                process.wait(timeout=3)
                deadline = time.monotonic() + 3
                descendant_path = pathlib.Path(f"/proc/{descendant}")
                while descendant_path.exists() and time.monotonic() < deadline:
                    state = (descendant_path / "stat").read_text(encoding="ascii").split()[2]
                    if state == "Z":
                        break
                    time.sleep(0.01)
                if descendant_path.exists():
                    state = (descendant_path / "stat").read_text(encoding="ascii").split()[2]
                    self.assertEqual("Z", state,
                                     f"owned descendant {descendant} remained active after parent SIGKILL")
            finally:
                try:
                    os.kill(descendant, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=3)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux process-group descendant observation")
    def test_normal_shutdown_cleans_owned_descendant_process_tree(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-host-clean-tree-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            deadline = time.monotonic() + 3
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists(), "owned child did not create its descendant")
            descendant = int(marker.read_text(encoding="ascii").strip())
            try:
                process.send_signal(signal.SIGTERM)
                process.wait(timeout=5)
                descendant_path = pathlib.Path(f"/proc/{descendant}")
                deadline = time.monotonic() + 3
                while descendant_path.exists() and time.monotonic() < deadline:
                    state = (descendant_path / "stat").read_text(encoding="ascii").split()[2]
                    if state == "Z":
                        break
                    time.sleep(0.01)
                if descendant_path.exists():
                    state = (descendant_path / "stat").read_text(encoding="ascii").split()[2]
                    self.assertEqual("Z", state,
                                     f"owned descendant {descendant} remained active after normal shutdown")
            finally:
                try:
                    os.kill(descendant, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=3)

    @unittest.skipUnless(sys.platform == "win32", "native Windows Job Object observation")
    def test_windows_normal_shutdown_waits_for_job_zero_active_processes(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-win-job-clean-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists(), "Job-owned descendant was not created")
            descendant = int(marker.read_text(encoding="ascii").strip())
            process.terminate()
            process.wait(timeout=6)
            deadline = time.monotonic() + 3
            while self.windows_process_active(descendant) and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertFalse(self.windows_process_active(descendant),
                             "supervisor completed before the Job reached zero active processes")

    @unittest.skipUnless(sys.platform == "win32", "native Windows Job Object observation")
    def test_windows_forced_parent_termination_kills_job_descendant_on_handle_close(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-win-job-crash-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            deadline = time.monotonic() + 5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(marker.exists(), "Job-owned descendant was not created")
            descendant = int(marker.read_text(encoding="ascii").strip())
            process.kill()
            process.wait(timeout=5)
            deadline = time.monotonic() + 3
            while self.windows_process_active(descendant) and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertFalse(self.windows_process_active(descendant),
                             "kill-on-close left a Job-owned descendant active")


if __name__ == "__main__":
    unittest.main(verbosity=2)
