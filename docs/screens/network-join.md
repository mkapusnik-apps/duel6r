# NET-03 — Join setup and connecting

## Status, purpose, and requirements

Entry may also come from `NET-10` with the selected listing, endpoint, and session identity. `NET-03-password` permits common password entry and correction for both hosting modes under [NET-PASS-001 through NET-PASS-018](../network-host-directory.md). Complete ongoing admission enters current `NET-05`, immediately participating only when committed strictly before the first-round outcome; otherwise it enters `NET-05-waiting-for-next-round` under NET-ADM-001–016. Lobby admission enters `NET-04`. Initial snapshots mean current authoritative lobby or match state, including waiting ownership. Functional acceptance uses NET-DIR-AC-003, NET-PASS-AC-001–005 and NET-ADM-AC-001–003. Failure permits return to `NET-10` and refresh.

### Public pilot functional contract

The states `NET-03-PUBLIC-EDIT`, `NET-03-PUBLIC-CONNECTING` and `NET-03-dedicated-selected` apply NET-PUB-017 through NET-PUB-020/024–025, NET-DIR-021 through NET-DIR-033, current TRU-PUB and common NET-PASS-001–018. They provide common-channel Direct and browser-selected dedicated admission without replacing directory origins. `NET-03-PUBLIC-EDIT` is the common unbound Direct task for either hosting kind, not a dedicated security choice. Existing local-player setup, immutable admission ownership, Cancel, deadline, compatibility and confirmed-success requirements remain applicable. All supported client platforms, including Linux AArch64/Pi 5 and experimental Apple Silicon macOS 14+, may join either hosting kind under NET-PUB-AC-006.

- **NET-JOIN-PUB-001** Direct-connect setup on every supported client platform must use the common EC-JPAKE channel without asking the user to choose a server type or security profile. Browser-origin setup must retain the validated selected hosting kind, endpoint and expected nonzero session identity without an editable hosting-kind selector. Hosting kind must identify lifecycle context, not a different secure channel.
- **NET-JOIN-PUB-006** Cancel or Edit setup must retain the endpoint, local-player setup, and any selected-session binding. Common Password correction and erasure must follow NET-PASS-013.
- **NET-JOIN-PUB-008** Successful dedicated admission must enter NET-04 or NET-05 under NET-ADM-011 with the service-confirmed controller or guest role.
- **NET-JOIN-PUB-009** The client must not infer controller authority from an empty lobby, connection order, or a local choice.
- **NET-JOIN-PUB-010** The endpoint input must accept an ASCII DNS hostname or an IPv4 literal separately from an integer port from 1 through 65535.
- **NET-JOIN-PUB-011** Endpoint validation must reject schemes, URL paths, embedded ports, whitespace, and IPv6 without starting a connection attempt.

An ASCII DNS hostname uses dot-separated labels of 1 through 63 letters, digits, or hyphens, with a letter or digit at each label end and a maximum total length of 253 characters. No trailing dot is accepted. An IPv4 literal has four decimal octets from 0 through 255. Port input contains ASCII digits only. Both hosting kinds use this input syntax and the current resolution policy, including loopback and eligible public-unicast IPv4 destinations. The client must not infer hosting kind from the address. Direct setup must not claim a controller or guest role before actual admission confirms it.

NET-JOIN-PUB-002 remains retired with the hard-coded production prefill. NET-JOIN-PUB-003–005/007/012–019 and NET-JOIN-PUB-AC-003 are retired invitation-only identifiers; none may be reused. NET-PASS-001–018 and NET-PASS-AC-001–005 own credential behavior without creating a new screen or state. Dedicated setup uses the operator common-channel endpoint and separate port under NET-PUB-017/NET-DIR-024, not a mandatory certificate frontend. A browser endpoint must not be wildcard, loopback, directory origin or fictional local interface; independent same-machine Direct joining may use loopback. Browser-origin setup remains bound to hosting kind, protection, endpoint and expected nonzero session; it offers no editable kind or fallback. NET-DIR-032 gives all supported clients the same dedicated eligibility. Exact release/content checks and directory-origin boundaries remain unchanged.

