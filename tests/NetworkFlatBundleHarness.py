#!/usr/bin/env python3
"""Run graphical behavior tests beside the real server in a flat content root."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("test_binary", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("server", type=Path)
    parser.add_argument("--bundle", type=Path,
                        help="Use an existing runtime bundle rather than staging build outputs")
    args = parser.parse_args()
    # The harness owns this independent execution copy and cleanup on every exit.
    # Never add a nested resources directory to compensate for application paths.
    with tempfile.TemporaryDirectory(prefix="duel6r-flat-behavior-") as directory:
        root = Path(directory)
        shutil.copytree(args.bundle or args.resources, root, dirs_exist_ok=True)
        assert not (root / "resources").exists(), "Expected a flat application content root"
        for required in ("data", "levels", "profiles", "shaders", "sound", "textures"):
            assert (root / required).is_dir(), f"Missing application content: {required}"
        executable = root / args.test_binary.name
        shutil.copy2(args.test_binary, executable)
        if args.bundle is None:
            # CMake may expose a version-suffixed target file. The application
            # launches its sibling through the normal unversioned runtime name.
            shutil.copy2(args.server, root / "duel6r-server")
            shutil.copy2(args.server.with_name("duel6r-resolver"), root / "duel6r-resolver")
        assert (root / "duel6r-server").is_file()
        environment = dict(os.environ, SDL_AUDIODRIVER="dummy")
        listed = subprocess.run([str(executable)], cwd=root,
                                env=dict(environment, D6R_TEST_LIST="1"),
                                capture_output=True, text=True, timeout=10, check=True)
        names = listed.stdout.splitlines()
        assert names, "No graphical behavior tests matched the requested filter"
        failures = 0
        deadline = time.monotonic() + 160
        # The application has process-global control presets referencing Input.
        # Each normal application lifetime is one process; do not let successive
        # test Applications/virtual controllers retain a previous case's Input.
        for name in names:
            case_environment = dict(environment, D6R_TEST_FILTER=name, D6R_TEST_EXACT="1")
            case_environment.pop("D6R_TEST_LIST", None)
            result = subprocess.run(["xvfb-run", "-a", str(executable)], cwd=root, env=case_environment,
                                    timeout=max(1, deadline - time.monotonic()), check=False)
            failures += result.returncode != 0
        print(f"Executed {len(names)} isolated graphical behavior case(s), failures: {failures}", flush=True)
        return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
