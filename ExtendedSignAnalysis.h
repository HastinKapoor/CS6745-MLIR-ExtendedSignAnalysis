//===- ExtendedSignAnalysis.h - Sparse forward analysis over ExtendedSignState ------------===//

#ifndef EXTENDED_SIGN_ANALYSIS_H
#define EXTENDED_SIGN_ANALYSIS_H

#include "ExtendedSignDomain.h"
#include "mlir/Analysis/DataFlow/SparseAnalysis.h"

namespace extended_sign {

using ExtendedSignLattice = mlir::dataflow::Lattice<ExtendedSignState>;

class ExtendedSignAnalysis
    : public mlir::dataflow::SparseForwardDataFlowAnalysis<ExtendedSignLattice> {
public:
  explicit ExtendedSignAnalysis(mlir::DataFlowSolver &solver,
                                bool injectUnsoundAddRule = false)
      : SparseForwardDataFlowAnalysis(solver),
        injectUnsoundAddRule(injectUnsoundAddRule) {}

  /// Transfer function: given the states of `op`'s operands, set the states of
  /// its results.  Must be monotone in the operand states.
  mlir::LogicalResult
  visitOperation(mlir::Operation *op,
                 llvm::ArrayRef<const ExtendedSignLattice *> operands,
                 llvm::ArrayRef<ExtendedSignLattice *> results) override;

  /// The state of anything entering the analysis from outside: function
  /// arguments, and results the transfer function declines to reason about.
  void setToEntryState(ExtendedSignLattice *lattice) override;

private:
  bool injectUnsoundAddRule;
};

} // namespace extended_sign

#endif
