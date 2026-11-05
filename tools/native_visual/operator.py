"""Private, finite JSON mailbox. No gameplay policy, process attachment or commands."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
import time

from .common import record, require, sha, verify_runtime
from .recognition import Canvas, recognize
from .recording import validate_frame_limit

MAX_REQUEST_BYTES = 256 * 1024


def load_request(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate JSON field")
            result[key] = value
        return result
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK), "rb") as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode), "request must be a regular file")
        data = stream.read(MAX_REQUEST_BYTES + 1)
    require(len(data) <= MAX_REQUEST_BYTES, "operator request too large")
    return json.loads(data, object_pairs_hook=unique,
                      parse_constant=lambda _: require(False, "nonfinite JSON number"))


def number(value, low, high):
    require(type(value) in (int, float) and math.isfinite(value) and low <= value <= high,
            "invalid bounded action number")


def validate_batch(schedule, steps):
    """Preflight every field before dispatch; execution still uses Scheduler.action."""
    fields = {"key": ({"key"}, {"hold", "modifiers"}),
              "click": ({"point"}, {"space", "hold", "modifiers"}),
              "text": ({"text"}, set()), "wait": ({"seconds"}, set()),
              "await": ({"state"}, {"timeout"}), "capture": (set(), set()),
              "record": ({"clip", "seconds"}, {"frames"}),
              "stop_record": ({"clip"}, set()), "exit_wait": (set(), {"timeout"})}
    clips, checked_states = set(schedule.recordings), set()
    remaining = schedule.remaining()
    for step in steps:
        require(isinstance(step, dict) and isinstance(step.get("action"), str)
                and step["action"] in fields, "unknown operator action")
        required, optional = fields[step["action"]]
        require(required | {"action", "client"} <= step.keys()
                and step.keys() <= required | optional | {"action", "client", "at", "late_tolerance"},
                "missing or unsupported action field")
        require(isinstance(step["client"], str) and step["client"] in schedule.clients, "unknown owned client")
        client = schedule.clients[step["client"]]
        if "at" in step:
            number(step["at"], 0, schedule.deadline - schedule.started)
            number(step.get("late_tolerance", .1), 0, 2)
            require(schedule.clock() <= schedule.started + step["at"] + step.get("late_tolerance", .1),
                    "missed global scheduled action")
        else:
            require("late_tolerance" not in step, "late tolerance requires global at")
        action = step["action"]
        if action in ("key", "click"):
            number(step.get("hold", .08), 0, 2)
            require(0 < step.get("hold", .08) < remaining, "input exceeds remaining budget")
            modifiers = step.get("modifiers", [])
            require(isinstance(modifiers, list) and len(modifiers) <= 3
                    and all(isinstance(k, str) and k in ("Shift_L", "Control_L", "Alt_L") for k in modifiers)
                    and len(set(modifiers)) == len(modifiers), "invalid explicit modifiers")
            if action == "key":
                require(isinstance(step["key"], str) and re.fullmatch(r"[A-Za-z0-9_]+", step["key"]),
                        "one explicit keysym required")
            else:
                point = step["point"]
                require(isinstance(point, list) and len(point) == 2, "invalid point")
                x, y = point
                number(x, 0, client.canvas.width if step.get("space") == "client" else 850)
                number(y, 0, client.canvas.height if step.get("space") == "client" else 700)
                space = step.get("space", "menu")
                if space in ("menu", "network-menu"):
                    Canvas(client.canvas.width, client.canvas.height, space == "network-menu").point(x, y)
                else:
                    require(space == "client" and x < client.canvas.width and y < client.canvas.height,
                            "invalid client point")
        elif action == "text":
            value = step["text"]
            require(isinstance(value, str) and len(value.encode()) <= 128
                    and not any(ord(c) < 32 for c in value), "invalid literal input text")
        elif action in ("wait", "record"):
            number(step["seconds"], 0, remaining)
            if action == "record":
                require(step["seconds"] > 0 and isinstance(step["clip"], str)
                        and re.fullmatch(r"[a-z][a-z0-9-]{0,39}", step["clip"])
                        and step["clip"] not in clips, "invalid or duplicate clip")
                clips.add(step["clip"])
                if "frames" in step:
                    validate_frame_limit(step["seconds"], step["frames"])
        elif action == "stop_record":
            require(isinstance(step["clip"], str) and step["clip"] in clips, "unknown recording")
        elif action in ("await", "exit_wait"):
            number(step.get("timeout", 5), 0, 10)
            require(step.get("timeout", 5) > 0, "invalid state/exit timeout")
            if action == "await":
                state = step["state"]
                require(isinstance(state, str) and state in schedule.states, "unknown route state")
                key = (step["client"], state)
                if key not in checked_states:
                    # Reuse the existing anchor validator, not a new outcome detector.
                    recognize(bytes(client.canvas.width * client.canvas.height * 3), client.canvas, schedule.states[state])
                    checked_states.add(key)
    schedule.remaining()


class OperatorChannel:
    def __init__(self, schedule, runtime, identity, recipe):
        self.schedule, self.runtime, self.identity, self.recipe = schedule, runtime, identity, recipe
        self.root = schedule.output / "operator"
        self.total, self.active, self.exited = 0, None, set()

    def write(self, path, value):
        with tempfile.TemporaryDirectory(dir=self.root, prefix=".receipt-") as tmp:
            temporary = Path(tmp) / "receipt.json"
            record(temporary, value)
            with temporary.open("rb") as stream:
                os.fsync(stream.fileno())
            os.link(temporary, path)  # Atomic, complete and never overwrites a receipt.

    def emit(self, kind, **value):
        self.log.write(json.dumps(dict(kind=kind, elapsed=self.schedule.clock() - self.schedule.started,
                                      remaining=max(0, self.schedule.deadline - self.schedule.clock()), **value)) + "\n")
        self.log.flush()
        os.fsync(self.log.fileno())

    def __enter__(self):
        self.root.mkdir(mode=0o700)
        self.log = (self.root / "events.jsonl").open("x")
        try:
            clients = {name: dict(pid=c.process.pid, window=c.window, display=c.env["DISPLAY"],
                                 geometry=[c.canvas.width, c.canvas.height]) for name, c in self.schedule.clients.items()}
            context = dict(identity=self.identity, recipe=self.recipe, clients=clients, started=self.schedule.started)
            self.context = hashlib.sha256(json.dumps(context, sort_keys=True).encode()).hexdigest()
            self.write(self.root / "ready.json", dict(context=self.context, clients=clients,
                       started_monotonic=self.schedule.started, deadline_monotonic=self.schedule.deadline,
                       maximum_actions=500, note="input channel only; not overall normal cleanup"))
        except BaseException:
            self.log.close()
            raise
        return self

    def __exit__(self, kind, error, traceback):
        try:
            if self.active is not None:
                self.receipt(self.active, False)
            self.emit("input_closed", failed=kind is not None, overall_normal_cleanup=False)
        except Exception:
            if kind is None:
                raise
            print("operator receipt write failed during abort", file=sys.stderr)
        finally:
            self.log.close()

    def receipt(self, batch, passed):
        self.write(self.root / f"{batch:04d}.receipt.json", dict(batch=batch, actions_completed=passed,
                   elapsed=self.schedule.clock() - self.schedule.started, actions_reserved=self.total,
                   next_ordinal=len(self.schedule.events), overall_normal_cleanup=False,
                   recordings={name: dict(encoder_progress=dict(r.counters), encoder_exit=r.process.poll())
                               for name, r in self.schedule.recordings.items()}))
        self.emit("batch_completed" if passed else "batch_failed", batch=batch)
        self.active = None

    def batch(self, ordinal, steps):
        self.active = ordinal
        require(steps or ordinal == 0, "empty operator batch")
        require(self.total + len(steps) <= 500, "global 500-action limit exceeded")
        validate_batch(self.schedule, steps)
        self.total += len(steps)
        self.emit("batch_accepted", batch=ordinal, actions=len(steps), first_ordinal=len(self.schedule.events))
        yield from steps
        self.receipt(ordinal, True)

    def steps(self, initial):
        from .controller import expand
        yield from self.batch(0, initial)
        for batch in range(1, 502):
            path = self.root / f"{batch:04d}.request.json"
            self.emit("awaiting_batch", batch=batch)
            while not path.exists() and not path.is_symlink():
                time.sleep(min(.05, self.schedule.remaining()))
            self.schedule.remaining()
            self.active = batch
            request = load_request(path)
            require(isinstance(request, dict) and request.get("context") == self.context, "stale operator context")
            verify_runtime(self.runtime, self.identity)
            self.schedule.remaining()
            if set(request) == {"context", "finish"} and request["finish"] is True:
                require(self.exited == set(self.schedule.clients)
                        and all(c.process.poll() == 0 for c in self.schedule.clients.values()),
                        "finish requires successful exit_wait for every owned client")
                self.receipt(batch, True)
                return
            if set(request) == {"context", "cancel"} and request["cancel"] is True:
                raise ValueError("operator cancelled")
            require(set(request) == {"context", "steps"}, "operator cancelled or unsupported request")
            yield from self.batch(batch, expand(request["steps"], self.recipe.get("macros")))
        raise ValueError("operator batch limit exceeded")

    def observe(self, step, event):
        if step["action"] == "exit_wait" and "finished" in event:
            self.exited.add(step["client"])
        details = {k: v for k, v in step.items() if k != "text"}
        if "text" in step:
            details["text_redacted"] = True
        if step["action"] == "capture" and "finished" in event:
            details["png"] = f"{event['ordinal']:04d}-{step['client']}.png"
        self.emit("action", batch=self.active, step=details, event=event)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("channel", type=Path)
    parser.add_argument("--context", required=True)
    parser.add_argument("--batch", type=int, required=True)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--wait", type=float, default=10, help="receipt wait only; never extends the session")
    args = parser.parse_args()
    sha(args.context)
    require(1 <= args.batch <= 501, "invalid batch number")
    number(args.wait, .01, 600)
    require(not args.channel.is_symlink() and args.channel.stat().st_uid == os.getuid()
            and stat.S_IMODE(args.channel.stat().st_mode) == 0o700,
            "operator channel must be a private directory")
    ready = load_request(args.channel / "ready.json")
    require(ready["context"] == args.context, "stale operator context")
    request = load_request(args.request)
    require(isinstance(request, dict) and "context" not in request, "request file must omit context")
    end = min(ready["deadline_monotonic"], time.monotonic() + args.wait)
    require(time.monotonic() < end, "operator deadline expired")
    with tempfile.NamedTemporaryFile(dir=args.channel, prefix=".request-", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            data = json.dumps(dict(request, context=args.context), allow_nan=False).encode()
            require(len(data) <= MAX_REQUEST_BYTES, "operator request too large")
            stream.write(data); stream.flush(); os.fsync(stream.fileno())
            os.link(temporary, args.channel / f"{args.batch:04d}.request.json")
        finally:
            temporary.unlink()
    receipt = args.channel / f"{args.batch:04d}.receipt.json"
    while not receipt.exists():
        require(time.monotonic() < end, "receipt wait expired; inspect receipts, never resubmit automatically")
        time.sleep(min(.05, end - time.monotonic()))
    result = load_request(receipt)
    print(json.dumps(result))
    require(result["actions_completed"], "operator batch failed; session is not a normal pass")


if __name__ == "__main__":
    main()
