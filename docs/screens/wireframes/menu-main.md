# MENU-01 wireframes — Non-Team and Teams Game Settings states

Representative viewport: 1920 by 1080 px desktop client.
Wireframe coordinates use a top-left origin.
The 850 by 700 logical menu canvas uses the 135% cap, renders at approximately 1148 by 945 client px, and starts near client coordinate `(386, 68)` at this viewport.
The screen has no mobile layout and does not reflow.
The complete client is filled by a centered-cover gameplay still with Gaussian-equivalent blur and a 55% black scrim.
The scaled grey canvas is unblurred and undimmed and has a 2-logical-pixel black perimeter keyline.
This document defines two conditional-layout wireframes for the same screen.
The non-Team wireframe shows hidden Team settings and standard roster rows.
The Teams wireframe shows retained Team settings and team-colored roster rows.
The Teams representative state shows applied Rounds `3` after gameplay returns during the same application session.
Both wireframes show one consolidated Persons list in place of the separate Elo Scoreboard and Persons lists.
Both representative states use eight saved people and eight selected players.
Alice and Bruno are ranked roster members.
Cora, Diego, Erin, Farah, Gus, and Hana are unranked roster members in person-record order.
Cora is the selected person.
The current implementation captures must use the implemented three-action footer.
The four-action footer remains a target variant for issue #38.

## Canvas zones

| Zone | Canvas bounds | Requirement |
|---|---:|---|
| Header | `x=10–839`, `y=5–121` | Center the existing 200 by 95 px animated banner and show the runtime version below it. |
| Setup panels | `x=10–839`, `y=122–447` | Show the Persons, Players, and Game Settings panel groups in one row. |
| Statistics | `x=10–839`, `y=457–606` | Show the complete persistent statistics header and table. |
| Footer actions | `x=10–839`, `y=620–689` | Target: show four 150 by 50 px actions with balanced horizontal spacing. |

The setup panel bounds are `x=10–384` for Persons, `x=390–644` for Players, and `x=650–839` for Game Settings.
The Persons panel uses the complete area of the two former left panels and the former 5 px internal gap.
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
│        │┌────────────── PERSONS ──────────────┐┌────── PLAYERS 8 ─────┐┌ GAME SETTINGS ┐│
│        ││Rank │ Name          │ Elo  │ Trend  ││name │ controller │ D ││Mode Deathmatch││
│        ││01   │ Alice         │ 1264 │ +18    ││Alice│ K1: Arrows │ D ││☑ Assistance  ││
│        ││02   │ Bruno         │ 1218 │ -7     ││Bruno│ K1: Arrows │ D ││☑ Quick Liquid ││
│        ││     │ Cora          │      │        ││                      ││☑ Burnable Trees││
│        ││     │ Diego         │      │        ││[E] [S] [batch D]     ││Rounds [3___]  ││
│        ││     │ Erin … Hana   │      │        ││… 6 more roster rows  ││               ││
│        ││         [person name________________]│                      ││               ││
│        ││         [Remove] [<<] [>>] [Add]    │                      ││               ││
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
The layout must not reserve empty rows for the hidden controls.
Every roster row must use the standard non-Team list colors.
The hidden values remain `4` and on for the current application session.
The Cora row must show the standard selected-row treatment.

## MENU-01-B — Teams state

Representative setup: Select `Teams`, set `Num. of Team` to `4`, set `Friendly Fire` on, start gameplay with Rounds `3`, and return to the menu in the same application session.

```text
┌──────────────────────────── 1920 × 1080 client ────────────────────────────┐
│                blurred gameplay still + 55% black scrim                   │
│        ┌──────────────────── 850 × 700 canvas ────────────────────┐        │
│        │                   [animated banner 200×95]                │        │
│        │                         version <runtime>                 │        │
│        │┌────────────── PERSONS ──────────────┐┌────── PLAYERS 8 ─────┐┌ GAME SETTINGS ┐│
│        ││Rank │ Name          │ Elo  │ Trend  ││Alice  Alpha red  │ D ││Mode Teams      ││
│        ││01   │ Alice         │ 1264 │ +18    ││Bruno  Bravo green│ D ││Num. of Team [4]││
│        ││02   │ Bruno         │ 1218 │ -7     ││                      ││☑ Friendly Fire ││
│        ││     │ Cora          │      │        ││                      ││☑ Assistance    ││
│        ││     │ Diego         │      │        ││                      ││☑ Quick Liquid  ││
│        ││     │ Erin … Hana   │      │        ││… rows repeat colors  ││☑ Burnable Trees││
│        ││         [person name________________]│[E] [S] [batch D]     ││Rounds [3___]   ││
│        ││         [Remove] [<<] [>>] [Add]    │                      ││                ││
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
The Cora row must show the standard selected-row treatment.

## Component and state notes

- The blurred, scrimmed gameplay still must remain visible on each side of the scaled canvas at the representative viewport.
- The canvas and all internal content and bevels must scale uniformly to 135%; pointer mapping must use the inverse transform.
- The black keyline must be 2 logical px and scale with the canvas.
- The canvas and raised controls must use the retro grey beveled treatment in `docs/design.md`.
- Each setup panel must use the blue panel header and white panel header text from `docs/design.md`.
- White inset surfaces must contain lists, spinners, and text fields.
- The Persons panel title must use exactly `PERSONS`.
- The Persons list must show each saved person one time.
- The Rank, Elo, and Trend columns must keep stable widths.
- The Name column must use the remaining width.
- Each row must remain one line.
- Cell text must clip instead of wrapping or changing the row height.
- The list must use one vertical scroll bar without horizontal scrolling.
- The Persons list must include all eight persons even though they are also in Players.
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
- `E` must mean Elo shuffle.
- `S` must mean random shuffle.
- The batch `D` action must remain visually separate from the row-level `D` actions.
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
- The representative state must show `3` in the unfocused Rounds field after gameplay returns.
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
- The empty-person variant must keep the person-name field and all four person actions.
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
- `>>` and a person-row double-click must make no visible change when any representative person is selected because all eight are roster members.
- `Remove` must make no visible change when any representative person is selected because all eight are roster members.
- `<<` and a player-row double-click must remove the applicable roster entry without removing that person from Persons.

Non-Team representative screenshot: [`SS-001`](../../screenshots/README.md#ss-001).
Teams representative screenshot: [`SS-024`](../../screenshots/README.md#ss-024).
The historical baseline at [`default-1706x938.png`](../../screenshots/MENU-01/default-1706x938.png) is a reference for the unchanged retro controls and footer only.
The historical baseline is not conformance evidence for the consolidated Persons list.
Issue #38 must replace these screenshots when it implements the target footer variant.
