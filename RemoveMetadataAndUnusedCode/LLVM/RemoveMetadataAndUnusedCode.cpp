// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk

//
// Metadata and Unused Code Removal Obfuscation Pass for LLVM
// ------------------------------------------------------------
// 
// This LLVM pass is designed to hinder reverse engineering by removing
// extraneous information from the module. It performs two main tasks:
// 
// 1. Debug Metadata Removal:
//    - Erases named metadata (e.g., "llvm.dbg.cu") that contains debug
//      compile unit information.
//    - Clears debug metadata from all instructions and global variables.
//    - Removes function-level debug info by setting each function's subprogram
// to null.
// 
// 2. Unused Code Removal:
//    - Iterates over defined functions with internal linkage.
//    - If a function has no uses (i.e. it is not referenced anywhere), it is
// removed.
// 
// Together, these techniques reduce the amount of information available to an
// attacker and help obscure the program's logic.
//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct RemoveMetadataAndUnusedCodePass
    : public PassInfoMixin<RemoveMetadataAndUnusedCodePass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    // 1. Remove debug metadata from the module.
    if (NamedMDNode *NMD = M.getNamedMetadata("llvm.dbg.cu"))
      M.eraseNamedMetadata(NMD);

    for (Function &F : M) {
      if (!F.isDeclaration()) {
        // Clear function-level debug information.
        F.setSubprogram(nullptr);
        // Remove per-instruction debug metadata.
        for (BasicBlock &BB : F) {
          for (Instruction &I : BB) {
            I.setMetadata("dbg", nullptr);
          }
        }
      }
    }
    for (GlobalVariable &GV : M.globals())
      GV.setMetadata("dbg", nullptr);

    // 2. Remove unused functions.
    SmallVector<Function *, 16> ToRemove;
    for (Function &F : M) {
      // Only consider functions that are defined and have internal linkage.
      if (!F.isDeclaration() && F.hasInternalLinkage() && F.use_empty()) {
        ToRemove.push_back(&F);
      }
    }
    for (Function *F : ToRemove) {
      errs() << "Removing unused function: " << F->getName() << "\n";
      F->eraseFromParent();
    }

    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "kovid-metadata-unused-code-removal") {
            MPM.addPass(RemoveMetadataAndUnusedCodePass());
            return true;
          }
          return false;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-metadata-unused-code-removal",
          "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
