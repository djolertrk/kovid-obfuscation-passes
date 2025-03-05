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
// 1. Identifies candidate basic blocks and splits them to create “noise” blocks.
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
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

//
// Legacy Pass (for the old pass manager)
//
namespace {
struct SimplifiedBreakCF : public FunctionPass {
  static char ID;
  SimplifiedBreakCF() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    bool modified = false;

    // We’ll collect our insertion points first, to avoid invalidating iterators
    // as we transform blocks.
    SmallVector<BasicBlock *, 8> blocksToTransform;

    // Simple criterion: we attempt to transform all "non-trivial" blocks.
    for (auto &BB : F) {
      // Skip empty blocks or the entry block to avoid messing up control flow.
      if (BB.size() > 1 && &BB != &F.getEntryBlock()) {
        blocksToTransform.push_back(&BB);
      }
    }

    // Transform each chosen block
    for (auto *BB : blocksToTransform) {
      Instruction *Term = BB->getTerminator();
      if (!Term)
        continue;

      // Create a new basic block immediately after BB
      BasicBlock *OriginalNext = BB->getNextNode(); 
      BasicBlock *SplitBlock = BasicBlock::Create(
          F.getContext(), BB->getName() + ".split", &F, OriginalNext);

      // Insert an unconditional jump in the new block to the original successor
      IRBuilder<> builder(SplitBlock);
      builder.CreateBr(Term->getSuccessor(0));

      // Now replace the old terminator with a conditional branch.
      // We'll use a dummy condition that's always false as a simple example.
      IRBuilder<> builderBB(Term);
      Value *cond = builderBB.getInt1(false);

      BasicBlock *origSucc = Term->getSuccessor(0);
      auto *newBr = BranchInst::Create(origSucc, SplitBlock, cond);

      // Replace the old terminator
      ReplaceInstWithInst(Term, newBr);

      modified = true;
    }

    return modified;
  }
};
} // namespace

char SimplifiedBreakCF::ID = 0;

// Register the legacy pass for the old pass manager
static RegisterPass<SimplifiedBreakCF>
    X("simplified-break-cf",
      "A simplified control-flow breaking pass that inserts dummy conditional branches",
      false, // CFG Only?
      false  // Is analysis?
    );

//
// New Pass Manager-compatible pass
//
struct SimplifiedBreakCFPass : public PassInfoMixin<SimplifiedBreakCFPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool modified = false;

    SmallVector<BasicBlock *, 8> blocksToTransform;
    for (auto &BB : F) {
      if (BB.size() > 1 && &BB != &F.getEntryBlock()) {
        blocksToTransform.push_back(&BB);
      }
    }

    for (auto *BB : blocksToTransform) {
      Instruction *Term = BB->getTerminator();
      if (!Term)
        continue;

      BasicBlock *OriginalNext = BB->getNextNode();
      BasicBlock *SplitBlock = BasicBlock::Create(
          F.getContext(), BB->getName() + ".split", &F, OriginalNext);

      IRBuilder<> builder(SplitBlock);
      builder.CreateBr(Term->getSuccessor(0));

      IRBuilder<> builderBB(Term);
      Value *cond = builderBB.getInt1(false);

      BasicBlock *origSucc = Term->getSuccessor(0);
      auto *newBr = BranchInst::Create(origSucc, SplitBlock, cond);

      ReplaceInstWithInst(Term, newBr);

      modified = true;
    }

    return (modified ? PreservedAnalyses::none() : PreservedAnalyses::all());
  }
};

//
// Pass Plugin Initialization
//
PassPluginLibraryInfo getSimplifiedBreakCFPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "kovid-simplified-break-cf", "0.0.1",
      [](PassBuilder &PB) {
        // Register our pass for the new pass manager’s pipeline.
        // Example: Insert early in the pipeline or wherever is appropriate.
        PB.registerPipelineEarlySimplificationEPCallback(
            [&](ModulePassManager &MPM, auto) {
              // We wrap our function pass to run on each function in the module
              MPM.addPass(createModuleToFunctionPassAdaptor(SimplifiedBreakCFPass()));
              return true;
            });
      }
  };
}

// This is the core "hook" for the new pass manager plugin
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getSimplifiedBreakCFPluginInfo();
}
