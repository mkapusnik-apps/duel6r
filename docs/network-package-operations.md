# Network package operations

## Status and scope

**Experimental/prototype networking: packaging is not accepted end-to-end network support.** This guide describes the production-path procedures to validate against the [deployment requirements](network-deployments.md). It does not remove the prototype warning or approve release readiness. Issue #41 remains the final network release gate.

The target is Linux x86-64 and Windows x86-64, player-hosted sessions on a trusted machine or trusted private IPv4 LAN, including Linux/Windows participants together. Dedicated hosting, Internet access, NAT traversal, port forwarding, relays, public hosting, accounts, passwords, authentication, and encryption are not supported. Do not expose the listener to untrusted people, even on a private network. Do not use a hostile shared machine.

### Evidence boundary

Prior non-checkpoint diagnostics reported Linux and Windows cross-build success, Windows dependency checks, manifest verification, and Linux library resolution and menu smoke on fresh Ubuntu 24.04. Those results do not establish acceptance of this guide or a later checkpoint. Package-specific evidence must identify its commit, artifact hash, platform, scenario, and observation.

Windows desktop production admission, graphical Local Play, reinstallation, rollback, and cross-platform LAN execution remain unverified. The available Linux Docker daemon cannot supply a Windows desktop. Repository instructions require Docker-only project execution; a Windows desktop environment and any needed policy exception must be explicitly approved before execution. Do not substitute Wine, transport echo, synthetic admission, source inspection, or compilation for these observations. Missing evidence does not remove Windows from the approved target.

## Identify and install a complete package

Obtain the complete artifact and its independently supplied archive SHA-256 and source commit from the release/evidence owner. A branch name or the network release ID alone does not identify an artifact. Reject an archive whose supplied digest does not match. The package's own checksums detect corruption, not authenticity; obtaining both a package and a changed checksum from an untrusted source proves nothing.

The current packaging layout may contain both operating systems in one archive. Extract the entire archive into a new, writable, local directory. Do not merge it into another installation. Keep separate directories for separate instances on one machine, so Local Play saves cannot overwrite each other. Do not use symlinks, junctions, hard-linked gameplay files, or a network share for the installation. The compatibility loader rejects unsafe content paths and filesystem aliases.

| Target | Required executable components | Integrity inventory |
|---|---|---|
| Linux x86-64 | `duel6r`, `duel6r-server`, `duel6r-host-supervisor`, `duel6r-resolver` | `linux-x86_64.sha256sums` |
| Windows x86-64 | `duel6r.exe`, `duel6r-server.exe`, `duel6r-host-supervisor.exe`, `duel6r-resolver.exe`, bundled DLLs | `windows-x86_64.sha256sums`, `windows-dependencies.txt` |

Both layouts also require `data/`, `levels/`, `profiles/`, `shaders/`, `sound/`, and `textures/`; `README.md`, `LICENSE`, and `docs/` ship with the package. Preserve execute permissions on Linux. The server, supervisor, and resolver are player-hosted implementation components, not a supported standalone or dedicated deployment. Start the graphical client, not a diagnostic server command.

Before first launch or data restoration, verify every listed file against the target's SHA-256 inventory from the extracted package root. On Linux, `sha256sum -c linux-x86_64.sha256sums` checks the inventory. On Windows, use a trusted SHA-256 verifier to compare every listed relative path with `windows-x86_64.sha256sums`; `Get-FileHash -Algorithm SHA256` can calculate individual file digests. Do not rely on a spot check. The inventories cover platform executables, resources, and documentation (and Windows DLLs), not every possible extra file, the archive itself, or user-created saves. Verify both inventories when preparing both targets from a shared archive. Never repair an integrity mismatch by rewriting the inventory.

### Runtime prerequisites

For the Ubuntu 24.04 x86-64 Linux package, the runtime package set validated in prior diagnostics is:

```text
libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0
libglew2.2 liblua5.3-0 libgl1-mesa-dri libstdc++6
```

