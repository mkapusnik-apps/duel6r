# PR #87 durable capture provenance

The [authoritative screenshot manifest](../../../../screenshots/README.md#network-arena-orientation--current-coverage) owns the accepted representatives, hashes, reproduction inputs, and visual assessment. This directory is a provenance integration index, not another screenshot matrix or assessment.

All evidence records the accepted checkpoint `84f63bc6fdee75444b06723b3c5f542546e27b15`, with production source unchanged from `78d97e5c9c4791262a81ffd4b0823671034ec872`. The seven assessed PNGs remain at their canonical destinations; they are not duplicated here.

## Durable packet mapping

The former temporary packet `/tmp/opencode/pr87-orientation-evidence/` is no longer needed. Its selected files map as follows:

- `captures.json` and `runtime-assets.sha256`: this directory, unchanged bytes.
- `ss019-final.png.json`: sidecar for [SS-019](../../../../screenshots/NET-05/six-player-lan-degraded-1280x900.png).
- `ss026-final.png.json`: sidecar for [SS-026](../NET-05-C.png).
- `network-x-mirrored.png.json`, `local-round-a.png.json`, and `local-round-f.png.json`: sidecars for the three `orientation-*.png` comparisons in the parent directory.
- `local-console-a.png`, `local-console-f.png`, `host-end-confirm.png`, and `gl-info.png`, with their JSON sidecars: this directory.
- `ss021-final.png.json` and `reconnect-restored.png` with its JSON sidecar: [NET-07 evidence](../../NET-07/evidence/README.md).
- `ss023-final.png.json`: [NET-09 evidence](../../NET-09/evidence/README.md).
- The reconstructed temporary `README.md` is retained byte-for-byte as [capture-handoff-at-ux-review.md](capture-handoff-at-ux-review.md).

The retained handoff is **historical, before final UX acceptance and finalization**. Its pending-gate, temporary-path, and cleanup statements describe that earlier handoff, not current status. The original prose was lost during interruption and explicitly reconstructed; original PNG/JSON bytes were recovered unchanged. Current visual acceptance is recorded only in the authoritative manifest linked above.

JSON paths and capture-time worktree statements are original provenance, not claims that temporary paths still exist. Nearby observed ticks bound forwarded state rather than identifying the exact GPU-consumed update. Local Play's absent network identities/tick counter were not fabricated. Rendering evidence is Mesa software OpenGL, not physical-GPU or native Windows execution evidence.

## Integrity

- `captures.json`: `ac7bada9a5e00b252f7033179e97835cec4f197c880549d7ea44d4c81687d4bf`.
- `runtime-assets.sha256`: `76b8ac391b15e07bdb905f20c5d37b8369bbf31115b4b191f1aa5d7a04d833ad`.
- `capture-handoff-at-ux-review.md`: `62b4cac05225ee2239d99b2921ffd3bcf881f74dd8f3ced1c99ba5b7c7b17833`.
- The accepted build archive, removed after its final local consumer: `bc7ae4b4c49f232d1d3a630c479e768d434aa7bdb87a638535eb885e548f9174`.
- Accepted application executable: `394281ccbe19b207cdf97d9dd025bc95350a8a194651df9bb6061982e1274198`.

Supporting PNG hashes are recorded in the authoritative manifest and unchanged sidecars. The original PR #83 `SS-019-state.txt`, `background-provenance.json`, and confirmation supplement in the parent directory remain historical; they do not describe the PR #87 replacements.

## Behavioral evidence boundary

Team supplied source approval at `78d97e5` and independent QA acceptance at `84f63bc`: 48 authoritative cases, 20 runtime cases, and one font case, 69 total, with no failures. Developer's isolated old-transform negative control rebuilt only the affected library/test path and ran `D6R_TEST_FILTER=PR87` through the registered network-session CTest harness. Both graphical cases rejected the prior negative-XY transform; the actual-simulation producer passed. The unchanged accepted checkpoint passed the same local filter. This diagnostic is not independent QA or hosted CI evidence.

Finalization does not change executable behavior. Required Linux compilation, MSVC transport CTests, and Feature Ready CI remain separate hosted gates owned by the CI specialist; no hosted pass is claimed here.
