#!/usr/bin/env python3
"""Controlled native kernel boundaries using a separately compiled test guardian."""
import json
from pathlib import Path
import os
import signal
import socket
import subprocess
import sys
import tempfile

from DarwinProcessTests import require, until


def run(helper, worker):
    require(sys.platform == "darwin", "native Darwin execution required")

    def identity(pid):
        text = subprocess.run([worker, "--identity", str(pid)], check=True,
                              capture_output=True, text=True, timeout=2).stdout.strip()
        return None if text == "absent" else tuple(map(int, text.split()))

    def gone(original):
        value = identity(original[0])
        return value is None or value[:3] != original[:3]

    def worker_rows(root):
        try:
            rows = (root / "worker").read_text().splitlines()
            return rows if len(rows) == 3 else None
        except FileNotFoundError:
            return None

    owner_script = Path(__file__).with_name("DarwinProcessTests.py")
    unrelated = subprocess.Popen([worker, "--descendant"], start_new_session=True)
    try:
        for fault in ("registration-failure", "before-registration", "after-registration",
                      "before-spawn", "after-spawn", "lost-anchor", "partial-status", "leaked-high-fd"):
            with tempfile.TemporaryDirectory(prefix="duel6r-darwin-boundary-") as temporary:
                root = Path(temporary)
                app = subprocess.Popen([sys.executable, str(owner_script), "--owner", helper,
                                        worker, str(root), "blocked", fault])
                known = []
                guardian_id = None
                try:
                    until(lambda: (root / "owner").exists())
                    guardian = json.loads((root / "owner").read_text())["guardian"]
                    if fault in ("before-registration", "after-registration", "before-spawn", "after-spawn"):
                        until(lambda: (root / "barrier").exists())
                        require((root / "barrier").read_text() == fault, "wrong controlled boundary")
                        guardian_id = identity(guardian)
                        require(guardian_id is not None, "guardian died before barrier")
                        if fault == "after-spawn":
                            rows = until(lambda: worker_rows(root))
                            known.extend(tuple(map(int, row.split())) for row in rows[:2])
                        app.kill()
                        app.wait(timeout=1)
                        (root / "release").touch()
                        until(lambda: (root / "guardian-result").exists())
                        until(lambda: gone(guardian_id))
                        if fault.endswith("registration"):
                            require(not (root / "spawned").exists(), "parent loss allowed worker spawn")
                        else:
                            require((root / "spawned").exists(), "spawn boundary was not exercised")
                            known.append(tuple(map(int, (root / "spawned").read_text().split())))
                            require((root / "reaped-by-cleanup").exists(), "no positive anchored cleanup")
                        until(lambda: all(gone(item) for item in known))
                    elif fault == "lost-anchor":
                        rows = until(lambda: worker_rows(root))
                        known = [tuple(map(int, row.split())) for row in rows[:2]]
                        os.kill(known[0][0], signal.SIGKILL)
                        require(app.wait(timeout=3) == 0, "owner failed")
                        require((root / "anchor-reaped").exists(), "real direct child was not reaped by fixture")
                        require(not (root / "signal-after-reap").exists(), "PGID signalled after ownership loss")
                        require(not (root / "reaped-by-cleanup").exists(), "ownership loss claimed anchored cleanup")
                        events = json.loads((root / "events").read_text())
                        require(any(event == 4 for event, _ in events), "ownership loss not terminal")
                        require(not any(event == 3 for event, _ in events), "ownership loss reported completed cleanup")
                        require((root / "exit").read_text() == "2", "ownership loss returned success")
                        # The negative ownership test intentionally removes the
                        # anchor before the guardian can kill descendants. The
                        # test owner, not production, disposes this exact fixture.
                        if not gone(known[1]):
                            os.kill(known[1][0], signal.SIGKILL)
                        until(lambda: all(gone(item) for item in known))
                    elif fault == "leaked-high-fd":
                        require(app.wait(timeout=3) == 0, "owner failed")
                        require((root / "worker-exit-status").read_text() == "4", "worker failed to reject leaked high FD")
                        require(not (root / "worker").exists(), "leaked-FD worker initialized a listener")
                        require((root / "reaped-by-cleanup").exists(), "rejected worker not positively reaped")
                    else:
                        require(app.wait(timeout=3) == 0, "owner failed")
                        require((root / "exit").read_text() == "2", "fault returned success")
                        if fault == "registration-failure":
                            require((root / "registration-failed").exists(), "registration fault not exercised")
                            require(not (root / "spawned").exists(), "failed registration spawned a worker")
                            require(not (root / "worker").exists(), "failed registration initialized a worker")
                        else:
                            require((root / "sends").read_text() == "1", "partial frame was followed by another send")
                            require((root / "channel-end").read_text() == "8", "partial frame was not sealed at EOF")
                            require(not (root / "events").exists(), "partial bytes were resynchronized into an event")
                            require((root / "reaped-by-cleanup").exists(), "status backpressure blocked cleanup")
                            known.append(tuple(map(int, (root / "spawned").read_text().split())))
                            until(lambda: all(gone(item) for item in known))
                    rows = worker_rows(root)
                    require(int((root / "high-fd-observed").read_text()) > 1023,
                            "fixture did not inherit a high FD into guardian")
                    if rows:
                        with socket.socket() as listener:
                            listener.bind(("127.0.0.1", int(rows[2])))
                    require(unrelated.poll() is None, "unrelated process affected")
                    print("PASS native boundary", fault, flush=True)
                finally:
                    # Release an injected pause even after an assertion failure.
                    (root / "release").touch()
                    (root / "cancel").touch()
                    if app.poll() is None:
                        try:
                            app.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            app.kill()
                            app.wait(timeout=2)
                    rows = worker_rows(root)
                    if rows:
                        known.extend(tuple(map(int, row.split())) for row in rows[:2])
                    for item in known:
                        if not gone(item):
                            try:
                                os.kill(item[0], signal.SIGKILL)
                            except ProcessLookupError:
                                pass
    finally:
        unrelated.kill()
        unrelated.wait(timeout=2)


if __name__ == "__main__":
    run(*sys.argv[1:])
