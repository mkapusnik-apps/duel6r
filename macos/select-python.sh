#!/usr/bin/env bash
# Sourced by native helpers. CI supplies setup-python's absolute executable;
# direct native builds use the documented Homebrew python@3.13 installation.
if [[ -z "${D6R_PYTHON:-}" ]]; then
  D6R_PYTHON="$(brew --prefix python@3.13)/bin/python3.13"
fi
D6R_PYTHON="$("$D6R_PYTHON" -c 'import sys; print(sys.executable)')"
export D6R_PYTHON
"$D6R_PYTHON" -c 'import platform, sys; print("Build Python:", sys.executable, platform.python_version(), platform.machine())'
