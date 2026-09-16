# NET-07 — Public connection loss

Functional authority: [NET-07](../../screens/network-reconnect.md). Structural wireframe: existing [NET-07](../../screens/wireframes/network-reconnect.md). No layout change is proposed.

State `NET-07-PUBLIC-RECONNECT` covers Host and Guest under NET-PUB-010–012 and HSL-PUB-006–010. The private-LAN reconnect contract remains unchanged. The public service owns both roles' fixed 30-second reservation deadlines.

- The panel must retain the last confirmed context without presenting it as live.
- A confirmed public reservation must use the existing positive countdown arrangement for either role.
- The public Host must show `Host` and an `End session` action in the existing action position.
- The Host action must open the contract's `End session for everyone?` confirmation.
- Host supporting copy must state `Reconnect to keep control. If time expires, the session ends.`
- The Host confirmation must explain `Reconnect will stop. The session ends when the service receives the request or the reconnect time expires.`
- The public Guest must retain the existing Leave session action and participant-removal consequence.
- Countdown and reserved-player copy must appear only when the runtime establishes the corresponding reservation.
- An unavailable connection must not be labeled `Host ended session` or `Server update` without authoritative evidence.
- The panel must not promise that an ended dedicated session can resume.
- Endpoint text must wrap inside the existing 640-pixel maximum panel width.
- The panel must not display an invite or reconnect credential.
- Existing modal focus, confirmation, and 16-pixel client-edge clearance must remain unchanged.

Acceptance requires a genuine Host connection-loss capture and QA evidence for guest recovery, Host restoration, expiry, and End during isolation. SS-021 remains the representative wireframe entry with a Host reservation and a positive countdown. The Host action occupies the existing guest action slot; no new layout is needed. Focus must start on the role-appropriate action. Do not substitute a simulated static overlay for an unreachable state.
