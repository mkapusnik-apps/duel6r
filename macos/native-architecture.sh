#!/usr/bin/env bash
# Sourced by native build helpers. Never treat Rosetta as a native Intel host.
[[ "$(uname -s)" == Darwin ]] || { echo "Native macOS is required." >&2; exit 1; }
export D6R_MACOS_ARCH="${D6R_MACOS_ARCH:-$(uname -m)}"
case "$D6R_MACOS_ARCH" in
  arm64) export D6R_MACOS_MINIMUM=14.0 ;;
  x86_64) export D6R_MACOS_MINIMUM=13.0 ;;
  *) echo "Unsupported macOS architecture: $D6R_MACOS_ARCH" >&2; exit 1 ;;
esac
if [[ "$D6R_MACOS_ARCH" != "$(uname -m)" || "$(sysctl -in sysctl.proc_translated 2>/dev/null || true)" == 1 ]]; then
  echo "Build on the matching native Mac architecture, without Rosetta." >&2
  exit 1
fi
[[ "$(sw_vers -productVersion | cut -d. -f1)" -ge "${D6R_MACOS_MINIMUM%%.*}" ]] || {
  echo "$D6R_MACOS_ARCH requires macOS $D6R_MACOS_MINIMUM or later." >&2; exit 1;
}
