# MENU-01 — Main menu and session setup

## Purpose and traceability

The screen builds the local roster, assigns controls, selects match settings, shows persistent results, starts a match, and exits the application.
Entry occurs when the application starts or when gameplay closes.
Exit occurs through `Play (F1)`, `Quit (ESC)`, or the window close action.
The screen implements `SET-001`–`SET-029`, `LIF-023`–`LIF-029`, `INP-001`–`INP-011`, `SCO-019`–`SCO-024`, and `PER-001`–`PER-005` from [`docs/features.md`](../features.md).
The visual source is Stitch screen `681ae093051749fd922ab74454f47121` in project `1219346282527961142`.
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
- The setup row must contain four panel groups in this order: Elo scoreboard, Persons, Players, and Game Settings.
- Each setup panel must use a blue title strip with white text.
- The Persons panel must contain the available-person list, person-name field, `Remove`, `<<`, `>>`, and `Add`.
- The Players panel must contain the selected-player list, each player's control spinner, each player's `D` action, the `E` action, the `S` action, and the batch `D` action.
- The Players panel must align each control spinner and `D` action with the applicable player row.
- The Game Settings panel must contain the mode spinner, Assistance checkbox, Quick Liquid checkbox, Burnable Trees checkbox, and Rounds field.
- The Burnable Trees checkbox must appear directly below the Quick Liquid checkbox.
- The Rounds field must move down by one compact control row.
- The Game Settings panel bounds must not change.
- The full persistent score table must span the middle width.
- The bottom action row must contain `Play (F1)`, `Clear (F3)`, and `Quit (ESC)`.
- The bottom action row must use action-first captions.
- The screen must not use `[F1] PLAY`, `[F3] CLEAR`, or `[ESC] QUIT`.

## Visible behavior and state variants

- The screen must load saved people into Persons or Players according to saved roster membership.
- The Players label must show the selected-player count.
- `E` must reorder players by Elo shuffle logic.
- `S` must reorder players by random shuffle logic.
- Team mode selection must color player rows by team assignment.
- The game mode spinner must contain Deathmatch, Predator, and the six implemented team variants.
- The Assistance, Quick Liquid, and Burnable Trees checkboxes must start checked in the default settings state.
- The Burnable Trees label must use exactly `Burnable Trees`.
- A checked Burnable Trees checkbox must allow explosions to ignite burnable decorative trees during the match.
- An unchecked Burnable Trees checkbox must prevent explosions from igniting burnable decorative trees during the match.
- A Burnable Trees selection must remain in effect when the user starts the match.
- The Rounds value `0` must mean no round limit.
- An empty people dataset must show empty list surfaces and must retain the complete setup layout.
- A person with no matching profile must remain selectable and must use runtime fallback customization when play starts.
- Duplicate or empty person names must produce no new row.
- The implementation has no visual loading state.
- The implementation has no disabled control style.
- A failed level or weapon prerequisite must leave the complete menu layout visible under `MENU-02`.
- Dismissal of the prerequisite message must return to this screen without changing the roster or match settings.
- A new Play attempt may repeat the prerequisite check.
- The Play action must not enter `PLAY-01` while either prerequisite is missing.
- A valid start must keep the existing prompt and entry flow unchanged.
- The visible version must come from the runtime application version.
- The menu must select the still once during initialization, retain it when gameplay returns to the menu, retry untried eligible files after load failures, and silently use solid black if all eligible files fail.
- Startup diagnostics must identify the selected background filename or the solid-black fallback.
- Names, statistics, and settings in the Stitch screen are examples only.
- The screen must not add a copyright line from the Stitch sample.

## Controls and focus

- Mouse click must activate buttons, spinners, checkboxes, list selection, and text-field focus.
- Pointer hit testing must apply the inverse canvas translation and scale.
- Mouse wheel must scroll a list under the pointer.
- Double-click on a person must add that person to Players.
- Double-click on a player must return that player to Persons.
- Enter must add a person when the person-name field has focus.
- Enter must apply Rounds when the rounds field has focus.
- F1 must start the play workflow.
- F3 must open the clear confirmation.
- Escape must close the menu context.
- Focus must appear only as an underscore in the focused text field.

## Accessibility and viewport behavior

- Shortcut text must remain visible in the three bottom action captions.
- Labels must identify all major lists and settings.
- The Burnable Trees control must use a visible text label and must not rely only on its checked state.
- Team rows must use color and ordered team assignment, but the menu does not add team-name text to each player row.
- The uniformly scaled canvas must remain centered at desktop client sizes.
- The internal layout must not reflow.
- An 850 by 700 px client must render at 100%.
- A 1280 by 720 px client must evaluate at approximately 102.86%, limited by height.
- A 1920 by 1080 px client must render at the 135% cap, producing an approximately 1148 by 945 px centered canvas.
- Larger clients must retain the 135% cap and increase the visible photographic background area.
- A supported client must be at least 850 by 700 px.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-001`](../screenshots/README.md#ss-001).
