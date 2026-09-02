#!/usr/bin/env python3
"""Black-box authoritative headless process outcome and machine-output tests."""

import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


SERVER = sys.argv[1]
BASE = [SERVER, "--authoritative-match", "--resources=.", "--seed=424242"]
PROCESS_TIMEOUT_SECONDS = int(os.environ.get("D6R_TEST_TIMEOUT", "30"))
GOLDEN = json.loads((pathlib.Path(__file__).parent / "golden" /
                     "authoritative-match-compact-combat-linux-x86_64.json").read_text(encoding="utf-8"))
DEFAULT_GOLDEN = json.loads((pathlib.Path(__file__).parent / "golden" /
                             "authoritative-match-canonical-default-linux-x86_64.json").read_text(encoding="utf-8"))


def run(*arguments, stdin="", base=None):
    completed = subprocess.run(
        (BASE if base is None else base) + list(arguments), input=stdin, text=True, capture_output=True,
        timeout=PROCESS_TIMEOUT_SECONDS
    )
    assert completed.stderr == "", completed.stderr
    lines = completed.stdout.splitlines()
    assert len(lines) >= 2, completed.stdout
    return completed.returncode, lines


def require_outcome(arguments, status, identifier, copy, stdin="", result=False):
    actual_status, lines = run(*arguments, stdin=stdin)
    assert actual_status == status, (arguments, actual_status, lines)
    assert lines[0] == identifier, lines
    assert lines[1] == copy, lines
    result_lines = [line for line in lines[2:] if line.startswith("session-result=")]
    assert bool(result_lines) == result, lines
    return json.loads(result_lines[0].split("=", 1)[1]) if result else None


def diagnostics_from(lines):
    return dict(line.split("=", 1) for line in lines if "=" in line
                and not line.startswith("session-result="))


interrupted = require_outcome(
    ["--scenario=interrupted"],
    0,
    "authoritative-match-interrupted-no-winner",
    "Authoritative match ended without a winner.",
    result=True,
)
assert interrupted["label"] == "Session only"
assert interrupted["state"] == "Interrupted"
assert interrupted["finalNoWinner"] is True
assert interrupted["completedRounds"] == 0
assert interrupted["seed"] == 424242
assert interrupted["players"][1]["departed"] is True
serialized = json.dumps(interrupted).lower()
for secret_or_persistent_field in ("credential", "endpoint", "password", "elo", "history"):
    assert secret_or_persistent_field not in serialized

# This uses the production CanonicalMatchRuntime and real level/world, not a test world seam.
canonical_arguments = [
    "--scenario=complete", "--diagnostic-fixture=compact-combat", "--rounds=1", "--level-plan=fixed",
    "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json",
]
canonical_a = require_outcome(
    canonical_arguments, 0, "authoritative-match-completed",
    "Authoritative match completed.", result=True
)
canonical_b = require_outcome(
    canonical_arguments, 0, "authoritative-match-completed",
    "Authoritative match completed.", result=True
)
assert canonical_a == canonical_b
assert canonical_a["rounds"] == [{
    "roundNumber": 1,
    "level": "levels/duel_01.json",
    "orientation": "Normal",
    "winnerPlayerIds": [201],
    "winningTeam": "",
    "noWinner": False,
    "rosterOrder": [101, 201],
}]
assert canonical_a["players"][0]["playerId"] == 201
canonical_killer = canonical_a["players"][0]["cumulative"]
canonical_victim = canonical_a["players"][1]["cumulative"]
assert canonical_killer["wins"] == 1
assert canonical_killer["kills"] == 1
assert canonical_killer["deaths"] == 0
assert canonical_killer["penalties"] == 0
assert canonical_victim["kills"] == 0
assert canonical_victim["deaths"] == 1
assert canonical_victim["penalties"] == 0

# Canonical production startup is content-derived unless the diagnostic fixture
# is explicitly requested. An immediate authorized end keeps this assertion short.
default_status, default_lines = run(
    "--scenario=interrupted", "--rounds=1", "--level-plan=fixed",
    "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json"
)
assert default_status == 0, default_lines
default_diagnostics = diagnostics_from(default_lines)
assert default_diagnostics["diagnosticFixture"] == "none"
assert default_diagnostics["diagnosticFixture"] == DEFAULT_GOLDEN["diagnosticFixture"]
assert int(default_diagnostics["stateDigest"]) == DEFAULT_GOLDEN["stateDigest"]
assert int(default_diagnostics["randomDecisionCount"]) == DEFAULT_GOLDEN["randomDecisionCount"]
assert int(default_diagnostics["randomDigest"]) == DEFAULT_GOLDEN["randomDigest"]
assert [[int(value) for value in checkpoint.split(":", 1)]
        for checkpoint in default_diagnostics["stateCheckpoints"].split(",")] \
       == DEFAULT_GOLDEN["stateCheckpoints"]
