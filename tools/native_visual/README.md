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

The tool copies the runtime per client. Default `data/persons.json`
NAME fixtures contain 1–15 unique names, each at most 64 UTF-8 bytes,
all counters zero, default Elo 1000, no completed rounds. No arbitrary stats,
levels, result snapshots, clock changes, debugger, preload or simulated network
messages are used. Optional pre-start profile/gameplay fixtures are bounded below.
Do not include secrets in recipes.

## Optional disclosed pre-start fixtures

An optional client `profile_fixtures` object maps new profile directory names to
ordinary built-in `skin.json` objects. Names must be safe single path components;
existing artifact profiles are never replaced. The controller writes only
`skin.json` and copies the verified shipped `sample/sounds.json`. It copies no
profile script or custom artwork. Native application parsing remains responsible
for skin field validity.

An optional recipe `enable_gun16: true` changes the one existing `gun 16 false`
setting to `gun 16 true` in each mutable client copy before application startup.
It never appends a duplicate setting. The default is unchanged.

The complete input runtime is still verified before copying, each copy is verified
before these disclosed data changes, and the immutable input remains verified
through operator batches and completion. No inventory entry or check is removed.
The active client data differs from the producer bundle only by the existing name
fixture and these explicitly requested profile/gameplay fixtures. Preserve the
recipe with its existing hash/context receipts alongside the original producer
identity. Freeze all data before starting a consumer; do not change it between
consumers of the same acquisition.

## Finite recipes

```sh
python3 -B -m tools.native_visual.controller --runtime /inputs/runtime \
  --identity /inputs/identity.json --recipe /inputs/recipe.json \
  --output /work/new-acquisition
```

Output must be a **new** directory outside the runtime. Recipes explicitly name
1–3 clients, their viewport and names, an explicit `budget_seconds` (ordinary600s,
approved long FS-N at most1200s), and a
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
(`clip`, `seconds`, optional `frames`), `stop_record` (`clip`), `exit_wait`. Each key/click has an
explicit positive `hold` up to 2s (default .08); release is attempted on errors.
The bound includes native delivery and release acknowledgements, not just a
sleep. A separate release guard runs even if a delivered press's response stalls.
Its release target is capped at 1.9s, reserving 100ms within the common 2s ceiling
for all releases; requested holds near 2s are therefore shortened. Press commands
share the release target deadline, and releases share the final deadline. Any
acknowledgement or release overrun fails the action and stops later steps; a
successful action requires confirmed releases inside that bound. This is not a
hard-real-time guarantee under an unresponsive OS/X server. Input uses XTEST only
on the controller's owned private display, so normal Escape-on-KeyPress may
destroy its window without preventing key release. Other release errors still fail.
Use separate key pulses, not an assumed repeat-key hold, for navigation.
For bounded chords use `modifiers:["Shift_L"]` (also Control_L / Alt_L).
Release requests are dispatched in reverse modifier order even after a failed
press/response; waits share one deadline and do not serialize delayed responses.
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
Omitting `frames` preserves the duration-only default exactly. For an explicitly
count-bounded acquisition, e.g. `{"action":"record","client":"guest","clip":"apn","seconds":90,"frames":1350}`,
`frames` must be a positive integer <=9000 and equal `seconds *15`; explicit null
is invalid. The recorder adds `-bf 0 -frames:v N`, disabling B-frame reordering
for this capped profile and setting its acquisition stop, retaining `-t`,
the80MB threshold, wall deadline and bounded drain. It does not drop/resample or
duplicate already recorded frames, rewrite timestamps, or guarantee coverage if
another limit stops acquisition first. Strict offline qualification remains
required. Receipts distinguish planned `frame_limit` (N or null when omitted)
and duration/nominal rate from actual encoder results, and include the actual
`acquisition_args` passed to FFmpeg. Historical receipts are
unchanged. Stop observations record the request time,
polled return code and whether SIGINT was requested. `stop_reason` is the first
SIGINT request reason, or null; it is not a claim about the actual termination
cause. A late request after encoder exit does not relabel its completion.
Capture acquisition and OS scheduling are not exact GPU-present timestamps.
Normal recipes must end their own applications; forced cleanup is reported as
failure, not a normal-exit pass. The outer existing process owner reaps all owned
descendants even after a worker exception or timeout. Preserve failed outputs.

## Optional operator continuation

`controller --operator` keeps the **same owned clients and one Scheduler.run**
alive after the initial recipe's `steps` (which may be empty). Default invocation
without this flag is unchanged. This is an explicit finite-input mailbox, not a
gameplay agent: only the operator chooses subsequent steps after inspecting the
real full-client PNG. It neither recognizes RNG outcomes nor invents a winning
route. There is no network service, process attachment, shell/signal action or
application-state API.

