#!/usr/bin/env python3
"""Packaged Linux application regressions; run only in an approved Docker runtime.

Uses actual graphical clients, SDL input, allowlisted rendered-text assertions,
and their real sibling services. No synthesized admission messages or test server.
The archive is read-only; all installations and backups are disposable copies.
See PackageDeploymentBehaviorTests.md for scope, prerequisites, and commands.
"""

import argparse
from contextlib import ExitStack
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import textwrap
import time
import uuid
import zipfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def owned_processes(root):
    result = []
    for executable in Path("/proc").glob("[0-9]*/exe"):
        try:
            target = executable.resolve(strict=True)
        except (FileNotFoundError, PermissionError):
            continue
        if target.parent == root and target.name in (
                "duel6r", "duel6r-server", "duel6r-host-supervisor", "duel6r-resolver"):
            result.append(executable.parent.name)
    return result


def install(archive, root):
    require(not root.exists(), "Installation must be an empty new directory")
    # Preserve the executable modes in the release zip, unlike extractall alone.
    with zipfile.ZipFile(archive) as bundle:
        for item in bundle.infolist():
            name = Path(item.filename)
            require(not name.is_absolute() and ".." not in name.parts,
                    "Unsafe archive path")
            bundle.extract(item, root)
            mode = item.external_attr >> 16
            if mode:
                (root / name).chmod(mode & 0o777)
    result = subprocess.run(["sha256sum", "-c", "linux-x86_64.sha256sums"],
                            cwd=root, capture_output=True, timeout=30)
    require(result.returncode == 0, "Pristine package inventory mismatch")


def local_files(root):
    return [root / "data/persons.json", root / "data/config.script"] + sorted(
        p for p in (root / "profiles").rglob("*") if p.is_file())


def backup(root, destination):
    require(not destination.exists(), "Never overwrite an existing backup")
    destination.mkdir()
    for path in local_files(root):
        target = destination / path.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)


def restore(root, saved):
    for path in local_files(saved):
        target = root / path.relative_to(saved)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    assert_data(root, saved)


def assert_data(root, saved):
    for path in local_files(saved):
        require(sha256(path) == sha256(root / path.relative_to(saved)),
                "Local data differs from its own backup: " + str(path.relative_to(saved)))


