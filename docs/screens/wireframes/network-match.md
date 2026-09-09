# NET-05 wireframe — Six-player LAN Deathmatch

Target representative viewport: 1280 by 900 px. The gameplay renderer fills the client with one undivided arena. This wireframe is planned for issue #38 and is not implemented.

```text
┌──────────────────────────── 1280 × 900 client ───────────────────────┐
│ events             Round 2 | 3              ranking: six players    │
│                                                                      │
│                 complete shared LAN arena                            │
│       P1        P2          P3          P4       P5        P6        │
│        terrain • hazards • pickups • shots • water                   │
│                                                                      │
│ Host • LAN session • Connected         Session only scores           │
│ Network connection degraded.           Optional scripts disabled    │
└──────────────────────────────────────────────────────────────────────┘
```

- Existing arena, ranking, progress, event, and player-status presentation remains available.
- Compact textual network status does not obscure play or create player-specific views.
- Guest `Leave session` and host `End session` use consequence panels with Confirm and Cancel over this unchanged arena.
- Every unexpected host failure remains in guest `NET-07` through the fixed deadline; only a valid intentional End session notice accepted through the established session uses `NET-09`.
- Active-round batches evaluate one winner condition and may create `Session only • Interrupted • No winner`; non-final summaries preserve completed rounds before continue/interruption. Degraded-host-only continuation and other variants remain in the specification.
- A round-result variant uses the existing summary language over this arena.
- The round-result variant shows `World active • 1s` before `Round frozen • Next round in 5s`.
- The countdown advances automatically.
- Only the host sees enabled `Advance round` and `End session` actions.
- Guests do not receive an advance action.
- The final-round variant enters `NET-06` and does not show `Advance round`.
- Compact status states that optional scripts are disabled and results are session-only.
- The degraded-network state is a variant of this wireframe and is not a new screen.
- The representative variant shows the exact persistent text `Network connection degraded.` while complete canonical updates continue.
- The bottom status region uses a 16 px client-edge inset and standard 16 px gameplay text.
- The bottom status region keeps 4 px of clear inner space around its text.
- The left status group contains role, LAN scope, connection state, and the degraded indication.
- The right status group contains result scope and script policy.
- The two status groups keep at least 16 px of clear space.
- The status groups wrap at word boundaries before they overlap.
- The degraded indication does not truncate or use an ellipsis.
- The status region grows upward to no more than three text rows when width requires wrapping.
- The status region does not make ranking, round progress, events, player status, or session actions unreadable.
- The degraded indication does not dim, freeze, divide, or replace the arena.
- A visible local-player correction keeps one sprite and moves it toward one latest accepted canonical position within 150 ms.
- A correction does not use a ghost, trail, flash, duplicate sprite, camera shift, or outcome effect.
- Full resynchronization may retain only the last complete accepted frame as non-current context.
- Retained resynchronization context shows `Last confirmed state` and `Synchronizing current state…` without partial progress.
- Recovery removes the degraded indication only after the approved supported-condition interval.
- The network status region is non-interactive and does not receive focus or capture gameplay input.
- The exact degraded text supplies a non-color status cue.
- All six local and remote players use the built-in default network player skin and deterministic default animation and entity mappings.
- Replicated Team color, Predator opacity, invisibility, and other visual gameplay states modify the default network visuals when applicable.
- No selected local or remote profile changes a network player appearance.
- The client derives presentation from complete read-only replicated canonical state and does not advance a second gameplay simulation.
- Selected-profile appearance parity is deferred to issue #84.

Planned representative screenshot: [`SS-019`](../../screenshots/README.md#ss-019).
