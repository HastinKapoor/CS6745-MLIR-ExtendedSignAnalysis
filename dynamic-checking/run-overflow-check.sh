#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
clang_bin=${CLANG:-clang}
work_dir=${WORK_DIR:-"$repo_dir/dynamic-checking/output"}
mlir_opt=${MLIR_OPT:-mlir-opt}
mlir_translate=${MLIR_TRANSLATE:-mlir-translate}

if [[ -n "${EXTENDED_SIGN_PLUGIN:-}" ]]; then
  plugin=$EXTENDED_SIGN_PLUGIN
elif [[ -f "$repo_dir/build/ExtendedSignAnalysis.so" ]]; then
  plugin="$repo_dir/build/ExtendedSignAnalysis.so"
else
  plugin="$repo_dir/build/ExtendedSignAnalysis.dylib"
fi

mkdir -p "$work_dir"

"$clang_bin" -O1 -g -fwrapv -S -emit-llvm \
  "$repo_dir/dynamic-checking/overflow.c" -o "$work_dir/overflow.ll"
"$mlir_translate" --import-llvm "$work_dir/overflow.ll" \
  -o "$work_dir/overflow.mlir"

instrument_and_link() {
  local name=$1
  local options=$2
  "$mlir_opt" --load-pass-plugin="$plugin" \
    --pass-pipeline="builtin.module(extended-sign-analysis{$options})" \
    "$work_dir/overflow.mlir" -o "$work_dir/$name.mlir" \
    2>"$work_dir/$name.facts.txt"
  "$mlir_translate" --mlir-to-llvmir "$work_dir/$name.mlir" \
    -o "$work_dir/$name.ll"
  "$clang_bin" "$work_dir/$name.ll" \
    "$repo_dir/dynamic-checking/ExtendedSignRuntime.c" \
    -o "$work_dir/$name"
}

instrument_and_link sound "instrument=true"

assert_result_is_unannotated() {
  local function=$1
  local operation_pattern=$2
  local section
  section=$(awk -v marker="@$function(" '
    /^  llvm\.func/ && index($0, marker) { inside = 1 }
    inside { print }
    inside && /^  }/ { exit }
  ' "$work_dir/sound.facts.txt")
  if ! grep -Eq "llvm\\.($operation_pattern) " <<<"$section"; then
    echo "error: $function has no expected operation in imported MLIR" >&2
    exit 1
  fi
  if grep -Eq "llvm\\.($operation_pattern).* // .* is " <<<"$section"; then
    echo "error: $function unexpectedly received an ExtendedSign fact" >&2
    exit 1
  fi
}

assert_result_is_unannotated unchecked_add add
assert_result_is_unannotated unchecked_subtract 'add|sub'
assert_result_is_unannotated unchecked_multiply 'mul|shl'
assert_result_is_unannotated unchecked_divide 'sdiv|sub'

"$work_dir/sound"
echo "sound analysis: 1,000,064 input pairs passed"
echo "overflow-sensitive add/subtract/multiply/divide results remained unknown"

instrument_and_link unsound \
  "instrument=true inject-unsound-add-rule=true"
set +e
"$work_dir/unsound" 2>"$work_dir/unsound.failure.txt"
status=$?
set -e

if [[ $status -eq 0 ]]; then
  echo "error: deliberately unsound rule was not detected" >&2
  exit 1
fi
if ! grep -q "extended-sign assertion failed:.*expected positive" \
  "$work_dir/unsound.failure.txt"; then
  echo "error: executable failed without the expected assertion" >&2
  cat "$work_dir/unsound.failure.txt" >&2
  exit 1
fi

cat "$work_dir/unsound.failure.txt"
echo "unsound analysis: overflow bug detected (exit status $status)"
