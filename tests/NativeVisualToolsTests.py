#!/usr/bin/env python3
"""Routine helper regressions. Synthetic frames are NOT game capture evidence."""
import copy
import ctypes
from contextlib import contextmanager
import io
import json
import os
from pathlib import Path
import select
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest, record, verify_runtime
from tools.native_visual.controller import Scheduler, expand, seed_names, Native, probe_display_geometry
from tools.native_visual.recognition import Canvas, MAIN, ENTRY, GEOMETRIES, recognize


class GeometryTests(unittest.TestCase):
    def test_authoritative_float32_transform(self):
        for size, origin in zip(GEOMETRIES, ((0, 0), (202, 0), (93, 0), (386, 67))):
            with self.subTest(size=size):
                canvas = Canvas(*size)
                self.assertEqual(origin, canvas.origin)
                self.assertEqual((origin[0], size[1] - origin[1]), canvas.point(0, 0))
        with self.assertRaises(ValueError): Canvas(1280, 800)
        self.assertEqual((203,0),Canvas(1280,720,True).origin)
        self.assertEqual((94,0),Canvas(1280,900,True).origin)

    def frame(self, size, entry=False):
        canvas = Canvas(*size)
        pixels = bytearray(bytes((170, 170, 170)) * (size[0] * size[1]))
        for anchor in MAIN[:4] + (ENTRY[3:] if entry else []):
            left, top, right, bottom = Canvas(*size,anchor.get("network",False)).box(anchor["box"])
            for y in range(top, bottom):
                start = (y * canvas.width + left) * 3
                pixels[start:start + (right-left)*3] = bytes(anchor["rgb"]) * (right-left)
        return pixels

    def test_main_and_guest720_require_positive_and_negative_anchors(self):
        for size in GEOMETRIES:
            with self.subTest(size=size):
                canvas = Canvas(*size)
                frame = self.frame(size)
                self.assertTrue(recognize(frame, canvas, MAIN)[0])
                self.assertFalse(recognize(frame, canvas, ENTRY)[0])
                entry = self.frame(size, True)
                self.assertTrue(recognize(entry, canvas, ENTRY)[0])
                self.assertFalse(recognize(entry, canvas, MAIN)[0])
                for color in ((0,0,0), (255,255,255), (0,0,200), (170,170,170)):
                    self.assertFalse(recognize(bytes(color) * (size[0]*size[1]), canvas, MAIN)[0])
                left, top, right, bottom = canvas.box(MAIN[1]["box"])
                for y in range(top,bottom):
                    start=(y*canvas.width+left)*3
                    frame[start:start+(right-left)*3] = bytes((right-left)*3)
                self.assertFalse(recognize(frame, canvas, MAIN)[0])

    def test_wrong_client_size_and_empty_anchor_are_errors(self):
        with self.assertRaises(ValueError): recognize(b"", Canvas(1280,720), MAIN)
        with self.assertRaises(ValueError): recognize(self.frame((850,700)), Canvas(850,700), [])


class Clock:
    def __init__(self): self.now = 10
    def __call__(self): return self.now
    def sleep(self, duration): self.now += duration


class FakeNative:
    canvas = Canvas(850,700)
    def __init__(self, clock, broken=False): self.clock, self.broken, self.events = clock, broken, []
    def prepare_input(self, deadline): pass
    def input(self, action, value, *, deadline):
        self.events.append((action,value,self.clock()))
        if self.broken and action == "keydown": raise RuntimeError("press transport failure")
    def release_all(self, releases, *, deadline):
        errors=[]
        for action,value in releases:
            try: self.input(action,value,deadline=deadline)
            except Exception as error: errors.append(str(error))
        return errors
    def point(self,x,y): self.events.append(("point",(x,y),self.clock()))
    def frame(self): return b"test", GeometryTests().frame((850,700))


