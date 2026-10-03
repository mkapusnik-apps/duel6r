# MENU-02 — macOS local-only message variant

## Scope and authority

This section owns presentation for `MENU-02-MAC-LOCAL` over `MENU-01-MAC-LOCAL` only. The [functional contract](../../screens/menu-message.md) and [MAC-NET-001–008](../../macos.md#network-entry) own the exact copy, activation, dismissal, retention, and window-close behavior. Other message variants and platforms remain unchanged.

Structural baseline: existing [MENU-02](../../screens/wireframes/menu-message.md) and [MENU-01-A / MENU-01-B](../../screens/wireframes/menu-main.md). Reuse all wireframes unchanged. No migration or new wireframe is required. Styling remains owned by [the existing design system](../../design.md).

## Presentation requirements

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

## Acceptance and evidence

The message must remain complete and legible without clipping, smaller text, or a new layout at supported desktop sizes. Check the 850 by 700 compatibility floor and the 1280 by 720 evaluation minimum when a native Mac is available. Larger clients must retain the existing scale cap. The retained menu must have the same presentation before and after dismissal.

Use the [coordinated deferred matrix](../screenshots/README.md#macos-local-only-deferred-coverage). MAC-AC-006–009 explicitly defer native Mac runtime, gameplay, and visual verification until after merge and nightly publication. Specification readiness is not native visual acceptance. Existing other-platform images do not prove macOS rendering.
