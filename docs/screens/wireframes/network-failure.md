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

- The screen names the confirmed reason and preserves endpoint context.
- Retry remains visible but shows a reason when disabled.
- Initial admission and transport failures use the fixed product precedence and non-disclosing copy. Endpoint validation remains inline in `NET-03`.
- Retry repeats retained data; Edit setup returns to retained `NET-02` or `NET-03`; Return to Network enters `NET-01`.
- Terminal reconnect rejection and expiry disable Retry when the original reservation cannot restore. Expiry copy never claims host end or player removal.
- Compatibility, capacity, timeout, terminal reconnect, expiry, and host-local `Hosted session stopped unexpectedly.` variants remain in the specification. Only accepted intentional host End uses guest `NET-09`.
- The specification supplies exact copy and destinations for every issue #30 outcome.
- Each reason remains persistent text, and each disabled Retry state includes a textual reason.

Planned representative screenshot: [`SS-022`](../../screenshots/README.md#ss-022).
