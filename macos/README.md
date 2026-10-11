# Experimental macOS local and network play

These architecture-specific packages target **Apple Silicon on macOS 14 or later,
or Intel x86_64 on macOS 13 Ventura or later**, using GL1. Intel Core i5 Macs must support macOS 13;
not every historical Core i5 Mac qualifies. Support is
experimental, with Local Play and the existing IPv4 network flows. Build/package checks do not establish successful
Finder launch, audio, controllers, graphics or gameplay. Manual Mac runtime and
visual verification are deferred until after merge and the subsequent nightly.
`Contents/Resources/build-info.json` identifies the exact source revision and
target architecture and bundled dependency versions. Native automated
network acceptance is mandatory on each architecture after integration in Develop - Sanity,
not before review or merge; manual GUI and live cross-OS verification remain
deferred, not passed, under `docs/macos.md` in the source repository.

## Install and launch

1. Download `duel6r-macos-arm64.zip` for Apple Silicon or
   `duel6r-macos-x86_64.zip` for Intel, and its `.sha256` file from the same nightly.
   In the download directory, run `shasum -a 256 -c duel6r-macos-arm64.zip.sha256`
   or `shasum -a 256 -c duel6r-macos-x86_64.zip.sha256`, respectively.
2. Extract the ZIP and drag **Duel 6 Reloaded.app** to Applications (or a folder
   you own). Keep the application bundle intact; no Homebrew installation is
   required to play. Launch it in Finder, without setting a working directory.
3. This application has **no Developer ID signature or notarization**. It uses
   only local ad-hoc integrity seals required for Apple Silicon executables.
   macOS may block it as an unidentified developer. If you trust the source and
   have verified the download, attempt to open it, then use **System Settings →
   Privacy & Security → Open Anyway**, and confirm the macOS prompt. On versions
   that offer it, Finder's Control-click → Open also provides an approval prompt.
   Do not disable Gatekeeper or other system-wide protections. If managed-device
   policy prevents approval, ask the administrator rather than bypassing policy.

Use **Play (F1)** for Local Play. **Network (F2)** opens Host, Browse sessions,
and Direct connect. Use the same supported release and gameplay content on all
participants. Gameplay remains IPv4, LAN-first, with eligible assigned unicast
addresses and no automatic NAT/firewall changes or public reachability promise.
Secure capability failure disables network startup without disabling Local Play.
Network results never become Local Play person statistics.

Unchannelled/PR builds have no directory origin by default. Nightly builds use
`https://staging.duel.netusite.cz`; release-channel builds use
`https://duel.netusite.cz`. A present `D6R_DIRECTORY_URL` overrides the compiled
origin; empty/invalid values disable the directory without fallback. Directory
failure does not prevent direct joining, hosting, or Local Play.

## Saved data and replacement

Person records, roster names, statistics and played-round count are saved at:

```text
~/Library/Application Support/Duel 6 Reloaded/data/persons.json
```

Quit before backing up that file. To update, quit and replace the complete `.app`
with the next nightly. Replacing the app does not replace user data. Do not copy
saves into the app or assume cross-platform save migration is supported. Resource
files remain inside `Contents/Resources`; saves never require bundle write access.

## Dependencies and licenses

`Contents/Resources/licenses` contains the game's BSD license, pinned Lua source
and its MIT notice, and verbatim notices and build provenance for the private runtime.
It also includes pinned static Mbed TLS 3.6.7 notices/configuration/library hashes
and private curl 8.21.0 license and source/build provenance. The private curl
uses OpenSSL with Apple SecTrust and no CA-file/path fallback. The required
server, guardian and resolver live in `Contents/MacOS`, not `Resources`; every
helper is checked for the selected native architecture and its minimum macOS version
and sealed after relocation.
Non-system dynamic libraries are in `Contents/Frameworks`; Apple system frameworks
are supplied by macOS. The package includes the dynamic dependency closure and
the pinned sdl2-compat layer's dynamically loaded SDL3 runtime.

`macos/runtime-sources.json` pins the complete private runtime source inputs and
the GLEW extension patch by SHA-256. It retains the audited SDL3 3.4.14,
sdl2-compat 2.32.70, SDL2_image 2.8.12, SDL2_mixer 2.8.2, SDL2_ttf 2.24.0 and
GLEW 2.3.1 versions. Directory HTTPS uses source-built OpenSSL 3.5.9 LTS;
gameplay still uses the separate pinned hardware-only Mbed TLS profile.

