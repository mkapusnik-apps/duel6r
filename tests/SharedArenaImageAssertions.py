#!/usr/bin/env python3
"""Deterministic pixel assertions for the shared-arena runtime harness."""

import argparse
import functools
import os
from pathlib import Path
import subprocess
import sys

WIDTH = 1280
HEIGHT = 900
TEAM_NAMES = ("Alpha", "Bravo", "Charlie", "Delta")
FONT = Path(__file__).resolve().parents[1] / "resources/data/font.ttf"


@functools.lru_cache(maxsize=8)
def glyph_mask(text):
    """The same shipped-font mask convention used by the final-score checks."""
    raw = subprocess.check_output([
        "convert", "-background", "black", "-fill", "white", "-font", str(FONT),
        "-pointsize", "32", f"label:{text}", "-resize", f"{len(text) * 16}x32!",
        "-depth", "8", "gray:-"], timeout=10)
    return [value >= 64 for value in raw]


def glyph_similarity(data, text, left, top, dark=False):
    expected = glyph_mask(text)
    observed = [(max(rgb) <= 60 if dark else min(rgb) >= 205)
                for rgb in region_pixels(data, left, top, left + len(text) * 16, top + 32)]
    total = sum(expected) + sum(observed)
    return 2 * sum(a and b for a, b in zip(expected, observed)) / total if total else 0


def _score_title_candidates(data, tops):
    """Fresh per-call ink for the union of the existing overlapping title windows."""
    if not tops:
        return []
    expected = glyph_mask("---SCORE---")
    expected_count = sum(expected)
    left, width = WIDTH // 2 - 88, 176
    first = tops[0]
    ink = [min(rgb) >= 205 for rgb in region_pixels(data, left, first, left + width, tops[-1] + 32)]
    scores = []
    for top in tops:
        offset = (top - first) * width
        observed = ink[offset:offset + width * 32]
        total = expected_count + sum(observed)
        similarity = 2 * sum(a and b for a, b in zip(expected, observed)) / total if total else 0
        scores.append((top, similarity))
    return scores


def score_team_row(data, left, width, top):
    # Renderer contract: a complete32px backing, black unindented team glyphs;
    # children use indented yellow/red glyphs. Arena color alone is not identity.
    colors = []
    for y in range(top, top + 32):
        colors.append(parent_team_fractions(region_pixels(data, left + 2, y, left + width - 2, y + 1)))
    matches = [name for name in TEAM_NAMES
               if min(row[name] for row in colors) >= .25
               and glyph_similarity(data, name, left, top + 1, dark=True) >= .42]
    if len(matches) > 1:
        fail(f"ambiguous score team glyphs at row {top}: {matches}")
    return matches[0] if matches else None


def fail(message):
    raise AssertionError(message)


def load_rgb(path):
    width, height = map(int, subprocess.check_output(
        ["identify", "-format", "%w %h", path], text=True, timeout=10).split())
    if (width, height) != (WIDTH, HEIGHT):
        fail(f"{path}: expected {WIDTH}x{HEIGHT}, got {width}x{height}")
    pixels = subprocess.check_output(
        ["convert", path, "-alpha", "off", "-depth", "8", "rgb:-"], timeout=10)
    expected = width * height * 3
    if len(pixels) != expected:
        fail(f"{path}: expected {expected} RGB bytes, got {len(pixels)}")
    return pixels


def pixel(data, x, y):
    offset = (y * WIDTH + x) * 3
    return data[offset], data[offset + 1], data[offset + 2]


