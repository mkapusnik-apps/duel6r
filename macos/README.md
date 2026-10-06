# Experimental macOS local and network play

This package targets **Apple Silicon, macOS 14 or later**, using GL1. It is
experimental, with Local Play and the existing IPv4 network flows. Build/package checks do not establish successful
Finder launch, audio, controllers, graphics or gameplay. Manual Mac runtime and
visual verification are deferred until after merge and the subsequent nightly.
`Contents/Resources/build-info.json` identifies the exact source revision and
bundled dependency versions. Intel Macs are outside this target. Native automated
network acceptance is mandatory; manual GUI and live cross-OS verification remain
deferred, not passed, under `docs/macos.md` in the source repository.

## Install and launch

1. Download `duel6r-macos-arm64.zip` and its `.sha256` file from the same nightly.
   In the download directory, run `shasum -a 256 -c duel6r-macos-arm64.zip.sha256`.
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
and its MIT notice, and installed dependency notices, Homebrew recipes and receipts.
It also includes pinned static Mbed TLS 3.6.7 notices/configuration/library hashes
and private curl 8.21.0 license and source/build provenance. The private curl
uses OpenSSL with Apple SecTrust and no CA-file/path fallback. The required
server, guardian and resolver live in `Contents/MacOS`, not `Resources`; every
helper is checked for arm64/macOS 14 compatibility and sealed after relocation.
Non-system dynamic libraries are in `Contents/Frameworks`; Apple system frameworks
are supplied by macOS. The package includes the dynamic dependency closure and,
when Homebrew supplies sdl2-compat, its dynamically loaded SDL3 runtime.

The GLib 2.88.3 Sonoma bottle omits its upstream license files. The package
therefore carries checksum-verified upstream source and verbatim notices for
GLib, its gobject-introspection build resource, and its exact Homebrew patch.
The Homebrew BSD license accompanies the retained recipes and patch in GLib's
`sources/LICENSE.txt`.
The audited gettext 1.0 runtime LGPL notice is also absent from its keg; its
source archive and notices are included, together with libunistring 1.4.2 source.
These narrowly versioned supplements are bound to reviewed installed-recipe
hashes in `macos/license-sources.json`; a changed recipe/version fails closed.
Find the complete archives and patch under each dependency's `licenses/*/sources`,
extracted notices under `upstream`, and URLs/checksums in `source-provenance.json`.
The original recipe and installation receipt describe Homebrew build choices;
packaging changes library load paths and ad-hoc seals, not library source code.
No upstream attribution or license terms are replaced by generated summaries.
Notice discovery is not a blanket legal-compliance determination: the applicable
LGPL/source, modification and redistribution obligations continue to apply to
every bundled dependency. The collector audits the actual transitive closure
and reports all unresolved notice dependencies before refusing publication.

For libxmp 4.7.2, the complete main license in `README` and third-party notices
in `docs/CREDITS` are extracted verbatim from the checksum-verified release.
That notice-only archive is not shipped because it contains test music. The
separate Homebrew `demo_mods` resource is neither fetched nor redistributed.
See [the captured dependency audit](dependency-audit.md) for the reviewed hosted
recipe identities and full-closure replay evidence; this is not Mac runtime QA.

Bundled shared libraries may be replaced with compatible modified builds under
their respective licenses. Rebuilding from this revision is supported by
`bash macos/build.sh`; the recorded recipes identify dependency sources and build
options. After modifying Mach-O files, regenerate local ad-hoc integrity seals
with Apple's `codesign` tool. This is not trusted publisher signing. Nothing in
this package prohibits reverse engineering to debug modifications of libraries
where their licenses permit it.

## Native developer build

Only the macOS target has a native-execution policy exception. Other platforms
continue to use Docker. On an Apple Silicon Mac with Xcode command-line tools:

```sh
brew install cmake ninja pkg-config python@3.13 sdl2 sdl2_image sdl2_mixer sdl2_ttf glew openssl@3
D6R_SOURCE_REVISION="$(git rev-parse HEAD)" bash macos/build.sh
```

The helper requires a clean checkout, verifies and builds static Lua 5.3.6 from
the upstream SHA-256-pinned archive, builds the application, runs non-graphical
CTests, and packages it. Native packaged-helper checks then run against an
unchanged copy in a path containing spaces, from an unrelated working directory,
with isolated test homes. They exercise secure service data, resolver/guardian
lookup and cleanup, authoritative fixtures, and the actual bundled curl trust
path. They do not launch the graphical app or perform manual QA.
It writes the app, ZIP, checksum and metadata under `build/macos`, and CTest logs
under `build/macos-build/Testing/Temporary`. Dependency binaries must be arm64
and compatible with macOS 14.0; newer-only Homebrew bottles fail packaging rather
than silently raising the deployment floor. Use a compatible native build runner.
