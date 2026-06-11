#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${WORKSPACE_DIR:-/workspace}"
output_dir="build"
build_type="${BUILD_TYPE:-Release}"
renderer="${D6R_RENDERER:-gl4}"
with_lua="${D6R_WITH_LUA:-ON}"
build_testing="${BUILD_TESTING:-ON}"
run_tests="${RUN_TESTS:-OFF}"
toolchain_file="${TOOLCHAIN_FILE:-/opt/toolchains/mingw-w64-x86_64.cmake}"
clean_output_dir="${CLEAN_OUTPUT_DIR:-OFF}"

tmp_build_dir="$(mktemp -d /tmp/duel6r-build-win-XXXXXX)"
cleanup() {
  rm -rf "${tmp_build_dir}"
}
trap cleanup EXIT

cmake -S "${workspace_dir}" -B "${tmp_build_dir}" \
  -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}" \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DBUILD_TESTING="${build_testing}" \
  -DD6R_RENDERER="${renderer}" \
  -DD6R_WITH_LUA="${with_lua}"

icon_file="${workspace_dir}/source/icon1.ico"
if [[ -f "${icon_file}" ]]; then
  cp "${icon_file}" "${tmp_build_dir}/icon1.ico"
fi

cmake --build "${tmp_build_dir}" -j"$(nproc)"

if [[ "${run_tests}" == "ON" ]]; then
  echo "RUN_TESTS=ON is not supported for Windows cross builds in this container." >&2
  echo "Use RUN_TESTS=OFF or execute tests on a native Windows runner." >&2
  exit 1
fi

if [[ "${clean_output_dir}" == "ON" ]]; then
  rm -rf "${workspace_dir}/${output_dir}"
fi
mkdir -p "${workspace_dir}/${output_dir}"

source_binary="${tmp_build_dir}/duel6r.exe"
if [[ ! -f "${source_binary}" && -f "${tmp_build_dir}/duel6rd.exe" ]]; then
  source_binary="${tmp_build_dir}/duel6rd.exe"
fi

if [[ ! -f "${source_binary}" ]]; then
  echo "Unable to find built Windows application in ${tmp_build_dir}" >&2
  exit 1
fi

cp "${source_binary}" "${workspace_dir}/${output_dir}/duel6r.exe"
cp -R "${workspace_dir}/resources/." "${workspace_dir}/${output_dir}/"

python3 - "${workspace_dir}/${output_dir}/duel6r.exe" "${workspace_dir}/${output_dir}" <<'PY'
import pathlib
import shutil
import subprocess
import sys

exe_path = pathlib.Path(sys.argv[1])
dist_dir = pathlib.Path(sys.argv[2])
vcpkg_bin = pathlib.Path("/opt/vcpkg/installed/x64-mingw-dynamic/bin")

search_roots = [
    vcpkg_bin,
    pathlib.Path("/usr/x86_64-w64-mingw32/bin"),
    pathlib.Path("/usr/x86_64-w64-mingw32/lib"),
]
search_roots.extend(pathlib.Path("/usr/lib/gcc/x86_64-w64-mingw32").glob("*/"))

seen = set()
queue = [exe_path]
copied = []

windows_system_dlls = {
    "advapi32.dll",
    "bcrypt.dll",
    "comdlg32.dll",
    "crypt32.dll",
    "d3d11.dll",
    "d3d9.dll",
    "dwmapi.dll",
    "dxgi.dll",
    "gdi32.dll",
    "imm32.dll",
    "iphlpapi.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "rpcrt4.dll",
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
}

optional_sdl_mixer_dll_prefixes = (
    "flac",
    "fluidsynth",
    "gme",
    "libflac",
    "libfluidsynth",
    "libgme",
    "libmikmod",
    "libmodplug",
    "libmpg123",
    "libogg",
    "libopus",
    "libvorbis",
    "libwavpack",
    "libxmp",
    "mikmod",
    "modplug",
    "mpg123",
    "ogg",
    "opus",
    "vorbis",
    "vorbisfile",
    "wavpack",
    "xmp",
)


def imported_dlls(path: pathlib.Path) -> list[str]:
    result = subprocess.run(
        ["x86_64-w64-mingw32-objdump", "-p", str(path)],
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


def verify_pe64(path: pathlib.Path) -> None:
    result = subprocess.run(
        ["x86_64-w64-mingw32-objdump", "-f", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    if "file format pei-x86-64" not in result.stdout:
        raise RuntimeError(f"{path} is not a 64-bit Windows PE file")


def resolve_dll(name: str, origin: pathlib.Path) -> pathlib.Path | None:
    lower_name = name.lower()
    if lower_name in windows_system_dlls:
        return None

    origin_candidate = origin.parent / name
    if origin_candidate.exists():
        return origin_candidate
    for path in origin.parent.glob("*.dll"):
        if path.name.lower() == lower_name:
            return path

    for root in search_roots:
        if not root.exists():
            continue
        candidate = root / name
        if candidate.exists():
            return candidate
        for path in root.glob("*.dll"):
            if path.name.lower() == lower_name:
                return path
    return None


def copy_dll(path: pathlib.Path) -> pathlib.Path:
    verify_pe64(path)
    target = dist_dir / path.name
    if not target.exists():
        shutil.copy2(path, target)
        copied.append(target.name)
    return target


def copy_imported_dependency_closure() -> None:
    while queue:
        current = queue.pop(0)
        for dll_name in imported_dlls(current):
            normalized = dll_name.lower()
            if normalized in seen:
                continue
            seen.add(normalized)

            resolved = resolve_dll(dll_name, current)
            if resolved is None:
                continue

            queue.append(copy_dll(resolved))


verify_pe64(exe_path)
copy_imported_dependency_closure()

# SDL_mixer loads some music decoders dynamically, so they do not appear in
# the executable's import table. Bundle the optional decoder DLLs explicitly;
# libxmp/libmodplug are required for the menu's .xm tracker module on Windows.
optional_decoder_dlls = []
if vcpkg_bin.exists():
    for path in vcpkg_bin.glob("*.dll"):
        lower_name = path.name.lower()
        if lower_name.startswith(optional_sdl_mixer_dll_prefixes):
            optional_decoder_dlls.append(copy_dll(path))

queue.extend(optional_decoder_dlls)
copy_imported_dependency_closure()

(dist_dir / "windows-dependencies.txt").write_text("\n".join(sorted(copied)) + "\n")
PY

python3 "${workspace_dir}/tests/WindowsBundleDependencyTests.py" \
  --dist-dir "${workspace_dir}/${output_dir}"

echo "Windows runtime files written to ${workspace_dir}/${output_dir}"
