#!/usr/bin/env python3
"""Discover the compiled registry, including its actual platform conditions."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

binary, resources, server, destination = map(Path, sys.argv[1:])
names = subprocess.check_output([str(binary)], env=dict(os.environ, D6R_TEST_LIST="1"),
                                text=True, timeout=10).splitlines()
assert names and len(names) == len(set(names)), "empty or duplicate runtime registry"
# These cases exercise pure models, not the SDL/GL or real-service fixtures.
models = ("issue-38 ", "NET-DIR browser retains", "NET-DIR response decoder",
          "NET-DIR mixed decoder", "second jump oracle")
lines = []
for name in names:
    model = name.startswith(models)
    key = ("network-model-" if model else "network-runtime-") + hashlib.sha256(name.encode()).hexdigest()[:12]
    budget = 30 if model else 90  # capacity case includes its unchanged 36s sustained loop
    arguments = [sys.executable, str(Path(__file__).with_name("NetworkFlatBundleHarness.py")),
                 str(binary), str(resources), str(server), "--case", name, "--timeout", str(budget),
                 "--diagnostics", str(destination.parent / key)]
    if model:
        arguments.append("--model")
    def quote(value):
        assert "]==]" not in value
        return "[==[" + value + "]==]"
    lines.append("add_test(" + " ".join(map(quote, [key] + arguments)) + ")")
    lines.append(f'set_tests_properties({key} PROPERTIES TIMEOUT {budget + 15} '
                 'LABELS "application;integration;network;runtime;presentation;reconnect;regression;sanity-network")')
destination.write_text("\n".join(lines) + "\n")
