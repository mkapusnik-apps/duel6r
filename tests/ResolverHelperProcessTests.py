#!/usr/bin/env python3
"""Resolver helper protocol and real helper-process cleanup regressions."""
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile

REQUEST_MAGIC = 0x44365251
RESPONSE_MAGIC = 0x44365253
HEADER_BYTES = 12


def run_helper(helper, request):
    return subprocess.run([helper], input=request, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
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
        assert len(pid_file.read_text(encoding="utf-8").splitlines()) == 25


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: ResolverHelperProcessTests.py TEST_BINARY RESOLVER_BINARY")
    protocol_behavior(sys.argv[2])
    cleanup_and_descriptor_behavior(sys.argv[1])
    print("resolver helper protocol, cleanup stress, and descriptor isolation passed")
