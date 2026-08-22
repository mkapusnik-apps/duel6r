# Duel 6 Reloaded Visual Design System

## Canonical status

This file is the canonical visual design system for Duel 6 Reloaded.
Screen-specific requirements are in [`docs/screens`](screens/README.md).
Screenshot evidence is in [`docs/screenshots`](screenshots/README.md).
The root [`DESIGN.md`](../DESIGN.md) is a pointer to this file and is not a second source of truth.

The current native implementation is the source of truth for this baseline.
This baseline describes the presentation at branch `feature-documentation-audit-fixes`, capture source SHA `12cd6dca742b90293f552fefa3bfd3a8871aa7a2`.
The audit date is 2026-08-18.

## Visual principles

- The interface must preserve the compact desktop-game presentation.
- The interface must keep the arena visible when it presents live status information.
- The interface must use direct labels and immediate visual feedback.
- The interface must preserve player and team identity during fast play.
- The interface must use text, position, shape, or motion with color when the implementation provides these cues.
- New documentation must describe implemented behavior and must not invent a replacement style.

## Coordinate and viewport conventions

- Screen-space coordinates use the bottom-left origin in renderer code.
- Wireframes use the top-left origin for reading convenience.
- A wireframe note must identify any important bottom-left renderer position.
- The release build must use the current display width and height in exclusive full-screen mode.
- The debug build must use a 1280 by 900 window.
- The menu must center its fixed 850 by 700 logical canvas in the current client area.
- The menu background must fill the complete client area with `#C0C0C0`.
- The menu must not reflow its internal controls for narrow or wide displays.
- The gameplay renderer must fill the current client area.
- Full-screen gameplay must give each player the same complete client-area view.
- Split-screen gameplay must use the implemented half-screen view rectangles and 4 px red separators.
- The implementation does not define a mobile layout.
- Documentation must not claim mobile support until the implementation defines a mobile viewport and input model.
- The implementation does not add letterboxing to the menu or the arena.
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
| `menu-surface` | `#C0C0C0` | Complete menu background |
| `menu-label-surface` | `#AAAAAA` | Label strip background |
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
| `winner-curtain` | animated `rgba(128,0,0,0..0.78)` | Full-screen round-end curtain |
| `dead-view-curtain` | `rgba(255,0,0,0.50)` | Dead split-screen player view |
| `split-divider` | `#FF0000` | Split-screen gutters |
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
- Gameplay overlays must use flat translucent fills without drop shadows.
- The score summary must use two translucent rectangular layers and a solid blue heading strip.
- New documentation must not specify rounded corners, shadows, gradients, or blur that the implementation does not provide.

## Imagery and assets

- The menu must use the animated stack at `resources/textures/menu/` as its banner source.
- The menu banner must render at 200 by 95 px near the upper center of the client area.
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
- A person row must support double-click to add the person to the player roster.
- A player row must support double-click to remove the player from the roster.
- A spinner must use left and right triangle buttons.
- A checkbox must reverse its frame when it is checked.
- A focused text field must append an underscore to its text.
- Only one text field must have focus at a time.
- The person-name field must accept only its implemented character set.
- The rounds field must accept digits only.
- The menu has no implemented disabled style.
- Invalid actions may produce no visible change unless a blocking message is documented for that action.

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
- Team identity must also change headband, trousers, and hair-top colors.
- Predator identity must use a body alpha of 0.1 while the weapon remains visible.

### Blocking menu messages

- A blocking menu message must use a centered 20 px high panel.
- The panel width must equal eight times the message length plus 60 px.
- The panel must use a 2 px black frame.
- Confirmation copy must include its implemented keyboard choices.
- Controller detection must remain open until an accepted control input is detected.
- The one-player validation message must remain open until any event is received.

### Console

- The backquote key must toggle the console over the current menu or gameplay frame.
- The console must take keyboard and text input while it is open.
- The console must span the complete client width.
- The console height must contain 15 history rows, one separator row, one input row, and its lower edge.
- The console must sit against the top edge of the visible client area.
- The console must use `=` for the separator and `^` when history is scrolled.
- The input prompt must use `]` or `<` when the input is horizontally scrolled.