Both hosting modes use the same separate Password field, exact UTF-8/space/case validation and atomic typing/explicit-paste procedure under NET-PASS-011–013. Direct setup has unknown protection and accepts empty Password. Known protected selection blocks empty submission and initially focuses Password; No password selection initially focuses local setup with Password optional and reachable. No password values are fetched automatically from the clipboard, copied into an endpoint or sent as application admission proofs. Explicit non-empty input must be honored even for No password; it is never silently discarded or retried unlocked. A rejected insertion leaves the previous value unchanged and eligible under ordinary setup checks, with feedback but no sticky invalid history.

Cancel, Edit setup and initial password rejection retain editable correction only in the same permitted context. Endpoint/port or listing/session changes and explicit unbound Direct transition erase the old context's password. A selected hosting-kind change also invalidates the old context; it must not trigger a security-profile switch. Complete confirmed admission erases the editable UI buffer; only zeroizable channel state may survive for the established connection/reserved reconnect. Final Leave, End, terminal reconnect failure, context disposal and shutdown erase retained state. Only the scoped reconnect credential restores old authority; the password cannot become fresh admission or migration.

In `NET-03-dedicated-selected`, credential correction, Cancel/Edit setup and eligible secure Retry must preserve selected hosting kind, endpoint and expected identity. That nonzero identity is bound before authority allocation and through offer, final confirmation, initial snapshot and reconnect grant. An idle selected session may become lobby or first-round while setup is open; actual authority confirms role and destination for the same identity. End/expiry/restart invalidates an old selection immediately and it must reject before allocation, never become a fresh session silently. Browser-origin recovery permits return to NET-10 for refresh. Only an explicit unbound Direct transition clears selection binding and secrets and begins the direct contract; it must not rewrite or bypass the selected session.

Presentation authority: [NET-03 UX contract](../design/screens/NET-03.md). The service-type selector is removed; separate Port input remains required. No certificate enrollment, pin entry, trust prompt, or nested connection task is required. UX owns common masked Password presentation, labels, geometry, protection-aware focus and non-secret rejection feedback. There is no reveal action; paste is an explicit action for the focused Password field. `Host` may label the service-confirmed controller role; it never means ownership of the dedicated process or management of the operator password. Selected dedicated context may explain controller consequences, but unbound Direct must not promise that role.

The common-channel endpoint, explicit unbound Direct transition and browser-derived locked hosting/session binding are functional behavior. UX owns controls, grouping, focus, credential-entry presentation and feedback. A directory row does not establish reachability, authenticated gameplay endpoint identity, admission or controller authority. Unlocked first contact has the same active-intermediary limitation in both hosting kinds under NET-PASS-010. Controlled verification can use an isolated operator-configured common-channel endpoint without a new domain or live cloud activation.

**NET-JOIN-PUB-AC-001:** Unbound Direct setup accepts either hosting kind's endpoint without a service-type selector. Both hosting kinds use the same real EC-JPAKE exchange for Password and No password, without certificate setup or legacy retry. Browser-origin setup retains its validated hosting kind, protection, endpoint and expected nonzero identity without overwriting selection or directory origin. Target/context changes erase the old password. Known protected empty submission is blocked; Direct unknown/No password setup permits empty input and honors explicit non-empty input. Cancel/Edit setup and password correction preserve the permitted context and binding under NET-PASS-013. Only complete admission selects the service-confirmed role and current NET-04 or NET-05 destination. Expired/ended/restarted binding rejects before authority allocation without redirecting a fresh session. This criterion covers NET-JOIN-PUB-001/006/008–009, NET-PASS-001–018 and NET-DIR-027–029.

**NET-JOIN-PUB-AC-002:** Valid hostname/IPv4 and port input accepts endpoints for either hosting kind, including direct same-machine loopback. URL syntax, embedded ports, whitespace, IPv6, and invalid port bounds remain editable without a connection. The screen has no server-type selector and must not infer hosting kind from the address. This criterion covers NET-JOIN-PUB-001/010/011.


