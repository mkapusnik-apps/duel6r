# NET-01 — Network entry

## Purpose and authority

This screen separates network play from local-only Play. It is a modal over `MENU-01` with Host, Join, and Back only. This contract owns the entry actions and destinations. It supersedes the earlier four-action entry and `NET-01` → Join → `NET-03` route in [network play](../network-play-first-release.md#join), without changing dedicated admission or gameplay scope.

`NET-AC-001`, `NET-AC-002`, `NET-AC-003`, `NET-AC-015`, `NET-AC-017`, and `NET-AC-019` remain applicable. [NET-DIR-009](../network-host-directory.md) owns browser requests and selection. [NET-DIR-015](../network-host-directory.md) retains directory independence and direct-connection recovery. No platform, compatibility, admission, or gameplay requirement changes.

UX owns the compact modal layout, removal of explanatory text, focus, input presentation, and visual rules in [NET-01 presentation](../design/screens/NET-01.md). This screen does not show host settings or endpoint fields.

## Entry actions and outcomes

Entry remains `MENU-01` → `Network (F2)`, including pointer and F2 entry on experimental macOS. Entry grants no host or participant authority.

- **NET-ENTRY-001** `NET-01` must provide only the actions `Host`, `Join`, and `Back`.
- **NET-ENTRY-002** Host must open editable host setup in `NET-02` without starting a hosted session.
- **NET-ENTRY-003** Join must open `NET-10` through the existing browser-opening behavior previously labeled `Browse sessions`.
- **NET-ENTRY-004** Back must return to `MENU-01` without changing local setup or starting a network service.
- **NET-ENTRY-005** The application must prevent activation of underlying main-menu actions while the `NET-01` modal is open.

Direct connect remains in the existing `NET-10` browser and opens independent direct setup in `NET-03`. A directory failure must not disable the `NET-01` Join route to that browser or block its Direct connect action. NET-DIR-015 remains authoritative for directory independence. Back from direct setup retains its existing `NET-01` destination.

## Functional states and transitions

| State ID | Meaning and outcome |
| --- | --- |
| `NET-01-default` | The modal offers Host, Join, and Back; no session has started. |
| `NET-01-runtime-unavailable` | Host and Join are disabled with `Network runtime unavailable`; Back remains enabled. |

Directory unavailability is not network-runtime unavailability. Returning from setup or a recoverable failure restores this modal without an active-session claim. Escape or controller Back retains the Back outcome. Default focus remains Host; UX owns traversal and activation presentation for the three actions.

Browsing, optional password admission, and round-one admission follow the existing directory and network-play contracts. This entry adds no matchmaking, accounts, dedicated-server, NAT traversal, or host-migration actions and makes no Internet-reachability promise. Local Play remains outside this flow under NET-AC-015.

## Functional acceptance criteria

- **NET-ENTRY-AC-001 — Modal actions:** Entry by the supported pointer and F2 paths opens the modal with exactly Host, Join, and Back. Underlying main-menu actions cannot activate while the modal is open. This criterion covers NET-ENTRY-001 and NET-ENTRY-005.
- **NET-ENTRY-AC-002 — Destinations:** Host opens editable `NET-02` without session startup. Join opens `NET-10` and requests current listings under NET-DIR-009. Back, Escape, and controller Back return to unchanged `MENU-01` without starting a network service. This criterion covers NET-ENTRY-002 through NET-ENTRY-004.
- **NET-ENTRY-AC-003 — Direct recovery:** With the directory unavailable, Join still opens `NET-10`, and its existing Direct connect action reaches editable `NET-03` without a browser-selected session. Back from direct setup returns to `NET-01`. This criterion applies existing NET-DIR-015 and NET-DIR-AC-004 to the changed entry route.
- **NET-ENTRY-AC-004 — Unavailable runtime:** Network-runtime unavailability retains the disabled Host and Join actions and the exact reason above. Back remains usable and preserves local setup.

Final product acceptance requires behavioral evidence for these criteria at the frozen candidate and UX assessment with implementation evidence from the coordinated screenshot matrix for the compact modal and removed explanatory text. Earlier network-entry evidence does not establish the changed actions, route, or presentation. Representative screenshot reference: [`SS-015`](../screenshots/README.md#ss-015).

### Preserved dedicated and mixed-session contract

`NET-01-PUBLIC-ENTRY` consumes NET-PUB-017–021/024–025 through Join → mixed NET-10 → selected dedicated setup, or NET-10 → independent Direct connect. NET-03 owns the unbound choice and browser-derived locked hosting/security mode, operator TLS frontend endpoint and expected nonzero identity. This entry adds no dedicated administrator or matchmaking action and creates no controller/participant authority.

Prepared ready dedicated idle has zero participants/players, a nonzero identity and no playable match under NET-PUB-024–025. Exactly the first committed admission becomes controller; current player-hosted minimums do not invent an idle participant or controller. NET-DIR-021–033 retain mode/protection validation, legacy capability isolation, authoritative admission and unsupported Mac/Pi inspectable listings with Join disabled/no Invite fallback. The dedicated Linux/Windows x86-64 scope and required Windows journey in NET-PUB-AC-006 remain unchanged; entry presentation makes no native verification claim.
