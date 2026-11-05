#!/usr/bin/env python3
"""One-connection, opaque TCP relay for an explicitly disclosed local condition.

Run inside the capture's Docker --network none namespace. Host listens on
127.0.0.1:28201; Guest uses ordinary Direct Join to 127.0.0.1:28202. This tool
does not terminate encryption, inspect messages, or change application state.

Frozen 8b requires an initial loopback clock-calibration RTT <=20 ms. For AP-N,
use --delay-ms 25 --defer-delay, complete normal admission at zero added delay,
then explicitly signal THIS relay with SIGUSR1. It enables the configured fixed
delay once; there is no protocol detector, scheduler, or remote control plane.
The 8b network-budget degradation window is 3s and probe deadline is 250ms.
Neither these settings nor byte forwarding prove actual application quality.

An explicitly approved later 1s SIGSTOP/SIGCONT may target the relay ONLY, never
the game. Monotonic deadlines continue to elapse; no times are repaired on resume.
Do not claim effect-age continuity or resynchronization without observing it.

Each direction has at most 1 MiB pending, plus one partial-send chunk's retained
prefix. Backpressure stops reads, never drops/reorders bytes. Natural EOF drains
for at most 2s, then half-closes the destination while retaining the reverse leg.
The overall deadline (including waiting/connect/drain) is at most 600s. Signals,
errors and expired deadlines are forced exits, even if accepted bytes drained.
The sent hashes describe socket acceptance, not peer/application acknowledgement.
Logs contain metadata only. No payload, password, key or raw exception text.
"""

import argparse
from collections import deque
import hashlib
import json
import math
import select
import signal
import socket
import time


QUEUE_BYTES = 1024 * 1024
CHUNK_BYTES = 64 * 1024
DRAIN_SECONDS = 2.0


class Flow:
    def __init__(self, name, source, destination):
        self.name = name
        self.source = source
        self.destination = destination
        self.queue = deque()
        self.pending = 0
        self.peak_pending = 0
        self.eof = False
        self.half_closed = False
        self.received = 0
        self.sent = 0
        self.in_hash = hashlib.sha256()
        self.out_hash = hashlib.sha256()

    def summary(self):
        return {"direction": self.name, "received_bytes": self.received,
                "sent_bytes": self.sent, "pending_bytes": self.pending,
                "peak_pending_bytes": self.peak_pending,
                "received_sha256": self.in_hash.hexdigest(),
                "sent_sha256": self.out_hash.hexdigest(), "read_eof": self.eof,
                "write_half_closed": self.half_closed}


class ForcedExit(Exception):
    pass


def finite_number(low, high):
    def parse(value):
        number = float(value)
        if not math.isfinite(number) or not low <= number <= high:
            raise argparse.ArgumentTypeError(f"must be finite and within {low}..{high}")
        return number
    return parse


