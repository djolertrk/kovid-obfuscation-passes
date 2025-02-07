// Under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "llvm/ADT/SmallSet.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#define DEBUG_TYPE "kovid-rename-code"

using namespace llvm;

#ifndef CRYPTO_KEY
#define CRYPTO_KEY "default_key"
#endif

static bool runCodeRename(Function &F, std::string &CryptoKey) {
  llvm::dbgs() << "== KoviD Rename code on " << F.getName() << "\n";
  // << demangle(F.getName().str()) << '\n';
  llvm::dbgs() << "Using crypto key: " << CryptoKey << "\n";
  if (F.isDeclaration())
    return false;
  if (!F.hasLocalLinkage())
    return false;

  SmallVector<Instruction *, 8> ToDelete;
  for (BasicBlock &BB : F) {
    for (auto &I : BB) {
      if (auto Call = dyn_cast<CallInst>(&I)) {
        auto CalledFn = Call->getCalledFunction();
        if (!CalledFn || !CalledFn->hasName())
          continue;
        Call->dump();
      }
    }
  }

  for (auto &I : ToDelete) {
    LLVM_DEBUG(dbgs() << "Removing "; I->dump());
    I->eraseFromParent();
  }

  return false;
}

namespace {

struct RenameCode : PassInfoMixin<RenameCode> {
  std::string CryptoKey;
  RenameCode(std::string Key = CRYPTO_KEY) : CryptoKey(Key) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    llvm::dbgs() << "Running KoviD Rename Code Pass\n";
    runCodeRename(F, CryptoKey);
    return PreservedAnalyses::all();
  }
};

} // end anonymous namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(RenameCode()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-rename-code", "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
