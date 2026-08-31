# NET-08 wireframe — Host unreachable

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         CONNECTION FAILED                            │
│                                                                      │
│ Host unreachable.                                                    │
│ 192.168.1.24:27015                                                   │
│ Check that the host session is running and the endpoint is correct.  │
│                                                                      │
│          [ Retry ] [ Edit setup ] [ Return to Network ]               │
└──────────────────────────────────────────────────────────────────────┘
```

- The screen names the confirmed reason.
- A guest initial-connection failure preserves the locally entered endpoint context.
- A host-service lifecycle failure omits endpoint and process context.
- Retry remains visible but shows a reason when disabled.
- Initial admission and transport failures use the fixed product precedence and non-disclosing copy. Invalid complete host admission messages use `Connection ended before admission completed.`; an offer alone is not success. Endpoint validation remains inline in `NET-03`.
- Eligible Retry repeats retained data after cleanup; Edit setup returns to retained `NET-02` or `NET-03`; Return to Network enters `NET-01`.
- Host port unavailable, generic start failure, exit before readiness, and startup timeout use the exact copy in the specification.
- Retry stays disabled with `Cleanup in progress.` until final cleanup completes.
- Post-readiness `Hosted session stopped unexpectedly.` always disables Retry and directs the host to Edit setup for a new session.
- Terminal reconnect rejection and expiry disable Retry when the original reservation cannot restore. Expiry copy never claims host end or player removal.
- Compatibility, capacity, timeout, terminal reconnect, expiry, host-local `Hosted session stopped unexpectedly.`, and guest-local `Local gameplay content is invalid. Restore the supported gameplay content and restart the application.` variants remain in the specification. Guest-local invalid content disables Retry until restart, retains `NET-03` setup for Edit setup, and starts no connection. Only accepted intentional host End uses guest `NET-09`.
- The specification supplies exact copy and destinations for every issue #30 outcome.
- The specification supplies exact copy and destinations for invalid authoritative settings, unavailable content, runtime failure, and cleanup failure from issue #32.
- Invalid settings return to editable `NET-04` with readiness cleared.
- Unavailable content blocks Start match and keeps host-only End session available.
- Runtime failure maps the host to the existing hosted-session failure presentation and keeps guests out of intentional host-end presentation.
- Cleanup failure is operational-only in issue #32 and has no approved graphical destination or action.
- Each reason remains persistent text, and each disabled Retry state includes a textual reason.

Planned representative screenshot: [`SS-022`](../../screenshots/README.md#ss-022).
