#!/usr/bin/env python3
"""Package the native local-only app; no launch or graphical QA is performed."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile


def run(*args):
    try:
        return subprocess.check_output([str(arg) for arg in args], text=True).strip()
    except subprocess.CalledProcessError as error:
        print(error.output)
        raise


def load_commands(binary):
    return run("otool", "-l", binary)


def is_notice(path):
    return bool(re.match(r"^(licen[cs]es?|copying|copyright|notice|authors)([.\-_ ]|$)",
                         path.name, re.IGNORECASE)) or any(
        part.lower() in {"licenses", "licences", "copying"} for part in path.parts[:-1])


def supplement_notices(name, keg, target, cache, packet):
    recipe = keg / ".brew" / (name + ".rb")
    recipe_hash = hashlib.sha256(recipe.read_bytes()).hexdigest()
    if keg.name != packet["version"] or recipe_hash not in packet["recipe_sha256"]:
        raise RuntimeError(
            f"Unreviewed source/recipe for {name}@{keg.name}; "
            f"expected version={packet['version']}; recipe={recipe}; "
            f"actual sha256={recipe_hash}; expected sha256={','.join(packet['recipe_sha256'])}")
    cache.mkdir(parents=True, exist_ok=True)
    (target / "sources").mkdir(parents=True, exist_ok=True)
    for source in packet["sources"]:
        filename = source["url"].rsplit("/", 1)[1]
        archive = cache / filename
        if not archive.exists():
            partial = archive.with_name(filename + ".part")
            run("curl", "--fail", "--location", "--retry", "3", "--proto", "=https",
                "--proto-redir", "=https", "--tlsv1.2", source["url"], "-o", partial)
            partial.replace(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != source["sha256"]:
            raise RuntimeError(f"Source checksum mismatch: {filename}")
        # LGPL supplements retain complete source and Homebrew modifications.
        # The reviewed libxmp notice-only input also contains test music: extract
        # its notices without distributing that archive or unrelated test data.
        if not source.get("notices_only", False):
            shutil.copy2(archive, target / "sources" / filename)
        if filename.endswith((".tar.xz", ".tar.gz")):
            with tarfile.open(archive) as contents:
                for member in contents:
                    path = PurePosixPath(member.name)
                    if path.is_absolute() or ".." in path.parts:
                        raise RuntimeError(f"Unsafe source archive path: {member.name}")
                    if member.isfile() and (is_notice(path) or str(path) in packet["required_notices"]):
                        destination = target / "upstream" / path
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        with contents.extractfile(member) as notice:
                            destination.write_bytes(notice.read())
    for relative in packet["required_notices"]:
        if not (target / "upstream" / relative).is_file():
            raise RuntimeError(f"Source packet lacks required notice: {relative}")
    provenance = {**packet, "installed_recipe_sha256": recipe_hash}
    (target / "source-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")


def collect_notices(kegs, licenses, cache, packets):
    records, failures = [], []
    for name, keg in sorted(kegs.items()):
        try:
            target = licenses / name
            target.mkdir(parents=True, exist_ok=True)
            notices = sorted(path for path in keg.rglob("*")
                             if path.is_file() and is_notice(path.relative_to(keg)))
            if name in packets:
                supplement_notices(name, keg, target, cache, packets[name])
            elif not any(not path.name.lower().startswith("authors") for path in notices):
                raise RuntimeError("No distributable license notices; a verified source packet is required")
            for path in notices:
                destination = target / path.relative_to(keg)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
            for path in [keg / "INSTALL_RECEIPT.json", keg / "sbom.spdx.json",
                         *sorted((keg / ".brew").glob("*.rb"))]:
                if path.is_file():
                    shutil.copy2(path, target / path.name)
            record = {"formula": name, "version": keg.name,
                      "installed_notices": [str(path.relative_to(keg)) for path in notices],
                      "source_packet": name in packets}
            records.append(record)
            print(f"Dependency notices: {name}@{keg.name}: {len(notices)} installed; "
                  f"source packet={name in packets}")
        except (RuntimeError, OSError, tarfile.TarError, subprocess.CalledProcessError) as error:
            failures.append(f"{name}@{keg.name}: {error}")
    # Audit the whole actual relocated closure before failing, not one keg per CI run.
    if failures:
        raise RuntimeError("Dependency license collection failed:\n" + "\n".join(failures))
    return records


def stage_app(source, build, output):
    """Use current source resources, never an incremental build's stale copy."""
    app = output / "Duel 6 Reloaded.app"
    if app.exists():
        shutil.rmtree(app)
    shutil.copytree(build / app.name, app, symlinks=True)
    resources = app / "Contents/Resources"
    if resources.exists():
        shutil.rmtree(resources)
    shutil.copytree(source / "resources", resources, symlinks=True)
    # A developer's local save must never enter a release bundle.
    (resources / "data/persons.json").unlink(missing_ok=True)
    return app


