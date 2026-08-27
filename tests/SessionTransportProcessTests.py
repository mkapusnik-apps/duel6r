#!/usr/bin/env python3
"""Separate-process behavioral checks for duel6r-server's diagnostic TCP transport."""
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

MAGIC, VERSION, APPLICATION, MAX_PAYLOAD = 0x44365254, 1, 0, 1024 * 1024

def unused_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0)); return probe.getsockname()[1]

def recv_exact(connection, size):
    result = bytearray()
    while len(result) < size:
        chunk = connection.recv(size - len(result))
        if not chunk: raise AssertionError("peer closed before a complete frame")
        result.extend(chunk)
    return bytes(result)

def send_frame(connection, payload):
    connection.sendall(struct.pack("!IHHI", MAGIC, VERSION, APPLICATION, len(payload)) + payload)

def receive_application_frame(connection):
    while True:
        magic, version, kind, size = struct.unpack("!IHHI", recv_exact(connection, 12))
        assert (magic, version) == (MAGIC, VERSION)
        payload = recv_exact(connection, size)
        if kind == APPLICATION: return payload
        assert kind in (1, 2) and not payload
        if kind == 1: connection.sendall(struct.pack("!IHHI", MAGIC, VERSION, 2, 0))

def wait_ready(process, expected):
    lines, deadline = [], time.monotonic() + 10
    while time.monotonic() < deadline:
        line = process.stdout.readline()
        if line:
            lines.append(line)
            if expected in line: return lines
        elif process.poll() is not None: break
    raise AssertionError(f"server did not honestly report readiness: {''.join(lines)!r}")

def exercise(executable, host):
    port = unused_port()
    process = subprocess.Popen([executable, "--transport-echo", f"--host={host}", f"--port={port}", "--max-clients=15"],
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    try:
        output = wait_ready(process, f"transport ready on {host}:{port}")
        output.append(process.stdout.readline())
        assert any("no lobby, admission, simulation" in line for line in output)
        clients = [socket.create_connection((host, port), timeout=3) for _ in range(15)]
        for client in clients: client.settimeout(5)
        payloads = [bytes([index, 0, 255]) + f"opaque-handshake-{index}".encode() for index in range(15)]
        for client, payload in zip(clients, payloads): send_frame(client, payload)
        results = [None] * 15
        threads = [threading.Thread(target=lambda i=i: results.__setitem__(i, receive_application_frame(clients[i]))) for i in range(15)]
        for thread in threads: thread.start()
        for thread in threads:
            thread.join(6); assert not thread.is_alive(), "isolated echo made no bounded progress"
        assert results == payloads, "cross-client payload leakage or corruption"
        ordered = [b"", b"\x00\xffbinary\x00", bytes(range(256)), bytes([0xA5]) * MAX_PAYLOAD]
        clients[0].sendall(b"".join(struct.pack("!IHHI", MAGIC, VERSION, APPLICATION, len(value)) + value for value in ordered))
        assert [receive_application_frame(clients[0]) for _ in ordered] == ordered
        clients[0].settimeout(.15)
        try:
            assert not clients[0].recv(1), "frame duplication or merging produced trailing data"
        except socket.timeout: pass
        for client in clients: client.close()
        started = time.monotonic(); process.send_signal(signal.SIGTERM); process.wait(timeout=3)
        assert process.returncode == 0 and time.monotonic() - started < 3
        assert "transport stopped" in process.stdout.read()
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.settimeout(1); assert probe.connect_ex(("127.0.0.1", port)) != 0
    finally:
        if process.poll() is None: process.kill(); process.wait(timeout=3)

def ordinary_startup_has_no_listener(executable):
    port = unused_port()
    completed = subprocess.run([executable, "--host=127.0.0.1", f"--port={port}"], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, timeout=3, check=False)
    assert completed.returncode == 2 and "did not listen" in completed.stdout
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.settimeout(1); assert probe.connect_ex(("127.0.0.1", port)) != 0

if __name__ == "__main__":
    if len(sys.argv) != 2: raise SystemExit("usage: SessionTransportProcessTests.py /path/to/duel6r-server")
    ordinary_startup_has_no_listener(sys.argv[1]); exercise(sys.argv[1], "127.0.0.1"); exercise(sys.argv[1], "localhost")
    print("separate-process transport behavior passed for IPv4 literal and hostname")
