# Duel 6 Reloaded Visual Design System

## Canonical status

This file is the canonical visual design system for Duel 6 Reloaded.
Screen-specific requirements are in [`docs/screens`](screens/README.md).
Screenshot evidence is in [`docs/screenshots`](screenshots/README.md).
The root [`DESIGN.md`](../DESIGN.md) is a pointer to this file and is not a second source of truth.

The current NET-01 through NET-10 visual requirements are in [Unified network presentation](#unified-network-presentation). The [network capture matrix and assessment](design/screenshots/README.md#network-presentation-current-capture-matrix) own the current status of all 16 affected representatives. Earlier network acceptance below and in historical packets applies only to the earlier presentation.

The approved product requirements are the source of truth for visual-impact changes.
The current native implementation remains the source for unchanged visual details.
The fixed product baseline is the current product-owned `docs/features.md` content.
This baseline includes the shared arena view requirements, the retro menu layout approved on 2026-08-23, the scaled photographic menu presentation approved on 2026-08-26, the consolidated main-menu Persons list specified in `SET-048`–`SET-072`, the Equalize and Shuffle behavior specified in `SET-017`–`SET-019` and `SET-073`–`SET-077`, the person-action alignment specified in `SET-078`–`SET-083`, the person-list and action-button refinement specified in `SET-084`–`SET-091`, the final limited Deathmatch and Team game summary specified in `UI-GAME-001`–`UI-GAME-007`, and the first-release network UI defined for issue #28.
Issue #38 implements the network graphical states. Its earlier acceptance is historical for affected network-arena evidence. The [current screenshot manifest](screenshots/README.md#network-arena-orientation--current-coverage) accepts the corrected PR #87 orientation representatives `SS-019`, `SS-021`, `SS-023`, and `SS-026` at checkpoint `84f63bc6fdee75444b06723b3c5f542546e27b15`; other entries retain their current status. The manifest owns artifact hashes and assessment limits.
This visual acceptance does not establish parent issue #27 release readiness.
Issue #35 implements presentation-ready responsiveness, correction, degraded-state, resynchronization, and recovery handoffs without a graphical consumer.
Issue #35 does not change a rendered application screen, layout, or graphical state.
Issue #30 may implement protocol, command-line, or scaffold outcomes, but it must not add graphical network UI.
Issue #32 defines authoritative headless match states, result data, and fixed outcome copy for the planned network screens.
Issue #32 must not add graphical network UI.
Issue #34 defines stable replicated identities and presentation-independent result-state replication.
Issue #34 must not add graphical network UI.
Issue #38 owns the graphical consumption, presentation, accessibility, and visual evidence for the replicated states.

## Visual principles

- The interface must preserve the compact desktop-game presentation.
- The interface must keep the arena visible when it presents live status information.
- The interface must use direct labels and immediate visual feedback.
- The interface must preserve player and team identity during fast play.
- The interface must use text, position, shape, or motion with color when the implementation provides these cues.
- New documentation must distinguish implemented behavior from approved target behavior and must not invent a replacement style.

## Coordinate and viewport conventions

- Screen-space coordinates use the bottom-left origin in renderer code.
- Wireframes use the top-left origin for reading convenience.
- A wireframe note must identify any important bottom-left renderer position.
- The release build must use the current display width and height in exclusive full-screen mode.
- The debug build must use a 1280 by 900 window.
- The menu must preserve its fixed 850 by 700 logical canvas and uniformly scale it by `min(1.35, clientWidth/850, clientHeight/700)`.
- The menu must center the scaled canvas in the current client area.
- An 850 by 700 client is the compatibility floor and renders the canvas at 100%.
- A 1280 by 720 client is the modern evaluation minimum.
- A 1920 by 1080 or larger client uses the 135% scale cap.
- The complete client area behind the canvas must show the session-selected menu background, or solid black after all eligible images fail to load.
- The 850 by 700 menu canvas must use `#C0C0C0`.
- The menu must not reflow its internal controls for narrow or wide displays.
- The gameplay renderer must fill the current client area.
- Gameplay must use one undivided arena view for each match.
- The shared arena view must show the whole level and all players.
- The shared arena view must support two through 15 players.
- Each game mode and player count must use the same client-area view model.
- The gameplay view must not contain player-specific camera regions or camera separators.
- The implementation does not define a mobile layout.
- Documentation must not claim mobile support until the implementation defines a mobile viewport and input model.
- The menu canvas must have a 2-logical-pixel black perimeter keyline.
- The gameplay renderer must not add letterboxing.
- A capture must show the complete client area without external window chrome unless the environment requires windowed debug mode.

## Typography

- The application must load `resources/data/font.ttf` through SDL_ttf.
- The application must rasterize the loaded typeface from a 32 px source size.
- Standard menu, console, message, ranking, counter, and version text must use the same loaded typeface.
- Standard UI text must render at a 16 px character height unless the source specifies another size.
- Standard text measurement must use an effective character width of one half of the text height.
- Round and game score summaries must use a 32 px character height.
- In-world player labels and ammunition values must use the world-space 0.3 unit text height.
- Text must keep the exact capitalization and abbreviations that the implementation supplies.
- A missing font is a fatal resource error in the current implementation.

## Color tokens

The following values come from renderer and GUI source.

| Token | Value | Implemented use |
|---|---:|---|
| `menu-background-scrim` | `rgba(0,0,0,0.55)` | Full-client layer over the blurred menu gameplay still |
| `menu-keyline` | `#000000` | 2-logical-pixel perimeter around the scaled menu canvas |
| `menu-surface` | `#C0C0C0` | Fixed menu canvas and control surfaces |
| `menu-label-surface` | `#AAAAAA` | Label strip background |
| `menu-panel-header` | `#0000C8` | Setup panel title strips |
| `menu-panel-header-text` | `#FFFFFF` | Setup panel title text |
| `field-surface` | `#FFFFFF` | List, spinner, and text field surface |
| `text-default` | `#000000` | Menu and console text |
| `frame-light` | `#EBEBEB` | Raised top and left edges |
| `frame-dark` | `#000000` | Raised bottom and right edges |
| `selection` | `#0000C8` | Selected list row |
| `selection-text` | `#FFFFFF` | Selected list text |
| `message-error-surface` | `#FFCCCC` | Menu blocking message |
| `message-error-text` | `#FF0000` | Menu blocking message text |
| `ranking-surface` | `rgba(0,0,255,0.70)` | Free-for-all ranking row |
| `ranking-live-text` | `#FFFF00` | Living player ranking text |
| `ranking-dead-text` | `#FF0000` | Dead player ranking text |
| `info-surface` | `rgba(0,0,255,0.70)` | Gameplay event message |
| `info-text` | `#FFFF00` | Gameplay event message text |
| `summary-outer` | `rgba(255,255,255,0.31)` | Score summary outer panel |
| `summary-inner` | `rgba(0,0,255,0.31)` | Score summary inner panel |
| `summary-header` | `#0000FF` | Score summary heading strip |
| `team-group-separator` | `rgba(255,255,255,0.70)` | Rule between adjacent team groups in Team score overviews |
| `winner-curtain` | animated `rgba(128,0,0,0..0.78)` | Full-screen round-end curtain |
| `console-surface` | `#EEDD00` | Console panel |
| `console-separator` | `#FF0000` | Console separator text |
| `console-edge` | `#000000` | Console lower edge |
| `team-alpha` | `#FF0000` | Alpha team identity |
| `team-bravo` | `#00FF00` | Bravo team identity |
| `team-charlie` | `#FFFF00` | Charlie team identity |
| `team-delta` | `#FF00FF` | Delta team identity |

## Spacing, shape, density, and elevation

- Menu controls must use integer pixel positions on the 850 by 700 logical canvas.
- Standard list rows must use a 16 px or 18 px row height as defined by the screen specification.
- Standard buttons must use square corners.
- Standard fields and lists must use square corners.
- Controls must use the implemented two-line light and dark frame.
- A pressed control must reverse the light and dark frame and must offset its caption by 1 px.
- The menu must use compact control spacing and must not add decorative whitespace.
- The main menu must use three raised panel groups for Persons, Players, and Game Settings.
- The Persons and Players panels must split their combined logical region equally.
- The Persons panel must use `x=10–324` and a width of 315 logical px.
- The Players panel must use `x=330–644` and a width of 315 logical px.
- The Game Settings panel must remain at `x=650–839` and a width of 190 logical px.
- The setup row must keep the 5-logical-pixel gap between Persons and Players.
- The setup row must keep the 5-logical-pixel gap between Players and Game Settings.
- Each setup panel must use the blue panel header and white panel header text.
- The Players panel must keep each player next to that player's control assignment.
- Gameplay overlays must use flat translucent fills without drop shadows.
- The score summary must use two translucent rectangular layers and a solid blue heading strip.
- A non-final limited-round summary must show its round-progress label in a dedicated row above the score heading strip.
- The round-progress label must use the score-summary type and the score-summary heading-strip text color.
- The round-progress label must align to the top-right of the score panel.
- The right edge of the round-progress label must be 16 px inside the right bound of the translucent outer panel.
- The progress row must start 32 px below the top bound of the translucent outer panel.
- The score heading must remain aligned to the horizontal center of the score panel.
- The progress row must use a 32 px row height.
- The progress row must not use the solid blue fill of the score heading strip.
- The score panel must keep the progress row and the score heading strip separate and legible.
- A Team score overview must keep each team row directly adjacent to that team's nested player rows.
- A Team score overview must use an 8 px separator band between adjacent team groups.
- The separator band must contain a 2 px horizontal `team-group-separator` rule at its vertical center.
- The separator rule must span the score-table width.
- The separator band must use 3 px of clear inner-panel space above and below the rule.
- A Team score overview must not add a separator band after the last team group.
- The separator treatment must apply to the active-round Tab scoreboard, the non-final post-round Team summary, and the final limited Team summary.
- The separator treatment must support two through four teams.
- The separator treatment must not change team colors, team names, row colors, score columns, ranking order, row alignment, controls, or round-progress behavior.
- A non-Team score overview must remain unchanged.
- A final limited Deathmatch or Team summary must show `End of Game` in a dedicated notice region at the bottom of the client.
- The final-state notice must use white 32 px score-summary text on a solid `summary-header` surface.
- The final-state notice must use at least 16 px of horizontal text padding and 8 px of vertical text padding.
- The final-state notice must align to the horizontal center of the client.
- The bottom edge of the final-state notice must be 16 px from the bottom client edge.
- The final-state notice must keep at least 16 px of clear space from the final score panel.
- The final-state notice must not overlap, clip, cover, replace, or reduce the final score content.
- A final limited Deathmatch or Team summary must keep the final round counter in the top-center counter region.
- The final round counter must use the same 32 px character height as the score heading.
- The final round counter must use white text on its existing opaque black backing.
- The final round counter backing must grow from the measured counter width and must keep visible inner space on each side of the text.
- The final round counter backing must contain every counter character inside its visible bounds.
- The layout must first use the available middle height to keep at least 16 px of clear space between the final round counter backing and the final score panel.
- A score panel that fits in the available middle height must not overlap the final round counter backing.
- A score panel that is taller than the available middle height may overlap only the final round counter backing.
- An oversized score panel must use the minimum backing overlap needed after the layout uses the complete available middle height.
- An oversized score panel must not obscure, clip, or reduce the contrast of the final round counter text or final score content.
- The final round counter text and every score heading, row, value, and separator must remain readable when backing surfaces overlap.
- The final round counter text, score content, and bottom notice must remain inside the client area.
- New documentation must not specify rounded corners, shadows, or gradients that the implementation does not provide. Blur is reserved for the approved full-client menu background.

## Imagery and assets

- The menu must use the animated stack at `resources/textures/menu/` as its banner source.
- The menu banner must render at 200 by 95 px near the upper center of the menu canvas.
- The menu must show the runtime application version with the banner.
- The menu must choose one eligible still from `resources/textures/menu-backgrounds/` with equal probability when the menu first initializes.
- The selected still must remain unchanged for the application session, including menu navigation and returns from gameplay.
- The still must fill the complete client with a centered aspect-ratio-preserving cover crop and no distortion.
- The rendered still must use a Gaussian-equivalent blur near sigma 12 px with a sampling radius of at least 24 px at client resolution, followed by the 55% black scrim.
- The grey canvas, its controls, and its keyline must remain unblurred and undimmed.
- A failed still must cause an untried eligible still to be attempted without an error dialog. Exhausting all eligible stills must fall back to solid black without blocking menu initialization.
- The selected background filename must be available in non-user-facing startup diagnostics.
- The menu must not use a version value, person name, score value, or copyright line from a Stitch sample.
- Gameplay must use the indexed images in `resources/textures/backgrounds/` behind level geometry.
- Gameplay must use level geometry from `resources/levels/*.json` and block definitions from `resources/data/blocks.json`.
- Gameplay must use `resources/textures/blocks/`, `resources/textures/man/`, `resources/textures/weapon/`, `resources/textures/bonus/`, and `resources/textures/elevator/` for visible world objects.
- The game must preserve nearest or linear filtering choices from each loader call.
- A missing required texture, level, or font may stop initialization in the current implementation.
- In Local Play, a missing person profile must fall back to random player colors and default player sounds.
- Documentation must not define a visual placeholder for a missing required world asset because the implementation has no visual placeholder.

## Motion and temporal feedback

- The menu banner may animate through its texture stack.
- A pressed button must move its caption by 1 px until release.
- A held spinner arrow must repeat after the implemented wait interval.
- The round start must fade the arena from a dark blue tint to full color.
- A yellow spiked ring must expand around each player during the initial location period.
- Timed indicators must fade according to their indicator alpha.
- Invulnerability must use a moving ring of red points around the player.
- Sudden death must raise the water in discrete timed steps.
- The full-screen round end must fade in a dark red curtain.
- The console cursor must blink between visible and hidden states.
- Documentation must not add transition durations that source constants do not expose in the reviewed files.

## Components and interaction states

### Menu controls

- A button must show raised and pressed frame states.
- A list must show the selected row with a blue fill and white text.
- A list must support wheel scrolling when the pointer is inside the list.
- The main-menu Persons list must use one row for each saved person.
- The main-menu Persons list must use the columns `Rank`, `Name`, `Elo`, and `Trend`.
- A ranked person row must show all four values.
- An unranked person row must show the name and must leave `Rank`, `Elo`, and `Trend` empty.
- A roster member must remain visible and selectable in the Persons list.
- The Persons list must not use a separate color or disabled treatment for a roster member.
- The Players list must show roster membership separately.
- A person row must support double-click to add the person to the player roster when the person is not already in the roster.
- A double-click on a roster member in the Persons list must make no visible change.
- A player row must support double-click to remove the player from the roster.
- A spinner must use left and right triangle buttons.
- A checkbox must reverse its frame when it is checked.
- The game mode spinner must show `Deathmatch`, `Predator`, and `Teams`.
- The Players panel must show `Equalize` and `Shuffle` only when `Teams` is selected.
- The Players panel must provide active interaction targets for `Equalize` and `Shuffle` only when `Teams` is selected.
- The Players panel must hide `Equalize` and `Shuffle` when `Deathmatch` or `Predator` is selected.
- A hidden roster-order control must not have an interaction target.
- The visibility of both roster-order controls must update immediately when the selected mode changes.
- `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and `Detect All` must use one common button height.
- Each of these button captions must have visible space from its border on all sides.
- The batch controller-detection action must use the caption `Detect All`.
- Each row-level controller-detection action must use the caption `D`.
- The game mode spinner must show `Teams` one time.
- The Game Settings panel must show `Num. of Team` and `Friendly Fire` only when `Teams` is selected.
- Conditional settings must stay inside the existing Game Settings panel bounds.
- Controls below a hidden conditional group must move up to keep one compact vertical stack.
- The roster must use the applicable team colors only when `Teams` is selected.
- A change to the team count must update the roster colors immediately.
- A non-Team mode must use the standard roster row colors.
- Player text and selection feedback must remain readable over each roster team color.
- A focused text field must append an underscore to its text.
- Only one text field must have focus at a time.
- The person-name field must accept only its implemented character set.
- The Rounds field must accept digits only.
- The Rounds field must show `0` at application startup unless a startup setting overrides it.
- The application must keep the applied Rounds value during the current application session.
- The Rounds field must show the applied value when gameplay returns to the menu.
- The application must not restore a Rounds value from an earlier application session.
- Focus must clear the Rounds field immediately when the field shows exactly `0`.
- Focus must keep the Rounds field value unchanged when the field shows a positive value.
- The focused empty Rounds field must show only the standard focus underscore.
- Focus loss from an empty Rounds field must show `0` and set unlimited-round semantics.
- Focus loss from a non-empty Rounds field must not apply the edit.
- Enter and Play must retain their existing Rounds application behavior.
- The menu has no implemented disabled style.
- Invalid actions may produce no visible change unless a blocking message is documented for that action.

### Target network controls and status

#### Unified network presentation

This section owns the approved network visual baseline. `UX-NET-*` identifiers define presentation only. They apply to NET-01 through NET-10, including contextual panels and confirmations. The native MENU-01 is the appearance reference; its local actions and input behavior are not network requirements. The current [network functional contracts](screens/README.md) and [directory contract](network-host-directory.md) retain authority over behavior, copy, permissions, and transitions. Their explicit directory, password, public-address, and round-one-admission updates supersede older exclusions below.

The existing screen documents and wireframes retain their identities and locations. The [UX index](design/README.md#screen-specifications) identifies the owning presentation sections. The [current capture matrix](design/screenshots/README.md#network-presentation-current-capture-matrix) supersedes older representative selections for this visual change. Legacy diagrams define content relationships, not production colors or exact pixel geometry. No legacy migration is authorized.

These numbered requirements take precedence over conflicting older appearance examples only. They do not override functional contracts. `Must` identifies acceptance requirements; `should` identifies a recommendation; `may` identifies a permitted presentation choice.

- **UX-NET-001** Menu-context network screens must retain the existing centered 850 by 700 canvas, scale cap, background session, banner, runtime version, and perimeter keyline.
- **UX-NET-002** Menu-context content must remain inside the 24-logical-pixel side and bottom margins below the existing banner and version.
- **UX-NET-003** Each primary menu panel must use `menu-surface` instead of a separate pale inner-card surface.
- **UX-NET-004** Each named content group must use the existing two-line raised frame and an 18-logical-pixel `menu-panel-header` strip with `menu-panel-header-text`.
- **UX-NET-005** Panel headings must keep at least 4 logical px of horizontal inner space from the frame.
- **UX-NET-006** Editable fields and selectable list bodies must use `field-surface` with an inset light-and-dark frame before they receive focus.
- **UX-NET-007** Table column headings must use `menu-label-surface` and remain visible while rows scroll.
- **UX-NET-008** Selected list rows must use `selection` and `selection-text` with the existing textual selection marker where one is provided.
- **UX-NET-009** Every existing actionable control must have a persistent square button or field boundary without requiring focus or pointer hover, except NET-04-R roster reordering, whose persistent capability indication must follow UX-NET-04-008.
- **UX-NET-010** Enabled buttons must use the MENU-01 raised frame, `menu-surface`, and centered `text-default` captions.
- **UX-NET-011** A pressed button must reverse its frame and offset its caption by 1 logical px without changing activation timing or repeat behavior.
- **UX-NET-012** Every focused control, including a disabled control focused by baseline traversal or state retention, must use a continuous 2-logical-pixel black outer keyline without replacing its normal or disabled surface with a different focus color.
- **UX-NET-013** A disabled action must use a flat frame, readable black text, and its persistent existing disabled reason.
- **UX-NET-014** Read-only values must retain an explicit ownership or locked-state label without an actionable raised frame.
- **UX-NET-015** A new visual arrow, checkbox, or button region must not imply an action that the functional contract and current interaction do not provide.
- **UX-NET-016** Standalone action captions must keep at least 4 logical px of clear inner space on each side at standard 16-logical-pixel text height.
- **UX-NET-017** Adjacent control bounds must keep at least 8 logical px of clear space except existing compact list-row controls, whose separate bounds and focus outlines must not overlap.
- **UX-NET-018** The displayed control bounds and pointer activation bounds must coincide after the existing canvas transform.
- **UX-NET-019** Added frames and headings must fit within the owning region without covering a value, status, adjacent focus outline, or footer.
- **UX-NET-020** A constrained body must reduce its visible row count before it reduces text size or moves fixed status and actions off screen.
- **UX-NET-021** Each list or result viewport must contain long values without splitting UTF-8 characters and expose only the scrolling and position feedback supported in its current interaction context.
- **UX-NET-022** Contextual panels must use the same grey surface, blue heading strip, and framed actions while retaining their existing client-relative placement and underlying context.
- **UX-NET-023** Live network status, world rendering, ranking, player indicators, and arena camera must retain their existing gameplay presentation.
- **UX-NET-024** Keyboard, controller, pointer, baseline focus traversal and retention, modal focus, and confirmation-arming semantics must remain unchanged.

Focus and availability are separate states. A disabled control that has baseline focus must retain the focus keyline, flat frame, readable label, and existing reason without accepting activation. This rule does not add disabled controls to traversal where the baseline skips them. NET-04-R must explain its existing host reorder access path without making its focus-dependent action permanently clickable. NET-05-S has no supported scroll handlers; its retained position information must remain non-actionable under UX-NET-05-008. Working navigation in NET-04-R, NET-06, and NET-05-R remains unchanged.

The standard 16 px text and 18 px list-row rhythm remain the default. Compact 20–24 px row controls must keep at least 2 logical px of caption space on each side. Existing 32–40 px primary actions may retain their size. This change does not force the local menu's 25 px roster buttons onto network footers. Fields must preserve the visible value width after their frame and padding are allocated. Main-menu bevels are a visual reference, not permission to import its pointer-release activation or held-spinner repetition.

##### Fixed visual acceptance

- **UX-NET-AC-01** Every affected representative must show the same MENU-01 panel, field, list, and button vocabulary without a pale inner-card or alternate blue-grey focus theme.
- **UX-NET-AC-02** A reviewer must distinguish an editable value, a selectable row, an enabled action, a disabled action, and a read-only value before moving focus.
- **UX-NET-AC-03** Each supported viewport must contain complete headings, current status, disabled reasons, and action captions without overlap or text-size reduction.
- **UX-NET-AC-04** Each focused control must retain a visible non-color focus cue distinct from row selection.
- **UX-NET-AC-05** Header and footer bounds must remain fixed while their owning body scrolls.
- **UX-NET-AC-06** Contextual overlays must preserve visible arena, lobby, or summary context outside their bounds.
- **UX-NET-AC-07** Styling must not expose unavailable actions or change activation, focus order, role permissions, admission, readiness, result semantics, or recovery destinations.
- **UX-NET-AC-08** All 16 wireframes in the current matrix must have one reviewed native implementation representative before visual acceptance.

##### NET-06 — Completed summary presentation

Functional authority: [NET-06](screens/network-summary.md), including NET-AC-010, NET-AC-011, and NET-AC-014–018. Structural authority: existing [NET-06 diagram](screens/wireframes/network-summary.md). This owning section specifies the changed styling and allocation without moving the legacy document. The current implementation uses a menu summary context; the diagram's older arena-background example does not require a context change.

- **UX-NET-06-001** The summary must retain its menu canvas and banner rather than introduce an arena-background transition.
- **UX-NET-06-002** The summary must present its title strip above fixed result-state, persistence, match-outcome, and last-round-outcome rows.
- **UX-NET-06-003** The fixed identity rows must remain above a white inset result viewport with separately labeled outcome, settings, round, and cumulative sections.
- **UX-NET-06-004** The result viewport must retain its sticky heading, horizontal scroll control, vertical navigation, and row/column position feedback.
- **UX-NET-06-005** The result viewport must end at least 8 logical px above the host action column or guest waiting-status region.
- **UX-NET-06-006** The host action column must retain Return to lobby before End session, while the guest footer retains waiting status and Leave.
- **UX-NET-06-007** Outcome overflow must use the existing bounded winner-count heading and complete scrollable identity rows rather than a smaller font or winner emphasis on ranking rows.

Acceptance: the Completed representative must expose outcome rows and persistent identity above the result body, with scrolling feedback and both host actions unobscured. Focused QA must check the guest footer, 14 Predator winners, departed identities, 64 UTF-8-byte names, and both scroll extremes. Confirmation uses NET-05-C's shared panel treatment without changing this screen's consequence copy or focus order.

##### NET-07 — Reconnect presentation

Functional authority: [NET-07](screens/network-reconnect.md), including NET-OWN-008–009 and NET-AC-012–013. Structural authority: existing [NET-07 diagram](screens/wireframes/network-reconnect.md).

- **UX-NET-07-001** The centered reconnect panel must retain a maximum width of 640 client px and at least 16 client px of edge clearance.
- **UX-NET-07-002** A fixed blue heading strip must precede the endpoint, positive countdown, reservation status, and context-dependent continuation copy.
- **UX-NET-07-003** The panel must keep the countdown and framed Leave session action visible while long supporting copy wraps inside the body.
- **UX-NET-07-004** The panel must retain the last confirmed context without adding a full menu canvas over an arena.
- **UX-NET-07-005** Leave confirmation must use NET-05-C's shared panel treatment with this state's existing reservation consequence.

Acceptance: a real interrupted guest connection must show a positive countdown and focused Leave session above the retained arena. Focused QA must check lobby/summary backgrounds, a long endpoint, and Leave/Cancel while the original deadline continues. A still does not prove countdown progression or deadline retention.

##### NET-09 — Intentional host-end presentation

Functional authority: [NET-09](screens/network-host-ended.md), including NET-AC-014 and NET-AC-016–019. Structural authority: existing [NET-09 diagram](screens/wireframes/network-host-ended.md).

- **UX-NET-09-001** The centered host-end panel must retain a maximum width of 640 client px and at least 16 client px of edge clearance.
- **UX-NET-09-002** The blue heading strip must use the existing HOST ENDED SESSION heading above the fixed terminal explanation.
- **UX-NET-09-003** The panel must keep its single framed Return to Network action visible below the context-dependent persistence copy.
- **UX-NET-09-004** The panel must retain the last confirmed context without adding a countdown or progress treatment.

Acceptance: an accepted intentional host End notice must produce the representative; crash, silence, timeout, or forced termination is not a substitute. Focused QA must check the lobby variant without match-result copy and the summary/reconnect contexts. Return to Network must retain its existing initial focus and supported inputs.

#### Existing network constraints

- `MENU-01`, `NET-01`–`NET-04`, and `NET-08` must use the retro 850 by 700 logical canvas and the same uniform scaling, centered presentation, photographic background, scrim, keyline, type, square controls, and compact density as the local menu.
- `NET-05`–`NET-07` may overlay the undivided shared arena or summary context where their screen specifications require it; they must not introduce player-specific viewports.
- `NET-09` must be a blocking panel over the last confirmed lobby, arena, summary, or reconnect context. It must not replace that context with a fixed 850 by 700 canvas requirement.
- Participant role, connection, readiness, and ownership must use separate textual fields or columns. Color may reinforce but must not replace `Host`, `Guest`, `Connected`, `Reconnecting`, `Ready`, or `Not ready`.
- Connection copy must be truthful: the UI must not show a lobby, listening state, successful connection, or restored session before the runtime confirms it.
- A disabled action must remain readable and must show a nearby textual reason, including the named unready participant or invalid configuration where applicable.
- Host-owned fields must be visibly read-only to guests, and participant-owned player controls must not appear editable to another participant.
- Reconnecting UI must show the positive ceiling seconds remaining from the host deadline, never active `0`, and state that active play continues when the match is active.
- Guest Leave, reconnect Leave session, and host End session actions must use consequence confirmations and the destinations defined by the product specification.
- Silence, refusal, unreachable, reset, timeout, host crash, host-machine/listener loss, temporary failure, or no response must remain guest `NET-07` through the fixed deadline; it must not be presented as host end.
- `NET-09` must use only the fixed intentional host-end copy after a valid End session notice is accepted through the current established session.
- Host-local supervised hosted-service failure must use host `NET-08` with `Hosted session stopped unexpectedly.` and must never become guest evidence.
- Host startup must show `Starting session…` while the startup attempt is active.
- Host startup must show `Startup can take up to 10 seconds.` without claiming readiness.
- Host startup must lock the retained setup and must replace Start session and Back with Cancel.
- Accepted startup Cancel must show `Cancelling session…` until cleanup completes.
- Completed startup Cancel must return to editable `NET-02` with the retained setup.
- A startup failure must keep Retry disabled until cleanup completes.
- An eligible startup Retry must start a new attempt with the retained setup.
- A post-readiness hosted-service failure must keep Retry disabled because Retry cannot restore the ended session.
- `Edit setup` must return to retained editable `NET-02` for a new host attempt.
- `Return to Network` must enter `NET-01` after cleanup.
- Only the confirmed `End session` action may produce the intentional host-end notice.
- Normal application shutdown, a crash, forced termination, and hosted-service failure must not produce or imply the intentional host-end notice.
- Release, manifest, content, admission, reconnect, and termination user copy must not include a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.
- Host-service lifecycle copy must not include an endpoint, process value, command, credential, filesystem path, payload, or operating-system error text.
- Trusted diagnostics may identify one differing path only after the application validates that path against every canonical-path rule.
- Trusted diagnostics must not include an invalid path or raw payload.
- `NET-06` must show the exact label `Session only` near the summary heading or result table.
- `NET-06` must show result state `Completed`.
- `NET-06` must show match outcome and last completed-round outcome as separate labeled values.
- A completed match outcome must equal the configured final-round outcome.
- A retained `NET-04` result must show result state `Completed` or `Interrupted`.
- An interrupted match outcome must show the exact value `No winner` in `NET-04`.
- An interrupted result must preserve the last completed-round outcome when that outcome exists.
- A cumulative ranking must remain separate from match outcome and last completed-round outcome.
- A cumulative ranking leader must not receive a champion label or treatment.
- Final network results must state `Not saved to local statistics or Elo`.
- Network match setup must expose only mode, level plan, round limit, Assistance, Quick Liquid, and Burnable Trees.
- Network round limit must accept only integers from 1 through 99.
- Network match setup must not expose weapon enablement, ammunition ranges, level data, or gameplay definitions as settings.
- Network match status must state that optional Lua and profile scripts are disabled for network play.
- Network setup must provide local person selection and local control assignment without a profile selector, profile column, or profile-editing action.
- `NET-02` must show a control labeled `Listening interface` directly after Port in the endpoint hierarchy.
- Port must keep initial focus in editable `NET-02`.
- `Listening interface` must follow Port in the keyboard and controller focus order.
- `Listening interface` must show IPv4 loopback and each currently eligible assigned private RFC1918 IPv4 address.
- The selector should show the IPv4 literal first and the scope as `Same machine` or `Private LAN`.
- The selector must not show wildcard, unspecified, public, multicast, link-local, unassigned, network, or broadcast addresses.
- The selector must select IPv4 loopback on first entry to `NET-02`.
- The interface list must show each eligible IPv4 literal once.
- The collapsed selector must keep the complete selected IPv4 literal visible.
- An expanded interface list must use one line per address.
- An expanded interface list must scroll vertically when all eligible addresses do not fit without changing the canvas or moving the split setup panels.
- A long option label must clip after the complete IPv4 literal and must not wrap or change row height.
- The selector pointer region must include the complete collapsed control and each complete visible option row.
- The selected option and selector focus must remain identifiable without color.
- Confirm must open the collapsed selector or accept the highlighted option.
- Keyboard or controller directional input must move through visible interface options while the selector is open.
- Escape or controller Back must close the expanded selector without leaving `NET-02`.
- `NET-02` must retain an eligible selected listening address through Cancel, failure, Edit setup, and eligible Retry.
- Start session must revalidate the selected listening address before startup begins.
- An ineligible retained address must leave `Listening interface` without a valid selection and keep `NET-02` editable.
- An ineligible retained address must show `Selected listening interface is no longer available. Choose another interface.`
- The application must not automatically replace an ineligible address with a private LAN address.
- Interface enumeration and selection must not discover another host or session.
- Interface enumeration and selection must not change an interface, route, firewall, Docker network, NAT rule, port-forwarding rule, or other network infrastructure.
- Network setup must let a participant add or remove local player slots only before host startup or guest Connect begins.
- Start session and Connect must lock the displayed local-player count and ordered slot set for that attempt.
- Cancel or a recoverable pre-admission failure may return to editable setup with the retained slot set.
- Successful host startup or guest admission must fix the participant's ordered player slots, identities, and ownership for the admitted lifetime.
- `NET-04` must not show or provide an action to add, remove, or transfer one player slot.
- An admitted participant may edit the person and local control only for an existing owned player slot.
- A person or control edit must retain the slot position, player identity, and owner.
- Only the host may reorder the authoritative roster.
- A host roster reorder must retain every player identity and owner.
- A person edit, control edit, or host roster reorder must clear every participant's readiness.
- Intentional participant Leave or authoritative expiry must remove every slot owned by that participant as one participant-level outcome.
- A removed player identity must not be reused during the same session.
- Reconnect must restore the same reserved participant, player slots, player identities, and ownership.
- Reconnect must not create, remove, replace, reorder, or transfer a reserved player slot.
- A network player name must identify the selected local person without implying selected-profile appearance parity.
- First-release network play must use one built-in default network visual set for every local and remote player.
- The default network visual set must provide the built-in player skin, animation mapping, and entity-resource mapping.
- The same supported release and the same complete replicated canonical state must select the same network player animation and entity visual on each client.
- Presentation must derive movement, action, entity type, and visual gameplay state from read-only replicated canonical state.
- Presentation must not create or advance a second gameplay simulation.
- A local or remote profile must not change a network player's skin, animation, or visual resource.
- The default network visual set must preserve authoritative Team colors, Predator opacity, invisibility, and each other replicated visual gameplay state.
- A network client must use the level's named background when that background is locally usable.
- A network client must use a deterministic pseudo-random built-in fallback when the level has no locally usable named background.
- The fallback mapping must use only the existing replicated session, match, round, and level logical identities and the client's stable ordered eligible background list.
- Equal logical identities and equal eligible background lists must select the same fallback background on every client of the supported release.
- A fixed identity tuple and eligible background list must keep the selected fallback unchanged across a full snapshot, equivalent incremental state, reconnect, and resynchronization.
- Controlled varied identity tuples must give every item in the eligible background list a selection opportunity.
- Fallback selection must not use or advance authoritative gameplay random state.
- Fallback selection must not add or consume a canonical background-selection field.
- Fallback selection must not change canonical state, gameplay, results, or Local Play behavior.
- Fallback selection must not load a profile background or peer content.
- Background fallback must not add visible copy, controls, status, or layout allocation.
- A missing, changed, or additional profile or cosmetic asset must not block network admission.
- A client must not load a peer-selected profile, file, or script as a visual fallback.
- A client that cannot load a required default network visual resource must not start network play.
- A required default network visual resource failure must keep the existing required-resource failure behavior.
- `NET-07` and `NET-09` must retain the default network visuals from the last complete accepted network state when they retain arena context.
- A retained arena context must not switch to a selected-profile appearance while it is non-current or blocked.
- The network arena must use the same visible axis orientation as Local Play for the same level and authoritative horizontal-mirror state.
- An unmirrored asymmetric level must not appear rotated or horizontally reversed relative to Local Play.
- An intentional horizontal level mirror must not invert the vertical axis.
- Standing players must appear head-up with their feet on the upper surface of their supporting platform.
- Trees must appear rooted below their crowns.
- Visible player names and ammunition text must remain upright and readable near their player.
- Terrain, water, entities, held weapons, effects, and player status must use consistent world placement.
- An orientation correction must preserve the complete centered arena, existing scale, clipping bounds, render order, and visible faces.
- An orientation correction must preserve background selection and background orientation.
- An orientation correction must preserve HUD placement, text wrapping, modal containment, focus order, and pointer regions.
- An orientation correction must preserve Invisibility presentation under `BON-013` and the existing Predator presentation.
- The retained arena in `NET-07` and `NET-09` must keep the same orientation as `NET-05` for the last complete accepted state.
- Selected-profile appearance parity is deferred to issue #84 and must not appear as first-release behavior or evidence.
- Only the host may show an enabled early-advance action after a round outcome exists.
- Guests must not see an enabled round-advance action.
- Network round-end presentation must distinguish the first-second active phase from the final-five-second frozen phase.
- The final round must enter the final summary and must not show a next-round action.
- An interrupted match must enter `NET-04` directly and must not enter `NET-06`.
- A completed result must appear in `NET-06` and then remain available in the following `NET-04` lobby.
- Only the host may show the `End session` action.
- The interface must keep Local Play copy, settings, advancement, scripting, and persistence behavior unchanged.
- Target network UI must not offer discovery, matchmaking, Internet, NAT traversal, accounts, passwords, dedicated servers, join-in-progress, or host migration.
- Live `NET-05` network status must use a compact, flat, translucent status region at the bottom of the client.
- The status region must use `info-surface`, `info-text`, and the standard 16 px gameplay text.
- The status region must keep a 16 px inset from the client edges.
- The status region must keep 4 px of clear inner space around its text.
- The status region must keep a minimum 16 px gap between its left and right content groups.
- The left group must show participant role, `LAN session`, and connection state.
- The right group must show `Session only scores` and `Optional scripts disabled`.
- The degraded state must add the exact persistent text `Network connection degraded.` to the left group.
- The degraded state must not use color, animation, or an icon as the only degraded cue.
- The degraded state must not dim, freeze, divide, or replace the shared arena.
- The degraded indication must not receive focus or create an interaction target.
- The degraded indication must remain above world imagery and below a blocking confirmation panel in the overlay hierarchy.
- The degraded indication must not make ranking, round progress, event text, player status, or a session action unreadable.
- Network status text must not truncate the degraded indication.
- Network status text may wrap only at word boundaries.
- A wrapped network status region must grow upward in 16 px text rows.
- A wrapped network status region must not extend beyond three text rows.
- Full resynchronization may retain the last complete accepted frame as context.
- Retained resynchronization context must show `Last confirmed state` and `Synchronizing current state…` as persistent text.
- Retained resynchronization context must not show a percentage, partial-state count, or another recovery-progress value.
- A movement correction must keep one visible sprite for the corrected player.
- A movement correction must move that sprite toward one latest accepted canonical position.
- A movement correction must not use a duplicate sprite, ghost trail, flashing marker, camera shift, or outcome effect.
- A movement correction must not change another player's visible state or an authoritative outcome.
- A menu-canvas network screen must keep a 24-logical-pixel inner margin around its primary panel.
- A menu-canvas network screen must use a fixed header region, a flexible body region, and a fixed action region.
- Adjacent network controls must keep at least 8 logical px of clear space.
- A primary network action must precede Back, Cancel, Leave, End session, Edit setup, and Return actions in reading order.
- A focused network control must add a continuous 2-logical-pixel black outer keyline outside its normal frame.
- The focus keyline must not change the control size or move adjacent content.
- A disabled network control must keep readable text, use a flat frame instead of the raised actionable frame, and show one persistent nearby reason.
- A disabled network control must not accept activation through any input method.
- A disabled network control must retain visible focus when baseline traversal or state retention gives it focus.
- A network text field must show its complete value when the value fits.
- A focused text field may scroll its text horizontally to keep the insertion position visible.
- An unfocused text field must clip an overlong value inside the field and must not draw into an adjacent region.
- A network list or table must keep its heading visible while its body scrolls vertically.
- A network list or table must not increase row height to fit a long participant or player name.
- A long participant or player value must clip inside its column.
- A horizontally wide result table with supported horizontal navigation must retain its explicit horizontal scroll control inside the result region.
- A supported scroll control must retain its existing input methods and visible position feedback.
- NET-05-S must not imply scrolling through an enabled arrow, focus target, or shortcut hint.
- A blocking network panel must keep at least 16 px between its outer edge and each client edge.
- A blocking network panel must wrap prose at word boundaries and may break an unspaced endpoint at a character boundary.
- A blocking network panel must keep its heading, current status, and primary recovery action visible when body content scrolls.

### Gameplay presentation

- The arena must keep terrain, water, sprites, elevators, pickups, players, shots, and explosions in the implemented render order.
- A live full-screen ranking must sit at the upper-right area in wireframe coordinates.
- A round counter must sit at the top center in wireframe coordinates when a round limit exists.
- An optional FPS counter must sit at the top right in wireframe coordinates.
- Event messages must stack from the upper-left area in wireframe coordinates.
- Player status must stay near the applicable player.
- Reload, air, bonus, and health must use green, blue, magenta, and red bars respectively.
- Player names must use yellow text on a blue rectangle.
- Ammunition must use blue text on a yellow rectangle.
- Round kills must use blue point marks.
- Team ranking must group named team rows and nested player rows.
- Team score-overview groups must use the defined separator treatment in `OVER-01` and non-final `OVER-02`.
- A final limited Team score overview must use the same separator treatment in `OVER-03`.
- A final limited Deathmatch or Team score overview must show the literal `End of Game` in the defined bottom notice region.
- A final limited Deathmatch or Team score overview must show its final round counter at the same character height as `---SCORE---`.
- The final round counter backing must contain the complete counter.
- The layout must separate the counter backing and score panel when the score panel fits in the available middle height.
- An oversized score panel may overlap the counter backing without obscuring counter text or score content.
- Team identity must also change headband, trousers, and hair-top colors.
- Predator identity must use a body alpha of 0.1 while the weapon remains visible.
- Live ranking must remain available for every supported player count.
- Event messages, player status, and score summaries must remain available in the shared arena view.
- Round progress must remain available in the shared arena view except while a non-final limited-round summary panel is visible.
- A non-final limited-round summary must show `Rounds: <played>|<total>` in a dedicated row above the solid blue score heading strip.
- The summary round-progress label must use the exact `Rounds: <played>|<total>` format.
- The summary round-progress label must align to the panel top-right in the dedicated progress row.
- The right edge of the summary round-progress label must be 16 px inside the right bound of the translucent outer panel.
- The summary round-progress label must include the round that has just ended in `<played>`.
- The summary round-progress label must use the configured positive round limit in `<total>`.
- A resumed match must use its accumulated played-round count in the summary round-progress label.
- An unlimited round summary must not show the summary round-progress label.
- The final game summary and the active-round Tab score overlay must not show the new summary round-progress label.
- The top-center arena round progress must be hidden while the non-final limited-round summary panel is visible.
- The top-center arena round progress must return in the first visible frame of the next active round.
- The summary popup must show only one round-count location.
- F2 must not change the gameplay view.

### Blocking menu messages

- A short blocking menu message must use a centered 20-logical-pixel high panel and remain on one line.
- A short panel width must equal eight times the message length plus 60 logical px.
- A long blocking message may wrap at word boundaries within the logical menu canvas; sentence boundaries should be preferred where practical.
- A wrapped message panel must grow vertically by one 16-logical-pixel text row per additional line.
- The panel must use a 2 px black frame.
- Confirmation copy must include its implemented keyboard choices.
- A start-prerequisite message must name each missing prerequisite in a separate sentence.
- A start-prerequisite message must tell the user to correct content or configuration and restart the application.
- A start-prerequisite message must show `Press any key.` as its dismissal instruction.
- A start-prerequisite message must keep the unchanged menu visible behind the panel.
- Controller detection must remain open until an accepted control input is detected.
- The one-player validation message must remain open until any event is received.

### Console

- The backquote key must toggle the console over the current menu or gameplay frame.
- The console must take keyboard and text input while it is open.
- The console must span the complete client width.
- The console must remain unscaled while the menu below it uses the menu presentation transform.
- The console height must contain 15 history rows, one separator row, one input row, and its lower edge.
- The console must sit against the top edge of the visible client area.
- The console must use `=` for the separator and `^` when history is scrolled.
- The input prompt must use `]` or `<` when the input is horizontally scrolled.

## Accessibility presentation

- Documentation must identify keyboard-only actions and mouse-only actions.
- Visible shortcut labels such as `F1`, `F3`, and `ESC` must remain in button captions.
- The current menu action captions use `Play (F1)`, `Clear (F3)`, and `Quit (ESC)`; the target network entry adds `Network (F2)` between Play and Clear.
- The menu action captions must not place a bracketed shortcut before the action name.
- A confirmation must show `Y/N` in its message.
- Team names must accompany team colors in rankings.
- Player names must accompany player color and status cues when their indicators are visible.
- Living and dead ranking entries use both state-dependent text color and continued row placement.
- Status bars currently rely on color and fill length without text labels.
- Team apparel currently relies on color during direct arena play.
- The implementation has no documented focus traversal, focus ring, screen reader output, reduced-motion mode, high-contrast mode, or text scaling mode.
- Screenshot evidence must not claim support for an accessibility mode that the implementation does not provide.
- Target network screens must define a deterministic keyboard and controller focus order, preserve a visible focused-control state, and allow primary, Back, Cancel, Retry, Ready, and Return actions without a mouse.
- Focus must not rely only on color, and status changes must remain as visible text rather than transient color or motion alone.
- Starting, cancelling, failure, Retry eligibility, and cleanup status must remain available as persistent text.
- A disabled Retry control must show a persistent textual reason and preserve its baseline focus behavior.
- Unsupported actions must not appear enabled; retained NET-05-S position information must follow its explicit non-actionable treatment.
- Round-end phase, automatic-advance timing, result state, match outcome, last completed-round outcome, no-winner state, script exclusion, and no-persistence status must remain visible as text.
- Result tables must use text headings for ranking criteria and values.
- Result tables must not rely only on row order or color to communicate rank, team, winner, or departed state.
- Result tables must not use rank or row emphasis to imply a match champion.

## Responsive behavior

- The menu must remain a centered, uniformly scaled fixed-layout canvas on supported desktop display sizes.
- A supported menu client area must be at least 850 by 700 px.
- The 850 by 700 logical positions, proportions, text, controls, banner, lists, score table, bevels, and interaction bounds must scale together.
- Pointer coordinates must use the inverse menu transform before GUI hit testing.
- Menu scaling must not alter the gameplay camera, world rendering, or gameplay overlays.
- The gameplay camera must use the current client dimensions.
- Every match must keep one undivided arena at each supported desktop viewport.
- The camera must keep the complete level and all active players in the shared view.
- A player count change must not create another viewport layout.
- Overlay panels must calculate their horizontal and vertical centers from the current client dimensions where the source does so.

## Freshness and change control

- A visual-impact change must update this file when it changes a shared rule.
- A visual-impact change must update each affected screen specification and wireframe.
- A visual-impact change must invalidate each affected screenshot entry.
- A shared token or component change must trigger an assessment of all screens.
- Screenshot provenance must record branch, source SHA, environment, workflow, state, viewport, and artifact path.
- Evidence for menu background selection or persistence must also record the selected filename, runtime asset manifest revision, and session identifier.
- The implementation source remains authoritative when a documented value conflicts with the reviewed baseline.
- The issue #38 entries `SS-002`, `SS-013`, and `SS-015`–`SS-023` have historical acceptance at source head `e70a057819c97100b083c3cdaae5dc24566435cd`; the current manifest governs subsequent corrections and affected-entry invalidation.
- `SS-001` and `SS-024` must represent the two approved `MENU-01` conditional-layout wireframes.
- `SS-001` and `SS-024` use the approved 50:50 Persons and Players panel geometry.
- The PR #59 `SS-001` and `SS-024` artifacts are historical because they show the prior person-action arrangement.
- The PR #60 `SS-001` and `SS-024` artifacts are historical because they show the shorter Persons list, the higher person-name row, and the previous batch controller-detection caption.
- The PR #62 `SS-001` and `SS-024` artifacts are historical because they show `<<` in the Persons panel.
- PR #69 provides the latest captured implementation screenshots for nine implemented wireframes.
- PR #70 provides the latest captured implementation screenshot for `OVER-03`.
- `SS-001` and `SS-024` represent the current four-action `MENU-01` implementation and conform at source head `e70a057819c97100b083c3cdaae5dc24566435cd`.
- `SS-003`, `SS-007`–`SS-011`, and `SS-014` represent the current implemented gameplay and overlay wireframes and conform at the same assessment head.
- The PR #70 `SS-012` replacement artifact conforms to the current `OVER-03` wireframe.
- Issue #38 recaptured `SS-001` and `SS-024` with the implemented Network footer.

## Reviewed implementation sources

- `source/Menu.cpp`
- `source/Game.cpp`
- `source/Round.cpp`
- `source/WorldRenderer.cpp`
- `source/GameSettings.cpp` and `source/GameSettings.h`
- `source/Application.cpp`
- `source/Video.cpp`
- `source/Font.cpp`
- `source/InfoMessageQueue.cpp`
- `source/console/Console.cpp` and `source/console/ConsoleRenderer.cpp`
- `source/gui/`
- `source/gamemodes/`
- `resources/data/`
- `resources/levels/`
- `resources/textures/`
- `docs/features.md`

## Stitch synchronization

The corresponding optional Stitch project is `projects/1219346282527961142`.
The project metadata reports the title `Duel 6 Reloaded`, private visibility, and the authenticated role `OWNER`.
The repository design system, screen specifications, wireframes, and implementation screenshot manifest remain authoritative.
The root `DESIGN.md` file remains a compatibility pointer.
The Stitch design system uses an exploratory dark tactical style that conflicts with the native visual baseline.
No Stitch design-system value may override a canonical repository value.
A Stitch artifact may support exploration, but it must not serve as implementation screenshot evidence.
A missing, stale, conflicting, or inaccessible Stitch artifact must not block implementation, screenshot capture, review, release, or issue completion.
The current wireframe-level mapping and its limitations are in [`docs/screens/README.md`](screens/README.md#optional-stitch-mapping).
