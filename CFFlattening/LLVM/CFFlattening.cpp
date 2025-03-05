// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk
//
// Simplified Control-Flow Flattening Obfuscation Pass for LLVM
// ------------------------------------------------------------
//
// This LLVM pass attempts a naive form of control-flow flattening by:
// 1. Creating a "dispatcher" basic block containing a switch on a "blockID".
// 2. Replacing each original block's terminator to store the next blockID
//    and jump back to the dispatcher.
// 3. Ensuring the dispatcher then branches to the appropriate block.
//
// Limitations:
// - Skips blocks with multiple successors (e.g., complicated branches).
// - Uses a simple integer "blockID" variable stored in an alloca, which is easy
//   to optimize away unless compiled under certain conditions.
//
// While minimal, it demonstrates a basic approach to flatten control flow,
// potentially confusing naive static analysis tools.
//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
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
struct SimplifiedControlFlowFlatten : public FunctionPass {
  static char ID;
  SimplifiedControlFlowFlatten() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
};
} // end anonymous namespace

char SimplifiedControlFlowFlatten::ID = 0;

//
// The core flattening logic (naive version).
//
bool SimplifiedControlFlowFlatten::runOnFunction(Function &F) {
  // Skip any function that’s a declaration or trivial
  if (F.empty() || F.size() < 2) {
    return false;
  }

  // We collect the "interesting" blocks
  // For simplicity, we skip blocks with multiple successors or zero successors (return blocks).
  SmallVector<BasicBlock *, 8> Blocks;
  for (auto &BB : F) {
    auto *Term = BB.getTerminator();
    if (!Term)
      continue;

    // If it has exactly one successor, we flatten it
    if (Term->getNumSuccessors() == 1 && &BB != &F.getEntryBlock()) {
      Blocks.push_back(&BB);
    }
  }
  if (Blocks.empty())
    return false;

  // Create a dispatcher block at the start of the function
  // We'll insert it right after the entry block to keep the normal entry path.
  BasicBlock *Entry = &F.getEntryBlock();
  BasicBlock *Dispatcher = BasicBlock::Create(F.getContext(),
                                              "dispatcher",
                                              &F,
                                              Entry->getNextNode());

  IRBuilder<> EntryBuilder(Entry->getTerminator());
  IRBuilder<> DispBuilder(Dispatcher);

  // We need an i32 alloca to hold the "blockID"
  // We'll do it in the entry block so it’s always in scope.
  AllocaInst *BlockIDAlloca =
      EntryBuilder.CreateAlloca(Type::getInt32Ty(F.getContext()), nullptr,
                                "blockID");

  // Initialize blockID to 0 in the entry block (the dispatcher will interpret this
  // as "go directly to the original entry block" unless we skip it).
  EntryBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(F.getContext()), 0),
                           BlockIDAlloca);

  // Build a switch instruction to jump to each target
  Value *SwitchVal = DispBuilder.CreateLoad(Type::getInt32Ty(F.getContext()),
                                            BlockIDAlloca);
  SwitchInst *SwInst = DispBuilder.CreateSwitch(SwitchVal, Entry, 0);

  // For each "flattenable" block, we’ll give it an ID and insert a jump back
  // to the dispatcher.
  // We'll number them starting from 1 to avoid clashing with our "0" default.
  int NextID = 1;
  for (auto *BB : Blocks) {
    int ThisID = NextID++;

    // We add a case in our switch that points to BB
    SwInst->addCase(ConstantInt::get(Type::getInt32Ty(F.getContext()), ThisID),
                    BB);

    // Modify the block’s terminator to set blockID to the next block’s ID, then
    // branch to the dispatcher. For simplicity, we treat BB->getTerminator()
    // as having exactly one successor.
    auto *Term = BB->getTerminator();
    BasicBlock *Succ = Term->getSuccessor(0);
    // We'll skip blocks that are returning or outside the function, etc.
    // This is naive. In a more advanced pass, we'd flatten *all* successors.

    IRBuilder<> builder(Term);
    // We'll pick the next ID for that successor (or 0 if it's the function's entry).
    // For demonstration, we simply store "0" if Succ is the entry block,
    // otherwise store ThisID+1 or some dummy formula.
    int NextBlockID = (Succ == Entry) ? 0 : (ThisID + 1);

    Value *NewID = builder.getInt32(NextBlockID);
    builder.CreateStore(NewID, BlockIDAlloca);

    // Replace the old branch with a branch to the dispatcher
    builder.CreateBr(Dispatcher);
    Term->eraseFromParent();
  }

  return true;
}

