#!/usr/bin/env bash
set -euo pipefail

# Usage: bash docker/resolve-tool-image.sh IMAGE DOCKERFILE [WAIT_SECONDS]
# Publication uses single-platform manifests. Resolve the committed tool inputs,
# never latest. Only a missing manifest is eligible for the publication wait.
image="$1"
version="$(bash docker/tool-image-version.sh "$2")"
deadline=$((SECONDS + ${3:-0}))
while ! manifest="$(docker manifest inspect --verbose "${image}:${version}" 2>&1)"; do
  case "$manifest" in
    *'manifest unknown'*|*'no such manifest'*)
      if (( SECONDS >= deadline )); then
        printf '%s\n' "$manifest" >&2
        echo "Tool image ${image}:${version} is not published; inspect Develop - Build Container Image." >&2
        exit 1
      fi
      echo "Waiting for ${image}:${version}" >&2
      sleep 30
      ;;
    *) printf '%s\n' "$manifest" >&2; exit 1 ;;
  esac
done
digest="$(jq -er '.Descriptor.digest' <<< "$manifest" | tr -d '\r')"
printf 'image=%s@%s\n' "$image" "$digest"
printf 'Selected %s:%s -> %s\n' "$image" "$version" "$digest" >&2
