# Duel 6 Reloaded Visual Design System

## Canonical status

This file is the canonical visual design system for Duel 6 Reloaded.
Screen-specific requirements are in [`docs/screens`](screens/README.md).
Screenshot evidence is in [`docs/screenshots`](screenshots/README.md).
The root [`DESIGN.md`](../DESIGN.md) is a pointer to this file and is not a second source of truth.

The approved product requirements are the source of truth for visual-impact changes.
The current native implementation remains the source for unchanged visual details.
This target baseline includes the shared arena view requirements, the retro menu layout approved on 2026-08-23, the scaled photographic menu presentation approved on 2026-08-26, the consolidated main-menu Persons list specified in `SET-048`–`SET-072`, and the planned first-release network UI defined for issue #28.
The network additions are target specifications for downstream issue #38 and are not implemented UI or evidence of playable networking.
Issue #30 may implement protocol, command-line, or scaffold outcomes, but it must not add graphical network UI.

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
- The separator treatment must apply only to the active-round Tab scoreboard and the non-final post-round interim scoreboard.
- The separator treatment must support two through four teams.
- The separator treatment must not change team colors, team names, row colors, score columns, ranking order, row alignment, controls, or round-progress behavior.
- A non-Team score overview and the final game summary must remain unchanged.
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
- A missing person profile must fall back to random player colors and default player sounds.
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
- Final network results must show the exact label `Session only` near the summary heading or result table.
- Target network UI must not offer discovery, matchmaking, Internet, NAT traversal, accounts, passwords, dedicated servers, join-in-progress, or host migration.

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
- A disabled Retry control must show a persistent textual reason and must not receive focus.
- Unsupported actions must be absent rather than represented by ambiguous disabled affordances.

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
- Eleven screenshot entries remain `Planned` until issue #38 implements and captures `MENU-02`, `CONS-01`, and `NET-01`–`NET-09`.
- `SS-001` and `SS-024` must represent the two approved `MENU-01` conditional-layout wireframes.
- `SS-001` and `SS-024` use the approved 50:50 Persons and Players panel geometry at exact PR #58 head `cb9cc3ee3081b399c2d042ee62e1f7b0cc016ac0`.
- The replacement `SS-001` and `SS-024` artifacts do not conform because the person-name field extends outside the Persons panel and into the Players region.
- Issue #38 must invalidate and recapture `SS-001` and `SS-024` when it implements the target Network footer.

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

The corresponding Stitch project is `projects/1219346282527961142` (`Duel 6 Reloaded`).
For PR #54 and subsequent assessment of this approved change, Stitch is a supplementary visual workspace.
`docs/features.md`, this file, the applicable screen specifications, the version-controlled wireframes, and conforming implementation screenshots are authoritative.
A missing or stale Stitch representation must not replace or weaken any local design, wireframe, screenshot, provenance, or implementation-presentation gate.
A Stitch synchronization failure does not block visual acceptance when all authoritative local sources exist, remain current, and conform.
The project includes two `MENU-01 — Main menu and session setup` explorations at screens `681ae093051749fd922ab74454f47121` and `e26294cba3d946a0af458bcf33c275a0`.
Screen `681ae093051749fd922ab74454f47121` is a historical visual reference for the retro grey canvas, compact density, score table, and footer actions.
Its black matte and four-panel hierarchy are stale and must not override the current scaled background or three-panel `MENU-01` wireframes.
The application behavior and copy in `docs/features.md` override illustrative Stitch names, statistics, settings, version text, and shortcut syntax.
The project also includes screens for Predator, Team gameplay, sudden death, and the score overlay.
An alignment edit was requested for those four screens on 2026-08-23.
The Stitch request timed out, so the screen update result is not confirmed.
The Stitch design system uses an exploratory dark tactical style that does not match this native visual baseline.
The retro `MENU-01` screen direction is an approved screen-specific exception to that exploratory design system.
This file and `docs/features.md` remain authoritative for implementation details that the Stitch samples do not represent accurately.
The project and screen inventory were reviewed again on 2026-09-01 for the Team score-overview grouping change.
Stitch screen `172c3e16a6424bf1a7d95723038f3e43` is `OVER-01 — Score-tab overlay`.
Stitch screen `46c697bc75274ba9a668b0641e077dc0` is `OVER-02 — Round-over summary`.
A separator-treatment edit was requested for both screens on 2026-09-01.
The request preserved the native overlay, four Team groups, score content, alignment, `OVER-02` progress row, and curtain behavior.
The request specified an 8 px boundary band with a centered 2 px white rule at 70% opacity.
The Stitch request timed out, so the screen update result is not confirmed.
The local specifications and wireframes remain the implementation target.
Two consolidated Teams variants were requested from screen `681ae093051749fd922ab74454f47121` on 2026-08-31.
An inspection after the timeout found no generated consolidated Teams variants in the project screen inventory.
The two existing `MENU-01` explorations remain `681ae093051749fd922ab74454f47121` and `e26294cba3d946a0af458bcf33c275a0`.
The local screen specification and wireframes are complete and remain sufficient for implementation and visual assessment.
The stale Stitch screens are a documented non-blocking limitation for PR #54.
