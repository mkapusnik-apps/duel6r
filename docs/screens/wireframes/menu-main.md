# MENU-01 wireframes — Non-Team and Teams Game Settings states

Representative viewport: 1920 by 1080 px desktop client.
Wireframe coordinates use a top-left origin.
The 850 by 700 logical menu canvas uses the 135% cap, renders at approximately 1148 by 945 client px, and starts at recorded client coordinate `(386, 67)` at this viewport.
The screen has no mobile layout and does not reflow.
The complete client is filled by a centered-cover gameplay still with Gaussian-equivalent blur and a 55% black scrim.
The scaled grey canvas is unblurred and undimmed and has a 2-logical-pixel black perimeter keyline.
This document defines two conditional-layout wireframes for the same screen.
The non-Team wireframe shows hidden Team settings and standard roster rows.
The Teams wireframe shows retained Team settings and team-colored roster rows.
The Teams representative state shows Rounds `0` and the default two-team setting.
Both wireframes show one consolidated Persons list in place of the separate Elo Scoreboard and Persons lists.
The non-Team representative state uses eight saved people and eight selected players.
The Teams representative state may use six selected players because player count does not change this refinement.
The screen specification defines ranked, unranked, roster-member, and selection variants.
The current implementation captures must use the implemented three-action footer.
The four-action footer remains a target variant for issue #38.
Both conditional-layout wireframes remain necessary because a mode change changes the visible Players panel controls and the Game Settings layout.
Both wireframes show the Persons list expanded by one standard list row and the person-name row lowered by the same amount.
Both wireframes show `Detect All` for batch controller detection and `D` for row-level detection.

## Canvas zones

| Zone | Canvas bounds | Requirement |
|---|---:|---|
| Header | `x=10–839`, `y=5–121` | Center the existing 200 by 95 px animated banner and show the runtime version below it. |
| Setup panels | `x=10–839`, `y=122–447` | Show the Persons, Players, and Game Settings panel groups in one row. |
| Statistics | `x=10–839`, `y=457–606` | Show the complete persistent statistics header and table. |
| Footer actions | `x=10–839`, `y=620–689` | Target: show four 150 by 50 px actions with balanced horizontal spacing. |

The setup panel bounds are `x=10–324` for Persons, `x=330–644` for Players, and `x=650–839` for Game Settings.
Persons and Players each use a width of 315 logical px and split their combined region 50:50.
The gap at `x=325–329` and the gap at `x=645–649` must each remain 5 logical px wide.
Game Settings must keep its width of 190 logical px.
Each panel title strip must identify its panel.
The panel layout may adjust internal column widths by 1 px to preserve integer coordinates.
The banner bounds are `x=325–524` and `y=5–99`.
The version line must stay centered below the banner at `y=103–120`.
Within the half-open footer content span `x=10–840`, the target footer buttons use `x=56–206`, `x=252–402`, `x=448–598`, and `x=644–794` from left to right. Every button is 150 logical px wide; every adjacent gap and both outer margins are 46 logical px.
Each footer button uses `y=630`, a width of 150 px, and a height of 50 px.

## MENU-01-A — Non-Team state

Representative setup: Select `Teams`, set `Num. of Team` to `4`, set `Friendly Fire` on, and then select `Deathmatch`.

