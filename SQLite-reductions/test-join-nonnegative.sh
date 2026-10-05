#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 input.ll" >&2
  exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
build_dir=${BUILD_DIR:-"$repo_root/build"}
plugin=${PLUGIN:-}
mlir_translate=${MLIR_TRANSLATE:-mlir-translate}
mlir_opt=${MLIR_OPT:-mlir-opt}

if [ -z "$plugin" ]; then
  for candidate in "$build_dir"/ExtendedSignAnalysis.so "$build_dir"/ExtendedSignAnalysis.dylib; do
    if [ -f "$candidate" ]; then
      plugin=$candidate
      break
    fi
  done
fi

if [ -z "$plugin" ]; then
  echo "No ExtendedSign plugin found in $build_dir." >&2
  exit 2
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/sqlite-extended-sign-reduce.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT

"$mlir_translate" --import-llvm "$1" -o "$tmp_dir/input.mlir" >/dev/null 2>&1
"$mlir_opt" --load-pass-plugin="$plugin" \
  --pass-pipeline='builtin.module(extended-sign-analysis)' \
  "$tmp_dir/input.mlir" >"$tmp_dir/stdout.mlir" 2>"$tmp_dir/facts.mlir"

icmp_values=$(sed -n 's/.*\(%[0-9][0-9]*\) = llvm\.icmp.*/\1/p' "$tmp_dir/facts.mlir")
found_icmp_branch=false
for value in $icmp_values; do
  if grep -Eq "llvm\\.cond_br $value(,| )" "$tmp_dir/facts.mlir"; then
    found_icmp_branch=true
    break
  fi
done

if [ "$found_icmp_branch" != true ]; then
  exit 1
fi

grep -Eq '// argument: %.* is nonnegative' "$tmp_dir/facts.mlir"