class Client:
    """One graphical process with a private display and bounded text assertions."""

    def __init__(self, root, scratch, display):
        self.root = root
        self.scratch = scratch
        scratch.mkdir(parents=True)
        self.request = scratch / "request.json"
        self.result = scratch / "result.json"
        self.request.write_text(json.dumps({"id": "idle", "predicates": []}))
        self.env = dict(os.environ, DISPLAY=display, SDL_AUDIODRIVER="dummy",
                        LIBGL_ALWAYS_SOFTWARE="1", D6R_TEXT_REQUEST=str(self.request),
                        D6R_TEXT_RESULT=str(self.result))
        self.display = subprocess.Popen(["Xvfb", display, "-screen", "0", "850x700x24",
                                         "+extension", "GLX", "+render", "-noreset"],
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.process = None
        try:
            time.sleep(1)
            require(self.display.poll() is None, "Private X display failed")
            self.process = subprocess.Popen([
                "gdb", "-q", "-nx", "-batch", "-x",
                str(Path(__file__).with_name("PackageRenderedTextObserver.py")),
                "--args", "./duel6r"], cwd=root, env=self.env,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.expect({"text": "PERSONS"}, timeout=20)
            self.window = self.x("search", "--onlyvisible", "--name", "Duel 6 Reloaded").splitlines()[0]
            self.x("windowfocus", self.window)
        except BaseException:
            self.close()
            raise

    def x(self, *args):
        return subprocess.check_output(["xdotool", *args], env=self.env,
                                       text=True, stderr=subprocess.DEVNULL, timeout=5).strip()

    def click(self, x, y):
        self.x("mousemove", str(x), str(700-y), "mousedown", "1", "sleep", "0.1", "mouseup", "1")
        time.sleep(.25)

    def key(self, key):
        self.x("key", key)
        time.sleep(.25)

    def text(self, text):
        self.x("type", "--clearmodifiers", "--delay", "30", text)
        time.sleep(.25)

    def expect(self, *predicates, timeout=15, invariant=None):
        identity = uuid.uuid4().hex
        temporary = self.request.with_suffix(".tmp")
        temporary.write_text(json.dumps({"id": identity, "predicates": list(predicates)}))
        temporary.replace(self.request)
        deadline = time.monotonic() + timeout
        last_check = 0
        while time.monotonic() < deadline:
            if invariant is not None and time.monotonic() - last_check >= .5:
                invariant()
                last_check = time.monotonic()
            if self.result.exists():
                result = json.loads(self.result.read_text())
                require("error" not in result, result.get("error", "Rendered-text observer failed"))
                if result.get("id") == identity:
                    require(result["matched"] == list(range(len(predicates))), "Incomplete UI assertion")
                    return
            require(self.process.poll() is None, "Packaged client/debugger exited before UI assertion")
            time.sleep(.05)
        raise AssertionError("Expected rendered text did not appear: " + repr(predicates))

    def add_people(self, prefix):
        for index in (1, 2):
            self.click(100, 298)
            self.text(prefix + str(index))
            self.click(294, 297)
        for y in (526, 508):
            self.click(90, y)
            self.click(300, 268)

    def network(self, host, address="127.0.0.1"):
        self.key("F2")
        self.expect({"text": "Host"}, {"text": "Join"})
        self.click(425, 340 if host else 295)
        if address != "127.0.0.1":
            target = ipaddress.IPv4Address(address)
            require(any(target in ipaddress.IPv4Network(network) for network in
                        ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")),
                    "Test endpoint must be RFC1918 IPv4")
            if host:
                # Isolated topology: exactly one private NIC plus loopback.
                interfaces = json.loads(subprocess.check_output(["ip", "-j", "-4", "addr"], text=True))
                private = [entry["local"] for interface in interfaces for entry in interface["addr_info"]
                           if not ipaddress.IPv4Address(entry["local"]).is_loopback]
                require(private == [address], "LAN host requires exactly the requested private NIC")
                self.click(200, 473)
                self.click(200, 429)
                self.expect({"contains": "Listening interface: " + address + " (Private LAN)"})
            else:
                self.click(200, 497)
                self.x("key", "--repeat", "9", "--delay", "80", "BackSpace")
                self.text(address)
                self.expect({"contains": "Address: " + address})
        self.click(425, 98)

    def lobby(self, ready=False, timeout=15):
        predicates = [{"contains": "2 participants • 4 players"}]
        predicates += [{"text": name} for name in ("QAHost1", "QAHost2", "QAGuest1", "QAGuest2")]
        # Readiness and ownership must be rendered together in a single frame.
        for y, role in ((456, "Host"), (438, "Guest 2")):
            predicates += [{"text": role, "x": 42, "y": y},
                           {"text": "Connected", "x": 142, "y": y},
                           {"text": "2", "x": 350, "y": y}]
            if ready is not None:
                predicates.append({"text": "Ready" if ready else "Not ready", "x": 254, "y": y})
        self.expect(*predicates, timeout=timeout)

    def quit_menu(self):
        self.key("Escape")
        require(self.process.wait(timeout=10) == 0, "Client did not exit cleanly")
        exit_file = self.result.with_suffix(".exit")
        require(exit_file.exists() and json.loads(exit_file.read_text())["exit_code"] == 0,
                "Packaged client inferior did not report exit status zero")

    def close(self):
        # Kill only our own debugger/inferior tree; never by a global executable name.
        if self.process is not None and self.process.poll() is None:
            subprocess.run(["pkill", "-TERM", "-P", str(self.process.pid)], check=False)
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        self.display.terminate()
        self.display.wait(timeout=5)
        deadline = time.monotonic() + 6
        while owned_processes(self.root) and time.monotonic() < deadline:
            time.sleep(.1)
        require(not owned_processes(self.root), "Owned application/service process survived cleanup")


def session(host_root, guest_root, scratch, seed=False):
    with ExitStack() as stack:
        host = Client(host_root, scratch / "host", ":91")
        stack.callback(host.close)
        guest = Client(guest_root, scratch / "guest", ":92")
        stack.callback(guest.close)
        if seed:
            host.add_people("QAHost")
            guest.add_people("QAGuest")
        host.network(True)
        host.expect({"contains": "1 participants • 2 players"})
        guest.network(False)
        host.lobby()
        guest.lobby()
        host.click(140, 178)
        guest.click(140, 178)
        host.lobby(ready=True)
        guest.lobby(ready=True)
        host.click(510, 64)
        host.expect({"contains": "Round 1/"})
        guest.expect({"contains": "Round 1/"})
        host.click(740, 100)
        host.click(225, 272)
        guest.expect({"text": "HOST ENDED SESSION"})
        host.expect({"text": "Host"}, {"text": "Join"})
        guest.click(425, 260)
        guest.expect({"text": "Host"}, {"text": "Join"})
        for client in (host, guest):
            client.key("Escape")
            client.expect({"text": "PERSONS"})
            client.quit_menu()
    # Timing is not asserted under debugger instrumentation. Listener release is.
    listeners = subprocess.check_output(["ss", "-Hltn", "sport", "=", ":26660"], text=True)
    require(not listeners.strip(), "Owned listener survived End session")


def local_round(root, scratch):
    # No non-loopback interface may be present in the offline test environment.
    interfaces = json.loads(subprocess.check_output(["ip", "-j", "link"], text=True))
    require(all(interface["ifname"] == "lo" for interface in interfaces),
            "Offline Local Play requires a container with only loopback")
    # Docker may retain its embedded DNS listener after detaching an internal
    # network. Assert no *new* listener, rather than blaming pre-existing OS/DNS
    # sockets on a client that has not started yet. Service processes are checked
    # independently below, including before any Local Play input.
    baseline_listeners = set(subprocess.check_output(["ss", "-Hltn"], text=True).splitlines())
    client = Client(root, scratch, ":91")
    samples = 0
    def no_service():
        nonlocal samples
        require(set(subprocess.check_output(["ss", "-Hltn"], text=True).splitlines()) == baseline_listeners,
                "TCP listeners changed during offline Local Play")
        for executable in Path("/proc").glob("[0-9]*/exe"):
            try:
                target = executable.resolve(strict=True)
            except (FileNotFoundError, PermissionError):
                continue
            require(target.name not in ("duel6r-server", "duel6r-host-supervisor", "duel6r-resolver"),
                    "Local Play started a network service component")
        samples += 1
    try:
        no_service()
        client.add_people("QAHost")
        client.click(804, 414)
        client.text("1")
        client.key("Return")
        client.key("F1")
        # Stock arenas and gameplay, no fixture replacing shipped levels/scripts.
        client.expect({"text": "End of Game"}, timeout=240, invariant=no_service)
        no_service()
        # Final Local Play summary exits on Escape after its six-second hold.
        time.sleep(7.5)
        client.key("Escape")
        client.expect({"text": "PERSONS"})
        client.quit_menu()
    finally:
        client.close()
    data = json.loads((root / "data/persons.json").read_text())
    require(data["rounds"] == 1, "Local Play did not complete its one-round match")
    require(all(person["games"] == 1 and person["eloGames"] == 1 for person in data["persons"]),
            "Completed Local Play did not persist game/Elo counts")
    # Simultaneous deaths may legitimately produce no winner on a stock arena.
    require(sum(person["wins"] for person in data["persons"]) <= 1
            and sum(person["deaths"] for person in data["persons"]) >= 1,
            "Completed Local Play result is inconsistent")
    print("Local Play no-service samples:", samples, flush=True)


def rejected_session(host_root, guest_root, scratch, invalid=False, check_return=False):
    config = guest_root / "data/config.script"
    if invalid:
        blocks = guest_root / "data/blocks.json"
        blocks.rename(guest_root / "data/saved-blocks.json")
        blocks.symlink_to("saved-blocks.json")
        message = "Local gameplay content is invalid. Restore the supported gameplay content and restart the application."
    else:
        config.write_text(config.read_text().replace("volume      128", "volume      64"))
        message = "Gameplay content mismatch. Use the host's exact supported gameplay content."
    original_config = sha256(config)
    with ExitStack() as stack:
        host = Client(host_root, scratch / "host", ":91")
        stack.callback(host.close)
        guest = Client(guest_root, scratch / "guest", ":92")
        stack.callback(guest.close)
        host.add_people("QAHost")
        guest.add_people("QAGuest")
        host.network(True)
        host.expect({"contains": "1 participants • 2 players"})
        guest.network(False)
        guest.expect(*[{"text": line} for line in textwrap.wrap(message, width=72)])
        if invalid:
            guest.expect({"text": "Retry unavailable"})
        host.expect({"contains": "1 participants • 2 players"})
        if check_return:
            # Regression: Return must not dispatch the Edit setup action.
            guest.click(685, 285)
            guest.expect({"text": "Host"}, {"text": "Join"})
        else:
            # Independently exercise the supported Edit setup -> Back path.
            guest.click(425, 285)
            guest.expect({"text": "Connect"})
            guest.click(425, 54)
            guest.expect({"text": "Host"}, {"text": "Join"})
        host.click(740, 64)
        host.click(225, 272)
        host.expect({"text": "Host"}, {"text": "Join"})
        for client in (host, guest):
            client.key("Escape")
            client.expect({"text": "PERSONS"})
            client.quit_menu()
    require(sha256(config) == original_config, "Admission rewrote guest configuration")
    require(not subprocess.check_output(["ss", "-Hltn", "sport", "=", ":26660"], text=True).strip(),
            "Negative scenario leaked its owned listener")


def lan_participant(root, scratch, host, address, peer_ready_file=None):
    def barrier(path):
        deadline = time.monotonic() + 60
        while not path.exists() and time.monotonic() < deadline:
            time.sleep(.1)
        require(path.exists(), "Independent guest observation was not acknowledged")
        path.unlink()
    if host:
        require(peer_ready_file is not None and not peer_ready_file.exists()
                and not peer_ready_file.with_suffix(".match").exists(),
                "LAN host requires a fresh --peer-ready-file for driver synchronization")
    client = Client(root, scratch, ":91")
    try:
        client.add_people("QAHost" if host else "QAGuest")
        client.network(host, address)
        if host:
            client.expect({"contains": "1 participants • 2 players"})
            print("LAN_HOST_READY", flush=True)
        client.lobby(ready=None, timeout=60)
        client.click(140, 178)
        client.lobby(ready=True, timeout=30)
        print("LAN_PARTICIPANT_READY", "host" if host else "guest", flush=True)
        if host:
            barrier(peer_ready_file)
            client.click(510, 64)
        client.expect({"contains": "Round 1/"}, timeout=70)
        print("LAN_PARTICIPANT_MATCH", "host" if host else "guest", flush=True)
        if host:
            barrier(peer_ready_file.with_suffix(".match"))
            client.click(740, 100)
            client.click(225, 272)
            client.expect({"text": "Host"}, {"text": "Join"})
        else:
            client.expect({"text": "HOST ENDED SESSION"}, timeout=70)
            client.click(425, 260)
            client.expect({"text": "Host"}, {"text": "Join"})
        client.key("Escape")
        client.expect({"text": "PERSONS"})
        client.quit_menu()
    finally:
        client.close()
    require(not subprocess.check_output(["ss", "-Hltn", "sport", "=", ":26660"], text=True).strip(),
            "LAN participant retained its listener")
    print("PASS: virtual LAN " + ("host" if host else "guest") + " rendered admission/readiness/gameplay/cleanup", flush=True)


def cancel_connection(root, scratch, address):
    require(address != "127.0.0.1", "Cancellation needs an unused peer on the isolated test LAN")
    client = Client(root, scratch, ":91")
    try:
        client.add_people("QAGuest")
        client.network(False, address)
        client.expect({"text": "Cancel"})
        client.click(425, 54)
        client.expect({"text": "Connect"})
        client.click(425, 54)
        client.expect({"text": "Host"}, {"text": "Join"})
        client.key("Escape")
        client.expect({"text": "PERSONS"})
        client.quit_menu()
    finally:
        client.close()
    print("PASS: pending guest connection Cancel returned to editable setup and cleaned up", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--scenario", choices=("session", "replacement", "local", "mismatch", "invalid", "failure-return", "lan-host", "lan-guest", "cancel"), default="session")
    parser.add_argument("--address", default="127.0.0.1", help="Host's explicit RFC1918 interface for isolated LAN tests")
    parser.add_argument("--peer-ready-file", type=Path,
                        help="LAN host barrier: controller creates this only after guest reports rendered Ready")
    args = parser.parse_args()
    require(sha256(args.archive) == args.sha256, "Archive digest does not match checkpoint handoff")
    with tempfile.TemporaryDirectory(prefix="duel6r-package-behavior-") as directory:
        work = Path(directory)
        host, guest = work / "host", work / "guest"
        install(args.archive, host)
        install(args.archive, guest)
        if args.scenario == "cancel":
            cancel_connection(host, work / "cancel", args.address)
            require(sha256(args.archive) == args.sha256, "Source archive changed during verification")
            return
        if args.scenario.startswith("lan-"):
            require(args.address != "127.0.0.1", "LAN coverage must not silently use loopback")
            lan_participant(host, work / "lan", args.scenario == "lan-host", args.address, args.peer_ready_file)
            require(sha256(args.archive) == args.sha256, "Source archive changed during verification")
            return
        if args.scenario in ("local", "replacement"):
            local_round(host, work / "local")
            print("PASS: offline packaged Local Play rendered final summary and saved real statistics", flush=True)
        if args.scenario == "local":
            require(sha256(args.archive) == args.sha256, "Source archive changed during verification")
            return
        if args.scenario in ("mismatch", "invalid", "failure-return"):
            rejected_session(host, guest, work / "negative", invalid=args.scenario == "invalid",
                             check_return=args.scenario == "failure-return")
            print("PASS: " + args.scenario + " exact rendered failure, host isolation and cleanup", flush=True)
            require(sha256(args.archive) == args.sha256, "Source archive changed during verification")
            return
        session(host, guest, work / "initial", seed=True)
        print("PASS: rendered admitted identities, ownership, readiness, gameplay, End and cleanup", flush=True)
        if args.scenario == "replacement":
            saved_archive = work / "prior.zip"
            shutil.copy2(args.archive, saved_archive)
            for root in (host, guest):
                config = root / "data/config.script"
                config.write_text(config.read_text().replace("volume      128", "volume      64"))
                shutil.copytree(root / "profiles/sample", root / "profiles/QALocalProfile")
                backup(root, work / (root.name + "-backup"))
            for phase in ("reinstall", "rollback"):
                for root in (host, guest):
                    root.rename(work / (root.name + "-" + phase + "-retired"))
                    install(args.archive if phase == "reinstall" else saved_archive, root)
                    restore(root, work / (root.name + "-backup"))
                session(host, guest, work / phase)
                for root in (host, guest):
                    assert_data(root, work / (root.name + "-backup"))
                print("PASS: " + phase + " own backup data and new production session", flush=True)
                if phase == "reinstall":
                    # Rollback must not use data changed after reinstallation.
                    for root in (host, guest):
                        config = root / "data/config.script"
                        config.write_text(config.read_text().replace("volume      64", "volume      32"))
                        shutil.rmtree(root / "profiles/QALocalProfile")
    require(sha256(args.archive) == args.sha256, "Source archive changed during verification")


if __name__ == "__main__":
    main()
