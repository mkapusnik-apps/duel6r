#!/usr/bin/env python3
"""Pixel-level assertions for the real round-summary SDL/OpenGL harness."""

import os
import subprocess
import sys


WIDTH = 1280
HEIGHT = 900
LABEL_WIDTH = 176
LABEL_HEIGHT = 32
PANEL_INSET = 16
SCORE_STRIP_OVERHANG = 5


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


def blue_strip_bands(image):
    rows = []
    for y in range(300, 430):
        blue = 0
        # Sample the glyph-free left end of the strip. SCORE text punches
        # holes in the middle, while a duplicate no-progress panel extends
        # this same opaque edge to a second vertical origin.
        for x in range(345, 535):
            red, green, value = pixel(image, x, y)
            if red <= 20 and green <= 20 and value >= 220:
                blue += 1
        if blue >= 180:
            rows.append(y)
    bands = []
    for row in rows:
        if not bands or row != bands[-1][1]:
            bands.append([row, row + 1])
        else:
            bands[-1][1] = row + 1
    return [tuple(band) for band in bands]


def only_blue_strip(image, label):
    """Identify the panel by its single opaque SCORE strip."""
    bands = blue_strip_bands(image)
    if len(bands) != 1:
        fail(f"{label}: expected exactly one summary panel/SCORE strip, got bands {bands}")
    return bands[0]


def blue_strip_bounds(image, strip):
    """Return the longest opaque-blue run in the SCORE strip."""
    best = None
    for y in range(*strip):
        run_start = None
        for x in range(300, 981):
            red, green, blue = pixel(image, x, y)
            opaque_blue = red <= 20 and green <= 20 and blue >= 220
            if opaque_blue and run_start is None:
                run_start = x
            if not opaque_blue and run_start is not None:
                candidate = (run_start, x)
                if best is None or candidate[1] - candidate[0] > best[1] - best[0]:
                    best = candidate
                run_start = None
        if run_start is not None:
            candidate = (run_start, 981)
            if best is None or candidate[1] - candidate[0] > best[1] - best[0]:
                best = candidate
    return best


def expected_text_mask(font_path, text):
    raw = subprocess.check_output([
        "convert", "-background", "black", "-fill", "white", "-font", font_path,
        "-pointsize", "32", f"label:{text}", "-resize", f"{LABEL_WIDTH}x{LABEL_HEIGHT}!",
        "-depth", "8", "gray:-",
    ])
    if len(raw) != LABEL_WIDTH * LABEL_HEIGHT:
        fail(f"could not render expected label {text!r}")
    return [value >= 64 for value in raw]


def observed_text_mask(image, left, top):
    return [
        white(image, left + x, top + y)
        for y in range(LABEL_HEIGHT)
        for x in range(LABEL_WIDTH)
    ]


def dice(first, second):
    overlap = sum(a and b for a, b in zip(first, second))
    total = sum(first) + sum(second)
    return 2.0 * overlap / total if total else 0.0


def crop_mask(mask, box):
    left, top, right, bottom = box
    return [
        mask[y * LABEL_WIDTH + x]
        for y in range(top, bottom)
        for x in range(left, right)
    ]


def played_numeral_region(expected_masks):
    """Return the tight label-relative box whose pixels distinguish numerals."""
    changing = [
        (x, y)
        for y in range(LABEL_HEIGHT)
        for x in range(LABEL_WIDTH)
        if len({mask[y * LABEL_WIDTH + x] for mask in expected_masks.values()}) > 1
    ]
    if not changing:
        fail("rendered round-progress numerals do not have a changing region")
    return (
        min(x for x, _ in changing),
        min(y for _, y in changing),
        max(x for x, _ in changing) + 1,
        max(y for _, y in changing) + 1,
    )


def label_evidence(image, font_path, left, total):
    expected_masks = {
        played: expected_text_mask(font_path, f"Rounds: {played}|{total}")
        for played in range(10)
    }
    numeral_region = played_numeral_region(expected_masks)
    numeral_scores = {played: 0.0 for played in expected_masks}
    full_scores = {played: 0.0 for played in expected_masks}
    for top in range(336, 344):
        observed = observed_text_mask(image, left, top)
        observed_numeral = crop_mask(observed, numeral_region)
        for played, expected in expected_masks.items():
            numeral_scores[played] = max(
                numeral_scores[played],
                dice(observed_numeral, crop_mask(expected, numeral_region)),
            )
            # The full-label score remains an exact-format guard. It no longer
            # selects the played count because its mostly shared glyphs made
            # tiny rasterization differences outweigh the changing numeral.
            full_scores[played] = max(full_scores[played], dice(observed, expected))
    return numeral_scores, full_scores, numeral_region


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


