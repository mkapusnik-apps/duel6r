#!/usr/bin/env python3
"""Synthetic metadata/media test inputs, explicitly not historical capture receipts."""
import copy
from fractions import Fraction
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest
from tools.native_visual.media import validate, index
from tools.native_visual.recording import Recording
from NativeVisualToolsTests import native_window


def probe90():
    pts=[i*1000000//15 for i in range(1350)]
    frames=[{"pts":p,"pkt_duration":pts[i+1]-p if i<1349 else 89999500-p,
             "width":1280,"height":900} for i,p in enumerate(pts)]
    return {"streams":[{"time_base":"1/1000000","width":1280,"height":900,"nb_frames":"1350"}],
            "frames":frames,"format":{"duration":"89.999500"}}


class BoundaryTests(unittest.TestCase):
    def test_1350_valid_frames_and_89999500_boundary_accepts_subframe_difference(self):
        data=probe90(); result=validate(data,1350,"90","15")
        self.assertEqual(Fraction("89.999500"),Fraction(result["content_end"]))
        self.assertEqual(1350,result["frame_count"])
        data["format"]["duration"]="3.0"  # Container duration alone is not the content oracle.
        validate(data,1350,"90","15")

    def test_missing_frame_wrong_contract_dimensions_and_short_content_rejected(self):
        for case in ("missing","surplus","stream-count","short","dimensions","no-duration","late-start"):
            with self.subTest(case=case):
                data=probe90()
                if case=="missing": data["frames"].pop()
                if case=="surplus": data["frames"].append(copy.deepcopy(data["frames"][-1]))
                if case=="stream-count": data["streams"][0]["nb_frames"]="1349"
                if case=="short": data["frames"][-1]["pkt_duration"]=1
                if case=="dimensions": data["frames"][10]["width"]=850
                if case=="no-duration": del data["frames"][-1]["pkt_duration"]
                if case=="late-start":
                    for frame in data["frames"]: frame["pts"]+=100000
                with self.assertRaises(ValueError): validate(data,1350,"90","15")
        with self.assertRaises(ValueError): validate(probe90(),1349,"90","15")

    def test_nonmonotonic_gap_and_false_large_duration_rejected(self):
        for case in ("equal","backwards","gap","duration"):
            with self.subTest(case=case):
                data=probe90()
                if case=="equal": data["frames"][1]["pts"]=0
                if case=="backwards": data["frames"][1]["pts"]=-1
                if case=="gap": data["frames"][10]["pts"]+=500000
                if case=="duration": data["frames"][10]["pkt_duration"]=500000
                with self.assertRaises(ValueError): validate(data,1350,"90","15")

    def test_missing_duration_is_unknown_boundary_not_decode_corruption(self):
        checks={}
        validate(probe90(),1350,"90","15",checks)
        self.assertTrue(checks["boundary"]["valid"])
        self.assertEqual("known",checks["boundary"]["status"])
        for missing in (None,"N/A"):
            with self.subTest(missing=missing):
                data=probe90(); data["frames"][-1]["pkt_duration"]=missing; checks={}
                with self.assertRaisesRegex(ValueError,"unsupported/unknown boundary"):
                    validate(data,1350,"90","15",checks)
                self.assertTrue(checks["count"]["valid"])
                self.assertTrue(checks["cadence"]["valid"])
                self.assertIsNone(checks["boundary"]["valid"])
                self.assertIsNone(checks["boundary"]["content_end"])
                self.assertEqual(1,checks["boundary"]["missing_duration_frames"])
        data=probe90(); data["frames"][-1]["pkt_duration"]=0; checks={}
        with self.assertRaises(ValueError): validate(data,1350,"90","15",checks)
        self.assertIs(checks["boundary"]["valid"],False)
        self.assertEqual("known",checks["boundary"]["status"])

    def test_181_samples_wrong_count_and_real_pts_gap_are_independent_rejections(self):
        # Synthetic metadata with the observed count/gap, not an acquisition receipt.
        pts=[i*1000000//15 for i in range(181)]
        pts[49]=pts[48]+119565; pts[-1]=11999809
        data={"streams":[{"time_base":"1/1000000","width":1280,"height":720,"nb_frames":"181"}],
              "frames":[{"pts":p,"width":1280,"height":720} for p in pts],
              "format":{"duration":"12.066476"}}
        checks={}
        with self.assertRaisesRegex(ValueError,"decoded frame count deficit or surplus"):
            validate(data,180,"12","15",checks)
        self.assertFalse(checks["count"]["valid"])
        self.assertEqual(180,checks["count"]["expected_frames"])
        self.assertEqual(181,checks["count"]["decoded_frames"])
        self.assertFalse(checks["cadence"]["valid"])
        self.assertEqual(Fraction(".119565"),Fraction(checks["cadence"]["max_pts_gap"]))
        self.assertEqual(1,checks["cadence"]["gaps_over_limit"])
        self.assertEqual(0,checks["cadence"]["nonmonotonic_gaps"])
        self.assertIsNone(checks["boundary"]["valid"])


class DecoderTests(unittest.TestCase):
    def create(self,path,frames=6):
        subprocess.run(["ffmpeg","-nostdin","-v","error","-f","lavfi","-i","testsrc=size=32x24:rate=15",
                        "-frames:v",str(frames),"-c:v","libx264rgb","-crf","0","-threads","1",
                        "-video_track_timescale","1000000",str(path)],check=True,capture_output=True,timeout=30)

    def create_vfr_b_frames(self,path):
        pts=[0,99722,121728,187409,299776,330000]
        expression=str(pts[-1])
        for ordinal,value in reversed(list(enumerate(pts[:-1]))):
            expression=f"if(eq(N,{ordinal}),{value},{expression})"
        subprocess.run(["ffmpeg","-nostdin","-v","error","-f","lavfi","-i","testsrc=size=32x24:rate=15",
                        "-frames:v","6","-vf",f"settb=expr=1/1000000,setpts='{expression}'",
                        "-fps_mode","passthrough","-enc_time_base","1/1000000",
                        "-c:v","libx264rgb","-crf","18","-bf","2","-b_strategy","0","-threads","1",
                        "-video_track_timescale","1000000",str(path)],check=True,capture_output=True,timeout=30)
        return pts

    def test_real_full_decode_exact_ordinal_mapping_and_immutable_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; self.create(source)
            before=source.read_bytes(); receipt=root/"historical.json"; receipt.write_text('{"failed":"old rule"}')
            result=index(source,root/"inspection",expected_frames=6,seconds="2/5",rate="15",
                         candidate="a"*40,provenance=str(receipt),expected_sha=digest(source))
            data=json.loads((root/"inspection/index.json").read_text())
            assessment=json.loads((root/"inspection/assessment.json").read_text())
            self.assertTrue(assessment["valid"]); self.assertTrue(assessment["decode_valid"])
            self.assertTrue(all(value["valid"] for value in assessment["qualification"].values()))
            self.assertEqual(list(range(6)),[x["ordinal"] for x in data["frames"]])
            self.assertEqual(result["source_pts"],[x["pts"] for x in data["frames"]])
            for item in data["frames"]:
                self.assertEqual(item["sha256"],digest(root/"inspection"/item["path"]))
            self.assertEqual(before,source.read_bytes())
            self.assertEqual('{"failed":"old rule"}',receipt.read_text())
            with self.assertRaises(FileExistsError):
                index(source,root/"inspection",expected_frames=6,seconds="2/5",rate="15",
                      candidate="a"*40,provenance=str(receipt),expected_sha=digest(source))

    def test_true_truncation_corruption_and_playable_short_file_fail(self):
        for case in ("truncated","corrupt","short"):
            with self.subTest(case=case), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); source=root/"original.mp4"; self.create(source,4 if case=="short" else 6)
                if case=="truncated": source.write_bytes(source.read_bytes()[:400])
                if case=="corrupt": source.write_bytes(b"not a video")
                before=source.read_bytes()
                with self.assertRaises(ValueError):
                    index(source,root/"rejected",expected_frames=6,seconds="2/5",rate="15",
                          candidate="a"*40,provenance="test-only, no historical receipt",expected_sha=digest(source))
                self.assertEqual(before,source.read_bytes())
                assessment=json.loads((root/"rejected/assessment.json").read_text())
                self.assertFalse(assessment["valid"])
                if case=="short":
                    self.assertTrue(assessment["decode_valid"])
                    self.assertFalse(assessment["qualification"]["count"]["valid"])
                    self.assertEqual("qualification",assessment["failure_stage"])
                else:
                    self.assertIsNot(assessment["decode_valid"],True)

    def test_vfr_b_frames_decode_without_output_rounding_but_keep_strict_rejections(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"vfr.mp4"; pts=self.create_vfr_b_frames(source)
            probe=json.loads(subprocess.check_output(["ffprobe","-v","error","-show_frames","-show_streams",
                "-show_entries","stream=has_b_frames:frame=pts","-of","json",str(source)],timeout=15))
            self.assertGreaterEqual(probe["streams"][0]["has_b_frames"],1)
            self.assertEqual(pts,[frame["pts"] for frame in probe["frames"]])
            before=source.read_bytes()
            with self.assertRaisesRegex(ValueError,"unsupported/unknown boundary"):
                index(source,root/"rejected",expected_frames=6,seconds="2/5",rate="15",
                      candidate="a"*40,provenance="Synthetic B-frame VFR regression only",expected_sha=digest(source))
            assessment=json.loads((root/"rejected/assessment.json").read_text())
            self.assertTrue(assessment["decode_valid"])
            self.assertFalse(assessment["valid"])
            self.assertEqual("qualification",assessment["failure_stage"])
            self.assertTrue(assessment["qualification"]["count"]["valid"])
            self.assertFalse(assessment["qualification"]["cadence"]["valid"])
            self.assertIsNone(assessment["qualification"]["boundary"]["valid"])
            self.assertEqual("unknown",assessment["qualification"]["boundary"]["status"])
            self.assertTrue(assessment["input_unchanged"])
            self.assertEqual(before,source.read_bytes())


class RecordingTests(unittest.TestCase):
    def test_actual_recorder_readiness_drain_and_immutable_finalization(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                original=root/"original.mp4"
                recorder=Recording(client,original,.4)
                try:
                    self.assertTrue(recorder.ready.wait(5),"no first encoded frame")
                    self.assertEqual(0,recorder.process.wait(timeout=5))
                    recorder.request_stop()  # Late request must not claim it ended the encoder.
                finally:
                    recorder.stop()
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertEqual(digest(original),receipt["sha256"])
                self.assertFalse(receipt["forced_stop"])
                self.assertEqual([850,700],receipt["geometry"])
                self.assertIsNone(receipt["stop_reason"])
                self.assertEqual(0,receipt["process_returncode_before_finalization"])
                self.assertEqual({"duration_limit_seconds":.4,"nominal_rate":"15","frame_limit":None,
                                  "timestamps":"passthrough"},receipt["requested_contract"])
                self.assertEqual(1,len(receipt["stop_requests"]))
                request=receipt["stop_requests"][0]
                self.assertEqual("recipe-stop",request["reason"])
                self.assertEqual(0,request["process_returncode_at_request"])
                self.assertFalse(request["sigint_requested"])
                self.assertGreaterEqual(request["requested_monotonic"],receipt["started_monotonic"])
                before=original.read_bytes()
                with self.assertRaises(ValueError): Recording(client,original,.4)
                self.assertEqual(before,original.read_bytes())
                subprocess.run(["ffmpeg","-v","error","-xerror","-i",str(original),
                                "-fps_mode","passthrough","-enc_time_base","1/1000000","-f","null","-"],
                               check=True,capture_output=True,timeout=10)

    def test_actual_encoder_failure_and_partial_original_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                original=root/"interrupted.mp4"
                recorder=Recording(client,original,5)
                try:
                    self.assertTrue(recorder.ready.wait(5))
                finally:
                    if recorder.process.poll() is None: recorder.process.kill()
                    recorder.process.wait(timeout=3)
                    with self.assertRaisesRegex(ValueError,"encoder failure"):
                        recorder.stop()
                self.assertTrue(original.exists())
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertEqual(-9,receipt["encoder_exit"])
                self.assertEqual(digest(original),receipt["sha256"])

    def test_missing_recorder_readiness_is_not_a_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                fake=root/"ffmpeg"; fake.write_text("#!/bin/sh\necho test-only encoder startup failure >&2\nexit 7\n")
                fake.chmod(0o755); client.env["PATH"]=str(root)+":"+client.env["PATH"]
                original=root/"absent.mp4"; recorder=Recording(client,original,1)
                recorder.process.wait(timeout=3)
                self.assertFalse(recorder.ready.is_set())
                with self.assertRaises(ValueError): recorder.stop()
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertEqual(7,receipt["encoder_exit"]); self.assertIsNone(receipt["sha256"])

    def test_actual_unresponsive_encoder_has_bounded_failed_drain(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                fake=root/"ffmpeg"
                fake.write_text("#!/usr/bin/python3\nimport pathlib,signal,sys,time\n"
                    "signal.signal(signal.SIGINT,signal.SIG_IGN)\n"
                    "pathlib.Path(sys.argv[-1]).write_bytes(b'test-only partial output')\n"
                    "print('frame=1',flush=True)\nwhile True: time.sleep(1)\n")
                fake.chmod(0o755); client.env["PATH"]=str(root)+":"+client.env["PATH"]
                original=root/"forced.mp4"; recorder=Recording(client,original,30)
                self.assertTrue(recorder.ready.wait(3))
                started=time.monotonic()
                with self.assertRaisesRegex(ValueError,"drain deadline"): recorder.stop()
                self.assertLess(time.monotonic()-started,13)
                self.assertIsNotNone(recorder.process.returncode)
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertTrue(receipt["forced_stop"])
                self.assertEqual("finalization",receipt["stop_reason"])
                self.assertIsNone(receipt["stop_requests"][0]["process_returncode_at_request"])
                self.assertTrue(receipt["stop_requests"][0]["sigint_requested"])
                self.assertEqual(digest(original),receipt["sha256"])


if __name__=="__main__": unittest.main()