def region_median(data, left, top, right, bottom):
    channels = [[], [], []]
    for y in range(top, bottom):
        for x in range(left, right):
            for channel, value in enumerate(pixel(data, x, y)):
                channels[channel].append(value)
    result = []
    for values in channels:
        values.sort()
        result.append(values[len(values) // 2])
    return tuple(result)


def blue_strength(rgb):
    red, green, blue = rgb
    return blue - max(red, green)


def region_pixels(data, left, top, right, bottom):
    return [pixel(data, x, y)
            for y in range(top, bottom)
            for x in range(left, right)]


def region_delta(first, second, left, top, right, bottom):
    total = count = 0
    for y in range(top, bottom):
        for x in range(left, right):
            rgb_first = pixel(first, x, y)
            rgb_second = pixel(second, x, y)
            total += sum(abs(a - b) for a, b in zip(rgb_first, rgb_second))
            count += 3
    return total / count


def fraction(values, predicate):
    return sum(predicate(rgb) for rgb in values) / len(values)


def blue_overlay_fraction(values):
    # A 178-alpha blue ranking rectangle guarantees this channel range for
    # every background pixel, independently of the animated arena beneath it.
    return fraction(values, lambda rgb: rgb[2] >= 175 and rgb[0] <= 82 and rgb[1] <= 82)


def parent_team_fractions(values):
    return {
        "Alpha": fraction(values, lambda rgb: rgb[0] >= 170 and rgb[1] <= 90 and rgb[2] <= 90),
        "Bravo": fraction(values, lambda rgb: rgb[1] >= 170 and rgb[0] <= 90 and rgb[2] <= 90),
        "Charlie": fraction(values, lambda rgb: rgb[0] >= 170 and rgb[1] >= 170 and rgb[2] <= 90),
        "Delta": fraction(values, lambda rgb: rgb[0] >= 170 and rgb[2] >= 170 and rgb[1] <= 90),
    }


def max_red_run(values):
    longest = current = 0
    for red, green, blue in values:
        if red >= 245 and green <= 20 and blue <= 20:
            current += 1
            longest = max(longest, current)
        else:
            current = 0
    return longest


def assert_shared_viewport(data, label):
    # Historical split views drew pure-red borders through these center bands.
    horizontal = max(max_red_run([pixel(data, x, y) for x in range(WIDTH)])
                     for y in range(444, 456))
    vertical = max(max_red_run([pixel(data, x, y) for y in range(HEIGHT)])
                   for x in range(634, 646))
    if horizontal >= WIDTH // 3:
        fail(f"{label}: split-like horizontal red divider run: {horizontal}px")
    if vertical >= HEIGHT // 3:
        fail(f"{label}: split-like vertical red divider run: {vertical}px")

    # Record edge coverage for diagnostics. Some shipped maps intentionally
    # leave black border areas, so this is not itself a pass/fail criterion.
    edge_samples = []
    for x in range(0, WIDTH, 8):
        edge_samples.extend((pixel(data, x, 0), pixel(data, x, HEIGHT - 1)))
    for y in range(0, HEIGHT, 8):
        edge_samples.extend((pixel(data, 0, y), pixel(data, WIDTH - 1, y)))
    non_black = sum(max(rgb) > 8 for rgb in edge_samples)
    return horizontal, vertical, non_black, len(edge_samples)


def ranking_geometry(players, teams):
    longest = 3
    if teams:
        longest = max(longest, *(len(name) for name in TEAM_NAMES[:teams]))
    width = (longest + 6) * 8
    return WIDTH - width - 3, width, players + teams


def assert_live_ranking(data, without_ranking, label, players, teams):
    left, width, rows = ranking_geometry(players, teams)
    groups = []
    for index in range(rows):
        center_y = 12 + 16 * index
        values = region_pixels(data, left + 1, center_y - 5,
                               left + width - 1, center_y + 5)
        delta = region_delta(data, without_ranking, left + 1, center_y - 5,
                             left + width - 1, center_y + 5)
        if delta < 12:
            fail(f"{label}: live ranking row {index + 1}/{rows} missing: delta={delta:.3f}")
        if not teams:
            continue
        else:
            parent_fractions = parent_team_fractions(values)
            color = max(parent_fractions, key=parent_fractions.get)
            parent_coverage = parent_fractions[color]
            if parent_coverage >= 0.25:
                groups.append(color)
    if teams and (len(groups) != teams or len(set(groups)) != teams):
        fail(f"{label}: expected {teams} grouped ranking headers, got {groups}")
    if left + width != WIDTH - 3:
        fail(f"{label}: ranking is not anchored to the full viewport")
    return groups


def score_geometry(players, teams, team_separators=True):
    rows = players + teams
    longest = 3
    if teams:
        longest = max(longest, *(len(name) for name in TEAM_NAMES[:teams]))
    width = (longest + 6 + 20) * 16
    separator_height = 8 if teams and team_separators else 0
    height = 96 + 32 * rows + separator_height * max(0, teams - 1)
    first_y = 530 - height // 2
    if not teams:
        centers = [first_y + 32 * index for index in range(rows)]
        return WIDTH // 2 - width // 2, width, centers, []

    return WIDTH // 2 - width // 2, width, first_y, height


def team_rule_fraction(data, left, width, y):
    values = region_pixels(data, left, y, left + width, y + 1)
    return fraction(values, lambda rgb: min(rgb) >= 165 and max(rgb) - min(rgb) <= 85)


def assert_team_separators(data, label, left, width, separators):
    def rule_fraction(y):
        return team_rule_fraction(data, left, width, y)
    for index, top in enumerate(separators, 1):
        rule = min(rule_fraction(top), rule_fraction(top + 1))
        clear_above = max(rule_fraction(y) for y in range(top - 3, top))
        clear_below = max(rule_fraction(y) for y in range(top + 2, top + 5))
        if rule < 0.85:
            fail(f"{label}: team separator {index} does not span the table: coverage={rule:.3f}")
        if clear_above >= 0.85 or clear_below >= 0.85:
            fail(
                f"{label}: team separator {index} is not a centered 2px rule in an 8px band: "
                f"above={clear_above:.3f} below={clear_below:.3f}"
            )


def assert_no_team_separators(data, label, left, width, centers):
    for index, (first, second) in enumerate(zip(centers, centers[1:]), 1):
        strongest = 0.0
        for y in range(first + 12, second - 11):
            values = region_pixels(data, left, y, left + width, y + 1)
            strongest = max(
                strongest,
                fraction(values, lambda rgb: min(rgb) >= 165 and max(rgb) - min(rgb) <= 85),
            )
        if strongest >= 0.85:
            fail(f"{label}: Deathmatch has a Team-like separator after row {index}: coverage={strongest:.3f}")


def assert_end_of_game_notice(data, label, font_path):
    if not font_path or not os.path.isfile(font_path):
        fail(f"{label}: --font must name the shipped font for final-score assertions")

    text = "End of Game"
    text_width = len(text) * 16
    text_height = 32
    surface_left = WIDTH // 2 - (text_width + 32) // 2
    surface_top = HEIGHT - 16 - (text_height + 16)
    surface_right = surface_left + text_width + 32
    surface_bottom = HEIGHT - 16
    text_left = surface_left + 16
    text_top = surface_top + 8

    # The fixed opaque surface makes the notice legible independently of the
    # arena and curtain. Check all padding strips, which are not interrupted by
    # glyphs, and the exact 16 px bottom inset.
    padding = (
        (surface_left, surface_top, surface_right, text_top),
        (surface_left, text_top + text_height, surface_right, surface_bottom),
        (surface_left, text_top, text_left, text_top + text_height),
        (text_left + text_width, text_top, surface_right, text_top + text_height),
    )
    for index, box in enumerate(padding, 1):
        coverage = fraction(
            region_pixels(data, *box),
            lambda rgb: rgb[2] >= 245 and rgb[0] <= 20 and rgb[1] <= 20,
        )
        if coverage < 0.98:
            fail(f"{label}: End of Game surface padding {index} is not opaque blue: {coverage:.3f}")

    expected = subprocess.check_output([
        "convert", "-background", "black", "-fill", "white", "-font", font_path,
        "-pointsize", "32", f"label:{text}", "-resize", f"{text_width}x{text_height}!",
        "-depth", "8", "gray:-",
    ])
    expected_mask = [value >= 64 for value in expected]
    observed_mask = [
        min(pixel(data, text_left + x, text_top + y)) >= 205
        for y in range(text_height)
        for x in range(text_width)
    ]
    overlap = sum(a and b for a, b in zip(expected_mask, observed_mask))
    total = sum(expected_mask) + sum(observed_mask)
    similarity = 2.0 * overlap / total if total else 0.0
    if similarity < 0.42:
        fail(f"{label}: exact End of Game text is missing or illegible: similarity={similarity:.3f}")

    # At the largest supported final Team table the production layout moves the
    # panel just enough to leave this entire 16 px band clear. Reject score text
    # or either blue UI surface bleeding into that dedicated gap.
    gap_values = region_pixels(data, surface_left, surface_top - 16, surface_right, surface_top)
    gap_blue = fraction(gap_values, lambda rgb: rgb[2] >= 175 and rgb[0] <= 82 and rgb[1] <= 82)
    gap_white = fraction(gap_values, lambda rgb: min(rgb) >= 165 and max(rgb) - min(rgb) <= 85)
    if gap_blue >= 0.85 or gap_white >= 0.20:
        fail(
            f"{label}: final score and End of Game notice are not visibly separate: "
            f"gap-blue={gap_blue:.3f} gap-white={gap_white:.3f}"
        )
    return similarity, (surface_left, surface_top, surface_right, surface_bottom)


@functools.lru_cache(maxsize=128)
def final_counter_mask(font_path, text):
    raw = subprocess.check_output([
        "convert", "-background", "black", "-fill", "white", "-font", font_path,
        "-pointsize", "32", f"label:{text}", "-resize", f"{len(text) * 16}x32!",
        "-depth", "8", "gray:-",
    ])
    return [value >= 64 for value in raw]


def assert_final_round_counter(data, label, font_path, played=1, rounds=1):
    text = f"Rounds: {played} | {rounds}"
    text_width = len(text) * 16
    text_height = 32
    surface_left = (WIDTH - text_width - 16) // 2
    surface_top = HEIGHT - 120
    surface_right = surface_left + text_width + 16
    surface_bottom = surface_top + 40
    text_left = surface_left + 8
    text_top = surface_top + 4

    # Compact footer: centered40px backing,8px sides and4px vertical padding.
    padding = (
        (surface_left, text_top, text_left, text_top + text_height),
        (text_left + text_width, text_top, surface_right, text_top + text_height),
        (surface_left, surface_top, surface_right, text_top),
        (surface_left, text_top + text_height, surface_right, surface_bottom),
    )
    for index, box in enumerate(padding, 1):
        coverage = fraction(
            region_pixels(data, *box),
            lambda rgb: max(rgb) <= 10,
        )
        if coverage < 0.98:
            fail(f"{label}: final round counter padding {index} is not opaque black: {coverage:.3f}")

    expected_mask = final_counter_mask(font_path, text)
    observed_mask = [
        min(pixel(data, text_left + x, text_top + y)) >= 205
        for y in range(text_height)
        for x in range(text_width)
    ]
    overlap = sum(a and b for a, b in zip(expected_mask, observed_mask))
    total = sum(expected_mask) + sum(observed_mask)
    similarity = 2.0 * overlap / total if total else 0.0
    if similarity < 0.42:
        fail(f"{label}: final round counter text is missing or illegible: similarity={similarity:.3f}")
    # Shared label glyphs must not hide a stale or incorrect played/total digit.
    for index, digit in enumerate(text):
        if not digit.isdigit():
            continue
        offsets = [y * text_width + x for y in range(text_height)
                   for x in range(index * 16, (index + 1) * 16)]
        # SDL glyph rasterization and ImageMagick's label rasterization differ
        # by at most one pixel here. Padding/full-label checks keep layout fixed.
        observations = [[min(pixel(data, text_left + index * 16 + x + dx,
                                   text_top + y + dy)) >= 205
                         for y in range(text_height) for x in range(16)]
                        for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
        scores = {}
        for candidate in "0123456789":
            mask = final_counter_mask(font_path, text[:index] + candidate + text[index + 1:])
            expected = [mask[offset] for offset in offsets]
            scores[candidate] = max(
                2.0 * sum(a and b for a, b in zip(expected, observed))
                / (sum(expected) + sum(observed))
                if sum(expected) + sum(observed) else 0.0
                for observed in observations)
        if max(scores, key=scores.get) != digit or scores[digit] < 0.42:
            fail(f"{label}: final round counter numeral {index} expected {digit}: {scores}")
    return similarity, (surface_left, surface_top, surface_right, surface_bottom)


def compact_final_geometry(players, teams):
    """Actual OVER-02 footprint for the harness's P00-style short names."""
    longest = max([3] + [len(name) for name in TEAM_NAMES[:teams]])
    table_width = (longest + 26) * 16
    row_height = (players + teams) * 32 + max(0, teams - 1) * 8
    navigation = 36 if row_height + 160 > HEIGHT - 152 else 0
    panel_height = min(160 + row_height + navigation, HEIGHT - 152)
    top = min((HEIGHT - panel_height) // 2, HEIGHT - 136 - panel_height)
    viewport = panel_height - 128 - navigation
    return {
        "left": (WIDTH - table_width) // 2, "width": table_width,
        "top": top, "height": panel_height, "first": top + 96,
        "bottom": top + 64 + viewport, "scroll": max(0, 32 + row_height - viewport),
        "name_width": (longest + 3) * 16,
    }


def assert_shared_final_body(data, label, players, teams, scroll=0):
    # Final-specific oracle. Keep the actual interim assertions below unchanged.
    geometry = compact_final_geometry(players, teams)
    left, table_width = geometry["left"], geometry["width"]
    right = left + table_width
    top = geometry["top"]
    strip = fraction(region_pixels(data, left - 21, top + 28, right + 21, top + 64),
                     lambda rgb: rgb[2] >= 245 and rgb[0] <= 20 and rgb[1] <= 20)
    title = glyph_similarity(data, "---SCORE---", WIDTH // 2 - 88, top + 32)
    if strip < .65 or title < .42:
        fail(f"{label}: compact interim score strip missing: blue={strip:.3f} title={title:.3f}")
    old_title = fraction(region_pixels(data, 50, 18, 150, 58),
                         lambda rgb: rgb[2] >= 245 and rgb[0] <= 20 and rgb[1] <= 20)
    if top > 64 and old_title > .85:
        fail(f"{label}: obsolete large final window/title remains")
    row_top, row_bottom = geometry["first"], geometry["bottom"]
    row_y = row_top - (geometry["scroll"] if scroll else 0)
    groups = []
    for group in range(teams or 1):
        count = players
        if group:
            if row_top <= row_y + 2 and row_y + 4 <= row_bottom:
                assert_team_separators(data, label, left, table_width, [row_y + 2])
            row_y += 8
        if teams:
            # The deterministic native fixture's only entirely hidden group at
            # the maximum scroll is its surviving Alpha group.
            color = "Alpha"
            if row_top <= row_y and row_y + 32 <= row_bottom:
                colors = parent_team_fractions(region_pixels(data, left + 2, row_y, right - 2, row_y + 32))
                color = max(colors, key=colors.get)
                if colors[color] < .25 or color in groups:
                    fail(f"{label}: full final team fill missing or repeated at {row_y}: {colors}")
                if glyph_similarity(data, color, left, row_y, dark=True) < .42:
                    fail(f"{label}: named final team row missing at {row_y}")
            elif row_y >= row_bottom:
                break
            groups.append(color)
            team = ("Alpha", "Bravo", "Charlie", "Delta").index(color)
            count = 1 + sum(player % teams == team for player in range(players))
        for row in range(count):
            if row_top <= row_y and row_y + 32 <= row_bottom:
                if not teams or row:
                    values = region_pixels(data, left + 5, row_y, left + geometry["name_width"], row_y + 32)
                    ink = fraction(values, lambda rgb: rgb[0] > 190 and rgb[2] < 80)
                    numeric = region_pixels(data, left + geometry["name_width"], row_y, right, row_y + 32)
                    yellow = fraction(numeric, lambda rgb: min(rgb[:2]) > 190 and rgb[2] < 80)
                    if ink < .005:
                        fail(f"{label}: reference final identity row missing at {row_y}")
                    if yellow < .005:
                        fail(f"{label}: yellow final values/pipes missing at {row_y}")
            row_y += 32
    if scroll and row_y != row_bottom:
        fail(f"{label}: final bottom does not align with the bounded row viewport: {row_y}")
    return groups, strip, top


def assert_score_overlay(data, label, players, teams, team_separators=True):
    geometry = score_geometry(players, teams, team_separators)
    left, width = geometry[:2]
    rows = players + teams
    groups = []
    if not teams:
        centers = geometry[2]
        for index, center_y in enumerate(centers):
            values = region_pixels(data, left + 2, center_y - 9,
                                   left + width - 2, center_y + 9)
            coverage = blue_overlay_fraction(values)
            if coverage < 0.25:
                fail(f"{label}: score row {index + 1}/{rows} missing: blue={coverage:.3f}")
        header_y = centers[0] - 64
        assert_no_team_separators(data, label, left, width, centers)
    else:
        first_y, height = geometry[2:]
        # Locate the real title glyph, including the renderer's final-panel
        # bounds adjustment, rather than deriving origin from arena-color bands.
        title_scores = _score_title_candidates(
            data, range(max(0, first_y - 144), min(HEIGHT - 32, first_y - 16)))
        title_top, title_similarity = max(title_scores, key=lambda item: item[1])
        if title_similarity < .42:
            fail(f"{label}: SCORE title glyphs missing")
        header_y = title_top + 16
        # The title's best raster-mask origin is not a quad/row origin (GL
        # coordinates may be fractional). Use it only to bound the table search:
        # one32px heading cell before the body, plus one cell after it so a real
        # appended header remains detectable. Do not quantize row starts to ink.
        body_height = rows * 32 + (teams - 1) * (8 if team_separators else 0)
        search_top = title_top + 32
        search_bottom = min(HEIGHT, title_top + 96 + body_height)
        separators = []
        if team_separators:
            # Discover the full-width rules independently of team-color pixels.
            # Their centered2px lines anchor the unpainted8px gutters, whose
            # translucent arena background can otherwise resemble a team row.
            previous_rule_y = None
            # The preceding column heading is translucent: a bright arena edge
            # there is not an inter-team rule. Start at the first team row;
            # glyph/quad raster offsets cannot hide a rule after a full group.
            for y in range(title_top + 64, search_bottom):
                if team_rule_fraction(data, left, width, y) >= 0.85:
                    if previous_rule_y != y - 1:
                        separators.append(y)
                    previous_rule_y = y
            if len(separators) != teams - 1:
                fail(f"{label}: expected {teams - 1} separator positions, got {separators}")
            assert_team_separators(data, label, left, width, separators)
        # Full-row backing and complete source-font word establish identity;
        # color alone only narrows candidate windows. Retain the original8px
        # gutter exclusion, independently of the glyph/quad raster alignment.
        gutters = {y for top in separators for y in range(top - 3, top + 5)}
        color_rows = {
            y: parent_team_fractions(region_pixels(data, left + 2, y, left + width - 2, y + 1))
            for y in range(search_top, search_bottom)
        }
        parent_rows = []
        for y in range(search_top, min(search_bottom - 31, HEIGHT - 32)):
            if any(row in gutters for row in range(y, y + 32)):
                continue
            if not any(min(color_rows[row][name] for row in range(y, y + 32)) >= .25
                       for name in TEAM_NAMES):
                continue
            name = score_team_row(data, left, width, y)
            if name:
                parent_rows.append([y, y + 32, name])
        groups = [band[2] for band in parent_rows]
        if len(groups) != teams or len(set(groups)) != teams:
            fail(f"{label}: expected {teams} score group headers, got {groups}")

    header = region_median(data, WIDTH // 2 - 80, header_y - 8,
                           WIDTH // 2 + 80, header_y + 8)
    if blue_strength(header) < 80:
        fail(f"{label}: SCORE overlay header missing: rgb={header}")
    return groups, header, header_y - 48


def assert_score_overlay_closed(data, with_score, label, players, teams):
    _, _, panel_top = assert_score_overlay(
        with_score, label + "-open-reference", players, teams)
    header_y = panel_top + 48
    # The SCORE title has static white glyphs on an opaque blue strip. Blue
    # alone is not evidence of that strip: shipped arena textures can be blue
    # at the same location. Compare the glyph pattern to the validated open
    # frame, independently of the animated arena and changing score rows.
    box = (WIDTH // 2 - 88, header_y - 16, WIDTH // 2 + 88, header_y + 16)
    expected = [min(rgb) >= 205 for rgb in region_pixels(with_score, *box)]
    observed = [min(rgb) >= 205 for rgb in region_pixels(data, *box)]
    if not any(expected):
        fail(f"{label}: open reference has no SCORE title glyphs")
    overlap = sum(a and b for a, b in zip(expected, observed))
    similarity = 2.0 * overlap / (sum(expected) + sum(observed))
    if similarity >= 0.80:
        fail(f"{label}: SCORE overlay header still visible: glyph-similarity={similarity:.3f}")
    return similarity


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    parser.add_argument("label")
    parser.add_argument("players", type=int)
    parser.add_argument("teams", type=int)
    parser.add_argument("--score", action="store_true")
    parser.add_argument("--final-score", action="store_true")
    parser.add_argument("--final-bottom", action="store_true", help="maximum-scroll four-team native fixture")
    parser.add_argument("--viewport-only", action="store_true")
    parser.add_argument("--live-ready", action="store_true")
    parser.add_argument("--start-prompt", action="store_true")
    parser.add_argument("--without-ranking")
    parser.add_argument("--without-score", help="validated open-summary image to compare with the closed state")
    parser.add_argument("--font")
    args = parser.parse_args()

    data = load_rgb(args.image)
    horizontal, vertical, edge_count, edge_total = assert_shared_viewport(data, args.label)
    if args.start_prompt:
        center = region_pixels(data, WIDTH // 2 - 130, HEIGHT // 2 - 8,
                               WIDTH // 2 + 130, HEIGHT // 2 + 8)
        if fraction(center, lambda rgb: rgb == (255, 204, 204)) < .25:
            fail(f"{args.label}: statistics confirmation panel not rendered")
        if fraction(center, lambda rgb: rgb[0] > 200 and rgb[1] < 80 and rgb[2] < 80) < .01:
            fail(f"{args.label}: confirmation text not rendered")
    elif args.live_ready:
        left, width, rows = ranking_geometry(args.players, args.teams)
        values = region_pixels(data, left + 1, 7, left + width - 1, 17)
        coverage = (max(parent_team_fractions(values).values()) if args.teams
                    else blue_overlay_fraction(values))
        if coverage < .25:
            fail(f"{args.label}: live ranking not rendered")
    elif args.viewport_only:
        print(f"{args.label}: viewport dividers={horizontal}/{vertical} "
              f"edge={edge_count}/{edge_total}")
    elif args.without_score:
        with_score = load_rgb(args.without_score)
        similarity = assert_score_overlay_closed(
            data, with_score, args.label, args.players, args.teams)
        print(f"{args.label}: SCORE overlay header absent: glyph-similarity={similarity:.3f}")
    elif args.score or args.final_score:
        groups, header, panel_top = (assert_shared_final_body(
            data, args.label, args.players, args.teams, scroll=1 if args.final_bottom else 0) if args.final_score else assert_score_overlay(
            data, args.label, args.players, args.teams, team_separators=True))
        notice = ""
        if args.final_score:
            similarity, bounds = assert_end_of_game_notice(data, args.label, args.font)
            counter_similarity, counter_bounds = assert_final_round_counter(data, args.label, args.font)
            counter_gap = HEIGHT - 120 - (panel_top + compact_final_geometry(args.players, args.teams)["height"])
            notice = (f" notice={bounds} text-similarity={similarity:.3f}"
                      f" counter={counter_bounds} counter-height=40"
                      f" counter-similarity={counter_similarity:.3f}"
                      f" counter-panel-gap={counter_gap}")
        print(f"{args.label}: score-rows={args.players + args.teams} groups={groups} "
              f"header={header}{notice} dividers={horizontal}/{vertical} "
              f"edge={edge_count}/{edge_total}")
    else:
        if not args.without_ranking:
            fail("live ranking assertions require --without-ranking")
        without_ranking = load_rgb(args.without_ranking)
        groups = assert_live_ranking(
            data, without_ranking, args.label, args.players, args.teams)
        print(f"{args.label}: live-rows={args.players + args.teams} players={args.players} "
              f"groups={groups} dividers={horizontal}/{vertical} edge={edge_count}/{edge_total}")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, subprocess.CalledProcessError) as error:
        print(f"shared-arena-image-assertions: {error}", file=sys.stderr)
        sys.exit(1)
