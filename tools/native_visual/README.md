# Bounded native visual tools

These developer tools reuse the native-input / pixel-anchor approach of
`tests/SharedArenaBehaviorTests.sh`, `MenuTransferFrame.py` and
`PackageDeploymentBehaviorTests.py`, and the existing `OwnedProcessRunner.py`.
PR [#180](https://github.com/mkapusnik-apps/duel6r/pull/180), source
`cb54f21d30ac3732775652a57020b920bbefdde9`, is the shared-arena reference.
These are **new generalized helpers**, not recovered historical controller,
viewer or receipt bytes. They do not change the game or certify conformance.

## Environment and immutable inputs

Run from the repository root **inside the approved Linux Docker runtime**.
Use the repository Docker API workspace-transfer procedure; a daemon bind mount
must not be assumed. Python 3, Xvfb, xdotool, ImageMagick and the application's
runtime libraries are required. Recording / indexing additionally need FFmpeg
with x11grab, libx264rgb and ffprobe. No Python package or web framework is needed.
Use a disposable container with `--network none` for private loopback Host/Join,
no host X display, no privileged mode, no shared application data or credentials.
The tool never builds or changes its runtime input.

Provide a flat Release runtime and a separately supplied identity JSON:

```json
{"source_revision":"<40 lowercase hex>","application_sha256":"<64 lowercase hex>",
 "files":{"duel6r":"<sha256>","data/config.script":"<sha256>","…":"…"}}
```

`files` must enumerate **every file** (relative POSIX paths), including the
application's sibling executables and assets. No symlinks, unexpected files,
missing files or changed hashes are accepted. Keep the identity file outside
the flat runtime. Supply identities from the artifact producer, not a version
banner or a guessed source revision. A rebuild has a new artifact identity.

The tool copies the runtime per client. Only supported `data/persons.json`
NAME fixtures are written: 1–15 unique names, each at most 64 UTF-8 bytes,
all counters zero, default Elo 1000, no completed rounds. No arbitrary stats,
profiles, levels, gameplay scripts, result snapshots, clock changes, debugger,
preload or simulated network messages are used. Do not include secrets in recipes.

## Finite recipes

```sh
python3 -B -m tools.native_visual.controller --runtime /inputs/runtime \
  --identity /inputs/identity.json --recipe /inputs/recipe.json \
  --output /work/new-acquisition
```

Output must be a **new** directory outside the runtime. Recipes explicitly name
1–3 clients, their viewport and names, a budget (at most 600 seconds), and a
finite list of at most 500 actions. Setup macros expand finite lists only;
cycles, unknown macros and oversized expansion fail. Example inactive setup:

```json
{
  "clients":[{"id":"guest","geometry":[1280,720],"names":["Guest"]}],
  "budget_seconds":20,
  "macros":{"open-network":[
    {"client":"guest","action":"await","state":"main-menu"},
    {"client":"guest","action":"key","key":"F2"},
    {"client":"guest","action":"await","state":"network-entry"}
  ]},
  "steps":[{"macro":"open-network"},
    {"client":"guest","action":"key","key":"Escape"},
    {"client":"guest","action":"await","state":"main-menu"},
    {"client":"guest","action":"key","key":"Escape"},
    {"client":"guest","action":"exit_wait"}]
}
```

Actions: `key` (one keysym), `click` (`point:[x,y]`), `text` (literal), `wait`
(`seconds`), `await` (`state`, timeout at most 10s), `capture`, `record`
(`clip`, `seconds`), `stop_record` (`clip`), `exit_wait`. Each key/click has an
explicit positive `hold` up to 2s (default .08); release is attempted on errors.
Use separate key pulses, not an assumed repeat-key hold, for navigation.
For bounded chords use `modifiers:["Shift_L"]` (also Control_L / Alt_L); all
modifiers are released in reverse order even after a failed press/response.
`at` schedules an action at seconds from the local recipe start, with a bounded
`late_tolerance` (default .1s). Missed deadlines fail; they are not silently shifted.
This is a finite operator-selected sequence, **not a gameplay agent**. Choose
ordinary input routes for the approved scenario; do not invent a winning result.

Menu points are bottom-left **logical 850×700** coordinates. `space:"menu"`
uses `Menu.cpp::initializePresentation`: float32
`min(1.35,w/850,h/700)`, then integer centered origin. `space:"network-menu"`
matches NetworkMenu's truncation of the scaled extent before centering. These
origins differ by one pixel at 720p / 900p. `space:"client"` uses top-left
unscaled full-client pixels, including the shared final shell. Never resize a
Debug window or use menu coordinates for world positions.

The four supported full-client/display geometries are 850×700, 1280×720,
1280×900 and 1920×1080. Built-in `main-menu` / `network-entry` states use multiple
positive and negative pixel anchors, including the popup exclusion. They are
route guards, not recognition of an outcome, bonus or a visual pass. Custom
`states` map names to anchor lists (`box:[x,y,width,height]`, bottom-left menu
coordinates; `rgb`, optional tolerance 0–32, `minimum`/`maximum` fractions,
optional `network:true`). Built-ins cannot be overridden. Unknown/stale states
must stop the workflow. Qualify new scenario anchors in native preflight before
using them for accepted evidence. Synthetic regression frames prove only the
helper's logic, not game rendering or reachability.

`record` runs full-display x11grab independently (15 fps, libx264rgb CRF18,
veryfast, no audio, passthrough timestamps). It stops conservatively at 80 MB
or its bounded duration; mux drain and final size must still pass. No crop,
resize, trimming, composition, re-encoding or silent frame downsampling is used.
`stop_record` requests a stop without a synchronous ffprobe roundtrip. Encoder
drain, hashing and offline probes are deferred until outside the input schedule.
Capture acquisition and OS scheduling are not exact GPU-present timestamps.
Normal recipes must end their own applications; forced cleanup is reported as
failure, not a normal-exit pass. The outer existing process owner reaps all owned
descendants even after a worker exception or timeout. Preserve failed outputs.

## Additive offline index and viewer

```sh
python3 -B -m tools.native_visual.media /evidence/original.mp4 /work/new-inspection \
  --sha256 ORIGINAL_SHA256 --candidate PRODUCER_SOURCE_SHA \
  --provenance ORIGINAL_RECEIPT_REFERENCE --frames 1350 --seconds 90 --rate 15
```

Use the actual application's producer SHA (for #180 captures:
`8b1e4a32fb70465df5c5828cb7701f7f35a80fed`), not the later documentation commit.
Mark unavailable historical receipt references explicitly; do not fabricate one.
Original files and receipts are never rewritten. Outputs use receipt version 1
and tool ID `native-visual/1`, with independent original and derivative hashes.