Install these with their normal distribution dependencies in the runtime environment. A working display, graphics driver compatible with the selected renderer (default build: OpenGL 4), audio support, and keyboard/controllers are also needed. This is not a claim that arbitrary Linux distributions or older library ABIs work. `xvfb`, `xdotool`, and ImageMagick are evidence tools, not player dependencies. A compiler, headers, and a development checkout are not player prerequisites.

Windows packages are x86-64 MinGW builds with non-system DLL dependencies beside the executables. Keep all supplied DLLs and music decoder libraries together; do not download replacement DLLs individually or take them from another game release. Windows-provided system DLLs and a compatible OpenGL graphics driver remain prerequisites. Native desktop/Windows-version acceptance is pending; transport-only MSVC CI does not qualify this MinGW graphical package or establish a minimum supported Windows version.

### Working directory and launch

Use the extracted package root as the process working directory: resource and save paths are relative to it. The client is `./duel6r` on Linux and `.\duel6r.exe` on Windows, with no extra console-command arguments needed. The graphical client locates its server beside its executable and uses the working directory as its resource root. Keep both locations consistent. Configure a launcher/shortcut's working directory accordingly.

For repository validation these launch instructions apply only inside an approved Docker runtime with the package, prerequisites, display, and input configured by the evidence owner. They are not authorization to run on the native host or change firewall/display infrastructure. Windows graphical validation must wait for the approved environment/policy decision above. A developer build container containing extra libraries must not be passed off as a clean-machine installation.

## Host, join, and stop

### Endpoints and manual firewall configuration

The production transport is TCP over IPv4. The default port is **26660**; the UI accepts an integer port from 1 through 65535. Use the same selected port at host and guests. Prefer an available unprivileged port; do not run the game as administrator/root to obtain a privileged port. There is no separate discovery or UDP game port.

For same-machine sessions, retain the default loopback listening interface (`127.0.0.1`) and connect separate instances to that address and port. Loopback on a guest machine never refers to another LAN machine.

For private LAN sessions, explicitly select the host's assigned eligible RFC1918 IPv4 address in **Listening interface**. Eligible ranges are `10.0.0.0/8`, `172.16.0.0/12`, and `192.168.0.0/16`; the address must also be a valid assigned unicast address for that interface prefix. Do not select wildcard/public/link-local/multicast/network/broadcast or unassigned addresses. The application validates the selection again at startup. On a multihomed machine, choose the intended trusted interface deliberately.

An authorized operator must manually allow inbound TCP on that exact host interface/port from the intended trusted peers and permit guest outbound connections. Keep the rule restricted to the private network; do not disable the firewall globally. Remove a temporary rule after use under the operator's policy. The application does not open firewall rules, discover hosts, configure routing, or forward ports. Docker-based LAN observations require an approved network topology that exposes the intended private endpoint; a successful internal container loopback test is not LAN evidence. Do not change shared Docker networks or host routes merely to collect evidence.

### Production host and guest procedure

1. Verify the packages and common gameplay content while all instances are stopped. Launch each instance from its own package root. Configure local people and control assignments as needed; do not share save directories between instances.
2. Open **Network**, then **Host**. Select the listening interface, port, and local-player setup before **Start session**. A host-alone lobby is valid, but a host-alone match is not.
3. Wait for completed host startup and the lobby. A spawned process, bound socket, or transport echo is not confirmed readiness: the host must complete content/compatibility validation and host admission as well as listening. Startup has a ten-second deadline; a failure must not be interpreted as a ready service.
4. Guests open **Network**, then **Join**, enter the host IPv4 address or hostname and port, choose their local players, and connect. A permitted hostname must resolve to an allowed loopback/private IPv4 destination. Use the actual host's private address for LAN checks.
5. Confirm that each guest reaches the lobby with its admitted participant and owned players visible. A connected TCP socket or progress label alone is not admission. No join-in-progress is supported: admit guests before the match starts.
6. The host chooses match settings, levels, and roster order. Each participant configures its own person/control assignments. Ready each participant after configuration; changes that invalidate readiness require confirmation again. A match requires 2–15 connected participants and 2–15 roster players, at least one player per participant. First-release network rounds are limited to 1–99; Local Play keeps its separate unlimited-round behavior.
7. The host starts only when all participants are ready. For package verification, confirm the real session enters gameplay and follows the expected lifecycle; lobby admission alone is not a complete-match observation. Network results are session-only and do not write local statistics or Elo.
8. To stop intentionally, the host uses **End session** and confirms it. Guests use **Leave session** (or **Leave** on the final summary) when leaving individually. Wait for cleanup and the corresponding destination before exiting every client. Ending the session loses its retained results; a new launch does not restore that session.

