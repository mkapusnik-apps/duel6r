#!/usr/bin/env python3
"""One independently budgeted case in an isolated flat runtime root."""
import argparse
import os
from pathlib import Path
import shutil
import tempfile
from OwnedProcessRunner import run_owned


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("test_binary", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("server", type=Path)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--case", required=True)
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--diagnostics", type=Path, required=True)
    parser.add_argument("--model", action="store_true")
    args = parser.parse_args()
    diagnostics = args.diagnostics.resolve()
    diagnostics.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="duel6r-flat-behavior-"))
    cleanup = True
    try:
        shutil.copytree(args.bundle or args.resources, root, dirs_exist_ok=True)
        assert not (root / "resources").exists(), "Expected a flat application content root"
        for required in ("data", "levels", "profiles", "shaders", "sound", "textures"):
            assert (root / required).is_dir(), f"Missing application content: {required}"
        executable = root / args.test_binary.name
        shutil.copy2(args.test_binary, executable)
        if args.bundle is None:
            shutil.copy2(args.server, root / "duel6r")
            shutil.copy2(args.server.with_name("duel6r-resolver"), root / "duel6r-resolver")
        assert (root / "duel6r").is_file()
        environment = dict(os.environ, SDL_AUDIODRIVER="dummy", LIBGL_ALWAYS_SOFTWARE="1",
                           D6R_TEST_FILTER=args.case, D6R_TEST_EXACT="1")
        environment.pop("D6R_TEST_LIST", None)
        command = [str(executable)] if args.model else ["xvfb-run", "-a", str(executable)]
        print(f"BEGIN case={args.case!r} budget={args.timeout}s", flush=True)
        with (diagnostics / "case.stdout").open("w") as out, (diagnostics / "case.stderr").open("w") as err:
            cleanup = False
            result = run_owned(command, args.timeout, diagnostics / "process.log",
                               cwd=root, env=environment, stdout=out, stderr=err)
            cleanup = result["cleanup"]
        for name in ("case.stdout", "case.stderr"):
            print((diagnostics / name).read_text(errors="replace"), end="", flush=True)
        print(f"END case={args.case!r} result={result}", flush=True)
        return int(not cleanup or result["phase"] != "exited" or result["exit"] != 0)
    finally:
        if cleanup:
            shutil.rmtree(root)
        else:
            print(f"Unconfirmed cleanup: retaining runtime {root}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
