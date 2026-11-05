#!/usr/bin/env python3
"""Synthetic metadata/media test inputs, explicitly not historical capture receipts."""
import copy
from fractions import Fraction
import io
import json
from pathlib import Path
import re
import subprocess
import struct
import sys
import tempfile
import time
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest
from tools.native_visual.media import validate, index, inspect
from tools.native_visual.mp4_timing import recover, tables
from tools.native_visual.recording import Recording
from tools.native_visual.controller import Scheduler
from NativeVisualToolsTests import native_window
from NativeVisualViewerFixture import create_vfr_b_frames


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


class SourceTrackTimingTests(unittest.TestCase):
    """Small synthetic timing tables, not encoded content/acquisition receipts."""
    def fixture(self, root, *, cap=0, edit_length=400, rate=1, edit_count=1):
        def box(kind, body): return struct.pack('>I4s',len(body)+8,kind)+body
        def integers(*values): return struct.pack('>'+'I'*len(values),*values)
        full=bytes(4)
        durations=[66000,68000,66000,67000,66000,67000]
        dts=[0,66000,134000,200000,267000,333000]
        pts=[0,200000,66000,134000,333000,267000]
        shift=134000
        offsets=[p+shift-d for p,d in zip(pts,dts)]
        ftyp=box(b'ftyp',b'isom'+integers(512)+b'isomiso2avc1')
        mdat=box(b'mdat',bytes(60))
        stts=box(b'stts',full+integers(6)+b''.join(integers(1,d) for d in durations))
        ctts=box(b'ctts',full+integers(6)+b''.join(integers(1,d) for d in offsets))
        stsd=box(b'stsd',full+integers(1)+box(b'avc1',bytes(6)+b'\0\1'+bytes(70)))
        stsz=box(b'stsz',full+integers(10,6))
        dref=box(b'dref',full+integers(1)+box(b'url ',b'\0\0\0\1'))
        minf=box(b'minf',box(b'dinf',dref)+box(b'stbl',stsd+stts+ctts+stsz))
        mdhd=box(b'mdhd',full+integers(0,0,1000000,400000+cap)+bytes(4))
        hdlr=box(b'hdlr',full+integers(0)+b'vide'+bytes(12))
        elst=box(b'elst',full+integers(edit_count,edit_length,shift)+struct.pack('>hh',rate,0))
        tkhd=box(b'tkhd',full+integers(0,0,1,0,400)+bytes(60))
        trak=box(b'trak',tkhd+box(b'edts',elst)+box(b'mdia',mdhd+hdlr+minf))
        mvhd=box(b'mvhd',full+integers(0,0,1000,400)+bytes(80))
        path=root/'timing.mp4';path.write_bytes(ftyp+mdat+box(b'moov',mvhd+trak))
        stream=dict(index=0,id='0x1',codec_name='h264',time_base='1/1000000',
                    start_pts=0,duration_ts=400000,nb_frames='6',width=32,height=24)
        packets=[dict(stream_index=0,pts=p,dts=d-shift,pos=str(len(ftyp)+8+i*10),
                      size='10',flags='K__' if i==0 else '___') for i,(p,d) in enumerate(zip(pts,dts))]
        normal=dict(streams=[stream],packets=packets)
        raw=copy.deepcopy(normal);raw['streams'][0]['start_pts']=shift
        for p in raw['packets']:p['pts']+=shift;p['dts']+=shift
        decoded=dict(streams=[copy.deepcopy(stream)],frames=[dict(pts=p,width=32,height=24) for p in sorted(pts)],
                     format=dict(duration='89.999500'))
        return path,decoded,normal,raw,durations

    def recover(self, fixture):
        source,decoded,normal,raw,_=fixture
        return recover(source,decoded,lambda args:json.dumps(raw if '-ignore_editlist' in args else normal).encode())

    def test_reordered_vfr_uses_source_decode_lifetimes_not_last_packet_or_format(self):
        with tempfile.TemporaryDirectory() as tmp:
            fixture=self.fixture(Path(tmp));source,decoded,normal,raw,_=fixture
            timing=self.recover(fixture)
            self.assertEqual([66000,66000,67000,68000,67000,66000],timing['durations'])
            result=validate(decoded,6,'2/5','15',source_timing=timing)
            self.assertEqual(Fraction(399,1000),Fraction(result['content_end']))
            for value in ('3','9000',None):
                decoded['format']['duration']=value
                self.assertEqual(result['content_end'],validate(decoded,6,'2/5','15',source_timing=self.recover(fixture))['content_end'])
            self.assertEqual(4,max(range(6),key=lambda i:normal['packets'][i]['pts']))

    def test_missing_conflicting_packet_mapping_and_source_metadata_reject(self):
        cases=('missing-dts','float-pts','order','count','map','flags','packet-duration','stream-cap','frame-duration')
        for case in cases:
            with self.subTest(case=case),tempfile.TemporaryDirectory() as tmp:
                fixture=self.fixture(Path(tmp));source,decoded,normal,raw,_=fixture
                if case=='missing-dts':raw['packets'][2].pop('dts')
                if case=='float-pts':normal['packets'][2]['pts']=66000.0
                if case=='order':raw['packets'][1],raw['packets'][2]=raw['packets'][2],raw['packets'][1]
                if case=='count':normal['packets'].pop()
                if case=='map':decoded['frames'][1]['pts']+=1
                if case=='flags':raw['packets'][1]['flags']=normal['packets'][1]['flags']='_D'
                if case=='packet-duration':normal['packets'][1]['duration']=68000
                if case=='stream-cap':raw['streams'][0]['duration_ts']-=1
                if case=='frame-duration':decoded['frames'][1]['duration']=1
                with self.assertRaises((ValueError,KeyError)):self.recover(fixture)

    def test_capped_mdhd_clipped_nonunit_multiple_edits_and_truncated_tables_reject(self):
        for options in (dict(cap=-1),dict(edit_length=200),dict(rate=0),dict(edit_count=2)):
            with self.subTest(options=options),tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(ValueError):self.recover(self.fixture(Path(tmp),**options))
        with tempfile.TemporaryDirectory() as tmp:
            fixture=self.fixture(Path(tmp));source=fixture[0];source.write_bytes(source.read_bytes()[:-1])
            with self.assertRaises(ValueError):self.recover(fixture)

    def test_longer_mdhd_does_not_replace_stts_or_presentation_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            fixture=self.fixture(Path(tmp),cap=134000)
            timing=self.recover(fixture)
            self.assertEqual(400000,timing['stts_sum'])
            self.assertEqual(534000,timing['mdhd_duration'])
            self.assertEqual('399/1000',validate(fixture[1],6,'2/5','15',source_timing=timing)['content_end'])