assert int(default_diagnostics["eventCount"]) == DEFAULT_GOLDEN["eventCount"]
assert default_diagnostics["forbiddenGlobalRandomAccessCount"] == "0"
assert default_diagnostics["forbiddenWallClockAccessCount"] == "0"
default_random = default_diagnostics["randomTrace"].split(",")
assert any(":starting-weapon:13:" in decision for decision in default_random)
assert any(":starting-ammo:1:0" in decision for decision in default_random)
assert all(int(player.split(":")[2]) == 15
           for player in default_diagnostics["canonicalPlayers"].split(","))
default_players = {fields[0]: {
    "life": int(fields[1]), "ammo": int(fields[2]), "shots": int(fields[3]),
    "hits": int(fields[4]), "positionX": int(fields[5]), "positionY": int(fields[6])
} for fields in (player.split(":") for player in default_diagnostics["canonicalPlayers"].split(","))}
assert default_players == DEFAULT_GOLDEN["canonicalPlayers"]

action_status, action_lines = run(
    "--actions-stdin", "--rounds=1", "--level-plan=fixed",
    "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json",
    stdin="0 1 1 0 end 0 0 0\n"
)
assert action_status == 0, action_lines
assert action_lines[:2] == ["authoritative-match-ended-intentionally", "Authoritative match ended by the host."]
action_diagnostics = diagnostics_from(action_lines)
assert action_diagnostics["diagnosticFixture"] == "none"
assert action_diagnostics["randomTrace"] == default_diagnostics["randomTrace"]
assert action_diagnostics["forbiddenGlobalRandomAccessCount"] == "0"
assert action_diagnostics["forbiddenWallClockAccessCount"] == "0"
assert all(int(player.split(":")[2]) == 15
           for player in action_diagnostics["canonicalPlayers"].split(","))

zero_status, zero_lines = run(
    "--seed=8", "--actions-stdin", "--rounds=1", "--level-plan=fixed",
    "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json",
    stdin=("0 1 1 101 input 0 0 0\n"
           "0 2 2 201 input 0 0 0\n"
           "1 3 1 0 end 0 0 0\n")
)
assert zero_status == 0, zero_lines
zero_diagnostics = diagnostics_from(zero_lines)
zero_input_shot_events = [event for event in zero_diagnostics["canonicalEvents"].split(",")
                          if ":shot-fired:" in event]

elevator_status, elevator_lines = run(
    "--scenario=interrupted", "--rounds=1", "--level-plan=fixed",
    "--fixed-level=levels/duel_21.json", "--level=levels/duel_21.json"
)
assert elevator_status == 0, elevator_lines
elevator_diagnostics = diagnostics_from(elevator_lines)
assert elevator_diagnostics["diagnosticFixture"] == "none"
elevators = [entity for entity in elevator_diagnostics["canonicalElevators"].split(",") if entity]
assert elevators
assert all(entity.split(":")[1:3] == ["elevator", "elevator"] for entity in elevators)

