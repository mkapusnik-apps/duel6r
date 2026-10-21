#!/usr/bin/env python3
"""Observe the rendered selection/roster transition, not merely a nonblank frame."""
import argparse
from pathlib import Path
import subprocess
import time


def transfer_state(pixels, roster_size):
    if len(pixels) != 850 * 700 * 3:
        raise ValueError("expected normalized 850x700 RGB menu")

    def rgb(x, y):
        offset = (y * 850 + x) * 3
        return tuple(pixels[offset:offset + 3])

    # Blank right side of UnrankedFree's fifth Persons row. ListBox's selected
    # backing is exactly (0, 0, 200); selection on another row cannot pass.
    selected = all(rgb(x, y) == (0, 0, 200)
                   for x in range(270, 290) for y in range(240, 250))
    # The fixture starts with two players. The third row's name area is white
    # until >> has actually added UnrankedFree. Exclude the list/frame borders.
    third_row = any(max(rgb(x, y)) < 128
                    for x in range(338, 425) for y in range(185, 199))
    return selected and third_row == (roster_size == 3), f"selected={selected} third-roster-row={third_row}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("roster_size", type=int, choices=(2, 3))
    parser.add_argument("--deadline", type=float, help="existing application's monotonic deadline")
    args = parser.parse_args()
    state = "not observed"
    while True:
        remaining = args.deadline - time.monotonic() if args.deadline is not None else 10
        if remaining <= 0:
            raise SystemExit(f"menu transfer state deadline: {state}")
        if args.deadline is not None:
            subprocess.run(["import", "-window", "root", str(args.image)], check=True, timeout=min(5, remaining))
            remaining = args.deadline - time.monotonic()
            if remaining <= 0:
                raise SystemExit(f"menu transfer state deadline: {state}")
        pixels = subprocess.check_output([
            "convert", str(args.image), "-crop", "1093x900+93+0", "+repage", "-filter", "point",
            "-resize", "850x700!", "-alpha", "off", "-depth", "8", "rgb:-"], timeout=min(5, remaining))
        ready, state = transfer_state(pixels, args.roster_size)
        if ready:
            print(f"menu transfer frame: {state}")
            return 0
        if args.deadline is None:
            print(f"menu transfer frame not ready: {state}")
            return 1
        time.sleep(min(.01, max(0, args.deadline - time.monotonic())))


if __name__ == "__main__":
    raise SystemExit(main())
