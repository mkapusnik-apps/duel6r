#!/usr/bin/env python3
"""Native non-GUI checks against a byte-identical relocated application copy."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(command, cwd, environment, timeout=180):
    subprocess.run([str(value) for value in command], cwd=cwd, env=environment,
                   check=True, timeout=timeout)


def inventory(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file() and not path.is_symlink()}


def main(args):
    app, build, source = args.app.resolve(), args.build.resolve(), args.source.resolve()
    original = inventory(app)
    metadata = json.loads((app / "Contents/Resources/build-info.json").read_text())
    assert metadata["source_revision"] == args.revision and metadata["local_only"] is False
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("DYLD_") and key != "D6R_DIRECTORY_URL"}
    with tempfile.TemporaryDirectory(prefix="duel6r packaged checks ") as temporary:
        root = Path(temporary)
        relocated = root / "Application path with spaces/Duel 6 Reloaded.app"
        shutil.copytree(app, relocated, symlinks=True)
        assert inventory(relocated) == original
        cwd = root / "unrelated working directory"
        cwd.mkdir()
        macos = relocated / "Contents/MacOS"
        resources = relocated / "Contents/Resources"
        environment["D6R_TEST_BUNDLE"] = str(relocated)
        # The probe alone is test-only. Its sibling lookups and service config
        # use the actual unchanged packaged resolver, guardian and server.
        adapter = macos / "duel6r-packaged-adapter-tests"
        shutil.copy2(build / "duel6r-darwin-adapter-tests", adapter)
        run(["codesign", "--force", "--sign", "-", "--timestamp=none", adapter], cwd, environment)
        timeout_child = macos / "duel6r-packaged-timeout-child"
        shutil.copy2(build / "duel6r-host-service-test-child", timeout_child)
        run(["codesign", "--force", "--sign", "-", "--timestamp=none", timeout_child], cwd, environment)
        directory_probe = macos / "duel6r-packaged-directory-tests"
        shutil.copy2(build / "duel6r-directory-independence-tests", directory_probe)
        # Both directory and trust probes must load the relocated production
        # curl. Other non-system dependencies are already in the bundle closure.
        for line in subprocess.check_output(["otool", "-L", str(directory_probe)], text=True).splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            candidate = relocated / "Contents/Frameworks" / Path(dependency).name
            if candidate.is_file():
                run(["install_name_tool", "-change", dependency,
                     "@executable_path/../Frameworks/" + candidate.name, directory_probe], cwd, environment)
            elif not dependency.startswith(("/System/Library/", "/usr/lib/")):
                raise RuntimeError("Packaged directory probe dependency missing from app: " + dependency)
        run(["codesign", "--force", "--sign", "-", "--timestamp=none", directory_probe], cwd, environment)
        saves = []
        for instance in ("first", "second"):
            home = root / instance
            save = home / "Library/Application Support/Duel 6 Reloaded/data/persons.json"
            save.parent.mkdir(parents=True)
            save.write_text(instance + " isolated local-person sentinel")
            saves.append((save, save.read_bytes()))
            environment["HOME"] = str(home)
            run([adapter], cwd, environment)
            run([directory_probe], cwd, environment, 60)
        for save, expected in saves:
            assert save.read_bytes() == expected, "Network helper changed local person data"
        assert not (resources / "data/persons.json").exists()
        run([sys.executable, source / "tests/AuthoritativeMatchProcessTests.py", macos / "duel6r-server"],
            resources, environment)

        probe = macos / "duel6r-packaged-curl-tests"
        shutil.copy2(build / "duel6r-darwin-curl-trust-tests", probe)
        observer = macos / "libduel6r-darwin-trust-observer.dylib"
        shutil.copy2(build / observer.name, observer)
        # Relocate the test probe only; never alter the signed production files.
        commands = subprocess.check_output(["otool", "-L", str(probe)], text=True).splitlines()[1:]
        for line in commands:
            dependency = line.strip().split(" (", 1)[0]
            name = Path(dependency).name
            if name.startswith("libcurl"):
                replacement = "@executable_path/../Frameworks/" + name
            elif name == observer.name:
                replacement = "@executable_path/" + name
            else:
                continue
            run(["install_name_tool", "-change", dependency, replacement, probe], cwd, environment)
        for binary in (observer, probe):
            run(["codesign", "--force", "--sign", "-", "--timestamp=none", binary], cwd, environment)
        environment["D6R_EXPECT_CURL_LIBRARY"] = str(relocated / "Contents/Frameworks/libcurl.4.dylib")
        run([probe], cwd, environment, 15)  # Existing approved read-only curl.se trust smoke.
        run([sys.executable, source / "tests/DarwinCurlTrustFixtures.py", probe, args.openssl], cwd, environment)
        current = inventory(relocated)
        assert all(current[name] == digest for name, digest in original.items()), "Probe mutated packaged production files"
    assert inventory(app) == original
    report = {"source_revision": args.revision, "checks": "passed", "gui_executed": False,
              "archive_sha256": hashlib.sha256((app.parent / "duel6r-macos-arm64.zip").read_bytes()).hexdigest(),
              "relocated_space_path": True, "unrelated_cwd": True, "isolated_home_instances": 2,
              "packaged_file_sha256": original}
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    publication = args.report.parent / "build-info.json"
    metadata = json.loads(publication.read_text())
    metadata["native_packaged_checks"] = report
    publication.write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("app", "build", "source", "openssl", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--revision", required=True)
    main(parser.parse_args())
