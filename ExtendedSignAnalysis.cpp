//===- ExtendedSignAnalysis.cpp - Transfer functions ------------------------------===//
//
// The transfer function: given what is known about an operation's operands,
// state what is known about its result.  ExtendedSign is deliberately a
// signed-integer analysis: unsigned-only operations and boolean i1 results are
// left unknown rather than mixing signed and unsigned interpretations of the
// same bit pattern.
//
//===----------------------------------------------------------------------===//

#include "ExtendedSignAnalysis.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Matchers.h"

using namespace mlir;

namespace extended_sign {

void ExtendedSignAnalysis::setToEntryState(ExtendedSignLattice *lattice) {
  propagateIfChanged(lattice, lattice->join(ExtendedSignState::top()));
}

LogicalResult
ExtendedSignAnalysis::visitOperation(Operation *op,
                                     ArrayRef<const ExtendedSignLattice *> operands,
                                     ArrayRef<ExtendedSignLattice *> results) {
  // Raising a result to top says "this operation could produce anything",
  // which is always a sound answer and is what every unhandled case does.
  auto unknown = [&] {
    setAllToEntryStates(results);
    return success();
  };

  // Only single-result integer operations are interesting here.  Calls, loads,
  // floats, and vectors all land in `unknown`.
  if (op->getNumResults() != 1 || !op->getResult(0).getType().isIntOrIndex())
    return unknown();
  ExtendedSignLattice *result = results[0];
  IntegerType resultIntegerType =
      dyn_cast<IntegerType>(op->getResult(0).getType());
  unsigned resultWidth = resultIntegerType ? resultIntegerType.getWidth() : 0;

  // ExtendedSign is a signed-integer analysis.  LLVM uses i1 as a boolean
  // bit, and signed interpretation of that bit conflicts with branch/select
  // truth semantics, so boolean results are left unknown.
  if (resultWidth == 1)
    return unknown();

  // Rule 1: a constant is zero or nonzero according to what it says.
  // This is the only rule that does not consult its operands, and without some
  // rule of this kind the analysis would have no facts to propagate at all.
  IntegerAttr value;
  if (matchPattern(op, m_Constant(&value))) {
    const llvm::APInt &intValue = value.getValue();

    ExtendedSignState state;
    if (intValue.isZero())
      state = Kind::Zero;
    else if (intValue.isOne())
      state = Kind::One;
    else if (intValue.isNegative())
      state = Kind::Negative;
    else if (intValue.isStrictlyPositive())
      state = Kind::Positive;
    else
      state = Kind::Top;

    propagateIfChanged(result, result->join(state));
    return success();
  }

  // Rule 2: `x & y` is zero if either operand is zero, since a zero operand
  // clears every bit.  Note what this rule does *not* say: two nonzero
  // operands tell us nothing, because 1 & 2 is 0.
  if (isa<LLVM::AndOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    // Bottom means the solver has not yet proved anything reaches this
    // operand.  Leaving the result alone keeps the analysis optimistic; the
    // solver will call back here once the operand moves up the lattice.
    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (lhs.kind == Kind::Zero || rhs.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }
  }

  // Rule 3: signed integer casts.
  if (isa<LLVM::SExtOp>(op)) {
    ExtendedSignState input = operands[0]->getValue();

    if (input.isBottom())
      return success();

    if (input.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }

    if (input.kind == Kind::One || input.kind == Kind::Negative ||
        input.kind == Kind::Positive || input.kind == Kind::Nonnegative ||
        input.kind == Kind::Nonpositive) {
      propagateIfChanged(result, result->join(input));
      return success();
    }
  }

  if (isa<LLVM::TruncOp>(op)) {
    ExtendedSignState input = operands[0]->getValue();

    if (input.isBottom())
      return success();

    if (input.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }

    if (input.kind == Kind::One) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::One)));
      return success();
    }
  }

  // Rule 4: `x + y`
  if (isa<LLVM::AddOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (lhs.kind == Kind::Zero || rhs.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(lhs.kind == Kind::Zero ? rhs : lhs));
      return success();
    }

    if (resultWidth > 2 && lhs.kind == Kind::One && rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
      return success();
    }

    if ((lhs.kind == Kind::One && rhs.kind == Kind::Negative) || 
        (lhs.kind == Kind::Negative && rhs.kind == Kind::One)) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }

    // The remaining addition facts are valid for mathematical integers, but
    // do not hold when fixed-width integer overflow is possible.
    
    // if ((lhs.kind == Kind::One && rhs.kind == Kind::Positive) || 
    //     (lhs.kind == Kind::Positive && rhs.kind == Kind::One)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::One && rhs.kind == Kind::Nonnegative) || 
    //     (lhs.kind == Kind::Nonnegative && rhs.kind == Kind::One)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Negative && rhs.kind == Kind::Negative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Positive && rhs.kind == Kind::Positive) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Nonnegative) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Nonpositive) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Positive) ||
    //     (lhs.kind == Kind::Positive && rhs.kind == Kind::Nonnegative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Negative) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Nonpositive)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
    //   return success();
    // }
  }

  // Rule 5: `x - y`
  if (isa<LLVM::SubOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();
    
    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (rhs.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(lhs));
      return success();
    }

    if (lhs.kind == Kind::Zero &&
        (rhs.kind == Kind::One || rhs.kind == Kind::Positive)) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
      return success();
    }

    if (lhs.kind == Kind::Zero && rhs.kind == Kind::Nonnegative) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }

    if (lhs.kind == Kind::One && rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }

    if (lhs.kind == Kind::One && rhs.kind == Kind::Positive) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }

    if (lhs.kind == Kind::Positive && rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
      return success();
    }

    // The remaining subtraction facts are valid for mathematical integers, but
    // do not hold when fixed-width integer overflow is possible.

    // if (lhs.kind == Kind::Zero && rhs.kind == Kind::Negative) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Zero && rhs.kind == Kind::Nonpositive) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::One && rhs.kind == Kind::Negative) ||
    //     (lhs.kind == Kind::Positive && rhs.kind == Kind::Negative) ||
    //     (lhs.kind == Kind::One && rhs.kind == Kind::Nonpositive) ||
    //     (lhs.kind == Kind::Positive && rhs.kind == Kind::Nonpositive) ||
    //     (lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Negative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Nonpositive) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Negative && rhs.kind == Kind::One) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Positive) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Nonnegative) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::One) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Positive)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
    //   return success();
    // }

    // if (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Nonnegative) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
    //   return success();
    // }
  }

  // Rule 6: `x * y`
  if (isa<LLVM::MulOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (lhs.kind == Kind::Zero || rhs.kind == Kind::Zero) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }

    if (lhs.kind == Kind::One || rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(lhs.kind == Kind::One ? rhs : lhs));
      return success();
    }

    // The remaining multiplication facts are valid for mathematical integers,
    // but do not hold when fixed-width integer overflow is possible.

    // if ((lhs.kind == Kind::Positive && rhs.kind == Kind::Positive) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Negative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Nonnegative) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Nonpositive)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Positive && rhs.kind == Kind::Negative) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Positive)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
    //   return success();
    // }

    // if ((lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Nonpositive) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Nonnegative) ||
    //     (lhs.kind == Kind::Positive && rhs.kind == Kind::Nonpositive) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Positive) ||
    //     (lhs.kind == Kind::Negative && rhs.kind == Kind::Nonnegative) ||
    //     (lhs.kind == Kind::Nonnegative && rhs.kind == Kind::Negative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
    //   return success();
    // }
  }

  // Rule 7: signed `x / y`
  if (isa<LLVM::SDivOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(lhs));
      return success();
    }

    if (lhs.kind == Kind::Zero &&
        (rhs.kind == Kind::Negative || rhs.kind == Kind::Positive)) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Zero)));
      return success();
    }

    if ((lhs.kind == Kind::One || lhs.kind == Kind::Positive ||
         lhs.kind == Kind::Nonnegative) &&
        rhs.kind == Kind::Positive) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
      return success();
    }

    if (((lhs.kind == Kind::Negative || lhs.kind == Kind::Nonpositive) &&
         rhs.kind == Kind::Positive) ||
        ((lhs.kind == Kind::One || lhs.kind == Kind::Positive ||
          lhs.kind == Kind::Nonnegative) &&
         rhs.kind == Kind::Negative)) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }

    // The remaining signed-division facts are valid for mathematical integers,
    // but do not hold when fixed-width integer overflow is possible.

    // if ((lhs.kind == Kind::Negative && rhs.kind == Kind::Negative) ||
    //     (lhs.kind == Kind::Nonpositive && rhs.kind == Kind::Negative)) {
    //   propagateIfChanged(result, result->join(ExtendedSignState(Kind::Nonnegative)));
    //   return success();
    // }
  }

  // Rule 8: signed min/max intrinsics.  These rules use the signed ordering
  // of the operands and do not involve arithmetic overflow.
  if (isa<LLVM::SMaxOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (lhs.kind == Kind::One || rhs.kind == Kind::One ||
        lhs.kind == Kind::Positive || rhs.kind == Kind::Positive) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
      return success();
    }

    if (lhs.kind == Kind::Zero || rhs.kind == Kind::Zero ||
        lhs.kind == Kind::Nonnegative || rhs.kind == Kind::Nonnegative) {
      propagateIfChanged(result,
                         result->join(ExtendedSignState(Kind::Nonnegative)));
      return success();
    }

    if (lhs.kind == Kind::Negative && rhs.kind == Kind::Negative) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
      return success();
    }

    if ((lhs.kind == Kind::Negative || lhs.kind == Kind::Nonpositive) &&
        (rhs.kind == Kind::Negative || rhs.kind == Kind::Nonpositive)) {
      propagateIfChanged(result,
                         result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }
  }

  if (isa<LLVM::SMinOp>(op)) {
    ExtendedSignState lhs = operands[0]->getValue();
    ExtendedSignState rhs = operands[1]->getValue();

    if (lhs.isBottom() || rhs.isBottom())
      return success();

    if (lhs.kind == Kind::Negative || rhs.kind == Kind::Negative) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Negative)));
      return success();
    }

    if (lhs.kind == Kind::Zero || rhs.kind == Kind::Zero ||
        lhs.kind == Kind::Nonpositive || rhs.kind == Kind::Nonpositive) {
      propagateIfChanged(result,
                         result->join(ExtendedSignState(Kind::Nonpositive)));
      return success();
    }

    if (lhs.kind == Kind::One && rhs.kind == Kind::One) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::One)));
      return success();
    }

    if ((lhs.kind == Kind::One || lhs.kind == Kind::Positive) &&
        (rhs.kind == Kind::One || rhs.kind == Kind::Positive)) {
      propagateIfChanged(result, result->join(ExtendedSignState(Kind::Positive)));
      return success();
    }

    if ((lhs.kind == Kind::One || lhs.kind == Kind::Positive ||
         lhs.kind == Kind::Nonnegative) &&
        (rhs.kind == Kind::One || rhs.kind == Kind::Positive ||
         rhs.kind == Kind::Nonnegative)) {
      propagateIfChanged(result,
                         result->join(ExtendedSignState(Kind::Nonnegative)));
      return success();
    }
  }

  return unknown();
}

} // namespace extended_sign
