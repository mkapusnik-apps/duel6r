# NET-09 — Confirmed session end

Functional authority: [NET-09](../../screens/network-host-ended.md). Structural wireframe: existing [NET-09](../../screens/wireframes/network-host-ended.md). No layout change is proposed.

State `NET-09-PUBLIC-CONTROLLER-END` consumes HSL-PUB-005 and HSL-PUB-012. It uses the existing fixed host-ended copy for a confirmed intentional controller end, including a received normal-shutdown request. Controller expiry and confirmed maintenance use NET-08. Ambiguous loss uses NET-07.

- A confirmed end must retain the last confirmed context behind the blocking panel.
- The heading and explanation must identify only the confirmed cause using product's fixed copy.
- Return to Network must remain the single primary recovery action for an outcome mapped here.
- The panel must not offer host election, resume, or saved session-only results.
- Match contexts must retain the session-only results statement.
- The panel must preserve its existing 640-pixel maximum width and 16-pixel client-edge clearance.
- The action must remain visible when explanatory text wraps.
- Keyboard and controller Back must use the approved Return to Network behavior.

SS-023 remains the representative: a real accepted intentional end notice over an arena context. Normal shutdown, expiry, and deployment cases require behavioral evidence for their distinct functional mappings. Unreachable outcomes must not be fabricated for capture. No new layout is needed.
