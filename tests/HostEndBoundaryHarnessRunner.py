#!/usr/bin/env python3
"""Run the native test-only close boundary in isolated, disposable session data."""
import os
import pathlib
import subprocess
import sys
import tempfile

executable, scenario = sys.argv[1:]
environment = dict(os.environ, SDL_AUDIODRIVER="dummy", LIBGL_ALWAYS_SOFTWARE="1")
with tempfile.TemporaryDirectory(prefix="duel6r-host-end-boundary-") as directory:
    result = subprocess.run(
        ["xvfb-run", "-a", "-s", "-screen 0 1280x900x24 +extension GLX +render -noreset",
         executable, "--scenario", scenario, "--run-dir", str(pathlib.Path(directory) / "session")],
        env=environment, timeout=60,
    )
sys.exit(result.returncode)
