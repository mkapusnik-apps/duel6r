#!/usr/bin/env python3
"""Verify the compiled cases each have exactly one independently budgeted CTest."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def enumerate_tests(build):
    # Even --show-only creates Testing/Temporary/LastTest.log. Never point a
    # nested CTest at the active outer build: include its actual registrations
    # in a disposable metadata directory, with its own diagnostic destination.
    registration = Path(build).resolve() / "CTestTestfile.cmake"
    assert "]==]" not in str(registration)
    with tempfile.TemporaryDirectory(prefix="duel6r-ctest-enumeration-") as directory:
        (Path(directory) / "CTestTestfile.cmake").write_text(f'include([==[{registration}]==])\n')
        return json.loads(subprocess.check_output(
            ["ctest", "--test-dir", directory, "--show-only=json-v1"], text=True, timeout=10))["tests"]


build, binary = sys.argv[1:3]
enumeration_environment = dict(os.environ, D6R_TEST_LIST="1")
for execution_filter in ("D6R_TEST_FILTER", "D6R_TEST_EXACT"):
    enumeration_environment.pop(execution_filter, None)
names = subprocess.check_output([binary], env=enumeration_environment,
                                text=True, timeout=10).splitlines()
tests = enumerate_tests(build)
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
    # Simulate an active outer diagnostic stream while enumerating the complete
    # real registry (not a toy replacement). Do not mutate the live outer log.
    with tempfile.TemporaryDirectory(prefix="duel6r-ctest-outer-") as directory:
        outer = Path(directory)
        registration = Path(build).resolve() / "CTestTestfile.cmake"
        (outer / "CTestTestfile.cmake").write_text(f'include([==[{registration}]==])\n')
        diagnostics = outer / "Testing/Temporary"
        diagnostics.mkdir(parents=True)
        sentinels = {diagnostics / name: f"outer diagnostic {name}\n".encode()
                     for name in ("LastTest.log", "LastTest.log.tmp", "LastTestsFailed.log")}
        for path, content in sentinels.items():
            path.write_bytes(content)
        listed = enumerate_tests(outer)
        assert sorted(test["name"] for test in listed) == sorted(test["name"] for test in tests)
        for path, content in sentinels.items():
            assert path.read_bytes() == content, f"nested enumeration overwrote {path.name}"
    print(f"Isolated enumeration preserved outer logs and all {len(tests)} CTest registrations")
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
        discovered = enumerate_tests(root)
        generated_names = [test["command"][test["command"].index("--case") + 1] for test in discovered]
        assert sorted(generated_names) == sorted(names), "exported filter truncated registration"
    # Independently exercise the coverage check with the same matching filter.
    # This fails if its own enumeration inherits the filter, even when the
    # generated CTest registry correctly contains every case.
    subprocess.run([sys.executable, __file__, build, binary, "--poisoned-environment"],
                   env=poisoned, check=True, timeout=15)
    print(f"Matching exported FILTER/EXACT retained all {len(names)} cases in discovery and coverage")
