# Gameplay rendering performance review

## Scope and evidence

Reviewed the local game loop, shared arena renderer, canonical network presenter,
sprite/text rendering, level geometry, and renderer backends. Source baseline:
`86d025e5b3c5be92df73e2307c05f83f0a76b3de`; the results below were reported with the
accompanying batching changes before PR preparation. The PR is based on develop
at `dd2e5dcc8b2b1177323ba650a8f458c029b4967f`. This is source analysis plus routine
local Docker verification, not a hardware-wide GPU profile or hosted CI result.
The original temporary logs were unavailable during PR preparation, so these
historical results are not independently provenance-verified checkpoint evidence.

## Implemented: batch player-effect primitives

`WorldRenderer::youAreHere` previously submitted 36 separate lines per player;
`WorldRenderer::invulRing` submitted 24 separate points. Each GL4 submission bound
the program and VAO, replaced a four-vertex buffer, looked up and uploaded a color
uniform, set point/line size, and issued a draw. During the overlapping arrival
and spawn-protection effects, 15 players therefore required **900 submissions per
frame for these effects alone**.

The renderer now accepts immediate, ordered point and independent-line batches:

- GL4 uploads the batch once and calls `glDrawArrays` once.
- GL1 uses one `glBegin`/`glEnd` pair per batch.
- Other renderers inherit the original scalar operation sequence.
- Local arrival and protection effects each use one batch per player: **900 to
  30 submissions (96.7% fewer)** in the above scenario. Protection bonuses also
  benefit after the round-start effects expire.
- The canonical network presenter batches its 15-point arrival and 24-point
  protection rings. With both effects on all 15 players, their submissions fall
  from 585 to 30.
- Vertex coordinates, primitive order, sizes, colors, depth/blend state and
  effect timing are preserved. Batches stay within each existing effect rather
  than sorting transparent objects or moving them across other draws.
- Small fixed-size stack arrays avoid new per-frame heap allocations. No new
  shader, extension, instancing, persistent mapping or GPU feature is required.

### Isolated timing

Environment: Linux Docker image `duel6r-build:local`, image ID
`604c41dbb788156bb580ae4eee88c7a00d6e06e9facf9cadccc1ba705e00d57e`, GCC 13.3 Release,
Xvfb, Mesa `llvmpipe (LLVM 20.1.2, 256 bits)`, 320×240 framebuffer.

Temporary timing instrumentation rendered the 15-player effect geometry from
`tests/RendererBatchTests.cpp`. Each path was warmed up, then measured in five
100-frame runs with alternating measurement order and `glFinish` at run boundaries.
The table gives the median time per frame, including completion, clear and geometry
generation. Scalar and batched paths used the same final renderer implementation;
this isolates submission granularity rather than comparing two game executables.
The temporary timing instrumentation was removed after measurement.

| Backend | Scalar submissions | Batched submissions | Reduction in measured time |
| --- | ---: | ---: | ---: |
| GL4 | 1.440 ms/frame | 0.628 ms/frame | 56% |
| GL1 | 1.182 ms/frame | 0.503 ms/frame | 57% |

These are **effect-only software-renderer timings**, not whole-game FPS gains.
Physical Intel/AMD/NVIDIA hardware and driver overhead can differ substantially.
The stable benefit is the reduced submission/upload count, particularly at round
start and with many simultaneously protected players.

## Other findings, in suggested investigation order

