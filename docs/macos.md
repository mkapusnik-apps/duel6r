# Experimental macOS local play

## Product scope

This document owns the macOS platform contract. [features.md](features.md) remains authoritative for local-game behavior. The macOS target is experimental implementation support, not verified platform support.

- **MAC-PLAT-001** The macOS application must target Apple Silicon on macOS 14 or later.
- **MAC-PLAT-002** The macOS application must provide local play with the existing two-through-15-player roster and all existing local game modes.
- **MAC-PLAT-003** The macOS application must use the existing `gl1` renderer.
- **MAC-PLAT-004** The macOS extension must preserve the behavior, packaging, and renderer defaults of other platforms.

Intel Macs, older macOS versions, network play on macOS, a networking port, new renderers, gameplay changes, visual redesign, signed or notarized distribution, App Store distribution, and cross-platform save migration are outside this contract.

## Application and distribution

- **MAC-DIST-001** The macOS distribution must provide an unsigned `.app` with its required runtime resources and dependencies.
- **MAC-DIST-002** The user must be able to launch the application from Finder without selecting a process working directory.
- **MAC-DIST-003** The nightly release must provide a separate macOS Apple Silicon artifact without replacing the existing platform artifacts.
- **MAC-DIST-004** The artifact documentation must identify the target, source revision, unsigned status, installation and launch procedure, and experimental verification status.
- **MAC-DIST-005** The launch instructions must explain the applicable macOS approval procedure without requiring the user to disable system-wide security protections.

## Saved data

- **MAC-SAVE-001** The macOS application must store `data/persons.json` under `~/Library/Application Support/Duel 6 Reloaded/` for the current user.
- **MAC-SAVE-002** The macOS application must create the required saved-data directories when they do not exist.
- **MAC-SAVE-003** The macOS application must preserve person records, roster membership, statistics, Elo data, and the played-round count across application restarts according to PER-002 through PER-005.
- **MAC-SAVE-004** The macOS application must not require write access to its `.app` to save person data.
- **MAC-SAVE-005** Replacing the macOS application with a nightly application must not replace or delete existing per-user person data.

MAC-SAVE-001 is the macOS-only location exception to PER-001. It does not change the saved-data format, save timing, missing-file behavior, or data locations on other platforms.

## Network entry

- **MAC-NET-001** The macOS application must keep the existing `Network (F2)` button available to report the local-only restriction.
- **MAC-NET-002** A pointer activation of `Network (F2)` or an F2 key press must show the existing `MENU-02` message strip without initializing networking, entering a network screen, or starting a network service.
- **MAC-NET-003** The message must show exactly `Network play is unavailable in this macOS build. Use Play (F1) for local play. Press any key.`.
- **MAC-NET-004** The macOS restriction must not change Local Play or network entry on other platforms.
- **MAC-NET-005** Any keyboard key must dismiss the message and return to the retained menu state.
- **MAC-NET-006** The application must consume the dismissal key without activating its normal menu action.
- **MAC-NET-007** The message and its dismissal must preserve the roster, control assignments, match settings, person data, and menu selection.
- **MAC-NET-008** The window close action must remain available while the message is visible.

These requirements define the platform-specific `MENU-01-MAC-LOCAL` state and its `MENU-02-MAC-LOCAL` message variant. They do not add a screen. [The existing screen inventory](screens/README.md) remains authoritative. The approved UX approach reuses `MENU-01-A`, `MENU-01-B`, and `MENU-02` wireframes unchanged, including the existing Network button and message-strip presentation. Existing network contracts remain unchanged for their approved targets.

## Acceptance boundaries

### Experimental implementation acceptance

- **MAC-AC-001** Build and package evidence identifies an Apple Silicon macOS 14-or-later target, `gl1`, and a complete unsigned `.app` at an immutable source revision.
- **MAC-AC-002** Packaging and launch-path review establish that application resources do not depend on the caller's working directory and that person data uses MAC-SAVE-001 rather than the application bundle.
- **MAC-AC-003** Focused behavioral results and source review establish the macOS Network restriction, unchanged Local Play selection, and unchanged network entry on other platforms.
- **MAC-AC-004** Nightly publication configuration provides the separate macOS artifact and preserves existing artifacts; the published artifact must subsequently identify its immutable source revision and experimental status.
- **MAC-AC-005** Relevant regression results and review show that the extension preserves other platforms and unrelated functional requirements.

Declarative package and publication behavior may use source/configuration review and relevant build checks. These checks do not establish successful Finder launch, macOS input, audio, rendering, or gameplay.

### Deferred macOS runtime verification

- **MAC-AC-006** On an approved Mac, the user can obtain the nightly artifact, approve and launch the unsigned application from Finder, reach the menu, quit, and relaunch without a resource-path failure.
- **MAC-AC-007** On that Mac, the existing local-player loop works with keyboard and supported controllers, audible sound, combat, pickups, water and elevators, round progression, summaries, and return to menu in Deathmatch, Predator, and Teams.
- **MAC-AC-008** On that Mac, person data survives restart and application replacement without requiring bundle write access.
- **MAC-AC-009** On that Mac, the existing menu, shared arena, and summary presentation remains usable and conforms to the approved UX specifications, including the unavailable Network state.

The user owns manual Mac verification after merge and the subsequent nightly publication. MAC-AC-006 through MAC-AC-009, including macOS gameplay, visual assessment, and manual QA, are **deferred, not passed**. Their absence does not block pre-review or experimental implementation acceptance. Actual nightly publication evidence for MAC-AC-004 follows publication; pre-merge review assesses its configuration only. No pre-merge claim may state that the unpublished artifact was verified.

Runtime observations must identify the nightly artifact, immutable source revision, Mac architecture, macOS version, and relevant display and input context. Team coordinates any UX evidence within the existing UX-owned matrix. Existing wireframes remain specification inputs; this extension does not require a redesign or an independent duplicate screenshot campaign.

## Execution policy boundary

The approved macOS work permits a narrow native macOS execution exception for building, packaging, testing, and running this target where Docker cannot provide the required macOS environment. DevOps owns the corresponding repository instruction wording through team. The exception does not change the Docker-only policy for other platforms. Product acceptance does not authorize execution by the product role.
