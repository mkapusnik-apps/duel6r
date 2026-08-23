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


def team_color(rgb):
    red, green, blue = rgb
    scores = {
        "Alpha": red - max(green, blue),
        "Bravo": green - max(red, blue),
        "Charlie": min(red, green) - blue,
        "Delta": min(red, blue) - green,
    }
    name = max(scores, key=scores.get)
    return name, scores[name]


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


def assert_live_ranking(data, label, players, teams):
    left, width, rows = ranking_geometry(players, teams)
    groups = []
    for index in range(rows):
        center_y = 12 + 16 * index
        rgb = region_median(data, left + 1, center_y - 5,
                            left + width - 1, center_y + 5)
        if not teams:
            if blue_strength(rgb) < 35:
                fail(f"{label}: live ranking row {index + 1}/{rows} missing: rgb={rgb}")
        else:
            color, strength = team_color(rgb)
            if strength < 12:
                fail(f"{label}: grouped ranking row {index + 1}/{rows} missing: rgb={rgb}")
            if strength >= 120:
                groups.append(color)
    if teams and (len(groups) != teams or len(set(groups)) != teams):
        fail(f"{label}: expected {teams} grouped ranking headers, got {groups}")
    if left + width != WIDTH - 3:
        fail(f"{label}: ranking is not anchored to the full viewport")
    return groups


def score_geometry(players, teams):
    rows = players + teams
    longest = 3
    if teams:
        longest = max(longest, len(("Alpha", "Bravo", "Charlie", "Delta")[teams - 1]))
    width = (longest + 6 + 20) * 16
    height = 96 + 32 * rows
    return WIDTH // 2 - width // 2, width, rows, 530 - height // 2


def assert_score_overlay(data, label, players, teams):
    left, width, rows, first_y = score_geometry(players, teams)
    groups = []
    for index in range(rows):
        center_y = first_y + 32 * index
        rgb = region_median(data, left + 2, center_y - 9,
                            left + width - 2, center_y + 9)
        if not teams:
            if blue_strength(rgb) < 30:
                fail(f"{label}: score row {index + 1}/{rows} missing: rgb={rgb}")
        else:
            color, strength = team_color(rgb)
            if strength >= 120:
                groups.append(color)
    if teams and (len(groups) != teams or len(set(groups)) != teams):
        fail(f"{label}: expected {teams} score group headers, got {groups}")

    header_y = first_y - 64
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
    args = parser.parse_args()

    data = load_rgb(args.image)
    horizontal, vertical, edge_count, edge_total = assert_shared_viewport(data, args.label)
    if args.score:
        groups, header = assert_score_overlay(
            data, args.label, args.players, args.teams)
        print(f"{args.label}: score-rows={args.players + args.teams} groups={groups} "
              f"header={header} dividers={horizontal}/{vertical} edge={edge_count}/{edge_total}")
    else:
        groups = assert_live_ranking(data, args.label, args.players, args.teams)
        print(f"{args.label}: live-rows={args.players + args.teams} players={args.players} "
              f"groups={groups} dividers={horizontal}/{vertical} edge={edge_count}/{edge_total}")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, subprocess.CalledProcessError) as error:
        print(f"shared-arena-image-assertions: {error}", file=sys.stderr)
        sys.exit(1)