class ScheduleTests(unittest.TestCase):
    def test_monotonic_schedule_and_maximum_hold(self):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock(); client=FakeNative(clock)
            schedule=Scheduler({"guest":client},tmp,10,clock=clock,sleep=clock.sleep)
            schedule.run([{"client":"guest","action":"key","key":"Right","hold":2,"at":1},
                          {"client":"guest","action":"key","key":"Left","hold":.1,"at":4}])
            self.assertEqual([("keydown","Right",11),("keyup","Right",12.9),
                              ("keydown","Left",14),("keyup","Left",14.1)],client.events)
            self.assertEqual(1,json.loads((Path(tmp)/"actions.json").read_text())["receipt_version"])

    def test_release_after_press_or_clock_failure(self):
        for broken in (True,False):
            with self.subTest(broken=broken), tempfile.TemporaryDirectory() as tmp:
                clock=Clock(); client=FakeNative(clock,broken)
                def failing_sleep(_): raise RuntimeError("sleep failure")
                schedule=Scheduler({"g":client},tmp,10,clock=clock,sleep=failing_sleep)
                with self.assertRaises(RuntimeError):
                    schedule.run([{"client":"g","action":"key","key":"Right","hold":1}])
                self.assertEqual(["keydown","keyup"],[x[0] for x in client.events])

    def test_modifier_chord_is_bounded_and_all_released(self):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock(); client=FakeNative(clock)
            Scheduler({"g":client},tmp,3,clock=clock,sleep=clock.sleep).run([
                {"client":"g","action":"key","key":"Page_Down","modifiers":["Shift_L"],"hold":.1}])
            self.assertEqual([("keydown","Shift_L"),("keydown","Page_Down"),
                              ("keyup","Page_Down"),("keyup","Shift_L")],
                             [(action,key) for action,key,_ in client.events])

    def test_reject_long_hold_missed_schedule_and_deadline_before_input(self):
        for steps in ([{"hold":2.01}], [{"hold":1,"at":0},{"hold":1,"at":0}], [{"hold":2}]):
            with tempfile.TemporaryDirectory() as tmp:
                clock=Clock(); client=FakeNative(clock)
                schedule=Scheduler({"g":client},tmp,1.5,clock=clock,sleep=clock.sleep)
                with self.assertRaises(ValueError):
                    schedule.run([dict(client="g",action="key",key="Left",**item) for item in steps])
                self.assertNotEqual("keydown",client.events[-1][0] if client.events else None)

    def test_unknown_state_never_passes_and_preserves_attempt(self):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock(); client=FakeNative(clock)
            schedule=Scheduler({"g":client},tmp,5,clock=clock,sleep=clock.sleep)
            with self.assertRaisesRegex(ValueError,"state not recognized"):
                schedule.run([{"client":"g","action":"await","state":"network-entry","timeout":.2},
                              {"client":"g","action":"key","key":"F1"}])
            self.assertFalse(client.events)
            self.assertTrue((Path(tmp)/"actions.json").exists())

    def test_macro_is_finite_data(self):
        step={"action":"key","client":"g","key":"F2"}
        self.assertEqual([step,step],expand([{"macro":"setup"}],{"setup":[step,step]}))
        with self.assertRaises(ValueError): expand([{"macro":"a"}],{"a":[{"macro":"a"}]})
        with self.assertRaises(ValueError): expand([{"macro":"missing"}])

    def test_one_failed_encoder_does_not_skip_other_finalization(self):
        with tempfile.TemporaryDirectory() as tmp:
            finalized=[]
            class Encoder:
                def __init__(self,name): self.name=name
                def stop(self):
                    finalized.append(self.name)
                    if self.name=="failed": raise ValueError("test-only drain failure")
            schedule=Scheduler({},tmp,5)
            schedule.recordings={name:Encoder(name) for name in ("failed","other")}
            with self.assertRaisesRegex(ValueError,"finalization failed"): schedule.run([])
            self.assertEqual(["failed","other"],finalized)
            receipt=json.loads((Path(tmp)/"actions.json").read_text())
            self.assertEqual({"failed":"test-only drain failure"},receipt["recording_errors"])


