#!/usr/bin/env python3
"""Package the experimental native network app; no graphical QA is performed."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import subprocess
import tarfile

HELPERS = ("duel6r-server", "duel6r-darwin-guardian", "duel6r-resolver")
CURL_VERSION = "8.21.0"
CURL_ARCHIVE_SHA256 = "ad6f2f94934b38e31e48272833c99b891d045b4565fe942a53fbd27bd3910e16"
MACOS_MINIMUMS = {"arm64": "14.0", "x86_64": "13.0"}


def minimum_macos(architecture):
    if architecture not in MACOS_MINIMUMS:
        raise RuntimeError(f"Unsupported macOS architecture: {architecture}")
    return MACOS_MINIMUMS[architecture]


def require_private_curl(origins, prefix):
    library = (prefix / "lib/libcurl.4.dylib").resolve(strict=True)
    if not library.is_relative_to(prefix.resolve()):
        raise RuntimeError("Private curl library escapes its prefix")
    # Directory aliases (notably Darwin's /var -> /private/var) must be
    # canonicalized on both sides. Equal contents at a foreign path do not
    # establish the required private dependency origin.
    try:
        curl_origins = {path.resolve(strict=True) for path in origins if path.name.startswith("libcurl")}
    except (OSError, RuntimeError) as error:
        raise RuntimeError("Expected only the pinned private curl in dependency closure: unresolved origin") from error
    if curl_origins != {library}:
        raise RuntimeError(f"Expected only the pinned private curl in dependency closure: {curl_origins}")
    return library


def collect_private_curl(prefix, archive, licenses, architecture):
    if hashlib.sha256(archive.read_bytes()).hexdigest() != CURL_ARCHIVE_SHA256:
        raise RuntimeError("Private curl source checksum mismatch")
    library = require_private_curl([(prefix / "lib/libcurl.4.dylib").resolve()], prefix)
    metadata = json.loads((prefix / "build-info.json").read_text())
    expected = dict(version=CURL_VERSION, archive_sha256=CURL_ARCHIVE_SHA256,
                    architecture=architecture, minimum_macos=minimum_macos(architecture),
                    native_trust="Apple SecTrust", ca_fallback=False,
                    library_sha256=hashlib.sha256(library.read_bytes()).hexdigest())
    if any(metadata.get(key) != value for key, value in expected.items()):
        raise RuntimeError("Private curl build provenance mismatch")
    # Read the notice from the verified archive, not a mutable extracted tree.
    with tarfile.open(archive) as contents:
        member = contents.getmember(f"curl-{CURL_VERSION}/COPYING")
        if not member.isfile():
            raise RuntimeError("Private curl COPYING must be a regular archive member")
        notice = contents.extractfile(member).read()
    target = licenses / "curl"
    target.mkdir(parents=True, exist_ok=True)
    (target / "COPYING").write_bytes(notice)
    (target / "source-provenance.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return metadata


def run(*args, timeout=None):
    try:
        return subprocess.check_output([str(arg) for arg in args], text=True, timeout=timeout).strip()
    except subprocess.CalledProcessError as error:
        print(error.output)
        raise


def load_commands(binary):
    return run("otool", "-l", binary)


def require_macos_binary(binary, architecture):
    minimum = minimum_macos(architecture)
    if run("lipo", "-archs", binary).split() != [architecture]:
        raise RuntimeError(f"Not a {architecture}-only binary: {binary}")
    commands = load_commands(binary)
    minimums = re.findall(r"\bminos (\d+(?:\.\d+)+)", commands)
    minimums += re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)+)", commands)
    ceiling = tuple(map(int, minimum.split('.'))) + (0,)
    if not minimums or any(tuple(map(int, value.split('.'))) > ceiling
                           for value in minimums):
        raise RuntimeError(f"Dependency requires newer than macOS {minimum}: {binary}: {minimums}")
    return commands


def require_macos_plist(app, architecture):
    with (app / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    if (info.get("LSMinimumSystemVersion") != minimum_macos(architecture)
            or info.get("LSArchitecturePriority") != [architecture]):
        raise RuntimeError("Bundle plist architecture/deployment floor mismatch")


def is_notice(path):
    return bool(re.match(r"^(licen[cs]es?|copying|copyright|notice|authors)([.\-_ ]|$)",
                         path.name, re.IGNORECASE)) or any(
        part.lower() in {"licenses", "licences", "copying"} for part in path.parts[:-1])


def source_notices(packet, archive):
    """Read verbatim notices from pinned source, including static vendored code."""
    if hashlib.sha256(archive.read_bytes()).hexdigest() != packet["sha256"]:
        raise RuntimeError("Source checksum mismatch: " + packet["name"])
    notices = {}
    with tarfile.open(archive) as contents:
        for member in contents:
            path = PurePosixPath(member.name)
            if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != packet["root"]:
                raise RuntimeError("Unsafe source archive path: " + member.name)
            relative = str(path.relative_to(packet["root"]))
            if member.isfile() and (is_notice(PurePosixPath(relative)) or relative in packet["required_notices"]):
                notices[relative] = contents.extractfile(member).read()
    if not set(packet["required_notices"]) <= notices.keys():
        raise RuntimeError("Source packet lacks required notice: " + packet["name"])
    return notices


def collect_runtime(prefix, manifest, archives, licenses, architecture, origins):
    prefix = prefix.resolve(strict=True)
    metadata = json.loads((prefix / "build-info.json").read_text())
    if (metadata.get("architecture") != architecture or metadata.get("minimum_macos") != minimum_macos(architecture)
            or metadata.get("lock_sha256") != hashlib.sha256(manifest.read_bytes()).hexdigest()
            or metadata.get("builder_sha256") != hashlib.sha256((manifest.parent / "build-runtime.py").read_bytes()).hexdigest()):
        raise RuntimeError("Private runtime build provenance mismatch")
    libraries = metadata.get("libraries", {})
    if not libraries:
        raise RuntimeError("Missing private runtime library inventory")
    verified = set()
    for relative, digest in libraries.items():
        path = (prefix / relative).resolve(strict=True)
        if not path.is_relative_to(prefix) or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError("Private runtime library identity mismatch: " + relative)
        verified.add(path)
    if not set(origins) <= verified:
        raise RuntimeError("Unaccounted private runtime dependency origin")
    target = licenses / "runtime"
    target.mkdir(parents=True, exist_ok=True)
    for project in json.loads(manifest.read_text())["projects"]:
        for packet in [project, *project.get("vendored", [])]:
            destination = target / project["name"] / packet["name"]
            for relative, data in source_notices(packet, archives / packet["sha256"]).items():
                notice = destination / relative
                notice.parent.mkdir(parents=True, exist_ok=True)
                notice.write_bytes(data)
            (destination / "source-provenance.json").write_text(json.dumps(packet, indent=2) + "\n")
        for patch in project.get("patches", []):
            data = (archives / patch["sha256"]).read_bytes()
            if hashlib.sha256(data).hexdigest() != patch["sha256"]:
                raise RuntimeError("Source patch checksum mismatch")
            (target / project["name"] / patch["name"]).write_bytes(data)
    shutil.copy2(manifest, target / manifest.name)
    shutil.copy2(manifest.parent / "build-runtime.py", target / "build-runtime.py")
    (target / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return metadata


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
    retrievals = []
    for source in packet["sources"]:
        filename = source["url"].rsplit("/", 1)[1]
        archive = cache / filename
        cache_hit = archive.exists()
        requested_url = effective_url = None
        if not cache_hit:
            partial = archive.with_name(filename + ".part")
            partial.unlink(missing_ok=True)
            urls = [source["url"], *source.get("fallback_urls", [])]
            for url in urls:
                try:
                    # Explicit mirrors replace repeated attempts at one failed
                    # host. Preserve three retries for packets without mirrors.
                    effective = run("curl", "--fail", "--location", "--retry", "0" if len(urls) > 1 else "3",
                                    "--connect-timeout", "10", "--max-time", "120", "--max-redirs", "5",
                                    "--proto", "=https", "--proto-redir", "=https", "--tlsv1.2",
                                    "--write-out", "%{url_effective}", url, "-o", partial,
                                    timeout=125 if len(urls) > 1 else 500)
                    if not effective.startswith("https://"):
                        raise RuntimeError(f"Non-HTTPS source origin: {filename}")
                    if hashlib.sha256(partial.read_bytes()).hexdigest() != source["sha256"]:
                        # Integrity failure is not a transport outage. Do not
                        # hide it with fallback or promote corrupt cache bytes.
                        raise RuntimeError(f"Source checksum mismatch: {filename}")
                    partial.replace(archive)
                    requested_url, effective_url = url, effective
                    break
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    print(f"Source retrieval failed: {url}")
                finally:
                    partial.unlink(missing_ok=True)
            else:
                raise RuntimeError(f"Source retrieval exhausted approved endpoints: {filename}")
        elif hashlib.sha256(archive.read_bytes()).hexdigest() != source["sha256"]:
            raise RuntimeError(f"Source checksum mismatch: {filename}")
        # Old verified caches have no retrieval receipt. Record that honestly;
        # never infer a primary or mirror origin merely from the archive name.
        retrievals.append({"archive": filename, "sha256": source["sha256"], "cache_hit": cache_hit,
                           "requested_url": requested_url, "effective_url": effective_url})
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
    provenance = {**packet, "installed_recipe_sha256": recipe_hash, "retrievals": retrievals}
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


def require_regular_entrypoints(app):
    for relative in ("Contents/Info.plist", "Contents/MacOS/Duel 6 Reloaded"):
        path = app / relative
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(app.resolve()):
            raise RuntimeError(f"Signing requires a regular file inside the bundle: {path}")


def require_relocatable_links(app):
    for path in app.rglob("*"):
        if path.is_symlink() and (path.readlink().is_absolute()
                                  or not path.resolve(strict=True).is_relative_to(app.resolve())):
            raise RuntimeError(f"Non-relocatable bundle symlink: {path}")


def stage_helpers(build, app):
    for name in HELPERS:
        source = build / name
        if not source.is_file() or not source.resolve().is_relative_to(build.resolve()):
            raise RuntimeError(f"Missing or external macOS network helper: {source}")
        destination = app / "Contents/MacOS" / name
        destination.unlink(missing_ok=True)
        shutil.copy2(source.resolve(), destination)
    expected = {"Duel 6 Reloaded", *HELPERS}
    for path in (app / "Contents/MacOS").iterdir():
        if path.name not in expected or path.is_symlink() or not path.is_file():
            raise RuntimeError(f"Unexpected executable entry in macOS bundle: {path}")


def collect_mbedtls(prefix, source, licenses):
    if not (source / "LICENSE").is_file():
        raise RuntimeError("Missing pinned static Mbed TLS license source")
    target = licenses / "mbedtls"
    for notice in sorted(source.rglob("*")):
        if notice.is_file() and is_notice(notice.relative_to(source)):
            destination = target / notice.relative_to(source)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(notice, destination)
    configuration = prefix / "include/mbedtls/mbedtls_config.h"
    shutil.copy2(configuration, target / "compiled-mbedtls-config.h")
    record = {"version": "3.6.7", "linkage": "static", "hardware_aes_only": True,
              "source_archive_sha256": "a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6",
              "configuration_sha256": hashlib.sha256(configuration.read_bytes()).hexdigest(),
              "libraries": {name: hashlib.sha256((prefix / "lib" / name).read_bytes()).hexdigest()
                            for name in ("libmbedtls.a", "libmbedx509.a", "libmbedcrypto.a")}}
    (target / "source-provenance.json").write_text(json.dumps(record, indent=2) + "\n")
    return record


def stage_app(source, build, output):
    """Use current source resources, never an incremental build's stale copy."""
    app = output / "Duel 6 Reloaded.app"
    if app.exists():
        shutil.rmtree(app)
    shutil.copytree(build / app.name, app, symlinks=True)
    main = app / "Contents/MacOS/Duel 6 Reloaded"
    if main.is_symlink():
        # CMake VERSION produces a logical-name symlink to the versioned binary.
        # codesign requires the declared main executable to be a regular file.
        # Move only a sibling target within this staged bundle, preserving bytes
        # and mode without leaving a redundant, unsealed executable alongside it.
        target = main.resolve(strict=True)
        if (target.parent != main.parent.resolve() or not target.is_file()
                or not target.is_relative_to(app.resolve())):
            raise RuntimeError(f"Unexpected main executable symlink target: {main} -> {target}")
        main.unlink()
        target.replace(main)
    require_regular_entrypoints(app)
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
    require_macos_plist(app, args.architecture)
    stage_helpers(args.build, app)
    resources = app / "Contents/Resources"
    frameworks = app / "Contents/Frameworks"
    frameworks.mkdir(exist_ok=True)
    licenses = resources / "licenses"
    licenses.mkdir(exist_ok=True)
    shutil.copy2(args.source / "LICENSE", licenses / "duel6r-LICENSE.txt")
    shutil.copy2(args.source / "macos/README.md", resources / "README-macos.md")
    shutil.copy2(args.lua / "src/lua.h", licenses / "lua-5.3.6-license-and-header.txt")
    shutil.copy2(args.lua_archive, licenses / args.lua_archive.name)
    mbedtls_record = collect_mbedtls(args.mbedtls_prefix, args.mbedtls_source, licenses)

    prefix = args.runtime_prefix.resolve(strict=True)
    extra = []
    origins = []
    # The pinned SDL2 compatibility layer loads SDL3 rather than linking it;
    # otool alone cannot discover it. No application renderer/API migration.
    sdl2 = (prefix / "lib/libSDL2.dylib").resolve(strict=True)
    if sdl2.is_relative_to(prefix):
        # sdl2-compat's macOS loader explicitly tries @loader_path/libSDL3.dylib.
        sdl3 = prefix / "lib/libSDL3.dylib"
        if not sdl3.is_file():
            raise RuntimeError("sdl2-compat requires the private SDL3 runtime")
        bundled = frameworks / sdl3.name
        shutil.copy2(sdl3.resolve(), bundled)
        extra.append(bundled)
        origins.append(sdl3.resolve())

    dependencies = args.build / "macos-dependencies.txt"
    run("cmake", f"-DAPP={app}", f"-DEXTRA_LIBS={';'.join(map(str, extra))}",
        f"-DLIB_DIRS={prefix / 'lib'}", f"-DDEPENDENCY_LIST={dependencies}",
        "-P", args.source / "macos/Bundle.cmake")
    origins.extend(Path(line).resolve() for line in dependencies.read_text().splitlines() if line)
    private_curl = require_private_curl(origins, args.curl_prefix)
    runtime_origins = []
    for path in origins:
        if path == private_curl:
            continue
        if not path.is_relative_to(prefix):
            if path.is_relative_to(app) or str(path).startswith(("/System/Library/", "/usr/lib/")):
                continue
            raise RuntimeError(f"Unaccounted dependency source: {path}")
        runtime_origins.append(path)
    runtime_record = collect_runtime(prefix, args.source / "macos/runtime-sources.json",
                                     args.runtime_archives, licenses, args.architecture, runtime_origins)
    curl_record = collect_private_curl(args.curl_prefix, args.curl_archive, licenses, args.architecture)
    curl_record = {**curl_record, "bundled": private_curl is not None}

    binaries = [app / "Contents/MacOS/Duel 6 Reloaded"]
    binaries.extend(app / "Contents/MacOS" / name for name in HELPERS)
    binaries.extend(path for path in frameworks.rglob("*")
                    if path.is_file() and not path.is_symlink() and
                    "Mach-O" in run("file", "-b", path))
    for binary in binaries:
        commands = require_macos_binary(binary, args.architecture)
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
        "source_revision": args.revision, "architecture": args.architecture,
        "minimum_macos": minimum_macos(args.architecture),
        "renderer": "gl1", "local_only": False, "configuration": "Release",
        "directory_default_url": args.directory_default_url,
        "build_host": {"macos": run("sw_vers", "-productVersion"), "architecture": run("uname", "-m"),
                       "sdk": run("xcrun", "--sdk", "macosx", "--show-sdk-version"),
                       "compiler": run("xcrun", "--sdk", "macosx", "clang", "--version")},
        "distribution": "unsigned (ad-hoc integrity seals only); not notarized",
        "status": "experimental; native launch, gameplay and visual QA deferred until after nightly",
        "lua": {"version": "5.3.6", "sha256": hashlib.sha256(args.lua_archive.read_bytes()).hexdigest()},
        "source_built_runtime": runtime_record,
        "private_curl": curl_record,
        "static_mbedtls": mbedtls_record,
        "network_helpers": list(HELPERS),
    }
    text = json.dumps(metadata, indent=2) + "\n"
    (output / "build-info.json").write_text(text)
    if args.architecture == "x86_64":
        (output / "build-info-x86_64.json").write_text(text)
    (resources / "build-info.json").write_text(text)
    require_relocatable_links(app)
    require_regular_entrypoints(app)
    for binary in binaries[1:]:
        run("codesign", "--force", "--sign", "-", "--timestamp=none", binary)
    run("codesign", "--force", "--sign", "-", "--timestamp=none", app)
    run("codesign", "--verify", "--deep", "--strict", app)
    # Keep the inventory outside the sealed bundle: no signing/hash cycles.
    (output / "bundle.sha256sums").write_text("".join(
        f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(output)}\n"
        for path in sorted(app.rglob("*")) if path.is_file() and not path.is_symlink()))
    archive = output / f"duel6r-macos-{args.architecture}.zip"
    archive.unlink(missing_ok=True)
    run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", app, archive)
    (output / (archive.name + ".sha256")).write_text(
        f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n")
    print(f"Packaged {archive} at {args.revision}; native manual QA remains deferred.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "build", "output", "lua", "lua-archive", "curl-prefix", "curl-archive",
                   "mbedtls-prefix", "mbedtls-source", "runtime-prefix", "runtime-archives"):
        parser.add_argument(f"--{option}", required=True, type=Path)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--architecture", required=True, choices=("arm64", "x86_64"))
    parser.add_argument("--directory-default-url", default="")
    package(parser.parse_args())
