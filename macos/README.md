# Experimental macOS local play

This package targets **Apple Silicon, macOS 14 or later**, using GL1. It is
local-only and experimental. Build/package checks do not establish successful
Finder launch, audio, controllers, graphics or gameplay. Manual Mac runtime and
visual verification are deferred until after merge and the subsequent nightly.
`Contents/Resources/build-info.json` identifies the exact source revision and
bundled dependency versions. Intel Macs and network play are not supported.

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

Use **Play (F1)** for local play. **Network (F2)** explains the local-only limit;
any keyboard key dismisses that explanation without activating its usual action.

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
Non-system dynamic libraries are in `Contents/Frameworks`; Apple system frameworks
are supplied by macOS. The package includes the dynamic dependency closure and,
when Homebrew supplies sdl2-compat, its dynamically loaded SDL3 runtime.

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
brew install cmake ninja pkg-config python@3.13 sdl2 sdl2_image sdl2_mixer sdl2_ttf glew
D6R_SOURCE_REVISION="$(git rev-parse HEAD)" bash macos/build.sh
```

The helper requires a clean checkout, verifies and builds static Lua 5.3.6 from
the upstream SHA-256-pinned archive, builds the application, runs non-graphical
CTests, and packages it. It does not launch the app or perform manual QA.
It writes the app, ZIP, checksum and metadata under `build/macos`, and CTest logs
under `build/macos-build/Testing/Temporary`. Dependency binaries must be arm64
and compatible with macOS 14.0; newer-only Homebrew bottles fail packaging rather
than silently raising the deployment floor. Use a compatible native build runner.
