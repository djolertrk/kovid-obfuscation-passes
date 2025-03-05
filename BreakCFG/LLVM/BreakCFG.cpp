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
//    them to create “noise” blocks.
// 2. Replaces original terminators with a conditional branch, leading to either
//    the old successor or the newly inserted block.
// 3. Uses naive or placeholder conditions (e.g., always false) to form extra
//    control-flow paths.
//
// While minimal in scope, this approach obscures direct analysis of the control
// flow, making it harder for an adversary to understand the program structure.
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

struct SimplifiedBreakCFPass : public PassInfoMixin<SimplifiedBreakCFPass> {
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

      // Replace the old terminator with a conditional branch:
      //  - false => go to SplitBlock
      //  - true  => go to the old successor
      IRBuilder<> builderBB(Term);
      Value *cond = builderBB.getInt1(false); // always false, naive example

      BasicBlock *oldSucc = Term->getSuccessor(0);
      auto *newBr = BranchInst::Create(oldSucc, SplitBlock, cond);

      // Replace the old terminator with our new branch
      ReplaceInstWithInst(Term, newBr);
      // llvm::dbgs() << F << '\n';
      modified = true;
    }

    return (modified ? PreservedAnalyses::none() : PreservedAnalyses::all());
  }
};

// New Pass Manager registration
PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    // Insert the pass into the "early simplification" pipeline (you could
    // choose a different EP)
    PB.registerPipelineEarlySimplificationEPCallback([&](ModulePassManager &MPM,
                                                         auto) {
      // We adapt the pass to run per function
      MPM.addPass(createModuleToFunctionPassAdaptor(SimplifiedBreakCFPass()));
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-break-cf", "0.0.1", callback};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
