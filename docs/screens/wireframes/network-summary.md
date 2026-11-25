# NET-06 wireframe — Compact completed scoreboard

Representative viewport: native 1280 by 900 px. The compact scoreboard retains the final arena behind the existing curtain.

[![NET-06 — Compact final scoreboard, network role variants](../../design/wireframes/NET-06/NET-06.svg)](../../design/wireframes/NET-06/NET-06.svg)

The [functional contract](../overlay-game-over.md#common-final-screen-contract), [NET-06 binding](../../design/screens/NET-06.md) and [appearance owner](../../design.md#shared-final-presentation) define the compact interim-style panel. There is one score strip and no large final window or fixed diagnostic-style outcome hierarchy. Required data remain in the existing continuous result body; this adds no tab, details screen or control. External annotations are not UI.

- The exact `Session only` label is visible with the authoritative final results.
- This screen presents only a `Completed` result.
- The match outcome equals the configured final-round outcome.
- Cumulative scores start the body. Outcome identities remain separate in result detail, and the last-round outcome remains in the corresponding round detail, without duplicate fixed captions.
- The cumulative ranking leader does not receive a champion label or treatment.
- Final-summary departure never reevaluates the completed outcome; retained rows use `Departed`. Starting a new match clears this set, while intentional host end or host-local supervised failure discards the host result.
- Guest Leave, full-deadline reconnect, and intentional host End session variants and confirmations remain in the specification.
- Keyboard/controller focus is explicit and does not depend on color.
- An interrupted result enters `NET-04` directly and never appears here.
- The result provides all completed match, round, player, and applicable team fields from issue #32.
- Ranking uses labeled columns and textual rank, team, winner, No winner, and Departed values.
- The content-sized panel keeps its single strip/current column headings visible above a bounded result viewport. Only actual overflow receives navigation feedback. Required status/persistence/waiting copy and role actions remain outside scrolling content.
- Wide result tables provide an internal horizontal scroll control and keep their column headings visible.

Current representative: [NET-06 / SS-020 compact final](../../design/screenshots/README.md#current-ss-020). The [compact-final matrix](../../design/screenshots/README.md#compact-final-scoreboard) retains that identity without inheriting an old round-count recipe.

Complete winner identities, scope, departure state and UTF-8 values remain accessible independently of current membership under the [summary specification](../network-summary.md). The [compact-final matrix](../../design/screenshots/README.md#compact-final-scoreboard) owns this follow-up's focused evidence; broader historical recipes do not impose another outcome-hunting journey for the removed chrome.
