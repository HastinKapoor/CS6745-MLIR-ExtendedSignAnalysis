#!/bin/sh
# Run the extended sign analysis over an MLIR file.
#
#   ./run-extended-sign.sh input.mlir
#
# The plugin is ExtendedSignAnalysis.dylib on macOS and ExtendedSignAnalysis.so
# on Linux and WSL2, so probe for it rather than hard-coding a suffix.  Set
# PLUGIN or BUILD_DIR to override.
set -eu

BUILD_DIR="${BUILD_DIR:-build}"

if [ -z "${PLUGIN:-}" ]; then
  for candidate in "$BUILD_DIR"/ExtendedSignAnalysis.so "$BUILD_DIR"/ExtendedSignAnalysis.dylib; do
    if [ -f "$candidate" ]; then
      PLUGIN="$candidate"
      break
    fi
  done
fi

if [ -z "${PLUGIN:-}" ]; then
  echo "No plugin found in $BUILD_DIR; build it first (see README.md)." >&2
  exit 1
fi

if [ "$#" -lt 1 ]; then
  echo "usage: $0 input.mlir" >&2
  exit 2
fi

# stdout is the unchanged IR and stderr is the annotated listing; send the
# listing to this script's stdout so it can be piped or paged.
mlir-opt --load-pass-plugin="$PLUGIN" \
         --pass-pipeline='builtin.module(extended-sign-analysis)' \
         "$@" 2>&1 1>/dev/null
