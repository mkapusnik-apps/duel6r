# MENU-01 — Main menu and session setup

## Purpose and traceability

The screen builds the local roster, assigns controls, selects match settings, shows persistent results, starts a match, and exits the application.
Entry occurs when the application starts or when gameplay closes.
Exit occurs through `Play (F1)`, `Quit (ESC)`, or the window close action.
The screen implements `SET-001`–`SET-023`, `LIF-023`–`LIF-029`, `INP-001`–`INP-011`, `SCO-019`–`SCO-024`, and `PER-001`–`PER-005` from [`docs/features.md`](../features.md).
Primary sources are `source/Menu.cpp`, `source/gui/`, `source/GameSettings.cpp`, and `resources/textures/menu/`.

## Prerequisites

- The application must initialize video, font, resources, controls, and saved person data before interaction.
- Starting a match must require at least two selected players.
- Each selected player must have an available control entry.
- A start attempt must require at least one successfully loaded level.
- A start attempt must require at least one enabled weapon.
- The application must check the level and weapon prerequisites before it shows `Resume previous game? (Y/N)` or `Clear statistics? (Y/N)`.

## Layout and hierarchy

- The screen must use the centered 850 by 700 menu canvas from [`menu-main.md`](wireframes/menu-main.md).
- The upper area must contain the banner and version text.
- The setup row must contain the Elo list, Persons list, Players list, controller spinners, and Game Settings.
- The person action row must contain `Remove`, `<<`, `>>`, `Add`, and the person-name field.
- The full persistent score table must span the middle width.
- The bottom action row must contain `Play (F1)`, `Clear (F3)`, and `Quit (ESC)`.

## Visible behavior and state variants

- The screen must load saved people into Persons or Players according to saved roster membership.
- The Players label must show the selected-player count.
- `E` must reorder players by Elo shuffle logic.
- `S` must reorder players by random shuffle logic.
- Team mode selection must color player rows by team assignment.
- The game mode spinner must contain Deathmatch, Predator, and the six implemented team variants.
- The Assistance and Quick Liquid checkboxes must start checked in the default settings state.
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

## Controls and focus

- Mouse click must activate buttons, spinners, checkboxes, list selection, and text-field focus.
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
- Team rows must use color and ordered team assignment, but the menu does not add team-name text to each player row.
- The canvas must remain centered at desktop client sizes.
- The internal layout must not reflow.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-001`](../screenshots/README.md#ss-001).
