#!/usr/bin/env python3
"""Focused mailbox/ownership regressions. X11 fixture is NOT game evidence."""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest, verify_runtime
from tools.native_visual.controller import Scheduler
from tools.native_visual.operator import OperatorChannel, load_request
from tools.native_visual.transport_relay import finite_number, private_port, Flow
from NativeVisualToolsTests import Clock, FakeNative, keys_down
import NativeVisualToolsTests as baseline

WORKSPACE = Path(__file__).resolve().parents[1]


class OperatorTests(unittest.TestCase):
    def test_explicit_recipe_budget_boundaries_preserve_600_and_allow_1200(self):
        for budget in (600, 600.01, 1200):
            with self.subTest(budget=budget):
                clock = Clock()
                schedule = Scheduler({}, ".", budget, clock=clock, sleep=clock.sleep)
                self.assertEqual(budget, schedule.remaining())
                self.assertEqual(clock() + budget, schedule.deadline)
        for budget in (0, -1, float("nan"), float("inf"), -float("inf"), 1200.01, sys.float_info.max,
                       True, False, "600", None, [600], {"seconds": 600}, 10**1000):
            with self.subTest(budget=budget), self.assertRaisesRegex(ValueError, "invalid recipe budget"):
                Scheduler({}, ".", budget)

    def test_cli_budget_validation_and_unchanged_outer_watchdog_allowance(self):
        from tools.native_visual.controller import main
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); runtime = root / "runtime"; runtime.mkdir()
            identity = baseline.IdentityTests().runtime(runtime)
            identity_path = root / "identity.json"; identity_path.write_text(json.dumps(identity))
            for index, budget in enumerate((600, 1200, 0, -1, float("nan"), float("inf"), 1200.01, sys.float_info.max,
                                             True, False, "600", None, [600], {"seconds": 600}, 10**1000)):
                for operator in (False, True):
                    with self.subTest(budget=budget, operator=operator):
                        recipe = root / "recipe.json"
                        recipe.write_text(json.dumps(dict(budget_seconds=budget,
                            clients=[dict(id="g", geometry=[850,700], names=["Test-only"])],
                            steps=[dict(action="wait", client="g", seconds=0)])))
                        output = root / f"output-{index}-{operator}"
                        argv = ["controller", "--runtime", str(runtime), "--identity", str(identity_path),
                                "--recipe", str(recipe), "--output", str(output)] + (["--operator"] if operator else [])
                        result = dict(cleanup=True, phase="exited", exit=0, reaped={123: 0})
                        with patch.object(sys, "argv", argv), patch(
                                "tools.native_visual.controller.run_owned", return_value=result) as owner:
                            if budget in (600, 1200):
                                main()
                                owner.assert_called_once()
                                command, timeout, log = owner.call_args.args
                                self.assertEqual(budget + 90, timeout)
                                self.assertEqual(operator, "--operator" in command)
                                self.assertIn("--worker", command)
                                self.assertEqual(output / "process.jsonl", log)
                            else:
                                with self.assertRaisesRegex(ValueError, "invalid recipe budget"): main()
                                owner.assert_not_called()

    def test_long_budget_is_one_deadline_across_batches_and_still_expires(self):
        for expired in (False, True):
            with self.subTest(expired=expired):
                with self.assertRaisesRegex(ValueError, "deadline expired") if expired else self.subTest():
                    with self.channel(budget=1200) as (channel, schedule, client, clock):
                        original = schedule.started, schedule.deadline
                        ready = load_request(channel.root / "ready.json")
                        self.assertEqual(original[0] + 1200, ready["deadline_monotonic"])
                        self.request(channel, 1, steps=[dict(action="wait", client="g", seconds=600 if expired else 599)])
                        self.request(channel, 2, steps=[dict(action="exit_wait", client="g", at=1199)])
                        self.request(channel, 3, finish=True)
                        try:
                            self.run_channel(channel, schedule, [dict(action="wait", client="g", seconds=600)])
                        finally:
                            self.assertEqual(original, (schedule.started, schedule.deadline))
                            self.assertEqual(1200 if expired else 1199, clock() - schedule.started)
                            self.assertFalse(client.events)

    @contextmanager
    def channel(self, budget=10, macros=None):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); runtime = root / "runtime"; runtime.mkdir()
            identity = baseline.IdentityTests().runtime(runtime)
            output = root / "output"; output.mkdir()
            clock = Clock(); client = FakeNative(clock)
            client.process = Mock(pid=123, returncode=0); client.process.poll.return_value = 0
            client.window = "456"; client.env = {"DISPLAY": ":test-only"}
            schedule = Scheduler({"g": client}, output, budget, clock=clock, sleep=clock.sleep)
            with OperatorChannel(schedule, runtime, identity, {"macros": macros or {}}) as channel:
                yield channel, schedule, client, clock

    def request(self, channel, batch, **body):
        (channel.root / f"{batch:04d}.request.json").write_text(json.dumps(dict(context=channel.context, **body)))

    def run_channel(self, channel, schedule, initial=()):
        schedule.run(channel.steps(list(initial)), observe=channel.observe)

    def test_global_expanded_500_cap_includes_initial_and_every_batch(self):
        wait = dict(action="wait", client="g", seconds=0)
        for accepted in (True, False):
            with self.subTest(accepted=accepted):
                with self.assertRaisesRegex(ValueError, "global 500-action") if not accepted else self.subTest():
                    with self.channel(macros={"two": [wait, wait]}) as (channel, schedule, client, clock):
                        self.request(channel, 1, steps=[{"macro": "two"}])
                        self.request(channel, 2, steps=[dict(action="exit_wait", client="g")])
                        self.request(channel, 3, finish=True)
                        initial = [wait] * (497 if accepted else 499)
                        try:
                            self.run_channel(channel, schedule, initial)
                        finally:
                            self.assertEqual(500 if accepted else 499, len(schedule.events))
                            self.assertFalse(client.events)
                        self.assertEqual(499, schedule.events[-1]["ordinal"])

    def test_whole_invalid_batch_is_rejected_before_any_input_and_no_retry(self):
        good = dict(action="key", client="g", key="Right", hold=.05)
        bad = [dict(good, hold=2.1), dict(good, key="Right;echo"), dict(good, arbitrary_shell="x"),
               dict(action="shell", client="g", command="x"), dict(good, client="unowned"),
               dict(action="await", client="g", state="unknown"),
               dict(action="click", client="g", point=[850, 1]),
               dict(action="record", client="g", clip="x", seconds=.4, frames=None),
               dict(action="wait", client="g", seconds=True), {"macro": "cycle"}]
        for step in bad:
            with self.subTest(step=step), self.assertRaises((ValueError, KeyError)):
                with self.channel(macros={"cycle": [{"macro": "cycle"}]}) as (channel, schedule, client, clock):
                    self.request(channel, 1, steps=[good, step])
                    self.request(channel, 2, steps=[good])
                    try:
                        self.run_channel(channel, schedule)
                    finally:
                        self.assertFalse(client.events)
                        self.assertFalse(schedule.events)
                        self.assertFalse((channel.root / "0002.receipt.json").exists())

    def test_global_at_and_remaining_budget_do_not_restart_at_next_batch(self):
        for step in (dict(action="key", client="g", key="Right", at=.5),
                     dict(action="wait", client="g", seconds=2)):
            with self.subTest(step=step), self.assertRaises(ValueError):
                with self.channel(budget=2) as (channel, schedule, client, clock):
                    original = (schedule.started, schedule.deadline)
                    self.request(channel, 1, steps=[step])
                    try:
                        self.run_channel(channel, schedule, [dict(action="wait", client="g", seconds=1)])
                    finally:
                        self.assertEqual(original, (schedule.started, schedule.deadline))
                        self.assertFalse(client.events)

    def test_finish_cannot_pass_if_runtime_verification_consumes_remaining_deadline(self):
        with self.assertRaisesRegex(ValueError, "deadline expired"):
            with self.channel(budget=1) as (channel, schedule, client, clock):
                self.request(channel, 1, finish=True)
                with patch("tools.native_visual.operator.verify_runtime", side_effect=lambda *_: clock.sleep(1)):
                    self.run_channel(channel, schedule, [dict(action="exit_wait", client="g")])

    def test_live_recording_and_inputs_across_batches_finalize_exactly_once(self):
        encoder = Mock(counters={"frame": 7}); encoder.ready.wait.return_value = True
        encoder.process.poll.return_value = None
        with patch("tools.native_visual.controller.Recording", return_value=encoder):
            with self.channel() as (channel, schedule, client, clock):
                self.request(channel, 1, steps=[dict(action="key", client="g", key="Right", hold=.1),
                                                dict(action="capture", client="g")])
                self.request(channel, 2, steps=[dict(action="exit_wait", client="g")])
                self.request(channel, 3, finish=True)
                original_receipt = channel.receipt
                def receipt(batch, passed):
                    if passed:
                        encoder.stop.assert_not_called()
                    if batch >= 1:
                        self.assertEqual(["keydown", "keyup"], [e[0] for e in client.events])
                    original_receipt(batch, passed)
                channel.receipt = receipt
                self.run_channel(channel, schedule, [dict(action="record", client="g", clip="live", seconds=1, frames=15)])
                encoder.stop.assert_called_once_with()
                self.assertEqual([0, 1, 2, 3], [e["ordinal"] for e in schedule.events])
                events = [json.loads(line) for line in (channel.root / "events.jsonl").read_text().splitlines()]
                capture = next(e for e in events if e["kind"] == "action" and e["step"]["action"] == "capture")
                self.assertTrue((schedule.output / capture["step"]["png"]).exists())
                receipt = load_request(channel.root / "0001.receipt.json")
                self.assertFalse(receipt["overall_normal_cleanup"])
                self.assertEqual({"frame": 7}, receipt["recordings"]["live"]["encoder_progress"])

    def test_cancel_truncated_eof_stale_context_or_changed_runtime_fail_without_input(self):
        for mode in ("cancel", "eof", "stale", "runtime", "finish"):
            with self.subTest(mode=mode), self.assertRaises((ValueError, json.JSONDecodeError)):
                with self.channel() as (channel, schedule, client, clock):
                    self.request(channel, 1, **({"finish": True} if mode == "finish" else {"cancel": True}))
                    path = channel.root / "0001.request.json"
                    if mode == "eof": path.write_text('{"steps":[')
                    if mode == "stale": path.write_text(json.dumps(dict(context="0"*64, steps=[])))
                    if mode == "runtime": (channel.runtime / "extra").write_text("changed input")
                    try:
                        self.run_channel(channel, schedule)
                    finally:
                        self.assertFalse(client.events)

    def test_absent_operator_deadline_releases_and_finalizes_without_budget_extension(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); runtime = root / "runtime"; runtime.mkdir()
            identity = baseline.IdentityTests().runtime(runtime)
            client = FakeNative(time.monotonic)
            client.process = Mock(pid=123); client.window = "456"; client.env = {"DISPLAY": ":test-only"}
            schedule = Scheduler({"g": client}, root, .25)
            encoder = Mock(counters={}); encoder.ready.wait.return_value = True; encoder.process.poll.return_value = None
            begin = time.monotonic()
            with self.assertRaisesRegex(ValueError, "deadline expired"), patch(
                    "tools.native_visual.controller.Recording", return_value=encoder):
                with OperatorChannel(schedule, runtime, identity, {}) as channel:
                    self.run_channel(channel, schedule, [dict(action="record", client="g", clip="live", seconds=.1),
                        dict(action="key", client="g", key="Right", hold=.01)])
            self.assertLess(time.monotonic() - begin, 1)
            self.assertEqual(["keydown", "keyup"], [e[0] for e in client.events])
            encoder.stop.assert_called_once_with()

    def test_diagnostic_write_failure_cannot_mask_input_failure_or_skip_release(self):
        with self.assertRaisesRegex(RuntimeError, "press transport failure"):
            with self.channel() as (channel, schedule, client, clock):
                client.broken = True
                channel.observe = Mock(side_effect=OSError("test-only journal failure"))
                channel.write = Mock(side_effect=OSError("test-only receipt failure"))
                try:
                    self.run_channel(channel, schedule, [dict(action="key", client="g", key="Right", hold=.1)])
                finally:
                    self.assertEqual(["keydown", "keyup"], [e[0] for e in client.events])
        channel.write.assert_called_once()

    def test_request_bounds_duplicate_fields_symlink_and_secret_free_journal(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "bad"
            for data in ('{"x":1,"x":2}', '{"x":NaN}', 'x' * (256*1024+1), ''):
                path.write_text(data)
                with self.assertRaises(ValueError): load_request(path)
            link = path.with_name("link"); link.symlink_to(path)
            with self.assertRaises(OSError): load_request(link)
        with self.channel() as (channel, schedule, client, clock):
            client.command = Mock()
            self.request(channel, 1, steps=[dict(action="text", client="g", text="test-only-redacted"),
                                          dict(action="exit_wait", client="g")])
            self.request(channel, 2, finish=True)
            self.run_channel(channel, schedule)
            journal = (channel.root / "events.jsonl").read_text()
            self.assertNotIn("test-only-redacted", journal)
            self.assertIn('"text_redacted": true', journal)


class NativeOperatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(); cls.binary = Path(cls.tmp.name) / "fixture"
        subprocess.run(["cc", str(WORKSPACE / "tests/NativeVisualX11Fixture.c"), "-o", str(cls.binary), "-lX11"],
                       check=True, capture_output=True, timeout=20)

    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()

    @contextmanager
    def worker(self, budget=10):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); runtime = root / "runtime"; runtime.mkdir()
            identity = baseline.IdentityTests().runtime(runtime); shutil.copy2(self.binary, runtime / "duel6r")
            identity["application_sha256"] = identity["files"]["duel6r"] = digest(runtime / "duel6r")
            (root / "identity.json").write_text(json.dumps(identity))
            recipe = dict(clients=[dict(id="fixture", geometry=[850,700], names=["Test-only"])],
                          budget_seconds=budget, steps=[])
            (root / "recipe.json").write_text(json.dumps(recipe))
            with (root / "stdout").open("wb") as stdout, (root / "stderr").open("wb") as stderr:
                process = subprocess.Popen([sys.executable, "-B", "-m", "tools.native_visual.controller", "--operator",
                    "--runtime", str(runtime), "--identity", str(root / "identity.json"),
                    "--recipe", str(root / "recipe.json"), "--output", str(root / "output")],
                    cwd=WORKSPACE, stdout=stdout, stderr=stderr)
                try:
                    ready = root / "output/operator/ready.json"
                    end = time.monotonic() + 10
                    while not ready.exists():
                        self.assertIsNone(process.poll(), (root / "stderr").read_text())
                        self.assertLess(time.monotonic(), end)
                        time.sleep(.02)
                    yield root, process, load_request(ready)
                    verify_runtime(runtime, identity)
                finally:
                    if process.poll() is None: process.terminate()
                    process.wait(timeout=8)

    def submit(self, root, ready, batch, **request):
        path = root / f"request-{batch}.json"; path.write_text(json.dumps(request))
        result = subprocess.run([sys.executable, "-B", "-m", "tools.native_visual.operator",
            str(root / "output/operator"), "--context", ready["context"], "--batch", str(batch), "--request", str(path)],
            cwd=WORKSPACE, capture_output=True, text=True, timeout=12)
        self.assertEqual(0, result.returncode, (result.stdout, result.stderr))
        return json.loads(result.stdout)

    def test_two_actual_batches_same_pid_window_full_png_releases_and_normal_finish(self):
        with self.worker() as (root, process, ready):
            client = ready["clients"]["fixture"]
            first = self.submit(root, ready, 1, steps=[dict(action="key", client="fixture", key="Right", hold=.12),
                                                      dict(action="capture", client="fixture")])
            self.assertFalse(first["overall_normal_cleanup"])
            self.assertFalse((root / "output/actions.json").exists())
            self.assertIsNone(process.poll()); os.kill(client["pid"], 0)
            self.assertFalse(any(keys_down(client["display"], ["Right", "Shift_L"]).values()))
            png = root / "output/0001-fixture.png"
            self.assertEqual(b"850 700", subprocess.check_output(["identify", "-format", "%w %h", str(png)]))
            self.assertEqual(ready, load_request(root / "output/operator/ready.json"))
            self.submit(root, ready, 2, steps=[dict(action="key", client="fixture", key="Left", hold=.08),
                dict(action="capture", client="fixture"), dict(action="key", client="fixture", key="Escape"),
                dict(action="exit_wait", client="fixture")])
            self.assertFalse(any(keys_down(client["display"], ["Left", "Escape"]).values()))
            self.submit(root, ready, 3, finish=True)
            self.assertEqual(0, process.wait(timeout=8), (root / "stderr").read_text())
            completion = load_request(root / "output/completion.json")
            self.assertTrue(completion["normal_cleanup"])
            self.assertEqual([], completion["ownership"]["remaining"])
            controller = load_request(root / "output/controller.json")
            self.assertEqual(client["pid"], controller["clients"]["fixture"]["pid"])
            self.assertEqual(client["window"], controller["clients"]["fixture"]["window"])
            actions = load_request(root / "output/actions.json")["events"]
            self.assertEqual(list(range(6)), [e["ordinal"] for e in actions])

    def test_cancel_eof_and_absent_operator_have_actual_owned_cleanup_not_normal_pass(self):
        for mode in ("cancel", "eof", "deadline"):
            with self.subTest(mode=mode), self.worker(budget=3) as (root, process, ready):
                client = ready["clients"]["fixture"]
                self.submit(root, ready, 1, steps=[dict(action="key", client="fixture", key="Right", hold=.05)])
                self.assertFalse(keys_down(client["display"], ["Right"])["Right"])
                if mode == "cancel":
                    path = root / "cancel.json"; path.write_text(json.dumps(dict(context=ready["context"], cancel=True)))
                    os.link(path, root / "output/operator/0002.request.json")
                if mode == "eof":
                    path = root / "eof.json"; path.write_text('{"steps":[')
                    os.link(path, root / "output/operator/0002.request.json")
                self.assertNotEqual(0, process.wait(timeout=8))
                completion = load_request(root / "output/completion.json")
                self.assertFalse(completion["normal_cleanup"])
                self.assertTrue(completion["ownership"]["cleanup"])
                self.assertEqual([], completion["ownership"]["remaining"])
                with self.assertRaises(ProcessLookupError): os.kill(client["pid"], 0)
                self.assertFalse(load_request(root / "output/controller.json")["normal_application_exit"])


class RelayContractTests(unittest.TestCase):
    def test_existing_relay_argument_bounds_and_metadata_only_summary(self):
        import argparse
        for value in ("nan", "inf", "-1", "1001"):
            with self.assertRaises(argparse.ArgumentTypeError): finite_number(0, 1000)(value)
        for value in ("1", "65536"):
            with self.assertRaises(argparse.ArgumentTypeError): private_port(value)
        self.assertEqual(25, finite_number(0, 1000)("25"))
        self.assertEqual(28202, private_port("28202"))
        flow = Flow("test-only", None, None)
        self.assertEqual(0, flow.summary()["pending_bytes"])
        self.assertNotIn("queue", flow.summary())


if __name__ == "__main__": unittest.main()