The requested duration/rate must agree with the expected decoded count. Full
decode, strict integer PTS ordering, actual frame durations, stable dimensions,
gaps (at most 1.5 nominal frame intervals) and actual last-frame end are checked.
Coverage tolerance is half a nominal frame plus one time-base tick, strictly
less than a full frame. It accepts 1350 valid 15fps frames ending at 89.999500s
for a 90s request, but never excuses a missing frame or a missing event chain.
Container duration alone cannot pass/fail an original. Files at or above 100 MiB
fail storage eligibility without altering them. A structural pass is **not** a
motion, effect-age, display-update-sampling or full visual acceptance claim.

Serve the new inspection directory read-only through an approved local HTTP
runtime and open `viewer.html` (secure context / localhost for SHA-256 checking).
The viewer shows one complete full-frame PNG decode, no overlay controls,
zero-based ordinal, exact source PTS ticks/time base, provenance and hashes.
Previous/Next never wrap. Playback waits for every frame and its PTS interval,
disclosing load stalls rather than skipping/interpolating to catch up. Arrow
keys / Space work with the frame panel focused; buttons have native keyboard
focus. Native-pixel scroll is the default; optional complete-frame fit discloses its
actual scale and does not alter original files. Missing, corrupted or mismapped
frames stop playback explicitly. PNG decodes are labeled inspection derivatives,
not original lossless native screenshots. Access for the actual assessor must
be approved separately; developer browser access does not grant UX permissions.

Run routine tests inside Docker:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest discover -s tests -p 'NativeVisual*Tests.py' -v
```

The existing Linux CTest registration includes helper tests; media tests are
registered when FFmpeg/ffprobe are available. No game preflight, new capture
campaign or conformance assessment is authorized merely by running these tests.

For permitted developer browser checks, `tests/NativeVisualViewerFixture.py`
serves clearly synthetic good/missing/hash/decode/gap/slow-delivery cases. Use
only a task-owned Docker container and a loopback-published test port; stop it
after inspection. This checks the tool, not the actual assessor's access or UX gate.