For cross-platform LAN coverage, perform the procedure with Linux as host and Windows as guest, then with Windows as host and Linux as guest, using the matching package set. Same-OS loopback checks on each target are also required. Neither direction substitutes for the other.

### Cancellation, failure, and cleanup

Use **Cancel** during startup/connection rather than terminating the process. Host cancellation returns to editable setup only after cleanup. On failure, read the fixed error; edit setup or retry only after the prior attempt has been cleaned up. If the port is unavailable, select another permitted port and tell guests the new value. Do not kill an unrelated listener.

Intentional host **End session** is different from closing/crashing the host application or losing transport. There is no host migration. A guest in reconnecting state must not assume the host ended intentionally; it may regain its reserved identity within the existing 30-second reconnect window only under the lifecycle/compatibility rules. Package replacement always ends the old session and never uses reconnect as an upgrade mechanism.

Owned-service cleanup has a three-second deadline when the OS honors termination, followed by force-stop supervision if necessary. Elapsed time is not proof of cleanup. Before replacement, verify all participating clients have exited, the owned server/process tree has terminated, and the selected listener is gone. If termination cannot be confirmed, stop the procedure and report the incomplete cleanup; do not replace locked/running files or start another service. Do not force-kill processes based only on a shared executable name.

## Local data, backup, and same-release reinstallation

There is no updater or automatic migration. Supported replacement is **offline complete same-release reinstallation on the same OS**, not a partial overlay. Keep an untouched copy of the original complete package and a separate recoverable backup of the installation's local data outside the installation directory. Never overwrite the only backup during replacement or verification.

| Location relative to the package working directory | Backup/restore meaning |
|---|---|
| `data/persons.json` | Persistent people, statistics/Elo, and saved roster/round data written by Local Play. It may be absent on a clean installation. Record absence rather than inventing a save. |
| `profiles/` | Local profile JSON, artwork, sounds, and optional scripts. Preserve the complete directory and any user-supplied files it references. Profile scripts remain disabled in supported network matches. |
| `data/config.script` | Startup console configuration; also part of network gameplay-content comparison. Preserve exact bytes. |
| User-created console `archive` files and scripts referenced by configuration | Back up the actual user-selected paths, including paths outside the package if used. There is no single automatic archive filename. Do not assume configuration is stored in a registry or per-user application-data folder. |
| Locally modified `levels/` and `data/blocks.json`, if any | Preserve separately for Local Play; they affect network content equality and are not an approved cross-content migration. Keep the original shipped content too. |

The people file contains names and statistics: treat backups as private data. Copy files after every instance has stopped, not during a save. Record the originating package digest, source checkpoint, platform, and which paths were present. Preserve backup provenance without putting private data into shared evidence.

### Reinstallation steps