class IdentityTests(unittest.TestCase):
    def runtime(self,root):
        for directory in ("data","levels","profiles","shaders","sound","textures"):
            (root/directory).mkdir()
        (root/"duel6r").write_text("test-only executable identity")
        (root/"duel6r").chmod(0o755)
        return {"source_revision":"a"*40,"application_sha256":digest(root/"duel6r"),
                "files":{"duel6r":digest(root/"duel6r")}}

    def test_added_changed_missing_and_symlink_fail(self):
        for mode in ("added","changed","missing","symlink"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); identity=self.runtime(root); verify_runtime(root,identity)
                if mode=="added": (root/"extra").write_text("unexpected")
                if mode=="changed": (root/"duel6r").write_text("changed")
                if mode=="missing": (root/"duel6r").unlink()
                if mode=="symlink": (root/"alias").symlink_to(root/"duel6r")
                with self.assertRaises(ValueError): verify_runtime(root,identity)

    def test_name_only_no_statistics_or_gameplay_injection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); self.runtime(root)
            seed_names(root,["H"*64,"Guest"])
            data=json.loads((root/"data/persons.json").read_text())
            self.assertEqual(["H"*64,"Guest"],data["playing"])
            self.assertEqual(0,data["rounds"])
            for person in data["persons"]:
                self.assertEqual(1000,person.pop("elo")); person.pop("name")
                self.assertTrue(all(x==0 for x in person.values()))
            with self.assertRaises(ValueError): seed_names(root,["é"*33])

    def test_multibyte_names_are_raw_utf8_for_the_byte_copy_consumer(self):
        names = ["H00-" + "á" * 30, "P01-" + "ž" * 30,
                 "N-" + "e\u0301" * 20 + "xy"]
        self.assertTrue(all(len(name.encode("utf-8")) == 64 for name in names))
        before = names.copy()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.runtime(root)
            seed_names(root, names)
            raw = (root / "data/persons.json").read_bytes()
            data = json.loads(raw)
            self.assertEqual(before, names)
            self.assertEqual(names, data["playing"])
            self.assertEqual(names, [person["name"] for person in data["persons"]])
            self.assertNotIn(b"\\u", raw)
            for name in names:
                # Frozen8b JsonParser::parseString/readUntil copies these bytes
                # verbatim; a JSON roundtrip alone would miss escaped Unicode.
                literal = name.encode("utf-8")
                self.assertIn(b'"name": "' + literal + b'"', raw)
                self.assertEqual(2, raw.count(literal))  # persons and playing

    def test_ascii_fixture_bytes_and_literal_backslashes_are_unchanged(self):
        names = ["Alder", r"User\u017e"]
        counters = dict.fromkeys(("shots", "hits", "kills", "deaths", "assistances", "wins", "penalties",
                                 "games", "timeAlive", "totalGameTime", "totalDamage", "assistedDamage",
                                 "eloTrend", "eloGames"), 0)
        counters["elo"] = 1000
        legacy = json.dumps({"persons": [dict(counters, name=name) for name in names],
                             "playing": names, "rounds": 0}).encode("utf-8")
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.runtime(root)
            seed_names(root, names)
            raw = (root / "data/persons.json").read_bytes()
            self.assertEqual(legacy, raw)
            self.assertEqual(names, json.loads(raw)["playing"])

    def test_full_multibyte_roster_keeps_only_zero_default_person_fields(self):
        names = [f"P{index:02d}-" + "ž" * 30 for index in range(15)]
        expected_counters = {"shots": 0, "hits": 0, "kills": 0, "deaths": 0,
                             "assistances": 0, "wins": 0, "penalties": 0, "games": 0,
                             "timeAlive": 0, "totalGameTime": 0, "totalDamage": 0,
                             "assistedDamage": 0, "eloTrend": 0, "eloGames": 0, "elo": 1000}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.runtime(root)
            before = {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}
            seed_names(root, names)
            data = json.loads((root / "data/persons.json").read_bytes())
            self.assertEqual({"persons", "playing", "rounds"}, set(data))
            self.assertEqual(names, data["playing"])
            self.assertEqual(0, data["rounds"])
            self.assertEqual([dict(expected_counters, name=name) for name in names], data["persons"])
            self.assertEqual(set(before) | {Path("data/persons.json")},
                             {p.relative_to(root) for p in root.rglob("*") if p.is_file()})
            for path, raw in before.items(): self.assertEqual(raw, (root / path).read_bytes())

    def test_invalid_name_guards_reject_without_rewriting_fixture(self):
        invalid = [[], [""], ["Same", "Same"], [str(index) for index in range(16)],
                   ["P01-" + "ž" * 31], ["line\nbreak"]]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.runtime(root)
            seed_names(root, ["Valid"])
            path = root / "data/persons.json"; before = path.read_bytes()
            for names in invalid:
                with self.subTest(names=names), self.assertRaises(ValueError): seed_names(root, names)
                self.assertEqual(before, path.read_bytes())

    def test_receipt_is_additive(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"receipt.json"; record(path,{"historical":True}); before=path.read_bytes()
            with self.assertRaises(FileExistsError): record(path,{"historical":False})
            self.assertEqual(before,path.read_bytes())


def retain_cli_failure(output):
    """Emit only existing bounded sinks before this CLI fixture's TempDir is removed."""
    for name, limit in (("display-startup.jsonl", 12288), ("fixture.log", 2048), ("completion.json", 2048)):
        try:
            with (output / name).open("rb") as stream:
                stream.seek(0, os.SEEK_END)
                stream.seek(max(0, stream.tell() - limit))
                text = stream.read(limit).decode(errors="replace")
            text = "".join(character if character.isprintable() or character in "\n\t" else "?"
                           for character in text)
            print(f"native-cli failure log {name} (tail <= {limit} bytes):\n{text}",
                  file=sys.stderr, flush=True)
        except BaseException:
            try:
                print(f"native-cli failure log unavailable: {name}", file=sys.stderr, flush=True)
            except BaseException:
                pass


class CLIFailureRetentionTests(unittest.TestCase):
    def test_retains_only_allowlisted_bounded_safe_text_before_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            (output / "display-startup.jsonl").write_text("discarded-prefix\n" + "x" * 12288 + "tail-marker")
            (output / "fixture.log").write_text("Xvfb error\x1b[31m\n")
            (output / "completion.json").write_text('{"normal_cleanup": false}')
            (output / "operator").mkdir()
            (output / "operator/ready.json").write_text("test-only-private-context")
            captured = io.StringIO()
            with patch("sys.stderr", captured): retain_cli_failure(output)
            text = captured.getvalue()
            self.assertIn("tail-marker", text)
            self.assertNotIn("discarded-prefix", text)
            self.assertNotIn("test-only-private-context", text)
            self.assertNotIn("\x1b", text)
            self.assertIn('"normal_cleanup": false', text)
            self.assertLess(len(text), 17000)
        self.assertFalse(output.exists())

    def test_missing_read_and_print_failures_preserve_primary_assertion_and_cleanup(self):
        for mode in ("missing", "read", "print"):
            primary = AssertionError("test-only primary assertion")
            with self.subTest(mode=mode), self.assertRaises(AssertionError) as raised:
                with tempfile.TemporaryDirectory() as tmp:
                    output = Path(tmp)
                    if mode != "missing": (output / "fixture.log").write_text("test-only Xvfb log")
                    sink = Mock()
                    if mode == "print": sink.write.side_effect = OSError("test-only stderr failure")
                    try:
                        raise primary
                    except BaseException:
                        with patch("sys.stderr", sink):
                            if mode == "read":
                                with patch.object(Path, "open", side_effect=PermissionError("test-only read failure")):
                                    retain_cli_failure(output)
                            else:
                                retain_cli_failure(output)
                        raise
            self.assertIs(primary, raised.exception)
            self.assertFalse(output.exists())


class ControllerCLITests(unittest.TestCase):
    def test_geometry_probe_success_is_single_and_strict(self):
        env={"DISPLAY":":23"}
        with patch("tools.native_visual.controller.subprocess.run",return_value=
                   subprocess.CompletedProcess([],0,b"850 700\n",b"")) as run:
            probe_display_geometry(":23",4242,Canvas(850,700),env)
        run.assert_called_once_with(["xdotool","getdisplaygeometry"],env=env,
                                    capture_output=True,timeout=1)

    def test_geometry_probe_failure_context_is_bounded_without_retry(self):
        cases=(subprocess.CompletedProcess([],0,b"1280 720\n",b""),
               subprocess.CompletedProcess([],7,b"wrong\n"*100,b"error\n"*100),
               subprocess.TimeoutExpired(["xdotool","getdisplaygeometry"],1,
                                         output=b"partial\n",stderr=b"unfinished\n"),
               FileNotFoundError(2,"not found","xdotool"))
        for result in cases:
            with self.subTest(result=type(result).__name__), patch(
                    "tools.native_visual.controller.subprocess.run") as run:
                if isinstance(result,Exception): run.side_effect=result
                else: run.return_value=result
                with self.assertRaisesRegex(ValueError,"unexpected display geometry: ") as raised:
                    probe_display_geometry(":23",4242,Canvas(850,700),{"DISPLAY":":23"})
                run.assert_called_once()
                message=str(raised.exception)
                self.assertNotIn("\n",message)
                details=json.loads(message.split(": ",1)[1])
                self.assertEqual((":23",4242,"850x700"),
                                 (details["display"],details["xvfb_pid"],details["expected"]))
                self.assertEqual(getattr(result,"returncode",None),details["probe_rc"])
                self.assertEqual((getattr(result,"stdout",None) or b"")[:160].decode(),
                                 details["probe_stdout"])
                self.assertEqual((getattr(result,"stderr",None) or b"")[:160].decode(),
                                 details["probe_stderr"])

    def test_real_controller_private_test_runtime_normal_and_orphan_failure(self):
        for leaking in (False,True):
            with self.subTest(leaking=leaking), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); runtime=root/"runtime"; runtime.mkdir()
                identity=IdentityTests().runtime(runtime)
                subprocess.run(["cc",str(Path(__file__).with_name("NativeVisualX11Fixture.c")),
                                *(["-DTEST_LEAK"] if leaking else []),"-o",str(runtime/"duel6r"),"-lX11"],
                               check=True,capture_output=True,timeout=20)
                identity["application_sha256"]=digest(runtime/"duel6r")
                identity["files"]["duel6r"]=identity["application_sha256"]
                (root/"identity.json").write_text(json.dumps(identity))
                (root/"recipe.json").write_text(json.dumps({"clients":[
                    {"id":"fixture","geometry":[850,700],"names":["Test-only"]}],"budget_seconds":10,
                    "states":{"fixture":[{"box":[5,5,5,5],"rgb":[0,0,0],"minimum":1}]},
                    "steps":[{"action":"await","client":"fixture","state":"fixture"},
                             {"action":"key","client":"fixture","key":"Escape"},
                             {"action":"exit_wait","client":"fixture"}]}))
                try:
                    result=subprocess.run([sys.executable,"-B","-m","tools.native_visual.controller",
                        "--runtime",str(runtime),"--identity",str(root/"identity.json"),
                        "--recipe",str(root/"recipe.json"),"--output",str(root/"output")],
                        cwd=Path(__file__).resolve().parents[1],capture_output=True,text=True,timeout=30)
                    completion=json.loads((root/"output/completion.json").read_text())
                    self.assertEqual(not leaking,result.returncode==0,(result.stdout,result.stderr))
                    self.assertEqual(not leaking,completion["normal_cleanup"])
                    self.assertTrue(completion["ownership"]["cleanup"])
                    if not leaking:
                        actions=json.loads((root/"output/actions.json").read_text())["events"]
                        self.assertEqual("exit_wait",actions[-1]["action"])
                        self.assertIn("finished",actions[-1])
                        self.assertIn("down",(root/"output/fixture.log").read_text())
                    verify_runtime(runtime,identity)
                except BaseException:
                    retain_cli_failure(root/"output")
                    raise


