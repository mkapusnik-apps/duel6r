#!/usr/bin/env python3
"""Production shared-entry regression, without display/audio or UI resources."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

application = str(Path(sys.argv[1]).resolve())
assert len(sys.argv) == 3 and sys.argv[2] in ("0", "1"), "compiled service capability required"
dedicated_service = sys.argv[2] == "1"
env = dict(os.environ, SDL_VIDEODRIVER="unavailable-test-driver",
           SDL_AUDIODRIVER="unavailable-test-driver", D6R_DIRECTORY_URL="")
env.pop("DISPLAY", None)
env.pop("WAYLAND_DISPLAY", None)
count = 0
with tempfile.TemporaryDirectory(prefix="duel6r shared entry ") as temporary:
    root = Path(temporary)
    def check(args, status, marker=None):
        global count
        result = subprocess.run([application, *args], cwd=root, env=env,
                                text=True, capture_output=True, timeout=5)
        assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
        if marker:
            assert marker in result.stdout, (args, result.stdout)
        if status == 1:
            assert result.stderr, args
        assert "initialization" not in result.stdout.lower()
        assert "application information" not in result.stdout.lower()
        assert "sentinel-secret" not in result.stdout + result.stderr
        assert len(result.stdout) + len(result.stderr) < 8192, "unbounded CLI output"
        assert not (root / "data").exists(), "headless dispatch wrote client data"
        assert not (root / "ready.sock").exists(), "CLI validation created readiness socket"
        count += 1

    check(["--help"], 0, "--server")
    check(["--server", "--help"], 0, "--dedicated")
    check(["--help", "--server"], 0, "--check-ready")
    check(["--server"], 2, "did not listen")
    check(["--port=26663", "--server"], 2, "26663")
    check(["--server", "--port=26664"], 2, "26664")
    for args in (["--transport"], ["--dedicated"], ["--unknown"],
                 ["--server=sentinel-secret"], ["--server", "--server"],
                 ["--server", "--unknown=sentinel-secret"],
                 ["--server", "--authoritative-match", "--unknown=sentinel-secret"],
                 ["--server", "--port=sentinel-secret"],
                 ["--server", "--transport", "--admission-client"],
                 ["--server", "--dedicated"],
                 ["--server", "--help", "--unknown=sentinel-secret"]):
        check(args, 1)
    # Help must not read a password path. A FIFO would block if read; even
    # checking its protected-file suitability would reject this valid help.
    password = root / "password"
    if hasattr(os, "mkfifo"):
        os.mkfifo(password, 0o600)
    else:
        password.write_text("sentinel-secret")
    dedicated = ["--server", "--dedicated", "--transport", "--host=127.0.0.1",
                 f"--readiness-socket={root / 'ready.sock'}",
                 f"--password-file={password}"]
    if dedicated_service:
        check([*dedicated, "--help"], 0, "--password-file")
        check(dedicated, 1)  # FIFO/missing private permissions fail before readiness.
        assert not (root / "ready.sock").exists()
    else:
        check([*dedicated, "--help"], 1)
        check(dedicated, 1)  # Dedicated host support was not expanded.
    probe = subprocess.run([application, "--server", f"--check-ready={root / 'absent.sock'}"],
                           cwd=root, env=env, capture_output=True, timeout=5)
    assert probe.returncode == 1 and not probe.stdout
    count += 1
print(f"Shared application entry: {count} cases PASS (no display/audio/UI resources)")
