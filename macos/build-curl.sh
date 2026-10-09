#!/usr/bin/env bash
set -euo pipefail

sdk="${1:?macOS SDK required}"
deps="${2:?private dependency directory required}"
openssl="${3:?OpenSSL prefix required}"
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || exit 1
version=8.21.0
checksum=ad6f2f94934b38e31e48272833c99b891d045b4565fe942a53fbd27bd3910e16
url="https://curl.se/download/curl-$version.tar.bz2"
archive="$deps/curl-$version.tar.bz2"
source="$deps/curl-$version"
build="$deps/curl-build"
prefix="$deps/curl"
mkdir -p "$deps"
if [[ ! -f "$archive" ]]; then
  /usr/bin/curl --fail --location --retry 3 --proto '=https' --proto-redir '=https' --tlsv1.2 \
    "$url" -o "$archive.part"
  mv "$archive.part" "$archive"
fi
printf '%s  %s\n' "$checksum" "$archive" | shasum -a 256 -c -
# Never reuse an unverified/mutated source tree or stale configure cache.
rm -rf "$source" "$build" "$prefix"
tar -xjf "$archive" -C "$deps"
mkdir -p "$build"
flags="-arch arm64 -mmacosx-version-min=14.0"
(
  cd "$build"
  # xcrun selects the SDK for compiler/configure probes as well as make.
  # Do not embed shell quotes in CFLAGS: configure expands them as data.
  SDKROOT="$sdk" CC="xcrun --sdk macosx clang" CFLAGS="$flags" LDFLAGS="$flags" \
    PKG_CONFIG_PATH="$openssl/lib/pkgconfig" "$source/configure" \
    --prefix="$prefix" --enable-shared --disable-static \
    --with-openssl="$openssl" --with-apple-sectrust \
    --without-ca-bundle --without-ca-path --without-ca-fallback --enable-threaded-resolver \
    --without-libpsl --without-libssh2 --without-libidn2 \
    --without-nghttp2 --without-nghttp3 --without-ngtcp2 --without-brotli --without-zstd \
    --disable-ldap --disable-ldaps
  make -j"$(sysctl -n hw.ncpu)"
  make install
)
python3 - "$build/lib/curl_config.h" "$prefix" "$version" "$checksum" <<'PY'
import hashlib, json, pathlib, re, subprocess, sys
config, prefix, version, checksum = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), *sys.argv[3:]
defines = set(re.findall(r'^\s*#define\s+(\w+)\b', config.read_text(), re.M))
required = {'USE_APPLE_SECTRUST', 'USE_OPENSSL', 'HAVE_THREADS_POSIX', 'USE_RESOLV_THREADED'}
forbidden = {'CURL_CA_FALLBACK', 'CURL_CA_BUNDLE', 'CURL_CA_PATH'}
if not required <= defines or forbidden & defines:
    raise SystemExit('Private curl native trust/resolver configuration is invalid')
def inspect(option):
    return subprocess.check_output([str(prefix / 'bin/curl-config'), option], text=True).strip()
features = inspect('--features').splitlines()
if not {'AsynchDNS', 'SSL'} <= set(features) or inspect('--ssl-backends') != 'OpenSSL':
    raise SystemExit('Private curl runtime features/backend are invalid')
library = (prefix / 'lib/libcurl.4.dylib').resolve(strict=True)
metadata = dict(version=version, archive_sha256=checksum, architecture='arm64', minimum_macos='14.0',
                native_trust='Apple SecTrust', ca_fallback=False, configure=inspect('--configure'),
                features=features, library_sha256=hashlib.sha256(library.read_bytes()).hexdigest())
(prefix / 'build-info.json').write_text(json.dumps(metadata, indent=2) + '\n')
print('Private curl built: version=' + version + '; native SecTrust; no CA fallback; threaded resolver')
PY