@contextmanager
def native_window(root):
    """Private, test-only event observer. No game process or accepted evidence."""
    binary=root/"events"
    subprocess.run(["cc",str(Path(__file__).with_name("NativeVisualX11Fixture.c")),
                    "-o",str(binary),"-lX11"],check=True,capture_output=True,timeout=20)
    env=dict(os.environ,DISPLAY=":119")
    with (root/"events.log").open("wb") as log:
        display=subprocess.Popen(["Xvfb",":119","-screen","0","850x700x24","-noreset"],
                                 stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        app=None
        try:
            for _ in range(40):
                check=subprocess.run(["xdotool","getdisplaygeometry"],env=env,capture_output=True,timeout=1)
                if check.returncode==0: break
                time.sleep(.05)
            app=subprocess.Popen([str(binary)],env=env,stdout=log)
            for _ in range(40):
                lines=(root/"events.log").read_text().splitlines()
                if lines: break
                time.sleep(.05)
            client=Native(":119",int(lines[0]),Canvas(850,700))
            client.process=app
            client.command("xdotool","windowfocus",client.window)
            yield client
        finally:
            if app is not None:
                if app.poll() is None: app.terminate()
                app.wait(timeout=3)
            display.terminate(); display.wait(timeout=3)


class NativeInputTests(unittest.TestCase):
    """Real X11 input into a test-only event window, not a game/preflight capture."""
    def test_native_hold_schedule_release_and_owned_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                start=time.monotonic()
                Scheduler({"g":client},root,3).run([
                    {"client":"g","action":"key","key":"Right","hold":.12,"at":.08},
                    {"client":"g","action":"key","key":"Left","hold":.06,"at":.35}])
                time.sleep(.1)
                events=[line.split() for line in (root/"events.log").read_text().splitlines()[1:]]
                self.assertEqual(["down","up","down","up"],[e[0] for e in events])
                self.assertGreaterEqual(float(events[1][1])-float(events[0][1]),.10)
                self.assertLess(float(events[1][1])-float(events[0][1]),.5)
                self.assertGreaterEqual(float(events[0][1])-start,.06)

    def test_native_full_frame_geometry_and_release_after_delivered_press_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                png,rgb=client.frame()
                self.assertTrue(png.startswith(b"\x89PNG")); self.assertEqual(850*700*3,len(rgb))
                deliver=client.input
                def broken(action,value,*,deadline):
                    deliver(action,value,deadline=deadline)
                    if action=="keydown": raise RuntimeError("lost response after actual native delivery")
                client.input=broken
                with self.assertRaises(RuntimeError):
                    Scheduler({"g":client},root,3).run([
                        {"client":"g","action":"key","key":"Right","hold":.12}])
                time.sleep(.1)
                events=[line.split()[0] for line in (root/"events.log").read_text().splitlines()[1:]]
                self.assertEqual(["down","up"],events)

    def test_keypress_exit_releases_without_targeting_destroyed_window(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                Scheduler({"g":client},root,3).run([
                    {"client":"g","action":"key","key":"Escape","hold":.15},
                    {"client":"g","action":"exit_wait"}])
                self.assertEqual(0,client.process.wait(timeout=1))
                with self.assertRaises(subprocess.CalledProcessError):
                    client.command("xdotool","getwindowgeometry",client.window)
                self.assertFalse(keys_down(client.env["DISPLAY"],["Escape"])["Escape"])
                receipt=json.loads((root/"actions.json").read_text())
                self.assertIn("finished",receipt["events"][-1])

    def test_delivered_press_with_three_second_ack_delay_fails_and_releases_in_time(self):
        for hold in (.15,2):
            with self.subTest(hold=hold), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp)
                with native_window(root) as client:
                    shadow_xdotool(root,client,"delay-press")
                    started=time.monotonic()
                    with self.assertRaises(ValueError) as raised:
                        Scheduler({"g":client},root,5).run([
                            {"client":"g","action":"key","key":"Right","modifiers":["Shift_L"],"hold":hold},
                            {"client":"g","action":"key","key":"Left"}])
                    self.assertLess(time.monotonic()-started,2.5)
                    self.assertTrue((root/"delivered").exists())
                    events=[line.split() for line in (root/"events.log").read_text().splitlines()[1:]]
                    for key in ("Right","Shift_L"):
                        downs=[float(e[1]) for e in events if e[0]=="down" and e[2]==key]
                        ups=[float(e[1]) for e in events if e[0]=="up" and e[2]==key]
                        self.assertEqual(1,len(downs),events); self.assertEqual(1,len(ups),events)
                        self.assertLessEqual(ups[0]-downs[0],2.0)
                    self.assertFalse(any(keys_down(client.env["DISPLAY"],["Right","Shift_L"]).values()))
                    self.assertRegex(str(raised.exception),"acknowledgement deadline")
                    receipt=json.loads((root/"actions.json").read_text())
                    self.assertEqual(1,len(receipt["events"]))
                    self.assertNotIn("finished",receipt["events"][0])

    def test_other_release_errors_are_not_hidden(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                shadow_xdotool(root,client,"release-error")
                with self.assertRaises(ValueError) as raised:
                    Scheduler({"g":client},root,3).run([
                        {"client":"g","action":"key","key":"Right","modifiers":["Shift_L"],"hold":.15}])
                self.assertTrue((root/"delivered").exists())
                events=[line.split() for line in (root/"events.log").read_text().splitlines()[1:]]
                for key in ("Right","Shift_L"):
                    downs=[float(e[1]) for e in events if e[0]=="down" and e[2]==key]
                    ups=[float(e[1]) for e in events if e[0]=="up" and e[2]==key]
                    self.assertEqual(1,len(downs),events); self.assertEqual(1,len(ups),events)
                    self.assertLessEqual(ups[0]-downs[0],2.0)
                self.assertFalse(any(keys_down(client.env["DISPLAY"],["Right","Shift_L"]).values()))
                self.assertRegex(str(raised.exception),"release failed")

    def test_delayed_release_ack_does_not_block_other_modifier_release(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                shadow_xdotool(root,client,"delay-release")
                started=time.monotonic()
                with self.assertRaises(ValueError):
                    Scheduler({"g":client},root,3).run([
                        {"client":"g","action":"key","key":"Right","modifiers":["Shift_L"],"hold":.15}])
                self.assertLess(time.monotonic()-started,1)
                self.assertFalse(any(keys_down(client.env["DISPLAY"],["Right","Shift_L"]).values()))
                events=[line.split() for line in (root/"events.log").read_text().splitlines()[1:]]
                for key in ("Right","Shift_L"):
                    down=next(float(e[1]) for e in events if e[0]=="down" and e[2]==key)
                    up=next(float(e[1]) for e in events if e[0]=="up" and e[2]==key)
                    self.assertLessEqual(up-down,2.0)


def shadow_xdotool(root,client,mode):
    """Real native delivery, then a delayed/error response from the test transport."""
    real=shutil.which("xdotool")
    path=root/"xdotool"
    # No Python interpreter/import startup inside the shared press deadline.
    # Mark delivery only after real xdotool succeeds; ACK faults remain after it.
    path.write_text("#!/bin/sh\n"
        f"{shlex.quote(real)} \"$@\"\n"
        "code=$?\n[ \"$code\" -eq 0 ] || exit \"$code\"\n"
        "if [ \"$#\" -eq 2 ] && [ \"$1\" = keydown ] && [ \"$2\" = Right ]; then\n"
        f" printf '%s\\n' 'native down delivered' > {shlex.quote(str(root/'delivered'))}\n"
        f" if [ {shlex.quote(mode)} = delay-press ]; then exec sleep 3; fi\n"
        "fi\n"
        "if [ \"$1\" = keyup ]; then\n"
        f" if [ {shlex.quote(mode)} = delay-release ]; then exec sleep 3; fi\n"
        f" if [ {shlex.quote(mode)} = release-error ]; then exit 7; fi\n"
        "fi\nexit 0\n")
    path.chmod(0o755)
    client.env["PATH"]=str(root)+":"+client.env["PATH"]


class FaultShimTests(unittest.TestCase):
    def test_faults_follow_delivery_and_keep_three_second_delay_argument(self):
        for mode in ("delay-press","delay-release","release-error"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory(prefix="shim space ") as tmp:
                root=Path(tmp)
                real=root/"real xdotool"
                real.write_text("#!/bin/sh\nprintf '%s\\n' \"$*\" >> "
                                +shlex.quote(str(root/"native-calls"))+"\n")
                real.chmod(0o755)
                # Unit-only sleep observer; actual X11 tests retain real sleep 3.
                sleep=root/"sleep"
                sleep.write_text("#!/bin/sh\nprintf '%s\\n' \"$*\" > "
                                 +shlex.quote(str(root/"sleep-argument"))+"\n")
                sleep.chmod(0o755)
                client=Native(":unused",1,Canvas(850,700))
                with patch.object(shutil,"which",return_value=str(real)):
                    shadow_xdotool(root,client,mode)
                down=subprocess.run([str(root/"xdotool"),"keydown","Right"],env=client.env,timeout=1)
                self.assertEqual(0,down.returncode)
                self.assertTrue((root/"delivered").exists())
                up=subprocess.run([str(root/"xdotool"),"keyup","Right"],env=client.env,timeout=1)
                self.assertEqual(7 if mode=="release-error" else 0,up.returncode)
                self.assertEqual(["keydown Right","keyup Right"],(root/"native-calls").read_text().splitlines())
                if mode!="release-error": self.assertEqual("3\n",(root/"sleep-argument").read_text())
                else: self.assertFalse((root/"sleep-argument").exists())

    def test_native_error_is_not_replaced_by_fault_or_delivery_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); real=root/"real-xdotool"
            real.write_text("#!/bin/sh\nexit 9\n"); real.chmod(0o755)
            client=Native(":unused",1,Canvas(850,700))
            with patch.object(shutil,"which",return_value=str(real)):
                shadow_xdotool(root,client,"delay-press")
            result=subprocess.run([str(root/"xdotool"),"keydown","Right"],env=client.env,timeout=1)
            self.assertEqual(9,result.returncode)
            self.assertFalse((root/"delivered").exists())


def keys_down(display, names):
    """Read the actual private X server keyboard state, including after Window destruction."""
    x=ctypes.CDLL("libX11.so.6")
    x.XOpenDisplay.argtypes=[ctypes.c_char_p]; x.XOpenDisplay.restype=ctypes.c_void_p
    x.XStringToKeysym.argtypes=[ctypes.c_char_p]; x.XStringToKeysym.restype=ctypes.c_ulong
    x.XKeysymToKeycode.argtypes=[ctypes.c_void_p,ctypes.c_ulong]; x.XKeysymToKeycode.restype=ctypes.c_ubyte
    x.XQueryKeymap.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
    x.XCloseDisplay.argtypes=[ctypes.c_void_p]
    connection=x.XOpenDisplay(display.encode())
    if not connection: raise RuntimeError("test display unavailable")
    try:
        bits=(ctypes.c_ubyte*32)(); x.XQueryKeymap(connection,bits)
        codes={name:x.XKeysymToKeycode(connection,x.XStringToKeysym(name.encode())) for name in names}
        return {name:bool(bits[code//8] & (1<<(code%8))) for name,code in codes.items()}
    finally: x.XCloseDisplay(connection)


if __name__=="__main__": unittest.main()
