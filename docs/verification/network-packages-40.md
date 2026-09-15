# QA #40 — immutable checkpoint local evidence

## Conclusion

**PASS: the requested applicable Linux local QA checks are satisfied.** No local
application failure or blocker was observed. This is fresh checkpoint evidence,
not promotion of the earlier diagnostic runs. It is not product acceptance,
Windows/cross-platform qualification, or final network-release acceptance.

Execution date: **2026-09-15 UTC**. Tester made no production or test edits and
performed no production rebuild. The supplied integrated tests were used as-is.

## Immutable provenance

| Item | Identity |
|---|---|
| Checkpoint | `662afaf9cc94b48b1f8a3ddaea6868e7bc82a523` |
| Fixed specification baseline | `1dc3512d1b4e075302dde9530802e64a15d9d753`; `docs/network-deployments.md` unchanged against it |
| Specification SHA-256 | `fad9f5c979063be7a00a96ca3a586e3940f717c5a89e6c5061f528cb2bf6e2ba` |
| Checkpoint operations document SHA-256 | `88ec4c242fc410c9c6d0c97d18c932d5c0dd02bc2661519cadd08ed92e5ee6f4` |
| `build/duel6r-package-662afaf.zip` | `60e5c1a55cd006dc35f3e26c763a1d864cd929393ca8962f21046b3a858c6f58` |
| `build/ctest-662afaf-linux.tar.gz` | `469bd5b935a1cd05d0c07e668c6a09080816137d513c2142390d12adf7d3b677` |
| `tests/PackageDeploymentBehaviorTests.py` | `d835e2dbf4c13f83f77042854371da39bb6b9c36655ccd7d08923027460e8ac0` |
| `tests/PackageRenderedTextObserver.py` | `0563620ecee5c832d74e908c16bcc8db00ad348b8dbb1fb7d101c64a0c12212c` |
| `tests/PackageDeploymentBehaviorTests.md` | `8aa474efeab705134bbf3f4f54c6bc6125651323da21df4d5d29a81df6bfcf30` |

Configuration: Release, gl4, Lua ON, BUILD_TESTING ON, RUN_TESTS OFF in the handoff.
The restored CMake cache independently confirmed Release/gl4/Lua/BUILD_TESTING and
source root `/workspace`. Checkout status was clean before and after verification;
the tip and both handed-off archive hashes were unchanged after cleanup.

## Environment and execution

All project execution occurred in disposable Docker containers transferred with
`docker/run-with-daemon-workspace.sh`. The wrapper ran from disposable workspaces,
not the frozen checkout, and its build entrypoint was overridden.

- CTest image: `sha256:037910295700dc301b5197edea374b555948d6cf7bef5d8f6752c74b0cb0014b`.
  CTest 3.28.3; checkpoint source copied to `/workspace`; archive restored to its
  original `/tmp/duel6r-build-ZJ4f3G` path. No supplied production or CTest target
  was rebuilt.
- Package runtime base: Ubuntu 24.04 x86-64,
  `sha256:224a1869083a311ef3f13648a154ba79832fbef6364d31493642ca03082da254`.
  Installed documented runtime prerequisites and test tools only; no development
  checkout was needed by the running package. Xvfb 850x700, Mesa software GL,
  dummy SDL audio, and container-local ptrace permission for the observer.
- Versions: GDB `15.1-1ubuntu1~24.04.1`, Python `3.12.3-0ubuntu2.1`, Xvfb
  `2:21.1.12-1ubuntu1.6`, xdotool `1:3.20160805.1-5build1`, SDL2
  `2.30.0+dfsg-1ubuntu3.1`, SDL2_image `2.8.2+dfsg-1build2`, SDL2_mixer
  `2.8.0+dfsg-1build3`, SDL2_ttf `2.22.0+dfsg-1`, GLEW `2.2.0-4build1`, Lua
  `5.3.6-2build2`, Mesa DRI `25.2.8-0ubuntu0.24.04.2`, libstdc++6
  `14.2.0-4ubuntu2~24.04.1`.
- Offline replacement ran after removing the runtime container's bridge attachment;
  only loopback remained. Virtual LAN used an owned internal Docker network,
  `172.30.240.0/24`, with exactly two endpoints (`.10` host, `.11` guest), no published
  ports and no external network attachments. `.99` was unused for Cancel testing.

Commands below ran inside those containers via `docker exec`:

```sh
tar -xzf /workspace/build/ctest-662afaf-linux.tar.gz -C /
ctest --test-dir /tmp/duel6r-build-ZJ4f3G -N
ctest --test-dir /tmp/duel6r-build-ZJ4f3G --output-on-failure

# Common package command; substitute each argument row below for SCENARIO_ARGS.
python3 /workspace/tests/PackageDeploymentBehaviorTests.py \
  /workspace/build/duel6r-package-662afaf.zip \
  --sha256 60e5c1a55cd006dc35f3e26c763a1d864cd929393ca8962f21046b3a858c6f58 \
  SCENARIO_ARGS
```

| SCENARIO_ARGS | Actual result |
|---|---|
| `--scenario failure-matrix` | All four host/guest x Retry-available/disabled cases passed |
| `--scenario failure-return` | Original mismatch Return regression passed |
| `--scenario mismatch` | Exact mismatch copy, host isolation, preserved guest config and cleanup passed |
| `--scenario invalid` | Exact local-invalid copy, Retry unavailable, host isolation and cleanup passed |
| `--scenario replacement` | Local Play, initial session, reinstall and rollback all passed; 47 no-service samples |
| `--scenario cancel --address 172.30.240.99` | Pending guest Cancel returned to editable setup and cleaned up |
| `--scenario lan-host --address 172.30.240.10 --peer-ready-file /tmp/qa40-peer-ready` | Host passed; exit 0 |
| `--scenario lan-guest --address 172.30.240.10` | Guest passed; exit 0 |

