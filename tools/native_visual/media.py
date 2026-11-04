"""Additive offline validation and source-ordinal frame indexing. No input edits."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import shutil
import subprocess

from .common import digest, record, require, sha


def validate(probe, expected_frames, seconds, rate, checks=None):
    """Integer source timestamps, not rounded container duration, own coverage."""
    seconds, rate = Fraction(seconds), Fraction(rate)
    require(0 < seconds <= 600 and 0 < rate <= 120, "invalid capture contract")
    require(expected_frames == seconds * rate and 0 < expected_frames <= 72000,
            "frame count contract disagrees with requested duration/rate")
    streams = probe["streams"]
    require(len(streams) == 1, "expected exactly one video stream")
    stream = streams[0]
    width, height = int(stream["width"]), int(stream["height"])
    require(width > 0 and height > 0, "invalid decoded dimensions")
    base = Fraction(stream["time_base"])
    require(base > 0, "invalid source time base")
    frames = probe["frames"]
    checks = checks if checks is not None else {}
    reported = stream.get("nb_frames", "N/A")
    count_errors = []
    if len(frames) != expected_frames:
        count_errors.append("decoded frame count deficit or surplus")
    if reported != "N/A" and int(reported) != expected_frames:
        count_errors.append("stream/decoded frame count mismatch")
    checks["count"] = {"valid": not count_errors, "expected_frames": expected_frames,
                       "decoded_frames": len(frames), "stream_frames": reported, "errors": count_errors}
    pts, durations = [], []
    for frame in frames:
        require((int(frame["width"]), int(frame["height"])) == (width, height), "frame dimensions changed")
        pts.append(int(frame["pts"]))
        duration = frame.get("duration", frame.get("pkt_duration"))
        durations.append(None if duration in (None, "N/A") else int(duration))
    interval = 1 / rate
    tolerance = interval / 2 + base  # Strictly sub-frame; never excuse a missing frame.
    require(tolerance < interval, "time base too coarse for sub-frame validation")
    gaps = [(b - a) * base for a, b in zip(pts, pts[1:])]
    cadence_valid = bool(pts) and all(0 < gap <= interval * Fraction(3, 2) for gap in gaps)
    checks["cadence"] = {"valid": cadence_valid, "max_gap_seconds": float(interval * Fraction(3, 2)),
                         "max_pts_gap": str(max(gaps, default=Fraction(0))),
                         "gaps_over_limit": sum(gap > interval * Fraction(3, 2) for gap in gaps),
                         "nonmonotonic_gaps": sum(gap <= 0 for gap in gaps)}
    boundary_errors = []
    missing = sum(duration is None for duration in durations)
    end = None
    if not pts or missing:
        boundary_errors.append("unsupported/unknown boundary: missing actual frame duration or frames")
        boundary_valid = None  # Unproven coverage, not a claim of decode corruption.
    else:
        if abs(pts[0] * base) > tolerance:
            boundary_errors.append("late/missing recording start")
        if not all(0 < duration * base <= interval * Fraction(3, 2) for duration in durations):
            boundary_errors.append("invalid frame duration or content gap")
        # Large duration/PTS inconsistencies cannot be disguised by a valid last timestamp.
        if not all(abs((pts[i] + durations[i] - pts[i + 1]) * base) <= tolerance
                   for i in range(len(pts) - 1)):
            boundary_errors.append("uncovered/overlapping source content boundary")
        end = (pts[-1] + durations[-1]) * base
        if abs(end - seconds) > tolerance:
            boundary_errors.append("short or excess actual content coverage")
        boundary_valid = not boundary_errors
    checks["boundary"] = {"valid": boundary_valid, "status": "unknown" if boundary_valid is None else "known",
                          "missing_duration_frames": missing, "content_end": None if end is None else str(end),
                          "subframe_tolerance": str(tolerance), "errors": boundary_errors}
    errors = count_errors + ([] if cadence_valid else ["nonmonotonic timestamps or source PTS gap"]) + boundary_errors
    require(not errors, "; ".join(errors))
    return {"width": width, "height": height, "time_base": f"{base.numerator}/{base.denominator}",
            "source_pts": pts, "source_durations": durations, "frame_count": len(frames),
            "content_end": str(end), "subframe_tolerance": str(tolerance),
            "max_gap_seconds": float(interval * Fraction(3, 2)),
            "max_pts_gap": str(max(gaps, default=Fraction(0))),
            "container_duration": probe.get("format", {}).get("duration"),
            "stream": {key: stream.get(key, "unavailable") for key in
                       ("codec_name", "pix_fmt", "r_frame_rate", "avg_frame_rate")},
            "limits": "Structural validation only; frame sampling is not display-update or visual conformance proof."}


def command(args, timeout=180):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    require(result.returncode == 0 and not result.stderr,
            "decode/probe error: " + result.stderr.decode(errors="replace")[-2000:])
    return result.stdout


def inspect(source, expected_frames, seconds, rate, diagnostics=None):
    diagnostics = diagnostics if diagnostics is not None else {}
    diagnostics.update(decode_valid=None, qualification={}, failure_stage="probe")
    probe = json.loads(command([
        "ffprobe", "-v", "error", "-select_streams", "v", "-show_streams", "-show_frames",
        "-show_format", "-show_entries",
        "stream=width,height,time_base,nb_frames,codec_name,pix_fmt,r_frame_rate,avg_frame_rate:"
        "frame=pts,pkt_duration,duration,width,height:format=duration",
        "-of", "json", str(source),
    ]))
    require(len(probe["streams"]) == 1, "expected exactly one video stream")
    base = Fraction(probe["streams"][0]["time_base"])
    require(base > 0, "invalid source time base")
    diagnostics["failure_stage"] = "decode"
    try:
        # A default null encoder timebase (1/average-fps) can merge distinct VFR
        # timestamps and report output DTS errors for a fully decodable original.
        command(["ffmpeg", "-v", "error", "-xerror", "-err_detect", "explode", "-i", str(source),
                 "-map", "0:v:0", "-an", "-fps_mode", "passthrough", "-enc_time_base", str(base),
                 "-f", "null", "-"])
    except ValueError:
        diagnostics["decode_valid"] = False
        raise
    diagnostics["decode_valid"] = True
    diagnostics["failure_stage"] = "qualification"
    result = validate(probe, expected_frames, seconds, rate, diagnostics["qualification"])
    diagnostics["failure_stage"] = None
    return result


def index(source, output, *, expected_frames, seconds, rate, candidate, provenance, expected_sha):
    source = Path(source).resolve(strict=True)
    sha(candidate, 40)
    sha(expected_sha)
    require(source.is_file() and digest(source) == expected_sha, "original media hash mismatch")
    require(source.stat().st_size < 104857600, "original exceeds 100 MiB; do not cut/reencode")
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    original = {"path": str(source), "sha256": expected_sha, "bytes": source.stat().st_size,
                "candidate": candidate, "provenance": provenance}
    diagnostics = {"decode_valid": None, "qualification": {}, "failure_stage": "probe"}
    try:
        result = inspect(source, expected_frames, seconds, rate, diagnostics)
        diagnostics["failure_stage"] = "index"
        frames = output / "frames"
        frames.mkdir()
        command(["ffmpeg", "-v", "error", "-xerror", "-i", str(source), "-map", "0:v:0",
                 "-an", "-fps_mode", "passthrough", "-threads", "1", "-start_number", "0", str(frames / "%06d.png")])
        paths = sorted(frames.glob("*.png"))
        require(len(paths) == expected_frames, "decode/index frame count mismatch")
        mapping = []
        for ordinal, path in enumerate(paths):
            require(path.name == f"{ordinal:06d}.png", "missing ordinal")
            mapping.append({"ordinal": ordinal, "pts": result["source_pts"][ordinal],
                            "duration": result["source_durations"][ordinal],
                            "path": path.relative_to(output).as_posix(), "sha256": digest(path)})
        require(digest(source) == expected_sha, "input changed during indexing")
        record(output / "index.json", {"original": original, "validation": result, "frames": mapping,
               "derivatives": "Full-frame PNG decodes for inspection, not original lossless screenshots."})
        shutil.copyfile(Path(__file__).with_name("viewer.html"), output / "viewer.html")
        diagnostics["failure_stage"] = None
        record(output / "assessment.json", {"original": original, "valid": True, **diagnostics})
        return result
    except Exception as error:
        record(output / "assessment.json", {"original": original, "valid": False,
                "error": str(error), "input_unchanged": digest(source) == expected_sha, **diagnostics})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--provenance", required=True, help="original receipt reference; unavailable must be explicit")
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--seconds", required=True)
    parser.add_argument("--rate", default="15")
    args = parser.parse_args()
    index(args.source, args.output, expected_frames=args.frames, seconds=args.seconds, rate=args.rate,
          candidate=args.candidate, provenance=args.provenance, expected_sha=args.sha256)


if __name__ == "__main__":
    main()
