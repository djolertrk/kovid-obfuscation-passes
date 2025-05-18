// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk
//
// Simplified Control-Flow Breaking Obfuscation Pass for LLVM
// -----------------------------------------------------------
//
// This LLVM pass complicates reverse engineering by injecting additional
// basic blocks and dummy conditional branches. Specifically, it:
//
// 1. Identifies candidate basic blocks (with exactly one successor) and splits
//    them to create "noise" blocks.
// 2. Replaces original terminators with a conditional branch, leading to either
//    the old successor or the newly inserted block.
// 3. Uses opaque predicates to form extra control-flow paths that are hard
//    to eliminate by the optimizer.
//
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

// Create an opaque predicate that the optimizer can't easily evaluate
// Returns a Value that is actually false at runtime but appears complex
static Value *createOpaquePredicate(IRBuilder<> &builder) {
  // Create a complex expression that evaluates to false but is hard to prove
  // statically Example: (x * x) % 2 == 1 where x = 2 will always be 0, thus
  // false
  Value *X = builder.getInt32(2); // A constant we know is even
  Value *Squared = builder.CreateMul(X, X);
  Value *Mod = builder.CreateURem(Squared, builder.getInt32(2));
  Value *Compare = builder.CreateICmpEQ(Mod, builder.getInt32(1));

  return Compare; // Always false at runtime (4 % 2 = 0, which is not equal to
                  // 1)
}

struct ImprovedBreakCFPass : public PassInfoMixin<ImprovedBreakCFPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool modified = false;

    // Print a note to show the pass is running on this function
    llvm::WithColor::note() << "Complicating: " << F.getName() << '\n';

    SmallVector<BasicBlock *, 8> blocksToTransform;
    for (auto &BB : F) {
      // Skip if it's the entry block or trivially small
      if (&BB == &F.getEntryBlock() || BB.size() <= 1)
        continue;

      Instruction *Term = BB.getTerminator();
      if (!Term)
        continue;

      // We only handle blocks with exactly one successor.
      if (Term->getNumSuccessors() == 1)
        blocksToTransform.push_back(&BB);
    }

    // Now do our transformations
    for (auto *BB : blocksToTransform) {
      Instruction *Term = BB->getTerminator();
      if (!Term)
        continue;

      // For safety, re-check we have exactly one successor
      if (Term->getNumSuccessors() != 1)
        continue;

      // Create a new block (SplitBlock) right after BB in the function.
      BasicBlock *OriginalNext = BB->getNextNode();
      BasicBlock *SplitBlock = BasicBlock::Create(
          F.getContext(), BB->getName() + ".split", &F, OriginalNext);

      // In the new block, place an unconditional branch to the old successor
      IRBuilder<> builder(SplitBlock);
      builder.CreateBr(Term->getSuccessor(0));

      // Replace the old terminator with a conditional branch based on an opaque
      // predicate
      IRBuilder<> builderBB(Term);
      // Create opaque predicate - will evaluate to false but hard to prove
      Value *cond = createOpaquePredicate(builderBB);

      BasicBlock *oldSucc = Term->getSuccessor(0);
      // Create conditional branch - actual control flow will always go to
      // oldSucc because our opaque predicate is false, but optimizer can't
      // easily prove that
      auto *newBr = BranchInst::Create(oldSucc, SplitBlock, cond);

      // Replace the old terminator with our new branch
      ReplaceInstWithInst(Term, newBr);
      modified = true;
    }

    return (modified ? PreservedAnalyses::none() : PreservedAnalyses::all());
  }
};

// New Pass Manager registration
PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    // Important: Register the pass at the end of the optimization pipeline
    // so that optimizations don't undo our obfuscation
    PB.registerOptimizerLastEPCallback(
        [&](ModulePassManager &MPM, OptimizationLevel Level) {
          // We adapt the pass to run per function
          MPM.addPass(createModuleToFunctionPassAdaptor(ImprovedBreakCFPass()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-break-cf", "0.0.1", callback};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
