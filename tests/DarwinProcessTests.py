#!/usr/bin/env python3
"""Native Darwin containment tests; no GUI, external network, or fabricated skips."""
import json
import fcntl
import os
from pathlib import Path
import select
import resource
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def until(predicate, seconds=3):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.005)
    raise AssertionError("bounded observation did not complete")


def owner(helper, worker, directory, mode, fault=""):
    def publish(name, value):
        temporary = directory / (name + ".part")
        temporary.write_text(value)
        temporary.replace(directory / name)

    read_fd, write_fd = os.pipe()
    status, child_status = socket.socketpair()
    # This owner alone retains write_fd. A deliberately unrelated inherited FD
    # and environment value must not cross the guardian -> worker boundary.
    unrelated_fd = os.open(os.devnull, os.O_RDONLY)
    soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    require(hard == resource.RLIM_INFINITY or hard > 4096, "high-FD inheritance fixture requires FD 4096")
    if soft != resource.RLIM_INFINITY and soft <= 4096:
        resource.setrlimit(resource.RLIMIT_NOFILE, (4097, hard))
    high_fd = fcntl.fcntl(unrelated_fd, fcntl.F_DUPFD, 4096)
    require(high_fd > 1023, "high-FD fixture was not created")
    environment = {"D6R_TEST_PRIVATE": "non-secret-inheritance-sentinel"}
    if fault:
        environment.update(D6R_TEST_GUARDIAN_FAULT=fault, D6R_TEST_GUARDIAN_DIRECTORY=str(directory),
                           D6R_TEST_GUARDIAN_HIGH_FD=str(high_fd))
    child = subprocess.Popen(
        [helper, str(os.getpid()), str(read_fd), str(child_status.fileno()),
         worker, str(directory / "worker"), mode],
        pass_fds=(read_fd, child_status.fileno(), unrelated_fd, high_fd),
        env=environment,
    )
    os.close(read_fd)
    os.close(unrelated_fd)
    os.close(high_fd)
    child_status.close()
    publish("owner", json.dumps({"guardian": child.pid}))
    buffered = b""
    events = []
    cancel = False
    eof = False
    exited_at = None
    while True:
        if (directory / "cancel").exists() and not cancel:
            os.close(write_fd)
            cancel = True
        readable, _, _ = select.select([] if eof else [status], [], [], 0.02)
        if readable:
            data = status.recv(128)
            if data:
                buffered += data
                while len(buffered) >= 8:
                    event, pid = struct.unpack("!II", buffered[:8])
                    buffered = buffered[8:]
                    events.append([event, pid])
                    publish("events", json.dumps(events))
            else:
                eof = True
                publish("channel-end", str(len(buffered)))
        code = child.poll()
        if code is not None:
            if not eof:
                if exited_at is None:
                    exited_at = time.monotonic()
                require(time.monotonic() - exited_at < 1, "status writer survived guardian exit")
                continue
            if not cancel:
                os.close(write_fd)
            publish("exit", str(code))
            status.close()
            return


