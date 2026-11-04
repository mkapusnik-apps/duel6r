#!/usr/bin/env python3
"""Serve synthetic viewer cases for direct permitted browser testing. No game runs."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.native_visual.common import digest
from tools.native_visual.media import index


def prepare(root):
    root.mkdir(parents=True, exist_ok=False)
    original = root / "test-only-original.mp4"
    subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-f", "lavfi", "-i",
                    "testsrc=size=1280x900:rate=15", "-frames:v", "6", "-c:v", "libx264rgb",
                    "-crf", "0", "-threads", "1", "-video_track_timescale", "1000000", str(original)],
                   check=True, capture_output=True, timeout=30)
    index(original, root / "good", expected_frames=6, seconds="2/5", rate="15", candidate="0"*40,
          provenance="Synthetic viewer regression fixture; NOT application evidence or historical provenance",
          expected_sha=digest(original))
    for case in ("missing", "hash", "decode", "gap", "slow"):
        shutil.copytree(root / "good", root / case)
    (root / "missing/frames/000001.png").unlink()
    path = root / "hash/frames/000001.png"
    path.write_bytes(path.read_bytes() + b"test-only hash corruption")
    path = root / "decode/frames/000001.png"
    path.write_bytes(b"not a PNG")
    mapping = root / "decode/index.json"
    data = json.loads(mapping.read_text()); data["frames"][1]["sha256"] = digest(path)
    mapping.write_text(json.dumps(data))
    path = root / "gap/index.json"
    data = json.loads(path.read_text()); data["frames"][1]["pts"] = data["frames"][0]["pts"]
    path.write_text(json.dumps(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    prepare(args.root)

    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *items, **kwargs):
            super().__init__(*items, directory=str(args.root), **kwargs)

        def do_GET(self):
            if self.path == "/slow/frames/000002.png":
                time.sleep(.25)  # Controlled derivative delivery stall, never a game clock pause.
            super().do_GET()

    print("SYNTHETIC_VIEWER_FIXTURE_READY", flush=True)
    ThreadingHTTPServer(("0.0.0.0", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