This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It configures a guest's direct endpoint and local players, then truthfully reports connection progress. It implements `NET-AC-001`, `NET-AC-002`, `NET-AC-004`, `NET-AC-005`, `NET-AC-007`, `NET-AC-008`, `NET-AC-009`, `NET-AC-016`, `NET-AC-017`, and `NET-AC-019` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
It preserves `INP-001` through `INP-010` and implements `NIN-OWN-006` and `NIN-BOUND-003` in [`docs/network-authoritative-player-input.md`](../network-authoritative-player-input.md).
It implements `NET-VIS-001`, `NET-VIS-002`, `NET-VIS-009` through `NET-VIS-011`, `NET-VIS-AC-001`, `NET-VIS-AC-004`, and `NET-VIS-AC-005`. It also consumes `CMP-VIS-001` through `CMP-VIS-004`, `CMP-VIS-AC-001`, and updated `AC-012` from [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
It implements `NET-OWN-001` through `NET-OWN-003` and `NET-OWN-AC-001` through `NET-OWN-AC-002`. Successful admission consumes `ADM-OWN-001` and `ADM-OWN-AC-001` from the compatibility and admission specification.
Issue #30 defines the compatibility and admission outcomes for this flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.

Entry is `NET-01` → Join → mixed `NET-10` → Direct connect or eligible player-hosted/dedicated selection. Direct connect opens independent direct setup without a selected session and clears binding/secrets. Complete validated admission enters current `NET-04` or `NET-05`, including Waiting under NET-ADM-011. Failure enters `NET-08`; Cancel restores permitted setup. Direct Back returns to the `NET-01` popup; browser-origin recovery returns contextually to `NET-10` without adopting another selection. Known later-match context reads `Match in progress. Your players join next round if admitted.` Known final context reads `Final round in progress. You may not play in this match if admitted.` Unknown context makes no immediate-play promise; the host confirms availability.

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
- Connect starts the single 10-second attempt. Guest-local gameplay-manifest validation, resolution, secure connection, request, admission offer, exact guest acceptance, atomic host commit, host-clock calibration, final `admitted` confirmation, initial full snapshot validation, and lobby or live-match handoff all share that boundary. There is no separate offer timer.
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
- A guest must validate the exact final confirmation, one valid host-clock calibration result, and one complete valid initial full snapshot before it enters `NET-04` or `NET-05` under NET-ADM-011.
- The guest must receive all three success inputs strictly before the single total deadline.
- The final confirmation must contain the exact offered participant identity, original player count, and ordered player identities.
- The initial snapshot must contain the same participant identity and ordered owned-player identities.
- Successful admission must fix the confirmed ordered player identities and ownership while that participant remains admitted.
- The snapshot production time must be valid under the host-clock calibration result.
- A final confirmation without valid calibration and a valid initial snapshot must not report success.
- A malformed, trailing, unexpected, or inconsistent complete host offer, rejection, or confirmation closes the attempt as `invalid-host-admission-message` with `Connection ended before admission completed.`.
- A started match alone is not rejection. Final-outcome admission closure uses `Match completed. Join when the host returns to the lobby.` without a first-round cutoff claim. Waiting after complete admission is not a timeout or reconnect failure.
- `NET-08` Retry repeats the retained attempt, Edit setup returns here with all data retained, and Return to Network enters `NET-01`.

## Truthful copy, disabled reasons, and input

- Example reasons are `Enter a hostname or address`, `Enter a valid port (1–65535)`, `Add at least one local player`, and `Assign a valid control to every local player`.
- Accounts, automatic LAN discovery, and matchmaking remain excluded. NET-DIR, NET-PASS, and NET-PUB govern the approved browser, password, and dedicated setup paths.
- Focus order is Hostname/address → Port → local-player controls → Connect → Back. During connection, focus is Cancel.
- Keyboard Tab/Shift+Tab and controller directions traverse controls; Enter/Space/controller Confirm activates; Escape/controller Back cancels an attempt or returns to `NET-01` from editable setup.
- A result must remain visible as text until the user selects a recovery action.
- The guest must be able to select any established keyboard preset or detected supported controller preset for each owned local player.
- The setup must permit the same control preset for more than one owned local player.
- Controller detection and connection changes must preserve the established local device behavior.
- A profile or cosmetic difference must not create an inline warning, compatibility failure, or disabled Connect state.
- A required default network visual resource failure must use the existing required-resource failure behavior and must not load peer content as a fallback.

Planned representative screenshot: [`SS-017`](../screenshots/README.md#ss-017).
