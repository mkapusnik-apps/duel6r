# Public pilot evidence integration

This developer-owned index makes the evidence referenced by the unchanged [UX assessment](../../README.md#current-artifact-assessment) portable. It does not modify the assessment or grant product acceptance. Native Windows execution, the final product decision, and hosted verification remain separate gates.

## Durable records

- [Portable manifest](manifest.json): repository-relative paths, original capture locations, file hashes, checkpoint attribution, viewports and timestamps where supplied.
- [Linked artifact index](ARTIFACTS.md): canonical images and all retained supplements.
- [SHA-256 checksums](SHA256SUMS): verify from the repository root with `sha256sum -c docs/design/screenshots/evidence/public-pilot/SHA256SUMS`.
- [Corrected capture report](reports/public-gui-recapture-report.md): exact assessed bytes, including reproduction steps and evidence boundaries.
- [Original corrected-capture inventory](reports/public-gui-recapture-inventory.json): exact assessed bytes. Its absolute paths identify the original capture locations; use the portable manifest for current locations.
- [Original capture report](reports/public-gui-capture-report.md): exact historical bytes establishing the four reused representatives and the earlier observations.
- [Gameplay-content manifest](reports/gameplay-content-manifest.json): identical content-manifest bytes for both capture fixtures.
- Runtime asset manifests: [a69a5ad](reports/runtime-assets-a69a5ad.sha256) and [a985ada](reports/runtime-assets-a985ada.sha256). These describe the original runtime bundles, not the final documentation-only checkout.

## Provenance and path mapping

The six corrected/new representatives and 38 current supplements use production source `a69a5adfd8fadb95e2024179bb5ecbd6ab3a62ad`, accepted QA checkpoint `424c4bde8a945f010432b554ad6de4e8c1ddc422`, and capture checkout `074bfe13ef214cea6a61833f2666759c9ab007f4`.

SS-015, SS-018, SS-026 and SS-023 reuse their unchanged captures from `a985ada14a3f514c0158d85a9bc8946c1c00dc03`. They are not relabeled as a69a5ad captures. The fourteen earlier boundary supplements are retained under `historical/`; their old correction findings are superseded, not evidence against the corrected presentation.

| Original location in the assessed handoff | Durable location |
|---|---|
| `/tmp/opencode/recapture-*.png` listed in the current UX table | `supplements/` with the same filename |
| `/tmp/opencode/public-boundary-*.png` listed in the historical UX table | `historical/` with the same filename |
| `/tmp/opencode/public-gui-recapture-report.md` | `reports/public-gui-recapture-report.md` |
| `/tmp/opencode/public-gui-recapture-inventory.json` | `reports/public-gui-recapture-inventory.json` |
| `/tmp/opencode/public-gui-capture-report.md` | `reports/public-gui-capture-report.md` |
| `/tmp/opencode/recapture-content-manifest.json` and `/tmp/opencode/public-gui-content-manifest.json` | `reports/gameplay-content-manifest.json` (identical bytes) |
| Original container `/runtime-a69a5ad/linux-x86_64.sha256sums` | `reports/runtime-assets-a69a5ad.sha256` |
| Original container `/runtime/linux-x86_64.sha256sums` | `reports/runtime-assets-a985ada.sha256` |

Canonical images remain at the paths in the UX matrix. The ordinary public NET-05, NET-06 and NET-04-R images use their `.public.png` destinations; no legacy extreme-scenario artifact was replaced.

The preserved reports contain historical statuses, temporary paths, teardown notes, and hashes of superseded captures. Those are immutable provenance, not instructions to depend on a live temporary directory, not current gate statuses, and not claims that a superseded image still occupies a canonical path. Use the current UX assessment and portable manifest for accepted artifact selection. Capture-start timestamps in the reports can precede PNG creation timestamps in the inventory by a fraction of a second.

## Integrity and scope

All copied PNGs and assessed handoff files retain their original SHA-256 hashes. The two UX-owned Markdown documents are integrated unchanged. This directory adds only non-executable evidence and an integration index; it includes no fixture runner, test-support source, certificate, private key, invitation or reconnect credential.

The visual gate is satisfied according to UX. This evidence does not establish native Windows TLS execution, complete functional regression, live public availability, or final product acceptance. Earlier legacy extreme requirements and their separate evidence remain governed by the [legacy manifest](../../../../screenshots/README.md).
