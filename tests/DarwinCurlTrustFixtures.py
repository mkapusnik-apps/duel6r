#!/usr/bin/env python3
"""Loopback HTTPS negatives; custom test CA does not mutate any trust store."""
import argparse
import http.server
import os
from pathlib import Path
import re
import ssl
import subprocess
import tempfile
import threading


def _diagnostic(stderr):
    text = (stderr or b"").decode("utf-8", errors="replace")
    # Redact before truncation, including an incomplete PEM on timeout. Never
    # print stdout or read the key file when reporting a command failure.
    text = re.sub(r"-----BEGIN [^-\r\n]*PRIVATE KEY-----.*?(?:-----END [^-\r\n]*PRIVATE KEY-----|\Z)",
                  "[private key redacted]", text, flags=re.DOTALL)
    return text[-4096:].strip() or "(no stderr)"


def _run_openssl(openssl, arguments, environment, timeout):
    try:
        result = subprocess.run([str(openssl), *map(str, arguments)], env=environment,
                                stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"OpenSSL {arguments[0]} timed out after {timeout}s: "
                           f"{_diagnostic(error.stderr)}") from None
    except OSError as error:
        raise RuntimeError(f"OpenSSL {arguments[0]} could not start: {error}") from None
    if result.returncode:
        raise RuntimeError(f"OpenSSL {arguments[0]} exited {result.returncode}: "
                           f"{_diagnostic(result.stderr)}")


def generate_certificate(root, openssl):
    """Write an isolated one-day localhost fixture; return PEM, key, DER paths.

    The caller owns the existing temporary directory and its cleanup. Only the
    OpenSSL children receive the fixture configuration; trust probes retain the
    caller's environment. Keys and serials remain freshly randomized.
    """
    root = Path(root).resolve()
    configuration = root / "openssl.cnf"
    configuration.write_text("""[req]
prompt = no
distinguished_name = subject
x509_extensions = extensions
[subject]
CN = localhost
[extensions]
subjectAltName = DNS:localhost
basicConstraints = critical, CA:TRUE
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
""", encoding="ascii")
    environment = {name: value for name, value in os.environ.items()
                   if not name.startswith("OPENSSL_") and name != "RANDFILE"}
    environment["OPENSSL_CONF"] = str(configuration)
    certificate, key, der = (root / name for name in ("certificate.pem", "key.pem", "certificate.der"))
    _run_openssl(openssl, ["req", "-config", configuration, "-batch", "-x509",
                          "-newkey", "rsa:2048", "-nodes", "-sha256", "-keyout", key,
                          "-out", certificate, "-days", "1"], environment, 20)
    _run_openssl(openssl, ["x509", "-in", certificate, "-outform", "DER", "-out", der], environment, 5)
    return certificate, key, der


class Handler(http.server.BaseHTTPRequestHandler):
    def do_HEAD(self):
        self.send_response(200)
        self.end_headers()

    def log_message(self, *_):
        pass


def run_trust_scenarios(executable, certificate, key, der):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true",
                        help="only generate and read PEM/key/DER with the selected OpenSSL")
    parser.add_argument("executable", nargs="?")
    parser.add_argument("openssl")
    args = parser.parse_args()
    if args.preflight == bool(args.executable):
        parser.error("provide a trust probe executable, or --preflight without a probe")
    with tempfile.TemporaryDirectory(prefix="duel6r-trust-") as temporary:
        certificate, key, der = generate_certificate(temporary, args.openssl)
        if args.preflight:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(certificate, key)
            if der.read_bytes() != ssl.PEM_cert_to_DER_cert(certificate.read_text(encoding="ascii")):
                raise RuntimeError("OpenSSL PEM/DER certificate mismatch")
            print("OpenSSL certificate generation, key loading and DER conversion passed")
        else:
            run_trust_scenarios(args.executable, certificate, key, der)


if __name__ == "__main__":
    main()
