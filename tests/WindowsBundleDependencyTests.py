#!/usr/bin/env python3
"""Validate that a packaged Windows runtime bundle is self-contained.

The test walks the import table of every PE executable and DLL in a bundle and
fails if a non-system DLL dependency cannot be resolved from the bundle itself.
This catches incomplete MinGW/vcpkg runtime packages before they are published
or launched on Windows.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
from collections import deque


WINDOWS_SYSTEM_DLLS = {
    "advapi32.dll",
    "bcrypt.dll",
    "cfgmgr32.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "crypt32.dll",
    "d3d11.dll",
    "d3d9.dll",
    "dinput8.dll",
    "dsound.dll",
    "dwmapi.dll",
    "dxgi.dll",
    "gdi32.dll",
    "hid.dll",
    "imm32.dll",
    "iphlpapi.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ntdll.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "powrprof.dll",
    "rpcrt4.dll",
    "secur32.dll",
    "setupapi.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "uuid.dll",
    "version.dll",
    "winhttp.dll",
    "winmm.dll",
    "winspool.drv",
    "ws2_32.dll",
    "xinput1_4.dll",
}


def imported_dlls(objdump: str, path: pathlib.Path) -> list[str]:
    result = subprocess.run(
        [objdump, "-p", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )

    dlls: list[str] = []
    for line in result.stdout.splitlines():
        marker = "DLL Name: "
        if marker in line:
            dll_name = line.split(marker, 1)[1].strip()
            if dll_name:
                dlls.append(dll_name)
    return dlls


def is_pe64(objdump: str, path: pathlib.Path) -> bool:
    result = subprocess.run(
        [objdump, "-f", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    return "file format pei-x86-64" in result.stdout


def bundle_files(dist_dir: pathlib.Path) -> dict[str, pathlib.Path]:
    files: dict[str, pathlib.Path] = {}
    for path in dist_dir.iterdir():
        if path.is_file() and path.suffix.lower() in {".exe", ".dll"}:
            files[path.name.lower()] = path
    return files


def validate_bundle(dist_dir: pathlib.Path, objdump: str) -> list[str]:
    files = bundle_files(dist_dir)
    errors: list[str] = []
    queue = deque(files.values())
    scanned: set[pathlib.Path] = set()

    while queue:
        current = queue.popleft()
        if current in scanned:
            continue
        scanned.add(current)

        if not is_pe64(objdump, current):
            errors.append(f"{current.name} is not a 64-bit Windows PE file")
            continue

        for dll_name in imported_dlls(objdump, current):
            normalized = dll_name.lower()
            if normalized in WINDOWS_SYSTEM_DLLS:
                continue

            dependency = files.get(normalized)
            if dependency is None:
                errors.append(f"{current.name} imports {dll_name}, but {dll_name} is not bundled")
                continue

            if dependency not in scanned:
                queue.append(dependency)

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist-dir", required=True, type=pathlib.Path, help="Windows runtime bundle directory")
    parser.add_argument(
        "--objdump",
        default="x86_64-w64-mingw32-objdump",
        help="objdump executable that can inspect PE import tables",
    )
    args = parser.parse_args()

    dist_dir = args.dist_dir.resolve()
    if not dist_dir.is_dir():
        print(f"Windows bundle directory does not exist: {dist_dir}", file=sys.stderr)
        return 1

    errors = validate_bundle(dist_dir, args.objdump)
    if errors:
        print("Windows runtime bundle dependency validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(f"Windows runtime bundle dependency validation passed: {dist_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
