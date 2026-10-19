#!/usr/bin/env bash
set -euo pipefail

# Run at the repository root after checking out the source to build. Git object
# identities avoid Windows checkout line-ending differences. No history is needed.
# Keep the publisher's path filter aligned with these tool inputs.
identity="$(git ls-tree -r HEAD -- "$1" .dockerignore docker/mbedtls docker/tool-image-version.sh | sha256sum)"
printf 'tool-%s\n' "${identity%% *}"
