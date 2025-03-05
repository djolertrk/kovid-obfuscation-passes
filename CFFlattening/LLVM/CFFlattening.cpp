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
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

struct SimplifiedControlFlowFlattenPass
    : public PassInfoMixin<SimplifiedControlFlowFlattenPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    bool changed = false;
    // Reuse the same logic from above, but we need a separate copy
    // or we can factor it out. For brevity, just inline something similar here.

    llvm::WithColor::note() << "Complicating: " << F.getName() << '\n';

    if (F.empty() || F.size() < 2) {
      return PreservedAnalyses::all();
    }

    SmallVector<BasicBlock *, 8> Blocks;
    for (auto &BB : F) {
      if (BB.getTerminator() && BB.getTerminator()->getNumSuccessors() == 1 &&
          &BB != &F.getEntryBlock()) {
        Blocks.push_back(&BB);
      }
    }
    if (Blocks.empty()) {
      return PreservedAnalyses::all();
    }

    BasicBlock *Entry = &F.getEntryBlock();
    BasicBlock *Dispatcher = BasicBlock::Create(F.getContext(), "dispatcher",
                                                &F, Entry->getNextNode());

    IRBuilder<> EntryBuilder(Entry->getTerminator());
    IRBuilder<> DispBuilder(Dispatcher);

    AllocaInst *BlockIDAlloca = EntryBuilder.CreateAlloca(
        Type::getInt32Ty(F.getContext()), nullptr, "blockID");
    EntryBuilder.CreateStore(
        ConstantInt::get(Type::getInt32Ty(F.getContext()), 0), BlockIDAlloca);

    Value *SwitchVal =
        DispBuilder.CreateLoad(Type::getInt32Ty(F.getContext()), BlockIDAlloca);
    SwitchInst *SwInst = DispBuilder.CreateSwitch(SwitchVal, Entry, 0);

    int NextID = 1;
    for (auto *BB : Blocks) {
      int ThisID = NextID++;
      SwInst->addCase(
          ConstantInt::get(Type::getInt32Ty(F.getContext()), ThisID), BB);

      auto *Term = BB->getTerminator();
      BasicBlock *Succ = Term->getSuccessor(0);

      IRBuilder<> builder(Term);
      int NextBlockID = (Succ == Entry) ? 0 : (ThisID + 1);
      builder.CreateStore(builder.getInt32(NextBlockID), BlockIDAlloca);
      builder.CreateBr(Dispatcher);
      Term->eraseFromParent();
    }

    changed = true;
    // llvm::dbgs() << F << '\n';
    return (changed ? PreservedAnalyses::none() : PreservedAnalyses::all());
  }
};

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(
              SimplifiedControlFlowFlattenPass()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-cf-flattening", "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
