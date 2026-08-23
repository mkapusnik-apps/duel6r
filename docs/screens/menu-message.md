# MENU-02 — Menu blocking message

## Purpose and traceability

This state requests confirmation, requests controller input, or reports why a match cannot start.
Entry occurs from Clear, person removal, Play, resume handling, statistics clearing, or a controller-detection button.
Exit occurs after an accepted key or event, as defined by the active variant.
The state implements `SET-003`, `SET-006`–`SET-007`, `SET-022`, `LIF-023`–`LIF-029`, and `INP-008`–`INP-009` from [`docs/features.md`](../features.md).
Primary source is `source/Menu.cpp:368-486`.

## Layout and hierarchy

- The state must place the message strip over the unchanged main menu.
- The unchanged main menu must include the black matte and the approved four-panel canvas.
- The strip must use the centered layout in [`menu-message.md`](wireframes/menu-message.md).
- The message must be the top visual layer.
- The strip must use the error surface, error text, and black frame from `docs/design.md`.

## Variants and recovery

- Clear and person deletion must show `Really delete? (Y/N)`.
- An unlimited-round start must show `Clear statistics? (Y/N)`.
- A resumable finite match must show `Resume previous game? (Y/N)`.
- Controller detection must show `Player <name>: Press any control`.
- A start with fewer than two players must show `Can't play alone ...`.
- A start with no successfully loaded level must show `No usable levels loaded. Correct content/configuration, restart the application, then try again. Press any key.`.
- A start with no enabled weapon must show `No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.`.
- A start with no successfully loaded level and no enabled weapon must show `No usable levels loaded. No weapons enabled. Correct content/configuration, restart the application, then try again. Press any key.`.
- The combined variant must report both missing prerequisites in one message.
- The application must show a prerequisite message before a resume or start-related statistics-clear confirmation.
- Confirmation must accept `A` or `Y` as yes and `N` as no.
- Controller detection must wait for an accepted directional, shoot, or pick input.
- One-player validation must wait for any polled event and must then return to the menu.
- A prerequisite message must wait for any keyboard key.
- A keyboard key must dismiss the prerequisite message and return input to the unchanged menu.
- The application must consume the dismissal key and must not activate its normal menu action.
- A mouse action must not dismiss the prerequisite message.
- The window close action must remain available while the message is visible.
- Dismissal must not start a match, change the roster, change match settings, or show a resume or statistics-clear confirmation.
- A repeated Play action with unchanged invalid content or configuration must show the applicable prerequisite message again.
- The implementation has no timeout state.
- The implementation has no explicit cancel path for controller detection.
- The implementation has no separate success panel after control detection.

## Controls, focus, and accessibility

- The message must block the normal menu workflow while its polling loop is active.
- Confirmation copy must show the available `Y/N` choices.
- Controller detection copy must identify the applicable player by name.
- A prerequisite message must show `Press any key.`.
- The state has no visible focused action.
- The state has no mouse action labels.

## Viewport behavior

- The strip width must derive from the message length.
- The strip must stay centered in the current client area.
- The background menu must keep its centered fixed canvas.
- The black matte must remain visible around the background canvas on a larger client.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-002`](../screenshots/README.md#ss-002).