# Drive the real frozen-content loadouts through authoritative stdin actions.
# The final two starting-weapon decisions are the canonical world's two loadouts.
enabled_weapon_keys = [
    "pistol", "bazooka", "lightning", "shotgun", "plasma", "laser", "machine-gun",
    "triton", "uzi", "bow", "slime", "double-laser", "kiss-of-death",
]
disabled_weapon_keys = {"spray", "sling", "stopper-gun", "shit-thrower"}
fired_weapon_indices = set()
observed_projectile_types = set()
observed_weapon_event_kinds = set()
weapon_action_blocker = None
weapon_actions = (
    "0 1 1 101 input 0 0 16\n"
    "0 2 2 201 input 0 0 16\n"
    "120 3 1 101 input 0 0 0\n"
    "120 4 2 201 input 0 0 0\n"
    "121 5 1 0 end 0 0 0\n"
)
for weapon_seed in range(1, 97):
    weapon_status, weapon_lines = run(
        f"--seed={weapon_seed}", "--actions-stdin", "--rounds=1", "--level-plan=fixed",
        "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json", stdin=weapon_actions
    )
    assert weapon_status == 0, weapon_lines
    weapon_diagnostics = diagnostics_from(weapon_lines)
    assert weapon_diagnostics["diagnosticFixture"] == "none"
    all_decisions = [decision.split(":") for decision in weapon_diagnostics["randomTrace"].split(",")]
    decisions = [decision for index, decision in enumerate(all_decisions[:-1])
                 if decision[1] == "starting-weapon" and all_decisions[index + 1][1] == "player-orientation"]
    assert len(decisions) == 2, (weapon_seed, decisions)
    loadout_indices = {int(decision[3]) for decision in decisions}
    fired_players = {int(event.split(":", 7)[4])
                     for event in weapon_diagnostics["canonicalEvents"].split(",")
                     if ":shot-fired:" in event}
    observed_weapon_event_kinds.update(event.split(":", 7)[2]
                                       for event in weapon_diagnostics["canonicalEvents"].split(","))
    if fired_players == {101, 201}:
        fired_weapon_indices.update(loadout_indices)
    elif 9 in loadout_indices and len(fired_players) == 1:
        bow_player = ({101, 201} - fired_players).pop()
        bow_participant = 1 if bow_player == 101 else 2
        other_player = 201 if bow_player == 101 else 101
        other_participant = 2 if bow_player == 101 else 1
        bow_actions = (
            f"0 1 {other_participant} {other_player} input 0 0 0\n"
            f"0 2 {bow_participant} {bow_player} input 0 0 16\n"
            f"120 3 {bow_participant} {bow_player} input 0 0 0\n"
            "121 4 1 0 end 0 0 0\n"
        )
        bow_status, bow_lines = run(
            f"--seed={weapon_seed}", "--actions-stdin", "--rounds=1", "--level-plan=fixed",
            "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json", stdin=bow_actions
        )
        assert bow_status == 0, bow_lines
        bow_events = diagnostics_from(bow_lines)["canonicalEvents"].split(",")
        if any(":shot-fired:" in event and int(event.split(":", 7)[4]) == bow_player
               for event in bow_events):
            fired_weapon_indices.add(9)
        elif weapon_action_blocker is None:
            weapon_action_blocker = (weapon_seed, bow_player, bow_lines)
    for projectile in filter(None, weapon_diagnostics["canonicalProjectiles"].split(",")):
        fields = projectile.split(":")
        observed_projectile_types.add(fields[2])
        expected_types = {enabled_weapon_keys[index] for index in loadout_indices}
        assert fields[2] in expected_types, (weapon_seed, fields[2], expected_types, decisions)
    if fired_weapon_indices == set(range(len(enabled_weapon_keys))):
        break
missing_fired_weapon_indices = set(range(len(enabled_weapon_keys))) - fired_weapon_indices
assert observed_projectile_types
assert observed_projectile_types.isdisjoint(disabled_weapon_keys)
assert "tree-burned" in observed_weapon_event_kinds

