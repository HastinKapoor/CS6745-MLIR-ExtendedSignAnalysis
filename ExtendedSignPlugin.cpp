//===- ExtendedSignPlugin.cpp - Pass definition and plugin entry point ----===//
//
// Scaffolding: wires ExtendedSignAnalysis into a pass and exposes it to
// mlir-opt as `extended-sign-analysis`.
//
//===----------------------------------------------------------------------===//

#include "Annotate.h"
#include "ExtendedSignAnalysis.h"

#include "mlir/Analysis/DataFlow/ConstantPropagationAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

struct ExtendedSignAnalysisPass
    : PassWrapper<ExtendedSignAnalysisPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ExtendedSignAnalysisPass)

  StringRef getArgument() const final { return "extended-sign-analysis"; }

  StringRef getDescription() const final {
    return "Determine coarse signed facts about integer values";
  }

  void runOnOperation() override {
    DataFlowConfig config;
    config.setInterprocedural(false);

    DataFlowSolver solver(config);
    // DeadCodeAnalysis supplies reachability, without which the solver must
    // assume every branch is taken; SparseConstantPropagation resolves branch
    // conditions for it.  Both are prerequisites, not extras.
    solver.load<dataflow::DeadCodeAnalysis>();
    solver.load<dataflow::SparseConstantPropagation>();
    solver.load<extended_sign::ExtendedSignAnalysis>();

    if (failed(solver.initializeAndRun(getOperation()))) {
      getOperation().emitError(
          "extended sign analysis failed to reach a fixed point");
      return signalPassFailure();
    }

    // Query states only now that the solver has converged.
    auto describe = [&](Value value, AsmState &asmState) -> std::string {
      const auto *lattice =
          solver.lookupState<extended_sign::ExtendedSignLattice>(value);
      if (!lattice)
        return {};
      extended_sign::Kind kind = lattice->getValue().kind;
      // Top and bottom say nothing; printing them would bury the real facts.
      if (kind == extended_sign::Kind::Top ||
          kind == extended_sign::Kind::Bottom)
        return {};
      std::string description;
      llvm::raw_string_ostream os(description);
      value.printAsOperand(os, asmState);
      os << " is " << extended_sign::name(kind);
      return description;
    };

    // Print to stderr, so that mlir-opt's stdout stays the unmodified IR and
    // the two can be redirected independently.
    zero::printAnnotated(getOperation(), describe, llvm::errs());

    // This pass only reads.
    markAllAnalysesPreserved();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  // LLVM_VERSION_STRING is baked in at compile time and checked by mlir-opt at
  // load time, which is what turns an ABI mismatch into a clear diagnostic.
  return {MLIR_PLUGIN_API_VERSION, "ExtendedSignAnalysis", LLVM_VERSION_STRING,
          []() { PassRegistration<ExtendedSignAnalysisPass>(); }};
}
