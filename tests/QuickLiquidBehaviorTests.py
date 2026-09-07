#!/usr/bin/env python3
"""Deterministic black-box coverage for Quick Liquid round starts and rise timing."""

import json
import pathlib
import shutil
import subprocess
import sys
import tempfile


SERVER = sys.argv[1]
TICK_RATE = 60
WIDTH = 12
HEIGHT = 7
PLAYERS = [
    "--player=1,101,Player 1",
    "--player=2,102,Player 2",
    "--player=3,103,Player 3",
    "--player=4,104,Player 4",
]
MODES = [
    ["--match-mode=deathmatch"],
    ["--match-mode=predator"],
    ["--match-mode=team-deathmatch", "--teams=2"],
]


def write_level(path, preferred_x, fallback_x):
    rows = [[0 for _ in range(WIDTH)] for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        rows[y][0] = rows[y][-1] = 1
    for x in range(1, WIDTH - 1):
        rows[0][x] = 4
    for x in fallback_x:
        rows[1][x] = 1
    for x in preferred_x:
        rows[3][x] = 1
    blocks = [block for y in reversed(range(HEIGHT)) for block in rows[y]]
    path.write_text(json.dumps({
        "width": WIDTH, "height": HEIGHT, "blocks": blocks, "elevators": [],
    }), encoding="utf-8")


def diagnostics(lines):
    return dict(line.split("=", 1) for line in lines if "=" in line
                and not line.startswith("session-result="))


def run(root, level, mode, quick_liquid, end_tick, players=PLAYERS, extra_actions="", seed=81081):
    arguments = [
        SERVER, "--authoritative-match", f"--resources={root}", f"--seed={seed}",
        "--actions-stdin", "--rounds=1", "--level-plan=fixed",
        f"--fixed-level=levels/{level}", f"--level=levels/{level}",
        f"--quick-liquid={'on' if quick_liquid else 'off'}",
        *mode, *players,
    ]
    end_sequence = 2 if extra_actions else 1
    completed = subprocess.run(
        arguments,
        input=extra_actions + f"{end_tick} {end_sequence} 1 0 end 0 0 0\n",
        text=True, capture_output=True, timeout=30,
    )
    assert completed.returncode == 0, (arguments, completed.stdout, completed.stderr)
    assert completed.stderr == "", completed.stderr
    lines = completed.stdout.splitlines()
    assert lines[:2] == [
        "authoritative-match-ended-intentionally",
        "Authoritative match ended by the host.",
    ], lines
    return diagnostics(lines)


def run_complete_rounds(root, level, rounds):
    arguments = [
        SERVER, "--authoritative-match", f"--resources={root}", "--seed=81081",
        "--scenario=complete", f"--rounds={rounds}", "--level-plan=fixed",
        f"--fixed-level=levels/{level}", f"--level=levels/{level}",
        "--quick-liquid=on", "--match-mode=deathmatch", *PLAYERS,
    ]
    completed = subprocess.run(arguments, text=True, capture_output=True, timeout=30)
    assert completed.returncode == 0, (arguments, completed.stdout, completed.stderr)
    assert completed.stderr == "", completed.stderr
    lines = completed.stdout.splitlines()
    assert lines[:2] == ["authoritative-match-completed", "Authoritative match completed."], lines
    result = json.loads(next(line for line in lines if line.startswith("session-result=")).split("=", 1)[1])
    return result, diagnostics(lines)


def player_blocks(result):
    return {
        int(fields[0]): (int(fields[5]) // 65536, int(fields[6]) // 65536)
        for fields in (encoded.split(":") for encoded in result["canonicalPlayers"].split(","))
    }


def events(result, kind):
    parsed = []
    for encoded in filter(None, result["canonicalEvents"].split(",")):
        fields = encoded.split(":", 7)
        if fields[2] == kind:
            parsed.append({"tick": int(fields[1]), "value": int(fields[7])})
    return parsed


with tempfile.TemporaryDirectory(prefix="duel6r-quick-liquid-") as temporary:
    root = pathlib.Path(temporary)
    (root / "data").mkdir()
    (root / "levels").mkdir()
    source = pathlib.Path.cwd()
    shutil.copyfile(source / "data" / "blocks.json", root / "data" / "blocks.json")
    shutil.copyfile(source / "data" / "config.script", root / "data" / "config.script")

    enough_preferred = set(range(1, WIDTH - 1))
    insufficient_preferred = {2, 8}
    fallback = {4, 6}
    write_level(root / "levels" / "enough.json", enough_preferred, set())
    write_level(root / "levels" / "insufficient.json", insufficient_preferred, fallback)

    # ENV-QL-001/002/003/007 and AC-ENV-QL-001: each selectable mode starts
    # every player above the surface that exists after two three-second rises.
    for mode in MODES:
        result = run(root, "enough.json", mode, True, 7 * TICK_RATE)
        rises = events(result, "water-level-changed")
        assert [rise["value"] for rise in rises] == [1, 2], (mode, rises)
        positions = player_blocks(result)
        assert len(set(positions.values())) == len(PLAYERS), (mode, positions)
        assert all(x in enough_preferred and y == 4 for x, y in positions.values()), (mode, positions)
        assert all(y > rises[-1]["value"] for _, y in positions.values()), (mode, positions, rises)

    # The production multi-round loop reconstructs the world three times on a
    # fixture whose only possible starting positions are all preferred.
    multi_round_result, multi_round_diagnostics = run_complete_rounds(root, "enough.json", 3)
    assert multi_round_result["completedRounds"] == 3, multi_round_result
    assert len(multi_round_result["rounds"]) == 3, multi_round_result
    starting_position_decisions = [decision for decision in multi_round_diagnostics["randomTrace"].split(",")
                                   if ":starting-position-order:" in decision]
    # Diagnostics intentionally retain the current round's random trace only;
    # seeing the complete final-round shuffle also guards against first-round-only setup.
    assert len(starting_position_decisions) == len(enough_preferred) - 1, \
        starting_position_decisions

    # ENV-QL-004/005/006/007 and AC-ENV-QL-002/003: all preferred positions
    # are consumed before fallback, the first rise is delayed to eight seconds,
    # and subsequent rises return to three-second spacing in every mode.
    for mode in MODES:
        result = run(root, "insufficient.json", mode, True, 12 * TICK_RATE)
        positions = player_blocks(result)
        occupied_preferred = {(x, y) for x, y in positions.values() if y == 4}
        occupied_fallback = {(x, y) for x, y in positions.values() if y == 2}
        assert occupied_preferred == {(x, 4) for x in insufficient_preferred}, (mode, positions)
        assert occupied_fallback == {(x, 2) for x in fallback}, (mode, positions)
        sudden_death = events(result, "sudden-death-started")
        rises = events(result, "water-level-changed")
        assert len(sudden_death) == 1 and len(rises) == 2, (mode, sudden_death, rises)
        assert rises[0]["tick"] - sudden_death[0]["tick"] in (8 * TICK_RATE, 8 * TICK_RATE + 1), \
            (mode, sudden_death, rises)
        assert rises[1]["tick"] - rises[0]["tick"] in (3 * TICK_RATE, 3 * TICK_RATE + 1), \
            (mode, rises)

    # ENV-QL-008 and AC-ENV-QL-003: with Quick Liquid off the legacy shuffled
    # placement can select fallback before preferred, and a later sudden death
    # retains the ordinary three-second first-rise interval.
    off_position_samples = [player_blocks(run(
        root, "insufficient.json", MODES[0], False, 0, players=PLAYERS[:2], seed=seed))
        for seed in range(1, 9)]
    assert any(y == 2 for positions in off_position_samples for _, y in positions.values()), \
        off_position_samples

    off_timing = run(
        root, "insufficient.json", MODES[0], False, 5 * TICK_RATE,
        players=PLAYERS[:3], extra_actions="0 1 1 0 remove 103 0 0\n")
    sudden_death = events(off_timing, "sudden-death-started")
    rises = events(off_timing, "water-level-changed")
    assert len(sudden_death) == 1 and len(rises) == 1, (sudden_death, rises)
    assert rises[0]["tick"] - sudden_death[0]["tick"] in (3 * TICK_RATE, 3 * TICK_RATE + 1), \
        (sudden_death, rises)

print("Quick Liquid behavior tests passed")
