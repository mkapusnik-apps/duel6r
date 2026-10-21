#!/usr/bin/env python3
import unittest
from MenuTransferFrame import transfer_state


class TransferFrameTests(unittest.TestCase):
    def frame(self, selected=False, transferred=False):
        pixels = bytearray(b"\xff" * (850 * 700 * 3))
        if selected:
            for x in range(270, 290):
                for y in range(240, 250):
                    offset = (y * 850 + x) * 3
                    pixels[offset:offset + 3] = bytes((0, 0, 200))
        if transferred:
            offset = (190 * 850 + 350) * 3
            pixels[offset:offset + 3] = bytes((0, 0, 0))
        return pixels

    def test_stale_nonblank_frame_is_not_selected(self):
        self.assertFalse(transfer_state(self.frame(), 2)[0])

    def test_selected_baseline_requires_two_players(self):
        self.assertTrue(transfer_state(self.frame(selected=True), 2)[0])
        self.assertFalse(transfer_state(self.frame(selected=True, transferred=True), 2)[0])

    def test_transfer_requires_new_row_and_retained_selection(self):
        self.assertFalse(transfer_state(self.frame(selected=True), 3)[0])
        self.assertFalse(transfer_state(self.frame(transferred=True), 3)[0])
        self.assertTrue(transfer_state(self.frame(selected=True, transferred=True), 3)[0])


if __name__ == "__main__":
    unittest.main()
