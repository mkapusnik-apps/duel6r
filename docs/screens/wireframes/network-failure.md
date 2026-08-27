# NET-08 wireframe — Host unreachable

Target representative viewport: 1920 by 1080 px with the scaled 850 by 700 retro canvas. This wireframe is planned for issue #38 and is not implemented.

```text
┌────────────────────── 850 × 700 logical canvas ──────────────────────┐
│                         CONNECTION FAILED                            │
│                                                                      │
│ Host unreachable                                                     │
│ 192.168.1.24:27015                                                   │
│ Check that the host session is running and the endpoint is correct.  │
│                                                                      │
│                 [ Retry ] [ Return to Network ]                       │
└──────────────────────────────────────────────────────────────────────┘
```

- The screen names the confirmed reason and preserves endpoint context.
- Retry remains visible but shows a reason when disabled.
- Compatibility, capacity, timeout, reconnect-expiry, and host-ended variants remain in the specification; host loss uses `NET-09`.

Planned representative screenshot: [`SS-022`](../../screenshots/README.md#ss-022).
