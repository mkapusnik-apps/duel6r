# NET-02 — Host setup

## Status, purpose, and requirements

Current functional target: `NET-02-password` is the optional password setup state under [NET-PASS-001](../network-host-directory.md). The host must be able to start without a password or set a session password before startup. Publication must follow NET-DIR-001 and NET-DIR-015; directory failure must not convert a ready host into a failed host. Listening-address eligibility follows the updated NET-HOST-IF-002 and NET-HOST-IF-003. These targets supersede earlier password and public-address exclusions. Functional acceptance uses NET-PASS-AC-001, NET-DIR-AC-001, and NET-AC-002; UX owns controls and presentation.

This screen is implemented and accepted for issue #38 at checkpoint `e70a057819c97100b083c3cdaae5dc24566435cd`. It collects the listening interface, direct listening port, and host local players before creating a player-hosted session. It implements `NET-AC-001`, updated `NET-AC-002`, `NET-AC-003`, `NET-AC-004`, `NET-AC-005`, `NET-AC-009`, `NET-AC-015`, `NET-AC-016`, `NET-AC-017`, `NET-AC-019`, `NET-HOST-IF-001` through `NET-HOST-IF-012`, and `NET-HOST-IF-AC-001` through `NET-HOST-IF-AC-006` in [`docs/network-play-first-release.md`](../network-play-first-release.md).
It preserves `INP-001` through `INP-010` and implements `NIN-OWN-006` and `NIN-BOUND-003` in [`docs/network-authoritative-player-input.md`](../network-authoritative-player-input.md).
It implements `NET-VIS-001`, `NET-VIS-002`, `NET-VIS-009` through `NET-VIS-011`, `NET-VIS-AC-001`, `NET-VIS-AC-004`, and `NET-VIS-AC-005`. It also consumes `CMP-VIS-001` through `CMP-VIS-004`, `CMP-VIS-AC-001`, and updated `AC-012` from [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
It implements `NET-OWN-001` and `NET-OWN-AC-001`. Successful startup consumes `ADM-OWN-001` and `ADM-OWN-AC-001` from the compatibility and admission specification.
It consumes `TRU-BIND-001` through `TRU-BIND-007` from [`docs/network-trust-and-abuse-limits.md`](../network-trust-and-abuse-limits.md).
It consumes `HSL-IF-001` through `HSL-IF-003` and `HSL-AC-019` from [`docs/network-host-service-lifecycle.md`](../network-host-service-lifecycle.md).
Issue #30 defines the host compatibility result for this flow in [`docs/network-compatibility-and-admission.md`](../network-compatibility-and-admission.md).
Issue #30 must not implement this graphical screen.
Issue #31 defines the hosted-service lifecycle for this flow in [`docs/network-host-service-lifecycle.md`](../network-host-service-lifecycle.md).
Issue #31 must not implement this graphical screen.
Issue #38 owns its graphical controls, focus, disabled reasons, and visual evidence.

Entry is `NET-01` → Host. Successful confirmed startup enters `NET-04`; startup failure enters `NET-08`; Back returns to `NET-01` before a session exists.

## Representative layout

- Use the scaled retro canvas with a `HOST NETWORK SESSION` title.
- Show an editable Port field first in the endpoint region.
- Show a visible `Listening interface` selector directly below Port and above the split setup panels.
- Show a read-only support note: `Same machine or LAN • Linux / Windows x86-64`.
- Show IPv4 loopback and each currently eligible assigned private RFC1918 IPv4 address in `Listening interface`.
- Show the IPv4 literal before a scope label such as `Same machine` or `Private LAN`.
- Do not show a wildcard, unspecified, public, multicast, link-local, unassigned, network, or broadcast address.
- Show the host's local Persons and Local Players panels with person and control assignment for each selected player.
- Do not show a profile selector, profile column, profile value, or profile-editing action.
- Show a capacity line such as `Local players: 2 • Lobby 1–15 • Match 2–15 participants and players`.
- Footer actions are `Start session` and `Back`, with a persistent reason line below or adjacent to Start session.
- Keep the screen content inside the shared 24-logical-pixel canvas margin.
- Use a fixed title, endpoint, selector, and support-note region, a flexible setup region, and a fixed status and action region.
- Keep Port above `Listening interface` without reducing its width or changing its initial-focus position.
- Keep the collapsed selector on one row and keep the complete selected IPv4 literal visible.
- Keep the split setup-panel top edge fixed when the selector opens.
- Overlay the expanded option list above lower content instead of moving the split panels.
- Limit the expanded list to the available canvas height and scroll its option rows when necessary.
- Keep one address per option row.
- Clip descriptive option text only after the complete IPv4 literal.
- Split the flexible setup region between Persons and Local Players with one 8-logical-pixel gap.
- Keep the two setup regions equal in height.
- Let each setup list scroll vertically without moving its title or actions.
- Keep each person and control assignment on one row.
- Clip an overlong row value inside its column.
- Keep the Port field wide enough for five digits and keep the full value visible.
- Keep the persistent validation or lifecycle status above the footer actions.
- Wrap a long status at word boundaries and grow the status region upward without covering setup controls.

## Navigation and significant variants

- Editable setup is the representative state. Start session remains disabled until Port, `Listening interface`, and every local-player requirement are valid.
- First entry must select IPv4 loopback by default.
- The host must explicitly select an eligible assigned private RFC1918 IPv4 address before it hosts on that address.
- A multihomed host must retain the host's explicit private-address choice and must not select one private address automatically.
- Start session must revalidate the selected listening address before service creation or the startup clock begins.
- An address that is no longer eligible must clear the valid selector state, keep `NET-02` editable, and show `Selected listening interface is no longer available. Choose another interface.`
- An ineligible address must not start a service attempt or create a listener.
- Editable setup must let the host add or remove local player slots before Start session begins.
- Start session must finalize the displayed local-player count and ordered slot set for the startup attempt.
- Starting and Cancelling must not permit a local player slot to be added, removed, transferred, or reordered.
- Starting must show exactly `Starting session…` and `Startup can take up to 10 seconds.`.
- Starting must lock every setup control.
- Starting must replace Start session and Back with Cancel.
- Starting must not show Retry or permit another startup attempt.
- Starting must not say `Listening`, `Ready`, `Connected`, or `Playable`.
- Accepted Cancel must show exactly `Cancelling session…` until cleanup completes.
- Cancelling must keep every setup control locked and must not accept another Cancel.
- Completed Cancel must return to editable `NET-02` with the port and local-player setup retained.
- Completed Cancel must retain the selected listening address when that address remains eligible.
- Completed Cancel may permit the host to change the retained local-player count before a new attempt.
- Completed Cancel must not show a success or failure message.
- Confirmed startup enters `NET-04` and identifies this participant as Host.
- Confirmed startup must commit the exact ordered host player identities and ownership for the displayed slot count.
- Confirmed readiness strictly before 10 seconds enters `NET-04`; at or after 10 seconds startup fails and leaves no listener.
- A port conflict or transport startup failure that is confirmed before timeout enters `NET-08` with the specific supported reason.
- A hosted-service exit before readiness enters `NET-08` with `Hosted session stopped before it was ready.`.
- A startup attempt without a complete result at the deadline enters `NET-08` with `Hosted session startup timed out.`.
- Another confirmed startup failure enters `NET-08` with `Hosted session could not start.`.
- An invalid host gameplay-content manifest must enter `NET-08` with `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.`
- An invalid host gameplay-content manifest must leave no listener or session.
- An invalid host gameplay-content manifest must disable Retry for the current application session.
- Edit setup must return here with the listening address, port, and local-player setup retained when the address remains eligible.
- Eligible Retry must reuse the retained listening address after cleanup and must revalidate it before startup.
- Return to Network must enter `NET-01`.
- A confirmed invalid host manifest must take precedence over a later startup timeout.
- An accepted user Cancel must take precedence over a later startup result.
- An accepted application shutdown must take precedence over a later startup result.
- A complete specific failure before the deadline must take precedence over a later timeout.
- Readiness must enter `NET-04` only when it is complete strictly before the deadline.
- Back discards uncommitted network setup only; it does not alter the local-only `MENU-01` setup.

## Truthful copy, disabled reasons, and input

- Example disabled reasons are `Enter a valid port (1–65535)`, `Select an eligible listening interface`, `Selected listening interface is no longer available. Choose another interface.`, `Add at least one local player`, and `Assign a valid control to every local player`.
- No dedicated-server, Internet exposure, password, discovery, or NAT control may appear.
- Interface enumeration and selection must not change an interface, route, firewall, Docker network, NAT rule, port-forwarding rule, or other network infrastructure.
- Interface enumeration must not discover another host or session.
- Editable focus order is Port → `Listening interface` → Persons/local-player controls in reading order → Start session → Back.
- The selector must use a textual focus indicator that does not rely on color.
- Enter, Space, or controller Confirm must open the collapsed selector or select the highlighted option.
- Keyboard or controller directional input must move through interface options while the selector is open.
- Escape or controller Back must close the expanded selector and retain focus on `Listening interface`.
- Escape or controller Back must activate Back only when the selector is closed and no startup is active.
- Starting focus must move to Cancel.
- Cancelling must keep a visible status, but it must have no activatable control.
- Tab or directional input must move focus.
- Enter, Space, or controller Confirm must activate the focused control.
- A pointer may open the complete collapsed selector or select any complete visible option row.
- Escape or controller Back must activate Cancel while Starting.
- Every selected local player and control assignment must be operable by keyboard and controller, with a visible textual focus state.
- The host must be able to select any established keyboard preset or detected supported controller preset for each owned local player.
- The setup must permit the same control preset for more than one owned local player.
- Controller detection and connection changes must preserve the established local device behavior.
- A selected person must not imply that the person's selected-profile appearance will appear in network play.
- A profile or cosmetic difference must not create a setup warning or prevent Start session.
- A required default network visual resource failure must use the existing required-resource failure behavior and must not load peer content as a fallback.
- The invalid host-manifest reason and disabled Retry reason must remain readable without color, sound, or transient motion.

Planned representative screenshot: [`SS-016`](../screenshots/README.md#ss-016).
