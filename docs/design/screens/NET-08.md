# NET-08 — Failure and recovery

Functional authority: [NET-08](../../screens/network-failure.md), NET-PASS-003/004/011–018, NET-DIR-014, NET-ADM-002–005, current trust and lifecycle contracts. Structural source: existing NET-08. No geometry, screen/state ID or failure wireframe changes.

Visual impact: preserve current develop's unified failure panel and browser recovery. Add dedicated outcomes without new controls, styles, or disclosure.

## Presentation and allocation

- **UX-NET-08-001** The CONNECTION FAILED or SESSION ENDED title strip must remain above the reason region.
- **UX-NET-08-002** Confirmed reason must remain separate from endpoint and recovery instructions.
- **UX-NET-08-003** Recovery controls must retain equal-height frames in existing reading order when captions fit.
- **UX-NET-08-004** Controls must wrap in reading order before text or focus crosses panel bounds.
- **UX-NET-08-005** Disabled Retry must retain a flat frame, readable caption, and persistent nearby reason.
- **UX-NET-08-006** Severity must remain explicit in text without replacing fixed outcome copy.
- **UX-NET-08-007** Title and recovery actions must remain visible during body overflow.

## Common Password and browser recovery

- Authoritative rejection must take precedence over an older open-admission listing.
- `NET-08-password-rejected` must show `Connection not authorized.` for both hosting types.
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
| `NET-08-PUBLIC-SECURITY` | CONNECTION FAILED | Edit setup first and focused; Return to browser for selected origin, Return to Network for direct origin; no Retry or bypass |
| `NET-08-PUBLIC-MAINTENANCE` | SESSION ENDED | Ended-session Retry treatment; Edit setup focused; Return to Network |
| `NET-08-PUBLIC-CONTROLLER-EXPIRED` | SESSION ENDED | Ended-session Retry treatment; Edit setup focused; Return to Network |

- Security and authorization must use product's exact non-disclosing copy.
- Pilot admission failure must not show generic player-hosted startup advice.
- Attempted endpoint may appear only in the bounded initial-connection context row.
- Password and reconnect credentials must remain absent.
- Confirmed ended-session recovery must not imply restoration.
- Maintenance wording must require the authenticated product-owned outcome.
- Resolution failure must not be relabeled as maintenance or authorization failure.
- Fixed outcome copy must retain `controller` where product uses that term.
- Any baseline-focused disabled Retry must keep its focus outline without activation.
- Maintenance and controller-expiry outcomes must use SESSION ENDED for either participant role.
- Certificate/identity/encryption outcomes must have no Retry frame, focus stop, pointer target or bypass. Common Password correction follows NET-PASS in the same permitted context without unlocked fallback.
- Endpoint context must remain absent from confirmed session-end and terminal reconnect outcomes.

Do not offer an insecure fallback for any connection type. Service type does not prove a failure cause. The scoped matrix keeps one certificate-security representative and focused real-path common Password rejection and confirmed expiry/maintenance coverage, without new rows. Password rejection is separate from certificate failure; it preserves correction under NET-PASS-013, not the retired invitation-clearing rule. No screenshot may substitute silence/SIGTERM for an authenticated terminal cause or imply deployment evidence.

## Selected recovery in both hosting modes

- **UX-NET-08-008** Browser-selected failure in either hosting mode must identify the selected origin without displaying credentials or unvalidated diagnostics.
- **UX-NET-08-009** Eligible correction must offer Edit setup first and focused and return to bound NET-03-E or NET-03-B with endpoint/session/type/protection still read-only.
- **UX-NET-08-011** Selected recovery in either hosting mode must show `Return to browser` after Edit setup in reading/focus order and return to NET-10 for refresh.
- **UX-NET-08-012** Escape or controller Back from a selected failure in either hosting mode must use that browser recovery rather than bypass selection binding through direct setup.
- **UX-NET-08-013** A confirmed changed endpoint/session must focus Return to browser and must not offer Retry or Edit setup as a way to adopt the replacement.
- **UX-NET-08-014** The fixed failure reason must follow the product contract and keep capacity, closed admission, certificate failure and common password rejection distinct.
- **UX-NET-08-015** A new independent Direct connect from the browser must clear binding/secrets in either hosting mode without transferring a stale selection or silently downgrading security.
- **UX-NET-08-016** Edit setup after password rejection in either hosting mode must focus masked Password with permitted local choices and selected binding retained under NET-PASS correction and clearing rules.

UX-NET-08-010 is a retired invitation-only identifier and must never be reused. Current correction authority is common NET-PASS-003/004/011–018; historical invitation assessments do not establish new Password conformance.

Retain the current complete, wrapped failure layout and action order. Ineligible correction must use the existing flat disabled treatment without a focus stop or activation target; product owns outcome eligibility. No new failure wireframe is added. Direct dedicated and player-hosted recovery destinations remain unchanged.
