# MENU-01 wireframe — Main menu

Representative viewport: 1706 by 938 px desktop client.
Wireframe coordinates use a top-left origin.
The 850 by 700 px menu canvas starts at client coordinate `(428, 119)` at this viewport.
The screen has no mobile layout and does not reflow.

## Canvas zones

| Zone | Canvas bounds | Requirement |
|---|---:|---|
| Header | `x=10–839`, `y=5–121` | Center the existing 200 by 95 px animated banner and show the runtime version below it. |
| Setup panels | `x=10–839`, `y=122–447` | Show the four raised panel groups in one row. |
| Statistics | `x=10–839`, `y=457–606` | Show the complete persistent statistics header and table. |
| Footer actions | `x=10–839`, `y=620–689` | Show three 150 by 50 px actions with balanced horizontal spacing. |

The setup panel bounds are `x=10–194` for Elo scoreboard, `x=200–384` for Persons, `x=390–644` for Players, and `x=650–839` for Game Settings.
Each panel title strip must identify its panel.
The panel layout may adjust internal column widths by 1 px to preserve integer coordinates.
The banner bounds are `x=325–524` and `y=5–99`.
The version line must stay centered below the banner at `y=103–120`.
The footer buttons use `x=50`, `x=350`, and `x=650` from left to right.
Each footer button uses `y=630`, a width of 150 px, and a height of 50 px.

```text
┌──────────────────────────── 1706 × 938 client ─────────────────────────────┐
│                              black matte                                  │
│        ┌──────────────────── 850 × 700 canvas ────────────────────┐        │
│        │                   [animated banner 200×95]                │        │
│        │                         version <runtime>                 │        │
│        │                                                          │        │
│        │┌ ELO SCOREBOARD ┐┌── PERSONS ──┐┌────── PLAYERS n ─────┐┌ GAME SETTINGS ┐│
│        ││rank name Elo Δ ││available    ││name │ controller │ D ││Mode spinner   ││
│        ││                ││people       ││1 …  │ preset      │ D ││☑ Assistance  ││
│        ││                ││             ││… up to 15 rows       ││☑ Quick Liquid ││
│        ││                ││             ││                      ││☑ Burnable Trees││
│        ││                ││             ││[E] [S] [batch D]     ││Rounds [0___]  ││
│        ││                ││[name_______]││                      ││               ││
│        ││                ││[Remove][<<][>>][Add]                 ││               ││
│        │└────────────────┘└──────────────┘└──────────────────────┘└───────────────┘│
│        │ Name | Elo | Pts | Win | Kill | Assist | Pen | Death | K/D | Shot | Acc. | GmTm | Dmg│
│        │┌──────────────── persistent statistics table ───────────────────┐│
│        ││ compact rows; horizontal content remains inside the canvas     ││
│        │└────────────────────────────────────────────────────────────────┘│
│        │ [Play (F1)]                 [Clear (F3)]              [Quit (ESC)]│
│        └──────────────────────────────────────────────────────────────────┘
│                              black matte                                  │
└───────────────────────────────────────────────────────────────────────────┘
```

## Component and state notes

- The black matte must remain visible on each side of the fixed canvas at the representative viewport.
- The canvas and raised controls must use the retro grey beveled treatment in `docs/design.md`.
- Each setup panel must use the blue panel header and white panel header text from `docs/design.md`.
- White inset surfaces must contain lists, spinners, and text fields.
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
- The default wireframe state must show Assistance, Quick Liquid, and Burnable Trees checked.
- All Game Settings controls must remain inside the existing panel bounds.
- The footer captions must put the action before the shortcut.
- The banner must use `resources/textures/menu/` and may animate.
- The version must use the current runtime value and must not use the Stitch sample value.
- Sample names and statistics must not become hard-coded UI content.
- The empty-data variant must keep the same panels and show empty list and table surfaces.
- The 15-player variant must keep each player aligned with that player's control assignment.
- The failed-start recovery variant must restore this complete layout and its previous values.
- The failed-start recovery variant must not add a disabled Play style or prerequisite status indicators.
- Pressed controls must reverse their bevel and move their caption by 1 px.

Representative screenshot: [`SS-001`](../../screenshots/README.md#ss-001).
