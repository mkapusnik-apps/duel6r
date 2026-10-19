#!/usr/bin/env python3
"""Portable regression coverage for the real OpenSSL certificate fixture."""
import contextlib
import os
from pathlib import Path
import ssl
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import DarwinCurlTrustFixtures as fixtures


OPENSSL = sys.argv.pop(1)


class CertificateFixtureTests(unittest.TestCase):
    def test_missing_host_config_and_unrelated_cwd(self):
        with tempfile.TemporaryDirectory(prefix="duel6r fixture tests ") as temporary:
            root = Path(temporary)
            output, cwd = root / "certificate output", root / "unrelated cwd"
            output.mkdir()
            cwd.mkdir()
            hostile = {"OPENSSL_CONF": str(root / "missing.cnf"),
                       "OPENSSL_CONF_INCLUDE": str(root / "missing includes"),
                       "OPENSSL_MODULES": str(root / "missing modules"),
                       "OPENSSL_ENGINES": str(root / "missing engines"),
                       "RANDFILE": str(root / "must not be written")}
            with mock.patch.dict(os.environ, hostile), contextlib.chdir(cwd):
                before = dict(os.environ)
                certificate, key, der = fixtures.generate_certificate(output, OPENSSL)
                self.assertEqual(dict(os.environ), before)
            self.assertEqual(list(cwd.iterdir()), [])
            self.assertFalse(Path(hostile["RANDFILE"]).exists())
            self.assertEqual(der.read_bytes(), ssl.PEM_cert_to_DER_cert(certificate.read_text()))
            ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER).load_cert_chain(certificate, key)
            # Real certificate semantics, not just successful command exit.
            environment = {**os.environ, "OPENSSL_CONF": str(output / "openssl.cnf")}
            details = subprocess.run([OPENSSL, "x509", "-in", str(certificate), "-noout", "-text"],
                                     env=environment, capture_output=True, text=True, check=True, timeout=5).stdout
            for expected in ("DNS:localhost", "CA:TRUE", "sha256WithRSAEncryption", "2048 bit"):
                self.assertIn(expected, details)
            for host, trusted, success in (("localhost", True, True), ("wrong.invalid", True, False),
                                           ("localhost", False, False)):
                command = [OPENSSL, "verify", "-no-CAfile", "-no-CApath", "-no-CAstore",
                           "-verify_hostname", host]
                if trusted:
                    command += ["-trusted", str(certificate)]
                result = subprocess.run([*command, str(certificate)], env=environment,
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode == 0, success, result.stderr)
                if not success:
                    self.assertIn("hostname mismatch" if trusted else "self-signed certificate", result.stderr)

    def test_preflight_cli(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run([sys.executable, str(Path(fixtures.__file__).resolve()),
                                     "--preflight", OPENSSL], cwd=temporary,
                                    env={**os.environ, "OPENSSL_CONF": str(Path(temporary) / "absent.cnf")},
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("DER conversion passed", result.stdout)
            self.assertNotIn("PRIVATE KEY", result.stdout + result.stderr)

    def test_real_generation_failure_has_exit_and_stderr(self):
        with tempfile.TemporaryDirectory() as temporary:
            # A directory cannot be opened as the private-key output file.
            (Path(temporary) / "key.pem").mkdir()
            with self.assertRaises(RuntimeError) as caught:
                fixtures.generate_certificate(temporary, OPENSSL)
            message = str(caught.exception)
            self.assertIn("OpenSSL req exited 1", message)
            self.assertIn("key.pem", message)
            self.assertNotIn("(no stderr)", message)
            self.assertNotIn("PRIVATE KEY", message)
            self.assertLess(len(message), 4200)

    def test_conversion_failure_is_not_hidden(self):
        with tempfile.TemporaryDirectory() as temporary:
            (Path(temporary) / "certificate.der").mkdir()
            with self.assertRaisesRegex(RuntimeError, "OpenSSL x509 exited 1"):
                fixtures.generate_certificate(temporary, OPENSSL)

    def test_diagnostics_are_bounded_and_redact_keys_before_truncation(self):
        stderr = (b"-----BEGIN PRIVATE KEY-----\n" + b"secret-key-material" * 1000
                  + b"\n-----END PRIVATE KEY-----\nuseful error")
        for status in (1, -9):
            with self.subTest(status=status), mock.patch.object(fixtures.subprocess, "run", return_value=
                    subprocess.CompletedProcess([], status, stderr=stderr)):
                with self.assertRaises(RuntimeError) as caught:
                    fixtures._run_openssl(OPENSSL, ["req"], {}, 20)
                message = str(caught.exception)
                self.assertIn(f"exited {status}", message)
                self.assertIn("useful error", message)
                self.assertNotIn("secret-key-material", message)
                self.assertLess(len(message), 4200)
        self.assertLessEqual(len(fixtures._diagnostic(b"x" * 10000)), 4096)

    def test_timeout_redacts_partial_key_and_reports_phase(self):
        error = subprocess.TimeoutExpired([], 5, stderr=b"conversion error\n-----BEGIN RSA PRIVATE KEY-----\nsecret")
        with mock.patch.object(fixtures.subprocess, "run", side_effect=error):
            with self.assertRaises(RuntimeError) as caught:
                fixtures._run_openssl(OPENSSL, ["x509"], {}, 5)
        self.assertIn("OpenSSL x509 timed out after 5s", str(caught.exception))
        self.assertIn("conversion error", str(caught.exception))
        self.assertNotIn("secret", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
