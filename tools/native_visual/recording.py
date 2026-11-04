"""Full-display original recording; finalization/probes never block scheduled input."""
import signal
import subprocess
import threading
import time
from datetime import datetime, timezone

from .common import digest, record, require


class Recording:
    def __init__(self, client, path, seconds):
        require(0 < seconds <= 600, "recording duration must be bounded")
        require(not path.exists(), "never overwrite an original recording")
        self.path, self.seconds = path, seconds
        self.started = time.monotonic()
        self.utc = datetime.now(timezone.utc).isoformat()
        self.reason = "requested-duration"
        self.ready = threading.Event()
        self.log = path.with_suffix(".encoder.log").open("xb")
        self.process = subprocess.Popen([
            "ffmpeg", "-nostdin", "-n", "-v", "warning", "-progress", "pipe:1", "-stats_period", "0.1",
            "-f", "x11grab", "-draw_mouse", "0",
            "-video_size", f"{client.canvas.width}x{client.canvas.height}", "-framerate", "15",
            "-i", client.env["DISPLAY"], "-an", "-c:v", "libx264rgb", "-crf", "18",
            "-preset", "veryfast", "-fps_mode", "passthrough", "-enc_time_base", "1/1000000",
            "-video_track_timescale", "1000000", "-t", str(seconds), str(path),
        ], env=client.env, stdout=subprocess.PIPE, stderr=self.log, text=True)
        self.cancel = threading.Event()
        self.reader = threading.Thread(target=self.progress, daemon=True)
        self.reader.start()
        self.thread = threading.Thread(target=self.watch, daemon=True)
        self.thread.start()

    def progress(self):
        for line in self.process.stdout:
            if line.startswith("frame=") and int(line.split("=", 1)[1]) > 0:
                self.ready.set()

    def watch(self):
        while not self.cancel.wait(.05) and self.process.poll() is None:
            if self.path.exists() and self.path.stat().st_size >= 80000000:
                self.reason = "byte-threshold"
            elif time.monotonic() - self.started >= self.seconds + 10:
                self.reason = "wall-deadline"
            else:
                continue
            self.request_stop(self.reason)
            return

    def request_stop(self, reason="recipe-stop"):
        self.reason = reason
        self.cancel.set()
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)

    def stop(self):
        self.cancel.set()
        self.thread.join(timeout=1)
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
        try:
            code = self.process.wait(timeout=10)
        finally:
            self.log.close()
        self.reader.join(timeout=1)
        record(self.path.with_suffix(".recording.json"), {
            "original": str(self.path), "sha256": digest(self.path) if self.path.exists() else None,
            "started_monotonic": self.started, "finalized_monotonic": time.monotonic(),
            "started_utc": self.utc,
            "stop_reason": self.reason, "encoder_exit": code,
            "codec": "libx264rgb", "crf": 18, "preset": "veryfast", "nominal_rate": "15",
            "capture": "full-display x11grab, lossy RGB, passthrough timestamps, no audio",
            "validation": "pending; run additive offline media validation separately",
        })
        require(self.path.exists() and self.path.stat().st_size < 104857600,
                "missing/oversized original; preserve it but do not claim eligible coverage")
        require(code in (0, 255), "encoder failure; preserve original and encoder log")