# Enable the normally disabled Shit Thrower through real frozen gameplay content.
# The first action stream retains real projectiles in the canonical snapshot. The
# second proves hits apply slowdown, a slowed player can still submit a real fire
# action, releasing the action prevents phantom shots, and both effects expire.
with tempfile.TemporaryDirectory(prefix="duel6r-shit-thrower-") as frozen_directory:
    frozen_root = pathlib.Path(frozen_directory)
    shutil.copytree("data", frozen_root / "data")
    shutil.copytree("levels", frozen_root / "levels")
    config_lines = ["volume 128", "music on"]
    config_lines.extend(f"gun {index} false" for index in range(16))
    config_lines.append("gun 16 true")
    (frozen_root / "data" / "config.script").write_text(
        "\n".join(config_lines) + "\n", encoding="utf-8")
    shit_base = [SERVER, "--authoritative-match", f"--resources={frozen_root}"]
    shit_arguments = [
        "--actions-stdin", "--rounds=1", "--level-plan=fixed",
        "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json",
    ]

    projectile_actions = (
        "0 1 1 101 input 0 0 18\n"
        "0 2 2 201 input 0 0 17\n"
        "120 3 1 101 input 0 0 0\n"
        "120 4 2 201 input 0 0 0\n"
        "121 5 1 0 end 0 0 0\n"
    )
    projectile_status, projectile_lines = run(
        "--seed=8", *shit_arguments, stdin=projectile_actions, base=shit_base)
    assert projectile_status == 0, projectile_lines
    projectile_diagnostics = diagnostics_from(projectile_lines)
    shit_projectiles = [entry.split(":") for entry in
                        projectile_diagnostics["canonicalProjectiles"].split(",") if entry]
    assert len(shit_projectiles) == 2, projectile_diagnostics["canonicalProjectiles"]
    assert {entry[2] for entry in shit_projectiles} == {"shit-thrower"}
    assert {int(entry[3]) for entry in shit_projectiles} == {101, 201}

    lifecycle_actions = (
        "0 1 1 101 input 0 0 16\n"
        "250 2 1 101 input 0 0 0\n"
        "300 3 2 201 input 0 0 16\n"
        "301 4 2 201 input 0 0 0\n"
        "1300 5 1 0 end 0 0 0\n"
    )
    lifecycle_status, lifecycle_lines = run(
        "--seed=69", *shit_arguments, stdin=lifecycle_actions, base=shit_base)
    assert lifecycle_status == 0, lifecycle_lines
    lifecycle_diagnostics = diagnostics_from(lifecycle_lines)
    lifecycle_events = lifecycle_diagnostics["canonicalEvents"].split(",")
    lifecycle_transitions = lifecycle_diagnostics["canonicalStateTransitions"].split(",")
    assert any(":239:shot-hit:" in event and ":101:201:life-delta:0" in event
               for event in lifecycle_events), lifecycle_events
    assert any(":301:shot-fired:" in event and ":201:0:shot-count:1" in event
               for event in lifecycle_events), lifecycle_events
    assert sum(":shot-fired:" in event and ":201:0:shot-count:1" in event
               for event in lifecycle_events) == 1, lifecycle_events
    expiries = [transition for transition in lifecycle_transitions
                if ":temporary-slowdown-expired:" in transition]
    assert len(expiries) == 2, lifecycle_transitions
    assert {int(expiry.split(":", 7)[4]) for expiry in expiries} == {101, 201}
    for encoded in lifecycle_diagnostics["canonicalPlayerStates"].split(","):
        fields = encoded.split(":")
        assert fields[1] == "shit-thrower"
        assert int(fields[6]) == 0
        assert fields[9] == "1"
    assert lifecycle_diagnostics["forbiddenGlobalRandomAccessCount"] == "0"
    assert lifecycle_diagnostics["forbiddenWallClockAccessCount"] == "0"

# Exercise real multi-round team gameplay. Team identity remains derived from the
# immutable roster order while the canonical world traverses sudden death and water.
team_status, team_lines = run(
    "--scenario=complete", "--diagnostic-fixture=compact-combat",
    "--match-mode=team-deathmatch", "--teams=2", "--rounds=3",
    "--level-plan=fixed", "--fixed-level=levels/duel_01.json", "--level=levels/duel_01.json"
)
assert team_status == 0, team_lines
team_result = json.loads(next(line for line in team_lines if line.startswith("session-result=")).split("=", 1)[1])
assert team_result["mode"] == "Team deathmatch"
assert team_result["completedRounds"] == 3
assert [player["team"] for player in sorted(team_result["players"], key=lambda row: row["rosterOrder"])] \
       == ["Alpha", "Bravo"]
assert all(round_result["rosterOrder"] == [101, 201] for round_result in team_result["rounds"])
assert all(len(player["rounds"]) == 3 for player in team_result["players"])
team_diagnostics = diagnostics_from(team_lines)
assert ":sudden-death-started:" in team_diagnostics["canonicalEvents"]
assert ":water-level-changed:" in team_diagnostics["canonicalEvents"]
assert team_diagnostics["forbiddenGlobalRandomAccessCount"] == "0"
assert team_diagnostics["forbiddenWallClockAccessCount"] == "0"

# Preserve bounded diagnostics needed to compare canonical Linux/Windows semantics.
diagnostic_status, diagnostic_lines = run(*canonical_arguments)
assert diagnostic_status == 0
diagnostics = dict(line.split("=", 1) for line in diagnostic_lines if "=" in line
                   and not line.startswith("session-result="))
assert diagnostics["diagnosticFixture"] == "compact-combat"
assert diagnostics["diagnosticFixture"] == GOLDEN["diagnosticFixture"]
assert int(diagnostics["stateDigest"]) != 0
assert int(diagnostics["randomDecisionCount"]) > 0
assert int(diagnostics["randomDigest"]) != 0
assert diagnostics["randomTrace"]
assert diagnostics["canonicalPlayers"]
assert int(diagnostics["eventCount"]) > 0
assert diagnostics["cleanupConfirmed"] == "true"
assert diagnostics["forbiddenGlobalRandomAccessCount"] == "0"
assert diagnostics["forbiddenWallClockAccessCount"] == "0"

