# NET-10 — Session browser

Functional contract: [network-browser](../../screens/network-browser.md), implementing [NET-DIR-001–016 and NET-DIR-AC-001–004](../../network-host-directory.md). Wireframes: [results NET-10](../wireframes/NET-10/NET-10.svg), [empty NET-10-E](../wireframes/NET-10/NET-10-E.svg), [failed refresh NET-10-U](../wireframes/NET-10/NET-10-U.svg).

## Presentation requirements

The [shared visual baseline](../../design.md#unified-network-presentation) applies.

- **UX-NET-10-001** BROWSE SESSIONS must occupy a blue title strip above the reserved directory-status allocation. Successful results must not show `Sessions listed`; removing that copy must not shift the table upward.
- **UX-NET-10-002** The session table must use an inset white body with a separate fixed grey heading strip.
- **UX-NET-10-003** The selected row must use the standard selection fill and text while retaining its textual marker and separate focused-row outline.
- **UX-NET-10-004** The endpoint column must retain the largest width without reducing the complete Players, Phase, Password, or Admission headings.
- **UX-NET-10-005** The selected-session region must remain below the list and above page navigation with enough height for endpoint, eligibility, and reachability text.
- **UX-NET-10-006** Previous page and Next page must use visible button boundaries on either side of the current page label when either page direction exists. Hide the entire pagination group when neither direction exists; retain an unavailable opposite direction as disabled when the group is visible.
- **UX-NET-10-007** The visible page label must show the available current page number without inventing a total page count. Hide it with unnecessary pagination buttons.
- **UX-NET-10-008** Refresh must be a compact 112 × 24 logical px upper action at bottom-left renderer position (710, 518). Its right edge at 822 aligns with the table header, and its bottom sits 8 logical px above the header top at 510. The 2 logical px outer focus keyline must retain 6 logical px clearance above the header and below the title strip at 550. Center the standard 16 logical px caption in the square frame; the entire button rectangle remains the pointer target, with existing pressed and disabled treatment. Loading and age-stale status must retain their existing left origin and have at least 8 logical px clearance from the Refresh focus keyline, without overlap or font shrinkage. Join selected and Direct connect retain their existing first-footer-row positions and sizes, with Back on the second row at the right. The former Refresh footer slot stays unused.
- **UX-NET-10-009** The list must give up visible rows before page controls, selected-session details, or footer captions overlap.
- **UX-NET-10-010** Empty, loading, stale, and unavailable feedback must remain inside the same list/status allocation without changing footer placement.

- **UX-NET-10-011** Empty and failed reads must retain the existing fixed grey heading strip and all five headings above the inset white table body at its existing bounds. Show the applicable description once below the header at its current left alignment with at least 4 logical px horizontal padding, spanning the body as one complete line in standard 16 logical px dark grey `#595959` text on white (contrast at least 4.5:1). Do not wrap, truncate, or shrink the font. Show no session rows, body column dividers, separators, scrollbar, selection marker or fill, hover or focus treatment, or action frame. The empty body and description must have no pointer, keyboard, or controller focus, selection, or activation. Keep table geometry, selected-details, reserved status, paging, and footer allocations unchanged.

The SVG uses approximate top-left allocation; UX-NET-10-008 defines authoritative production geometry in bottom-left renderer coordinates. Selected-details, paging, table geometry, and the remaining two-row footer relationship stay unchanged. Forward focus cycles Refresh → table → Previous page → Next page → Join selected → Direct connect → Back → Refresh; reverse traversal reverses this cycle, skipping hidden or disabled controls under existing eligibility. No new sorting, filtering, total-count query, or row-activation behavior is introduced.

## Task and structure

Entry workflow: MENU-01 → Network → Join in the NET-01 popup. Join invokes the existing Browse sessions action. This is a native workflow, not a URL. Join selected continues to NET-03 for local-player setup and password entry. Direct connect continues to NET-03 without a directory selection. Back returns to the NET-01 popup. This entry-label change does not change browser layout, controls, or behavior.

- The screen must use the existing centered 850 × 700 logical desktop canvas.
- The content must keep a 24 logical px inner margin.
- The fixed header must show `BROWSE SESSIONS`; directory status is reserved for loading and age-stale feedback, not successful-load copy.
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
- The footer must contain `Join selected`, `Direct connect`, and `Back`.
- `Refresh` must use the compact upper action defined by UX-NET-10-008.
- A fixed selected-session region above the footer must show the full endpoint and the admission explanation.
- The selected-session region must also show the advertised session identity and mode under NET-DIR-003.
- A loopback endpoint must show `Loopback address; not verified as the advertised host on this machine.` in the selected-session region.
- The same region must say `LAN-first. A listing does not guarantee reachability.`
- The UI must not claim that non-LAN addresses are prohibited or that NAT traversal exists.

## State presentation

| Functional state ID | Persistent feedback | Actions |
|---|---|---|
| `NET-10-loading` | `Loading sessions…` in the status region; no false empty message; use `Refreshing…` when prior rows remain | Direct connect and Back remain available; duplicate Refresh and Join remain disabled |
| `NET-10-results` | Current table, without `Sessions listed` | Join selected follows the selected admission state; Refresh, Direct connect, and Back remain available |
| `NET-10-empty` | `No active sessions listed. You can host a session or connect directly.` once in subdued dark grey inside the white table body below the retained headings, on one complete line | Refresh, Direct connect, and Back remain available |
| `NET-10-stale` | Age-stale feedback in the status allocation | Age-stale rows remain readable and selectable for inspection only; Join selected remains disabled until a successful refresh; Refresh, Direct connect, and Back remain available |
| `NET-10-unavailable` | `Directory unavailable. Direct connection and local play are still available.` once in subdued dark grey inside the white table body below the retained headings, on one complete line | Hide cached rows and selected identity, endpoint, and mode details, including after failed refresh; retain identity internally for recovery only if present. Every browser join path remains disabled. Refresh, Direct connect, and Back remain available |

- Successful current results with no selection, including a successful zero-row read, must leave the reason line blank without a replacement instruction; Join selected remains disabled. Retain loading, unavailable, full, closed, stale, expired, and removed-selection explanations as applicable, including `Session is full.` and `Admission is closed.`.
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
- Page controls must precede the footer in reading and focus order, with the upper Refresh action in the cycle defined by UX-NET-10-008.
- Hidden page controls must have no pointer target or input traversal. Empty and error table bodies and status descriptions must be noninteractive, with no pointer, keyboard, or controller focus, selection, or activation. Retain useful Join-disabled feedback without exposing hidden selected details after failure.
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
- Tab and Shift+Tab, and controller traversal in both directions, must follow the UX-NET-10-008 cycle while skipping hidden or disabled actions.
- Directional input must move row selection within the table.
- Enter or controller Confirm on a row must use the same eligibility checks as Join selected.
- Escape or controller Back must return to NET-01.
- Selection and focus must have distinct non-color cues.
- The canvas and pointer regions must scale together using the existing desktop transform.
- No mobile reflow, search, sorting controls, favorites, matchmaking, accounts, or service configuration panel is required.

## Visual acceptance

The representative must show at least two actual owned listings: a protected joinable session and an admission-closed session. The selected protected row must offer Join selected. A reviewer must be able to identify the endpoint, occupancy, phase, protection, and admission independently. Open-lobby, open-first-round, full, and closed variants remain focused checks; they need not coexist in the representative. A still image does not prove heartbeat expiry, host-side password enforcement, or exact admission closure timing.

The selected and focused row must remain distinguishable without color, and every footer and page action must remain identifiable when unfocused. Focused QA must cover all five functional states, both page directions, no-selection and removed-selection states, disabled joining reasons, retained selection on refresh, long endpoints, and independent direct setup. The [current matrix](../screenshots/README.md#network-presentation-current-capture-matrix) defines the representative and evidence limits.

The localized browser refinement uses the [NET-10 refinement matrix](../screenshots/README.md#net-10-localized-browser-refinement). Focused checks cover single-page and multiple first/middle/last pages, pagination after refresh, hidden input exclusion, empty/initial failure/failed populated refresh, recovery with identity present or absent, age-stale readability, all browser join paths disabled after failure, and independent Direct connect. Check blank reason lines for successful populated unselected and zero-row states while preserving necessary explanations. Verify both traversal directions with keyboard and controller, caption/status/focus clearance and single-line containment at 850 × 700 and 1280 × 720, plus pointer alignment at 1920 × 1080.