In an already approved private Docker worker, start the ordinary controller with
the additional `--operator` flag. Keep its runtime/identity/recipe and backend
configuration fixed. The existing outer process owner still supervises startup,
the single session and final cleanup. Ordinary profiles retain600s; only the
approved long FS-N recipe explicitly requests up to1200s, without automatic
extension or forced advancement. Single-clip recording and relay limits remain600s.
**The one requested scheduler deadline (absolute maximum1200s)
continues while the operator is absent; all expanded batches together, including
initial steps, have at most500 actions.** `at` remains relative to that original
start. Recordings remain live between batches and finalize once on overall exit.
The outer ownership watchdog retains the requested budget plus its existing90s
startup/cleanup allowance; no batch resets either budget.

`/work/acquisition/operator/ready.json` gives the context hash, actual owned
PID/window/display/geometry and absolute monotonic deadline. Use the same UID as
the controller; the mailbox directory is0700, outside the immutable flat runtime.
From separate authorized `docker exec` calls, invoke the data-only submitter:

```sh
# batch-1.json contains only the envelope below, not a context or executable code:
# {"steps":[{"client":"guest","action":"key","key":"Right","hold":0.12},
#           {"client":"guest","action":"capture"}]}
docker exec --user 1000:989 --workdir /workspace "$CAPTURE" \
  python3 -B -m tools.native_visual.operator /work/acquisition/operator \
  --context "$CONTEXT_FROM_READY" --batch 1 --request /work/batch-1.json
```

The submitter atomically publishes one new numbered JSON file without overwrite.
Wait for its receipt, inspect `operator/events.jsonl` and export the referenced
original PNG using Docker copy. Choose the next finite batch explicitly, using
batch2,3,…; recipe macros and state anchors remain the preselected ones. Every
expanded action is validated before any action in that batch is dispatched;
unknown fields/actions/clients, stale context, invalid macros, changed immutable
runtime or exceeding the global action cap abort the session, not a partial pass.
Requests are bounded to256KiB. Truncated/empty JSON at EOF is a failure. Waiting
for the next atomically complete file uses bounded sleeps, never a busy loop.

Action events are flushed/fsynced promptly with global ordinals and actual
timing, input-release observations and PNG paths. Batch receipts include current
encoder progress, not ffprobe or video hashing. Literal `text` is redacted from
the event journal; do not put credentials in recipes or request files. These are
private input files, not a secret-storage mechanism. A successful batch explicitly
does **not** claim overall normal cleanup. `--wait` bounds only the submitter's
receipt wait (default10s), never extends the worker deadline; if it expires, inspect
the existing receipts rather than automatically retrying or resubmitting.

End all clients through ordinary `key`/`exit_wait` steps, then submit the next
request containing only `{"finish":true}`. Finish is rejected unless every owned
client has completed a successful `exit_wait` and exited0. Only the later normal
`completion.json` can establish overall cleanup. `{"cancel":true}` aborts at the
next batch boundary; absence/deadline, invalid input and unexpected exit also
retain failure receipts and invoke existing owned teardown. A new invocation
creates new clients/context; it cannot resume this capture. If backend or capture
configuration changes, end the session instead of continuing under its context.

### Exact opaque relay integration

`transport_relay.py` is the DevOps-supplied infrastructure module, integrated
unchanged with SHA-256 `8327e98d429de5f8705fb9d7b447600626504251cb7a44d516111a46fecf9112`.
Its supplied smoke evidence covers bounded encrypted-byte forwarding, not an
actual application degraded/recovery state. Start it only under the separately
approved owned supervisor in the same private network-none namespace:

```sh
python3 -B -m tools.native_visual.transport_relay \
  --listen-port 28202 --target-port 28201 --delay-ms 25 --defer-delay --lifetime 600
```

Guest uses ordinary Direct Join127.0.0.1:28202; the real Host listens on28201.
For frozen8b, leave added delay at zero through normal admission **and the first
healthy in-world canonical presentation/clock calibration**, not merely the
connected/lobby label. The frozen `HeadlessServer.cpp` connection path classifies
the loopback target as SameMachine; `StateReplicationProtocol.cpp` defers the
initial snapshot until its bounded clock probe. The first calibration must
fit20ms, sustained degradation uses3s,
and the later probe deadline is250ms. After the operator confirms the ordinary
healthy world, the separate supervisor may explicitly SIGUSR1 **this owned relay**
once to activate25ms per direction. No payload detector or automatic activation
is present. Actual persistent degradation and complete updates still need proof.
Any separately approved bounded SIGSTOP/SIGCONT recovery condition targets only
that relay, never game/renderer processes or clocks. The module handles one TCP
connection, opaque bytes, bounded queues/lifetime and natural half-close drain;
it does not support automatic reconnect. Signals/deadlines are forced relay exits,
not successful application recovery. Its signals are not operator action types.