| Area | Evidence in source | Next optimization to measure |
| --- | --- | --- |
| Shader uniform lookup | `GL4Program::uniformLocation` and `GLES3Program::uniformLocation` call `glGetUniformLocation` on every uniform update. | Resolve fixed uniforms once per linked program, including inactive locations. Preserve the explicit program binds used for Intel compatibility. |
| Sprite submissions | `Sprite::render` submits one quad per sprite; GL4 replaces its tiny material VBO on every quad. | Batch adjacent compatible sprites with streamed vertices. Flush at material, matrix, depth, blend, render-target and viewport boundaries; retain transparent ordering. Existing texture arrays already support varying animation layers. |
| Font cache hits | `FontCache::get` copies an entry, erases a list node and allocates another on every hit. The cache holds only 100 full-string textures. | Use list splicing for LRU hits; measure cache misses with the 15-player HUD and score table before changing capacity. Consider a glyph atlas only if changing text causes sustained rasterization/upload churn. |
| Level animation uploads | Every animation tick, `FaceList::nextFrame` advances all faces and uploads every texture index, including one-frame wall textures. GL4 builds a temporary index vector each time. | Track actual index changes/dirty ranges and reuse staging memory. Account for hidden/burned faces and regenerated water when designing invalidation. |
| Network presentation CPU work | `CanonicalWorldPresenter::loadRound` builds two string sets before its unchanged-round early return; `update` rebuilds an entity map and several presentation paths perform linear searches. | Measure update cost under populated replicated worlds; cache validation against explicit session/settings revisions and index entities if warranted. |
| High-resolution bandwidth | Local `sharedArena` clears buffers and copies cached color/depth every frame. | Profile at 1080p and 4K on an integrated GPU. Verify full-target coverage and fade/depth behavior before removing clears or changing the cache strategy. |

Static geometry is already sensibly amortized: `LevelRenderData` constructs wall,
decoration and water buffers; local `WorldRenderer::prerender` records the static
background when requested by round preparation, rather than redrawing every tile
each frame. The canonical presenter draws the cached geometry buffers directly.
Whole-level visibility also limits the usefulness of conventional camera culling.

Outside rendering, `Application::syncUpdateAndRender` has an uncapped fixed-step
catch-up loop: a long stall can lead to many updates before the next frame.
`LegacyShot::checkShotCollision` scans other shots, allowing quadratic work when
shot-to-shot collision is active. Profile simulation separately from rendering
before changing either; dropping ticks or changing collision traversal order can
change gameplay and authoritative determinism.

## Hardware compatibility

The desktop `gl4` backend currently requests OpenGL 4.3; `gl1` requests an OpenGL
2.0 compatibility context. The optimization raises neither requirement and improves
both paths. A future OpenGL 3.3 desktop path could broaden reach, but would require
auditing shader versions and program-uniform APIs, not merely lowering the context
version. Advanced GPU-only batching is unnecessary for these small 2D workloads.

The configured ES backends should not be treated as verified hardware fallbacks:
the ES3 build attempted during this review failed in unchanged `Video.cpp` because
`glewInit`, `GLEW_OK` and `glewGetErrorString` are undeclared. The ES2 header also
contains stale renderer-interface declarations; ES2 was not built or run. Their
source receives only the shared scalar batch fallback in this change.

## Verification

- GL4 and GL1 Release game builds passed in Docker.
- New `renderer-batch-behavior` CTest passed on both backends. It compares exact
  scalar/batched framebuffer pixels for 15-player effects, depth testing, alpha
  overlap, transforms/clipping, changed point/line sizes, odd line vertex counts,
  empty/negative counts, and transitions to textured/colored primitives. It also
  requires non-empty visible output and checks GL errors.
- Existing GL4 `duel6r-tests` passed all 15 cases.
- Existing GL4 `shared-arena-behavior` passed Deathmatch, Predator and all Team
  configurations through 15 players, including ranking, score table and menu return.
- Existing GL4 `duel6r-network-session-runtime-tests` passed.
- The final pixel tests were rerun after removing temporary timing instrumentation.
  Production GL4/GL1 code was unchanged between timing and final verification.
- ES3 build verification was blocked as described above. Physical GPU testing and
  a full unrelated networking/security suite were not performed.

The new regression is registered with CTest for Linux GL4/GL1 builds and therefore
runs in the existing Docker test workflow. No timing threshold is used in CI.
