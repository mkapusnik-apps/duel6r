# MENU-01 wireframe — Main menu with planned network entry

Representative viewport: 1920 by 1080 px desktop client.
Wireframe coordinates use a top-left origin.
The 850 by 700 logical menu canvas uses the 135% cap, renders at approximately 1148 by 945 client px, and starts near client coordinate `(386, 68)` at this viewport.
The screen has no mobile layout and does not reflow.
The complete client is filled by a centered-cover gameplay still with Gaussian-equivalent blur and a 55% black scrim.
The scaled grey canvas is unblurred and undimmed and has a 2-logical-pixel black perimeter keyline.
The representative state shows applied Rounds `3` after gameplay returns during the same application session.

## Canvas zones

| Zone | Canvas bounds | Requirement |
|---|---:|---|
| Header | `x=10–839`, `y=5–121` | Center the existing 200 by 95 px animated banner and show the runtime version below it. |
| Setup panels | `x=10–839`, `y=122–447` | Show the four raised panel groups in one row. |
| Statistics | `x=10–839`, `y=457–606` | Show the complete persistent statistics header and table. |
| Footer actions | `x=10–839`, `y=620–689` | Target: show four 150 by 50 px actions with balanced horizontal spacing. |

The setup panel bounds are `x=10–194` for Elo scoreboard, `x=200–384` for Persons, `x=390–644` for Players, and `x=650–839` for Game Settings.
Each panel title strip must identify its panel.
The panel layout may adjust internal column widths by 1 px to preserve integer coordinates.
The banner bounds are `x=325–524` and `y=5–99`.
The version line must stay centered below the banner at `y=103–120`.
Within the half-open footer content span `x=10–840`, the target footer buttons use `x=56–206`, `x=252–402`, `x=448–598`, and `x=644–794` from left to right. Every button is 150 logical px wide; every adjacent gap and both outer margins are 46 logical px.
Each footer button uses `y=630`, a width of 150 px, and a height of 50 px.

```text
┌──────────────────────────── 1920 × 1080 client ────────────────────────────┐
│                blurred gameplay still + 55% black scrim                   │
│        ┌──────────────────── 850 × 700 canvas ────────────────────┐        │
│        │                   [animated banner 200×95]                │        │
│        │                         version <runtime>                 │        │
│        │                                                          │        │
│        │┌ ELO SCOREBOARD ┐┌── PERSONS ──┐┌────── PLAYERS n ─────┐┌ GAME SETTINGS ┐│
│        ││rank name Elo Δ ││available    ││name │ controller │ D ││Mode spinner   ││
│        ││                ││people       ││1 …  │ preset      │ D ││☑ Assistance  ││
│        ││                ││             ││… up to 15 rows       ││☑ Quick Liquid ││
│        ││                ││             ││                      ││☑ Burnable Trees││
│        ││                ││             ││[E] [S] [batch D]     ││Rounds [3___]  ││
│        ││                ││[name_______]││                      ││               ││
│        ││                ││[Remove][<<][>>][Add]                 ││               ││
│        │└────────────────┘└──────────────┘└──────────────────────┘└───────────────┘│
│        │ Name | Elo | Pts | Win | Kill | Assist | Pen | Death | K/D | Shot | Acc. | GmTm | Dmg│
│        │┌──────────────── persistent statistics table ───────────────────┐│
│        ││ compact rows; horizontal content remains inside the canvas     ││
│        │└────────────────────────────────────────────────────────────────┘│
│        │[Play (F1)] [Network (F2)] [Clear (F3)] [Quit (ESC)]              │
│        └──────────────────────────────────────────────────────────────────┘
│                same session-selected background remains visible           │
└───────────────────────────────────────────────────────────────────────────┘
```

## Component and state notes

- The blurred, scrimmed gameplay still must remain visible on each side of the scaled canvas at the representative viewport.
- The canvas and all internal content and bevels must scale uniformly to 135%; pointer mapping must use the inverse transform.
- The black keyline must be 2 logical px and scale with the canvas.
- The canvas and raised controls must use the retro grey beveled treatment in `docs/design.md`.
- Each setup panel must use the blue panel header and white panel header text from `docs/design.md`.
- White inset surfaces must contain lists, spinners, and text fields.
- The representative populated state must show every saved person with at least one Elo game in the Elo scoreboard.
- The populated Elo rows must use descending Elo order and show rank, person name, Elo, and Elo trend.
- The representative populated state must show two qualifying saved persons with existing Elo history.
- A selected list row must use blue fill and white text.
- Team-mode player rows must use the applicable team background colors.
- The Players panel must preserve one control spinner and one `D` action for each of the 15 roster positions.
- `E` must mean Elo shuffle.
- `S` must mean random shuffle.
- The batch `D` action must remain visually separate from the row-level `D` actions.
- The mode spinner must expose Deathmatch, Predator, and all six Team deathmatch variants.
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
- The banner must use `resources/textures/menu/` and may animate.
- The version must use the current runtime value and must not use the Stitch sample value.
- Sample names and statistics must not become hard-coded UI content.
- The empty Elo variant must keep the `ELO SCOREBOARD` title, `rank`, `name`, `Elo`, and `Δ` headings visible above an empty white list surface.
- The empty Elo variant must exclude each saved person whose Elo game count is zero.
- The empty Elo variant must not show placeholder copy.
- An unlimited Deathmatch, Predator match, Team deathmatch, or early match exit must leave the empty Elo variant unchanged when no person has a prior Elo game.
- The empty-data variant must keep the same panels and show empty list and table surfaces.
- The 15-player variant must keep each player aligned with that player's control assignment.
- The failed-start recovery variant must restore this complete layout and its previous values.
- The failed-start recovery variant must not add a disabled Play style or prerequisite status indicators.
- Pressed controls must reverse their bevel and move their caption by 1 px.
- Selection occurs once per application session; the still does not animate or change during menu navigation or a return from gameplay.

Planned representative screenshot: [`SS-001`](../../screenshots/README.md#ss-001).