1. End the network session deliberately. Stop all participants and confirm owned-service cleanup. All installations must be stopped before any replacement begins; there is no rolling upgrade.
2. Save the complete prior package, its original integrity inventories, and each participant's own pre-reinstallation data backup. Verify the backup can be read and is outside the directory to be replaced.
3. Obtain a complete package of the **same network release and shipped gameplay-content set** for each participant's existing target OS. Verify its trusted archive digest. Host and guest replacement order does not matter once all are stopped.
4. Extract into an empty staging directory and verify its platform inventory before restoring data. Retire the old directory intact and use the complete staged directory as the new installation; do not extract over old files. An existing pathname may be reused only after the old installation is fully moved out of the way.
5. Restore that participant's `data/persons.json` (if present), complete local profiles, `data/config.script`, and applicable user configuration files from its own backup. Do not copy old executables, DLLs, or arbitrary old resource trees over the new package. Keep restored local modifications distinct from the untouched package inventory.
6. Compare restored data with the backup and inspect people/statistics/profiles/settings in the client. Stock integrity inventories may now report intentional changes to restored profiles/configuration; record these rather than regenerating an inventory to disguise them. People saves created after packaging are not necessarily listed. Initial package verification and restored-data verification are separate checks.
7. Before network startup, ensure all participants use equal supported gameplay content. In particular, `data/config.script`, `data/blocks.json`, and regular files under `levels/` participate in the current canonical manifest. Restoring a local preference in `config.script` can therefore cause a correct content mismatch even when its intended effect seems cosmetic. Preserve the restored backup and use a separate clean matching package installation for networking, or deliberately restore the common shipped gameplay files on all stopped installations. Never delete or silently rewrite local data to make admission pass.
8. Start a **new** host session, admit matching guests, and verify ownership/readiness and clean shutdown on the reinstalled packages. Check Local Play separately without networking. Do not mark reinstallation accepted solely because files were copied or the menu opened.

Network session-only results and reconnect identities are not backup data and are not restored. Configuration restoration does not bypass content checks. Copying local data between Linux and Windows is not an approved migration path.

### Complete rollback

Stop every instance and confirm cleanup again. Set aside the failed replacement and its data separately. Restore the **whole saved previous package plus that package's own pre-upgrade data backup** into an otherwise empty installation, on its original OS. Verify the saved package against its original inventory before applying its corresponding data backup, then compare restored data with that backup. All participants must return to matching releases/content before a new host session starts. Verify readiness, actual guest admission, Local Play data, and clean shutdown again.

Do not give an older package data changed by a newer one. If the correct prior complete package or its own backup is missing, rollback is not supported; do not improvise a downgrade. First-release rollback evidence covers restoration after same-release reinstallation only, not a cross-release migration.

### Supported and unsupported paths

| Source → target | Procedure |
|---|---|
| No installation → target package | Clean install into an empty directory; no prior data restoration. |
| Target package → same release and shipped content, same OS | Offline full reinstallation with own backup/restore and new-session verification. |
| Reinstalled target → saved pre-reinstallation package | Complete rollback with that package's own pre-reinstallation backup. |
| Older network prototype → target | No migration support. A separate clean install proves no prototype migration. |
| Different or skipped network release → target | No approved cross-release upgrade path. |
| Newer data → older package without its own backup | Unsupported downgrade. |
| Running installation, mixed files, partial overlay, mixed release/content | Unsupported; stop all participants and use complete matching packages. Passing an observable admission check does not make an unsupported overlay safe. |

## Compatibility and recovery messages

The [compatibility contract](network-compatibility-and-admission.md) is authoritative: admission protocol `1`, exact case-sensitive network release ID `duel6r-network-r1`, and required capabilities `d6r.compatibility-admission.v1`, `d6r.gameplay-manifest.v1`, and `d6r.session-identity.v1`. There is no separate build-version check, protocol range, or release range. Exact gameplay content must match too; the release ID alone is insufficient. Additional capabilities do not cause rejection or grant new authority.

The compatible target combinations are Linux host/Linux client, Linux host/Windows client, Windows host/Linux client, and Windows host/Windows client, all x86-64 with that exact contract and equal canonical content. This is the target compatibility matrix, not a claim of completed execution evidence for each row.

Package inventories and admission manifests are different: inventories cover cosmetic and documentation files too, whereas admission excludes profiles, presentation assets, people, statistics, saves, controls, and documentation. Optional gameplay/profile scripts are not enabled for supported network matches.

