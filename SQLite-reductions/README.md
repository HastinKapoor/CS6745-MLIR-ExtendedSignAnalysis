# SQLite reductions

This directory contains reductions made from `/tmp/sqlite3.ll`, the LLVM IR
compiled from SQLite.

The interestingness tests are generalized versions of the small examples in
`reductions/`: they do not require toy function names and instead look for the
facts that ExtendedSign prints in SQLite-derived MLIR.

## Files

- `test-join-nonnegative.sh`: accepts an LLVM IR file if, after importing to
  MLIR and running ExtendedSign, the output contains a branch and a block
  argument proven `nonnegative`.
- `test-smax-positive.sh`: accepts an LLVM IR file if ExtendedSign proves a
  signed `llvm.intr.smax` result is `positive`.
- `test-smin-after-smax-nonnegative.sh`: accepts an LLVM IR file if
  ExtendedSign first proves a signed `llvm.intr.smax` result is
  `nonnegative`, then proves a signed `llvm.intr.smin` using that result is
  also `nonnegative`.
- `sqlite-join-reduced.ll`: LLVM IR reduced from SQLite with
  `test-join-nonnegative.sh`.
- `sqlite-join-reduced.mlir`: MLIR imported from `sqlite-join-reduced.ll`.
- `sqlite-smax-reduced.ll`: LLVM IR reduced from SQLite with
  `test-smax-positive.sh`.
- `sqlite-smax-reduced.mlir`: MLIR imported from `sqlite-smax-reduced.ll`.
- `sqlite-smin-after-smax-reduced.ll`: LLVM IR reduced from SQLite with
  `test-smin-after-smax-nonnegative.sh`.
- `sqlite-smin-after-smax-reduced.mlir`: MLIR imported from
  `sqlite-smin-after-smax-reduced.ll`.

There is no SQLite `sdiv` reduction here because the current SQLite
ExtendedSign output did not contain an `llvm.sdiv` result proven
`nonnegative`.

## Reproduce

From the repository root:

```sh
PATH=/path/to/llvm-build/bin:$PATH \
llvm-reduce --test="$PWD/SQLite-reductions/test-join-nonnegative.sh" \
  --max-pass-iterations=1 -j 1 /tmp/sqlite3.ll

PATH=/path/to/llvm-build/bin:$PATH \
mlir-translate --import-llvm reduced.ll \
  -o SQLite-reductions/sqlite-join-reduced.mlir
```

Use `test-smax-positive.sh` and `sqlite-smax-reduced.mlir` for the signed-max
case. Use `test-smin-after-smax-nonnegative.sh` and
`sqlite-smin-after-smax-reduced.mlir` for the signed-min-after-signed-max case.
