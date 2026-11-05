"""Finite native input recipes, executed locally without agent roundtrip latency."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import time

from .common import record, require, verify_runtime
from .recognition import Canvas, STATES, recognize
from .recording import Recording, validate_frame_limit
from .input_guard import hold_input

# Reuse the repository's existing process owner, without altering its contract.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))
from OwnedProcessRunner import run_owned


def seed_names(root, names):
    """Only names are variable. All counters/default Elo mirror SharedArenaBehaviorTests."""
    require(1 <= len(names) <= 15 and len(set(names)) == len(names), "invalid name-only roster")
    require(all(isinstance(n, str) and n and len(n.encode("utf-8")) <= 64
                and not any(ord(c) < 32 for c in n) for n in names), "invalid player name")
    counters = dict.fromkeys(("shots", "hits", "kills", "deaths", "assistances", "wins", "penalties",
                             "games", "timeAlive", "totalGameTime", "totalDamage", "assistedDamage",
                             "eloTrend", "eloGames"), 0)
    counters["elo"] = 1000
    (root / "data/persons.json").write_text(json.dumps({
        "persons": [dict(counters, name=n) for n in names], "playing": names, "rounds": 0,
    }), encoding="utf-8")


def expand(steps, macros=None, stack=()):
    """User-supplied setup macros are finite data, never shell/code or gameplay policies."""
    macros = macros or {}
    require(isinstance(steps, list) and len(steps) <= 500, "too many recipe steps")
    result = []
    for step in steps:
        require(isinstance(step, dict), "invalid step")
        if "macro" in step:
            name = step["macro"]
            require(set(step) == {"macro"} and name in macros and name not in stack
                    and len(stack) < 8, "unknown or recursive macro")
            result.extend(expand(macros[name], macros, stack + (name,)))
        else:
            result.append(step)
        require(len(result) <= 500, "expanded recipe exceeds 500 actions")
    return result


class Native:
    def __init__(self, display, window, canvas):
        self.env = dict(os.environ, DISPLAY=display)
        self.window, self.canvas = str(window), canvas

    def command(self, *args, timeout=5):
        return subprocess.check_output(args, env=self.env, stderr=subprocess.PIPE, timeout=timeout)

    def prepare_input(self, deadline):
        remaining = deadline - time.monotonic()
        require(remaining > 0, "native focus deadline expired")
        self.command("xdotool", "windowfocus", self.window, timeout=remaining)

    def input(self, action, value, *, deadline):
        remaining = deadline - time.monotonic()
        require(remaining > 0, "native input deadline expired")
        # XTEST on this private display: release must not target a Window that
        # Escape may already have destroyed. Do not suppress other X11 errors.
        try:
            self.command("xdotool", action, value, timeout=remaining)
        except subprocess.TimeoutExpired as error:
            raise ValueError("native press acknowledgement deadline exceeded") from error

    def release_all(self, releases, *, deadline):
        processes, errors = [], []
        # A delayed acknowledgement for one release cannot block dispatch of
        # modifier/button releases. All children remain in the owned process tree.
        for action, value in releases:
            try:
                processes.append(subprocess.Popen(["xdotool", action, value], env=self.env,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE))
            except OSError as error:
                errors.append(str(error))
        for process in processes:
            try:
                _, stderr = process.communicate(timeout=max(0, deadline - time.monotonic()))
                if process.returncode:
                    errors.append(stderr.decode(errors="replace")[-500:] or "native release command failed")
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                errors.append("native release acknowledgement deadline exceeded")
        return errors

    def point(self, x, y):
        self.command("xdotool", "mousemove", "--window", self.window, str(x), str(y))

    def verify_geometry(self):
        display = self.command("xdotool", "getdisplaygeometry").decode().split()
        require(display == [str(self.canvas.width), str(self.canvas.height)],
                "private display geometry changed")
        geometry = self.command("xdotool", "getwindowgeometry", "--shell", self.window).decode()
        geometry = dict(line.split("=", 1) for line in geometry.splitlines() if "=" in line)
        require([geometry.get(key) for key in ("X", "Y", "WIDTH", "HEIGHT")]
                == ["0", "0", str(self.canvas.width), str(self.canvas.height)],
                "client must fill its private display at native origin")

    def frame(self):
        # One capture, not a reread of a stale file. No normalization of evidence.
        self.verify_geometry()
        png = self.command("import", "-window", self.window, "png:-", timeout=3)
        dimensions = subprocess.check_output(["identify", "-format", "%w %h", "png:-"], input=png, timeout=3)
        require(dimensions.decode().split() == [str(self.canvas.width), str(self.canvas.height)],
                "display, client and capture geometry disagree")
        rgb = subprocess.check_output(["convert", "png:-", "-alpha", "off", "-depth", "8", "rgb:-"],
                                      input=png, timeout=3)
        return png, rgb


class Scheduler:
    def __init__(self, clients, output, budget, states=None, clock=time.monotonic, sleep=time.sleep):
        require(math.isfinite(budget) and 0 < budget <= 600, "invalid recipe budget")
        self.clients, self.output = clients, Path(output)
        self.clock, self.sleep = clock, sleep
        self.started = clock()
        self.deadline = self.started + budget
        self.states = dict(STATES, **(states or {}))
        require(not set(states or {}) & set(STATES), "built-in route anchors cannot be overridden")
        self.events = []
        self.recordings = {}

    def remaining(self):
        remaining = self.deadline - self.clock()
        require(remaining > 0, "recipe deadline expired")
        return remaining

    def wait(self, seconds):
        require(math.isfinite(seconds) and 0 <= seconds <= self.remaining(), "wait exceeds recipe deadline")
        self.sleep(seconds)
        self.remaining()

    def run(self, steps, *, observe=None):
        try:
            for ordinal, step in enumerate(steps):
                self.remaining()
                if "at" in step:
                    at = step["at"]
                    require(isinstance(at, (int, float)) and math.isfinite(at) and at >= 0,
                            "invalid scheduled time")
                    target = self.started + at
                    tolerance = step.get("late_tolerance", .1)
                    require(math.isfinite(tolerance) and 0 <= tolerance <= 2, "invalid schedule tolerance")
                    require(self.clock() <= target + tolerance, "missed scheduled action")
                    self.wait(max(0, target - self.clock()))
                event = {"ordinal": ordinal, "action": step["action"],
                         "started": self.clock() - self.started}
                self.events.append(event)
                client = self.clients[step["client"]]
                try:
                    self.action(client, step, ordinal)
                    event["finished"] = self.clock() - self.started
                finally:
                    if observe is not None:
                        active_error = sys.exc_info()[0] is not None
                        try:
                            observe(step, event)
                        except Exception:
                            if not active_error:
                                raise
                            print("operator observation failed during action abort", file=sys.stderr)
        finally:
            active_error = sys.exc_info()[0] is not None
            errors = {}
            # One failed encoder must not prevent the others from finalizing.
            # The original action exception also must not be hidden by teardown.
            for name, recording in self.recordings.items():
                try:
                    recording.stop()
                except Exception as error:
                    errors[name] = str(error)
            record(self.output / "actions.json", {"started_monotonic": self.started,
                   "events": self.events, "recording_errors": errors})
            if errors and not active_error:
                raise ValueError("recorder finalization failed; see additive actions receipt")

    def action(self, client, step, ordinal):
        action = step["action"]
        if action == "record":
            clip = step["clip"]
            require(re.fullmatch(r"[a-z][a-z0-9-]{0,39}", clip) and clip not in self.recordings,
                    "invalid/duplicate original clip ID")
            seconds = step["seconds"]
            require(math.isfinite(seconds) and 0 < seconds <= self.remaining(), "recording exceeds budget")
            if "frames" in step:
                validate_frame_limit(seconds, step["frames"])
            self.recordings[clip] = Recording(client, self.output / (clip + ".mp4"), seconds,
                                             **({"frames": step["frames"]} if "frames" in step else {}))
            require(self.recordings[clip].ready.wait(min(5, self.remaining())),
                    "recorder produced no first frame before readiness deadline")
            require(self.recordings[clip].process.poll() is None,
                    "recording ended before action readiness; no subsequent coverage")
            self.remaining()
        elif action == "stop_record":
            self.recordings[step["clip"]].request_stop()
        elif action in ("key", "click"):
            hold = step.get("hold", .08)
            require(isinstance(hold, (int, float)) and math.isfinite(hold) and 0 < hold <= 2,
                    "each input hold must be positive and <=2s")
            require(hold < self.remaining(), "input exceeds recipe deadline")
            if action == "click":
                x, y = step["point"]
                space = step.get("space", "menu")
                if space in ("menu", "network-menu"):
                    canvas = Canvas(client.canvas.width, client.canvas.height, space == "network-menu")
                    x, y = canvas.point(x, y)
                else:
                    require(step["space"] == "client" and 0 <= x < client.canvas.width
                            and 0 <= y < client.canvas.height, "invalid client point")
                client.point(x, y)
                down, up, value = "mousedown", "mouseup", "1"
            else:
                value = step["key"]
                require(isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9_]+", value),
                        "one explicit keysym; list modifiers separately")
                down, up = "keydown", "keyup"
            modifiers = step.get("modifiers", [])
            require(isinstance(modifiers, list) and len(modifiers) <= 3
                    and len(set(modifiers)) == len(modifiers)
                    and all(key in ("Shift_L", "Control_L", "Alt_L") for key in modifiers),
                    "invalid explicit modifiers")
            timing = {}
            try:
                hold_input(client, down, up, value, modifiers, hold, self.deadline,
                           self.clock, self.wait, timing)
            finally:
                self.events[-1]["input"] = timing
        elif action == "text":
            value = step["text"]
            require(isinstance(value, str) and len(value.encode()) <= 128
                    and not any(ord(c) < 32 for c in value), "invalid literal input text")
            client.command("xdotool", "type", "--window", client.window, "--clearmodifiers",
                           "--delay", "5", "--", value, timeout=min(3, self.remaining()))
        elif action == "wait":
            self.wait(step["seconds"])
        elif action == "exit_wait":
            # Observe normal closure without a blind fixed sleep or a new budget.
            timeout = step.get("timeout", 5)
            require(math.isfinite(timeout) and 0 < timeout <= 10, "invalid exit deadline")
            end = min(self.deadline, self.clock() + timeout)
            while self.clock() < end:
                if client.process.poll() is not None:
                    require(client.process.returncode == 0, "client exited abnormally")
                    return
                self.sleep(.02)
            raise ValueError("normal client exit deadline")
        elif action == "await":
            timeout = step.get("timeout", 5)
            require(math.isfinite(timeout) and 0 < timeout <= 10, "invalid state deadline")
            deadline = min(self.deadline, self.clock() + timeout)
            anchors = self.states[step["state"]]
            while self.clock() < deadline:
                png, rgb = client.frame()
                passed, observations = recognize(rgb, client.canvas, anchors)
                self.events[-1]["recognition"] = observations
                self.remaining()
                require(self.clock() <= deadline, "recognition completed after state deadline")
                if passed:
                    return
                self.sleep(min(.05, max(0, deadline - self.clock())))
            raise ValueError("state not recognized before deadline: " + step["state"])
        elif action == "capture":
            png, _ = client.frame()
            self.remaining()
            with (self.output / f"{ordinal:04d}-{step['client']}.png").open("xb") as stream:
                stream.write(png)
        else:
            raise ValueError("unknown finite action: " + str(action))


def probe_display_geometry(display, xvfb_pid, canvas, env):
    """One strict startup probe; preserve bounded context if it fails."""
    error = None
    try:
        probe = subprocess.run(["xdotool", "getdisplaygeometry"], env=env,
                               capture_output=True, timeout=1)
        if (probe.returncode == 0 and probe.stdout.decode(errors="replace").split()
                == [str(canvas.width), str(canvas.height)]):
            return
        code, stdout, stderr = probe.returncode, probe.stdout, probe.stderr
        failure = "geometry-or-command"
    except subprocess.TimeoutExpired as cause:
        error = cause
        code, stdout, stderr = None, cause.stdout or b"", cause.stderr or b""
        failure = "timeout"
    except OSError as cause:
        error = cause
        code, stdout, stderr = None, b"", b""
        failure = "launch-errno-" + str(cause.errno)
    details = {"display": display, "xvfb_pid": xvfb_pid,
               "expected": f"{canvas.width}x{canvas.height}", "probe_rc": code,
               "probe_stdout": stdout[:160].decode(errors="replace"),
               "probe_stderr": stderr[:160].decode(errors="replace"), "failure": failure}
    raise ValueError("unexpected display geometry: " + json.dumps(details, sort_keys=True)) from error


def execute(runtime, identity, recipe, output, *, operator=False):
    """Worker runs under OwnedProcessRunner; all processes are its descendants."""
    runtime = verify_runtime(runtime, identity)
    clients, processes = {}, []
    require(1 <= len(recipe["clients"]) <= 3, "expected 1–3 private clients")
    try:
        for index, item in enumerate(recipe["clients"]):
            name = item["id"]
            require(re.fullmatch(r"[a-z][a-z0-9-]{0,31}", name) and name not in clients, "invalid client ID")
            canvas = Canvas(*item["geometry"])
            root = output / name
            shutil.copytree(runtime, root)
            verify_runtime(root, identity)
            seed_names(root, item["names"])
            env = dict(PATH=os.environ["PATH"], LANG="C.UTF-8",
                       SDL_AUDIODRIVER="dummy", LIBGL_ALWAYS_SOFTWARE="1", HOME=str(output / (name + "-home")))
            Path(env["HOME"]).mkdir()
            for key in ("XDG_CACHE_HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME"):
                env[key] = env["HOME"] + "/" + key.lower()
            log = (output / (name + ".log")).open("xb")
            try:
                # Xvfb chooses an unused display and acknowledges its own startup.
                # Never treat a response from someone else's :91 as our readiness.
                reader, writer = os.pipe()
                try:
                    try:
                        xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(writer), "-screen", "0",
                            f"{canvas.width}x{canvas.height}x24", "+extension", "GLX", "+render", "-noreset"],
                            env=env, stdout=log, stderr=log, pass_fds=(writer,))
                        processes.append(xvfb)
                    finally:
                        os.close(writer)
                    require(select.select([reader], [], [], 5)[0], "private display startup deadline")
                    number = os.read(reader, 32).decode().strip()
                    require(number.isdecimal() and xvfb.poll() is None, "private display startup failed")
                    display = ":" + number
                    env["DISPLAY"] = display
                finally:
                    os.close(reader)
                probe_display_geometry(display, xvfb.pid, canvas, env)
                app = subprocess.Popen([str(root / "duel6r")], cwd=root, env=env, stdout=log, stderr=log)
                processes.append(app)
                end = time.monotonic() + 15
                while True:
                    require(app.poll() is None, "client exited before its window")
                    probe = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)],
                                           env=env, capture_output=True, timeout=1)
                    if probe.returncode == 0:
                        windows = probe.stdout.decode().split()
                        require(len(windows) == 1, "ambiguous owned client window")
                        break
                    require(time.monotonic() < end, "owned client window deadline")
                    time.sleep(.05)
                client = Native(display, windows[0], canvas)
                client.process = app
                client.command("xdotool", "windowfocus", client.window)
                clients[name] = client
            finally:
                log.close()
        schedule = Scheduler(clients, output, recipe["budget_seconds"], recipe.get("states"))
        steps = expand(recipe["steps"], recipe.get("macros"))
        if operator:
            from .operator import OperatorChannel
            with OperatorChannel(schedule, runtime, identity, recipe) as channel:
                schedule.run(channel.steps(steps), observe=channel.observe)
        else:
            schedule.run(steps)
    finally:
        # Recipes must exit normally themselves; fallback is independently reaped.
        normal = (len(processes) == len(recipe["clients"]) * 2
                  and all(process.poll() == 0 for process in processes[1::2]))
        wait_errors = []
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
        for process in reversed(processes):
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                wait_errors.append("owned process did not exit; outer process owner must finish cleanup")
        recipe_hash = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        record(output / "controller.json", {"runtime": str(runtime), "identity": identity,
               "recipe_sha256": recipe_hash, "normal_application_exit": normal,
               "wait_errors": wait_errors,
               "clients": {name: {"display": client.env["DISPLAY"], "window": client.window,
                                  "pid": client.process.pid,
                                  "geometry": [client.canvas.width, client.canvas.height]}
                           for name, client in clients.items()},
               "note": "new capture/tool receipt, not a reconstructed historical receipt"})
        verify_runtime(runtime, identity)
    require(normal and not wait_errors,
            "recipe did not close normally; fallback cleanup is not a normal pass")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--identity", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--operator", action="store_true", help="accept finite operator batches in output/operator")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    identity, recipe = json.loads(args.identity.read_text()), json.loads(args.recipe.read_text())
    if args.worker:
        execute(args.runtime, identity, recipe, args.output.resolve(), operator=args.operator)
        return
    root = verify_runtime(args.runtime, identity)
    require(not args.output.resolve().is_relative_to(root), "output must be outside immutable runtime")
    steps = expand(recipe["steps"], recipe.get("macros"))
    require(args.operator or len(steps) > 0, "empty recipe")
    for step in steps:
        if step.get("action") == "record" and "frames" in step:
            validate_frame_limit(step["seconds"], step["frames"])
    args.output.mkdir(parents=True, exist_ok=False)
    budget = recipe["budget_seconds"]
    require(math.isfinite(budget) and 0 < budget <= 600, "invalid recipe budget")
    command = [sys.executable, "-m", "tools.native_visual.controller", "--runtime", str(args.runtime.resolve()),
               "--identity", str(args.identity.resolve()), "--recipe", str(args.recipe.resolve()),
               "--output", str(args.output.resolve()), "--worker"]
    if args.operator:
        command.append("--operator")
    result = run_owned(command, budget + 90, args.output / "process.jsonl")
    verify_runtime(args.runtime, identity)
    normal = (result["cleanup"] and result["phase"] == "exited" and result["exit"] == 0
              and len(result["reaped"]) == 1 and not result.get("forced", False))
    record(args.output / "completion.json", {"normal_cleanup": normal, "ownership": result})
    require(normal,
            "controller failed or required fallback; see preserved receipts")


if __name__ == "__main__":
    main()
