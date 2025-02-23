// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk

//
// Dummy Code Insertion Obfuscation Pass for LLVM
// ------------------------------------------------
//
// This LLVM pass implements a dummy code insertion obfuscation technique.
// Its purpose is to thwart reverse engineering by injecting irrelevant code
// that does not affect the program’s execution. Dummy instructions are inserted
// at the beginning of each defined function (skipping declarations) to distract
// both human readers and automated analysis tools.
//
// The pass creates a dummy local variable and performs a series of volatile
// load/store and arithmetic operations (adding 1 then subtracting 1) on that
// variable. Each inserted instruction is tagged with metadata ("dummy") to help
// prevent it from being optimized away by later optimization passes.
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

#include <algorithm>
#include <sstream>
#include <string>

using namespace llvm;

namespace {

struct DummyCodeInsertion : public PassInfoMixin<DummyCodeInsertion> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    // Skip function declarations.
    if (F.isDeclaration())
      return PreservedAnalyses::all();

    LLVMContext &Ctx = F.getContext();
    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    // Create a dummy local variable of type i32.
    AllocaInst *dummyAlloca =
        Builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "dummy");

    // Create a metadata node with a "dummy" tag.
    MDNode *dummyMD = MDNode::get(Ctx, MDString::get(Ctx, "dummy"));

    // Insert a volatile store of 0.
    StoreInst *store0 = Builder.CreateStore(
        ConstantInt::get(Type::getInt32Ty(Ctx), 0), dummyAlloca);
    store0->setVolatile(true);
    store0->setMetadata("dummy", dummyMD);

    // Insert a volatile load.
    LoadInst *dummyLoad =
        Builder.CreateLoad(Type::getInt32Ty(Ctx), dummyAlloca, "dummy.load");
    dummyLoad->setVolatile(true);
    dummyLoad->setMetadata("dummy", dummyMD);

    // Insert dummy arithmetic: add 1 then subtract 1.
    Value *added = Builder.CreateAdd(
        dummyLoad, ConstantInt::get(Type::getInt32Ty(Ctx), 1), "dummy.add");
    Value *subtracted = Builder.CreateSub(
        added, ConstantInt::get(Type::getInt32Ty(Ctx), 1), "dummy.sub");

    // Insert a volatile store of the result.
    StoreInst *storeResult = Builder.CreateStore(subtracted, dummyAlloca);
    storeResult->setVolatile(true);
    storeResult->setMetadata("dummy", dummyMD);

    // This inserted code is now marked volatile and carries "dummy" metadata,
    // which should help prevent it from being optimized away.

    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(DummyCodeInsertion()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-dummy-code-insertion", "0.0.1",
          callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
