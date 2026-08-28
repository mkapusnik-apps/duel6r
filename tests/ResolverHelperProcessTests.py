#!/usr/bin/env python3
"""Resolver helper protocol and real helper-process cleanup regressions."""
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import time

REQUEST_MAGIC = 0x44365251
RESPONSE_MAGIC = 0x44365253
HEADER_BYTES = 12


def run_helper(helper, request):
    return subprocess.run([helper], input=request, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=3, check=False)


def run_helper_arguments(helper, arguments):
    return subprocess.run([helper, *arguments], stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=3, check=False)


def protocol_behavior(helper):
    host = b"localhost"
    request = struct.pack("!III", REQUEST_MAGIC, len(host), 0) + host
    completed = run_helper(helper, request)
    assert completed.returncode == 0
    assert len(completed.stdout) >= HEADER_BYTES
    magic, status, count = struct.unpack("!III", completed.stdout[:HEADER_BYTES])
    assert magic == RESPONSE_MAGIC and status == 0 and 0 < count <= 64
    assert len(completed.stdout) == HEADER_BYTES + count * 4
    assert b"\x7f\x00\x00\x01" in [completed.stdout[HEADER_BYTES + i * 4:HEADER_BYTES + (i + 1) * 4]
                                      for i in range(count)]

    malformed = [
        b"",
        b"short",
        struct.pack("!III", 0, 1, 0) + b"x",
        struct.pack("!III", REQUEST_MAGIC, 0, 0),
        struct.pack("!III", REQUEST_MAGIC, 4097, 0),
        struct.pack("!III", REQUEST_MAGIC, 10, 0) + b"short",
    ]
    for request in malformed:
        completed = run_helper(helper, request)
        assert completed.returncode == 2
        assert completed.stdout == b""

    direct = run_helper_arguments(helper, ["localhost", "26660"])
    assert direct.returncode == 0
    magic, status, count = struct.unpack("!III", direct.stdout[:HEADER_BYTES])
    assert magic == RESPONSE_MAGIC and status == 0 and count > 0
    for arguments in [
        [],
        ["localhost"],
        ["localhost", "26660", "extra"],
        ["", "26660"],
        ["localhost;touch-bad", "26660"],
        ["localhost/path", "26660"],
        ["a" * 4097, "26660"],
        ["localhost", ""],
        ["localhost", "0"],
        ["localhost", "65536"],
        ["localhost", "+1"],
        ["localhost", "abc"],
    ]:
        completed = run_helper_arguments(helper, arguments)
        assert completed.returncode == 2
        assert completed.stdout == b""


def cleanup_and_descriptor_behavior(test_binary):
    with tempfile.TemporaryDirectory(prefix="duel6r-resolver-test-") as directory:
        root = pathlib.Path(directory)
        copied_test = root / "duel6r-session-transport-tests"
        fake_helper = root / "duel6r-resolver"
        pid_file = root / "resolver-pids"
        shutil.copy2(test_binary, copied_test)
        fake_helper.write_text("#!/bin/sh\nprintf '%s\\n' \"$$\" >> \"$D6R_FAKE_RESOLVER_PID_FILE\"\nexec sleep 30\n",
                               encoding="utf-8")
        fake_helper.chmod(0o755)
        environment = os.environ.copy()
        environment["D6R_RUN_FAKE_RESOLVER_TEST"] = "1"
        environment["D6R_FAKE_RESOLVER_PID_FILE"] = str(pid_file)
        completed = subprocess.run([copied_test], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, env=environment, timeout=35, check=False)
        assert completed.returncode == 0, completed.stdout
        assert "[PASS] real resolver helper cleanup and descriptor EOF" in completed.stdout
        assert len(pid_file.read_text(encoding="utf-8").splitlines()) == 58


def malformed_response_behavior(test_binary):
    cases = {
        "malformed": b"bad",
        "truncated": struct.pack("!III", RESPONSE_MAGIC, 0, 1),
        "extra": struct.pack("!III", RESPONSE_MAGIC, 0, 1) + b"\x7f\x00\x00\x01extra",
        "too-many": struct.pack("!III", RESPONSE_MAGIC, 0, 65),
        "error-with-address": struct.pack("!III", RESPONSE_MAGIC, 1, 1) + b"\x7f\x00\x00\x01",
    }
    for name, response in cases.items():
        with tempfile.TemporaryDirectory(prefix=f"duel6r-resolver-{name}-") as directory:
            root = pathlib.Path(directory)
            copied_test = root / "duel6r-session-transport-tests"
            fake_helper = root / "duel6r-resolver"
            shutil.copy2(test_binary, copied_test)
            encoded = response.hex()
            fake_helper.write_text(
                "#!/usr/bin/env python3\nimport sys\nsys.stdout.buffer.write(bytes.fromhex(" + repr(encoded) + "))\n",
                encoding="utf-8")
            fake_helper.chmod(0o755)
            environment = os.environ.copy()
            environment["D6R_RUN_MALFORMED_RESOLVER_TEST"] = "1"
            completed = subprocess.run([copied_test], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, env=environment, timeout=5, check=False)
            assert completed.returncode == 0, f"{name}: {completed.stdout}"
            assert "[PASS] malformed real resolver response rejected" in completed.stdout


def parent_exit_terminates_helper(helper):
    with tempfile.TemporaryDirectory(prefix="duel6r-resolver-parent-exit-") as directory:
        root = pathlib.Path(directory)
        helper_pid_file = root / "helper-pid"
        sleeper_pid_file = root / "sleeper-pid"
        script = """
import os, subprocess, sys
helper = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
sleeper = subprocess.Popen(["sleep", "30"], pass_fds=(helper.stdin.fileno(),))
open(sys.argv[2], "w", encoding="utf-8").write(str(helper.pid))
open(sys.argv[3], "w", encoding="utf-8").write(str(sleeper.pid))
import time; time.sleep(.2)
os._exit(0)
"""
        parent = subprocess.Popen([sys.executable, "-c", script, helper,
                                   str(helper_pid_file), str(sleeper_pid_file)])
        parent.wait(timeout=3)
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline and not helper_pid_file.exists():
            time.sleep(.01)
        helper_pid = int(helper_pid_file.read_text(encoding="utf-8"))
        sleeper_pid = int(sleeper_pid_file.read_text(encoding="utf-8"))
        try:
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                status = pathlib.Path(f"/proc/{helper_pid}/stat")
                if not status.exists() or status.read_text(encoding="utf-8").split()[2] == "Z":
                    break
                time.sleep(.02)
            else:
                raise AssertionError("resolver helper survived its spawning parent")
            os.kill(sleeper_pid, 0)
        finally:
            try:
                os.kill(sleeper_pid, 9)
            except ProcessLookupError:
                pass


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: ResolverHelperProcessTests.py TEST_BINARY RESOLVER_BINARY")
    protocol_behavior(sys.argv[2])
    cleanup_and_descriptor_behavior(sys.argv[1])
    malformed_response_behavior(sys.argv[1])
    parent_exit_terminates_helper(sys.argv[2])
    print("resolver helper protocol, cleanup stress, and descriptor isolation passed")
