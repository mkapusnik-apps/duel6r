# MENU-01 — Main menu and session setup

## Purpose and traceability

The implemented screen builds the local roster, assigns controls, selects match settings, shows persistent results, starts a local match, and exits the application. The approved target adds a distinct network entry without changing local Play.
Entry occurs when the application starts or when gameplay closes.
Implemented exit occurs through `Play (F1)`, `Quit (ESC)`, or the window close action. In the target UI, `Network (F2)` enters `NET-01`.
The screen implements `SET-001`–`SET-091`, `LIF-023`–`LIF-029`, `INP-001`–`INP-011`, `SCO-019`–`SCO-024`, `PER-001`–`PER-005`, `AC-011`, `AC-040`–`AC-051`, and `AC-053`–`AC-069` from [`docs/features.md`](../features.md). The planned Network action traces to `NET-AC-002`, `NET-AC-009`, and `NET-AC-015` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
The Equalize and Shuffle behavior specifically traces to `SET-017`–`SET-019`, `SET-073`–`SET-077`, `AC-011`, and `AC-063`–`AC-064` at fixed product baseline `e75552f`.
The person-action alignment specifically traces to `SET-078`–`SET-083` and `AC-065` at fixed product baseline `e75552f`.
The person-list and action-button refinement specifically traces to `SET-084`–`SET-091` and `AC-066`–`AC-069` at fixed product baseline `88b72a6`.
Stitch screen `681ae093051749fd922ab74454f47121` in project `1219346282527961142` is supplementary historical context for the retro treatment only.
The Stitch screen does not define the consolidated Persons panel.
Behavioral sources are `source/Menu.cpp`, `source/gui/`, `source/GameSettings.cpp`, and `resources/textures/menu/`.

## Prerequisites

- The application must initialize video, font, resources, controls, and saved person data before interaction.
- Starting a match must require at least two selected players.
- Each selected player must have an available control entry.
- A start attempt must require at least one successfully loaded level.
- A start attempt must require at least one enabled weapon.
- The application must check the level and weapon prerequisites before it shows `Resume previous game? (Y/N)` or `Clear statistics? (Y/N)`.

## Layout and hierarchy