class DecoderTests(unittest.TestCase):
    def create(self,path,frames=6):
        subprocess.run(["ffmpeg","-nostdin","-v","error","-f","lavfi","-i","testsrc=size=32x24:rate=15",
                        "-frames:v",str(frames),"-c:v","libx264rgb","-crf","0","-threads","1",
                        "-video_track_timescale","1000000",str(path)],check=True,capture_output=True,timeout=30)

    def create_vfr_b_frames(self,path):
        return create_vfr_b_frames(path)

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
            with self.assertRaisesRegex(ValueError,"source PTS gap"):
                index(source,root/"rejected",expected_frames=6,seconds="2/5",rate="15",
                      candidate="a"*40,provenance="Synthetic B-frame VFR regression only",expected_sha=digest(source))
            assessment=json.loads((root/"rejected/assessment.json").read_text())
            self.assertTrue(assessment["decode_valid"])
            self.assertFalse(assessment["valid"])
            self.assertEqual("qualification",assessment["failure_stage"])
            self.assertTrue(assessment["qualification"]["count"]["valid"])
            self.assertFalse(assessment["qualification"]["cadence"]["valid"])
            self.assertFalse(assessment["qualification"]["boundary"]["valid"])
            self.assertEqual("known",assessment["qualification"]["boundary"]["status"])
            self.assertEqual(6,assessment["qualification"]["boundary"]["source_timing"]["sample_count"])
            self.assertTrue(assessment["input_unchanged"])
            self.assertEqual(before,source.read_bytes())

    def test_explicit_inspection_preserves_rejections_source_lifetimes_hashes_and_receipt(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"vfr.mp4"; pts=self.create_vfr_b_frames(source)
            before=source.read_bytes(); receipt=root/"old.json"; receipt.write_text('{"valid":false,"historical":true}')
            options=dict(expected_frames=9,seconds="3/5",rate="15",candidate="a"*40,
                         provenance=str(receipt),expected_sha=digest(source))
            with self.assertRaises(ValueError): index(source,root/"strict",**options)
            index(source,root/"inspection",inspection_only=True,**options)
            strict=json.loads((root/"strict/assessment.json").read_text())
            data=json.loads((root/"inspection/index.json").read_text())
            assessment=json.loads((root/"inspection/assessment.json").read_text())
            for item in (data,assessment):
                self.assertFalse(item["valid"]); self.assertFalse(item["qualification_valid"])
                self.assertTrue(item["inspection_only"]); self.assertTrue(item["inspection_available"])
                self.assertTrue(item["decode_valid"])
                self.assertEqual(strict["error"],item["qualification_error"])
                self.assertEqual(strict["qualification"],item["qualification"])
                self.assertEqual(9,item["qualification"]["count"]["expected_frames"])
                self.assertEqual(6,item["qualification"]["count"]["decoded_frames"])
                self.assertFalse(item["qualification"]["boundary"]["valid"])
                self.assertEqual("known",item["qualification"]["boundary"]["status"])
                self.assertEqual(2,item["media_index_version"])
            self.assertEqual(pts,[frame["pts"] for frame in data["frames"]])
            self.assertEqual(list(range(6)),[frame["ordinal"] for frame in data["frames"]])
            self.assertEqual({"frame_count":9,"seconds":"3/5","rate":"15"},data["validation"]["requested"])
            self.assertIsNotNone(data["validation"]["content_end"])
            self.assertIsInstance(data["frames"][-1]["duration"],int)
            self.assertGreater(data["frames"][-1]["duration"],0)
            self.assertEqual([None,None,None,None,112367,None],[frame["gap_before_ticks"] for frame in data["frames"]])
            for frame in data["frames"]: self.assertEqual(frame["sha256"],digest(root/"inspection"/frame["path"]))
            self.assertFalse(strict["inspection_available"])
            self.assertFalse((root/"strict/index.json").exists())
            self.assertTrue(assessment["input_unchanged"])
            self.assertEqual(before,source.read_bytes())
            self.assertEqual('{"valid":false,"historical":true}',receipt.read_text())
            with self.assertRaises(FileExistsError): index(source,root/"inspection",inspection_only=True,**options)

    def test_inspection_does_not_allow_corrupt_truncated_or_wrong_hash_originals(self):
        for case in ("corrupt","truncated","hash"):
            with self.subTest(case=case),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp); source=root/"original.mp4"; self.create(source)
                if case=="corrupt": source.write_bytes(b"not a video")
                if case=="truncated": source.write_bytes(source.read_bytes()[:400])
                before=source.read_bytes()
                with self.assertRaises(ValueError):
                    index(source,root/"inspection",inspection_only=True,expected_frames=6,seconds="2/5",rate="15",
                          candidate="a"*40,provenance="Synthetic only",expected_sha="0"*64 if case=="hash" else digest(source))
                self.assertFalse((root/"inspection/index.json").exists())
                if case!="hash":
                    assessment=json.loads((root/"inspection/assessment.json").read_text())
                    self.assertFalse(assessment["inspection_available"])
                    self.assertIsNot(assessment["decode_valid"],True)
                self.assertEqual(before,source.read_bytes())

    def test_inspection_refuses_missing_noninteger_nonmonotonic_and_inconsistent_mappings(self):
        for case in ("missing","string","float","unsafe","equal","backward","count","timebase"):
            with self.subTest(case=case):
                probe=probe90()
                if case=="missing": del probe["frames"][1]["pts"]
                if case=="string": probe["frames"][1]["pts"]="66666"
                if case=="float": probe["frames"][1]["pts"]=66666.5
                if case=="unsafe": probe["frames"][1]["pts"]=2**53
                if case=="equal": probe["frames"][1]["pts"]=0
                if case=="backward": probe["frames"][1]["pts"]=-1
                if case=="count": probe["streams"][0]["nb_frames"]="1349"
                if case=="timebase": probe["streams"][0]["time_base"]="1/9007199254740992"
                with mock.patch("tools.native_visual.media.command",side_effect=[json.dumps(probe).encode(),b""]):
                    with self.assertRaises(ValueError): inspect(Path("synthetic.mp4"),1350,"90","15",inspection_only=True)

    def test_cli_distinguishes_qualified_index_nonqualifying_inspection_and_fatal_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; self.create(source)
            for name,frames,flag,code in (("qualified",6,False,0),("strict",9,False,1),("inspection",9,True,2)):
                args=[sys.executable,"-B","-m","tools.native_visual.media",str(source),str(root/name),
                      "--sha256",digest(source),"--candidate","a"*40,"--provenance","Synthetic only",
                      "--frames",str(frames),"--seconds","2/5" if frames==6 else "3/5"]
                if flag: args.append("--inspection-only")
                result=subprocess.run(args,capture_output=True,text=True,timeout=30)
                self.assertEqual(code,result.returncode,result.stderr)
                if code!=1:
                    status=json.loads(result.stdout)
                    self.assertEqual("inspection_available",status["operation"])
                    self.assertTrue(status["inspection_available"])
                    self.assertEqual(code==0,status["qualification_valid"])


