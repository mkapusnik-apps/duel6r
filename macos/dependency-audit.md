# macOS dependency-notice replay

This is developer-run packaging evidence, not native runtime QA, a complete
legal-compliance determination, or candidate acceptance.

## Authoritative input

- Diagnostic run: [37123730851](https://github.com/mkapusnik-apps/duel6r/actions/runs/37123730851), attempt 1.
- Application source: `359f08b67f69efbad3abf28d25172b57f8103fcc`.
- Temporary workflow: `cf392faa04c9bb1e2646462c4fb5d9f9037c4e30`.
- `snapshot.tar.gz` SHA-256: `31fe8ad3f67f70318e8491a75714222ec84ff702bbd08a954f5b64e5671611cf`.
- Hosted environment: arm64 macOS 14.8.9, image `20260831.0302.1`, Xcode 15.4,
  macOS 14.5 SDK, Homebrew 6.0.20, CMake 4.4.3, Python 3.14.7.
- Captured closure: 70 paths representing 40 kegs. The original build exit code
  was 1; capture did not convert that failure into acceptance.

## Audited corrections

GLib 2.88.3's captured recipe hash is
`0105d4552c1cc68b38cdcd23c4df05c2cb0d93ce44929f581204a6d33dd16d59`.
Reversing just the added `deny_network_access!` and the post-install change from
`mkdir_p "lib/gio/modules", base: :homebrew_prefix` to
`mkdir_p "{{HOMEBREW_PREFIX}}/lib/gio/modules"` reconstructs the previously audited
recipe hash `d2ebed597d64ab5518ea78604840e28fb9839fa923bd3917bef5f50b76d369be`.
Source/archive checksums, resources and library build instructions are unchanged.
The patch at the captured tap revision
`ae26c0a85ae0292ac247463040c812e4ab922f9e` also matches the retained patch SHA-256
`d846efd0bf62918350da94f850db33b0f8727fece9bfaf8164566e3094e80c97`.
Both specifically reviewed recipe hashes are accepted; arbitrary drift is not.

libxmp 4.7.2's captured recipe hash is
`27d5a9516c44e8345bdb238c43e4a24f579431fe5b668b4dcc92f3669504f116`.
Its recipe and SBOM identify the release archive pinned in `license-sources.json`:
`510a96eefd79e4558fb1fa41fb5494870328776b3f77563f94f61f241f64bde1`.
The fetched archive verified against that checksum. Required extracted notices:

| File | SHA-256 |
| --- | --- |
| `libxmp-4.7.2/README` | `a5d9bc35899cd1d1ab906649dd779225dd37e70e555443b6d84228b62d73bdc4` |
| `libxmp-4.7.2/docs/CREDITS` | `3f12c572c2918dd1674c1025b2b9ac69967fedbebe4ffd87adf0718249d797d0` |

The first matches the installed README; CREDITS was missing from the keg.
Both are preserved verbatim, including third-party terms. The archive contains
`test/test.it` and `test/test.xm`, so it is a notice-only input, not a distributed
source archive. Neither those files nor the separate Homebrew `demo_mods`
resource are distributed. Existing LGPL source/patch supplements remain intact.

## Replay and scope

The archive digest and all 255 selected recipe, receipt, SBOM, notice and cached
source files were checked against the snapshot inventory. Regular and hardlinked
payloads were materialized without executing captured binaries. The unchanged
collector first reproduced exactly the GLib recipe and libxmp notice failures.
After the corrections, the production `collect_notices` function passed for
**40/40 captured kegs** in Docker image
`sha256:604c41dbb788156bb580ae4eee88c7a00d6e06e9facf9cadccc1ba705e00d57e`.
Missing pinned sources were fetched and checksum-verified into the replay cache
with Python because this Linux image lacks curl. This verifies collection from
those bytes, not the native curl download path or native signing/archive steps.

The checked keg set was:

```text
aom/3.14.1 brotli/1.2.0 dav1d/1.5.4 flac/1.5.0 fluid-synth/2.6.0
freetype/2.14.3 game-music-emu/0.6.5 gettext/1.0 glew/2.3.1 glib/2.88.3
graphite2/1.3.15 harfbuzz/14.4.0 highway/1.4.0 jpeg-turbo/3.2.0 jpeg-xl/0.12.0
lame/4.0 libavif/1.4.2 libogg/1.3.6 libpng/1.6.58 libsndfile/1.2.2_1
libtiff/4.7.2 libvmaf/3.2.0 libvorbis/1.3.7 libxmp/4.7.2 little-cms2/2.19.1
mpg123/1.33.7 opus/1.6.1 opusfile/0.12_1 pcre2/10.47_1 portaudio/19.7.0
readline/8.3.3 sdl2-compat/2.32.70 sdl2_image/2.8.12_1 sdl2_mixer/2.8.2
sdl2_ttf/2.24.0 sdl3/3.4.14 wavpack/5.9.0 webp/1.6.0 xz/5.8.3 zstd/1.5.7_1
```

The 10 packaging tests in `tests/MacPackagingTests.py` also passed. They retain
checksum, recipe-drift, missing-notice, traversal and aggregate-failure checks,
and verify notice-only extraction excludes test music and the source archive.
The replay separately checked actual libxmp output notice hashes, exclusion of
music/archive payloads, and retained source/patch hashes for applicable LGPL
packets. libunistring is not in this actual linked closure; its existing packet
is unchanged and was not treated as an extra runtime dependency.

The temporary workflow, local snapshot and replay workspace are removed after
this evidence is recorded. Upstream application compilation, tests and retained
Linux application evidence are unaffected; a new native package is still needed.
