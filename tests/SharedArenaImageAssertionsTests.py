#!/usr/bin/env python3
"""Regression checks for SCORE closure over arena-colored backgrounds."""

from pathlib import Path
import subprocess
import unittest

import SharedArenaImageAssertions as images


class ScoreClosureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        font = Path(__file__).resolve().parents[1] / "resources/data/font.ttf"
        # Use the shipped font and real title, not a mask mirroring the detector.
        cls.title = subprocess.check_output([
            "convert", "-background", "blue", "-fill", "white", "-font", str(font),
            "-pointsize", "32", "label:---SCORE---", "-resize", "176x32!",
            "-depth", "8", "rgb:-",
        ])

    def frame(self, color):
        return bytearray(bytes(color) * (images.WIDTH * images.HEIGHT))

    def paint_title(self, frame, players, teams):
        # Fixture layout comes from the score table geometry; runtime tests also
        # exercise all real layouts, including uneven team membership.
        geometry = images.score_geometry(players, teams)
        first_row = geometry[2] if teams else geometry[2][0]
        header_y = first_row - 64
        for row in range(32):
            offset = ((header_y - 16 + row) * images.WIDTH + images.WIDTH // 2 - 88) * 3
            frame[offset:offset + 176 * 3] = self.title[row * 176 * 3:(row + 1) * 176 * 3]

    def open_frame(self, players, teams):
        frame = self.frame((0, 0, 255))
        if teams:
            left, width, first_row, _ = images.score_geometry(players, teams)
            colors = ((255, 0, 0), (0, 255, 0), (255, 255, 0), (255, 0, 255))
            for team in range(teams):
                # Two child rows per team, with the required 8px separator band.
                center = first_row + team * (3 * 32 + 8)
                for y in range(center - 8, center + 8):
                    offset = (y * images.WIDTH + left) * 3
                    frame[offset:offset + width * 3] = bytes(colors[team]) * width
                if team:
                    for y in range(center - 13, center - 11):
                        offset = (y * images.WIDTH + left) * 3
                        frame[offset:offset + width * 3] = bytes((255, 255, 255)) * width
        self.paint_title(frame, players, teams)
        return frame

    def test_closed_blue_arena_is_not_a_score_title(self):
        for players, teams in ((2, 0), (3, 0), (4, 2), (6, 3), (8, 4)):
            with self.subTest(players=players, teams=teams):
                self.assertLess(images.assert_score_overlay_closed(
                    self.frame((0, 0, 255)), self.open_frame(players, teams),
                    "closed-blue", players, teams), 0.80)

    def test_white_background_is_not_a_score_title(self):
        images.assert_score_overlay_closed(
            self.frame((255, 255, 255)), self.open_frame(2, 0), "closed-white", 2, 0)

    def test_open_overlay_is_rejected(self):
        for players, teams in ((2, 0), (3, 0), (4, 2), (6, 3), (8, 4)):
            with self.subTest(players=players, teams=teams):
                reference = self.open_frame(players, teams)
                with self.assertRaisesRegex(AssertionError, "header still visible"):
                    images.assert_score_overlay_closed(reference, reference, "still-open", players, teams)

    def test_visible_title_is_rejected_despite_changed_arena(self):
        candidate = self.frame((70, 100, 30))
        self.paint_title(candidate, 2, 0)
        with self.assertRaisesRegex(AssertionError, "header still visible"):
            images.assert_score_overlay_closed(candidate, self.open_frame(2, 0), "changed-arena", 2, 0)

    def test_reference_without_title_glyphs_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "reference has no SCORE title glyphs"):
            images.assert_score_overlay_closed(
                self.frame((0, 0, 0)), self.frame((0, 0, 255)), "bad-reference", 2, 0)


if __name__ == "__main__":
    unittest.main()
