#!/usr/bin/env python3
"""Separate-process behavioral checks for duel6r-server's diagnostic TCP transport."""
import ctypes
import os
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

MAGIC, VERSION, APPLICATION, MAX_PAYLOAD = 0x44365254, 1, 0, 1024 * 1024
IS_WINDOWS = os.name == "nt"


def start_server(executable, host, port):
    creationflags = subprocess.CREATE_NEW_CONSOLE if IS_WINDOWS else 0
    return subprocess.Popen(
        [executable, "--transport-echo", f"--host={host}", f"--port={port}", "--max-clients=15"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1,
        creationflags=creationflags)


def request_graceful_shutdown(process):
    if IS_WINDOWS:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        def api_error(name):
            error = ctypes.get_last_error()
            return OSError(error, f"{name} failed: {ctypes.FormatError(error).strip()}")
        kernel32.FreeConsole()
        try:
            if not kernel32.AttachConsole(process.pid):
                raise api_error("AttachConsole")
            if not kernel32.SetConsoleCtrlHandler(None, True):
                raise api_error("SetConsoleCtrlHandler(ignore=True)")
            if not kernel32.GenerateConsoleCtrlEvent(0, 0):
                raise api_error("GenerateConsoleCtrlEvent(CTRL_C_EVENT, group=0)")
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired as error:
                process.kill()
                process.wait(timeout=3)
                output = process.stdout.read()
                raise AssertionError(f"Windows server ignored CTRL_C_EVENT; output={output!r}") from error
        finally:
            kernel32.FreeConsole()
            restore_failed = not kernel32.SetConsoleCtrlHandler(None, False)
            if restore_failed and sys.exc_info()[0] is None:
                raise api_error("SetConsoleCtrlHandler(ignore=False)")
    else:
        process.send_signal(signal.SIGTERM)

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
    lines, started = [], time.monotonic()
    deadline = started + 10
    while time.monotonic() < deadline:
        line = process.stdout.readline()
        if line:
            lines.append(line)
            if expected in line: return lines
        elif process.poll() is not None: break
    raise AssertionError(
        "server did not honestly report readiness: "
        f"pid={process.pid}, returncode={process.poll()}, elapsed={time.monotonic() - started:.3f}s, "
        f"expected={expected!r}, output={''.join(lines)!r}")

def exercise(executable, host):
    port = unused_port()
    process = start_server(executable, host, port)
    clients = []
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
        for client in clients:
            client.close()
        clients.clear()
        started = time.monotonic(); request_graceful_shutdown(process); process.wait(timeout=3)
        shutdown_output = process.stdout.read()
        assert process.returncode == 0, f"server exit code {process.returncode}; output={shutdown_output!r}"
        assert time.monotonic() - started < 3
        assert "transport stopped" in shutdown_output, f"missing graceful-stop output: {shutdown_output!r}"
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.settimeout(1); assert probe.connect_ex(("127.0.0.1", port)) != 0
    finally:
        for client in clients:
            client.close()
        if process.poll() is None: process.kill(); process.wait(timeout=3)

def ordinary_startup_has_no_listener(executable):
    port = unused_port()
    completed = subprocess.run([executable, "--host=127.0.0.1", f"--port={port}"], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, timeout=3, check=False)
    assert completed.returncode == 2 and "did not listen" in completed.stdout
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.settimeout(1); assert probe.connect_ex(("127.0.0.1", port)) != 0

def unsupported_listener_addresses_are_rejected_before_listen(executable):
    expected = "Network session cannot use a public or wildcard address. Use loopback or a private LAN address.\n"
    for host in ("0.0.0.0", "8.8.8.8", "169.254.1.1", "224.0.0.1", "255.255.255.255"):
        port = unused_port()
        completed = subprocess.run(
            [executable, "--transport-echo", f"--host={host}", f"--port={port}"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=3, check=False)
        assert completed.returncode == 2, (host, completed.returncode, completed.stdout)
        assert completed.stdout == expected, (host, completed.stdout)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.settimeout(1)
            assert probe.connect_ex(("127.0.0.1", port)) != 0, f"rejected host {host} created a listener"

def hostname_startup_stress(executable, attempts=16):
    """Exercise the short-lived resolver pipe-close/process-exit ordering repeatedly."""
    for iteration in range(attempts):
        port = unused_port()
        process = start_server(executable, "localhost", port)
        try:
            wait_ready(process, f"transport ready on localhost:{port}")
        except Exception as error:
            raise AssertionError(
                f"hostname startup stress iteration={iteration}/{attempts}, port={port}: {error}") from error
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=3)

if __name__ == "__main__":
    if len(sys.argv) != 2: raise SystemExit("usage: SessionTransportProcessTests.py /path/to/duel6r-server")
    ordinary_startup_has_no_listener(sys.argv[1]); unsupported_listener_addresses_are_rejected_before_listen(sys.argv[1])
    hostname_startup_stress(sys.argv[1])
    exercise(sys.argv[1], "127.0.0.1"); exercise(sys.argv[1], "localhost")
    print("separate-process transport behavior passed for IPv4 literal and hostname")
