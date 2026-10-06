#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || {
  echo "This helper requires native Apple Silicon macOS 14+." >&2; exit 1;
}
[[ "$(sw_vers -productVersion | cut -d. -f1)" -ge 14 ]] || exit 1
revision="$(git -C "$root" rev-parse HEAD)"
[[ "${D6R_SOURCE_REVISION:?Set D6R_SOURCE_REVISION to the immutable checkout SHA}" == "$revision" ]] || {
  echo "Source revision does not match HEAD." >&2; exit 1;
}
[[ -z "$(git -C "$root" status --porcelain)" ]] || {
  echo "Package builds require a clean source checkpoint." >&2; exit 1;
}

build="$root/build/macos-build"
output="$root/build/macos"
deps="$root/build/macos-deps"
mkdir -p "$deps" "$output"
python3 "$root/tests/MacPackagingTests.py"

# Invoking Xcode's clang by its absolute path does not reliably select an SDK.
# Use the selected Xcode's macOS SDK explicitly for both Lua and the application.
sdk="$(xcrun --sdk macosx --show-sdk-path)"
[[ -d "$sdk" && -f "$sdk/usr/include/string.h" ]] || {
  echo "The selected macOS SDK is missing its system headers: $sdk" >&2; exit 1;
}
export SDKROOT="$sdk"
target_flags="-isysroot \"$sdk\" -arch arm64 -mmacosx-version-min=14.0"

# lua@5.3 is no longer available in Homebrew. Build only the pinned static
# library, not an unrelated interpreter or a newer, incompatible Lua API.
lua_version=5.3.6
lua_sha256=fc5fd69bb8736323f026672b1b7235da613d7177e72558893a0bdcd320466d60
archive="$deps/lua-$lua_version.tar.gz"
if [[ ! -f "$archive" ]]; then
  curl --fail --location --retry 3 --proto '=https' --tlsv1.2 \
    "https://www.lua.org/ftp/lua-$lua_version.tar.gz" -o "$archive.part"
  mv "$archive.part" "$archive"
fi
printf '%s  %s\n' "$lua_sha256" "$archive" | shasum -a 256 -c -
lua="$deps/lua-$lua_version"
rm -rf "$lua"
tar -xzf "$archive" -C "$deps"
make -C "$lua/src" a CC="$(xcrun --sdk macosx --find clang)" \
  MYCFLAGS="-DLUA_USE_MACOSX $target_flags" MYLDFLAGS="$target_flags"

prefix="$(brew --prefix)"
curl_prefix="$(brew --prefix curl)"
openssl_prefix="$(brew --prefix openssl@3)"
cmake -S "$root/docker/mbedtls" -B "$deps/mbedtls-build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_INSTALL_PREFIX="$deps/mbedtls"
cmake --build "$deps/mbedtls-build" --parallel "$(sysctl -n hw.ncpu)"
cmake --install "$deps/mbedtls-build"
cmake -S "$root" -B "$build" -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_PREFIX_PATH="$deps/mbedtls;$curl_prefix;$prefix" \
  -DCURL_INCLUDE_DIR="$curl_prefix/include" -DCURL_LIBRARY_RELEASE="$curl_prefix/lib/libcurl.dylib" \
  -DD6R_OPENSSL_EXECUTABLE="$openssl_prefix/bin/openssl" \
  -DD6R_DIRECTORY_DEFAULT_URL="${D6R_DIRECTORY_DEFAULT_URL:-}" \
  -DD6R_RENDERER=gl1 -DD6R_WITH_LUA=ON -DBUILD_TESTING=ON \
  -DLUA_INCLUDE_DIR="$lua/src" -DLUA_LIBRARY="$lua/src/liblua.a"
cmake --build "$build" --parallel "$(sysctl -n hw.ncpu)"
ctest --test-dir "$build" --output-on-failure

python3 "$root/macos/package.py" --source "$root" --build "$build" \
  --output "$output" --revision "$revision" --lua "$lua" --lua-archive "$archive"
