#!/usr/bin/env python3
"""Deterministic relay total-budget tests without sockets or wall-clock sleeps."""
from collections import deque
import ssl
import struct
import threading
import unittest
from unittest.mock import patch

from PublicDedicatedProcessTests import TlsPeer


class RelayBudgetTests(unittest.TestCase):
    def check_total_budget(self, retry):
        clock = [100.0]
        peer = object.__new__(TlsPeer)
        peer.stop = threading.Event()
        peer.control_root = None
        peer.frames = deque(maxlen=64)
        peer.events = deque(maxlen=64)
        peer.evidence_lock = threading.Lock()
        peer.application_bytes = 0
        peer.forwarded_ends = 0
        peer.eof_observation = None
        peer.end_write_race = False
        payload = b'D6LC\0\1\0\6\0\1' + bytes(16)
        wire = struct.pack('!IHHI', 0x44365254, 1, 0, len(payload)) + payload

        class Socket:
            def __init__(self, reader=False):
                self.first = reader
                self.sent = 0
                self.attempt = 0
                self.retry_bytes = None
            def setblocking(self, _): pass
            def pending(self): return 0
            def shutdown(self, _): pass
            def recv(self, _):
                if self.first:
                    self.first = False
                    return wire
                raise BlockingIOError
            def send(self, data):
                if self.retry_bytes is not None:
                    assert data == self.retry_bytes, 'SSL retry changed the in-progress write buffer'
                    self.retry_bytes = None
                self.attempt += 1
                if retry and self.attempt % 2 == 0:
                    self.retry_bytes = bytes(data)
                    raise retry()
                count = min(8, len(data))
                self.sent += count
                if self.sent == len(wire): peer.stop.set()
                return count

        client, backend = Socket(True), Socket()
        def readiness(reads, writes, errors, timeout):
            clock[0] += .4
            self.assertLess(clock[0], 110, 'relay failed to terminate within bounded fake time')
            return list(reads), list(writes), []
        with patch('PublicDedicatedProcessTests.time.monotonic', lambda: clock[0]), \
             patch('PublicDedicatedProcessTests.select.select', readiness):
            peer.relay_streams(client, backend, 1)
        self.assertLess(backend.sent, len(wire), 'partial progress refreshed the total send budget')
        self.assertTrue(any(event[2] == 'write-budget' for event in peer.events))

    def test_partial_progress_cannot_refresh_total_deadline(self):
        self.check_total_budget(None)

    def test_ssl_read_retry_cannot_refresh_partial_write_deadline(self):
        self.check_total_budget(ssl.SSLWantReadError)

    def test_ssl_write_retry_cannot_refresh_partial_write_deadline(self):
        self.check_total_budget(ssl.SSLWantWriteError)

    def test_eof_interval_excludes_other_connections_and_teardown(self):
        peer = object.__new__(TlsPeer)
        peer.evidence_lock = threading.Lock()
        peer.forwarded_ends = 0
        peer.eof_observation = None
        peer.begin_eof_observation(2)
        peer.record_forwarded_end(1, 1)  # Legitimate controller stream.
        peer.complete_eof_observation(2)  # Native ambiguity assertions completed.
        peer.record_forwarded_end(2, 1)  # Later teardown is outside the interval.
        self.assertEqual(peer.forwarded_ends, 2)
        self.assertEqual(peer.eof_observation['ends'], 0)

    def test_eof_interval_retains_affected_connection_end_evidence(self):
        peer = object.__new__(TlsPeer)
        peer.evidence_lock = threading.Lock()
        peer.forwarded_ends = 0
        peer.eof_observation = None
        peer.begin_eof_observation(2)
        peer.record_forwarded_end(2, 1)
        peer.complete_eof_observation(2)
        self.assertEqual(peer.eof_observation['ends'], 1)

    def test_summary_scheduler_recognizes_only_complete_replication_phase_prefix(self):
        for kind in (1, 2):
            payload = struct.pack('<IHH', 0x44365250, 3, kind) + bytes(40) + b'\3'
            self.assertEqual(TlsPeer.replication_phase(payload), 3)
            self.assertIsNone(TlsPeer.replication_phase(payload[:-1]))
        for identifier, version, kind, phase in ((0, 3, 1, 3), (0x44365250, 2, 1, 3),
                                                 (0x44365250, 3, 6, 3), (0x44365250, 3, 1, 255)):
            payload = struct.pack('<IHH', identifier, version, kind) + bytes(40) + bytes([phase])
            self.assertIsNone(TlsPeer.replication_phase(payload))


if __name__ == '__main__':
    unittest.main()
