# NET-03 — Join setup and connecting

## Status, purpose, and requirements

### Public pilot functional contract

The states `NET-03-PUBLIC-EDIT` and `NET-03-PUBLIC-CONNECTING` apply `NET-PUB-016`–`NET-PUB-020` and `TRU-PUB-001`–`TRU-PUB-016`. They replace LAN-only endpoint and credential exclusions for public mode. Existing local-player setup, immutable admission ownership, Cancel, deadline, compatibility, and confirmed-success requirements remain applicable.

- **NET-JOIN-PUB-001** Join setup must let the user explicitly choose encrypted public mode or trusted private-LAN mode before Connect.
- **NET-JOIN-PUB-002** First entry must select public mode with the production endpoint from NET-PUB-016.
- **NET-JOIN-PUB-003** Public setup must let the user supply an invitation without making it part of the endpoint.
- **NET-JOIN-PUB-004** A missing invitation in public mode must block Connect in editable setup.
- **NET-JOIN-PUB-005** Switching to LAN mode must clear the supplied invitation.
- **NET-JOIN-PUB-006** Cancel or Edit setup must retain the selected mode, endpoint, and local-player setup.
- **NET-JOIN-PUB-007** An authorization failure must clear the rejected invitation and require a new invitation before another initial attempt.
- **NET-JOIN-PUB-008** Successful dedicated admission must enter NET-04 with the service-confirmed controller or guest role.
- **NET-JOIN-PUB-009** The client must not infer controller authority from an empty lobby, connection order, or a local choice.
- **NET-JOIN-PUB-010** The endpoint input must accept an ASCII DNS hostname or an IPv4 literal separately from an integer port from 1 through 65535.
- **NET-JOIN-PUB-011** Endpoint validation must reject schemes, URL paths, embedded ports, whitespace, and IPv6 without starting a connection attempt.
- **NET-JOIN-PUB-012** Invitation input must accept an opaque case-sensitive value of 1 through 256 printable ASCII characters without spaces.
- **NET-JOIN-PUB-013** Invitation input must reject whitespace, control characters, non-ASCII characters, and overlength input without trimming, normalizing, or truncating the submitted value.
- **NET-JOIN-PUB-014** The user must be able to type or explicitly paste an invitation into the invitation input.
- **NET-JOIN-PUB-015** The client must retain the invitation only in memory for the current public setup and its active connection attempt.
- **NET-JOIN-PUB-016** Cancel, Edit setup, or a recoverable non-authorization failure must retain the invitation only while the public mode and endpoint remain unchanged.
- **NET-JOIN-PUB-017** A hostname or port edit, a mode change, Back, Return to Network, successful admission, or application shutdown must clear the invitation.
- **NET-JOIN-PUB-018** Reconnect must use the existing participant-scoped reconnect credential rather than retain or resubmit the invitation.
- **NET-JOIN-PUB-019** Editable setup must prevent Connect when the invitation does not satisfy NET-JOIN-PUB-012 and NET-JOIN-PUB-013.

An ASCII DNS hostname uses dot-separated labels of 1 through 63 letters, digits, or hyphens, with a letter or digit at each label end and a maximum total length of 253 characters. No trailing dot is accepted. An IPv4 literal has four decimal octets from 0 through 255. Port input contains ASCII digits only. Both modes use this input syntax; LAN resolution retains the existing private-address restriction. Public certificates must validate the entered hostname or IP identity. Public and LAN modes must not be inferred from the address.

The initial public endpoint is `duel.netusite.cz` with separate port `26660` under NET-PUB-016 and NET-PUB-022. A custom endpoint, including `staging.duel.netusite.cz`, uses the same syntax. Invitation syntax is an input bound, not a check of entropy or authenticity; only the service validates the exact operator-issued value. An invalid local value stays editable; a service authorization rejection clears it under NET-JOIN-PUB-007. No invitation is fetched automatically from the clipboard or copied into an endpoint.

Presentation authority: [NET-03 UX contract](../design/screens/NET-03.md). The public/private selector and separate Port input are required, not conditional. The proposed masked invitation treatment is approved; there is no reveal action in this pilot. UX owns mask rendering, labels, layout, focus, and validation presentation. Paste is permitted only as the explicit input action in NET-JOIN-PUB-014. `Host` may label the service-confirmed controller role; it never means ownership of the dedicated process.

