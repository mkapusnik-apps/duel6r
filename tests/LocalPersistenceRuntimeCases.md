# Local persistence and network non-persistence integration tests

`LocalPersistenceRuntimeCases.h` is included by the existing network runtime test
executable and discovered into one CTest entry per `Persistence ...` case. Run
only the selected entries when checking this boundary; no separate fixture script
or recorder is needed.

For a frozen Unix Makefiles build, refresh only this target's include dependencies
before a narrow test-only build (a bare `/fast` can miss a newly included header):

```sh
make -C build/check -f CMakeFiles/duel6r-network-session-runtime-tests.dir/build.make \
  CMakeFiles/duel6r-network-session-runtime-tests.dir/depend
cmake --build build/check --target duel6r-network-session-runtime-tests/fast --parallel 4
```

These commands run inside the repository's Docker image. Compare the application
hash before/after and retain the original runtime bundle; this test target links
the existing production libraries and does not require rebuilding the application.

Use the repository Docker workspace procedure and an existing compatible build:

```sh
python3 tests/NetworkFlatBundleHarness.py \
  build/check/duel6r-network-session-runtime-tests resources build/check/duel6r \
  --bundle "$RUNTIME_BUNDLE" \
  --case 'Persistence finite offline restart resumes real rounds and updates Deathmatch Elo once' \
  --timeout 90 --diagnostics build/persistence-check/restart
```

The bundle argument is optional: without it the normal CTest harness assembles a
fresh flat runtime from the supplied resource directory and server executable.
Set HOME/XDG/TMPDIR to task-owned directories inside the Docker namespace. Use
`--network none` (loopback remains available). The harness removes its temporary
flat root only after process cleanup and retains stdout/stderr/process diagnostics.

Registered cases:

- `Persistence finite offline restart resumes real rounds and updates Deathmatch Elo once`
- `Persistence real local final wait Shift Escape preserves one Deathmatch Elo update`
- `Persistence real Predator completion excludes Elo across navigation and Shift Escape`
- `Persistence real Team completion excludes Elo across navigation and Shift Escape`
- `Persistence actual network Host completion never writes local statistics or Elo`
- `Persistence actual network Guest completion never writes local statistics or Elo`

Local fixtures create fifteen ordinary persons through Menu actions, select
finite rounds, and move keyboard-controlled players off small platforms into
real water. They do not assign statistics, outcomes, damage, or end-delay timers.
Application input routing, Game/Round updates, mode rules, menu lifecycle and JSON
persistence are production implementations linked into the integration binary.
Local simulation advances at its normal 1/90-second step without wall-clock
sleep; this is not a performance or visual-conformance test.

The restart case uses three fresh child processes, forked before Application/SDL
initialization: finish/save round one and exit; restore and answer Yes to Resume,
finish round two; restart and verify all persisted person fields and roster.
Finite Deathmatch checks the known 15-player winner/loser Elo values and exactly
one Elo game per person. Other mode cases check exclusion. Full end delays are
advanced naturally, checking plain Escape during final wait and plain/Shift
Escape after completion, plus Shift Escape during final wait. Completed scrolling,
wheel, console precedence and repeated updates must not rewrite the save file.

Network cases use the real supervised frozen server and encrypted loopback
Host/Guest sessions; the observing Application role varies between cases. A tiny
valid arena fills naturally to produce Completed. The local save's exact bytes
and modification time are checked during Lobby, play, completion, navigation and
End/Leave; ordinary menu exit may rewrite identical content, but must not alter any
statistics or Elo. These are filesystem assertions, not assertions about a
"not saved" caption. No remote directory is involved.

All generated gameplay fixtures are routine-test-only and must never be reused
as formal visual-capture evidence. Existing case semantics are unchanged.
