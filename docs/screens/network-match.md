# NET-05 — Network match shared arena

## Status, purpose, and requirements

This is a target screen for downstream issue #38; it is not implemented. It presents the authoritative network match in the existing undivided shared arena. It implements `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, and `NET-AC-009`–`NET-AC-014` in [`docs/network-play-first-release.md`](../network-play-first-release.md) alongside the unchanged local gameplay presentation requirements.

The host starts this screen from `NET-04` after all participants are ready. Match completion enters `NET-06`; guest disconnect enters `NET-07`; host loss enters `NET-09`.

## Representative layout

- Fill the client with one undivided arena that shows the complete level and all 2–15 players.
- Preserve existing world, ranking, round progress, event, and player-status presentation.
- Add only compact textual session status that does not obscure required play: endpoint/session role and exceptional connection state where applicable.
- The representative state is a six-player LAN Deathmatch at 1280 by 900 with all connected participants active.

## Navigation and significant variants

- Each participant's devices control only that participant's local players; all world and score outcomes are authoritative.
- Tab continues to show the applicable authoritative score overlay. Network score context is `Session only`.
- A connected participant's intentional Leave requires confirmation and is immediate; it is not a pause.
- A guest transport loss replaces that guest's view with `NET-07`; other connected participants continue seeing the live arena and a textual reconnecting status for reserved players.
- Reserved players receive no input, remain targets, count for winner conditions, and follow normal damage, death, scoring, and round progression.
- Host loss replaces the guest view with `NET-09`; there is no migration state.

## Truthful copy, disabled reasons, and input

- The screen must never show `Paused for reconnect`; active-round simulation continues.
- Text must distinguish `Guest reconnecting (24s)` from an intentional departure or expired reservation.
- Existing gameplay controls remain unchanged. Session actions use a deterministic keyboard/controller overlay focus with Continue/Cancel confirmation and must not capture player controls unless the session menu is visibly open.
- No join, invite, migration, account, or persistent-statistics action may appear during the match.

Planned representative screenshot: [`SS-019`](../screenshots/README.md#ss-019).