def package(args):
    output = args.output.resolve()
    app = stage_app(args.source, args.build, output)
    resources = app / "Contents/Resources"
    frameworks = app / "Contents/Frameworks"
    frameworks.mkdir(exist_ok=True)
    licenses = resources / "licenses"
    licenses.mkdir(exist_ok=True)
    shutil.copy2(args.source / "LICENSE", licenses / "duel6r-LICENSE.txt")
    shutil.copy2(args.source / "macos/README.md", resources / "README-macos.md")
    shutil.copy2(args.lua / "src/lua.h", licenses / "lua-5.3.6-license-and-header.txt")
    shutil.copy2(args.lua_archive, licenses / args.lua_archive.name)

    prefix = Path(run("brew", "--prefix"))
    cellar = Path(run("brew", "--cellar")).resolve()
    extra = []
    origins = []
    # Current Homebrew SDL2 may be sdl2-compat. Its SDL3 is dlopen'ed rather
    # than linked, so otool alone cannot discover it. No renderer/API migration.
    sdl2 = (prefix / "lib/libSDL2.dylib").resolve(strict=True)
    if "sdl2-compat" in sdl2.parts:
        # sdl2-compat's macOS loader explicitly tries @loader_path/libSDL3.dylib.
        sdl3 = prefix / "lib/libSDL3.dylib"
        if not sdl3.is_file():
            raise RuntimeError("sdl2-compat requires the installed SDL3 runtime")
        bundled = frameworks / sdl3.name
        shutil.copy2(sdl3.resolve(), bundled)
        extra.append(bundled)
        origins.append(sdl3.resolve())

    dependencies = args.build / "macos-dependencies.txt"
    run("cmake", f"-DAPP={app}", f"-DEXTRA_LIBS={';'.join(map(str, extra))}",
        f"-DLIB_DIRS={prefix / 'lib'}", f"-DDEPENDENCY_LIST={dependencies}",
        "-P", args.source / "macos/Bundle.cmake")
    origins.extend(Path(line).resolve() for line in dependencies.read_text().splitlines() if line)
    kegs = {}
    for path in origins:
        if not path.is_relative_to(cellar):
            if path.is_relative_to(app) or str(path).startswith(("/System/Library/", "/usr/lib/")):
                continue
            raise RuntimeError(f"Unaccounted dependency source: {path}")
        name, version, *_ = path.relative_to(cellar).parts
        kegs[name] = cellar / name / version

    packets = json.loads((args.source / "macos/license-sources.json").read_text())
    records = collect_notices(kegs, licenses, args.build / "license-sources", packets)

    binaries = [app / "Contents/MacOS/Duel 6 Reloaded"]
    binaries.extend(path for path in frameworks.rglob("*")
                    if path.is_file() and not path.is_symlink() and
                    "Mach-O" in run("file", "-b", path))
    for binary in binaries:
        if run("lipo", "-archs", binary).split() != ["arm64"]:
            raise RuntimeError(f"Not an arm64-only binary: {binary}")
        commands = load_commands(binary)
        minimums = re.findall(r"\bminos (\d+(?:\.\d+)+)", commands)
        minimums += re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)+)", commands)
        if not minimums or any(tuple(map(int, value.split('.'))) > (14, 0, 0)
                               for value in minimums):
            raise RuntimeError(f"Dependency requires newer than macOS 14.0: {binary}: {minimums}")
        # Remove Homebrew/build-tree RPATHs. Any remaining runtime search must
        # be local to the bundle, including explicitly bundled loadable modules.
        for rpath in re.findall(r"cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset", commands):
            run("install_name_tool", "-delete_rpath", rpath, binary)
        run("install_name_tool", "-add_rpath", "@loader_path", binary)
        if binary == binaries[0]:
            run("install_name_tool", "-add_rpath", "@executable_path/../Frameworks", binary)

    # Verify the relocated closure again, then repair the code-integrity seals
    # required by arm64 after load-command edits. This is NOT Developer ID
    # signing/notarization; no identity, entitlement or system trust is granted.
    run("cmake", f"-DAPP={app}", "-P", args.source / "macos/VerifyBundle.cmake")
    metadata = {
        "source_revision": args.revision, "architecture": "arm64", "minimum_macos": "14.0",
        "renderer": "gl1", "local_only": True, "configuration": "Release",
        "distribution": "unsigned (ad-hoc integrity seals only); not notarized",
        "status": "experimental; native launch, gameplay and visual QA deferred until after nightly",
        "lua": {"version": "5.3.6", "sha256": hashlib.sha256(args.lua_archive.read_bytes()).hexdigest()},
        "homebrew_dependencies": records,
    }
    text = json.dumps(metadata, indent=2) + "\n"
    (output / "build-info.json").write_text(text)
    (resources / "build-info.json").write_text(text)
    for path in app.rglob("*"):
        if path.is_symlink() and not path.resolve().is_relative_to(app):
            raise RuntimeError(f"External bundle symlink: {path}")
    for binary in binaries[1:]:
        run("codesign", "--force", "--sign", "-", "--timestamp=none", binary)
    run("codesign", "--force", "--sign", "-", "--timestamp=none", app)
    run("codesign", "--verify", "--deep", "--strict", app)
    # Keep the inventory outside the sealed bundle: no signing/hash cycles.
    (output / "bundle.sha256sums").write_text("".join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(output)}\n"
        for path in sorted(app.rglob("*")) if path.is_file() and not path.is_symlink()))
    archive = output / "duel6r-macos-arm64.zip"
    archive.unlink(missing_ok=True)
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, archive)
    (output / (archive.name + ".sha256")).write_text(
        f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n")
    print(f"Packaged {archive} at {args.revision}; native manual QA remains deferred.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "build", "output", "lua", "lua-archive"):
        parser.add_argument(f"--{option}", required=True, type=Path)
    parser.add_argument("--revision", required=True)
    package(parser.parse_args())
