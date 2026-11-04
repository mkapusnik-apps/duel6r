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

    def paint_box(self, frame, left, top, width, height, color):
        for y in range(top, top + height):
            offset = (y * images.WIDTH + left) * 3
            frame[offset:offset + width * 3] = bytes(color) * width

    def uneven_four_team_frame(self):
        frame = self.frame((0, 0, 255))
        left, width, first_row, _ = images.score_geometry(15, 4)
        # Production draws32px rows with a1px backing offset. Order reflects
        # a real sorted scoreboard, not the roster's alphabetical team order.
        top = first_row - 17
        colors = {"Alpha":(255,0,0), "Bravo":(0,255,0),
                  "Charlie":(255,255,0), "Delta":(255,0,255)}
        headers = []
        rules = []
        for index, team in enumerate(("Bravo", "Alpha", "Delta", "Charlie")):
            headers.append((top, team))
            self.paint_box(frame, left, top, width, 32, colors[team])
            top += 32 * (1 + (3 if team == "Delta" else 4))
            if index < 3:
                # Magenta arena/lava visible in both3px sides of the gutter
                # must not create extra Delta headers or extend a real one.
                self.paint_box(frame, left, top, width, 8, (210,40,210))
                rules.append(top + 3)
                self.paint_box(frame, left, top + 3, width, 2, (200,200,200))
                top += 8
        self.paint_title(frame, 15, 4)
        return frame, left, width, headers, rules

    def test_four_team_geometry_includes_charlie_not_only_delta(self):
        self.assertEqual((1173,104,19),images.ranking_geometry(15,4))
        self.assertEqual((376,528,166,728),images.score_geometry(15,4))

    def test_magenta_separator_gutters_do_not_create_headers(self):
        frame, _, _, _, _ = self.uneven_four_team_frame()
        groups, _, _ = images.assert_score_overlay(frame,"magenta-gutters",15,4)
        self.assertEqual(["Bravo","Alpha","Delta","Charlie"],groups)

    def test_missing_duplicate_and_extra_genuine_headers_still_fail(self):
        for kind in ("missing","duplicate","extra"):
            with self.subTest(kind=kind):
                frame, left, width, headers, _ = self.uneven_four_team_frame()
                if kind=="missing":
                    self.paint_box(frame,left,headers[2][0],width,32,(0,0,255))
                elif kind=="duplicate":
                    self.paint_box(frame,left,headers[2][0],width,32,(255,0,0))
                else:
                    self.paint_box(frame,left,headers[0][0]+64,width,32,(255,0,0))
                with self.assertRaisesRegex(AssertionError,"expected 4 score group headers"):
                    images.assert_score_overlay(frame,kind,15,4)

    def test_missing_short_thick_and_extra_separator_rules_still_fail(self):
        for kind in ("missing","short","thick","extra"):
            with self.subTest(kind=kind):
                frame, left, width, headers, rules = self.uneven_four_team_frame()
                if kind=="missing":
                    self.paint_box(frame,left,rules[1],width,2,(210,40,210))
                elif kind=="short":
                    self.paint_box(frame,left,rules[1],width//2,2,(210,40,210))
                elif kind=="thick":
                    self.paint_box(frame,left,rules[1]+2,width,1,(200,200,200))
                else:
                    self.paint_box(frame,left,headers[0][0]+64,width,2,(200,200,200))
                with self.assertRaisesRegex(AssertionError,"separator"):
                    images.assert_score_overlay(frame,kind,15,4)


if __name__ == "__main__":
    unittest.main()