The initial default, custom endpoint, and public/LAN choice are functional behavior; UX owns controls, grouping, focus, credential-entry presentation, and feedback. Setup must not imply that the production domain is already active. A valid custom secure test endpoint can exercise the same public behavior before domain activation.

**NET-JOIN-PUB-AC-001:** First entry uses public production setup. Selecting a custom endpoint preserves public security. Explicit LAN selection clears the invitation and uses existing private-address restrictions. Missing invitations cannot initiate public admission. Cancel and Edit setup preserve non-secret setup; a rejected invitation requires replacement. Only complete admission selects the NET-04 role. This criterion covers NET-JOIN-PUB-001 through NET-JOIN-PUB-009.

**NET-JOIN-PUB-AC-002:** Valid hostname/IPv4 and port input accepts production, staging, and custom endpoints. URL syntax, embedded ports, whitespace, IPv6, and invalid port bounds remain editable without a connection. Mode is explicit and never inferred from the address. This criterion covers NET-JOIN-PUB-010 and NET-JOIN-PUB-011.

**NET-JOIN-PUB-AC-003:** Typing or explicit paste preserves the exact invitation. Empty, whitespace-containing, non-ASCII, and overlength values cannot initiate Connect. Cancel and eligible failure recovery retain the value only for unchanged public setup. Authorization rejection, endpoint/mode changes, leaving setup for Network, successful admission, and application shutdown clear it. Reconnect uses only its scoped credential. This criterion covers NET-JOIN-PUB-007 and NET-JOIN-PUB-012 through NET-JOIN-PUB-019. UX assesses masked rendering under its own contract.

