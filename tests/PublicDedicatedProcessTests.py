#!/usr/bin/env python3
"""Local trusted-CA integration of the real public client and dedicated service.

TRU-PUB-AC-001/002, NET-PUB-AC-001/002 and HSL-PUB-002/003.
The fixture owns temporary certificates, content, sockets and processes. It is
an application protocol peer, not a deployment or proxy configuration test.
"""
import contextlib
from collections import deque
import json
import os
from pathlib import Path
import select
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time


INVITE = "staging-test-only-0123456789abcdef0123456789abcdef"
ROTATED = "staging-rotated-abcdef0123456789abcdef0123456789"
OTHER = "production-test-only-abcdef0123456789abcdef0123456789"
RELAY_TIMEOUT = 2  # Preserve the fixture's existing socket I/O budget.
RELAY_QUEUE_LIMIT = 4 * 1024 * 1024


def port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def proxy_header(source="198.51.100.7"):
    return (b"\r\n\r\n\0\r\nQUIT\n" + b"\x21\x11\0\x0c"
            + socket.inet_aton(source) + socket.inet_aton("127.0.0.1")
            + struct.pack("!HH", 40000, 26661))


class TlsPeer:
    next_source = 100

    def __init__(self, cert, key, backend, control_root=None, end_write_race=False):
        self.backend = backend
        self.source = f"198.51.100.{TlsPeer.next_source}"
        TlsPeer.next_source += 1
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = ssl.TLSVersion.TLSv1_2
        self.context.load_cert_chain(cert, key)
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen()
        self.listener.settimeout(.1)
        self.stop = threading.Event()
        self.workers = []
        self.application_bytes = 0
        self.handshakes = 0
        self.frames = deque(maxlen=64)
        self.events = deque(maxlen=64)
        self.evidence_lock = threading.Lock()
        self.forwarded_ends = 0
        self.eof_observation = None
        self.end_write_race = end_write_race
        self.race_write_failed = threading.Event()
        self.control_root = control_root
        self.block_reconnect = False
        self.reconnect_after = 0
        self.connection_count = 0
        self.thread = threading.Thread(target=self.accept)
        self.thread.start()

    def action(self):
        if self.control_root:
            try:
                return (self.control_root / "action").read_text().split()
            except FileNotFoundError:
                pass
        return []

    def record(self, connection, stage, direction="none", code=0):
        # Local connection aliases and fixed stages/codes only, never payloads,
        # exception text, credentials, endpoints or certificate details.
        if connection <= 8:
            with self.evidence_lock:
                self.events.append((connection, direction, stage, code))

    def evidence(self):
        with self.evidence_lock:
            return "relay events=" + repr(list(self.events)) + "; frame tail=" + repr(list(self.frames))

    def begin_eof_observation(self, connection):
        with self.evidence_lock:
            self.eof_observation = dict(connection=connection, ends=0, complete=False)

    def complete_eof_observation(self, connection):
        with self.evidence_lock:
            assert self.eof_observation and self.eof_observation["connection"] == connection, "EOF observation binding"
            self.eof_observation["complete"] = True

    def record_forwarded_end(self, connection, count):
        with self.evidence_lock:
            self.forwarded_ends += count
            observation = self.eof_observation
            if observation and not observation["complete"] and observation["connection"] == connection:
                observation["ends"] += count

    @staticmethod
    def is_end(payload):
        return len(payload) == 16 and payload[:8] == b"D6LC\0\1\0\3"

    def synchronize_end_write_race(self, client, backend, connection):
        """Order a real End before a genuine closed-write-half failure.

        Only used by the permanent regression. No terminal message/outcome is
        injected: the caller already holds actual backend End bytes. Confirm
        actual EOF, then use ordinary client traffic against a closed write half.
        The before-fix relay discarded End when this send raised BrokenPipeError.
        """
        self.record(connection, "race-end-pending", "backend-to-client")
        deadline = time.monotonic() + RELAY_TIMEOUT
        while True:
            try:
                trailing = backend.recv(65536)
                assert not trailing, "End regression expected the service's final record"
                break
            except BlockingIOError:
                remaining = deadline - time.monotonic()
                assert remaining > 0, "End regression backend EOF deadline"
                select.select([backend], [], [], min(.01, remaining))
        self.record(connection, "race-backend-eof", "backend-to-client")
        backend.shutdown(socket.SHUT_WR)
        while True:
            try:
                traffic = client.recv(65536)
                assert traffic, "End regression requires ordinary active client traffic"
                break
            except (BlockingIOError, ssl.SSLWantReadError, ssl.SSLWantWriteError) as error:
                remaining = deadline - time.monotonic()
                assert remaining > 0, "End regression client traffic deadline"
                select.select([] if isinstance(error, ssl.SSLWantWriteError) else [client],
                              [client] if isinstance(error, ssl.SSLWantWriteError) else [], [], min(.01, remaining))
        try:
            backend.send(traffic)
        except OSError as error:
            self.race_write_failed.set()
            self.record(connection, "race-opposite-write-failed", "client-to-backend", error.errno or 0)
            raise
        raise AssertionError("End regression did not reach closed write half")

    def accept(self):
        while not self.stop.is_set():
            fields = self.action()
            if len(fields) == 3 and fields[1] == "observed-eof":
                # The native ambiguity assertion has completed, before runtime
                # unwinding can send a legitimate controller teardown Leave.
                self.complete_eof_observation(int(fields[0]))
                (self.control_root / "action").unlink()
                (self.control_root / "acted").touch()
            if len(fields) == 3 and fields[1] == "resume":
                self.block_reconnect = False
                (self.control_root / "action").unlink()
                (self.control_root / "acted").touch()
            try:
                raw, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self.connection_count += 1
            worker = threading.Thread(target=self.relay, args=(raw, self.connection_count))
            self.workers.append(worker)
            worker.start()

    def relay(self, raw, connection_id):
        try:
            raw.settimeout(RELAY_TIMEOUT)
            raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            with self.context.wrap_socket(raw, server_side=True) as client:
                self.handshakes += 1
                if connection_id > 2:
                    while not self.stop.is_set() and (self.block_reconnect or time.monotonic() < self.reconnect_after):
                        time.sleep(.01)
                    if self.stop.is_set():
                        return
                with socket.create_connection(("127.0.0.1", self.backend), timeout=2) as backend:
                    backend.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    backend.sendall(proxy_header(self.source))
                    self.relay_streams(client, backend, connection_id)
        except (OSError, ssl.SSLError) as error:
            self.record(connection_id, "connection-error", code=error.errno or 0)
            pass  # Invalid certificates and deliberate disconnects are scenarios.
        finally:
            raw.close()

    def relay_streams(self, client, backend, connection_id):
        """Bounded FIFO relay: losing one write leg must not discard the other.

        Nonblocking SSL retries follow their requested readiness; select alone
        does not establish that a complete TLS record can be read. Preserve the
        previous 2s I/O budget for stalled output and directional close draining.
        """
        client.setblocking(False)
        backend.setblocking(False)
        streams = (backend, client)  # Drain backend output before new input.
        incoming = {stream: bytearray() for stream in streams}
        outgoing = {stream: deque() for stream in streams}
        queued = {stream: 0 for stream in streams}
        read_open = {stream: True for stream in streams}
        write_open = {stream: True for stream in streams}
        read_want = {stream: "read" for stream in streams}
        write_want = {stream: "write" for stream in streams}
        close_deadline = None
        malicious_frame_seen = False
        race_done = False

        def direction(source):
            return "backend-to-client" if source is backend else "client-to-backend"

        def begin_close():
            nonlocal close_deadline
            if close_deadline is None:
                close_deadline = time.monotonic() + RELAY_TIMEOUT

        def enqueue(destination, data, end_markers=0):
            assert queued[destination] + len(data) <= RELAY_QUEUE_LIMIT, "relay FIFO limit"
            # Hold exactly the same bytes across SSLWant* retries. Appending to
            # the FIFO cannot change the in-progress SSL write arguments.
            # One absolute item deadline includes FIFO waiting, partial sends
            # and every SSL retry. Progress never replenishes sendall's budget.
            outgoing[destination].append([data, end_markers, time.monotonic() + RELAY_TIMEOUT])
            queued[destination] += len(data)

        def lose_backend_write(stage, code=0):
            self.record(connection_id, stage, "client-to-backend", code)
            write_open[backend] = False
            read_open[client] = False  # No destination for subsequent input.
            outgoing[backend].clear()
            queued[backend] = 0
            begin_close()
            # Backend receive and queued client output remain available.

        def lose_client_write(stage, code=0):
            self.record(connection_id, stage, "backend-to-client", code)
            write_open[client] = False
            read_open[backend] = False  # No recipient for subsequent output.
            outgoing[client].clear()
            queued[client] = 0
            begin_close()
            # Still flush already accepted input (notably controller Leave)
            # before propagating client EOF to the service.

        while not self.stop.is_set():
            now = time.monotonic()
            if close_deadline is not None and now >= close_deadline:
                self.record(connection_id, "close-budget")
                return
            if not read_open[client] and not outgoing[backend] and write_open[backend]:
                # Flush accepted input before propagating EOF to the service.
                try:
                    backend.shutdown(socket.SHUT_WR)
                    write_open[backend] = False
                except OSError as error:
                    lose_backend_write("half-close-error", error.errno or 0)
                begin_close()
            if not any(read_open.values()) and not any(outgoing.values()):
                return

            action_path = self.control_root / "action" if self.control_root else None
            if action_path and not incoming[backend]:
                fields = self.action()
                if len(fields) == 3 and int(fields[0]) == connection_id and fields[1] != "resume":
                    _, action, session = fields
                    action_path.unlink()
                    if action in ("eof", "expiry", "recovery"):
                        if action == "eof": self.begin_eof_observation(connection_id)
                        self.record(connection_id, "deliberate-unconfirmed-loss")
                        self.block_reconnect = action != "recovery"
                        self.reconnect_after = time.monotonic() + .75
                        (self.control_root / "acted").touch()
                        return
                    identity = int(session) + (action == "wrong")
                    payload = b"D6PE" + bytes([action != "controller"]) + struct.pack("<Q", identity)
                    enqueue(client, struct.pack("!IHHI", 0x44365254, 1, 0, len(payload)) + payload)
                    (self.control_root / "acted").touch()

            reads, writes = set(), set()
            for source in streams:
                destination = client if source is backend else backend
                if read_open[source] and queued[destination] <= RELAY_QUEUE_LIMIT - 65536:
                    (writes if read_want[source] == "write" else reads).add(source)
                if write_open[source] and outgoing[source]:
                    (reads if write_want[source] == "read" else writes).add(source)
            pending_tls = read_open[client] and read_want[client] == "read" and client.pending() > 0
            readable, writable, _ = select.select(list(reads), list(writes), [], 0 if pending_tls else .01)
            if pending_tls:
                readable.append(client)

            for source in streams:
                destination = client if source is backend else backend
                ready = source in (writable if read_want[source] == "write" else readable)
                if not read_open[source] or not ready or queued[destination] > RELAY_QUEUE_LIMIT - 65536:
                    continue
                try:
                    data = source.recv(65536)
                    read_want[source] = "read"
                except ssl.SSLWantWriteError:
                    read_want[source] = "write"; continue
                except (ssl.SSLWantReadError, BlockingIOError):
                    read_want[source] = "read"; continue
                except OSError as error:
                    self.record(connection_id, "read-error", direction(source), error.errno or 0)
                    data = b""
                if not data:
                    self.record(connection_id, "read-eof", direction(source))
                    read_open[source] = False
                    begin_close()
                    if source is backend:
                        if malicious_frame_seen:
                            (self.control_root / "attack-rejected").touch()
                        lose_backend_write("backend-eof")
                    continue
                if source is client:
                    self.application_bytes += len(data)
                buffer = incoming[source]
                buffer.extend(data)
                end_markers = 0
                while len(buffer) >= 12:
                    identifier, version, kind, size = struct.unpack("!IHHI", buffer[:12])
                    assert identifier == 0x44365254 and version == 1 and kind <= 2 and size <= 1048576, "relay envelope"
                    if len(buffer) < 12 + size:
                        break
                    with self.evidence_lock:
                        self.frames.append((connection_id, "server" if source is backend else "client", kind, size))
                    if source is backend and self.is_end(buffer[12:12 + size]):
                        end_markers += 1
                        self.record(connection_id, "end-observed", "backend-to-client")
                    if source is client and self.control_root and (self.control_root / "attack.bin").exists():
                        if bytes(buffer[12:12 + size]) == (self.control_root / "attack.bin").read_bytes():
                            malicious_frame_seen = True
                    del buffer[:12 + size]
                enqueue(destination, data, end_markers)
                if end_markers and self.end_write_race and connection_id == 3 and not race_done:
                    race_done = True
                    try:
                        self.synchronize_end_write_race(client, backend, connection_id)
                    except OSError as error:
                        read_open[backend] = False  # The barrier observed actual backend EOF.
                        lose_backend_write("write-error", error.errno or 0)

            for destination in (client, backend):
                if not write_open[destination] or not outgoing[destination]:
                    continue
                item = outgoing[destination][0]
                if time.monotonic() >= item[2]:
                    if destination is backend:
                        lose_backend_write("write-budget")
                        continue
                    lose_client_write("write-budget")
                    continue
                ready = destination in (readable if write_want[destination] == "read" else writable)
                if not ready:
                    continue
                try:
                    sent = destination.send(item[0])
                    assert sent > 0, "relay write progress"
                    write_want[destination] = "write"
                except ssl.SSLWantReadError:
                    write_want[destination] = "read"; continue
                except (ssl.SSLWantWriteError, BlockingIOError):
                    write_want[destination] = "write"; continue
                except OSError as error:
                    if destination is backend:
                        lose_backend_write("write-error", error.errno or 0)
                        continue
                    lose_client_write("write-error", error.errno or 0)
                    continue
                queued[destination] -= sent
                if sent == len(item[0]):
                    outgoing[destination].popleft()
                    if item[1]:
                        self.record_forwarded_end(connection_id, item[1])
                        self.record(connection_id, "end-forwarded", "backend-to-client", item[1])
                else:
                    item[0] = item[0][sent:]

    def close(self):
        self.stop.set()
        self.listener.close()
        self.thread.join(3)
        for worker in self.workers:
            worker.join(3)
            assert not worker.is_alive(), "TLS fixture worker leaked"