For LAN, the controller launched the guest after `LAN_HOST_READY`; created the
host's ready marker only after `LAN_PARTICIPANT_READY guest`; and created its
`.match` marker only after `LAN_PARTICIPANT_MATCH guest`. Both were consumed.
This ensured independent guest observations before host advancement/shutdown.

Inventory verification used the integrated `install()` helper in disposable
roots, including its `sha256sum -c linux-x86_64.sha256sums`, plus a separate
`sha256sum -c windows-x86_64.sha256sums`. **597 Linux and 619 Windows entries passed**.
The Windows result verifies integrity only, not Windows runtime dependencies or execution.

## Expected versus actual coverage

The complete restored suite enumerated **29 tests**. It ran once, without filters
or exclusions: **29 passed, 0 failed, CTest exit 0, 728.01 seconds**. This includes
the full admission-compatibility, session-runtime, host-service/process/orphan,
transport, resolver, persistence/domain/resource and graphical behavior tests.

| Criterion | Expected | Observed / conclusion |
|---|---|---|
| NET-DEP-AC-001 | Complete identifiable package; clean-runtime installation/start | Both inventories passed; Linux graphical clients started from fresh verified extractions with documented runtime dependencies. Linux execution PASS; Windows execution remains open. |
| NET-DEP-AC-002 | Production host readiness, guest admission and shutdown on supported endpoints | Loopback and two-container Linux virtual LAN showed both connected participant roles, all four named players, owned-player counts and both Ready states in single-frame assertions; both entered gameplay; intentional End and cleanup passed. Windows/cross-platform directions remain open. |
| NET-DEP-AC-003 | Operations match actual ports, recovery actions and cleanup | Non-default port 26740 and K1/K2 controls survived Edit; Return entered NET-01; eligible Retry repeated and recovered; disabled Retry made no Start/Join call. Guest Cancel passed. Existing lifecycle/process CTests passed. No new deadline claim from the debugger runs. |
| NET-DEP-AC-004 | Complete Local Play independent of networking | Only loopback was available; stock-content match rendered End of Game; saved records had the intended people/roster, one round and real game/Elo/death data. Forty-seven samples found no game-service component or added listener. PASS in stated environment. |
| NET-DEP-AC-005 | Claims match demonstrated matrix and retain release boundary | Source-review approval supplied by user; not re-reviewed as a runtime test. This manifest explicitly does not claim Windows or final release acceptance. |
| NET-UPG-AC-001 | Offline complete same-release reinstall preserves own data and supports a new session | Host seeded once through Local Play; fresh guest seeded separately. Exact person/roster sets and full host Local Play records were asserted before backup, after restoration and after new sessions. Config/profiles/people backups remained byte-identical. Fresh inventory verified before restoration. Linux PASS. |
| NET-UPG-AC-002 | Complete rollback uses saved package plus its own prior backup | Original complete archive and original per-instance backups restored after deliberate later config/profile changes; data, new production session and cleanup passed. Source and target archive both have the package hash above. Same-release Linux rollback only. |
| NET-UPG-AC-003 | Supported/unsupported paths and combinations documented without invented migrations | Source-review approval supplied by user. Executed paths were clean install, same-release reinstall and own-backup rollback on Linux only; no prototype, cross-release or cross-OS migration claim. |
| NET-UPG-AC-004 | Stable compatibility outcomes, no silent content repair | Full deterministic compatibility CTest passed protocol/release/capability checks, extra capability acceptance, invalid/unequal content, precedence and identifiers/copy. Package GUI observed real unequal-content rejection and local invalid-content outcomes; guest config was not rewritten. Remote incompatible-release/capability/invalid-manifest requests were not synthesized through package clients. |

Exact UI copy was asserted (all wrapped lines), rather than inferred from sockets:

- Guest mismatch: `Gameplay content mismatch. Use the host's exact supported gameplay content.`
- Guest local invalidity: `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.`
- Host local invalidity: `Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.`
- Host port collision: `The selected port is unavailable. Choose another port and try again.`
- Guest unavailable host: `Host unreachable.`

Local-invalid host/guest messages are deliberately distinguished from the remote
canonical `gameplay-content-manifest-invalid` policy outcome. Deterministic CTest
coverage of canonical identifiers is not represented as a graphical mismatched-release run.

## Limits, cleanup and handoff

- No local blocker remains in the requested execution scope. The earlier pointer
  Return defect passed its integrated regression and the full four-case matrix.
- Windows desktop and all Windows/cross-platform execution remain a required
  **product matrix gap**, not a Linux test failure. No Wine substitute was used.
  The Linux-run Windows command-line-contract CTest is not Windows execution.
- Virtual LAN is not physical-LAN hardware evidence. Dummy audio/software GL do
  not qualify physical audio, GPU or controller hardware. This package smoke does
  not replace the separate complete network-release gate.
- The observer checks allowlisted text submitted to rendering and argument-free
  invocation counts, not pixels or uninstrumented timing. No final-pixel test was
  requested for this routing-only change. Existing automated CTest image assertions
  ran as registered; their temporary images are not retained as acceptance evidence.
- Tester removed the three owned containers, internal network, runtime/build
  staging copies, restored CTest tree, private test backups and temporary outputs.
  Only this requested manifest remains outside the repository. Handed-off archives
  remain in the isolated checkout unchanged; repository status is clean.
- No tests changed, no production rebuild, no commits or external issue mutations.
  Developer may attach this manifest to checkpoint `662afaf...` evidence. Further
  platform qualification and product/release acceptance remain with their owners.