## Additive offline index and viewer

```sh
python3 -B -m tools.native_visual.media /evidence/original.mp4 /work/new-inspection \
  --sha256 ORIGINAL_SHA256 --candidate PRODUCER_SOURCE_SHA \
  --provenance ORIGINAL_RECEIPT_REFERENCE --frames 1350 --seconds 90 --rate 15
```

Use the actual application's producer SHA (for #180 captures:
`8b1e4a32fb70465df5c5828cb7701f7f35a80fed`), not the later documentation commit.
Mark unavailable historical receipt references explicitly; do not fabricate one.
Original files and receipts are never rewritten. New outputs use receipt version 1,
media index version 2 and tool-family ID `native-visual/1`, with independent
original and derivative hashes; they do not reconstruct historical receipts.

The requested duration/rate must agree with the expected decoded count. Full
decode, strict integer PTS ordering, actual frame durations, stable dimensions,
gaps (at most 1.5 nominal frame intervals) and actual last-frame end are checked.
Coverage tolerance is half a nominal frame plus one time-base tick, strictly
less than a full frame. It accepts 1350 valid 15fps frames ending at 89.999500s
for a 90s request, but never excuses a missing frame or a missing event chain.
Container duration alone cannot pass/fail an original. Files at or above 100 MiB
fail storage eligibility without altering them. A structural pass is **not** a
motion, effect-age, display-update-sampling or full visual acceptance claim.
New additive assessments report `decode_valid` separately from strict count,
cadence and boundary qualification. Full decode preserves the source timebase
and passthrough timestamps, avoiding default null-muxer output rounding errors.
When FFprobe omits frame and packet durations for a single unfragmented AVC MP4,
the validator can recover source sample lifetimes from complete positive STTS,
raw integer packet DTS deltas and the checked STTS sum. CTTS, packet identity,
counts and decoded presentation PTS must match exactly. Only no edit or one
unit-rate edit preserving every whole presentation sample with a constant
PTS/DTS shift is supported. A capped mdhd/stream duration, conflicting timing,
clipped/dropped samples or unsupported tables remain UNKNOWN, not guessed.
The source-table reader is deliberately bounded and is not a general demuxer.
Neither nominal FPS nor format.duration supplies a lifetime or coverage end;
composition reorder is resolved before taking the maximum presentation end.
These are declared media sample times, not GPU acquisition timestamps.
Missing unsupported actual durations mean unknown coverage (`boundary.valid`
is null), not automatic corruption. Unknown boundaries still fail `valid`;
successful decode never waives wrong counts, cadence gaps or coverage limits.

For explicitly permitted partial-reference **inspection only**, add
`--inspection-only` and use a new output directory. This can create an index for
a fully decodable original with safe, complete, strictly increasing source
ordinal/PTS mapping even when qualification fails. The requested contract and
original rejection diagnostics remain unchanged: `valid=false`,
`qualification_valid=false`, `inspection_available=true`. Missing/noninteger or
nonmonotonic PTS, inconsistent mapping counts, corruption and hash mismatches
still fail. CLI exit **0** means qualified indexing, **2** means nonqualifying
inspection is available (not verification success), and **1** means rejection
or an operation failure. The CLI also emits the operation/qualification flags
for an available index. Unknown frame lifetimes/coverage end remain null, never
inferred from nominal FPS. The viewer permanently shows NONQUALIFYING/UNKNOWN
status and rejection diagnostics; explicitly annotated cadence gaps are played
using actual PTS intervals without skips/interpolation or temporal acceptance.

For a slower full-frame PNG export, explicitly set `--export-timeout 600`
(seconds; default **180**, finite **>0 and <=1800**). It changes only the PNG
export deadline, not probe/full-decode budgets, requested capture coverage or
qualification rules. New indexes/assessments record `png_export_timeout_seconds`.
A timeout remains a failed operation with preserved partial output, never an
automatic retry or a usable inspection index; use a new output directory for
an explicitly authorized attempt with a different budget.

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
serves clearly synthetic good/missing/hash/decode/gap/slow-delivery and
nonqualifying `inspection-gap` cases. The genuine VFR `inspection-gap` now has
recoverable source lifetimes; separate `inspection-unknown` uses an explicitly
synthetic additional timecode track outside the supported timing shape. Its
UNKNOWN assessment is generated from that file, not faked in the recoverable
file's receipt. Use only an approved task-owned reachable
origin and stop its runtime after inspection. A Docker daemon host's loopback
is not necessarily OpenCode's loopback. Offline schema/static viewer tests do
not prove browser behavior or the actual assessor's access/UX acceptance.
