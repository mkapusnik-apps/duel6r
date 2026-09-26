# NET-05 — First-round arrival

Functional contract: [network-match](../../screens/network-match.md). Existing NET-VIS and NRP presentation requirements remain applicable. [NET-ADM-001–012 and NET-ADM-AC-001–003](../../network-play-first-release.md) own admission and arrival. Entry from `NET-03-live-admission` uses the existing arena layout. Structural source: [NET-05](../../screens/wireframes/network-match.md).

This is a state change within the existing shared arena, not a new layout.

- An admitted first-round arrival must enter the current confirmed shared arena directly.
- The screen must not show a new-round introduction for every existing player because one participant arrived.
- The arriving player's name and normal gear indicators must remain readable using existing gameplay presentation.
- The UI must not show a spectator or next-round-waiting message for an arrival admitted to play immediately.
- Existing players, current world objects, round progress, and Predator presentation must remain visually continuous.
- A directory refresh must not appear over live play or take gameplay focus.
- The arrival must not add a split viewport, modal, or new gameplay action.

Acceptance needs a guest-side first-round arrival artifact. A still cannot establish default gear, unchanged world state, unchanged existing players, Predator retention, or the outcome-time admission boundary. Developer and behavioral QA must supply those checks separately. The existing NET-05 representative remains required; arrival is a focused supplemental state, not a new stable wireframe.