class ExportBudgetTests(unittest.TestCase):
    def options(self,source):
        return dict(expected_frames=6,seconds="2/5",rate="15",candidate="a"*40,
                    provenance="Synthetic export-budget regression only",expected_sha=digest(source))

    def test_invalid_budgets_fail_before_probe_or_output_creation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; DecoderTests.create(self,source)
            for number,budget in enumerate((0,-1,1800.01,10**1000,float("inf"),float("-inf"),float("nan"),None,True,"600")):
                with self.subTest(budget=budget),mock.patch("tools.native_visual.media.command") as command:
                    output=root/str(number)
                    with self.assertRaisesRegex(ValueError,"PNG export timeout must be finite"):
                        index(source,output,export_timeout=budget,**self.options(source))
                    command.assert_not_called()
                    self.assertFalse(output.exists())

    def test_default_decimal_and_upper_bound_reach_only_png_export_phase(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; DecoderTests.create(self,source)
            for number,budget in enumerate((None,1.25,600,1800)):
                with self.subTest(budget=budget),mock.patch("tools.native_visual.media.subprocess.run",wraps=subprocess.run) as run:
                    output=root/str(number)
                    index(source,output,**({} if budget is None else {"export_timeout":budget}),**self.options(source))
                    expected=180 if budget is None else budget
                    self.assertEqual([180,180,expected],[call.kwargs["timeout"] for call in run.call_args_list])
                    self.assertEqual("ffprobe",run.call_args_list[0].args[0][0])
                    self.assertEqual("-",run.call_args_list[1].args[0][-1])
                    self.assertTrue(run.call_args_list[2].args[0][-1].endswith("%06d.png"))
                    for name in ("index.json","assessment.json"):
                        receipt=json.loads((output/name).read_text())
                        self.assertEqual(expected,receipt["png_export_timeout_seconds"])
                        self.assertTrue(receipt["valid"])

    def test_export_timeout_is_fatal_once_with_receipt_and_immutable_original(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; DecoderTests.create(self,source)
            before=source.read_bytes(); actual_run=subprocess.run
            def deadline(args,**kwargs):
                if args[-1].endswith("%06d.png"):
                    raise subprocess.TimeoutExpired(args,kwargs["timeout"])
                return actual_run(args,**kwargs)
            with mock.patch("tools.native_visual.media.subprocess.run",side_effect=deadline) as run:
                with self.assertRaises(subprocess.TimeoutExpired) as caught:
                    index(source,root/"failed",export_timeout=600,**self.options(source))
                self.assertEqual(600,caught.exception.timeout)
                self.assertEqual(3,run.call_count)  # probe, decode, one export attempt; no retry.
            receipt=json.loads((root/"failed/assessment.json").read_text())
            self.assertEqual(600,receipt["png_export_timeout_seconds"])
            self.assertEqual("index",receipt["failure_stage"])
            self.assertFalse(receipt["inspection_available"])
            self.assertFalse(receipt["valid"])
            self.assertTrue(receipt["decode_valid"])
            self.assertTrue(receipt["input_unchanged"])
            self.assertEqual(before,source.read_bytes())

    def test_cli_accepts_explicit_budget_and_refuses_nonfinite_or_out_of_bounds(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); source=root/"original.mp4"; DecoderTests.create(self,source)
            for number,budget in enumerate(("600","nan","inf","0","1801")):
                output=root/str(number)
                args=[sys.executable,"-B","-m","tools.native_visual.media",str(source),str(output),
                      "--sha256",digest(source),"--candidate","a"*40,"--provenance","Synthetic only",
                      "--frames","6","--seconds","2/5","--export-timeout",budget]
                result=subprocess.run(args,capture_output=True,text=True,timeout=30)
                self.assertEqual(0 if budget=="600" else 1,result.returncode,result.stderr)
                if budget=="600":
                    self.assertTrue(json.loads(result.stdout)["qualification_valid"])
                    self.assertEqual(600,json.loads((output/"assessment.json").read_text())["png_export_timeout_seconds"])
                else:
                    self.assertIn("PNG export timeout must be finite",result.stderr)
                    self.assertFalse(output.exists())


class RecordingTests(unittest.TestCase):
    def recording_argv(self, path, **options):
        """Exercise production argv/finalization without a display or real encoder."""
        client=mock.Mock(env={"DISPLAY":":argv-fixture"})
        client.canvas.width,client.canvas.height=32,24
        process=mock.Mock(stdout=io.StringIO('frame=6\ndup_frames=0\ndrop_frames=0\n'))
        process.poll.return_value=process.wait.return_value=0
        def start(*args,**kwargs):
            path.write_bytes(b'Test-only argv fixture, not encoded media')
            return process
        with mock.patch('tools.native_visual.recording.subprocess.Popen',side_effect=start) as launch:
            recorder=Recording(client,path,.4,**options)
        recorder.stop()
        client.verify_geometry.assert_called_once_with()
        return launch.call_args.args[0],json.loads(path.with_suffix('.recording.json').read_text())

    def test_capped_bframes_argv_receipt_and_exact_uncapped_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name,options in (('uncapped',{}),('capped',dict(frames=6))):
                with self.subTest(name=name):
                    path=Path(tmp)/(name+'.mp4')
                    args,receipt=self.recording_argv(path,**options)
                    expected=['ffmpeg','-nostdin','-n','-v','warning','-progress','pipe:1','-stats_period','0.1',
                              '-f','x11grab','-draw_mouse','0','-video_size','32x24','-framerate','15',
                              '-i',':argv-fixture','-an','-c:v','libx264rgb','-crf','18','-preset','veryfast',
                              '-fps_mode','passthrough','-enc_time_base','1/1000000',
                              '-video_track_timescale','1000000','-t','0.4',
                              *(['-frames:v','6'] if options else []),str(path)]
                    if options:
                        bf=args.index('-bf')
                        self.assertEqual(['-bf','0'],args[bf:bf+2])
                        self.assertEqual(expected,args[:bf]+args[bf+2:])
                    else:self.assertEqual(expected,args)
                    self.assertEqual(args,receipt['acquisition_args'])
                    self.assertEqual(6 if options else None,receipt['requested_contract']['frame_limit'])

    def test_capped_offline_irregular_pts_has_strict_whole_sample_boundary(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);source=root/'synthetic-capped.mp4'
            production_args,_=self.recording_argv(root/'argv-only.mp4',frames=6)
            pts=[0,66000,134000,200000,267000,333000]
            expression=str(pts[-1])
            for ordinal,value in reversed(list(enumerate(pts[:-1]))):
                expression=f'if(eq(N,{ordinal}),{value},{expression})'
            # Reuse production output options; substitute only a finite synthetic
            # source for x11grab. showinfo proves its actual pre-encoder timestamps.
            args=production_args[:production_args.index('-f')]+[
                '-v','info','-f','lavfi','-i','testsrc=size=32x24:rate=15:duration=0.4',
                '-vf',f"settb=expr=1/1000000,setpts='{expression}',showinfo",
            ]+production_args[production_args.index('-an'):-1]+[str(source)]
            encoded=subprocess.run(args,capture_output=True,text=True,timeout=30)
            (root/'synthetic.encoder.log').write_text(encoded.stderr)
            (root/'synthetic.progress.log').write_text(encoded.stdout)
            (root/'synthetic-source.json').write_text(json.dumps(dict(
                synthetic_only=True,production_args=production_args,actual_args=args,
                input_pts=pts,input_time_base='1/1000000',input_filter_expression=expression),indent=2))
            self.assertEqual(0,encoded.returncode,encoded.stderr)
            observed=[int(p) for p in re.findall(r'\bn:\s*\d+\s+pts:\s*(-?\d+)\s+pts_time:',encoded.stderr)]
            self.assertEqual(pts,observed)
            probes={}
            for mode,extra in (('normal',[]),('raw',['-ignore_editlist','1'])):
                probe=json.loads(subprocess.check_output(['ffprobe','-v','error',*extra,
                    '-select_streams','v:0','-show_streams','-show_packets','-show_entries',
                    'stream=index,id,codec_name,width,height,time_base,start_pts,duration_ts,nb_frames,has_b_frames:'
                    'packet=stream_index,pts,dts,duration,pos,size,flags','-of','json',str(source)],timeout=15))
                probes[mode]=probe
                (root/('synthetic-'+mode+'.json')).write_text(json.dumps(probe,indent=2))
                self.assertEqual(0,probe['streams'][0]['has_b_frames'])
                self.assertEqual(pts,[p['pts'] for p in probe['packets']])
                self.assertEqual(pts,[p['dts'] for p in probe['packets']])
                self.assertTrue(all('D' not in p['flags'] for p in probe['packets']))
            self.assertEqual(probes['normal']['packets'],probes['raw']['packets'])
            track=tables(source)
            (root/'synthetic-tables.json').write_text(json.dumps(track,indent=2,default=str))
            self.assertTrue(all(c==0 for c in track['offsets']))
            self.assertEqual([b-a for a,b in zip(pts,pts[1:])],track['durations'][:-1])
            self.assertEqual(track['duration'],track['mdhd_duration'])
            for probe in probes.values():
                self.assertEqual(track['duration'],probe['streams'][0]['duration_ts'])
                self.assertEqual(track['durations'],[p['duration'] for p in probe['packets']])
            self.assertIsNotNone(track['edit'])
            media_start,edit_length=track['edit']
            self.assertEqual(0,media_start)
            self.assertTrue(all(0<=p and p+d<=edit_length for p,d in zip(pts,track['durations'])))
            before=digest(source);diagnostics={}
            try:result=inspect(source,6,Fraction(2,5),Fraction(15),diagnostics)
            finally:
                (root/'synthetic-qualification.json').write_text(json.dumps(diagnostics,indent=2))
            self.assertEqual(before,digest(source))
            self.assertTrue(diagnostics['decode_valid'])
            self.assertTrue(diagnostics['qualification_valid'])
            self.assertTrue(all(v['valid'] for v in diagnostics['qualification'].values()))
            self.assertEqual(pts,result['source_pts'])
            self.assertEqual(track['durations'],result['source_durations'])
            self.assertEqual(Fraction(pts[-1]+track['durations'][-1],track['scale']),
                             Fraction(result['content_end']))
            (root/'synthetic-result.json').write_text(json.dumps(dict(sha256=before,validation=result),indent=2))
            self.assertFalse(list(root.rglob('*.png')))

    def test_invalid_explicit_caps_fail_before_geometry_or_encoder_launch(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"absent.mp4"
            for cap in (None,True,False,0,-1,6.0,"6",9001,5,7):
                with self.subTest(cap=cap),mock.patch("tools.native_visual.recording.subprocess.Popen") as launch:
                    client=mock.Mock()
                    with self.assertRaisesRegex(ValueError,"recording frames"):
                        Recording(client,path,.4,frames=cap)
                    client.verify_geometry.assert_not_called();launch.assert_not_called()
                    self.assertFalse(path.exists())
            client=mock.Mock();client.verify_geometry.side_effect=RuntimeError("test stop before launch")
            with self.assertRaisesRegex(RuntimeError,"test stop before launch"):
                Recording(client,path,600,frames=9000)

    def test_recipe_forwards_cap_only_when_explicit_and_cli_rejects_invalid_before_worker(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for number,extra in enumerate(({},dict(frames=6))):
                output=root/str(number);output.mkdir()
                with mock.patch("tools.native_visual.controller.Recording") as encoder:
                    encoder.return_value.ready.wait.return_value=True
                    encoder.return_value.process.poll.return_value=None
                    client=object()
                    Scheduler({"g":client},output,2).run([dict(client="g",action="record",clip="sample",seconds=.4,**extra)])
                    encoder.assert_called_once_with(client,output/"sample.mp4",.4,**extra)
                    encoder.return_value.stop.assert_called_once_with()
            from tools.native_visual.controller import main
            identity=root/"identity.json";identity.write_text('{}')
            recipe=root/"recipe.json";recipe.write_text(json.dumps(dict(steps=[dict(action="record",seconds=.4,frames=None)])))
            output=root/"cli-output"
            runtime=root/"runtime"
            with mock.patch.object(sys,"argv",["controller","--runtime",str(runtime),"--identity",str(identity),
                   "--recipe",str(recipe),"--output",str(output)]),mock.patch("tools.native_visual.controller.verify_runtime",return_value=runtime),\
                   mock.patch("tools.native_visual.controller.run_owned") as launch:
                with self.assertRaisesRegex(ValueError,"recording frames"):main()
                launch.assert_not_called();self.assertFalse(output.exists())

    def test_actual_x11_frame_cap_stops_at_requested_sample_without_timestamp_resampling(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                original=root/"capped.mp4"
                with mock.patch("tools.native_visual.recording.subprocess.Popen",wraps=subprocess.Popen) as launch:
                    recorder=Recording(client,original,.4,frames=6)
                try:
                    self.assertTrue(recorder.ready.wait(5))
                    self.assertEqual(0,recorder.process.wait(timeout=5))
                finally:recorder.stop()
                args=next(call.args[0] for call in launch.call_args_list if call.args[0][0]=="ffmpeg")
                self.assertEqual("6",args[args.index("-frames:v")+1])
                self.assertEqual("0.4",args[args.index("-t")+1])
                self.assertEqual("passthrough",args[args.index("-fps_mode")+1])
                self.assertEqual("1/1000000",args[args.index("-enc_time_base")+1])
                self.assertNotIn("-r",args);self.assertNotIn("-vf",args)
                probe=json.loads(subprocess.check_output(["ffprobe","-v","error","-select_streams","v",
                    "-show_frames","-show_entries","frame=pts,width,height","-of","json",str(original)],timeout=10))
                frames=probe["frames"];self.assertEqual(6,len(frames))
                self.assertTrue(all((f["width"],f["height"])==(850,700) for f in frames))
                self.assertTrue(all(b["pts"]>a["pts"] for a,b in zip(frames,frames[1:])))
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertEqual(6,receipt["requested_contract"]["frame_limit"])
                self.assertEqual(.4,receipt["requested_contract"]["duration_limit_seconds"])
                self.assertEqual(6,receipt["encoder_progress"]["frame"])
                self.assertIsNone(receipt["stop_reason"]);self.assertFalse(receipt["forced_stop"])

    def test_actual_recorder_readiness_drain_and_immutable_finalization(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with native_window(root) as client:
                original=root/"original.mp4"
                with mock.patch("tools.native_visual.recording.subprocess.Popen",wraps=subprocess.Popen) as launch:
                    recorder=Recording(client,original,.4)
                args=next(call.args[0] for call in launch.call_args_list if call.args[0][0]=="ffmpeg")
                self.assertNotIn("-frames:v",args)
                self.assertEqual("0.4",args[args.index("-t")+1])
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
                original=root/"forced.mp4"; recorder=Recording(client,original,30,frames=450)
                self.assertTrue(recorder.ready.wait(3))
                started=time.monotonic()
                with self.assertRaisesRegex(ValueError,"drain deadline"): recorder.stop()
                self.assertLess(time.monotonic()-started,13)
                self.assertIsNotNone(recorder.process.returncode)
                receipt=json.loads(original.with_suffix(".recording.json").read_text())
                self.assertTrue(receipt["forced_stop"])
                self.assertEqual(450,receipt["requested_contract"]["frame_limit"])
                self.assertEqual("finalization",receipt["stop_reason"])
                self.assertIsNone(receipt["stop_requests"][0]["process_returncode_at_request"])
                self.assertTrue(receipt["stop_requests"][0]["sigint_requested"])
                self.assertEqual(digest(original),receipt["sha256"])


if __name__=="__main__": unittest.main()