assert int(diagnostics["stateDigest"]) == GOLDEN["stateDigest"], (
    int(diagnostics["stateDigest"]), GOLDEN["stateDigest"])
assert int(diagnostics["randomDecisionCount"]) == GOLDEN["randomDecisionCount"]
assert int(diagnostics["randomDigest"]) == GOLDEN["randomDigest"]
checkpoints = [[int(field) for field in checkpoint.split(":", 1)]
               for checkpoint in diagnostics["stateCheckpoints"].split(",")]
assert checkpoints == GOLDEN["stateCheckpoints"]

events = []
for encoded in diagnostics["canonicalEvents"].split(","):
    sequence, tick, kind, entity, player, target, category, value = encoded.split(":", 7)
    events.append({
        "sequence": int(sequence), "tick": int(tick), "kind": kind,
        "entity": int(entity), "player": int(player), "target": int(target),
        "category": category, "value": int(value),
    })
assert [event["sequence"] for event in events] == list(range(1, len(events) + 1))
assert all(events[index]["tick"] <= events[index + 1]["tick"] for index in range(len(events) - 1))
assert all(event["entity"] != 0 for event in events if event["kind"] == "entity-spawned")
assert len({event["entity"] for event in events if event["kind"] == "entity-spawned"}) == 16
event_kinds = {}
for event in events:
    event_kinds[event["kind"]] = event_kinds.get(event["kind"], 0) + 1
assert event_kinds == GOLDEN["eventKinds"]
assert len(events) == GOLDEN["eventCount"]
assert next(event for event in events if event["kind"] == "player-died") == {
    "sequence": 36, "tick": 335, "kind": "player-died", "entity": 282574488338438,
    "player": 101, "target": 201, "category": "life-delta", "value": -10,
}
assert next(event for event in events if event["kind"] == "player-killed") == {
    "sequence": 37, "tick": 335, "kind": "player-killed", "entity": 282574488338438,
    "player": 201, "target": 101, "category": "kill-count", "value": 1,
}

canonical_players = {}
for encoded in diagnostics["canonicalPlayers"].split(","):
    player, life, ammo, shots, hits, x, y = (int(field) for field in encoded.split(":"))
    canonical_players[str(player)] = {"life": life, "ammo": ammo, "shots": shots, "hits": hits,
                                         "positionX": x, "positionY": y}
for player_id, expected in GOLDEN["players"].items():
    actual = canonical_players[player_id]
    for field in ("life", "ammo", "shots", "hits"):
        assert actual[field] == expected[field]

random_decisions = diagnostics["randomTrace"].split(",")
assert random_decisions[0] == "1:round-orientation:2:1"
starting_weapons = [decision.split(":") for decision in random_decisions
                    if ":starting-weapon:" in decision]
assert starting_weapons
assert all(fields[2] == "13" and 0 <= int(fields[3]) < 13 for fields in starting_weapons)
assert any(":pickup-kind:" in decision for decision in random_decisions)
assert any(":pickup-position:" in decision for decision in random_decisions)
assert any(":bonus-type:" in decision for decision in random_decisions)
assert any(":bonus-duration:" in decision for decision in random_decisions)

require_outcome(
    ["--rounds=0"], 2, "authoritative-match-settings-invalid",
    "Match settings are invalid. Correct the settings and try again."
)
require_outcome(
    ["--scenario=content-unavailable"], 2, "authoritative-match-content-unavailable",
    "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application."
)

# The production preflight parses every frozen level before the first round,
# including levels that would only be reached later by a multi-round plan.
for malformed_level, plan_arguments in (
        ("duel_01.json", ["--level-plan=fixed", "--fixed-level=levels/duel_01.json",
                          "--level=levels/duel_01.json"]),
        ("duel_02.json", ["--level-plan=shuffle", "--rounds=3"])):
    with tempfile.TemporaryDirectory(prefix="duel6r-malformed-level-") as frozen_directory:
        frozen_root = pathlib.Path(frozen_directory)
        shutil.copytree("data", frozen_root / "data")
        shutil.copytree("levels", frozen_root / "levels")
        (frozen_root / "levels" / malformed_level).write_text("{", encoding="utf-8")
        malformed_base = [SERVER, "--authoritative-match", f"--resources={frozen_root}", "--seed=424242"]
        malformed_status, malformed_lines = run(*plan_arguments, base=malformed_base)
        assert malformed_status == 2, malformed_lines
        assert malformed_lines == [
            "authoritative-match-content-unavailable",
            "The match cannot start with the supported gameplay content. Restore the supported gameplay content and restart the application.",
        ]

