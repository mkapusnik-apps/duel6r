# MENU-02 wireframe — Menu blocking message

Representative viewport: 1280 by 900 debug client.
The background is the complete MENU-01 layout.
The strip uses message-dependent width and stays centered.
The screen has no mobile layout, so this single desktop wireframe covers all implemented message variants.

```text
┌──────────────────────────────── 1280 × 900 ────────────────────────────────────┐
│                              MAIN MENU                                         │
│                                                                                │
│                                                                                │
│               ┌─────────────────────────────────────────────┐                  │
│               │          Really delete? (Y/N)               │  20 px high     │
│               └─────────────────────────────────────────────┘                  │
│                 pink surface · 2 px black frame · red text                     │
│                                                                                │
│                         unchanged menu remains visible                          │
└────────────────────────────────────────────────────────────────────────────────┘
```

Variants replace only the message text and computed width.

Representative screenshot: [`SS-002`](../../screenshots/README.md#ss-002).
