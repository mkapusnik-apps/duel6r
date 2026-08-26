#!/usr/bin/env bash
set -euo pipefail

workspace_dir="${WORKSPACE_DIR:-/workspace}"
output="${TMPDIR:-/tmp}/duel6r-networking-prototype-tests"

c++ -std=c++17 -Wall -Wextra -Werror -I"${workspace_dir}" \
    "${workspace_dir}/tests/TestMain.cpp" \
    "${workspace_dir}/tests/NetworkingPrototypeTests.cpp" \
    "${workspace_dir}/source/network/Protocol.cpp" \
    "${workspace_dir}/source/network/ProtocolSerialization.cpp" \
    "${workspace_dir}/source/client/ConnectionPlan.cpp" \
    "${workspace_dir}/source/client/LocalServerLauncher.cpp" \
    "${workspace_dir}/source/client/LoopbackSession.cpp" \
    "${workspace_dir}/source/server/HeadlessServer.cpp" \
    "${workspace_dir}/source/server/ServerConfig.cpp" \
    -o "${output}"

"${output}"
