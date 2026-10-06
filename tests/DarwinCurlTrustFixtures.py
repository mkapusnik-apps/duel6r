#!/usr/bin/env python3
"""Loopback HTTPS negatives; custom test CA does not mutate any trust store."""
import http.server
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import threading

executable, openssl = sys.argv[1:]


class Handler(http.server.BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200)
        self.end_headers()

    def log_message(self, *_):
        pass


with tempfile.TemporaryDirectory(prefix="duel6r-trust-") as temporary:
    root = Path(temporary)
    certificate, key = root / "certificate.pem", root / "key.pem"
    subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                    "-keyout", str(key), "-out", str(certificate), "-days", "1",
                    "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost"],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=20)
    der = root / "certificate.der"
    subprocess.run([openssl, "x509", "-in", str(certificate), "-outform", "DER", "-out", str(der)],
                   check=True, timeout=5)
    failures = []
    control = subprocess.run([executable, "--observer-control", str(der)], timeout=12)
    if control.returncode:
        failures.append("observer-control")
    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    try:
        port = server.server_port
        for mode, host, ca in (("fixture-untrusted", "localhost", "-"),
                               ("fixture-trusted", "localhost", str(certificate)),
                               ("fixture-wrong-host", "wrong.invalid", str(certificate))):
            result = subprocess.run([executable, mode, f"https://{host}:{port}/",
                                     f"{host}:{port}:127.0.0.1", ca], timeout=12)
            if result.returncode:
                failures.append(mode)
    finally:
        server.shutdown()
        thread.join()
        server.server_close()
    if failures:
        raise AssertionError("native trust failures: " + ", ".join(failures))
