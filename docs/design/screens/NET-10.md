# NET-10 — Session browser

Status: implemented task; unified presentation pending evidence. Functional contract: [network-browser](../../screens/network-browser.md), implementing [NET-DIR-001–016 and NET-DIR-AC-001–004](../../network-host-directory.md). Wireframe: [NET-10](../wireframes/NET-10/NET-10.svg).

## Presentation requirements

The [shared visual baseline](../../design.md#unified-network-presentation) applies.

- **UX-NET-10-001** BROWSE SESSIONS must occupy a blue title strip above the fixed directory-status row.
- **UX-NET-10-002** The session table must use an inset white body with a separate fixed grey heading strip.
- **UX-NET-10-003** The selected row must use the standard selection fill and text while retaining its textual marker and separate focused-row outline.
- **UX-NET-10-004** The endpoint column must retain the largest width without reducing the complete Players, Phase, Password, or Admission headings.
- **UX-NET-10-005** The selected-session region must remain below the list and above page navigation with enough height for endpoint, eligibility, and reachability text.
- **UX-NET-10-006** Previous page and Next page must use visible button boundaries on either side of the current page label.
- **UX-NET-10-007** The page label must show the available current page number without inventing a total page count.
- **UX-NET-10-008** Join selected, Refresh, and Direct connect must retain the existing first footer row, with Back on the second row at the right.
- **UX-NET-10-009** The list must give up visible rows before page controls, selected-session details, or footer captions overlap.
- **UX-NET-10-010** Empty, loading, stale, and unavailable feedback must remain inside the same list/status allocation without changing footer placement.

The SVG uses approximate allocation. UX-NET-10-005 through UX-NET-10-008 fix the existing selected-details, paging, and two-row footer relationship; their controls retain the current traversal. No new sorting, filtering, total-count query, or row-activation behavior is introduced.

## Task and structure

Entry workflow: MENU-01 → Network → Browse sessions. This is a native workflow, not a URL. Join selected continues to NET-03 for local-player setup and password entry. Direct connect continues to NET-03 without a directory selection. Back returns to NET-01.

- The screen must use the existing centered 850 × 700 logical desktop canvas.
- The content must keep a 24 logical px inner margin.
- The fixed header must show `BROWSE SESSIONS` and directory status.
- The main body must contain one selectable table with fixed headings.
- The columns must be `Session / endpoint`, `Players`, `Phase`, `Password`, and `Admission`.
- The endpoint column must receive the largest share of table width.
- The table must show every active listing returned by the directory, including full, protected, and started sessions.
- The UI must not provide a default joinable-only filter.
- The UI must show player occupancy as a count and limit rather than an unlabeled number.
- Phase, password protection, and admission must occupy separate columns.
- The Password column must use `Required` or `None` instead of a lock icon alone.
- An unknown protection value must use `Unknown`, not `None`.
- An active first round must use `Round 1` with `Open` when admission is advertised as open.
- A phase after admission closes must show `Closed` in Admission even if its player count is below capacity.
- A full session must show `Full` without hiding its phase or password value.
- A locked listing must use `Required` in Password, not a closed-admission label.
- Password protection alone must not disable Join selected.
- The footer must contain `Join selected`, `Refresh`, `Direct connect`, and `Back`.
- A fixed selected-session region above the footer must show the full endpoint and the admission explanation.
- The selected-session region must also show the advertised session identity and mode under NET-DIR-003.
- A loopback endpoint must show `Loopback address; not verified as the advertised host on this machine.` in the selected-session region.
- The same region must say `LAN-first. A listing does not guarantee reachability.`
- The UI must not claim that non-LAN addresses are prohibited or that NAT traversal exists.

## State presentation

| Functional state ID | Persistent feedback | Actions |
|---|---|---|
| `NET-10-loading` | `Loading sessions…` in the status region; no false empty message; use `Refreshing…` when prior rows remain | Direct connect and Back remain available; duplicate Refresh and Join remain disabled |
| `NET-10-results` | `Sessions listed` and the current table | Join selected follows the selected admission state; Refresh, Direct connect, and Back remain available |
| `NET-10-empty` | `No active sessions listed. You can host a session or connect directly.` inside the table body | Refresh, Direct connect, and Back remain available |
| `NET-10-stale` | `Results are out of date. Refresh before joining.`; add `Directory unavailable.` after a failed refresh | Rows remain readable and selectable for inspection only; Join selected remains disabled until a successful refresh; Refresh, Direct connect, and Back remain available |
| `NET-10-unavailable` | `Directory unavailable. Direct connection and local play are still available.` | Refresh retries the directory request; Join selected remains disabled; Direct connect and Back remain available; retained rows use the stale treatment |

- A disabled Join selected must show `Select a session.`, `Session is full.`, or `Admission is closed.` as applicable.
- A stale selected listing must show `Refresh before joining.` as its primary disabled reason.
- Stale rows must show `Unconfirmed` instead of a current Open admission claim.
- The freshness transitions must follow NET-DIR-010–011 without a separate UX timer policy.
- When a selected row is both full and closed, the detail region must show both conditions.
- Unknown admission must not be labeled Open.
- An unknown admission value must keep Join selected disabled with `Availability unconfirmed. Refresh before joining.`
- No row activation, shortcut, or controller action may bypass the stale-result join restriction in NET-DIR-012.
- Direct connect must open independent direct setup without transferring a stale selected listing or treating it as join authorization.
- When a successful refresh removes an expired selected listing, the detail region must show `Session is no longer listed.`
- Removal must not move selection to another session automatically.
- Refresh must preserve selection by session identity rather than row position.
- Refresh must not move keyboard focus to another action.
- A rejected connection must use NET-08 rather than silently return to the table.

## Allocation, overflow, and input

- The table body must scroll vertically without scrolling its headings or footer.
- Bounded result pages must provide Previous and Next controls below the rows when more pages exist.
- Page controls must show current position and remain keyboard- and controller-operable.
- Page controls must precede the footer in reading and focus order.
- Paging must expose every active listing under NET-DIR-008 without an implicit first-page limit.
- The table must keep one fixed-height row per session.
- Long endpoint text must clip inside its column without covering Players.
- The selected-session region must wrap the full endpoint at character boundaries when needed.
- The selected-session region must scroll if its full content exceeds its reserved height.
- The body must not expand when loading, empty, or failure feedback replaces rows.
- Each visible row must have one complete pointer selection target.
- Row selection must not immediately connect.
- Initial focus must enter the table when rows exist and Refresh when no rows exist and Refresh is enabled.
- A pending initial read must place focus on Direct connect rather than a disabled Refresh action.
- Tab and Shift+Tab must traverse the table and enabled footer actions in reading order.
- Directional input must move row selection within the table.
- Enter or controller Confirm on a row must use the same eligibility checks as Join selected.
- Escape or controller Back must return to NET-01.
- Selection and focus must have distinct non-color cues.
- The canvas and pointer regions must scale together using the existing desktop transform.
- No mobile reflow, search, sorting controls, favorites, matchmaking, accounts, or service configuration panel is required.

## Visual acceptance

The representative must show an open lobby, a protected open first round, a full session, and a closed started session together. The selected protected first-round row must still offer Join selected. A reviewer must be able to identify the endpoint, occupancy, phase, protection, and admission independently. A still image does not prove heartbeat expiry, host-side password enforcement, or exact admission closure timing.

The selected and focused row must remain distinguishable without color, and every footer and page action must remain identifiable when unfocused. Focused QA must cover all five functional states, both page directions, no-selection and removed-selection states, disabled joining reasons, retained selection on refresh, long endpoints, and independent direct setup. The [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix) defines the representative and evidence limits.
