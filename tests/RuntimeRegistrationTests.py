#!/usr/bin/env python3
"""Verify the compiled cases each have exactly one independently budgeted CTest."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

build, binary = sys.argv[1:3]
enumeration_environment = dict(os.environ, D6R_TEST_LIST="1")
for execution_filter in ("D6R_TEST_FILTER", "D6R_TEST_EXACT"):
    enumeration_environment.pop(execution_filter, None)
names = subprocess.check_output([binary], env=enumeration_environment,
                                text=True, timeout=10).splitlines()
tests = json.loads(subprocess.check_output(["ctest", "--test-dir", build, "--show-only=json-v1"],
                                           text=True, timeout=10))["tests"]
registered = []
arena = []
runtime_command = None
for test in tests:
    properties = {entry["name"]: entry["value"] for entry in test["properties"]}
    primary = [label for label in properties.get("LABELS", []) if label.startswith("sanity-")]
    assert len(primary) == 1, (test["name"], primary)
    command = test.get("command", [])
    if test["name"].startswith(("network-runtime-", "network-model-")):
        runtime_command = command
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

if "--poisoned-environment" not in sys.argv[3:]:
    assert len(names) > 1, "poisoned-filter regression requires a nontrivial registry"
    poisoned = dict(os.environ, D6R_TEST_FILTER=names[0], D6R_TEST_EXACT="1", D6R_TEST_LIST="1")
    # Prove this exported filter really selects a strict subset in TestMain;
    # intentional filtering during execution must continue to work unchanged.
    filtered = subprocess.check_output([binary], env=poisoned, text=True, timeout=10).splitlines()
    assert filtered == [names[0]], filtered
    with tempfile.TemporaryDirectory(prefix="duel6r-registration-") as directory:
        root = Path(directory)
        registration = root / "network.cmake"
        subprocess.run([sys.executable, str(Path(__file__).with_name("RegisterNetworkRuntimeTests.py")),
                        binary, runtime_command[3], runtime_command[4], str(registration)],
                       env=poisoned, check=True, timeout=10)
        (root / "CTestTestfile.cmake").write_text(f'include("{registration}")\n')
        discovered = json.loads(subprocess.check_output(
            ["ctest", "--test-dir", str(root), "--show-only=json-v1"], text=True, timeout=10))["tests"]
        generated_names = [test["command"][test["command"].index("--case") + 1] for test in discovered]
        assert sorted(generated_names) == sorted(names), "exported filter truncated registration"
    # Independently exercise the coverage check with the same matching filter.
    # This fails if its own enumeration inherits the filter, even when the
    # generated CTest registry correctly contains every case.
    subprocess.run([sys.executable, __file__, build, binary, "--poisoned-environment"],
                   env=poisoned, check=True, timeout=15)
    print(f"Matching exported FILTER/EXACT retained all {len(names)} cases in discovery and coverage")
