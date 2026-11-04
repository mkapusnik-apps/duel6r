#!/usr/bin/env python3
"""Synthetic metadata/media test inputs, explicitly not historical capture receipts."""
import copy
from fractions import Fraction
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest
from tools.native_visual.media import validate, index


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


class DecoderTests(unittest.TestCase):
    def create(self,path,frames=6):
        subprocess.run(["ffmpeg","-nostdin","-v","error","-f","lavfi","-i","testsrc=size=32x24:rate=15",
                        "-frames:v",str(frames),"-c:v","libx264rgb","-crf","0","-threads","1",
                        "-video_track_timescale","1000000",str(path)],check=True,capture_output=True,timeout=30)

    def test_real_full_decode_exact_ordinal_mapping_and_immutable_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; self.create(source)
            before=source.read_bytes(); receipt=root/"historical.json"; receipt.write_text('{"failed":"old rule"}')
            result=index(source,root/"inspection",expected_frames=6,seconds="2/5",rate="15",
                         candidate="a"*40,provenance=str(receipt),expected_sha=digest(source))
            data=json.loads((root/"inspection/index.json").read_text())
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
                self.assertFalse(json.loads((root/"rejected/assessment.json").read_text())["valid"])


if __name__=="__main__": unittest.main()
