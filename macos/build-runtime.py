#!/usr/bin/env python3
"""Build the private, pinned macOS multimedia/HTTPS dependency prefix."""

import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile

from package import minimum_macos, require_macos_binary, source_notices


def checked(*args, cwd=None, env=None):
    subprocess.run([str(arg) for arg in args], cwd=cwd, env=env, check=True)


def fetch(packet, cache):
    archive = cache / packet["sha256"]
    if not archive.exists():
        partial = archive.with_suffix(".part")
        try:
            checked("/usr/bin/curl", "--fail", "--location", "--retry", "3",
                    "--connect-timeout", "10", "--max-time", "180", "--max-redirs", "5",
                    "--proto", "=https", "--proto-redir", "=https", "--tlsv1.2",
                    packet["url"], "-o", partial)
            if hashlib.sha256(partial.read_bytes()).hexdigest() != packet["sha256"]:
                raise RuntimeError("Source checksum mismatch: " + packet["name"])
            partial.replace(archive)
        finally:
            partial.unlink(missing_ok=True)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != packet["sha256"]:
        raise RuntimeError("Source checksum mismatch: " + packet["name"])
    return archive


def unpack(packet, archive, destination):
    # Audit paths, digest and required notices before extracting any source.
    source_notices(packet, archive)
    destination.mkdir(parents=True)
    with tarfile.open(archive) as contents:
        contents.extractall(destination, filter="data")
    return destination / packet["root"]


def main():
    architecture = os.environ["D6R_MACOS_ARCH"]
    minimum = minimum_macos(architecture)
    if platform.system() != "Darwin" or platform.machine() != architecture:
        raise RuntimeError("Runtime dependencies require the matching native Mac")
    root = Path(__file__).resolve().parents[1]
    lock = root / "macos/runtime-sources.json"
    projects = json.loads(lock.read_text())["projects"]
    deps = root / "build/macos-deps"
    cache = deps / "runtime-archives"
    cache.mkdir(parents=True, exist_ok=True)
    # Preflight every pinned archive/notice before replacing any task-owned output.
    for project in projects:
        for packet in [project, *project.get("vendored", [])]:
            source_notices(packet, fetch(packet, cache))
        for patch in project.get("patches", []):
            fetch(patch, cache)
    prefix = deps / "runtime"
    work = deps / "runtime-build"
    for path in (prefix, work):
        if path.exists():
            shutil.rmtree(path)
        path.mkdir()
    sdk = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
    jobs = subprocess.check_output(["sysctl", "-n", "hw.ncpu"], text=True).strip()
    flags = f"-arch {architecture} -mmacosx-version-min={minimum}"
    environment = {**os.environ, "SDKROOT": sdk, "MACOSX_DEPLOYMENT_TARGET": minimum,
                   "CFLAGS": flags, "CXXFLAGS": flags, "LDFLAGS": flags,
                   "PKG_CONFIG_PATH": str(prefix / "lib/pkgconfig"),
                   "PKG_CONFIG_LIBDIR": str(prefix / "lib/pkgconfig")}
    commands = []
    for project in projects:
        name = project["name"]
        source = unpack(project, cache / project["sha256"], work / name / "source")
        for packet in project.get("vendored", []):
            extracted = unpack(packet, cache / packet["sha256"], work / name / packet["name"])
            destination = source / "external" / packet["name"]
            if destination.exists():
                shutil.rmtree(destination)
            shutil.move(extracted, destination)
        for patch in project.get("patches", []):
            checked("patch", "-p1", "-i", cache / patch["sha256"], cwd=source)
        build = work / name / "build"
        build.mkdir()
        if project.get("builder") == "openssl":
            target = "darwin64-arm64-cc" if architecture == "arm64" else "darwin64-x86_64-cc"
            command = [source / "Configure", target, "shared", "no-tests", "no-module",
                       f"--prefix={prefix}", f"--openssldir={prefix / 'ssl'}", "--libdir=lib"]
            checked(*command, cwd=build, env=environment)
            checked("make", f"-j{jobs}", cwd=build, env=environment)
            checked("make", "install_sw", cwd=build, env=environment)
        else:
            command = ["cmake", "-S", source / project.get("cmake_subdir", "."), "-B", build, "-G", "Ninja",
                       "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                       f"-DCMAKE_OSX_ARCHITECTURES={architecture}", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={minimum}",
                       f"-DCMAKE_OSX_SYSROOT={sdk}", f"-DCMAKE_INSTALL_PREFIX={prefix}",
                       f"-DCMAKE_INSTALL_NAME_DIR={prefix / 'lib'}", f"-DCMAKE_PREFIX_PATH={prefix}",
                       "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local",
                       "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DBUILD_SHARED_LIBS=ON", "-DBUILD_TESTING=OFF",
                       *["-D" + option for option in project.get("options", [])]]
            checked(*command, env=environment)
            checked("cmake", "--build", build, "--parallel", jobs, env=environment)
            checked("cmake", "--install", build, env=environment)
        commands.append([str(arg) for arg in command])
    # sdl2-compat intentionally provides the SDL2 API and loads SDL3 dynamically.
    pkgconfig = prefix / "lib/pkgconfig"
    if not (pkgconfig / "sdl2.pc").exists():
        (pkgconfig / "sdl2.pc").symlink_to("sdl2-compat.pc")
    binaries = [path for path in (prefix / "lib").rglob("*.dylib") if not path.is_symlink()]
    if not binaries:
        raise RuntimeError("Source build produced no runtime libraries")
    for binary in binaries:
        require_macos_binary(binary, architecture)
    metadata = {"architecture": architecture, "minimum_macos": minimum,
                "lock_sha256": hashlib.sha256(lock.read_bytes()).hexdigest(),
                "builder_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "sdk": sdk, "configure_commands": commands,
                "libraries": {str(path.relative_to(prefix)): hashlib.sha256(path.read_bytes()).hexdigest()
                              for path in binaries}}
    (prefix / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
