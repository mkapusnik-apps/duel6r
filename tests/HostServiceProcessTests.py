#!/usr/bin/env python3
"""Bounded black-box lifecycle checks for the host supervisor and its owned child."""

import os
import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest


SUPERVISOR = pathlib.Path(sys.argv[1]).resolve()
CHILD = pathlib.Path(sys.argv[2]).resolve()
SERVER = pathlib.Path(sys.argv[3]).resolve()
RESOURCES = pathlib.Path(sys.argv[4]).resolve()
EXTRA_ARGUMENTS = sys.argv[5:]
ORPHAN_STRESS = "--orphan-stress" in EXTRA_ARGUMENTS
EXTRA_ARGUMENTS = [argument for argument in EXTRA_ARGUMENTS if argument != "--orphan-stress"]
sys.argv = [sys.argv[0]]


class HostServiceProcesses(unittest.TestCase):
    maxDiff = None

    @staticmethod
    def linux_process_metadata(pid):
        stat = pathlib.Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        fields = stat[stat.rfind(")") + 2:].split()
        return fields[0], fields[19]  # state and kernel start-time identity

    def assert_linux_process_terminated(self, pid, expected_start_time, deadline, message):
        while True:
            try:
                state, start_time = self.linux_process_metadata(pid)
            except (FileNotFoundError, ProcessLookupError):
                return
            self.assertEqual(expected_start_time, start_time,
                             f"process identity changed while checking {pid}")
            if state == "Z":
                return
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return
            if time.monotonic() >= deadline:
                self.fail(message)
            time.sleep(0.01)

    def kill_linux_process_if_matching(self, pid, expected_start_time):
        try:
            _, start_time = self.linux_process_metadata(pid)
        except (FileNotFoundError, ProcessLookupError):
            return
        self.assertEqual(expected_start_time, start_time,
                         f"process identity changed before cleanup of {pid}")
        try:
            os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            return

    def run_case(self, mode, *extra, timeout=5):
        command = [str(SUPERVISOR), f"--server={CHILD}", f"--resources={mode}", *extra]
        return subprocess.run(command, cwd=str(CHILD.parent), text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, timeout=timeout, check=False,
                              env={"PATH": "/test/path-that-must-not-be-used"})

    @staticmethod
    def close_process_streams(process):
        if process.poll() is None:
            process.kill()
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate(timeout=5)

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
                         "scaffold only: no graphical network UI or playable network session is implemented.\n"
                         "intentional-host-end\n",
                         result.stdout)
        self.assertNotIn("NET-09", result.stdout + result.stderr)
        self.assertNotIn("guest", result.stdout + result.stderr)
        self.assertEqual("", result.stderr)

    @staticmethod
    def available_port():
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]

    def assert_listener_released(self, port):
        with socket.socket() as probe:
            if sys.platform == "win32":
                probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            else:
                probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            probe.bind(("127.0.0.1", port))
            probe.listen(1)

    def test_supervised_real_listener_has_os_environment_without_parent_secrets(self):
        port = self.available_port()
        # The supervisor must derive SystemRoot from the OS even when its own
        # environment is missing it or supplies a bogus override. Never log it.
        for system_root in (None, "Z:\\not-the-system-directory"):
            with self.subTest(system_root_present=system_root is not None):
                environment = {"PATH": "/test/path-that-must-not-be-used",
                               "D6R_TEST_PARENT_SECRET": "must-not-reach-child"}
                if system_root is not None:
                    environment["SystemRoot"] = system_root
                result = subprocess.run(
                    [str(SUPERVISOR), f"--server={CHILD}", "--resources=listener-environment",
                     f"--port={port}", "--local-players=1", "--end-after-ready"],
                    cwd=str(CHILD.parent), text=True, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, timeout=12, check=False, env=environment)
                self.assertEqual(0, result.returncode, result)
                self.assertIn("host-service-active\n", result.stdout)
                self.assertEqual(1, result.stdout.count("intentional-host-end\n"))
                self.assertEqual("", result.stderr)
                self.assert_listener_released(port)

    def stage_gameplay_resources(self, root):
        # Windows CI bind-mounts C:\workspace. The manifest's pinned ancestor
        # contract deliberately rejects reparse/cross-volume traversal, so use
        # ordinary container-local files, like AdmissionProcessTests does.
        # Preserve the shipped bytes; do not replace them with a synthetic map
        # or bypass the production manifest builder.
        for directory in ("data", "levels"):
            source = RESOURCES / directory
            destination = root / directory
            shutil.copytree(source, destination)
            source_files = {path.relative_to(source) for path in source.rglob("*") if path.is_file()}
            copied_files = {path.relative_to(destination) for path in destination.rglob("*") if path.is_file()}
            self.assertTrue(source_files)
            self.assertEqual(source_files, copied_files)
            for relative in source_files:
                self.assertEqual((source / relative).read_bytes(), (destination / relative).read_bytes())

    def run_real_server(self, root, resource_argument, port):
        return subprocess.run(
            [str(SUPERVISOR), f"--server={SERVER}", f"--resources={resource_argument}",
             "--host=127.0.0.1", f"--port={port}", "--local-players=1", "--end-after-ready"],
            cwd=str(root), text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, timeout=14, check=False)

    def test_real_server_one_player_readiness_and_cleanup(self):
        with tempfile.TemporaryDirectory(prefix="duel6r hosted resources ") as directory:
            root = pathlib.Path(directory).resolve()
            self.stage_gameplay_resources(root)
            # Cover an absolute path with spaces and the graphical host's flat
            # bundle working-directory convention without changing content.
            for resources in (str(root), "."):
                with self.subTest(relative_root=resources == "."):
                    port = self.available_port()
                    result = self.run_real_server(root, resources, port)
                    self.assertEqual(0, result.returncode, result)
                    self.assertIn("host-service-active\n", result.stdout)
                    self.assertEqual(1, result.stdout.count("intentional-host-end\n"))
                    self.assertEqual("", result.stderr)
                    self.assert_listener_released(port)

    def test_real_server_missing_gameplay_content_still_fails_closed(self):
        with tempfile.TemporaryDirectory(prefix="duel6r hosted resources ") as directory:
            root = pathlib.Path(directory).resolve()
            self.stage_gameplay_resources(root)
            (root / "data" / "blocks.json").unlink()
            port = self.available_port()
            result = self.run_real_server(root, ".", port)
            self.assertEqual(2, result.returncode, result)
            self.assertEqual("host-gameplay-content-manifest-invalid\n"
                             "Hosted gameplay content is invalid. Restore the supported gameplay content "
                             "and restart the application.\n", result.stdout)
            self.assertEqual("", result.stderr)
            self.assert_listener_released(port)

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

    def test_exit_sealed_invalid_status_frames_fail_closed(self):
        for mode in ("raw-partial", "raw-truncated", "raw-oversized", "raw-malformed"):
            with self.subTest(mode=mode):
                result = self.run_case(mode)
                self.assertEqual(2, result.returncode, result)
                self.assertEqual(
                    "host-service-exited-before-ready\nHosted session stopped before it was ready.\n",
                    result.stdout)
                self.assertEqual("", result.stderr)
                self.assertNotIn("port", result.stdout.lower())
                self.assertNotIn("manifest", result.stdout.lower())

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
                self.close_process_streams(process)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux PDEATHSIG process observation")
    def test_parent_sigkill_does_not_orphan_owned_child(self):
        process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=timeout"],
                                   cwd=str(CHILD.parent), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, env={})
        try:
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
            _, child_start_time = self.linux_process_metadata(child_pid)
            os.kill(process.pid, signal.SIGKILL)
            process.communicate(timeout=3)
            self.assert_linux_process_terminated(
                child_pid, child_start_time, time.monotonic() + 3,
                f"owned child {child_pid} remained active after parent SIGKILL")
        finally:
            self.close_process_streams(process)

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
            _, descendant_start_time = self.linux_process_metadata(descendant)
            try:
                os.kill(process.pid, signal.SIGKILL)
                process.communicate(timeout=3)
                self.assert_linux_process_terminated(
                    descendant, descendant_start_time, time.monotonic() + 3,
                    f"owned descendant {descendant} remained active after parent SIGKILL")
            finally:
                self.kill_linux_process_if_matching(descendant, descendant_start_time)
                self.close_process_streams(process)

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
            _, descendant_start_time = self.linux_process_metadata(descendant)
            try:
                process.send_signal(signal.SIGTERM)
                process.communicate(timeout=5)
                self.assert_linux_process_terminated(
                    descendant, descendant_start_time, time.monotonic() + 3,
                    f"owned descendant {descendant} remained active after normal shutdown")
            finally:
                self.kill_linux_process_if_matching(descendant, descendant_start_time)
                self.close_process_streams(process)

    @unittest.skipUnless(sys.platform == "win32", "native Windows Job Object observation")
    def test_windows_normal_shutdown_waits_for_job_zero_active_processes(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-win-job-clean-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            try:
                deadline = time.monotonic() + 5
                while not marker.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(marker.exists(), "Job-owned descendant was not created")
                descendant = int(marker.read_text(encoding="ascii").strip())
                process.terminate()
                stdout, stderr = process.communicate(timeout=6)
                self.assertEqual(("", ""), (stdout, stderr))
                deadline = time.monotonic() + 3
                while self.windows_process_active(descendant) and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertFalse(self.windows_process_active(descendant),
                                 "supervisor completed before the Job reached zero active processes")
            finally:
                self.close_process_streams(process)

    @unittest.skipUnless(sys.platform == "win32", "native Windows Job Object observation")
    def test_windows_forced_parent_termination_kills_job_descendant_on_handle_close(self):
        with tempfile.TemporaryDirectory(prefix="duel6r-win-job-crash-") as directory:
            marker = pathlib.Path(directory) / "descendant.pid"
            process = subprocess.Popen([str(SUPERVISOR), f"--server={CHILD}", "--resources=tree",
                                        f"--gameplay-script={marker}"], cwd=str(CHILD.parent),
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env={})
            try:
                deadline = time.monotonic() + 5
                while not marker.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(marker.exists(), "Job-owned descendant was not created")
                descendant = int(marker.read_text(encoding="ascii").strip())
                process.kill()
                stdout, stderr = process.communicate(timeout=5)
                self.assertEqual(("", ""), (stdout, stderr))
                deadline = time.monotonic() + 3
                while self.windows_process_active(descendant) and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertFalse(self.windows_process_active(descendant),
                                 "kill-on-close left a Job-owned descendant active")
            finally:
                self.close_process_streams(process)


if __name__ == "__main__":
    if ORPHAN_STRESS:
        names = [
            "test_parent_sigkill_does_not_orphan_owned_child",
            "test_parent_sigkill_does_not_orphan_owned_descendant",
            "test_normal_shutdown_cleans_owned_descendant_process_tree",
        ]
        suite = unittest.TestSuite(HostServiceProcesses(name) for name in names)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        raise SystemExit(0 if result.wasSuccessful() else 1)
    unittest.main(verbosity=2)
