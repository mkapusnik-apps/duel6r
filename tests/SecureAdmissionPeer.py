"""Fake admission host using the production secure adapter through a test process.

The private pipes carry synthetic test traffic only. TLS and PAKE remain in the
same maintained-library adapter used by the application; no guessed FFI layouts.
"""
import os
import struct
import subprocess


class SecureAdmissionPeer:
    def __init__(self, port):
        self.process = subprocess.Popen(
            [os.environ["D6R_TEST_SECURE_PEER"], str(port)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.established = False
        if self._read(1) != b"\x01":
            self.__exit__()
            raise ConnectionError("Secure fake host startup failed")

    def _read(self, size):
        data = self.process.stdout.read(size)
        if len(data) != size:
            raise ConnectionError("Secure fake host exchange failed")
        return data

    def handshake(self):
        if not self.established:
            if self._read(1) != b"\x02":
                raise ConnectionError("Secure fake host handshake failed")
            self.established = True

    def recv(self, size):
        self.handshake()
        self.process.stdin.write(b"R" + struct.pack("!I", size))
        self.process.stdin.flush()
        count = struct.unpack("!i", self._read(4))[0]
        if count < 0:
            raise ConnectionError("Secure fake host read failed")
        return self._read(count)

    def sendall(self, data):
        self.handshake()
        self.process.stdin.write(b"W" + struct.pack("!I", len(data)) + data)
        self.process.stdin.flush()
        if struct.unpack("!i", self._read(4))[0] != 0:
            raise ConnectionError("Secure fake host write failed")

    def __enter__(self):
        return self

    def __exit__(self, *_):
        try:
            self.process.stdin.close()
            self.process.wait(timeout=2)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait(timeout=2)
        finally:
            self.process.stdout.close()
