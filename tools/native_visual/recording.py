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
        client.verify_geometry()
        self.path, self.seconds = path, seconds
        self.geometry = [client.canvas.width, client.canvas.height]
        self.display = client.env["DISPLAY"]
        self.started = time.monotonic()
        self.utc = datetime.now(timezone.utc).isoformat()
        self.reason = None
        self.stop_requests = []
        self.stop_lock = threading.Lock()
        self.ready = threading.Event()
        self.counters = {}
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
            key, _, value = line.strip().partition("=")
            if key in ("frame", "dup_frames", "drop_frames", "out_time_us"):
                # FFmpeg emits N/A until reordered packets have timestamps.
                # This is explicitly unavailable, not zero drops or a parse crash.
                self.counters[key] = None if value == "N/A" else int(value)
            if key == "frame" and value != "N/A" and int(value) > 0:
                self.ready.set()

    def watch(self):
        while not self.cancel.wait(.05) and self.process.poll() is None:
            if self.path.exists() and self.path.stat().st_size >= 80000000:
                reason = "byte-threshold"
            elif time.monotonic() - self.started >= self.seconds + 10:
                reason = "wall-deadline"
            else:
                continue
            self.request_stop(reason)
            return

    def request_stop(self, reason="recipe-stop"):
        self.cancel.set()
        with self.stop_lock:
            code = self.process.poll()
            observation = {"reason": reason, "requested_monotonic": time.monotonic(),
                           "requested_utc": datetime.now(timezone.utc).isoformat(),
                           "process_returncode_at_request": code, "sigint_requested": False}
            try:
                if code is None:
                    observation["sigint_requested"] = True
                    self.process.send_signal(signal.SIGINT)
                    if self.reason is None:
                        self.reason = reason
            finally:
                # This is an observed request/state, never proof of signal
                # delivery or which mechanism actually ended the encoder.
                self.stop_requests.append(observation)

    def stop(self):
        self.cancel.set()
        self.thread.join(timeout=1)
        before_finalize = self.process.poll()
        if before_finalize is None:
            self.request_stop("finalization")
        forced = False
        try:
            try:
                code = self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                forced = True
                self.process.kill()
                code = self.process.wait(timeout=2)
        finally:
            self.log.close()
        self.reader.join(timeout=1)
        if not self.reader.is_alive():
            self.process.stdout.close()
        record(self.path.with_suffix(".recording.json"), {
            "original": str(self.path), "sha256": digest(self.path) if self.path.exists() else None,
            "started_monotonic": self.started, "finalized_monotonic": time.monotonic(),
            "started_utc": self.utc,
            "stop_reason": self.reason, "encoder_exit": code,
            "stop_reason_semantics": "first SIGINT request reason, or null; not the encoder termination cause",
            "stop_requests": self.stop_requests, "process_returncode_before_finalization": before_finalize,
            "requested_contract": {"duration_limit_seconds": self.seconds, "nominal_rate": "15",
                                   "frame_limit": None, "timestamps": "passthrough"},
            "forced_stop": forced, "geometry": self.geometry, "display": self.display,
            "controller_receipt": "controller.json", "encoder_progress": self.counters,
            "codec": "libx264rgb", "crf": 18, "preset": "veryfast", "nominal_rate": "15",
            "capture": "full-display x11grab, lossy RGB, passthrough timestamps, no audio",
            "validation": "pending; run additive offline media validation separately",
        })
        require(self.path.exists() and self.path.stat().st_size < 104857600,
                "missing/oversized original; preserve it but do not claim eligible coverage")
        require(not forced, "encoder drain deadline; forced cleanup is not a valid recording")
        require(code in (0, 255), "encoder failure; preserve original and encoder log")
