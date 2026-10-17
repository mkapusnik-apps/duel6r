"""Owned local crypto composition using the existing PKI/opaque relay/service.

No deployment, credentials, network profile or application source changes.
Each process owns its private fixtures; only fixed result text is retained.
"""
import contextlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import time
from PublicDedicatedProcessTests import INVITE, ROTATED, OTHER, ChannelPeer, service


def wait_marker(root, name, process, absolute):
    while time.monotonic() < absolute:
        if (root / name).exists():
            return
        assert process.poll() is None, f"crypto fixture exited before {name}"
        time.sleep(.01)
    raise AssertionError(f"crypto fixture original deadline before {name}")


def main():
    client, server, resources = (Path(value).resolve() for value in sys.argv[1:4])
    cases = sys.argv[4:] or ["scoped-resume", "restart-same", "restart-rotated", "pw-cancel", "pw-terminal",
                             "pw-leave", "pw-end", "pw-disposal", "pw-shutdown"]
    with tempfile.TemporaryDirectory(prefix="duel6r-public-crypto-") as directory:
        root = Path(directory)
        shutil.copytree(resources / "data", root / "data")
        (root / "levels").mkdir()
        width, height = 10, 3
        (root / "levels/arena.json").write_text(json.dumps(dict(width=width, height=height,
            blocks=[int(x in (0, width - 1) or y in (0, height - 1)) for y in range(height) for x in range(width)], elevators=[])))
        executable = root / client.name
        shutil.copy2(client, executable)
        shutil.copy2(server.with_name("duel6r-resolver"), root / "duel6r-resolver")
        environment = dict(os.environ)
        for scenario in cases:
            for name in ("action", "action.tmp", "acted", "outer-handshake-1", "restart-ready",
                         "restart-reconnecting", "shutdown-ready", "outer-latest-binding"):
                (root / name).unlink(missing_ok=True)
            process = None
            with (root / "client.log").open("w+") as log, contextlib.ExitStack() as stack:
                handle = {}
                backend, ready = stack.enter_context(service(server, root, INVITE, process_handle=handle,
                                                              readiness_name=f"{scenario}-ready.sock"))
                peer = ChannelPeer(backend, root)
                stack.callback(peer.close)
                peer.hold_initial_inner = scenario == "pw-cancel"
                peer.observe_latest_binding = scenario == "scoped-resume"
                scoped_port = 0
                if scenario == "scoped-resume":
                    scoped = ChannelPeer(backend)
                    stack.callback(scoped.close)
                    scoped_port = scoped.port
                process = subprocess.Popen([str(executable), str(peer.port), str(root), scenario, str(scoped_port)],
                    cwd=root, env=environment, stdin=subprocess.PIPE, stdout=log, stderr=log, text=True)
                process.stdin.write(INVITE + "\n" + ROTATED + "\n")
                process.stdin.close()
                try:
                    if scenario.startswith("restart-"):
                        wait_marker(root, "restart-ready", process, time.monotonic() + 12)
                        peer.block_reconnect = True
                        handle["process"].kill()  # Unconfirmed process loss, never signed maintenance/End.
                        handle["process"].wait(5)
                        # The production readiness guard intentionally never
                        # unlinks an existing pathname, including after SIGKILL.
                        # Give the replacement process a distinct owned probe;
                        # preserve the actual same advertised/gameplay endpoint.
                        assert stat.S_ISSOCK(ready.stat().st_mode), "qualify stale owned control socket after SIGKILL"
                        wait_marker(root, "restart-reconnecting", process, time.monotonic() + 5)
                        password = INVITE if scenario == "restart-same" else ROTATED
                        stack.enter_context(service(server, root, password, backend_port=backend,
                                                    readiness_name=f"{scenario}-replacement-ready.sock"))
                        peer.block_reconnect = False
                    elif scenario == "pw-shutdown":
                        wait_marker(root, "shutdown-ready", process, time.monotonic() + 12)
                        handle["process"].terminate()  # Real service-owned maintenance notice.
                    process.wait(45)  # Harness failsafe only; individual original5s/12s budgets are in C++.
                finally:
                    if process.poll() is None:
                        process.kill(); process.wait(5)
                    log.seek(0)
                    output = log.read()
                    assert all(value not in output for value in (INVITE, ROTATED, OTHER, "incorrect-private-fixture")), "private Password disclosed in crypto fixture output"
                    print(output.strip(), flush=True)
                assert process.returncode == 0, f"crypto coverage {scenario} failed; no production correction/retry"
                assert peer.handshakes > 0, "common-channel fixture was never connected"


if __name__ == "__main__":
    main()
