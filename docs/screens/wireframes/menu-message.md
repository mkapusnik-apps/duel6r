# MENU-02 wireframe — Menu blocking message

Representative viewport: 1706 by 938 px desktop client.
The background is the complete MENU-01 layout.
The strip uses message-dependent width and stays centered.
The screen has no mobile layout, so this single desktop wireframe covers all implemented message variants.

```text
┌──────────────────────────────── 1706 × 938 ────────────────────────────────────┐
│                         black matte around MAIN MENU                            │
│                                                                                │
│                                                                                │
│   ┌───────────────────────────────────────────────────────────────────────┐    │
│   │                    Really delete? (Y/N)                               │20px│
│   └───────────────────────────────────────────────────────────────────────┘    │
│                 pink surface · 2 px black frame · red text                     │
│                                                                                │
│                         unchanged menu remains visible                          │
└────────────────────────────────────────────────────────────────────────────────┘
```

The implementation must render each exact message on one 20 px high line.
The implementation must calculate the panel width from the complete message length.
The implementation must keep the approved four-panel MENU-01 canvas unchanged behind the message.

Start-prerequisite variants:

- No level: `No usable levels loaded. Correct content/configuration, restart the application, then try again. Press any key.`
- No weapon: `No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.`
- Both missing: `No usable levels loaded. No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.`

The `Really delete? (Y/N)` confirmation is the representative state for this wireframe.
The confirmation must accept `A` or `Y` as yes and `N` as no.
The start-prerequisite variants must remain available as documented states.
Any keyboard key dismisses a start-prerequisite variant and reveals the unchanged usable menu.
The application consumes the start-prerequisite dismissal key without activating its normal menu action.
Mouse actions do not dismiss a start-prerequisite variant.
The window close action remains available.
Other message variants replace only the message text and computed width.

Representative screenshot: [`SS-002`](../../screenshots/README.md#ss-002).