# Non-fixed plans always use the complete frozen level set; legacy subset flags
# are rejected before gameplay starts rather than silently narrowing the pool.
for level_plan in ("shuffle", "random"):
    subset_status, subset_lines = run(
        f"--level-plan={level_plan}", "--level=levels/duel_01.json", "--rounds=3")
    assert subset_status == 2, subset_lines
    assert subset_lines == [
        "authoritative-match-settings-invalid",
        "Match settings are invalid. Correct the settings and try again.",
    ]

# A real canonical player remains weapon-locked for the complete fixed 0.5 s
# (30 ticks), becomes usable at the exact boundary, then accepts the still-held
# Pick action on the following tick. This deliberately uses a tiny valid frozen
# level so pickup collision and timing remain deterministic without a runtime seam.
with tempfile.TemporaryDirectory(prefix="duel6r-headless-pick-lock-") as frozen_directory:
    frozen_root = pathlib.Path(frozen_directory)
    shutil.copytree("data", frozen_root / "data")
    (frozen_root / "levels").mkdir()
    (frozen_root / "levels" / "pick.json").write_text(json.dumps({
        "width": 6,
        "height": 4,
        "blocks": [
            1, 1, 1, 1, 1, 1,
            1, 0, 0, 0, 0, 1,
            1, 0, 0, 0, 0, 1,
            1, 1, 1, 1, 1, 1,
        ],
        "elevators": [],
    }), encoding="utf-8")
    pick_config = ["volume 128", "music on"]
    pick_config.extend(f"gun {index} {'true' if index == 0 else 'false'}" for index in range(17))
    (frozen_root / "data" / "config.script").write_text(
        "\n".join(pick_config) + "\n", encoding="utf-8")
    pick_base = [SERVER, "--authoritative-match", f"--resources={frozen_root}", "--seed=1"]
    pick_arguments = [
        "--actions-stdin", "--rounds=1", "--level-plan=fixed",
        "--fixed-level=levels/pick.json", "--level=levels/pick.json",
    ]
    pick_prefix = (
        "0 1 1 101 input 0 0 0\n"
        "0 2 2 201 input 0 0 0\n"
        "120 3 2 201 input 0 0 34\n"
        "140 4 2 201 input 0 0 32\n"
    )
    expected_has_weapon = {191: "0", 192: "1", 193: "0"}
    for end_tick, expected in expected_has_weapon.items():
        pick_status, pick_lines = run(
            *pick_arguments, stdin=pick_prefix + f"{end_tick} 5 1 0 end 0 0 0\n", base=pick_base)
        assert pick_status == 0, pick_lines
        pick_diagnostics = diagnostics_from(pick_lines)
        player_states = {fields[0]: fields for fields in
                         (encoded.split(":") for encoded in
                          pick_diagnostics["canonicalPlayerStates"].split(","))}
        assert player_states["201"][9] == expected, (end_tick, player_states["201"])
        picked = [event for event in pick_diagnostics["canonicalEvents"].split(",")
                  if ":weapon-picked:" in event]
        assert len(picked) == 1 and picked[0].split(":")[1] == "162", (end_tick, picked)
require_outcome(
    ["--scenario=runtime-failure"], 3, "authoritative-match-runtime-failed",
    "The authoritative match stopped unexpectedly."
)
require_outcome(
    ["--actions-stdin"], 0, "authoritative-match-ended-intentionally",
    "Authoritative match ended by the host.", stdin="0 1 1 0 end 0 0 0\n"
)
require_outcome(
    ["--scenario=cleanup-failure", "--actions-stdin"], 4,
    "authoritative-match-shutdown-failed", "Authoritative match cleanup did not complete.",
    stdin="0 1 1 0 end 0 0 0\n"
)

# Parsing failures remain bounded machine output and do not disclose the supplied value.
oversized = "9" * 600
status, lines = run("--rounds=" + oversized)
assert status == 2
assert lines == [
    "authoritative-match-settings-invalid",
    "Match settings are invalid. Correct the settings and try again.",
]
assert oversized not in "\n".join(lines)

assert not zero_input_shot_events, zero_input_shot_events
assert not missing_fired_weapon_indices, (missing_fired_weapon_indices, weapon_action_blocker)

print("authoritative match process outcomes passed")
