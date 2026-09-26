# NET-08 — Directory-selected admission failure

Functional contract: [network-failure](../../screens/network-failure.md). States `NET-08-password-rejected` and `NET-08-admission-closed` consume [NET-PASS-003–004 and NET-DIR-014](../../network-host-directory.md) and [NET-ADM-002–005](../../network-play-first-release.md). Existing initial-admission failure requirements remain applicable except the superseded no-join-in-progress outcome. Structural source: [NET-08](../../screens/wireframes/network-failure.md).

- The screen must retain its heading, bounded reason region, endpoint context, and existing recovery controls.
- A host rejection must take precedence over an older directory claim that admission is open.
- NET-08-password-rejected must show `Connection not authorized.`
- Password rejection should offer Edit setup as the first useful recovery action.
- Edit setup must retain the selected endpoint and local-player configuration.
- Edit setup after a password rejection must focus Password.
- The UI must not echo the submitted password or an expected password.
- A capacity rejection must not be presented as a password error.
- A closed-admission rejection must not use `Join-in-progress is not supported.`
- NET-08-admission-closed must show `Round-one admission has closed. Join when the host returns to the lobby.`
- An unreachable host must retain the existing `Host unreachable.` outcome.
- Directory presence must not convert an unreachable outcome into a success claim.
- Return to Network must retain its NET-01 destination, where Browse sessions remains available.
- Browser-origin failure must also show `Return to browser` with destination NET-10 for refresh.
- Return to browser must follow Edit setup in reading and focus order.
- Recovery actions must wrap inside the existing action region before their captions clip.
- Retry must remain disabled when the confirmed outcome or invalid input makes it ineligible.

No new modal or wireframe is needed for this localized recovery-action addition. The representative must show NET-08-password-rejected from a browser-origin attempt with Edit setup focused and Return to browser available. Loading and directory-service errors belong to NET-10, not this screen.
