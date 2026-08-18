# MENU-01 wireframe — Main menu

Representative viewport: 1280 by 900 debug client.
Wireframe coordinates use a top-left origin.
The 850 by 700 menu canvas is centered with 215 px horizontal and 100 px vertical offsets at this viewport.
The screen has no mobile layout and does not reflow, so this single desktop wireframe covers the implemented layout.

```text
┌────────────────────────────── 1280 × 900 client ──────────────────────────────┐
│ #C0C0C0                                                                       │
│          ┌────────────────── 850 × 700 logical canvas ──────────────────┐      │
│          │                    [animated menu banner 200×95] version x.y │      │
│          │                                                              │      │
│          │ [Elo scoreboard] [Persons] [Players n] [E][S] [Controller][D]│      │
│          │ ┌──────────────┐ ┌─────────┐ ┌───────┐ ┌────────┐ ┌────────┐ │      │
│          │ │ Elo ranking  │ │ people  │ │active │ │control │ │ Game   │ │      │
│          │ │ 23×15 rows   │ │18×15   │ │13×15  │ │spinners│ │Settings│ │      │
│          │ │              │ │         │ │       │ │ + D    │ │ mode   │ │      │
│          │ │              │ │         │ │       │ │        │ │ assist │ │      │
│          │ │              │ │         │ │       │ │        │ │ liquid │ │      │
│          │ └──────────────┘ └─────────┘ └───────┘ └────────┘ │ rounds │ │      │
│          │ [Remove] [<<] [>>] [Add] [new person____________] └────────┘ │      │
│          │ Name | Elo | Pts | Win | Kill | Assist | Pen | Death | ...   │      │
│          │ ┌──────────────── full persistent score table ─────────────┐ │      │
│          │ │ 12 compact rows                                          │ │      │
│          │ └──────────────────────────────────────────────────────────┘ │      │
│          │                         [Play (F1)] [Clear (F3)] [Quit (ESC)] │      │
│          └──────────────────────────────────────────────────────────────┘      │
└────────────────────────────────────────────────────────────────────────────────┘
```

Implementation notes:

- Renderer control coordinates use a bottom-left origin.
- The wireframe reverses the vertical order for documentation.
- List surfaces are white with black text.
- Selected rows are blue with white text.
- Team-mode player rows use team background colors.

Representative screenshot: [`SS-001`](../../screenshots/README.md#ss-001).
