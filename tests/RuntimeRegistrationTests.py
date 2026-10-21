#!/usr/bin/env python3
"""Verify the compiled cases each have exactly one independently budgeted CTest."""
import json
import os
import subprocess
import sys

build, binary = sys.argv[1:]
names = subprocess.check_output([binary], env=dict(os.environ, D6R_TEST_LIST="1"),
                                text=True, timeout=10).splitlines()
tests = json.loads(subprocess.check_output(["ctest", "--test-dir", build, "--show-only=json-v1"],
                                           text=True, timeout=10))["tests"]
registered = []
arena = []
for test in tests:
    properties = {entry["name"]: entry["value"] for entry in test["properties"]}
    primary = [label for label in properties.get("LABELS", []) if label.startswith("sanity-")]
    assert len(primary) == 1, (test["name"], primary)
    command = test.get("command", [])
    if test["name"].startswith(("network-runtime-", "network-model-")):
        registered.append(command[command.index("--case") + 1])
        budget = float(command[command.index("--timeout") + 1])
        assert properties["TIMEOUT"] >= budget + 10
        assert primary == ["sanity-network"]
    if test["name"].startswith("shared-arena-behavior-"):
        arena.append(test["name"].removeprefix("shared-arena-behavior-"))
        assert primary == ["sanity-common"]
assert sorted(names) == sorted(registered), (set(names) - set(registered), set(registered) - set(names))
assert sorted(arena) == sorted(["deathmatch-2", "predator-3", "team2-ff-off-4", "team2-ff-on-4",
                               "team3-ff-off-6", "team3-ff-on-6", "team4-ff-off-8", "team4-ff-on-15"])
print(f"Registered all {len(names)} compiled network cases and {len(arena)} arena scenarios exactly once")
