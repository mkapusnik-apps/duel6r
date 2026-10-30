# OVER-03 — Local shared final summary

Functional authority: [OVER-03](../../screens/overlay-game-over.md), SUMMARY-VIEW-001–007 / SUMMARY-VIEW-AC-001–005, LIF-018–029, SCO-022–023, UI-GAME and AC-070–072. States are `OVER-03-final-wait` and `OVER-03-completed`. Structural authority: [OVER-03](../wireframes/OVER-03/OVER-03.svg). Appearance authority: [Shared final presentation](../../design.md#shared-final-presentation), UX-FINAL-001–034. This binding does not duplicate the common shell requirements or migrate the legacy design system.

Visual impact is high: all completed finite local modes use the same arena-backed shell as NET-06. The shared header, fixed outcomes, compact result body, footer allocation and End of Game notice replace the former oversized local panel. Non-final OVER-02, active Tab, unlimited matches and the active arena retain their existing layout. The shared final type sizes do not change those other summaries.

Local content retains the available cumulative K, A, D, K/D and PTS values and Team nesting. It does not acquire invented network identities, ownership, session-only disclaimers, per-round records or settings that the local functional contract does not provide. Local statistics and eligible completed limited Deathmatch Elo remain product-owned and unchanged. The presentation must not assert a successful save merely because it has a completed result. No new Exit button, network confirmation, auto-dismissal or next-round action is introduced.

## Approved local result navigation

The updated product baseline resolves the former fixed top-center/oversized overlap rules and Predator exclusion. Final counter values, completion timing, local result fields, ranking and persistence stay unchanged. Labels refer to actual final-round outcome data, not a result computed by the view. The local role footer has no new exit button or guidance copy.

The approved [local completed-result navigation contract](../../screens/overlay-game-over.md#local-completed-result-navigation), SUMMARY-LOCAL-NAV-001–005, supplies the input binding for UX-FINAL-020–026. SUMMARY-VIEW-AC-006–007 own overflow access and input-isolation acceptance. The functional decision is closed; specification and coverage are ready for implementation.

Use the approved bounded wheel/Page Up/Down navigation and Shift horizontal variants only in the completed result context, with wheel input scoped to the result body. Preserve console precedence, final-wait input isolation, existing exit/player/controller behavior and unchanged network navigation under that contract. No new controller binding, exit control or confirmation is introduced. Ordinary two-player and 15-player/four-team content uses the same layout; content extremes do not add a new screen or mobile presentation class.

The [coordinated matrix](../screenshots/README.md#arena-parity-and-shared-final-presentation) owns capture. The representative is a real finite 15-player/four-team completion with final winner different from cumulative leader. Local exit/persistence and scroll/input ownership require source-tied functional QA; a still cannot prove them.
