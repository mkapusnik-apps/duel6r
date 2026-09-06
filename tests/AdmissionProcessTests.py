#!/usr/bin/env python3
"""Actual-process compatibility admission checks for the explicit scaffold."""
import os
import json
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


def unused_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def resources(root, runtime_root, changed=False):
    for directory in ("levels", "data", "profiles", "textures"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    # Canonical-runtime fixture: a closed ten-by-three arena with eight valid
    # standing positions, matching the proven headless process-test shape.
    width, height = 10, 3
    blocks = [1] * width + [1] + [0] * (width - 2) + [1] + [1] * width
    shipped_level = json.dumps({
        "width": width, "height": height, "blocks": blocks, "elevators": []
    }, separators=(",", ":")).encode("utf-8")
    # Whitespace preserves a valid canonical level while producing a distinct
    # gameplay hash for the mismatch case.
    (root / "levels" / "arena.json").write_bytes(shipped_level + (b"\n" if changed else b""))
    (root / "levels" / "arena.meta").write_bytes(b"metadata")
    shutil.copy(runtime_root / "data" / "blocks.json", root / "data" / "blocks.json")
    config = ["volume 128", "music on"]
    config.extend(f"gun {index} {'true' if index == 0 else 'false'}" for index in range(17))
    (root / "data" / "config.script").write_text("\n".join(config) + "\n", encoding="utf-8")
    (root / "profiles" / "guest.lua").write_bytes(b"must never execute")
    (root / "textures" / "cosmetic.png").write_bytes(b"cosmetic")


def wait_line(process, text, timeout=10):
    deadline, lines = time.monotonic() + timeout, []
    while time.monotonic() < deadline:
        line = process.stdout.readline()
        if line:
            lines.append(line)
            if text in line:
                return "".join(lines)
        elif process.poll() is not None:
            break
    raise AssertionError(f"missing {text!r}; rc={process.poll()} output={''.join(lines)!r}")


def run_client(executable, root, port):
    return subprocess.run(
        [executable, "--admission-client", "--host=127.0.0.1", f"--port={port}",
         f"--resources={root}", "--local-players=2"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=12, check=False)


def process_smoke(executable, base):
    host_root, guest_root, changed_root = base / "host", base / "guest", base / "changed"
    runtime_root = Path(executable).resolve().parent
    resources(host_root, runtime_root)
    resources(guest_root, runtime_root)
    resources(changed_root, runtime_root, changed=True)
    # Excluded guest-local content differs without affecting admission.
    (guest_root / "profiles" / "guest.lua").write_bytes(b"different and not executable")
    (guest_root / "textures" / "cosmetic.png").write_bytes(b"different cosmetic")
    port = unused_port()
    server = subprocess.Popen(
        [executable, "--transport", "--local-only", "--host=127.0.0.1", f"--port={port}",
         f"--resources={host_root}", "--local-players=1"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    compatible = None
    try:
        startup = wait_line(server, f"transport ready on 127.0.0.1:{port}")
        warning = server.stdout.readline()
        startup += warning
        assert "compatibility admission are active" in startup
        assert "no lobby" in startup and "playable" in startup

        pending = socket.create_connection(("127.0.0.1", port), timeout=2)
        pending.settimeout(5)
        pending_started = time.monotonic()
        assert pending.recv(1) == b"", "pending connection remained open without its first request"
        pending_elapsed = time.monotonic() - pending_started
        pending.close()
        assert 2.5 <= pending_elapsed < 4.5, pending_elapsed

        compatible = subprocess.Popen(
            [executable, "--admission-client", "--host=127.0.0.1", f"--port={port}",
             f"--resources={guest_root}", "--local-players=2"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        admitted_output = wait_line(compatible, "admitted")
        identity_line = compatible.stdout.readline()
        lines = (admitted_output + identity_line).splitlines()
        assert lines[0] == "admitted", compatible.stdout
        assert lines[1].startswith("participant-id=") and "player-ids=" in lines[1]
        ids = lines[1].replace("participant-id=", "").replace(" player-ids=", ",").split(",")
        assert len(ids) == 3 and all(int(value) > 0 for value in ids) and len(set(ids)) == 3
        time.sleep(0.2)
        if compatible.poll() is not None:
            remaining = compatible.stdout.read()
            server_output = server.stdout.read() if server.poll() is not None else "<server still running>"
            raise AssertionError(
                f"admitted client did not remain connected: rc={compatible.returncode} "
                f"output={admitted_output + identity_line + remaining!r} "
                f"server-rc={server.poll()} server-output={server_output!r}")
        rejected = run_client(executable, changed_root, port)
        assert rejected.returncode == 2
        assert rejected.stdout == "gameplay-content-mismatch\nGameplay content mismatch. Use the host's exact supported gameplay content.\n"
    finally:
        if compatible is not None and compatible.poll() is None:
            compatible.terminate()
            compatible.wait(timeout=5)
        if server.poll() is None:
            server.send_signal(signal.SIGTERM)
            server.wait(timeout=5)


def invalid_host_fails_before_listener(executable, base):
    invalid = base / "invalid"
    (invalid / "levels").mkdir(parents=True)
    (invalid / "levels" / "arena.json").write_bytes(b"arena")
    port = unused_port()
    completed = subprocess.run(
        [executable, "--transport", "--local-only", "--host=127.0.0.1", f"--port={port}",
         f"--resources={invalid}"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=3, check=False)
    expected = ("host-gameplay-content-manifest-invalid\n"
                "Hosted gameplay content is invalid. Restore the supported gameplay content and restart the application.\n")
    assert completed.returncode == 2 and completed.stdout == expected, completed.stdout
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.settimeout(1)
        assert probe.connect_ex(("127.0.0.1", port)) != 0


def invalid_guest_fails_before_connection(executable, base):
    invalid = base / "invalid-guest"
    (invalid / "levels").mkdir(parents=True)
    (invalid / "levels" / "arena.json").write_bytes(b"arena")
    port = unused_port()
    accepted = []
    ready = threading.Event()
    def listen():
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.bind(("127.0.0.1", port)); server.listen(1); server.settimeout(1)
            ready.set()
            try:
                connection, _ = server.accept()
                accepted.append(connection)
                connection.close()
            except socket.timeout:
                pass
    listener = threading.Thread(target=listen)
    listener.start(); assert ready.wait(2)
    completed = run_client(executable, invalid, port)
    listener.join(timeout=2)
    expected = ("guest-gameplay-content-manifest-invalid\n"
                "Local gameplay content is invalid. Restore the supported gameplay content and restart the application.\n")
    assert completed.returncode == 2 and completed.stdout == expected, completed.stdout
    assert not accepted, "guest-local invalid manifest attempted a connection"


def incomplete_transport_outcomes(executable, base):
    guest = base / "incomplete-guest"
    resources(guest, Path(executable).resolve().parent)

    def fake_host(close_immediately):
        port = unused_port()
        ready = threading.Event()
        def serve():
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                listener.bind(("127.0.0.1", port)); listener.listen(1); ready.set()
                connection, _ = listener.accept()
                with connection:
                    if not close_immediately:
                        time.sleep(10.5)
        worker = threading.Thread(target=serve)
        worker.start(); assert ready.wait(2)
        return port, worker

    port, worker = fake_host(False)
    timed_out = run_client(executable, guest, port)
    worker.join(timeout=2)
    assert timed_out.returncode == 2
    assert timed_out.stdout == "Connection timed out.\n", timed_out.stdout

    port, worker = fake_host(True)
    closed = run_client(executable, guest, port)
    worker.join(timeout=2)
    assert closed.returncode == 2
    assert closed.stdout == "Connection ended before admission completed.\n", closed.stdout


def invalid_and_partial_host_messages(executable, base):
    guest = base / "host-message-guest"
    resources(guest, Path(executable).resolve().parent)

    def serve(payload, partial=False):
        port = unused_port(); ready = threading.Event()
        def worker():
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
                listener.bind(("127.0.0.1", port)); listener.listen(1); ready.set()
                connection, _ = listener.accept()
                with connection:
                    header = connection.recv(12)
                    if len(header) == 12:
                        _, _, _, size = struct.unpack("!IHHI", header)
                        remaining = size
                        while remaining:
                            chunk = connection.recv(remaining)
                            if not chunk: break
                            remaining -= len(chunk)
                    wire_size = len(payload) + (10 if partial else 0)
                    connection.sendall(struct.pack("!IHHI", 0x44365254, 1, 0, wire_size) + payload)
        thread = threading.Thread(target=worker); thread.start(); assert ready.wait(2)
        return port, thread

    # Offer repeats participant identity as its player identity.
    invalid_offer = b"D6RO" + struct.pack("!QBQ", 1, 1, 1)
    port, thread = serve(invalid_offer)
    invalid = run_client(executable, guest, port); thread.join(timeout=2)
    assert invalid.returncode == 2
    assert invalid.stdout == ("invalid-host-admission-message\n"
                              "Connection ended before admission completed.\n"), invalid.stdout

    stale_admitted_result = b"D6RS" + struct.pack("!HQBQQ", 11, 10, 2, 11, 12)
    port, thread = serve(stale_admitted_result)
    stale = run_client(executable, guest, port); thread.join(timeout=2)
    assert stale.returncode == 2
    assert stale.stdout == ("invalid-host-admission-message\n"
                            "Connection ended before admission completed.\n"), stale.stdout

    port, thread = serve(b"partial", partial=True)
    partial = run_client(executable, guest, port); thread.join(timeout=2)
    assert partial.returncode == 2
    assert partial.stdout == "Connection ended before admission completed.\n", partial.stdout

    # A valid offer accepted by the guest is not success without the fourth confirmation.
    valid_offer = b"D6RO" + struct.pack("!QBQQ", 10, 2, 11, 12)
    port, thread = serve(valid_offer)
    lost_confirmation = run_client(executable, guest, port); thread.join(timeout=2)
    assert lost_confirmation.returncode == 2
    assert lost_confirmation.stdout == "Connection ended before admission completed.\n", lost_confirmation.stdout


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: AdmissionProcessTests.py /path/to/duel6r-server")
    with tempfile.TemporaryDirectory(prefix="duel6r-admission-process-") as directory:
        base = Path(directory)
        invalid_host_fails_before_listener(sys.argv[1], base)
        invalid_guest_fails_before_connection(sys.argv[1], base)
        process_smoke(sys.argv[1], base)
        incomplete_transport_outcomes(sys.argv[1], base)
        invalid_and_partial_host_messages(sys.argv[1], base)
    print("actual-process compatible and rejected admission behavior passed")
