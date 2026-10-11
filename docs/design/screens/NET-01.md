# NET-01 — Network entry

Functional contract: [network-entry](../../screens/network-entry.md), including NET-AC-001–003 and NET-AC-015–019. [NET-DIR-009 and NET-DIR-015](../../network-host-directory.md) own browser entry and directory independence. Structural wireframe: [NET-01](../wireframes/NET-01/NET-01.svg). Capture: [SS-015](../screenshots/README.md#net-01-modal-current-coverage).

## Scope and authority

This bounded change presents NET-01 as a modal over MENU-01. It replaces the former full task panel, scope copy, and four-action entry. Product owns Host, browser-entry Join, and Back outcomes. NET-10 and NET-03 destination layouts and behavior remain unchanged. The [shared visual baseline](../../design.md#unified-network-presentation) supplies unchanged styling and desktop scaling. This section owns the changed NET-01-specific structure.

Stable screen and wireframe IDs remain `NET-01`. The representative is initial inactive entry with Host focused. No new functional state ID or presentation class is introduced.

## Presentation requirements

- **UX-NET-01-001** The popup must show `NETWORK PLAY` in the existing panel title strip.
- **UX-NET-01-002** The popup must show `Host`, `Join`, and `Back` in one centered column in that order.
- **UX-NET-01-003** The popup must omit the former LAN, directory, compatibility, and participant-limit copy.
- **UX-NET-01-004** A focused enabled action must retain the same surface and caption alignment as the other enabled actions.
- **UX-NET-01-005** An unavailable action must retain a readable boundary and its existing reason without moving Back off screen.

UX-NET-01-006–008 are retired exact-copy and placement requirements. The popup does not display their former compatibility line. Removing entry copy does not change functional platform, release, content, or participant limits.

- **UX-NET-01-009** The popup must use a fixed 380 by 210 logical px region centered in the 850 by 700 logical canvas.
- **UX-NET-01-010** Each action must retain a 300 by 32 logical px target with 13 logical px gaps between actions.
- **UX-NET-01-011** The popup must retain the existing text size, panel treatment, button treatment, and non-color focus keyline.
- **UX-NET-01-012** The actual main menu must remain visible behind the popup with its current roster, settings, banner, version, and main action row.
- **UX-NET-01-013** The underlying main menu must not receive pointer activation, focus traversal, or Local Play shortcuts while the popup is open.
- **UX-NET-01-014** Host must receive initial focus on entry and re-entry.
- **UX-NET-01-015** Keyboard and controller traversal must follow Host, Join, and Back, with predictable reverse traversal.
- **UX-NET-01-016** Pointer activation regions must coincide with visible action bounds after the inverse canvas transform.
- **UX-NET-01-017** The popup and underlying menu must use the same existing centered desktop canvas transform.
- **UX-NET-01-018** A directory outage must not disable Host, Join, or Back in the entry popup.
- **UX-NET-01-019** The popup must fit without scrolling, clipped captions, clipped focus outlines, or a second task panel.

## Allocation and responsive behavior

All positions below use logical px and a top-left origin. The SVG annotation margin is outside the logical canvas and is not part of the UI.

| Region | X | Y | Width | Height |
|---|---:|---:|---:|---:|
| Popup | 235 | 245 | 380 | 210 |
| Host | 275 | 287 | 300 | 32 |
| Join | 275 | 332 | 300 | 32 |
| Back | 275 | 377 | 300 | 32 |

The title strip retains its existing height and inset. The action column has 40 logical px side clearance. The container shrinks; text and input targets do not shrink. Fixed allocation is sufficient for the three approved captions. Existing unavailable-action feedback must stay within unused popup body clearance without overlapping actions or the title.

Supported desktop clients use the existing scale `min(1.35, clientWidth/850, clientHeight/700)`. The minimum supported menu client is 850 by 700 px. There is no mobile layout or new viewport class. The underlying menu retains its existing layout; the SVG shows contextual groups, not a new MENU-01 specification.

## Navigation and input

The functional contract owns outcomes. Present Host as `Host`, the former Browse sessions action as `Join`, and return to MENU-01 as `Back`. Join opens existing NET-10 and invokes its existing current-results request. Direct connect remains in NET-10. The popup must not add endpoint fields or alter NET-10/NET-03 tasks.

Enter, Space, or controller Confirm activates the focused action. Escape or controller Back performs the existing Back outcome. Clicking outside the popup does not activate the underlying menu or add a new dismissal outcome. Returning from Host or the browser shows the same popup with Host focused. Closing it restores the unchanged main menu. Local Play remains outside the network flow.

## Acceptance and evidence

SS-015 must show the compact popup over the actual retained main menu, with only three framed actions and Host's visible outer focus keyline. The former scope copy must be absent. Banner/version and unobscured main-menu groups must remain in their original positions. The popup must not replace the main menu with a background-only replica. A still does not prove input isolation or destination behavior.

Focused QA must check keyboard, controller, and pointer activation; all three destinations; outside-click isolation; Local Play shortcut isolation; repeated entry; and return transitions. Check containment at 850 by 700 and 1280 by 720 clients. Check scaled pointer alignment at 1920 by 1080. These checks add no permanent representatives unless an approved materially different presentation is needed.

Preserve corrected macOS pointer mapping and full-drawable rendering from PR #110 and PR #119. Native Mac visual conformance remains deferred, not passed, under the [coordinated matrix](../screenshots/README.md#macos-deferred-coverage). Automated evidence is not native visual assessment.
