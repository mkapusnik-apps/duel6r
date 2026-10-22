#!/usr/bin/env python3
"""Register every compiled native transport case with an independent budget."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

binary, destination = map(Path, sys.argv[1:])
environment = dict(os.environ, D6R_TEST_LIST="1")
for execution_filter in ("D6R_TEST_FILTER", "D6R_TEST_EXACT",
                         "D6R_RUN_FAKE_RESOLVER_TEST", "D6R_RUN_MALFORMED_RESOLVER_TEST"):
    environment.pop(execution_filter, None)
names = subprocess.check_output([str(binary)], env=environment,
                                text=True, timeout=10).splitlines()
assert names and len(names) == len(set(names)), "empty or duplicate transport registry"


def quote(value):
    assert "]==]" not in value
    return "[==[" + value + "]==]"


lines = []
for name in names:
    key = "darwin-session-transport-" + hashlib.sha256(name.encode()).hexdigest()[:12]
    # The one-way scenario has an unchanged 34s producer + 45s drain bound.
    # Other cases include up to 15 sequential two-second connection closes;
    # allow setup/observation overhead without changing any inner assertion.
    budget = 120 if name == "continuous one-way output liveness" else 60
    lines.append("add_test(" + " ".join(map(quote, [key, str(binary)])) + ")")
    lines.append(f"set_tests_properties({key} PROPERTIES TIMEOUT {budget} "
                 'LABELS "application;network;darwin;transport;sanity-network" '
                 + "ENVIRONMENT " + quote("D6R_TEST_FILTER=" + name + ";D6R_TEST_EXACT=1") + ")")
destination.write_text("\n".join(lines) + "\n")