```text
┌──────────────────────────── 1920 × 1080 client ────────────────────────────┐
│                blurred gameplay still + 55% black scrim                   │
│        ┌──────────────────── 850 × 700 canvas ────────────────────┐        │
│        │                   [animated banner 200×95]                │        │
│        │                         version <runtime>                 │        │
│        │                                                          │        │
│        │┌────────── PERSONS 315 ──────────┐5px┌───────── PLAYERS 8 / 315 ─────────┐5px┌ GAME SETTINGS 190 ┐│
│        ││Rank │ Name          │ Elo  │ Trend  ││name │ controller │ D ││Mode Deathmatch││
│        ││01   │ Alice         │ 1264 │ +18    ││Alice│ K1: Arrows │ D ││☑ Assistance  ││
│        ││02   │ Bruno         │ 1218 │ -7     ││Bruno│ K1: Arrows │ D ││☑ Quick Liquid ││
│        ││     │ Cora          │      │        ││                      ││☑ Burnable Trees││
│        ││     │ Diego         │      │        ││                      ││Rounds [3___]  ││
│        ││     │ Erin … Hana   │      │        ││… 6 more roster rows  ││               ││
│        ││     │ additional standard list row    ││                      ││               ││
│        ││[person name________________] [Add]   │                      ││               ││
│        ││[Remove]                       [>>] │[<<]              [Detect All]││               ││
│        │└─────────────────────────────────────┘└──────────────────────┘└───────────────┘│
│        │ Name | Elo | Pts | Win | Kill | Assist | Pen | Death | K/D | Shot | Acc. | GmTm | Dmg│
│        │┌──────────────── persistent statistics table ───────────────────┐│
│        ││ compact rows; horizontal content remains inside the canvas     ││
│        │└────────────────────────────────────────────────────────────────┘│
│        │[Play (F1)] [Network (F2)] [Clear (F3)] [Quit (ESC)]              │
│        └──────────────────────────────────────────────────────────────────┘
│                same session-selected background remains visible           │
└───────────────────────────────────────────────────────────────────────────┘
```

`Num. of Team` and `Friendly Fire` must not appear in `MENU-01-A`.
`Equalize` and `Shuffle` must not appear in `MENU-01-A`.
The hidden roster-order controls must not leave empty button frames or interaction targets.
The layout must not reserve empty rows for the hidden controls.
The Persons and Players action rows must keep the same vertical positions as they use in `MENU-01-B`.
Every roster row must use the standard non-Team list colors.
The hidden values remain `4` and on for the current application session.
The Cora row must show the standard selected-row treatment.

## MENU-01-B — Teams state

Representative setup: Select `Teams` with six selected players, `Num. of Team` set to `2`, Friendly Fire off, and Rounds `0`.

```text
┌──────────────────────────── 1920 × 1080 client ────────────────────────────┐
│                blurred gameplay still + 55% black scrim                   │
│        ┌──────────────────── 850 × 700 canvas ────────────────────┐        │
│        │                   [animated banner 200×95]                │        │
│        │                         version <runtime>                 │        │
│        │┌────────── PERSONS 315 ──────────┐5px┌───────── PLAYERS 6 / 315 ─────────┐5px┌ GAME SETTINGS 190 ┐│
│        ││Rank │ Name          │ Elo  │ Trend  ││Alice  Alpha red  │ D ││Mode Teams      ││
│        ││01   │ Alice         │ 1264 │ +18    ││Bruno  Bravo green│ D ││Num. of Team [2]││
│        ││02   │ Bruno         │ 1218 │ -7     ││                      ││☐ Friendly Fire ││
│        ││     │ Cora          │      │        ││                      ││☑ Assistance    ││
│        ││     │ Diego         │      │        ││                      ││☑ Quick Liquid  ││
│        ││     │ Erin … Hana   │      │        ││… rows repeat colors  ││☑ Burnable Trees││
│        ││     │ additional standard list row    ││                      ││               ││
│        ││[person name________________] [Add]   │                      ││Rounds [value]  ││
│        ││[Remove]                       [>>] │[<<] [Equalize] [Shuffle] [Detect All]│             ││
│        │└─────────────────────────────────────┘└──────────────────────┘└────────────────┘│
│        │ Name | Elo | Pts | Win | Kill | Assist | Pen | Death | K/D | Shot | Acc. | GmTm | Dmg│
│        │┌──────────────── persistent statistics table ───────────────────┐│
│        │└────────────────────────────────────────────────────────────────┘│
│        │[Play (F1)] [Network (F2)] [Clear (F3)] [Quit (ESC)]              │
│        └──────────────────────────────────────────────────────────────────┘
└───────────────────────────────────────────────────────────────────────────┘
```

The team names in this text wireframe identify the required color sequence.
The implementation does not need to add team-name text to each roster row.
All seven visible Game Settings controls must stay inside the existing panel bounds.
The controls must use one compact vertical stack without overlap.
Selection feedback remains a documented state variant.

