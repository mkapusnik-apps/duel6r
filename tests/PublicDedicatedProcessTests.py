#!/usr/bin/env python3
"""Local trusted-CA integration of the real public client and dedicated service.

TRU-PUB-AC-001/002, NET-PUB-AC-001/002 and HSL-PUB-002/003.
The fixture owns temporary certificates, content, sockets and processes. It is
an application protocol peer, not a deployment or proxy configuration test.
"""
import contextlib
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

    def __init__(self, cert, key, backend, control_root=None):
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
        self.frames = []
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

    def accept(self):
        while not self.stop.is_set():
            fields = self.action()
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
            raw.settimeout(2)
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
                    buffers = {client: bytearray(), backend: bytearray()}
                    while not self.stop.is_set():
                        action_path = self.control_root / "action" if self.control_root else None
                        if action_path and not buffers[backend]:
                            fields = self.action()
                            if len(fields) == 3 and int(fields[0]) == connection_id and fields[1] != "resume":
                                _, action, session = fields
                                action_path.unlink()
                                if action in ("eof", "expiry", "recovery"):
                                    self.block_reconnect = action != "recovery"
                                    self.reconnect_after = time.monotonic() + .75
                                    (self.control_root / "acted").touch()
                                    return
                                identity = int(session) + (action == "wrong")
                                payload = b"D6PE" + bytes([action != "controller"]) + struct.pack("<Q", identity)
                                client.sendall(struct.pack("!IHHI", 0x44365254, 1, 0, len(payload)) + payload)
                                (self.control_root / "acted").touch()
                        readable, _, _ = select.select([client, backend], [], [], 0 if client.pending() else .01)
                        if client.pending() and client not in readable:
                            readable.append(client)
                        for source in readable:
                            data = source.recv(65536)
                            if not data:
                                return
                            if source is client:
                                self.application_bytes += len(data)
                            buffer = buffers[source]
                            buffer.extend(data)
                            while len(buffer) >= 12:
                                _, _, kind, size = struct.unpack("!IHHI", buffer[:12])
                                if len(buffer) < 12 + size:
                                    break
                                # Only protocol kind/size, never credential-bearing payloads.
                                self.frames.append(("client" if source is client else "server", kind, size))
                                del buffer[:12 + size]
                            (backend if source is client else client).sendall(data)
        except (OSError, ssl.SSLError):
            pass  # Invalid certificates and deliberate disconnects are scenarios.
        finally:
            raw.close()

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
    if cases == ["--review-regressions"]:
        cases = [f"notice-{reason}-{phase}" for phase in ("lobby", "match", "summary")
                 for reason in ("maintenance", "controller", "wrong", "eof")] + ["recovery", "expiry"]
    with tempfile.TemporaryDirectory(prefix="duel6r-public-test-") as temp:
        root = Path(temp)
        shutil.copytree(resources / "data", root / "data")
        (root / "levels").mkdir()
        width, height = (10, 3) if review else (24, 8)
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
            result = subprocess.run([str(executable), str(peer.port), str(root), scenario],
                                    cwd=root, env=env, capture_output=True, text=True, timeout=140)
            assert all(value not in result.stdout + result.stderr for value in (INVITE, ROTATED, OTHER)), "client logged invitation"
            assert result.returncode == 0, result.stdout + result.stderr + str(peer.frames)
            print(result.stdout.strip(), flush=True)
        if review:
            failures = []
            for scenario in cases:
                # Independent service instances preserve all negative scenarios
                # after a production failure without carrying stranded sessions.
                for name in ("action", "acted", "action.tmp"):
                    (root / name).unlink(missing_ok=True)
                with service(server, root, INVITE) as (backend, _):
                    peer = TlsPeer(root / "valid.pem", root / "valid.key", backend, root)
                    try:
                        run(peer, INVITE, scenario)
                    except (AssertionError, subprocess.TimeoutExpired) as error:
                        failures.append(scenario)
                        print(f"FAIL {scenario}: {str(error)[:500]}", flush=True)
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
