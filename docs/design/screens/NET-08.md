# NET-08 — Failure and recovery

Functional authority: [NET-08](../../screens/network-failure.md), `NET-PASS-003/004`, `NET-DIR-014`, `NET-ADM-002`–`NET-ADM-005`, the [trust policy](../../network-trust-and-abuse-limits.md), and the [lifecycle contract](../../network-host-service-lifecycle.md). Structural source: existing [NET-08](../../screens/wireframes/network-failure.md).

Visual impact: preserve develop's unified failure panel and browser recovery. Add pilot-specific outcomes without new controls or disclosure.

## Presentation and allocation

- **UX-NET-08-001** The CONNECTION FAILED or SESSION ENDED title strip must remain above the reason region.
- **UX-NET-08-002** Confirmed reason must remain separate from endpoint and recovery instructions.
- **UX-NET-08-003** Recovery controls must retain equal-height frames in existing reading order when captions fit.
- **UX-NET-08-004** Controls must wrap in reading order before text or focus crosses panel bounds.
- **UX-NET-08-005** Disabled Retry must retain a flat frame, readable caption, and persistent nearby reason.
- **UX-NET-08-006** Severity must remain explicit in text without replacing fixed outcome copy.
- **UX-NET-08-007** Title and recovery actions must remain visible during body overflow.

## Player-hosted and browser recovery

- Authoritative rejection must take precedence over an older open-admission listing.
- `NET-08-password-rejected` must show `Connection not authorized.`
- Edit setup after password rejection must retain endpoint and local players and focus Password.
- Submitted or expected passwords must remain absent.
- Capacity failure must not appear as a password error.
- `NET-08-admission-closed` must show `Round-one admission has closed. Join when the host returns to the lobby.`
- Unreachable transport must retain `Host unreachable.` without a listing-based success claim.
- Browser-origin failure must retain Return to browser after Edit setup in reading and focus order.
- Return to Network must retain its NET-01 destination.
- Retry must follow the confirmed outcome and input eligibility.

## Dedicated pilot outcomes

| Functional state | Heading | Recovery presentation |
|---|---|---|
| `NET-08-PUBLIC-SECURITY` | CONNECTION FAILED | Edit setup first and focused; Return to Network; no Retry or bypass |
| `NET-08-PUBLIC-MAINTENANCE` | SESSION ENDED | Ended-session Retry treatment; Edit setup focused; Return to Network |
| `NET-08-PUBLIC-CONTROLLER-EXPIRED` | SESSION ENDED | Ended-session Retry treatment; Edit setup focused; Return to Network |

- Security and authorization must use product's exact non-disclosing copy.
- Pilot admission failure must not show generic player-hosted startup advice.
- Attempted endpoint may appear only in the bounded initial-connection context row.
- Invitation and reconnect credentials must remain absent.
- Confirmed ended-session recovery must not imply restoration.
- Maintenance wording must require the authenticated product-owned outcome.
- Resolution failure must not be relabeled as maintenance or authorization failure.
- Fixed outcome copy must retain `controller` where product uses that term.
- Any baseline-focused disabled Retry must keep its focus outline without activation.

Do not offer an insecure fallback for any connection type. The service type does not determine the security failure cause. The [coordinated matrix](../screenshots/README.md#network-presentation-current-capture-matrix) keeps one representative and requires focused real-path checks for browser/password and terminal pilot outcomes. No screenshot may substitute silence for an authenticated maintenance or controller-expiry notice.
