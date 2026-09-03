#!/usr/bin/env python3
"""Deterministic pixel assertions for the shared-arena runtime harness."""

import argparse
import subprocess
import sys

WIDTH = 1280
HEIGHT = 900


def fail(message):
    raise AssertionError(message)


def load_rgb(path):
    width, height = map(int, subprocess.check_output(
        ["identify", "-format", "%w %h", path], text=True).split())
    if (width, height) != (WIDTH, HEIGHT):
        fail(f"{path}: expected {WIDTH}x{HEIGHT}, got {width}x{height}")
    pixels = subprocess.check_output(
        ["convert", path, "-alpha", "off", "-depth", "8", "rgb:-"])
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
        longest = max(longest, len(("Alpha", "Bravo", "Charlie", "Delta")[teams - 1]))
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
        longest = max(longest, len(("Alpha", "Bravo", "Charlie", "Delta")[teams - 1]))
    width = (longest + 6 + 20) * 16
    separator_height = 8 if teams and team_separators else 0
    height = 96 + 32 * rows + separator_height * max(0, teams - 1)
    first_y = 530 - height // 2
    if not teams:
        centers = [first_y + 32 * index for index in range(rows)]
        return WIDTH // 2 - width // 2, width, centers, []

    return WIDTH // 2 - width // 2, width, first_y, height


def assert_team_separators(data, label, left, width, separators):
    def rule_fraction(y):
        values = region_pixels(data, left, y, left + width, y + 1)
        return fraction(values, lambda rgb: min(rgb) >= 165 and max(rgb) - min(rgb) <= 85)

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


def assert_no_team_separators(data, label, left, width, parent_rows):
    for index in range(1, len(parent_rows)):
        # With separator treatment, the rule is five pixels above the next
        # parent row. Final Team summaries retain directly adjacent 32 px rows
        # and must not contain that full-table-width rule or its 8 px band.
        separator_y = parent_rows[index][0] - 5
        values = region_pixels(data, left, separator_y, left + width, separator_y + 2)
        coverage = fraction(
            values, lambda rgb: min(rgb) >= 165 and max(rgb) - min(rgb) <= 85)
        if coverage >= 0.85:
            fail(
                f"{label}: final Team summary retained separator {index}: "
                f"coverage={coverage:.3f}"
            )


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
    else:
        first_y, height = geometry[2:]
        # Ranking order can change when team point totals tie, and a 15-player
        # roster does not divide evenly among four teams. Locate the rendered
        # parent-row bands rather than assuming either team order or group size.
        parent_rows = []
        for y in range(first_y - 16, first_y + height - 80):
            values = region_pixels(data, left + 2, y, left + width - 2, y + 1)
            parent_fractions = parent_team_fractions(values)
            color = max(parent_fractions, key=parent_fractions.get)
            if parent_fractions[color] >= 0.25:
                if not parent_rows or y != parent_rows[-1][1]:
                    parent_rows.append([y, y + 1, color])
                else:
                    parent_rows[-1][1] = y + 1
        groups = [band[2] for band in parent_rows]
        if len(groups) != teams or len(set(groups)) != teams:
            fail(f"{label}: expected {teams} score group headers, got {groups}")
        if team_separators:
            separators = [parent_rows[index][0] - 5 for index in range(1, len(parent_rows))]
            if len(separators) != teams - 1:
                fail(f"{label}: expected {teams - 1} separator positions, got {separators}")
            assert_team_separators(data, label, left, width, separators)
        else:
            assert_no_team_separators(data, label, left, width, parent_rows)
        header_y = (parent_rows[0][0] + parent_rows[0][1]) // 2 - 64

    header = region_median(data, WIDTH // 2 - 80, header_y - 8,
                           WIDTH // 2 + 80, header_y + 8)
    if blue_strength(header) < 80:
        fail(f"{label}: SCORE overlay header missing: rgb={header}")
    return groups, header


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image")
    parser.add_argument("label")
    parser.add_argument("players", type=int)
    parser.add_argument("teams", type=int)
    parser.add_argument("--score", action="store_true")
    parser.add_argument("--final-score", action="store_true")
    parser.add_argument("--viewport-only", action="store_true")
    parser.add_argument("--without-ranking")
    args = parser.parse_args()

    data = load_rgb(args.image)
    horizontal, vertical, edge_count, edge_total = assert_shared_viewport(data, args.label)
    if args.viewport_only:
        print(f"{args.label}: viewport dividers={horizontal}/{vertical} "
              f"edge={edge_count}/{edge_total}")
    elif args.score or args.final_score:
        groups, header = assert_score_overlay(
            data, args.label, args.players, args.teams,
            team_separators=not args.final_score)
        print(f"{args.label}: score-rows={args.players + args.teams} groups={groups} "
              f"header={header} dividers={horizontal}/{vertical} edge={edge_count}/{edge_total}")
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