def private_port(value):
    number = int(value)
    if not 1024 <= number <= 65535:
        raise argparse.ArgumentTypeError("must be an unprivileged port (1024..65535)")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--listen-port", type=private_port, default=28202)
    parser.add_argument("--target-port", type=private_port, default=28201)
    parser.add_argument("--delay-ms", type=finite_number(0, 1000), default=0.0,
                        help="fixed added delay per direction, not an end-to-end guarantee")
    parser.add_argument("--defer-delay", action="store_true",
                        help="start at zero delay; SIGUSR1 enables --delay-ms once")
    parser.add_argument("--lifetime", type=finite_number(0.1, 600), default=600.0)
    args = parser.parse_args()
    if args.listen_port == args.target_port:
        parser.error("listen and target ports must differ")

    stop = [None]
    activate = [False]
    signal.signal(signal.SIGINT, lambda number, frame: stop.__setitem__(0, stop[0] or "SIGINT"))
    signal.signal(signal.SIGTERM, lambda number, frame: stop.__setitem__(0, stop[0] or "SIGTERM"))
    signal.signal(signal.SIGUSR1, lambda number, frame: activate.__setitem__(0, True))
    deadline = time.monotonic() + args.lifetime
    flows = []
    sockets = []
    connected = False
    normal = False
    reason = "internal_error"
    error_number = None
    active_delay = not args.defer_delay

    def log(event, **fields):
        print(json.dumps({"event": event, "monotonic": time.monotonic(),
                          "connection": int(connected), **fields}), flush=True)

    def check_deadline():
        if time.monotonic() >= deadline:
            raise ForcedExit("lifetime_expired")

    try:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sockets.append(listener)
        listener.bind(("127.0.0.1", args.listen_port))
        listener.listen(1)
        listener.setblocking(False)
        log("ready", listen=["127.0.0.1", args.listen_port],
            target=["127.0.0.1", args.target_port], delay_ms=args.delay_ms,
            defer_delay=args.defer_delay, lifetime=args.lifetime,
            queue_bytes_per_direction=QUEUE_BYTES, deadline=deadline)
        while True:
            check_deadline()
            if stop[0]:
                raise ForcedExit("signal_" + stop[0])
            if select.select([listener], [], [], max(0.0, min(0.05, deadline - time.monotonic())))[0]:
                client, _ = listener.accept()
                sockets.append(client)
                connected = True
                listener.close()  # Exactly one Guest connection; no automatic reconnect.
                break
        check_deadline()
        backend = socket.create_connection(("127.0.0.1", args.target_port),
                                           timeout=min(3.0, deadline - time.monotonic()))
        sockets.append(backend)
        for endpoint in (client, backend):
            endpoint.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            endpoint.setblocking(False)
        flows = [Flow("guest_to_host", client, backend), Flow("host_to_guest", backend, client)]
        log("connected")
        drain_deadline = None
        stopping = False
        while True:
            check_deadline()
            now = time.monotonic()
            if stop[0] and not stopping:
                stopping = True
                reason = "signal_" + stop[0]
                drain_deadline = min(drain_deadline or deadline, now + DRAIN_SECONDS)
                log("stop_requested", reason=reason)
            if activate[0] and not active_delay:
                active_delay = True
                log("delay_activated", delay_ms_per_direction=args.delay_ms)
            for flow in flows:
                if flow.eof and not flow.queue and not flow.half_closed:
                    flow.destination.shutdown(socket.SHUT_WR)
                    flow.half_closed = True
                    log("half_close", **flow.summary())
            if stopping and all(not flow.queue for flow in flows):
                break
            if all(flow.half_closed for flow in flows):
                normal = all(flow.received == flow.sent
                             and flow.in_hash.digest() == flow.out_hash.digest() for flow in flows)
                reason = "normal_eof" if normal else "byte_identity_mismatch"
                break
            if drain_deadline is not None and now >= drain_deadline:
                raise ForcedExit(reason if stopping else "eof_drain_deadline")
            readers = [flow.source for flow in flows
                       if not stopping and not flow.eof and flow.pending < QUEUE_BYTES]
            writers = [flow.destination for flow in flows if flow.queue and flow.queue[0][0] <= now]
            wake_at = min(deadline, now + 0.05, drain_deadline or deadline)
            for flow in flows:
                if flow.queue and flow.queue[0][0] > now:
                    wake_at = min(wake_at, flow.queue[0][0])
            readable, writable, _ = select.select(readers, writers, [], max(0.0, wake_at - time.monotonic()))
            check_deadline()
            for flow in flows:
                if flow.destination in writable:
                    item = flow.queue[0]
                    view = memoryview(item[1])[item[2]:]
                    try:
                        sent = flow.destination.send(view)
                    except (BlockingIOError, InterruptedError):
                        continue
                    if sent == 0:
                        raise ForcedExit("zero_send")
                    flow.out_hash.update(view[:sent])
                    flow.sent += sent
                    flow.pending -= sent
                    item[2] += sent
                    if item[2] == len(item[1]):
                        flow.queue.popleft()
                if flow.source in readable:
                    try:
                        data = flow.source.recv(min(CHUNK_BYTES, QUEUE_BYTES - flow.pending))
                    except (BlockingIOError, InterruptedError):
                        continue
                    if data:
                        flow.in_hash.update(data)
                        flow.received += len(data)
                        flow.pending += len(data)
                        flow.peak_pending = max(flow.peak_pending, flow.pending)
                        delay = args.delay_ms / 1000.0 if active_delay else 0.0
                        flow.queue.append([time.monotonic() + delay, data, 0])
                    else:
                        flow.eof = True
                        drain_deadline = min(drain_deadline or deadline, time.monotonic() + DRAIN_SECONDS)
                        log("read_eof", **flow.summary())
    except ForcedExit as error:
        reason = str(error)
    except OSError as error:
        reason = "socket_error"
        error_number = error.errno
    except Exception:
        reason = "internal_error"  # Never print exception text or transport bytes.
    finally:
        for endpoint in sockets:
            endpoint.close()
        if stop[0]:
            normal = False
            reason = "signal_" + stop[0]
        log("exit", normal=normal, forced=not normal, reason=reason, errno=error_number,
            flows=[flow.summary() for flow in flows])
    return 0 if normal else 1


if __name__ == "__main__":
    raise SystemExit(main())