def run(helper, worker, unknown_helper):
    require(sys.platform == "darwin", "native Darwin execution required")

    def identity(pid):
        result = subprocess.run([worker, "--identity", str(pid)], check=True,
                                capture_output=True, text=True, timeout=2).stdout.strip()
        return None if result == "absent" else tuple(map(int, result.split()))

    def gone(original):
        current = identity(original[0])
        return current is None or current[:3] != original[:3]

    # Another owned test process outside every guarded group must survive.
    unrelated = subprocess.Popen([worker, "--descendant"], start_new_session=True)
    try:
        for scenario in ("cancel", "app-kill", "app-kill-stopped", "leader-kill",
                         "guardian-kill", "resolve", "unknown-inspection"):
            with tempfile.TemporaryDirectory(prefix="duel6r-darwin-") as temporary:
                directory = Path(temporary)
                mode = "stopped" if scenario == "app-kill-stopped" else "resolve" if scenario == "resolve" else "blocked"
                selected_helper = unknown_helper if scenario == "unknown-inspection" else helper
                app = subprocess.Popen([sys.executable, __file__, "--owner", selected_helper,
                                        worker, str(directory), mode])
                identities = []
                guardian = None
                try:
                    until(lambda: (directory / "owner").exists())
                    guardian = json.loads((directory / "owner").read_text())["guardian"]
                    if scenario == "resolve":
                        require(app.wait(timeout=3) == 0, "resolver owner failed")
                        require((directory / "worker").read_text() == "resolved\n", "real resolver did not run")
                        require((directory / "exit").read_text() == "0", "resolver cleanup failed")
                        continue

                    def ready():
                        try:
                            lines = (directory / "worker").read_text().splitlines()
                            return lines if len(lines) == 3 else None
                        except FileNotFoundError:
                            return None
                    lines = until(ready)
                    identities = [tuple(map(int, line.split())) for line in lines[:2]]
                    port = int(lines[2])
                    require(all(identity(item[0])[:4] == item[:4] for item in identities), "identity changed before fault")
                    guardian_identity = identity(guardian)
                    require(guardian_identity is not None, "guardian not live before fault")
                    if scenario == "app-kill-stopped":
                        until(lambda: all(identity(item[0])[4] == 4 for item in identities))
                    started = time.monotonic()
                    if scenario in ("app-kill", "app-kill-stopped"):
                        app.kill()
                        app.wait(timeout=1)
                    elif scenario == "leader-kill":
                        os.kill(identities[0][0], signal.SIGKILL)
                    elif scenario == "guardian-kill":
                        os.kill(guardian, signal.SIGKILL)
                    else:
                        (directory / "cancel").touch()
                    if scenario == "unknown-inspection":
                        # The deliberately retained direct-child zombie is the
                        # ownership anchor, not a running leaked service.
                        until(lambda: gone(identities[1]) and (directory / "events").exists()
                              and any(event == 2 for event, _ in json.loads((directory / "events").read_text())))
                    else:
                        until(lambda: all(gone(item) for item in identities))
                    require(time.monotonic() - started < 3, "three-second cleanup deadline exceeded")
                    with socket.socket() as listener:
                        listener.bind(("127.0.0.1", port))
                    require(unrelated.poll() is None, "unrelated process was affected")
                    if scenario == "unknown-inspection":
                        # Actual worker tree is gone, but intentionally incomplete
                        # inspection must keep guardian/anchor ownership pending.
                        events = json.loads((directory / "events").read_text())
                        require(not any(event == 3 for event, _ in events), "unknown inspection claimed cleanup")
                        require(app.poll() is None and identity(guardian) is not None, "cleanup ownership discarded")
                        os.kill(guardian, signal.SIGKILL)  # Test-only owner release after proven worker death.
                    if app.poll() is None:
                        require(app.wait(timeout=3) == 0, "owner failed")
                    until(lambda: gone(guardian_identity))
                    until(lambda: all(gone(item) for item in identities))
                    if scenario not in ("app-kill", "app-kill-stopped"):
                        events = json.loads((directory / "events").read_text())
                        confirmed = any(event == 3 for event, _ in events)
                        require(confirmed == (scenario not in ("guardian-kill", "unknown-inspection")),
                                "guardian loss/incomplete inspection misreported completed cleanup")
                    print("PASS", scenario, "identity/tree/listener/inheritance", flush=True)
                finally:
                    # Only task-owned processes; allow an operational guardian to
                    # finish first. A stopped worker is resumed for its independent
                    # monitor if the test itself failed before fault injection.
                    for item in identities:
                        if not gone(item):
                            try:
                                os.kill(item[0], signal.SIGCONT)
                            except ProcessLookupError:
                                pass
                    if app.poll() is None:
                        (directory / "cancel").touch()
                        try:
                            app.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            if guardian is not None and identity(guardian) is not None:
                                os.kill(guardian, signal.SIGKILL)
                            app.kill()
                            app.wait(timeout=2)
        # Pre-spawn cancellation: the owner deliberately closes its sole writer
        # before launch. No worker initialization or Started event is allowed.
        with tempfile.TemporaryDirectory(prefix="duel6r-darwin-cancel-") as temporary:
            read_fd, write_fd = os.pipe()
            status, child_status = socket.socketpair()
            os.close(write_fd)
            try:
                result = subprocess.run([helper, str(os.getpid()), str(read_fd), str(child_status.fileno()),
                    worker, str(Path(temporary) / "worker"), "blocked"],
                    pass_fds=(read_fd, child_status.fileno()), timeout=3)
                require(result.returncode == 0, "pre-spawn cancellation failed")
                require(not (Path(temporary) / "worker").exists(), "cancelled startup initialized worker")
                require(struct.unpack("!II", status.recv(8)) == (3, 0), "late spawn after cancellation")
            finally:
                os.close(read_fd)
                status.close()
                child_status.close()
        print("PASS pre-spawn cancellation", flush=True)
    finally:
        unrelated.kill()
        unrelated.wait(timeout=2)


if __name__ == "__main__":
    if sys.argv[1] == "--owner":
        owner(sys.argv[2], sys.argv[3], Path(sys.argv[4]), sys.argv[5], sys.argv[6] if len(sys.argv) > 6 else "")
    else:
        run(*sys.argv[1:])
