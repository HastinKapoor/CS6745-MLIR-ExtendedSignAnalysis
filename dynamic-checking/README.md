# Dynamic soundness checking

The `extended-sign-analysis` pass accepts `instrument=true`. With that option,
every nontrivial static fact becomes a runtime assertion immediately after the
value is defined (or at the start of a block for a block argument). For
example, a `Positive` fact becomes a signed `value > 0` comparison.

`ExtendedSignRuntime.c` implements the assertion handler. A failure reports the
fact kind, a stable check number, and `file:line:column`, then aborts. With
usable debug locations this points into the source program; otherwise it points
into the imported MLIR file.

`overflow.c` exercises signed min/max, arithmetic, and division over boundary
values and one million deterministic pseudorandom input pairs. The final case
adds two positive `INT_MAX` values under `-fwrapv`, producing `-2`.
It also verifies that wrapping addition, subtraction, and multiplication, plus
the exceptional signed-division shape, receive no ExtendedSign result fact.
The exact `INT_MIN / -1` input is not executed because it is undefined in both
C and LLVM IR; the closest safe dividend, `INT_MIN + 1`, is executed instead.
At `-O1`, Clang may canonicalize multiplication by two to a shift and division
by negative one to subtraction from zero. The test accepts those equivalent
forms, requires that the generated operation is present, and still checks that
ExtendedSign does not annotate its result.

`overflow-bug.mlir` is the minimal standalone reproducer: it adds two positive
`i32` maximum values, whose fixed-width result is `-2`.

`run-overflow-check.sh` performs the complete round trip twice:

1. C to LLVM IR to MLIR, with checks for the normal analysis. All 1,000,064
   input pairs must pass.
2. The same program with `inject-unsound-add-rule=true`, which deliberately
   enables the invalid rule `Positive + Positive -> Positive`. The generated
   assertion must catch the negative wrapped result.

Build the plugin, put matching LLVM/MLIR tools on `PATH`, and run:

```sh
cmake -S . -B build
cmake --build build
./dynamic-checking/run-overflow-check.sh
```

If the matching tools are not on `PATH`, set `MLIR_OPT`, `MLIR_TRANSLATE`, and
`CLANG`. Set `EXTENDED_SIGN_PLUGIN` when the plugin is outside `build/`.

The bug-injection option is a negative control for the checker. It is off by
default and does not affect normal analysis results.
