"""Additive offline validation and source-ordinal frame indexing. No input edits."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import shutil
import subprocess

from .common import digest, record, require, sha


def validate(probe, expected_frames, seconds, rate):
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
    require(len(frames) == expected_frames, "decoded frame count deficit or surplus")
    if stream.get("nb_frames", "N/A") != "N/A":
        require(int(stream["nb_frames"]) == expected_frames, "stream/decoded frame count mismatch")
    pts, durations = [], []
    for frame in frames:
        require((int(frame["width"]), int(frame["height"])) == (width, height), "frame dimensions changed")
        pts.append(int(frame["pts"]))
        duration = frame.get("duration", frame.get("pkt_duration"))
        require(duration is not None and int(duration) > 0, "missing actual frame duration")
        durations.append(int(duration))
    interval = 1 / rate
    tolerance = interval / 2 + base  # Strictly sub-frame; never excuse a missing frame.
    require(tolerance < interval, "time base too coarse for sub-frame validation")
    require(abs(pts[0] * base) <= tolerance, "late/missing recording start")
    gaps = [(b - a) * base for a, b in zip(pts, pts[1:])]
    require(all(0 < gap <= interval * Fraction(3, 2) for gap in gaps),
            "nonmonotonic timestamps or source PTS gap")
    require(all(0 < duration * base <= interval * Fraction(3, 2) for duration in durations),
            "invalid frame duration or content gap")
    # Large duration/PTS inconsistencies cannot be disguised by a valid last timestamp.
    require(all(abs((pts[i] + durations[i] - pts[i + 1]) * base) <= tolerance
                for i in range(len(pts) - 1)), "uncovered/overlapping source content boundary")
    end = (pts[-1] + durations[-1]) * base
    require(abs(end - seconds) <= tolerance, "short or excess actual content coverage")
    return {"width": width, "height": height, "time_base": str(base),
            "source_pts": pts, "source_durations": durations, "frame_count": len(frames),
            "content_end": str(end), "subframe_tolerance": str(tolerance),
            "max_gap_seconds": float(interval * Fraction(3, 2)),
            "max_pts_gap": str(max(gaps, default=Fraction(0))),
            "container_duration": probe.get("format", {}).get("duration"),
            "limits": "Structural validation only; frame sampling is not display-update or visual conformance proof."}


def command(args, timeout=180):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    require(result.returncode == 0 and not result.stderr,
            "decode/probe error: " + result.stderr.decode(errors="replace")[-2000:])
    return result.stdout


def inspect(source, expected_frames, seconds, rate):
    probe = json.loads(command([
        "ffprobe", "-v", "error", "-select_streams", "v", "-show_streams", "-show_frames",
        "-show_format", "-show_entries",
        "stream=width,height,time_base,nb_frames:frame=pts,pkt_duration,duration,width,height:format=duration",
        "-of", "json", str(source),
    ]))
    result = validate(probe, expected_frames, seconds, rate)
    command(["ffmpeg", "-v", "error", "-xerror", "-i", str(source), "-map", "0:v:0",
             "-an", "-f", "null", "-"])
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
    try:
        result = inspect(source, expected_frames, seconds, rate)
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
        record(output / "assessment.json", {"original": original, "valid": True})
        return result
    except Exception as error:
        record(output / "assessment.json", {"original": original, "valid": False,
               "error": str(error), "input_unchanged": digest(source) == expected_sha})
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