| Observable admission failure | Stable identifier | User-visible outcome and action |
|---|---|---|
| Admission protocol mismatch | `protocol-incompatible` | `Network release mismatch. Use the same supported game release as the host.` Obtain the matching complete package; do not edit protocol fields. |
| Release mismatch | `network-release-mismatch` | Same release-mismatch message; stop and use matching packages. |
| Missing required capability | `required-capability-unsupported` | Same release-mismatch message; no degraded mode is supported. |
| Remote invalid-manifest rejection received from the host | `gameplay-content-manifest-invalid` | `Gameplay content manifest is invalid. Use the host's exact supported gameplay content.` Verify a clean package and safe local filesystem layout. Retry the retained initial attempt only when otherwise eligible. |
| Guest-local content validation fails before resolution/connection | `guest-gameplay-content-manifest-invalid` | `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.` Restore supported content and restart; Retry is disabled until restart. |
| Host-local content validation fails before readiness | `host-gameplay-content-manifest-invalid` | `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.` Restore supported content and restart; Retry is disabled for the current application session. |
| Unequal valid gameplay content | `gameplay-content-mismatch` | `Gameplay content mismatch. Use the host's exact supported gameplay content.` Preserve backups and restore common supported content while stopped. |

Local host/guest invalidity is not the remote `gameplay-content-manifest-invalid` rejection. For both local failures, Retry shows `Restart the application to try again.`; no local-invalid guest connection or ready host session is established. After cleanup, **Edit setup** returns to retained host setup (`NET-02`) or guest setup (`NET-03`), while **Return to Network** enters `NET-01`. The remote rejection alone does not impose the local restart requirement; normal Retry eligibility still applies. These distinctions follow [NET-08 outcome mapping](screens/network-failure.md), `HeadlessServer.cpp` local-validation outcomes, `HostServiceSupervisor.cpp` host outcome copy, and `NetworkSessionRuntime.cpp` restart-required mapping. No new protocol error detects installation history or a downgrade. Unsupported procedures are documentation boundaries; runtime rejects only the incompatibilities it can observe under the existing contract.

## Diagnostics and safe reporting

Use the fixed network error/status shown by the client first. The backquote key opens the built-in console. `dump <file_name>` saves its current text buffer to the chosen file, overwriting that destination; choose a new private diagnostic path, never a backup or gameplay file. `archive <file_name>` writes archived console variables, not a full installation backup or network-session save. `exec` and client command-line arguments execute console commands; they are not a server deployment interface and must not contain untrusted commands.

Unhandled client startup errors are reported to standard error and an SDL error dialog. This does not mean all console or service output is continuously recorded to a persistent log. Diagnostic server/supervisor CLI output is not proof of graphical readiness or guest admission. There is no required automatic network logfile to preserve.

Before sharing a dump or crash output, review it for local names, paths, addresses, and private configuration; share the minimum redacted information. Never collect or publish reconnect credentials, raw network payloads, memory dumps containing secrets, or authentication material. Fixed admission errors intentionally omit peer-supplied values. Do not add secrets to generated command lines, environment variables, logs, or bug reports. Report checkpoint and artifact digests, OS/architecture, selected scenario, fixed outcome, and whether cleanup was confirmed separately from any private backup data.

## Verification handoff

For each immutable package set, DevOps records archive and inventory hashes, image identities, prerequisites, and clean-runtime setup. Tester independently checks clean installation; same-OS loopback; private LAN with each OS as host; cancellation/readiness/admission/cleanup; restored-data reinstallation and complete rollback; and canonical compatibility rejection outcomes. A packaged Local Play match must start and complete with network availability removed and no service started. Do not run standalone server diagnostics as that Local Play check.

Reviewer traces these procedures to actual package contents and runtime behavior; security review covers trust, ports, firewall guidance, and diagnostics. Product assesses the supplied criterion-level evidence. Unsupported overlays require explicit documentation review, not an invented runtime detector. Until those observations exist, report gaps rather than accepted deployment support.

Source anchors: `source/Defines.h` and `source/Menu.cpp` (local data), `source/Application.cpp` and `source/console/ConsoleCommands.cpp` (configuration/console), `source/NetworkMenu.cpp` and `source/client/NetworkSessionRuntime.cpp` (graphical host/join), `source/network/Protocol.h` (default port), `source/network/AdmissionProtocol.cpp` and `CompatibilityManifest.cpp` (compatibility), and `docker/build.sh` / `docker/build-windows.sh` (package inventories). The canonical requirements and lifecycle/trust documents take precedence over this operational guide.
