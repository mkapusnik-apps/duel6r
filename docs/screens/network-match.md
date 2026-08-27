# NET-05 — Network match shared arena

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It presents the authoritative network match in the existing undivided shared arena. It implements `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-010`, `NET-AC-011`, `NET-AC-012`, `NET-AC-013`, `NET-AC-014`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-018` in [`docs/network-play-first-release.md`](../network-play-first-release.md) alongside unchanged local gameplay presentation requirements.

The host starts this screen from `NET-04` after all participants are ready and clears any prior retained result. Match completion enters `NET-06`; ambiguous guest isolation enters `NET-07`. Only a valid host-end notice or independently definitive session termination enters `NET-09`.

## Representative layout

- Fill the client with one undivided arena that shows the complete level and all 2–15 players.
- Preserve existing world, ranking, round progress, event, and player-status presentation.
- Add only compact textual session status that does not obscure required play: endpoint/session role and exceptional connection state where applicable.
- The representative state is a six-player LAN Deathmatch at 1280 by 900 with all connected participants active.

## Navigation and significant variants

- Each participant's devices control only that participant's local players; all world and score outcomes are authoritative.
- Tab continues to show the applicable authoritative score overlay. Network score context is `Session only`.
- Guest `Leave session` opens `Leave session? Your players will be removed immediately and the match will continue without reconnect.` Confirm sends that guest to `NET-01`; Cancel returns to active play. Leaving is immediate and is not a pause.
- Host `End session` opens `End session for everyone?` Confirm sends the host to `NET-01` and guests to host-ended `NET-09`; Cancel returns to active play.
- A guest transport loss replaces that guest's view with `NET-07`; other connected participants continue seeing the live arena and a textual reconnecting status for reserved players.
- Reserved players receive no input, remain targets, count for winner conditions, and follow normal damage, death, scoring, and round progression.
- During an active round, same-clock confirmed leaves and authoritative expiries are removed atomically without removal combat statistics, followed by exactly one winner evaluation. With at least two roster players play continues; with fewer than two, create `Session only • Interrupted • No winner`, retain it, and return connected participants to `NET-04`.
- During a non-final round summary, preserve the completed round, then either continue to the next round with at least two roster players or retain `Session only • Interrupted • No winner` and return to `NET-04`.
- Contact loss, silence, refusal, unreachable, reset, temporary failure, or no response shows `NET-07` for the full original deadline. Valid host end or independently definitive termination shows the applicable `NET-09`; there is no migration.

## Truthful copy, disabled reasons, and input

- The screen must never show `Paused for reconnect`; active-round simulation continues.
- Text must distinguish `Guest reconnecting (24s)` from an intentional departure or expired reservation.
- Existing gameplay controls remain unchanged. Session actions use deterministic keyboard/controller focus. Confirmation actions are `Leave session`/`Cancel` for guests and `End session`/`Cancel` for the host and do not capture player controls unless visibly open.
- No join, invite, migration, account, or persistent-statistics action may appear during the match.

Planned representative screenshot: [`SS-019`](../screenshots/README.md#ss-019).