- The screen must use the uniformly scaled and centered 850 by 700 logical menu canvas from [`menu-main.md`](wireframes/menu-main.md).
- The canvas scale must be `min(1.35, clientWidth/850, clientHeight/700)` with no internal reflow.
- The complete client behind the canvas must show one session-selected gameplay still using centered cover crop, Gaussian-equivalent blur near sigma 12 px, and a 55% black scrim.
- The grey canvas must remain unblurred and undimmed inside a 2-logical-pixel black perimeter keyline.
- The upper area must contain the existing 200 by 95 px animated banner and runtime version text.
- The setup row must contain three panel groups in this order: Persons, Players, and Game Settings.
- Each setup panel must use a blue title strip with white text.
- The consolidated panel title must use exactly `PERSONS`.
- The Persons and Players panels must split their combined region 50:50.
- The Persons panel must use `x=10–324` and a width of 315 logical px.
- The Players panel must use `x=330–644` and a width of 315 logical px.
- The Game Settings panel must remain at `x=650–839` and a width of 190 logical px.
- The layout must keep a 5-logical-pixel gap between each adjacent setup panel.
- The Persons panel must contain one person list, the person-name field, `Add`, and a Persons action row.
- The person-name field and `Add` must use one row below the person list.
- `Add` must be to the right of the person-name field.
- The Persons action row must contain `Remove` and `>>`.
- The Persons action row must be below the person-name row.
- `Remove` must align to the bottom-left of the Persons panel.
- `>>` must align to the bottom-right of the Persons panel.
- The Players action row must contain `<<` and `Detect All` in every mode.
- `<<` must align to the bottom-left of the Players panel.
- `Detect All` must align to the bottom-right of the Players panel.
- The Players action row must show `Equalize` and `Shuffle` between `<<` and `Detect All` when Teams is selected.
- `Equalize` and `Shuffle` must form one horizontal group.
- The roster-order group must be horizontally centered in the available span between `<<` and `Detect All`.
- The edge controls and the roster-order group must not overlap.
- The Persons and Players action rows must use one vertical centerline.
- The action rows must keep the same position when `Equalize` and `Shuffle` are hidden.
- Each Persons control must remain inside the Persons panel bounds.
- A Persons control must not draw into a setup-panel gap or the Players region.
- The person list must use the visible columns `Rank`, `Name`, `Elo`, and `Trend` in that order.
- The Rank, Elo, and Trend columns must use stable widths.
- The Name column must use the remaining list width.
- The Rank, Elo, and Trend values must align by column.
- A person row must use one line and must not wrap.
- Text that exceeds its cell must clip at the cell boundary.
- The person list must use one vertical scroll bar and must not add horizontal scrolling.
- The person list must be the first and largest interactive element in the Persons panel.
- The person list must show one additional standard list row compared with the PR #60 layout.
- The person-name row must be one standard list row lower than in the PR #60 layout.
- The person-name field, `Add`, and Persons action row must remain below the person list.
- The Players panel must contain the selected-player list, each player's control spinner, each player's `D` action, and the `Detect All` action in every mode.
- The Players panel must show buttons labeled `Equalize` and `Shuffle` when `Teams` is selected.
- The Players panel must hide `Equalize` and `Shuffle` when `Deathmatch` or `Predator` is selected.
- The Players panel must not reserve visible control space for `Equalize` or `Shuffle` in a non-Team mode.
- The Players panel must align each control spinner and `D` action with the applicable player row.
- Each player name, control label, spinner action, and `D` action must remain readable inside the Players panel.
- `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and the batch controller-detection action must use one common button height.
- Each caption in the common-height buttons must have visible space from its border on all sides.
- The batch controller-detection action must use the caption `Detect All`.
- Each row-level controller-detection action must keep the caption `D`.
- The Game Settings panel must contain the mode spinner, Assistance checkbox, Quick Liquid checkbox, Burnable Trees checkbox, and Rounds field in every mode.
- The mode spinner must show `Deathmatch`, `Predator`, and exactly one `Teams` option.
- The Game Settings panel must show `Num. of Team` and `Friendly Fire` below the mode spinner when `Teams` is selected.
- The Game Settings panel must hide `Num. of Team` and `Friendly Fire` when `Deathmatch` or `Predator` is selected.
- The controls below the Team settings must move up when the Team settings are hidden.
- The visible controls must form one compact vertical stack without empty reserved Team rows.
- The Burnable Trees checkbox must appear directly below the Quick Liquid checkbox.
- The Rounds field must move down by one compact control row.
- The Game Settings panel bounds must not change.
- The full persistent score table must span the middle width.
- The current bottom action row contains `Play (F1)`, `Clear (F3)`, and `Quit (ESC)`.
- The target bottom action row must contain `Play (F1)`, `Network (F2)`, `Clear (F3)`, and `Quit (ESC)`.
- The four target footer buttons must have equal 150-logical-pixel widths, equal 46-logical-pixel gaps, and equal 46-logical-pixel outer margins within the `x=10–840` footer content span.
- The bottom action row must use action-first captions.
- The screen must not use `[F1] PLAY`, `[F3] CLEAR`, or `[ESC] QUIT`.
- The planned `Network (F2)` control is not implemented by issue #28 and must not be treated as current runtime behavior.

## Visible behavior and state variants

- The person list must show each saved person one time.
- The person list must include each person in the player roster.
- The person list must put ranked persons before unranked persons.
- The person list must sort ranked persons by descending Elo.
- A ranked row must show rank, name, Elo, and signed Elo trend.
- The rank must start at `01` and must follow the ranked row order.
- The person list must keep unranked persons in person-record order after the ranked rows.
- An unranked row must show the name and must leave Rank, Elo, and Trend empty.
- A roster member row must use the same default and selected row treatment as another person row.
- The separate Players panel must identify roster membership.
- The person list must not add a roster badge, duplicate row, disabled row, or team color.
- A selected person row must use the standard blue selection fill and white text across the complete row.
- The list must keep the selection on the same person after a refresh when that person still exists.
- The list must have no selected row after the selected person is deleted.
- A completed Deathmatch with `Rounds` greater than `0` must refresh rank, Elo, and trend when the match returns to the menu.
- An unlimited Deathmatch, Predator match, Team deathmatch, or early match exit must not add an Elo game or change a ranked row.
- A statistics clear must preserve Elo values and must refresh the person list.
- An application restart must restore the same persons, roster membership, statistics, Elo values, Elo game counts, and row grouping.
- The empty-person state must keep the `PERSONS` title, all column headings, panel bounds, empty white list surface, person-name field, and person actions visible.
- The empty-person state must not show placeholder copy.
- The Players label must show the selected-player count.
- `Equalize` must first order players by descending Elo.
- `Equalize` must divide the ordered roster into consecutive groups.
- Each complete group must contain as many players as the selected team count.
- `Equalize` must randomly reorder players within each group.
- `Shuffle` must randomly reorder the player roster.
- `Equalize` and `Shuffle` must preserve each player's assigned control preset.
- `Equalize` and `Shuffle` must be visible and interactive only when `Teams` is selected.
- Selecting `Deathmatch` or `Predator` must hide both roster-order controls immediately.
- Selecting `Teams` must show both roster-order controls immediately.
- This change must not alter another control or game behavior.
- The list expansion and button refinement must not change person-list content, person actions, roster-order actions, or controller-detection behavior.
- Selecting `Teams` must color player rows by roster position modulo the selected team count.
- The team order must be Alpha, Bravo, Charlie, and Delta.
- The roster must use Alpha red, Bravo green, Charlie yellow, and Delta magenta.
- A change to `Num. of Team` must update all visible roster team colors immediately.
- Selecting `Deathmatch` or `Predator` must remove all team coloring from the roster immediately.
- A shuffle or roster change must update team coloring for the new roster positions while `Teams` is selected.
- `Num. of Team` must provide the values `2`, `3`, and `4`.
- `Num. of Team` must show `2` at each application start.
- `Friendly Fire` must provide off and on states.
- `Friendly Fire` must show off at each application start.
- Switching to another mode must retain both Team setting values while their controls are hidden.
- Switching back to `Teams` must show both retained values.
- A return from gameplay must retain the selected mode and both Team setting values.
- Starting `Teams` must apply the selected team count and Friendly Fire value to the existing Team deathmatch rules.
- The Assistance, Quick Liquid, and Burnable Trees checkboxes must start checked in the default settings state.
- The Burnable Trees label must use exactly `Burnable Trees`.
- A checked Burnable Trees checkbox must allow explosions to ignite burnable decorative trees during the match.
- An unchecked Burnable Trees checkbox must prevent explosions from igniting burnable decorative trees during the match.
- A Burnable Trees selection must remain in effect when the user starts the match.
- The Rounds value `0` must mean no round limit.
- The Rounds field must show `0` at application startup unless a startup setting overrides it.
- The application must keep the applied Rounds value during the current application session.
- The Rounds field must show a positive applied value after focus changes.
- The Rounds field must show the applied value when gameplay returns to the menu.
- An application restart must not restore the Rounds value from the previous application session.
- A startup setting may override the startup value.
- An empty people dataset must show an empty Persons list, an empty Players list, and an empty score table while it retains the complete setup layout.
- A person with no matching profile must remain selectable and must use runtime fallback customization when play starts.
- Duplicate or empty person names must produce no new row.
- The implementation has no visual loading state.
- The implementation has no disabled control style.
- A failed level or weapon prerequisite must leave the complete menu layout visible under `MENU-02`.
- Dismissal of the prerequisite message must return to this screen without changing the roster or match settings.
- A new Play attempt may repeat the prerequisite check.
- The Play action must not enter `PLAY-01` while either prerequisite is missing.
- A valid start must keep the existing prompt and entry flow unchanged.
- `Play (F1)` must remain local-only and must not start, connect to, or require a network service.
- In the target UI, `Network (F2)` must enter `NET-01` without starting a session and without changing the local roster, settings, or statistics.
- The visible version must come from the runtime application version.
- The menu must select the still once during initialization, retain it when gameplay returns to the menu, retry untried eligible files after load failures, and silently use solid black if all eligible files fail.
- Startup diagnostics must identify the selected background filename or the solid-black fallback.
- Names, statistics, and settings in the Stitch screen are examples only.
- The screen must not add a copyright line from the Stitch sample.

## Controls and focus

- Mouse click must activate buttons, spinners, checkboxes, list selection, and text-field focus.
- Pointer hit testing must apply the inverse canvas translation and scale.
- Mouse wheel must scroll a list under the pointer.
- Pointer click must select one person row.
- `>>` must add the selected person when that person is not in Players.
- `>>` must make no visible change when the selected person is already in Players.
- Double-click on a person must apply the same behavior as `>>`.
- `<<` must return the applicable selected player from Players and must keep that person in the Persons list.
- Double-click on a player must return that player to Persons.
- `Remove` must open the existing delete confirmation only when the selected person is not in Players.
- `Remove` must make no visible change when the selected person is in Players.
- `Add` and Enter in the focused person-name field must retain the existing add behavior.
- `Add` must remain in the person-name row.
- `Remove` and `>>` must remain in the Persons action row in every mode.
- `<<` must remain in the Players action row in every mode.
- Enter must add a person when the person-name field has focus.
- Enter must apply Rounds when the rounds field has focus.
- Focus must clear the Rounds field immediately when the field shows exactly `0`.
- The focused field must show only the focus underscore after it clears `0`.
- Focus must keep the displayed digits unchanged when the Rounds field shows a positive value.
- The focused positive value must include the standard focus underscore.
- Focus loss from an empty Rounds field must show `0`.
- Focus loss from an empty Rounds field must set the round limit to unlimited.
- Focus loss from a non-empty Rounds field must not apply the edited value.
- Enter and Play must retain their existing Rounds application behavior.
- F1 must start the play workflow.
- In the target UI, F2 must open `NET-01`; the current application has no network action.
- F3 must open the clear confirmation.
- Escape must close the menu context.
- Focus must appear only as an underscore in the focused text field.
- Each mode-spinner action must expose only the three approved mode labels.
- Each team-count spinner action must move within `2`, `3`, and `4`.
- The Friendly Fire checkbox must use the standard checked and unchecked feedback.
- Hiding a Team control must remove its interaction target.
- Hiding `Equalize` or `Shuffle` must remove its interaction target.
- `Detect All` must retain the batch controller-detection behavior.
- Each row-level `D` action must retain its controller-detection behavior.

## Accessibility and viewport behavior

- Shortcut text must remain visible in all bottom action captions; the target network entry adds a fourth caption.
- Labels must identify all major lists and settings.
- The Persons columns must use text headings and must not depend on position alone.
- Selection must use fill color and inverse text color across the row.
- Rank and signed Trend text must reinforce the ordered position and score change without color.
- A roster member must remain operable as a selected person row even when `>>` and `Remove` make no change.
- The Players panel must provide the visible roster-membership cue.
- The panel placement of `<<` must provide a spatial cue that it removes the selected roster entry from Players.
- The current menu does not provide an inline reason when `>>` or `Remove` makes no change for a roster member.
- The Burnable Trees control must use a visible text label and must not rely only on its checked state.
- The `Num. of Team` and `Friendly Fire` labels must remain visible next to their controls in the Teams state.
- The Teams state must use the full labels `Equalize` and `Shuffle` instead of the abbreviations `E` and `S`.
- The non-Team state must not show an empty or disabled substitute for either roster-order control.
- Team roster rows must keep readable player names and visible selection feedback over team colors.
- Team roster rows use color and ordered roster position, but the menu does not add team-name text to each row.
- This color-only team cue is an existing accessibility limitation.
- The non-Team state must not leave team color on any roster row.
- The uniformly scaled canvas must remain centered at desktop client sizes.
- The internal layout must not reflow.
- The Persons panel, its columns, its row height, its scroll bar, and its interaction bounds must scale with the complete logical canvas.
- An 850 by 700 px client must render at 100%.
- A 1280 by 720 px client must evaluate at approximately 102.86%, limited by height.
- A 1920 by 1080 px client must render at the 135% cap, producing an approximately 1148 by 945 px centered canvas.
- Larger clients must retain the 135% cap and increase the visible photographic background area.
- A supported client must be at least 850 by 700 px.
- No mobile layout exists, so each conditional state needs only one desktop viewport wireframe.

## Screenshot link

Required representative evidence: [`SS-001`](../screenshots/README.md#ss-001) for the non-Team wireframe and [`SS-024`](../screenshots/README.md#ss-024) for the Teams wireframe.
Each capture must show the expanded person list and the person-name row one standard list row lower than in the PR #60 layout.
Each capture must show the common action-button height, visible caption padding, `Detect All`, and row-level `D` captions.
Each capture must show `Remove` and `>>` at the left and right edges of the Persons action row.
Each capture must show `<<` and `Detect All` at the left and right edges of the Players action row.
The non-Team capture must show no `Equalize` or `Shuffle` control and must keep both action rows at the documented position.
The Teams capture must show the full `Equalize` and `Shuffle` labels as a centered group between the Players edge controls.
The screen specification documents list-content, retention, and behavior variants without requiring another screenshot entry.
Both current implementation captures must show the implemented three-action footer.
Issue #38 must replace this evidence when it implements the target Network action.
