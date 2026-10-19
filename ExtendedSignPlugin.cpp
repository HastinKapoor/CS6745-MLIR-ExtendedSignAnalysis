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
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/SymbolTable.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {

constexpr StringLiteral checkerName = "__extended_sign_assert";

struct RuntimeFact {
  Value value;
  extended_sign::Kind kind;
  uint64_t id;
};

static bool isConcreteFact(extended_sign::Kind kind) {
  return kind != extended_sign::Kind::Bottom &&
         kind != extended_sign::Kind::Top;
}

static LLVM::ICmpPredicate predicateFor(extended_sign::Kind kind) {
  switch (kind) {
  case extended_sign::Kind::One:
  case extended_sign::Kind::Zero:
    return LLVM::ICmpPredicate::eq;
  case extended_sign::Kind::Negative:
    return LLVM::ICmpPredicate::slt;
  case extended_sign::Kind::Positive:
    return LLVM::ICmpPredicate::sgt;
  case extended_sign::Kind::Nonnegative:
    return LLVM::ICmpPredicate::sge;
  case extended_sign::Kind::Nonpositive:
    return LLVM::ICmpPredicate::sle;
  case extended_sign::Kind::Bottom:
  case extended_sign::Kind::Top:
    llvm_unreachable("top and bottom facts are not instrumented");
  }
  llvm_unreachable("unknown extended-sign kind");
}

static FileLineColLoc findFileLocation(Location location) {
  return location->findInstanceOf<FileLineColLoc>();
}

static LogicalResult addRuntimeChecks(
    ModuleOp module, DataFlowSolver &solver) {
  llvm::SmallVector<RuntimeFact> facts;
  uint64_t nextId = 1;

  auto collect = [&](Value value) {
    const auto *lattice =
        solver.lookupState<extended_sign::ExtendedSignLattice>(value);
    if (!lattice || !isConcreteFact(lattice->getValue().kind))
      return;
    auto type = dyn_cast<IntegerType>(value.getType());
    if (!type || type.getWidth() == 1)
      return;
    facts.push_back({value, lattice->getValue().kind, nextId++});
  };

  module.walk([&](Operation *op) {
    for (Value result : op->getResults())
      collect(result);
    for (Region &region : op->getRegions())
      for (Block &block : region)
        for (BlockArgument argument : block.getArguments())
          collect(argument);
  });

  if (facts.empty())
    return success();

  if (module.lookupSymbol(checkerName)) {
    module.emitError() << "cannot instrument extended-sign facts: symbol @"
                       << checkerName << " already exists";
    return failure();
  }

  OpBuilder moduleBuilder(module.getContext());
  moduleBuilder.setInsertionPointToStart(module.getBody());
  auto i1 = moduleBuilder.getI1Type();
  auto i32 = moduleBuilder.getI32Type();
  auto i64 = moduleBuilder.getI64Type();
  auto pointer = LLVM::LLVMPointerType::get(module.getContext());
  auto functionType = LLVM::LLVMFunctionType::get(
      LLVM::LLVMVoidType::get(module.getContext()),
      {i1, i32, i64, pointer, i32, i32}, false);
  LLVM::LLVMFuncOp checker = LLVM::LLVMFuncOp::create(
      moduleBuilder, module.getLoc(), checkerName, functionType);

  for (const RuntimeFact &fact : facts) {
    OpBuilder builder(module.getContext());
    Location location = fact.value.getLoc();
    if (auto result = dyn_cast<OpResult>(fact.value))
      builder.setInsertionPointAfter(result.getOwner());
    else
      builder.setInsertionPointToStart(cast<BlockArgument>(fact.value).getOwner());

    auto valueType = cast<IntegerType>(fact.value.getType());
    int64_t expected = fact.kind == extended_sign::Kind::One ? 1 : 0;
    Value reference = LLVM::ConstantOp::create(
        builder, location, valueType,
        builder.getIntegerAttr(valueType, expected));
    Value condition = LLVM::ICmpOp::create(
        builder, location, predicateFor(fact.kind), fact.value, reference);

    FileLineColLoc fileLocation = findFileLocation(location);
    uint32_t line = fileLocation ? fileLocation.getLine() : 0;
    uint32_t column = fileLocation ? fileLocation.getColumn() : 0;
    std::string filename =
        fileLocation ? fileLocation.getFilename().getValue().str() : "<unknown>";
    filename.push_back('\0');
    Value filenameValue = LLVM::createGlobalString(
        location, builder,
        (Twine("__extended_sign_filename_") + Twine(fact.id)).str(), filename,
        LLVM::Linkage::Private);
    Value kind = LLVM::ConstantOp::create(
        builder, location, i32,
        builder.getI32IntegerAttr(static_cast<int32_t>(fact.kind)));
    Value id = LLVM::ConstantOp::create(
        builder, location, i64, builder.getI64IntegerAttr(fact.id));
    Value lineValue = LLVM::ConstantOp::create(
        builder, location, i32, builder.getI32IntegerAttr(line));
    Value columnValue = LLVM::ConstantOp::create(
        builder, location, i32, builder.getI32IntegerAttr(column));
    LLVM::CallOp::create(builder, location, checker,
                         ValueRange{condition, kind, id, filenameValue, lineValue,
                                    columnValue});
  }

  llvm::errs() << "inserted " << facts.size()
               << " extended-sign runtime checks\n";
  return success();
}

struct ExtendedSignAnalysisPass
    : PassWrapper<ExtendedSignAnalysisPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(ExtendedSignAnalysisPass)

  ExtendedSignAnalysisPass() = default;
  ExtendedSignAnalysisPass(const ExtendedSignAnalysisPass &pass)
      : PassWrapper(pass) {}

  StringRef getArgument() const final { return "extended-sign-analysis"; }

  StringRef getDescription() const final {
    return "Determine coarse signed facts about integer values";
  }

  Option<bool> instrument{
      *this, "instrument",
      llvm::cl::desc("Insert runtime checks for every proved fact"),
      llvm::cl::init(false)};

  Option<bool> injectUnsoundAddRule{
      *this, "inject-unsound-add-rule",
      llvm::cl::desc("Enable a deliberately unsound rule for checker testing"),
      llvm::cl::init(false)};

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
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
    solver.load<extended_sign::ExtendedSignAnalysis>(
        static_cast<bool>(injectUnsoundAddRule));

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

    if (instrument) {
      if (failed(addRuntimeChecks(getOperation(), solver)))
        return signalPassFailure();
    } else {
      // Without instrumentation this pass only reads.
      markAllAnalysesPreserved();
    }
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  // LLVM_VERSION_STRING is baked in at compile time and checked by mlir-opt at
  // load time, which is what turns an ABI mismatch into a clear diagnostic.
  return {MLIR_PLUGIN_API_VERSION, "ExtendedSignAnalysis", LLVM_VERSION_STRING,
          []() { PassRegistration<ExtendedSignAnalysisPass>(); }};
}
