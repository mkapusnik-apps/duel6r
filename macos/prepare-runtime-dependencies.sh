#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source "$root/macos/native-architecture.sh"
source "$root/macos/select-python.sh"
echo '=== Preparation: portable packaging checks ==='
"$D6R_PYTHON" "$root/tests/MacPackagingTests.py"
# No Homebrew runtime mutation or host-bottle fallback. All shipped libraries
# use the same checked source recipe and architecture-specific deployment floor.
echo '=== Preparation: private runtime dependencies ==='
"$D6R_PYTHON" -u "$root/macos/build-runtime.py"