The SDL image/mixer projects use their upstream vendored codec defaults rather
than Homebrew's optional codec graph. Missing submodule sources are pinned to
the exact upstream release gitlinks and independently checksum-verified; no
mutable submodule fetch runs during a build. Codec sources are compiled into
the SDL libraries (any shared outputs are also inventoried). Image loading
retains PNG/JPEG and the upstream AVIF/TIFF/WebP backends; audio retains
WAVE, Vorbis, MP3, FLAC, MOD, Opus, WavPack and native/timidity MIDI.
FreeType and HarfBuzz come from SDL2_ttf's verified release archive, preserving
text shaping. No application SDL2 API or renderer migration is involved.

`licenses/runtime` contains the source lock, build recipe, configure commands,
source and library identities, required notices for all projects and vendored
code, and the applied GLEW patch. FreeType is distributed under its FreeType
License with its copyright notices; HarfBuzz and embedded codec notices are
included verbatim. libxmp's `README`, `docs/CREDITS` and `docs/COPYING` are
retained, but its test music and source archive are not shipped. Missing notices,
changed recipe/source checksums, unexpected dependency origins or altered
library bytes fail packaging. No notices are replaced by generated summaries.

The previous [Homebrew dependency audit](dependency-audit.md) and
`license-sources.json` remain historical inputs for the existing bottle-notice
regressions, not evidence for this source-built closure or native Mac runtime QA.

Bundled shared libraries may be replaced with compatible modified builds under
their respective licenses. Rebuilding from this revision is supported by
`bash macos/prepare-runtime-dependencies.sh` followed by `bash macos/build.sh`;
the recorded recipes identify dependency sources and build
options. After modifying Mach-O files, regenerate local ad-hoc integrity seals
with Apple's `codesign` tool. This is not trusted publisher signing. Nothing in
this package prohibits reverse engineering to debug modifications of libraries
where their licenses permit it.

## Native developer build

Only the macOS target has a native-execution policy exception. Other platforms
continue to use Docker. On a matching native Mac with Xcode command-line tools:

```sh
brew install cmake ninja pkg-config python@3.13
# Build the private runtime from checksum-pinned sources for this architecture:
bash macos/prepare-runtime-dependencies.sh
D6R_SOURCE_REVISION="$(git rev-parse HEAD)" bash macos/build.sh
```

Architecture is detected natively, or asserted with `D6R_MACOS_ARCH=arm64` or
`D6R_MACOS_ARCH=x86_64`. Cross-architecture and Rosetta builds are rejected.
The dependency preparation helper builds into `build/macos-deps/runtime`, with
macOS 14.0 for ARM or macOS 13.0 for Intel. It does not depend on continued
availability of Sonoma/Ventura bottles and never modifies installed Homebrew
kegs. Source downloads, patches and required notices are checked before building;
only task-owned runtime output is replaced. Build-only tools may use the host's
bottles. Packaging independently checks every bundled Mach-O's deployment
target, architecture, dependency closure and license provenance. A build on a
newer host is not evidence of execution on the minimum OS; native automated and
deferred user verification retain their separate acceptance boundaries.

The helper requires a clean checkout, verifies and builds static Lua 5.3.6 from
the upstream SHA-256-pinned archive, builds the application, runs non-graphical
CTests, and packages it. Native packaged-helper checks then run against an
unchanged copy in a path containing spaces, from an unrelated working directory,
with isolated test homes. They exercise secure service data, resolver/guardian
lookup and cleanup, authoritative fixtures, and the actual bundled curl trust
path. They do not launch the graphical app or perform manual QA.
It writes the app, ZIP, checksum and metadata under `build/macos`, and CTest logs
under `build/macos-build/Testing/Temporary`. Use separate clean build workspaces
for each architecture. The archive is `duel6r-macos-<architecture>.zip`; ARM keeps
the published `build-info.json`, and Intel additionally produces
`build-info-x86_64.json` for collision-free publication. Each bundle retains its
own `Contents/Resources/build-info.json`. Dependency binaries must match the
selected architecture and support macOS 14.0 on ARM or macOS 13.0 on Intel; newer-only dependencies fail
packaging rather than silently raising the deployment floor. Native Mac build,
package and automatic network checks remain mandatory post-integration;
Linux checks do not establish native Mac execution.
