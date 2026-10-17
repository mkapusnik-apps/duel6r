#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source "$root/macos/native-architecture.sh"
# No Homebrew runtime mutation or host-bottle fallback. All shipped libraries
# use the same checked source recipe and architecture-specific deployment floor.
python3 "$root/macos/build-runtime.py"