## Component and state notes

- The blurred, scrimmed gameplay still must remain visible on each side of the scaled canvas at the representative viewport.
- The canvas and all internal content and bevels must scale uniformly to 135%; pointer mapping must use the inverse transform.
- The black keyline must be 2 logical px and scale with the canvas.
- The canvas and raised controls must use the retro grey beveled treatment in `docs/design.md`.
- Each setup panel must use the blue panel header and white panel header text from `docs/design.md`.
- White inset surfaces must contain lists, spinners, and text fields.
- The Persons panel title must use exactly `PERSONS`.
- The Persons and Players panels must each use a width of 315 logical px.
- All Persons controls must remain inside `x=10–324`.
- The person-name field and `Add` must use one row.
- `Add` must be to the right of the person-name field.
- `Remove` and `>>` must use a separate Persons action row below the person-name row.
- `Remove` must align to the bottom-left of Persons.
- `>>` must align to the bottom-right of Persons.
- `<<` and `Detect All` must use the Players action row.
- `<<` must align to the bottom-left of Players.
- `Detect All` must align to the bottom-right of Players.
- The Teams state must place `Equalize` and `Shuffle` as one horizontal group between `<<` and `Detect All`.
- The roster-order group must have equal available space on its left and right, within normal integer-pixel rounding.
- The Persons action row and the Players action row must have the same vertical centerline.
- Neither action row must move when the roster-order controls become hidden.
- The person-name row must move down by one standard list row from the PR #60 position.
- The Persons list must fill the released space with one additional standard list row.
- All player names, controller labels, spinner actions, and row actions must remain inside `x=330–644`.
- No control may draw into either 5-logical-pixel setup-panel gap.
- The Persons list must show each saved person one time.
- The Rank, Elo, and Trend columns must keep stable widths.
- The Name column must use the remaining width.
- Each row must remain one line.
- Cell text must clip instead of wrapping or changing the row height.
- The list must use one vertical scroll bar without horizontal scrolling.
- The non-Team representative Persons list must include all eight persons even though they are also in Players.
- Alice and Bruno must appear first in descending Elo order.
- The Alice and Bruno rows must show Rank, Name, Elo, and signed Trend values.
- Cora through Hana must follow in person-record order.
- The Cora through Hana rows must leave Rank, Elo, and Trend empty.
- The Persons list must not show an inline roster badge or team color.
- A selected list row must use blue fill and white text.
- The selected-row fill must span all four person columns.
- The Teams state must use the applicable team background colors.
- The non-Team state must use the standard list row colors.
- A selected roster row must remain visibly selected in both states.
- The Players panel must preserve one control spinner and one `D` action for each of the 15 roster positions.
- The Teams state must show buttons labeled `Equalize` and `Shuffle`.
- `Equalize` must use the documented Elo-based team distribution behavior.
- `Shuffle` must randomly reorder the roster.
- Both roster-order actions must keep each player's assigned control preset.
- The non-Team state must hide both roster-order controls and their interaction targets.
- The roster-order control visibility must update immediately when the selected mode changes.
- The `Detect All` action must remain visually separate from the row-level `D` actions.
- The batch controller-detection action must use the caption `Detect All`.
- Each row-level controller-detection action must keep the caption `D`.
- `Detect All` must keep the existing batch function and roster-action-row position.
- `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and `Detect All` must use one common button height.
- Each common-height button must keep visible space between its caption and border on all sides.
- This change must not alter another control or game behavior.
- The mode spinner must expose Deathmatch, Predator, and exactly one Teams option.
- The Teams state must show `Num. of Team` and `Friendly Fire` directly below Mode.
- The non-Team state must hide both Team controls and their interaction targets.
- `Num. of Team` must expose `2`, `3`, and `4`.
- The team count must default to `2` at each application start.
- Friendly Fire must default to off at each application start.
- Both Team values must survive mode switching and a return from gameplay.
- A team-count change must recolor the roster immediately.
- The color order must repeat Alpha red, Bravo green, Charlie yellow, and Delta magenta according to roster position.
- The Burnable Trees checkbox must use the same size, bevel, label alignment, and compact row spacing as Assistance and Quick Liquid.
- The Burnable Trees checkbox must appear directly below Quick Liquid.
- The Rounds field must move down by one compact control row.
- Rounds retention remains a documented state variant and does not require the PR #60 alignment artifact to show `3`.
- The startup state must show `0` unless a startup setting overrides it.
- Focus on exactly `0` must clear the value immediately and show only the focus underscore.
- Focus on a positive value must keep the digits and append the focus underscore.
- Focus loss from an empty field must restore the visible value `0` and unlimited-round semantics.
- Focus loss from a non-empty field must not apply the edit.
- Enter and Play must retain their existing application behavior.
- A positive applied value must remain visible after focus changes and after gameplay returns during the same application session.
- A new application session must not restore the previous session value.
- The default wireframe state must show Assistance, Quick Liquid, and Burnable Trees checked.
- All Game Settings controls must remain inside the existing panel bounds.
- The footer captions must put the action before the shortcut.
- Footer button widths, three internal gaps, and two outer margins must remain equal as specified above at every uniformly scaled viewport.
- `Play (F1)` remains the independent local-only action; `Network (F2)` is a distinct target action leading to `NET-01`.
- Issue #28 specifies this footer layout but does not implement it. The current application still has the three-action footer.
- The current representative screenshot must use the three-action footer and must not wait for issue #38.
- The banner must use `resources/textures/menu/` and may animate.
- The version must use the current runtime value and must not use the Stitch sample value.
- Sample names and statistics must not become hard-coded UI content.
- The empty-person variant must keep the `PERSONS` title and the `Rank`, `Name`, `Elo`, and `Trend` headings above an empty white list surface.
- The empty-person variant must keep the person-name field, `Add`, `Remove`, and `>>` in Persons.
- The empty-person variant must keep `<<` and `Detect All` in Players.
- The empty-person variant must not show placeholder copy.
- A dataset with only unranked persons must show every person after the headings with empty Rank, Elo, and Trend cells.
- An unlimited Deathmatch, Predator match, Team deathmatch, or early match exit must not move an unranked person into the ranked group.
- The empty-data variant must keep the same three panels and show empty list and table surfaces.
- The 15-player variant must keep each player aligned with that player's control assignment.
- The failed-start recovery variant must restore this complete layout and its previous values.
- The failed-start recovery variant must not add a disabled Play style or prerequisite status indicators.
- Pressed controls must reverse their bevel and move their caption by 1 px.
- Selection occurs once per application session; the still does not animate or change during menu navigation or a return from gameplay.
- A person-list refresh must keep Cora selected while Cora still exists.
- Deleting the selected person must leave the person list with no selection.
- `>>` and a person-row double-click must add a selected non-roster person to Players without removing that person from Persons.
- In the non-Team representative state, `>>` and a person-row double-click must make no visible change when a representative person is selected because all eight are roster members.
- In the non-Team representative state, `Remove` must make no visible change when a representative person is selected because all eight are roster members.
- `<<` and a player-row double-click must remove the applicable roster entry without removing that person from Persons.

Non-Team representative screenshot: [`SS-001`](../../screenshots/README.md#ss-001).
Teams representative screenshot: [`SS-024`](../../screenshots/README.md#ss-024).
The PR #59 screenshots are historical because they show the prior person-action arrangement.
The PR #60 Teams and non-Team artifacts are historical because they show the prior list height, person-name position, button geometry, and batch controller-detection caption.
The PR #62 Teams and non-Team artifacts are historical because they show `<<` in Persons.
The PR #65 Teams artifact conforms at unchanged production source `8eeb60061c32d4ecf1088e5bbf710b691bb76fb1`.
The non-Team wireframe needs a replacement implementation screenshot for the PR #65 placement refinement.
The historical baseline at [`default-1706x938.png`](../../screenshots/MENU-01/default-1706x938.png) is a reference for the unchanged retro controls and footer only.
The historical baseline is not conformance evidence for the consolidated Persons list.
Issue #38 must replace these screenshots when it implements the target footer variant.
