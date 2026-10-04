# Native host-end close-boundary harness

This Linux-only test executable exercises the actual encrypted transport, host
service, production guest admission/recovery callbacks, and native network menu.
It is not installed in runtime bundles and provides no shipped fault switch.
Product/UX requirements and wireframes remain unchanged.

## Controlled boundary

`HostEndBoundaryRuntime.cpp` compiles the unchanged client runtime into this test
executable. Only its guest `HeadlessServer` construction is substituted. After
normal production dependencies are installed, a decorator wraps the original
client factory. It retains bounded actual post-arm inbound frames, with original
payloads, order, and transport receipt timestamps, until a naturally due quality
probe is genuinely rejected by the closed TCP transport. Terminal observation is
delayed during this interval; canonical/UI state is never fabricated. The sealed
input operation releases those actual frames and the real close timestamp.

The `host-end` scenario starts an actual two-player match, confirms the host's
normal End action, verifies the rejected probe and eligible sealed notice, renders
NET-09 over the confirmed match context, and activates Return to Network to render
NET-01 with Host focused. `ordinary-close` uses normal supervisor application exit,
verifies a rejected probe with no intentional notice, and renders actual reconnect
state with a positive countdown. Existing deterministic admission regressions
cover wrong-session, malformed, and late notices; this harness does not forge them.

## Docker execution

Build normally with `BUILD_TESTING=ON`, Release, GL4, and Lua enabled. The target is
`duel6r-host-end-boundary-harness`. CTest entries `host-end-boundary-host-end` and
`host-end-boundary-ordinary-close` use isolated temporary data and Xvfb without
creating screenshot files.

For the preserved task container and artifact paths:

```sh
docker exec -e DISPLAY=:111 -e SDL_AUDIODRIVER=dummy -e LIBGL_ALWAYS_SOFTWARE=1 \
  duel6r-issue111-work /tmp/issue111-build/duel6r-host-end-boundary-harness \
  --scenario host-end --port 26661 --run-dir /tmp/issue111-sessions/qa-host-end-01

docker exec -e DISPLAY=:111 -e SDL_AUDIODRIVER=dummy -e LIBGL_ALWAYS_SOFTWARE=1 \
  duel6r-issue111-work /tmp/issue111-build/duel6r-host-end-boundary-harness \
  --scenario ordinary-close --port 26661 --run-dir /tmp/issue111-sessions/qa-ordinary-01
```

Use a new run-directory name for each scenario. The parent must exist. A supplied
port must be free; it is not reserved between scenarios. Omitting `--port` selects
an ephemeral loopback port. Never run two scenarios concurrently on the same
display or supplied port. Each scenario owns and closes its own clients and host
service without resetting the persistent container/display. Manual run data is
retained for inspection; its consumer owns removal after use.

Normal TCP TIME_WAIT may remain after cleanup. Check listener availability with
the same `SO_REUSEADDR` policy as the production TCP listener, not a plain bind
that mistakes TIME_WAIT for a live service.

`--hold-seconds N` holds each reached representative while continuing normal
update/render processing (maximum 300 seconds). No UI journey is assigned for a
hold. Ordinary reconnect still follows its original deadline.

## Capture, only after functional closure authorization

Add `--capture-dir /tmp/issue111-captures/campaign-name` to a fresh `host-end`
scenario. Native framebuffer PNGs are written as `host-ended-1280x900.png` and
`NET-01.png`. These correspond to the authorized SS023 and SS015 destinations;
the capture owner transfers them to those canonical destinations and records
provenance/hashes. No screenshot manifest is edited by this executable.

Existing image files are rejected rather than overwritten. To resume one missing
row, use `--capture-state host-ended` or `--capture-state entry` with a fresh
scenario/run-directory and the same capture directory. The default is `both`.
This is capture machinery, not independent UX conformance evidence. Do not use
capture options before the coordinated functional-closure gate.

The task environment/artifact owner retains the container, display, and immutable
artifacts through QA/capture. Session consumers remove only their own run data.