This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It configures a guest's direct endpoint and local players, then truthfully reports connection progress. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
It preserves `INP-001` through `INP-010` and implements `NIN-OWN-006` and `NIN-BOUND-003` in [`docs/network-authoritative-player-input.md`](../network-authoritative-player-input.md).
It implements `NET-VIS-001`, `NET-VIS-002`, `NET-VIS-009` through `NET-VIS-011`, `NET-VIS-AC-001`, `NET-VIS-AC-004`, and `NET-VIS-AC-005`. It also consumes `CMP-VIS-001` through `CMP-VIS-004`, `CMP-VIS-AC-001`, and updated `AC-012` from [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
It implements `NET-OWN-001` through `NET-OWN-003` and `NET-OWN-AC-001` through `NET-OWN-AC-002`. Successful admission consumes `ADM-OWN-001` and `ADM-OWN-AC-001` from the compatibility and admission specification.
Issue #30 defines the compatibility and admission outcomes for this flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.

Entry is `NET-01` → `Connect`. Complete validated production admission enters `NET-04`; failure enters `NET-08`; Cancel during connection restores editable setup; Back returns to `NET-01`.

## Representative layout

- Use the scaled retro canvas with a `JOIN NETWORK SESSION` title.
- Show Hostname or address and Port fields, plus the guest's Local Persons and Local Players configuration.
- Do not show a profile selector, profile column, profile value, or profile-editing action.
- The representative state is `Connecting to 127.0.0.1:26660…` with two finalized local-player slots and a visible Cancel action.
- The representative capture must use a project-local loopback host process bound to `127.0.0.1:26660`.
- The loopback host must accept the connection and keep the admission exchange incomplete until after the Connecting capture.
- The capture setup must not change a host interface, route, firewall, Docker network, NAT rule, port-forwarding rule, or other network infrastructure.
- The loopback representative state covers same-machine connection behavior. `SS-016` retains separate explicit private-LAN address coverage.
- Preserve the entered endpoint and local configuration while connecting and after a recoverable failure.
- Keep the screen content inside the shared 24-logical-pixel canvas margin.
- Use a fixed title and endpoint region, a flexible local-player region, and a fixed status and action region.
- Give Hostname or address the remaining width after the Port field and the 8-logical-pixel field gap.
- Keep local-player headings fixed while local-player rows scroll vertically.
- Keep each person and control assignment on one row and clip it inside its column.
- Keep connecting status above Cancel and wrap it at word boundaries.
- Keep Cancel visible when retained setup or connecting copy is longer than the representative values.

## Navigation and significant variants

- Editable setup performs hostname/address and port validation inline. Invalid input never leaves `NET-03` and never starts the connection clock.
- Editable setup must let the guest add or remove local player slots before Connect begins.
- Connect must finalize the displayed local-player count and ordered slot set for the connection attempt.
- Connecting must not permit a local player slot to be added, removed, transferred, or reordered.
- Connect starts the single 10-second attempt. Guest-local gameplay-manifest validation, resolution, connection, request, admission offer, exact guest acceptance, atomic host commit, host-clock calibration, final `admitted` confirmation, initial full snapshot validation, and lobby handoff all share that boundary. There is no separate offer timer.
- The controlled loopback capture host must not send a complete success or rejection before the representative screenshot is captured.
- A Cancel revalidation run must stop the loopback attempt and restore editable setup with `127.0.0.1`, Port `26660`, and both local-player slots retained.
- A timeout revalidation run must withhold every complete terminal result through the original deadline and enter `NET-08` with `Connection timed out.`
- Guest-local manifest validation must finish before resolution. An invalid result established before the deadline uses `guest-gameplay-content-manifest-invalid` and the exact `NET-08` copy, performs no connection, disables Retry until restart, and retains endpoint and local players for Edit setup.
- Cancel stops the attempt and returns to editable setup with endpoint and local-player configuration retained; it does not show Disconnected as though a session existed.
- After Cancel, editable setup may permit the guest to change the retained local-player count before a new attempt.
- Cancel is evaluated before every queued host message and immediately before acceptance or outcome publication. At the deadline, the client must drain already queued complete messages atomically. A valid rejection or a complete valid production admission result received strictly before the deadline must retain precedence over delayed polling or a later close. A late success input must not create admission.
- After Cancel and local validation, a complete host response must use the first applicable result in this order: `malformed-request`, `not-authorized`, `protocol-incompatible`, `network-release-mismatch`, `required-capability-unsupported`, `gameplay-content-manifest-invalid`, `gameplay-content-mismatch`, `match-already-started`, `session-full`, `host-policy-rejected`, and `admitted`.
- Without a complete valid rejection or complete valid production admission result, initial transport outcomes use name-resolution failure, unreachable/refusal, reset/close before complete admission, then generic timeout. A complete valid rejection or complete valid production admission result received strictly before the deadline must outrank later generic transport symptoms. An offer alone must not report success.
- User copy must use the exact fixed messages in `NET-08`.
- User copy must not include a peer-supplied name, release ID, capability, path, hash, count, credential, source address, threshold, payload, or raw filesystem value.
- A guest must validate the exact final confirmation, one valid host-clock calibration result, and one complete valid initial full snapshot before it enters `NET-04`.
- The guest must receive all three success inputs strictly before the single total deadline.
- The final confirmation must contain the exact offered participant identity, original player count, and ordered player identities.
- The initial snapshot must contain the same participant identity and ordered owned-player identities.
- Successful admission must fix the confirmed ordered player identities and ownership while that participant remains admitted.
- The snapshot production time must be valid under the host-clock calibration result.
- A final confirmation without valid calibration and a valid initial snapshot must not report success.
- A malformed, trailing, unexpected, or inconsistent complete host offer, rejection, or confirmation closes the attempt as `invalid-host-admission-message` with `Connection ended before admission completed.`.
- Join-in-progress rejection is explicit when the host already started.
- `NET-08` Retry repeats the retained attempt, Edit setup returns here with all data retained, and Return to Network enters `NET-01`.

## Truthful copy, disabled reasons, and input

- Example reasons are `Enter a hostname or address`, `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No server browser, discovery, Internet, account, password, or matchmaking affordance may appear.
- Focus order is Hostname/address → Port → local-player controls → Connect → Back. During connection, focus is Cancel.
- Keyboard Tab/Shift+Tab and controller directions traverse controls; Enter/Space/controller Confirm activates; Escape/controller Back cancels an attempt or returns to `NET-01` from editable setup.
- A result must remain visible as text until the user selects a recovery action.
- The guest must be able to select any established keyboard preset or detected supported controller preset for each owned local player.
- The setup must permit the same control preset for more than one owned local player.
- Controller detection and connection changes must preserve the established local device behavior.
- A profile or cosmetic difference must not create an inline warning, compatibility failure, or disabled Connect state.
- A required default network visual resource failure must use the existing required-resource failure behavior and must not load peer content as a fallback.

Planned representative screenshot: [`SS-017`](../screenshots/README.md#ss-017).
