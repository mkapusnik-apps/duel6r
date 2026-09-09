# NET-05 — Network match shared arena

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It presents the authoritative network match in the existing undivided shared arena. It implements `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-009` through `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md) alongside unchanged local gameplay presentation requirements. Its degraded-network, visible-correction, and recovery variants implement `NRP-BUD-001` through `NRP-BUD-007`, `NRP-PRS-001` through `NRP-PRS-011`, `NRP-REC-001` through `NRP-REC-012`, `NRP-AUT-001` through `NRP-AUT-006`, and `NRP-AC-001` through `NRP-AC-014` in [`docs/network-responsiveness-and-recovery.md`](../network-responsiveness-and-recovery.md).
It preserves `INP-011` through `INP-016` and implements `NIN-OWN-006`, `NIN-BOUND-003`, and `NIN-COMP-AC-001` through `NIN-COMP-AC-004` in [`docs/network-authoritative-player-input.md`](../network-authoritative-player-input.md).
It implements `NET-VIS-003` through `NET-VIS-011` and `NET-VIS-AC-002` through `NET-VIS-AC-005`. It consumes updated `REP-028`, `REP-041`, `REP-AC-002`, `REP-AC-012`, `REP-PRES-001` through `REP-PRES-006`, and `REP-PRES-AC-001` through `REP-PRES-AC-003` from [`docs/network-state-replication.md`](../network-state-replication.md). It also consumes `CMP-VIS-001` through `CMP-VIS-004`, `CMP-VIS-AC-001`, and updated `AC-012` from [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).

The host starts this screen from `NET-04` after all participants are ready and clears any prior retained result. Match completion enters `NET-06`; unexpected host contact failure enters guest `NET-07`. Only a valid End session notice accepted through the current established session enters guest `NET-09`.

## Representative layout

- Fill the client with one undivided arena that shows the complete level and all 2–15 players.
- Preserve existing world, ranking, round progress, event, and player-status presentation.
- Add only compact textual session status that does not obscure required play: session role, LAN scope, connection state, result scope, and script policy.
- The representative state is a six-player LAN Deathmatch at 1280 by 900 during a sustained degraded-network condition while complete canonical updates continue.
- Network status must state `Optional scripts disabled` without obscuring play.
- Every local and remote player must use the built-in default network visual set.
- The screen must not use a selected local or remote profile for player skin, animation, or visual-resource selection.
- Player names must remain distinct from player appearance selection.

## Status hierarchy and allocation

- The arena must remain the primary visual region.
- The existing ranking, round progress, event text, and player status must remain above world imagery.
- A compact network status region must align to the bottom edge of the client.
- The network status region must keep a 16 px inset from the left, right, and bottom client edges.
- The network status region must use standard 16 px gameplay text.
- The network status region must use a flat translucent `info-surface` behind `info-text`.
- The network status region must keep 4 px of clear inner space around its text.
- The status region must use one left content group and one right content group.
- The left group must align to the left inset.
- The right group must align to the right inset.
- The groups must keep at least 16 px of clear horizontal space between them.
- The left group must show role, `LAN session`, and `Connected` on its first row.
- The degraded variant must show `Network connection degraded.` on the next row.
- The right group must show `Session only scores` and `Optional scripts disabled`.
- The degraded indication must have priority over the other status text when width is constrained.
- The degraded indication must not truncate, clip, scroll, or use an ellipsis.
- Other status phrases may wrap only at word boundaries.
- A wrapped status region must grow upward in 16 px rows.
- A wrapped status region must not exceed three rows.
- The status region must not make ranking, round progress, event text, player status, or session actions unreadable.
- A blocking confirmation panel must render above the network status region.

## Responsive behavior

- The gameplay renderer must fill each supported desktop client.
- The arena must remain one undivided view at each supported desktop viewport.
- The status groups must remain edge-aligned when the client width changes.
- The status groups must wrap before they overlap each other.
- A wrapped right group must move above the left group when two horizontal groups do not fit.
- The complete degraded indication must remain visible at the 1280 by 720 evaluation minimum.
- The status region must not change world scale, camera bounds, or player-specific allocation.

## Degraded, correction, and recovery states

- The supported state must show `Connected` without the degraded indication.
- The client must add `Network connection degraded.` after the sustained breach in `NRP-REC-001` or `NRP-REC-002`.
- The degraded indication must remain persistent while the degraded state applies.
- The degraded indication must not say that the participant disconnected.
- The degraded indication must not say that the host ended the session.
- The degraded state must keep presenting the latest complete accepted canonical state while updates continue.
- The degraded state must not dim, freeze, divide, or replace the arena.
- The degraded state must not add a modal panel or capture player input.
- A predicted local-player correction must keep one visible local-player sprite.
- The correction must move that sprite toward one latest accepted canonical position.
- The correction must finish within 150 ms after acceptance of that canonical state.
- The correction must not alternate between an older and a newer accepted position.
- The correction must not create a ghost, trail, flash, duplicate sprite, camera shift, or outcome effect.
- The correction must not change another player's visible state.
- The correction must not predict a shot, hit, pickup, bonus, damage, death, score, winner, or progression outcome.
- Authoritative outcomes must use their unchanged gameplay presentation.
- Full resynchronization may keep the last complete accepted frame as context.
- Retained resynchronization context must show `Last confirmed state`.
- Retained resynchronization context must show `Synchronizing current state…`.
- Retained resynchronization context must not show a percentage, partial-state count, or another recovery-progress value.
- Full resynchronization must replace retained context only with the latest complete valid full state.
- Recovery must not visibly rewind match time.
- Recovery must not repeat an event.
- Recovery must not restore a removed entity.
- The client must remove the degraded indication after all applicable budgets remain satisfied for three continuous seconds.
- The client must present current canonical state without the degraded indication within five seconds after supported conditions return.
- A closed guest transport must replace `NET-05` with `NET-07`.

## Interaction, focus, and accessibility

- The network status region must be non-interactive.
- The network status region must not receive keyboard, controller, or pointer focus.
- The network status region must not add a pointer or touch target.
- The network status region must not capture gameplay input.
- The exact degraded text must provide the primary degraded-state cue.
- Color or motion may reinforce the degraded state but must not replace the text.
- The degraded text must remain continuously available and must not use a transient toast.
- The degraded text must keep the contrast of `info-text` on `info-surface` over every arena background.
- A movement correction must not use flashing feedback.
- Existing session-action focus and confirmation containment must remain unchanged.

## Navigation and significant variants

- Each participant's devices control only that participant's local players; all world and score outcomes are authoritative.
- Tab continues to show the applicable authoritative score overlay. Network score context is `Session only`.
- Guest `Leave session` opens `Leave session? Your players will be removed immediately and the match will continue without reconnect.` Confirm sends that guest to `NET-01`; Cancel returns to active play. Leaving is immediate and is not a pause.
- Host `End session` opens `End session for everyone?` Confirm sends the host to `NET-01` and guests to host-ended `NET-09`; Cancel returns to active play.
- A guest transport loss replaces that guest's view with `NET-07`; other connected participants continue seeing the live arena and a textual reconnecting status for reserved players.
- Reserved players receive no input, remain targets, count for winner conditions, and follow normal damage, death, scoring, and round progression.
- During an active round, same-clock confirmed leaves and authoritative expiries are removed atomically without removal combat statistics, followed by exactly one winner evaluation. With at least two roster players play continues; with fewer than two, create `Session only • Interrupted • No winner`, retain it, and return connected participants to `NET-04`.
- During a non-final round summary, preserve the completed round, then either continue to the next round with at least two roster players or retain `Session only • Interrupted • No winner` and return to `NET-04`.
- Contact loss, silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, or no response shows guest `NET-07` for the full original deadline. Only an accepted intentional host End notice shows `NET-09`; there is no migration.
- A round outcome must start one six-second round-end phase.
- The first second must show that arena updates continue.
- The final five seconds must show a frozen round state and a countdown to automatic advancement.
- A non-final round must advance automatically when the countdown ends.
- The host may activate `Advance round` only after the round outcome exists.
- A guest must not receive an `Advance round` action.
- An arbitrary key press must not advance a network round.
- Shift+F1 must not force advancement before a winner exists.
- The final round must enter `NET-06` and must not show `Advance round`.
- Host `End session` must remain distinct from round advancement.
- Accepted host `End session` must discard all session-only results.

## Truthful copy, disabled reasons, and input

- The screen must never show `Paused for reconnect`; active-round simulation continues.
- Text must distinguish `Guest reconnecting (24s)` from an intentional departure or expired reservation.
- Existing gameplay controls remain unchanged. Session actions use deterministic keyboard/controller focus. Confirmation actions are `Leave session`/`Cancel` for guests and `End session`/`Cancel` for the host and do not capture player controls unless visibly open.
- No join, invite, migration, account, or persistent-statistics action may appear during the match.
- Phase and countdown text must remain visible without reliance on curtain motion or color.
- Only the host may receive focus on `Advance round` or `End session`.
- Local Play advancement, scripting, presentation, and persistence behavior must remain unchanged.
- Each established keyboard or controller action must affect only an owned player through the network session.
- Move left, move right, jump, crouch, shoot, pick a weapon, and show status must keep their established meanings.
- Double-jump, weapon-pick eligibility and drop-first behavior, five-second status display, and dead-player input behavior must remain unchanged.
- A controller connection change during the round must not reassign a player automatically.
- The graphical client must collect local device input, but the authoritative service must not initialize a renderer, audio, or local input device.
- The client must derive player animation and entity visuals from complete read-only replicated canonical state and the default network visual set.
- The client must not create or advance a second gameplay simulation for presentation.
- A full snapshot and equivalent incremental state must select the same default network visuals.
- Equal supported releases presenting the same replicated canonical state must select the same player skin, animation, and entity visual.
- Authoritative Team colors, Predator opacity, invisibility, and other replicated visual gameplay states must modify the default network visuals as defined by canonical state.
- A local or remote profile must not change the network player's visible skin, animation, or visual resource.
- A client must not load a peer profile, file, or script as a visual fallback.
- Selected-profile appearance parity remains outside first-release scope and is tracked by issue #84.

## Observable acceptance and evidence

- A static artifact must show one complete 1280 by 900 client without external window chrome.
- The artifact must show six living players in one complete LAN Deathmatch arena.
- The artifact must show the built-in default network player skin for all six local and remote players.
- The artifact setup must include local persons whose available profile appearances differ from the default network player skin.
- The artifact must not show a profile-selected player appearance or a profile-selection control.
- The artifact must show ranking, round progress, event text, and player status without status overlap.
- The artifact must show `Host`, `LAN session`, `Connected`, `Session only scores`, and `Optional scripts disabled`.
- The artifact must show the exact text `Network connection degraded.` during a sustained budget breach.
- The artifact must not show reconnect, disconnect, pause, or host-end copy.
- The artifact must keep the degraded indication fully readable without clipping or truncation.
- The canonical screenshot matrix must keep exactly one representative artifact for this wireframe.
- A supplementary recording should show supported remote motion, one visible local correction, degraded entry, and recovery clearance.
- The recording should show that correction converges within 150 ms.
- The recording should show that degraded text clears only after three continuous supported seconds.
- The recording should show current canonical presentation without the degraded indication within five seconds of restored supported conditions.
- Functional evidence must measure all non-static timing, authority, state-integrity, player-count, and Local Play requirements.

Planned representative screenshot: [`SS-019`](../screenshots/README.md#ss-019).
