// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk
//
// Control-Flow Taint Obfuscation Pass for LLVM
// ---------------------------------------------
//
// This LLVM pass implements control flow obfuscation using dispatcher-based
// control flow flattening to obscure the program's control flow graph.
//
// Before:          After:
// A → B → C        Entry → Dispatcher ←┐
//                            ↓        │
//                     Switch(blockID) │
//                      ↙    ↓    ↘    │
//                     A     B     C ──┘
//
// TODO: This pass will handle more CF related obfuscation techniques.
//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Pass.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

// Simplified opaque predicate - always returns false
static Value *createSimpleOpaquePredicate(IRBuilder<> &builder) {
  // (x & 1) == 2 is always false (no odd number equals 2)
  Value *X = builder.getInt32(5); // Any odd number
  Value *AndOp = builder.CreateAnd(X, builder.getInt32(1));
  Value *Compare = builder.CreateICmpEQ(AndOp, builder.getInt32(2));
  return Compare;
}

static bool runControlFlowTaint(Function &F) {
  // Skip various problematic functions
  if (F.empty() || F.size() < 2 || F.isDeclaration() || F.isIntrinsic() ||
      F.isVarArg() || F.hasAvailableExternallyLinkage() ||
      F.hasLinkOnceLinkage() || F.getName() == "main") {
    return false;
  }

  // TODO: Improve this by more testing.
  // Skip large functions
  if (F.size() > 20) {
    return false;
  }

  // Skip functions with complex control flow patterns
  for (auto &BB : F) {
    // Skip functions with exception handling
    if (BB.isEHPad() || BB.isLandingPad()) {
      return false;
    }

    // Skip functions with indirect branches
    if (auto *Term = BB.getTerminator()) {
      if (isa<IndirectBrInst>(Term) || isa<CallBrInst>(Term) ||
          isa<InvokeInst>(Term) || isa<ResumeInst>(Term) ||
          isa<CleanupReturnInst>(Term) || isa<CatchReturnInst>(Term)) {
        return false;
      }

      // Skip functions with switch statements
      if (isa<SwitchInst>(Term)) {
        return false;
      }
    }

    // Skip functions with PHI nodes - our transformation doesn't handle them well
    if (!BB.phis().empty()) {
      return false;
    }
  }

  llvm::WithColor::note() << "Tainting control flow: " << F.getName() << '\n';

  // Collect only simple blocks to transform
  SmallVector<BasicBlock *, 8> BlocksToTransform;
  BasicBlock *Entry = &F.getEntryBlock();

  for (auto &BB : F) {
    // Skip entry block and blocks with multiple predecessors
    if (&BB == Entry || pred_size(&BB) > 1) {
      continue;
    }

    // Only handle blocks with unconditional branches
    auto *Term = dyn_cast<BranchInst>(BB.getTerminator());
    if (!Term || Term->isConditional()) {
      continue;
    }

    BasicBlock *Succ = Term->getSuccessor(0);

    // Skip if successor has PHI nodes
    if (!Succ->phis().empty()) {
      continue;
    }

    // Skip if successor has multiple predecessors
    if (pred_size(Succ) > 1) {
      continue;
    }

    // Skip if this would create a cycle
    if (Succ == Entry || Succ == &BB) {
      continue;
    }

    BlocksToTransform.push_back(&BB);
  }

  // Need at least 2 blocks to make flattening worthwhile
  if (BlocksToTransform.size() < 2) {
    return false;
  }

  // Limit transformation to avoid overwhelming the compiler
  if (BlocksToTransform.size() > 10) {
    BlocksToTransform.resize(10);
  }

  // Create block ID variable in entry block
  IRBuilder<> EntryBuilder(Entry->getFirstNonPHI());
  AllocaInst *BlockIDVar = EntryBuilder.CreateAlloca(
      Type::getInt32Ty(F.getContext()), nullptr, "blockID");
  EntryBuilder.CreateStore(EntryBuilder.getInt32(0), BlockIDVar);

  // Create dispatcher block
  BasicBlock *Dispatcher = BasicBlock::Create(
      F.getContext(), "dispatcher", &F, Entry->getNextNode());

  // Get original entry successor before modification
  BasicBlock *OriginalEntrySucc = nullptr;
  if (auto *EntryTerm = dyn_cast<BranchInst>(Entry->getTerminator())) {
    if (!EntryTerm->isConditional()) {
      OriginalEntrySucc = EntryTerm->getSuccessor(0);
    }
  }

  // Connect entry to dispatcher
  if (Entry->getTerminator()) {
    Entry->getTerminator()->eraseFromParent();
  }
  IRBuilder<> EntryTermBuilder(Entry);
  EntryTermBuilder.CreateBr(Dispatcher);

  // Build dispatcher
  IRBuilder<> DispBuilder(Dispatcher);
  LoadInst *SwitchVal = DispBuilder.CreateLoad(
      Type::getInt32Ty(F.getContext()), BlockIDVar, "switchval");

  // Create default destination - use function exit or create a trap block
  BasicBlock *DefaultDest = BasicBlock::Create(
      F.getContext(), "default.trap", &F);
  IRBuilder<> TrapBuilder(DefaultDest);
  TrapBuilder.CreateUnreachable();

  SwitchInst *SwInst = DispBuilder.CreateSwitch(
      SwitchVal, DefaultDest, BlocksToTransform.size() + 2);

  // Map blocks to IDs
  DenseMap<BasicBlock *, int> BlockToID;
  int NextID = 0;

  // Add original entry successor if it exists
  if (OriginalEntrySucc) {
    BlockToID[OriginalEntrySucc] = NextID;
    SwInst->addCase(
        ConstantInt::get(Type::getInt32Ty(F.getContext()), NextID),
        OriginalEntrySucc);
    NextID++;
  }

  // Assign IDs to transformable blocks
  for (auto *BB : BlocksToTransform) {
    BlockToID[BB] = NextID;
    SwInst->addCase(
        ConstantInt::get(Type::getInt32Ty(F.getContext()), NextID), BB);
    NextID++;
  }

  // Transform each block
  for (auto *BB : BlocksToTransform) {
    auto *Term = BB->getTerminator();
    if (!Term)
      continue;

    BasicBlock *Succ = Term->getSuccessor(0);

    // Determine next block ID
    int NextBlockID;
    if (BlockToID.count(Succ)) {
      NextBlockID = BlockToID[Succ];
    } else {
      // Add new mapping for this successor
      NextBlockID = NextID++;
      BlockToID[Succ] = NextBlockID;
      SwInst->addCase(
          ConstantInt::get(Type::getInt32Ty(F.getContext()), NextBlockID),
          Succ);
    }

    // Replace terminator
    IRBuilder<> Builder(Term);
    Builder.CreateStore(Builder.getInt32(NextBlockID), BlockIDVar);
    Builder.CreateBr(Dispatcher);
    Term->eraseFromParent();
  }

  return true;
}

namespace {

#if defined(LLVM_ON_UNIX)
struct ControlFlowTaintPass : public PassInfoMixin<ControlFlowTaintPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (runControlFlowTaint(F))
      return PreservedAnalyses::none();
    else
      return PreservedAnalyses::all();
  }
};
#else
struct ControlFlowTaintPass : public FunctionPass {
  static char ID;
  ControlFlowTaintPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    return runControlFlowTaint(F);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};
#endif

} // end anonymous namespace

#if defined(LLVM_ON_UNIX)
PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(ControlFlowTaintPass()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-control-flow-taint", "0.0.2",
          callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
#else
char ControlFlowTaintPass::ID = 0;
static RegisterPass<ControlFlowTaintPass>
X("kovid-control-flow-taint", "KoviD Control Flow Taint Pass");
#endif