## Accessibility presentation

- Documentation must identify keyboard-only actions and mouse-only actions.
- Visible shortcut labels such as `F1`, `F3`, and `ESC` must remain in button captions.
- A confirmation must show `Y/N` in its message.
- Team names must accompany team colors in rankings.
- Player names must accompany player color and status cues when their indicators are visible.
- Living and dead ranking entries use both state-dependent text color and continued row placement.
- Split-screen player regions must use spatial separation and red boundaries.
- A dead split-screen view must use a translucent red curtain in addition to the absent live state.
- Status bars currently rely on color and fill length without text labels.
- Team apparel currently relies on color during direct arena play.
- The implementation has no documented focus traversal, focus ring, screen reader output, reduced-motion mode, high-contrast mode, or text scaling mode.
- Screenshot evidence must not claim support for an accessibility mode that the implementation does not provide.

## Responsive behavior

- The menu must remain a centered fixed canvas on supported desktop display sizes.
- The gameplay camera must use the current client dimensions.
- Two-player split-screen must place Player 2 in the upper centered view and Player 1 in the lower centered view.
- Three-player split-screen must place Player 3 in the upper centered view and Players 1 and 2 in the lower row.
- Four-player split-screen must place Players 3 and 4 in the upper row and Players 1 and 2 in the lower row.
- Split-screen must be available only when the match has fewer than five players.
- More than four players must use the full-screen arena presentation.
- Overlay panels must calculate their horizontal and vertical centers from the current client dimensions where the source does so.

## Freshness and change control

- A visual-impact change must update this file when it changes a shared rule.
- A visual-impact change must update each affected screen specification and wireframe.
- A visual-impact change must invalidate each affected screenshot entry.
- A shared token or component change must trigger an assessment of all screens.
- Screenshot provenance must record branch, source SHA, environment, workflow, state, viewport, and artifact path.
- The implementation source remains authoritative when a documented value conflicts with the reviewed baseline.

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

- Project title: `Duel 6`.
- Product scope: Dedicated Duel 6 Reloaded visual workspace.
- Project ID: `1219346282527961142`.
- Project URL: [Duel 6 Stitch project](https://stitch.withgoogle.com/projects/1219346282527961142).
- Owner scope: The currently authenticated Stitch account is the owner.
- Access scope: The project is private.
- Collaborator scope: This synchronization does not request an additional collaborator.
- Design system: `Duel 6 Reloaded — Native Baseline` (`assets/7257739717738214874`).
- Synchronization date: 2026-08-22.
- Verification status: Blocked.

The available Stitch project API does not provide a project-title update operation.
The project title therefore remains `Duel 6`.
The user accepts this existing project as the dedicated product workspace.
The project is private and owner-only access is the intended final scope.

The final Stitch screen listing returns three screen records and no duplicate stable screen ID.
Eleven required screen records remain unavailable after individual generation requests and completion checks.
The active project theme reports an unapproved dark glass style with cyan accents.
The active project theme does not conform to this native baseline.
A baseline design-system application was requested twice for every available screen instance.
Stitch timed out and still reports Aura Kinetic as the active project theme.
The project must confirm the baseline design system before coverage can pass review.

Manual Stitch UI action is required if the asynchronous operations do not appear later.
The owner must rename the project to `Duel 6 Reloaded` when the UI supports project-title editing.
The owner must set `Duel 6 Reloaded — Native Baseline` as the project design system and remove the Aura Kinetic design-system instance.
The owner must create only the 11 pending stable screen IDs in the screen inventory.
The owner must edit `MENU-01` and `PLAY-02` in place against their canonical wireframes.
The owner must not create replacement records for those two existing IDs.

The Stitch workspace must preserve the documented native baseline.
The workspace must contain one representative artifact for each of the 14 authoritative wireframes.
The artifact name must start with the stable screen ID.
The artifact notes must contain applicable variants and workflows.
Stitch artifacts must not replace implementation screenshot evidence.
The local screen inventory records the required stable artifact identifiers.
The mapping must not be treated as complete until Stitch returns one screen record for each identifier.
