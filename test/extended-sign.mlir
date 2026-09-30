// Exercises the ExtendedSign transfer rules that remain sound for fixed-width
// LLVM integer arithmetic when overflow is possible.
module {
  llvm.func @transfers(%arg0: i32) -> i32 {
    // Rule 1: constants.
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    %neg = llvm.mlir.constant(-3 : i32) : i32
    %two = llvm.mlir.constant(2 : i32) : i32

    // Rule 2: `and` with a zero operand, either way round.
    %and_lhs = llvm.and %zero, %arg0 : i32
    %and_rhs = llvm.and %arg0, %zero : i32

    // Rule 3: addition rules that are still sound with overflow.
    %add_zero_rhs = llvm.add %two, %zero : i32
    %add_zero_lhs = llvm.add %zero, %neg : i32
    %add_one_one = llvm.add %one, %one : i32
    %add_one_neg = llvm.add %one, %neg : i32
    %add_neg_one = llvm.add %neg, %one : i32

    // Rule 4: subtraction rules that are still sound with overflow.
    %sub_rhs_zero = llvm.sub %neg, %zero : i32
    %sub_zero_one = llvm.sub %zero, %one : i32
    %sub_zero_pos = llvm.sub %zero, %two : i32
    %sub_one_one = llvm.sub %one, %one : i32
    %sub_one_pos = llvm.sub %one, %two : i32
    %sub_pos_one = llvm.sub %two, %one : i32
    %sub_zero_nonneg = llvm.sub %zero, %sub_pos_one : i32

    // Rule 5: multiplication rules that are still sound with overflow.
    %mul_zero_lhs = llvm.mul %zero, %arg0 : i32
    %mul_zero_rhs = llvm.mul %arg0, %zero : i32
    %mul_one_neg = llvm.mul %one, %neg : i32
    %mul_pos_one = llvm.mul %two, %one : i32
    %mul_one_nonpos = llvm.mul %one, %add_one_neg : i32

    // Rule 6: signed division rules that are still sound with overflow.
    %div_rhs_one = llvm.sdiv %neg, %one : i32
    %div_zero_pos = llvm.sdiv %zero, %two : i32
    %div_pos_pos = llvm.sdiv %two, %two : i32
    %div_nonneg_pos = llvm.sdiv %sub_pos_one, %two : i32
    %div_one_pos = llvm.sdiv %one, %two : i32
    %div_neg_pos = llvm.sdiv %neg, %two : i32
    %div_nonpos_pos = llvm.sdiv %add_one_neg, %two : i32
    %div_one_neg = llvm.sdiv %one, %neg : i32
    %div_pos_neg = llvm.sdiv %two, %neg : i32
    %div_nonneg_neg = llvm.sdiv %sub_pos_one, %neg : i32

    // Rule 7: signed casts.
    %zero64 = llvm.mlir.constant(0 : i64) : i64
    %one64 = llvm.mlir.constant(1 : i64) : i64
    %sext_zero = llvm.sext %zero : i32 to i64
    %sext_one = llvm.sext %one : i32 to i64
    %sext_neg = llvm.sext %neg : i32 to i64
    %sext_pos = llvm.sext %two : i32 to i64
    %sext_nonneg = llvm.sext %sub_pos_one : i32 to i64
    %sext_nonpos = llvm.sext %add_one_neg : i32 to i64
    %trunc_zero = llvm.trunc %zero64 : i64 to i32
    %trunc_one = llvm.trunc %one64 : i64 to i32

    // Rule 8: signed min/max intrinsics.
    %smax_pos_unknown = llvm.intr.smax(%two, %arg0) : (i32, i32) -> i32
    %smax_zero_unknown = llvm.intr.smax(%zero, %arg0) : (i32, i32) -> i32
    %smax_neg_neg = llvm.intr.smax(%neg, %neg) : (i32, i32) -> i32
    %smax_nonpos_neg = llvm.intr.smax(%add_one_neg, %neg) : (i32, i32) -> i32
    %smin_neg_unknown = llvm.intr.smin(%neg, %arg0) : (i32, i32) -> i32
    %smin_zero_unknown = llvm.intr.smin(%zero, %arg0) : (i32, i32) -> i32
    %smin_one_one = llvm.intr.smin(%one, %one) : (i32, i32) -> i32
    %smin_pos_one = llvm.intr.smin(%two, %one) : (i32, i32) -> i32
    %smin_nonneg_pos = llvm.intr.smin(%sub_pos_one, %two) : (i32, i32) -> i32

    // Mathematical-integer rules that are disabled because overflow can make
    // them unsound for fixed-width LLVM integers.
    %add_one_pos = llvm.add %one, %two : i32
    %add_pos_pos = llvm.add %two, %two : i32
    %add_neg_neg = llvm.add %neg, %neg : i32
    %sub_zero_neg = llvm.sub %zero, %neg : i32
    %sub_pos_neg = llvm.sub %two, %neg : i32
    %mul_pos_pos = llvm.mul %two, %two : i32
    %mul_neg_neg = llvm.mul %neg, %neg : i32
    %mul_pos_neg = llvm.mul %two, %neg : i32
    %div_neg_neg = llvm.sdiv %neg, %neg : i32
    %div_nonpos_neg = llvm.sdiv %add_one_neg, %neg : i32

    llvm.return %div_rhs_one : i32
  }

  // The lattice join of zero and one is nonnegative.
  llvm.func @join_nonnegative(%flag: i1) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %one = llvm.mlir.constant(1 : i32) : i32
    llvm.cond_br %flag, ^zero_edge, ^one_edge
  ^zero_edge:
    llvm.br ^exit(%zero : i32)
  ^one_edge:
    llvm.br ^exit(%one : i32)
  ^exit(%merged: i32):
    llvm.return %merged : i32
  }

  // The lattice join of zero and negative is nonpositive.
  llvm.func @join_nonpositive(%flag: i1) -> i32 {
    %zero = llvm.mlir.constant(0 : i32) : i32
    %neg = llvm.mlir.constant(-3 : i32) : i32
    llvm.cond_br %flag, ^zero_edge, ^neg_edge
  ^zero_edge:
    llvm.br ^exit(%zero : i32)
  ^neg_edge:
    llvm.br ^exit(%neg : i32)
  ^exit(%merged: i32):
    llvm.return %merged : i32
  }
}
