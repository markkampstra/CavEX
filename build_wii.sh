#!/usr/bin/env bash
# Build the Wii .dol via devkitPPC. Sets the env vars the .pkg installer
# leaves out, sanity-checks the toolchain is present, then runs make.
#
# Usage:
#   ./build_wii.sh             # incremental build
#   ./build_wii.sh clean       # remove build artifacts
#   ./build_wii.sh -j4         # parallel build (or any flags make accepts)

set -euo pipefail

export DEVKITPRO=/opt/devkitpro
export DEVKITPPC="$DEVKITPRO/devkitPPC"
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$PATH"

if [ ! -x "$DEVKITPPC/bin/powerpc-eabi-gcc" ]; then
	echo "error: devkitPPC not found at $DEVKITPPC" >&2
	echo "install the macOS .pkg from https://github.com/devkitPro/pacman/releases" >&2
	exit 1
fi

cd "$(dirname "$0")"

make "$@"

if [ -f CavEX.dol ] && [ "${1:-}" != "clean" ]; then
	echo
	echo "build done -> $(pwd)/CavEX.dol"
	ls -lh CavEX.dol
fi
