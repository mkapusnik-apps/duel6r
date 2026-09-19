# NET-08 — Public connection or session failure

Functional authority: [NET-08](../../screens/network-failure.md). Structural wireframe: existing [NET-08](../../screens/wireframes/network-failure.md). No layout change is proposed.

- The panel must keep state heading, confirmed reason, recovery instruction, and actions in that reading order.
- The failure reason must use product's fixed, non-disclosing copy.
- Invite rejection and secure-connection failure must not expose the submitted invite or transport diagnostics.
- An attempted public endpoint may appear only in the existing bounded initial-connection context row.
- Retry must appear enabled only when the approved outcome allows another attempt.
- An ended session must not offer Retry as if it restores that session.
- Edit setup must return to the approved setup destination without implying restoration.
- Disabled Retry must show its persistent reason and remain outside focus traversal.
- Deployment-related wording must appear only for an authenticated, approved deployment outcome.
- DNS not yet configured must use the actual resolution failure rather than a deployment or authorization claim.

## State presentation

The [trust policy](../../network-trust-and-abuse-limits.md) owns exact security and authorization messages. The [lifecycle contract](../../network-host-service-lifecycle.md) owns exact expiry and maintenance messages. Render their copy verbatim; do not replace `controller` in fixed outcome messages with the UI role label `Host`.

| State | Heading | Action presentation |
|---|---|---|
| `NET-08-PUBLIC-SECURITY` | CONNECTION FAILED | Edit setup first and focused; Return to Network second; no Retry action or bypass. |
| `NET-08-PUBLIC-MAINTENANCE` | SESSION ENDED | Disabled Retry with the existing ended-session reason; Edit setup focused; Return to Network. |
| `NET-08-PUBLIC-CONTROLLER-EXPIRED` | SESSION ENDED | Disabled Retry with the existing ended-session reason; Edit setup focused; Return to Network. |

Existing resolution and timeout variants retain their contract's secure Retry eligibility. SS-022 remains the representative and must use an actual secure-connection failure from a wrong-identity test certificate. Verify Edit setup recovery separately. Auth denial, maintenance notice, and controller-expiry copy require QA observations, not additional default screenshots for the same wireframe.
