#!/usr/bin/env python3
"""Regression checks for SCORE closure over arena-colored backgrounds."""

from contextlib import contextmanager
from pathlib import Path
import subprocess
import time
import unittest
from unittest.mock import patch

import SharedArenaImageAssertions as images

STARTED = time.monotonic()
CHECK_OUTPUT = subprocess.check_output


def progress(stage, **fields):
    try:
        print(f"image-assertions elapsed={time.monotonic() - STARTED:.6f} stage={stage} {fields}", flush=True)
    except OSError:
        pass  # Observations must not replace the test's own outcome.


@contextmanager
def observed_subcase(**fields):
    began = time.monotonic()
    progress("subcase-begin", **fields)
    try:
        yield
    finally:
        progress("subcase-end", seconds=time.monotonic() - began, **fields)


def font_output(command, *args, **kwargs):
    label = next((part[6:] for part in command if part.startswith("label:")), "unknown")
    began = time.monotonic()
    progress("font-convert-begin", label=label)
    code = "unavailable"
    try:
        result = CHECK_OUTPUT(command, *args, **kwargs)
        code = 0
        return result
    except subprocess.CalledProcessError as error:
        code = error.returncode
        raise
    finally:
        progress("font-convert-end", label=label, rc=code, seconds=time.monotonic() - began)


class ProgressResult(unittest.TextTestResult):
    def startTest(self, test):
        progress("test-begin", test=test.id())
        super().startTest(test)

    def stopTest(self, test):
        progress("test-end", test=test.id())
        super().stopTest(test)


class ProgressRunner(unittest.TextTestRunner):
    resultclass = ProgressResult


class ScoreClosureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        progress("class-setup-begin", test_class=cls.__name__)
        # Observe fixture and detector font subprocesses without changing their args/timeouts.
        observer = patch.object(subprocess, "check_output", font_output)
        observer.start()
        cls.addClassCleanup(observer.stop)
        font = Path(__file__).resolve().parents[1] / "resources/data/font.ttf"
        # Use the shipped font and real title, not a mask mirroring the detector.
        try:
            cls.title = subprocess.check_output([
                "convert", "-background", "blue", "-fill", "white", "-font", str(font),
                "-pointsize", "32", "label:---SCORE---", "-resize", "176x32!",
                "-depth", "8", "rgb:-",
            ])
        finally:
            progress("class-setup-end", test_class=cls.__name__)

    def frame(self, color):
        return bytearray(bytes(color) * (images.WIDTH * images.HEIGHT))

    def paint_title(self, frame, players, teams, ink_offset=0):
        # Fixture layout comes from the score table geometry; runtime tests also
        # exercise all real layouts, including uneven team membership.
        geometry = images.score_geometry(players, teams)
        first_row = geometry[2] if teams else geometry[2][0]
        header_y = first_row - 64 + ink_offset
        for row in range(32):
            offset = ((header_y - 16 + row) * images.WIDTH + images.WIDTH // 2 - 88) * 3
            frame[offset:offset + 176 * 3] = self.title[row * 176 * 3:(row + 1) * 176 * 3]

    def open_frame(self, players, teams):
        frame = self.frame((0, 0, 255))
        if teams:
            left, width, first_row, _ = images.score_geometry(players, teams)
            top = first_row - 17
            for team in range(teams):
                self.paint_header(frame, left, top, width, images.TEAM_NAMES[team])
                top += 32 * (1 + players // teams + (team < players % teams))
                # Full-width2px rule centered in the renderer's8px gutter.
                if team < teams - 1:
                    self.paint_box(frame, left, top + 3, width, 2, (200, 200, 200))
                    top += 8
        self.paint_title(frame, players, teams)
        return frame

    def test_closed_blue_arena_is_not_a_score_title(self):
        for players, teams in ((2, 0), (3, 0), (4, 2), (6, 3), (8, 4)):
            with self.subTest(players=players, teams=teams), observed_subcase(players=players, teams=teams):
                self.assertLess(images.assert_score_overlay_closed(
                    self.frame((0, 0, 255)), self.open_frame(players, teams),
                    "closed-blue", players, teams), 0.80)

    def test_white_background_is_not_a_score_title(self):
        images.assert_score_overlay_closed(
            self.frame((255, 255, 255)), self.open_frame(2, 0), "closed-white", 2, 0)

    def test_open_overlay_is_rejected(self):
        for players, teams in ((2, 0), (3, 0), (4, 2), (6, 3), (8, 4)):
            with self.subTest(players=players, teams=teams), observed_subcase(players=players, teams=teams):
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

    def paint_header(self, frame, left, top, width, name):
        colors = {"Alpha": (255,0,0), "Bravo": (0,255,0),
                  "Charlie": (255,255,0), "Delta": (255,0,255)}
        color = colors[name]
        self.paint_box(frame, left, top, width, 32, color)
        # Real shipped-font raster, independently composed as transparent RGBA,
        # not a detector-generated boolean mask or a colored-rectangle identity.
        font = Path(__file__).resolve().parents[1] / "resources/data/font.ttf"
        rgba = subprocess.check_output([
            "convert", "-background", "none", "-fill", "black", "-font", str(font),
            "-pointsize", "32", f"label:{name}", "-resize", f"{len(name)*16}x32!",
            "-depth", "8", "rgba:-"])
        for y in range(32):
            for x in range(len(name)*16):
                alpha = rgba[(y*len(name)*16+x)*4+3]
                offset = ((top + 1 + y)*images.WIDTH + left + x)*3
                frame[offset:offset+3] = bytes(round(c*(255-alpha)/255) for c in color)

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
            self.paint_header(frame, left, top, width, team)
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
        for ink_offset in (0, -1, 1):
            with self.subTest(title_ink_offset=ink_offset), observed_subcase(title_ink_offset=ink_offset):
                frame, _, _, _, _ = self.uneven_four_team_frame()
                # Moving title ink does not move renderer rows or separators.
                # It must not become a new exact row-origin requirement.
                title_top = images.score_geometry(15,4)[2] - 80
                self.paint_box(frame,552,title_top-1,176,34,(0,0,255))
                self.paint_title(frame,15,4,ink_offset)
                groups, _, _ = images.assert_score_overlay(frame,"magenta-gutters",15,4)
                self.assertEqual(["Bravo","Alpha","Delta","Charlie"],groups)

    def test_missing_duplicate_and_extra_genuine_headers_still_fail(self):
        for kind in ("missing","duplicate","extra"):
            with self.subTest(kind=kind), observed_subcase(kind=kind):
                frame, left, width, headers, _ = self.uneven_four_team_frame()
                if kind=="missing":
                    self.paint_box(frame,left,headers[2][0],width,32,(0,0,255))
                elif kind=="duplicate":
                    self.paint_header(frame,left,headers[2][0],width,"Alpha")
                else:
                    self.paint_header(frame,left,headers[0][0]+64,width,"Alpha")
                with self.assertRaisesRegex(AssertionError,"expected 4 score group headers"):
                    images.assert_score_overlay(frame,kind,15,4)

    def test_footer_scenery_is_not_identity_but_genuine_appended_header_is_extra(self):
        frame, left, width, headers, _ = self.uneven_four_team_frame()
        bottom = headers[0][0] + 19*32 + 3*8
        self.paint_box(frame, left, bottom, width, 32, (210,40,210))
        with observed_subcase(kind="magenta-footer"):
            self.assertEqual(4, len(images.assert_score_overlay(frame,"magenta-footer",15,4)[0]))
        self.paint_header(frame, left, bottom, width, "Delta")
        with observed_subcase(kind="real-extra-footer"), self.assertRaisesRegex(AssertionError,"expected 4 score group headers"):
            images.assert_score_overlay(frame,"real-extra-footer",15,4)

    def test_color_without_name_and_name_without_full_backing_fail(self):
        for kind in ("no-glyph", "partial-row"):
            with observed_subcase(kind=kind):
                frame, left, width, headers, _ = self.uneven_four_team_frame()
                top = headers[1][0]
                if kind == "no-glyph":
                    self.paint_box(frame,left,top,width,32,(255,0,0))
                else:
                    self.paint_box(frame,left,top,width,1,(0,0,255))
                with self.assertRaisesRegex(AssertionError,"expected 4 score group headers"):
                    images.assert_score_overlay(frame,kind,15,4)

    def test_missing_short_thick_and_extra_separator_rules_still_fail(self):
        for kind in ("missing","short","thick","extra"):
            with self.subTest(kind=kind), observed_subcase(kind=kind):
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

    @staticmethod
    def legacy_title_scores(data, tops):
        # Pinned3825bed: glyph_similarity itself is unchanged by the title-only optimization.
        return [(top, images.glyph_similarity(data, "---SCORE---", images.WIDTH // 2 - 88, top))
                for top in tops]

    def test_title_scores_match_3825_reference_with_one_union_read(self):
        frame, _, _, _, _ = self.uneven_four_team_frame()
        first_y = images.score_geometry(15, 4)[2]
        tops = range(max(0, first_y - 144), min(images.HEIGHT - 32, first_y - 16))
        expected = self.legacy_title_scores(frame, tops)
        with patch.object(images, "region_pixels", wraps=images.region_pixels) as reads:
            actual = images._score_title_candidates(frame, tops)
        self.assertEqual(expected, actual)  # Every candidate score and its order, not only the winner.
        reads.assert_called_once_with(frame, 552, tops[0], 728, tops[-1] + 32)
        self.assertEqual(128, len(actual))
        self.assertEqual(27984, (tops[-1] + 32 - tops[0]) * 176)
        progress("title-mask-work", old_threshold_pixels=len(tops) * 176 * 32,
                 union_threshold_pixels=(tops[-1] + 32 - tops[0]) * 176)

    def test_title_mask_is_fresh_on_mutation_and_ties_keep_first_position(self):
        frame = self.frame((204, 255, 255))
        tops = range(10, 18)
        before = images._score_title_candidates(frame, tops)
        self.assertEqual(self.legacy_title_scores(frame, tops), before)
        self.assertTrue(all(score == 0 for _, score in before))
        self.assertEqual(tops[0], max(before, key=lambda item: item[1])[0])
        self.paint_box(frame, 552, tops[0], 176, tops[-1] + 32 - tops[0], (205, 205, 205))
        after = images._score_title_candidates(frame, tops)
        self.assertEqual(self.legacy_title_scores(frame, tops), after)
        self.assertTrue(all(score == after[0][1] > 0 for _, score in after))
        self.assertEqual(tops[0], max(after, key=lambda item: item[1])[0])
        self.paint_box(frame, 552, tops[0], 176, tops[-1] + 32 - tops[0], (255, 204, 255))
        self.assertEqual(before, images._score_title_candidates(frame, tops))

    def test_title_mask_keeps_edge_empty_and_truncated_buffer_behavior(self):
        frame = self.frame((205, 205, 205))
        for tops in (range(0, 3), range(images.HEIGHT - 35, images.HEIGHT - 32)):
            with self.subTest(first=tops[0]):
                self.assertEqual(self.legacy_title_scores(frame, tops), images._score_title_candidates(frame, tops))
        empty = images._score_title_candidates(b"", range(0))
        self.assertEqual([], empty)
        with self.assertRaises(ValueError) as legacy_error:
            max(range(0), key=lambda top: images.glyph_similarity(b"", "---SCORE---", 552, top))
        with self.assertRaises(ValueError) as current_error:
            max(empty, key=lambda item: item[1])
        self.assertEqual(legacy_error.exception.args, current_error.exception.args)
        truncated = frame[:(33 * images.WIDTH + 728) * 3 - 1]
        for score in (self.legacy_title_scores, images._score_title_candidates):
            with self.assertRaises(IndexError):
                score(truncated, range(0, 3))


if __name__ == "__main__":
    unittest.main(testRunner=ProgressRunner, verbosity=2)