def certificates(root):
    def openssl(*args):
        subprocess.run(["openssl", *args], cwd=root, capture_output=True, check=True, timeout=15)
    openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
            "-subj", "/CN=Duel6 test CA", "-keyout", "ca.key", "-out", "ca.pem")
    for name, san in (("valid", "IP:127.0.0.1"), ("wrong", "DNS:wrong.example"),
                      ("expired", "IP:127.0.0.1")):
        openssl("req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=fixture",
                "-keyout", name + ".key", "-out", name + ".csr")
        (root / (name + ".ext")).write_text("subjectAltName=" + san + "\nextendedKeyUsage=serverAuth\n")
        openssl("x509", "-req", "-in", name + ".csr", "-CA", "ca.pem", "-CAkey", "ca.key",
                "-CAcreateserial", "-days", "-1" if name == "expired" else "2",
                "-extfile", name + ".ext", "-out", name + ".pem")


@contextlib.contextmanager
def service(server, root, invitation):
    backend = port()
    invite = root / "invite"
    invite.write_text(invitation)
    invite.chmod(0o600)
    ready = root / "ready.sock"
    with (root / "server.log").open("w+") as log:
        process = subprocess.Popen([str(server), "--dedicated", "--transport", "--host=127.0.0.1",
            f"--port={backend}", f"--resources={root}", "--trusted-proxy-protocol=v2",
            f"--invite-file={invite}", f"--readiness-socket={ready}"], stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                assert process.poll() is None, "dedicated service exited before readiness"
                check = subprocess.run([str(server), f"--check-ready={ready}"], capture_output=True, timeout=2)
                if check.returncode == 0:
                    break
                time.sleep(.05)
            else:
                raise AssertionError("dedicated readiness deadline")
            yield backend, ready
        finally:
            process.terminate()
            try:
                process.wait(5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(5)
            log.seek(0)
            output = log.read()
            assert all(value not in output for value in (INVITE, ROTATED, OTHER)), "server logged invitation"


def malformed_proxy(backend):
    valid = proxy_header()
    cases = [b"PROXY TCP4 198.51.100.7 127.0.0.1 40000 26661\r\n",
             valid[:12] + b"\x20" + valid[13:],  # LOCAL
             valid[:13] + b"\x12" + valid[14:],  # UDP
             valid[:13] + b"\x21" + valid[14:],  # IPv6
             valid[:14] + b"\xff\xff" + valid[16:],
             valid[:24] + b"\0\0" + valid[26:], valid[:15]]
    for payload in cases:
        with socket.create_connection(("127.0.0.1", backend), timeout=2) as sock:
            sock.settimeout(2)
            sock.sendall(payload)
            try:
                assert sock.recv(1) == b"", "malformed PROXY reached application"
            except ConnectionResetError:
                pass


def source_accounting(backend):
    # Distinct original peers share the same loopback proxy. Saturating one
    # source must not classify all public clients as one loopback participant.
    with contextlib.ExitStack() as stack:
        def connect(source):
            sock = stack.enter_context(socket.create_connection(("127.0.0.1", backend), timeout=2))
            sock.settimeout(.15)
            sock.sendall(proxy_header(source))
            return sock
        held = [connect("198.51.100.40") for _ in range(4)]
        for sock in held:
            try:
                sock.recv(1)
                raise AssertionError("per-source allowance closed too early")
            except socket.timeout:
                pass
        excess = connect("198.51.100.40")
        excess.settimeout(1)
        try:
            assert excess.recv(1) == b"", "fifth pending source admission accepted"
        except ConnectionResetError:
            pass
        other = connect("198.51.100.41")
        try:
            other.recv(1)
            raise AssertionError("independent source was rejected")
        except socket.timeout:
            pass


def main():
    client, server, resources = map(lambda value: Path(value).resolve(), sys.argv[1:4])
    review = len(sys.argv) > 4
    cases = sys.argv[4:]
    end_eof_regressions = cases == ["--end-eof-regressions"]
    if end_eof_regressions:
        # One synchronized forwarding race, distinct unconfirmed loss, then the
        # existing real flow journeys. Original arena, inputs/deadlines remain.
        cases = ["flow-race", "notice-eof-match"] + ["flow"] * 4
    if cases == ["--review-regressions"]:
        cases = [f"notice-{reason}-{phase}" for phase in ("lobby", "match", "summary")
                 for reason in ("maintenance", "controller", "wrong", "eof")] + ["recovery", "expiry", "summary-return"]
    if cases == ["--authorization-regressions"]:
        cases = ["attack-start", "attack-setup", "attack-roster", "attack-return", "attack-advance"]
    with tempfile.TemporaryDirectory(prefix="duel6r-public-test-") as temp:
        root = Path(temp)
        shutil.copytree(resources / "data", root / "data")
        (root / "levels").mkdir()
        width, height = (10, 3) if review and not end_eof_regressions and "flow-race" not in cases else (24, 8)
        blocks = [int(x in (0, width - 1) or y in (0, height - 1))
                  for y in range(height) for x in range(width)]
        (root / "levels" / "arena.json").write_text(json.dumps(
            dict(width=width, height=height, blocks=blocks, elevators=[])))
        certificates(root)
        # Resolver uses executable-relative lookup. Stage a separate test copy.
        executable = root / client.name
        shutil.copy2(client, executable)
        shutil.copy2(server.with_name("duel6r-resolver"), root / "duel6r-resolver")
        def run(peer, invitation, scenario, trusted=True):
            env = dict(os.environ, D6R_TEST_INVITE=invitation,
                       SSL_CERT_FILE=str(root / "ca.pem") if trusted else str(root / "missing.pem"),
                       SSL_CERT_DIR=str(root / "no-ca-directory"))
            forwarded_before = peer.forwarded_ends
            result = subprocess.run([str(executable), str(peer.port), str(root), scenario],
                                    cwd=root, env=env, capture_output=True, text=True, timeout=140)
            assert all(value not in result.stdout + result.stderr for value in (INVITE, ROTATED, OTHER)), "client logged invitation"
            assert result.returncode == 0, result.stdout + result.stderr + peer.evidence()
            if scenario in ("flow", "flow-race"):
                assert peer.forwarded_ends >= forwarded_before + 2, "End was observed but not forwarded; " + peer.evidence()
            if scenario == "flow-race":
                assert peer.race_write_failed.is_set(), "End forwarding regression missed closed-write failure"
                print("PASS real End forwarded after synchronized opposite write failure", flush=True)
            if scenario == "notice-eof-match":
                observation = peer.eof_observation
                assert observation and observation["complete"] and observation["connection"] == 2, "EOF observation did not finish before unwind"
                assert observation["ends"] == 0, "unconfirmed affected-connection EOF must not become End"
                print("PASS unconfirmed active-match loss stays distinct from End", flush=True)
            print(result.stdout.strip(), flush=True)
        if review:
            failures = []
            for iteration, scenario in enumerate(cases, 1):
                # Independent service instances preserve all negative scenarios
                # after a production failure without carrying stranded sessions.
                for name in ("action", "acted", "action.tmp", "attack.bin", "attack-rejected"):
                    (root / name).unlink(missing_ok=True)
                with service(server, root, INVITE) as (backend, _):
                    peer = TlsPeer(root / "valid.pem", root / "valid.key", backend, root, scenario == "flow-race")
                    try:
                        run(peer, INVITE, scenario)
                    except (AssertionError, subprocess.TimeoutExpired) as error:
                        failures.append(scenario)
                        print(f"FAIL iteration {iteration} {scenario}: {str(error)[:1000]}", flush=True)
                        print(peer.evidence(), flush=True)  # Bounded decisive tail is never source-truncated.
                    finally:
                        peer.close()
            assert not failures, "Public review regressions failed: " + ", ".join(failures)
            return
        with service(server, root, INVITE) as (backend, ready):
            malformed_proxy(backend)
            print("PASS strict malformed PROXY framing", flush=True)
            source_accounting(backend)
            print("PASS original-source pending accounting", flush=True)
            for cert, trusted in (("wrong", True), ("valid", False), ("expired", True)):
                peer = TlsPeer(root / (cert + ".pem"), root / (cert + ".key"), backend)
                try:
                    run(peer, INVITE, "security", trusted)
                    assert peer.application_bytes == 0, "credentials/application bytes disclosed before verification"
                finally:
                    peer.close()
            peer = TlsPeer(root / "valid.pem", root / "valid.key", backend)
            try:
                run(peer, OTHER, "unauthorized")
                run(peer, INVITE, "flow")
                assert peer.application_bytes > 0 and peer.handshakes >= 4
                assert subprocess.run([str(server), f"--check-ready={ready}"], capture_output=True, timeout=2).returncode == 0
            finally:
                peer.close()
            peer = TlsPeer(root / "valid.pem", root / "valid.key", backend)
            try:
                run(peer, INVITE, "concurrent")
            finally:
                peer.close()
        with service(server, root, ROTATED) as (backend, _):
            peer = TlsPeer(root / "valid.pem", root / "valid.key", backend)
            try:
                run(peer, INVITE, "unauthorized")
                run(peer, ROTATED, "flow")
            finally:
                peer.close()
        print("PASS invitation rotation, non-disclosing logs, readiness after reset", flush=True)


if __name__ == "__main__":
    main()
