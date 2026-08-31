#!/usr/bin/env python3
"""Pixel-level assertions for the real round-summary SDL/OpenGL harness."""

import os
import subprocess
import sys


WIDTH = 1280
HEIGHT = 900


def fail(message):
    raise SystemExit(f"round-summary-progress: FAIL: {message}")


def pixels(path):
    output = subprocess.check_output(
        ["convert", path, "-resize", f"{WIDTH}x{HEIGHT}!", "-depth", "8", "rgb:-"]
    )
    expected = WIDTH * HEIGHT * 3
    if len(output) != expected:
        fail(f"unexpected decoded image size for {path}: {len(output)}")
    return output


def pixel(image, x, y):
    offset = (y * WIDTH + x) * 3
    return image[offset], image[offset + 1], image[offset + 2]


def white(image, x, y):
    red, green, blue = pixel(image, x, y)
    return min(red, green, blue) >= 205 and max(red, green, blue) - min(red, green, blue) <= 48


def white_count(image, box):
    left, top, right, bottom = box
    return sum(white(image, x, y) for y in range(top, bottom) for x in range(left, right))


def blue_strip_rows(image):
    rows = []
    for y in range(300, 430):
        blue = 0
        for x in range(345, 935):
            red, green, value = pixel(image, x, y)
            if red <= 20 and green <= 20 and value >= 220:
                blue += 1
        if blue >= 540:
            rows.append(y)
    if not rows:
        return None
    return min(rows), max(rows) + 1


def expected_text_mask(font_path, text):
    raw = subprocess.check_output([
        "convert", "-background", "black", "-fill", "white", "-font", font_path,
        "-pointsize", "32", f"label:{text}", "-resize", "176x32!", "-depth", "8", "gray:-",
    ])
    if len(raw) != 176 * 32:
        fail(f"could not render expected label {text!r}")
    return [value >= 64 for value in raw]


def observed_text_mask(image, left, top):
    return [white(image, left + x, top + y) for y in range(32) for x in range(176)]


def dice(first, second):
    overlap = sum(a and b for a, b in zip(first, second))
    total = sum(first) + sum(second)
    return 2.0 * overlap / total if total else 0.0


def label_scores(image, font_path):
    scores = {}
    for played in range(10):
        expected = expected_text_mask(font_path, f"Rounds: {played}|5")
        score = 0.0
        for left in range(549, 556):
            for top in range(336, 344):
                score = max(score, dice(observed_text_mask(image, left, top), expected))
        scores[played] = score
    return scores


def has_top_progress(image):
    # The live finite-round indicator has an opaque black 136x18 backing and
    # white glyphs. Requiring both avoids treating bright arena scenery as UI.
    dark = 0
    for y in range(2, 21):
        for x in range(571, 709):
            red, green, blue = pixel(image, x, y)
            dark += max(red, green, blue) <= 5
    glyphs = white_count(image, (575, 3, 706, 20))
    return dark >= 1700 and glyphs >= 75, dark, glyphs


def assert_included(root, font_path, label, played):
    for frame in ("summary-early.png", "summary-late.png"):
        image = pixels(os.path.join(root, label, frame))
        strip = blue_strip_rows(image)
        if strip != (366, 402):
            fail(f"{label}/{frame}: expected unobstructed score strip at rows 366..401, got {strip}")
        scores = label_scores(image, font_path)
        winner = max(scores, key=scores.get)
        if winner != played or scores[played] < 0.42:
            fail(f"{label}/{frame}: expected exact Rounds: {played}|5 label; OCR scores={scores}")
        if white_count(image, (550, 366, 730, 402)) < 250:
            fail(f"{label}/{frame}: SCORE heading is missing or obstructed")
        if white_count(image, (550, 403, 895, 433)) < 180:
            fail(f"{label}/{frame}: score-table title is missing or overlaps the heading")
        shown, dark, glyphs = has_top_progress(image)
        if shown:
            fail(f"{label}/{frame}: duplicate top-center progress remained (dark={dark}, glyphs={glyphs})")


def assert_excluded(root, label, relative_path, expect_top):
    image = pixels(os.path.join(root, relative_path))
    strip = blue_strip_rows(image)
    if strip != (350, 386):
        fail(f"{label}: unchanged score strip expected at rows 350..385, got {strip}")
    shown, dark, glyphs = has_top_progress(image)
    if shown != expect_top:
        fail(f"{label}: top-center progress expectation failed (shown={shown}, dark={dark}, glyphs={glyphs})")


def main():
    if len(sys.argv) != 3:
        fail("usage: assertions.py TEST_ROOT FONT_PATH")
    root, font_path = sys.argv[1:]
    for label, played in (("first", 1), ("resumed", 3), ("penultimate", 4)):
        assert_included(root, font_path, label, played)

    assert_excluded(root, "final", "final/summary.png", True)
    assert_excluded(root, "unlimited", "unlimited/summary.png", False)
    assert_excluded(root, "active-tab", "first/active-tab.png", True)

    for frame in ("next-round-first.png", "next-round-settled.png"):
        image = pixels(os.path.join(root, "first", frame))
        shown, dark, glyphs = has_top_progress(image)
        if not shown:
            fail(f"{frame}: next-round top progress was not restored (dark={dark}, glyphs={glyphs})")
        if blue_strip_rows(image) is not None:
            fail(f"{frame}: round-summary panel remained on the next-round frame")

    print("round-summary-progress: all visual behavior assertions passed")


if __name__ == "__main__":
    main()