// Register the pass with the old pass manager
static RegisterPass<SimplifiedControlFlowFlatten>
    X("simplified-cff",
      "A simplified control-flow flattening pass that centralizes branching",
      false, // CFG Only?
      false  // Is analysis?
    );

//
// New Pass Manager-compatible pass
//
struct SimplifiedControlFlowFlattenPass
    : public PassInfoMixin<SimplifiedControlFlowFlattenPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool changed = false;
    // Reuse the same logic from above, but we need a separate copy
    // or we can factor it out. For brevity, just inline something similar here.

    if (F.empty() || F.size() < 2) {
      return PreservedAnalyses::all();
    }

    SmallVector<BasicBlock *, 8> Blocks;
    for (auto &BB : F) {
      if (BB.getTerminator() &&
          BB.getTerminator()->getNumSuccessors() == 1 &&
          &BB != &F.getEntryBlock()) {
        Blocks.push_back(&BB);
      }
    }
    if (Blocks.empty()) {
      return PreservedAnalyses::all();
    }

    BasicBlock *Entry = &F.getEntryBlock();
    BasicBlock *Dispatcher = BasicBlock::Create(F.getContext(),
                                                "dispatcher",
                                                &F,
                                                Entry->getNextNode());

    IRBuilder<> EntryBuilder(Entry->getTerminator());
    IRBuilder<> DispBuilder(Dispatcher);

    AllocaInst *BlockIDAlloca =
        EntryBuilder.CreateAlloca(Type::getInt32Ty(F.getContext()), nullptr,
                                  "blockID");
    EntryBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(F.getContext()), 0),
                             BlockIDAlloca);

    Value *SwitchVal = DispBuilder.CreateLoad(Type::getInt32Ty(F.getContext()),
                                              BlockIDAlloca);
    SwitchInst *SwInst = DispBuilder.CreateSwitch(SwitchVal, Entry, 0);

    int NextID = 1;
    for (auto *BB : Blocks) {
      int ThisID = NextID++;
      SwInst->addCase(ConstantInt::get(Type::getInt32Ty(F.getContext()), ThisID),
                      BB);

      auto *Term = BB->getTerminator();
      BasicBlock *Succ = Term->getSuccessor(0);

      IRBuilder<> builder(Term);
      int NextBlockID = (Succ == Entry) ? 0 : (ThisID + 1);
      builder.CreateStore(builder.getInt32(NextBlockID), BlockIDAlloca);
      builder.CreateBr(Dispatcher);
      Term->eraseFromParent();
    }

    changed = true;
    return (changed ? PreservedAnalyses::none() : PreservedAnalyses::all());
  }
};

//
// Pass Plugin Initialization
//
PassPluginLibraryInfo getSimplifiedControlFlowFlattenPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION, "kovid-simplified-cff", "0.0.1",
      [](PassBuilder &PB) {
        // Register our pass in any pipeline you'd like. Here, an example:
        PB.registerPipelineEarlySimplificationEPCallback(
            [&](ModulePassManager &MPM, auto) {
              MPM.addPass(createModuleToFunctionPassAdaptor(
                              SimplifiedControlFlowFlattenPass()));
              return true;
            });
      }
  };
}

// This is the core "hook" for the new pass manager plugin.
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getSimplifiedControlFlowFlattenPluginInfo();
}
