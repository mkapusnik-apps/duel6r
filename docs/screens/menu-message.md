# MENU-02 — Menu blocking message

## Purpose and traceability

This state requests confirmation, requests controller input, or reports that one player cannot start a match.
Entry occurs from Clear, person removal, Play, resume handling, statistics clearing, or a controller-detection button.
Exit occurs after an accepted key or event, as defined by the active variant.
The state implements `SET-003`, `SET-006`–`SET-007`, `SET-022`, `LIF-023`–`LIF-029`, and `INP-008`–`INP-009` from [`docs/features.md`](../features.md).
Primary source is `source/Menu.cpp:368-486`.

## Layout and hierarchy

- The state must place the message strip over the unchanged main menu.
- The strip must use the centered layout in [`menu-message.md`](wireframes/menu-message.md).
- The message must be the top visual layer.
- The strip must use the error surface, error text, and black frame from `docs/design.md`.

## Implemented variants and recovery

- Clear and person deletion must show `Really delete? (Y/N)`.
- An unlimited-round start must show `Clear statistics? (Y/N)`.
- A resumable finite match must show `Resume previous game? (Y/N)`.
- Controller detection must show `Player <name>: Press any control`.
- A start with fewer than two players must show `Can't play alone ...`.
- Confirmation must accept `A` or `Y` as yes and `N` as no.
- Controller detection must wait for an accepted directional, shoot, or pick input.
- One-player validation must wait for any polled event and must then return to the menu.
- The implementation has no timeout state.
- The implementation has no explicit cancel path for controller detection.
- The implementation has no separate success panel after control detection.

## Controls, focus, and accessibility

- The message must block the normal menu workflow while its polling loop is active.
- Confirmation copy must show the available `Y/N` choices.
- Controller detection copy must identify the applicable player by name.
- The state has no visible focused action.
- The state has no mouse action labels.

## Viewport behavior

- The strip width must derive from the message length.
- The strip must stay centered in the current client area.
- The background menu must keep its centered fixed canvas.
- No mobile layout exists, so one desktop wireframe is sufficient.

## Screenshot link

Representative evidence: [`SS-002`](../screenshots/README.md#ss-002).