def summary_state_evidence(image, played, total, font_path):
    """Return whether a frame has the complete expected summary semantics."""
    bands = blue_strip_bands(image)
    non_final_limited = total > 0 and played < total
    expected_strip = (366, 402) if non_final_limited else (350, 386)
    if bands != [expected_strip]:
        return False, f"score-strip bands={bands}, expected={[expected_strip]}"

    # A non-final limited summary moves progress into its dedicated panel row.
    # Final summaries retain the live top indicator; unlimited summaries have
    # no round progress in either location.
    top_progress, _, _ = has_top_progress(image)
    expected_top_progress = total > 0 and not non_final_limited
    if top_progress != expected_top_progress:
        return False, (
            f"top-progress shown={top_progress}, expected={expected_top_progress}"
        )

    label_diagnostic = "panel progress not applicable"
    if non_final_limited:
        strip_bounds = blue_strip_bounds(image, expected_strip)
        if strip_bounds is None:
            return False, "SCORE strip horizontal bounds unavailable"
        panel_right = strip_bounds[1] - SCORE_STRIP_OVERHANG
        progress_right = panel_right - PANEL_INSET
        progress_left = progress_right - LABEL_WIDTH
        numeral_scores, full_scores, region = label_evidence(
            image, font_path, progress_left, total
        )
        winner = max(numeral_scores, key=numeral_scores.get)
        runner_up = max(
            (candidate for candidate in numeral_scores if candidate != winner),
            key=numeral_scores.get,
        )
        label_diagnostic = (
            f"played-numeral winner={winner} score={numeral_scores[winner]:.4f}, "
            f"runner-up={runner_up} score={numeral_scores[runner_up]:.4f}, "
            f"expected-full-label score={full_scores[played]:.4f}, "
            f"label-relative-region={region}, label-x={progress_left}"
        )
        if winner != played or numeral_scores[played] < 0.42:
            return False, (
                f"played-numeral winner={winner}, expected={played}, "
                f"scores={numeral_scores}, label-relative-region={region}, "
                f"label-x={progress_left}"
            )
        if full_scores[played] < 0.42:
            return False, (
                f"exact Rounds: {played}|{total} full-label score="
                f"{full_scores[played]:.4f}, scores={full_scores}, label-x={progress_left}"
            )

    return True, f"complete expected summary semantic state; {label_diagnostic}"


def is_expected_summary_state(image, played, total, font_path):
    return summary_state_evidence(image, played, total, font_path)[0]


def assert_included(root, font_path, label, played, frames=("summary-early.png", "summary-late.png")):
    for frame in frames:
        image = pixels(os.path.join(root, label, frame))
        strip = only_blue_strip(image, f"{label}/{frame}")
        if strip != (366, 402):
            fail(f"{label}/{frame}: expected unobstructed score strip at rows 366..401, got {strip}")
        strip_bounds = blue_strip_bounds(image, strip)
        if strip_bounds is None:
            fail(f"{label}/{frame}: could not determine SCORE strip horizontal bounds")
        # The opaque SCORE strip extends five pixels beyond each side of the
        # translucent outer panel. The progress text's layout box must end
        # exactly 16 pixels inside that panel's right edge. Checking the one
        # resulting origin (rather than a horizontal OCR search range) rejects
        # the former centered placement and every off-by-one inset regression.
        panel_right = strip_bounds[1] - SCORE_STRIP_OVERHANG
        progress_right = panel_right - PANEL_INSET
        progress_left = progress_right - LABEL_WIDTH
        semantic_match, semantic_diagnostic = summary_state_evidence(
            image, played, 5, font_path
        )
        if not semantic_match:
            fail(
                f"{label}/{frame}: expected exact right-aligned Rounds: {played}|5 label "
                f"at x={progress_left} (panel right={panel_right}, inset={PANEL_INSET}); "
                f"semantic evidence: {semantic_diagnostic}"
            )
        if white_count(image, (550, 366, 730, 402)) < 250:
            fail(f"{label}/{frame}: SCORE heading is missing or obstructed")
        # Screenshot coordinates increase downward. The dedicated progress row
        # must therefore finish at the SCORE strip's top edge, not share or
        # obscure any of its rows.
        progress_glyphs = white_count(image, (progress_left, strip[0] - LABEL_HEIGHT,
                                               progress_right, strip[0]))
        if progress_glyphs < 90:
            fail(
                f"{label}/{frame}: dedicated round-progress row immediately above SCORE "
                f"is missing (white pixels={progress_glyphs})"
            )
        if white_count(image, (550, 403, 895, 433)) < 180:
            fail(f"{label}/{frame}: score-table title is missing or overlaps the heading")
        shown, dark, glyphs = has_top_progress(image)
        if shown:
            fail(f"{label}/{frame}: duplicate top-center progress remained (dark={dark}, glyphs={glyphs})")


def assert_excluded(root, label, relative_path, expect_top):
    image = pixels(os.path.join(root, relative_path))
    strip = only_blue_strip(image, label)
    if strip != (350, 386):
        fail(f"{label}: unchanged score strip expected at rows 350..385, got {strip}")
    shown, dark, glyphs = has_top_progress(image)
    if shown != expect_top:
        fail(f"{label}: top-center progress expectation failed (shown={shown}, dark={dark}, glyphs={glyphs})")


def main():
    if len(sys.argv) != 3:
        fail("usage: assertions.py TEST_ROOT FONT_PATH")
    root, font_path = sys.argv[1:]
    assert_included(
        root,
        font_path,
        "first",
        1,
        ("winner-while-tab-held-early.png", "winner-while-tab-held-late.png"),
    )
    for label, played in (("resumed", 3), ("penultimate", 4)):
        assert_included(root, font_path, label, played)

    assert_excluded(root, "final", "final/summary.png", True)
    assert_excluded(root, "unlimited", "unlimited/summary.png", False)
    assert_excluded(root, "active-tab", "first/active-tab.png", True)

    for frame in ("next-round-first.png", "next-round-settled.png"):
        image = pixels(os.path.join(root, "first", frame))
        shown, dark, glyphs = has_top_progress(image)
        if not shown:
            fail(f"{frame}: next-round top progress was not restored (dark={dark}, glyphs={glyphs})")
        if blue_strip_bands(image):
            fail(f"{frame}: round-summary panel remained on the next-round frame")

    print("round-summary-progress: all visual behavior assertions passed")


if __name__ == "__main__":
    main()
