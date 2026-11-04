#!/usr/bin/env python3
"""Routine helper regressions. Synthetic frames are NOT game capture evidence."""
import copy
from contextlib import contextmanager
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest, record, verify_runtime
from tools.native_visual.controller import Scheduler, expand, seed_names, Native
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
    def input(self, action, value):
        self.events.append((action,value,self.clock()))
        if self.broken and action == "keydown": raise RuntimeError("press transport failure")
    def point(self,x,y): self.events.append(("point",(x,y),self.clock()))
    def frame(self): return b"test", GeometryTests().frame((850,700))


class ScheduleTests(unittest.TestCase):
    def test_monotonic_schedule_and_maximum_hold(self):
        with tempfile.TemporaryDirectory() as tmp:
            clock=Clock(); client=FakeNative(clock)
            schedule=Scheduler({"guest":client},tmp,10,clock=clock,sleep=clock.sleep)
            schedule.run([{"client":"guest","action":"key","key":"Right","hold":2,"at":1},
                          {"client":"guest","action":"key","key":"Left","hold":.1,"at":4}])
            self.assertEqual([("keydown","Right",11),("keyup","Right",13),
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

    def test_receipt_is_additive(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"receipt.json"; record(path,{"historical":True}); before=path.read_bytes()
            with self.assertRaises(FileExistsError): record(path,{"historical":False})
            self.assertEqual(before,path.read_bytes())


class ControllerCLITests(unittest.TestCase):
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
                result=subprocess.run([sys.executable,"-B","-m","tools.native_visual.controller",
                    "--runtime",str(runtime),"--identity",str(root/"identity.json"),
                    "--recipe",str(root/"recipe.json"),"--output",str(root/"output")],
                    cwd=Path(__file__).resolve().parents[1],capture_output=True,text=True,timeout=30)
                completion=json.loads((root/"output/completion.json").read_text())
                self.assertEqual(not leaking,result.returncode==0,(result.stdout,result.stderr))
                self.assertEqual(not leaking,completion["normal_cleanup"])
                self.assertTrue(completion["ownership"]["cleanup"])
                verify_runtime(runtime,identity)


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
            client.command("xdotool","windowfocus",client.window)
            yield client
        finally:
            if app is not None: app.terminate(); app.wait(timeout=3)
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
                def broken(action,value):
                    deliver(action,value)
                    if action=="keydown": raise RuntimeError("lost response after actual native delivery")
                client.input=broken
                with self.assertRaises(RuntimeError):
                    Scheduler({"g":client},root,3).run([
                        {"client":"g","action":"key","key":"Right","hold":.12}])
                time.sleep(.1)
                events=[line.split()[0] for line in (root/"events.log").read_text().splitlines()[1:]]
                self.assertEqual(["down","up"],events)


if __name__=="__main__": unittest.main()
