# MENU-02 — Retired macOS local-only message variant

## Scope and authority

`MENU-02-MAC-LOCAL` over `MENU-01-MAC-LOCAL` and UX-MAC-MSG-001 through UX-MAC-MSG-011 below are retired historical-only requirements. They record the former local-only build, not the network-capable target. The [functional contract](../../screens/menu-message.md) and [macOS network contract](../../macos.md#network-entry) now route Network to [NET-01](NET-01.md). Other message variants and platforms remain unchanged.

Structural baseline: existing [MENU-02](../../screens/wireframes/menu-message.md) and [MENU-01-A / MENU-01-B](../../screens/wireframes/menu-main.md). Reuse all wireframes unchanged. No migration or new wireframe is required. Styling remains owned by [the existing design system](../../design.md).

## Historical presentation requirements (retired)

- **UX-MAC-MSG-001** The variant must retain the existing Network button caption, bounds, and enabled appearance in the four-action footer.
- **UX-MAC-MSG-002** The message strip must occupy the existing centered MENU-02 position above the unchanged menu.
- **UX-MAC-MSG-003** The strip must use the existing message surface, text treatment, black frame, and standard text size.
- **UX-MAC-MSG-004** The strip must use the same uniform scale as the menu canvas.
- **UX-MAC-MSG-005** The complete MAC-NET-003 explanation and dismissal instruction must remain visible inside the logical canvas.
- **UX-MAC-MSG-006** Text must wrap at word boundaries when it cannot fit on one line.
- **UX-MAC-MSG-007** The strip must grow by the existing text-row height for each additional line.
- **UX-MAC-MSG-008** Wrapping should prefer sentence boundaries where practical.
- **UX-MAC-MSG-009** The message must retain reading order from the restriction to the local-play alternative and then the dismissal instruction.
- **UX-MAC-MSG-010** The variant must not add a button, focus target, tooltip, loading indicator, disabled-control style, or color-only availability cue.
- **UX-MAC-MSG-011** The session-selected background, canvas, underlying controls, and footer must retain their existing presentation.

The existing fixed 850 by 700 logical canvas, desktop scaling, and no-reflow rules apply. No mobile presentation is introduced. The message has no focused action or pointer dismissal affordance. Its visible keyboard instruction explains the approved recovery path; this bounded variant does not redesign the existing message interaction.

## Historical acceptance and current evidence boundary

The retired variant required complete, legible copy and retained-menu presentation. It is no longer a required capture or acceptance state for network-capable macOS. The existing MENU-02 wireframe and unrelated messages remain unchanged.

Use the [coordinated deferred matrix](../screenshots/README.md#macos-deferred-coverage). MAC-AC-006–009 and MAC-NET-AC-006 defer actual Mac GUI, gameplay, live cross-OS, and visual verification until after merge/nightly. Work-branch checks run only on Linux/Docker. Automatic ARM, macOS, and Windows validation runs in Develop - Sanity after integration; native automated MAC-NET-AC-001–005 remain mandatory at that stage, not before review or merge. All platform checks remain gating for sanity/nightly. Specification readiness is not native visual acceptance. Existing other-platform images do not prove macOS rendering.
