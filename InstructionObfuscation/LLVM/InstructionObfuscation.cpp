// Under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// author: djolertrk
//
// Instruction Pattern/Flow Transformation Obfuscation Pass for LLVM
// -------------------------------------------------------------------
//
// This LLVM pass implements an instruction pattern transformation technique
// for code obfuscation. Its purpose is to transform common instructions into
// less common, more complex sequences that perform the same operation, thereby
// making reverse engineering more challenging.
//
// In this example, we target integer addition instructions. For each add
// instruction of the form:
//
//    %result = add i32 %a, %b
//
// the pass replaces it with an equivalent sequence that computes:
//
//    dummy = add i32 42, 0           // a dummy computation that yields 42.
//    temp  = sub i32 dummy, 42        // subtract 42: result is 0.
//    left  = add i32 %a, temp         // effectively, %a + 0 = %a.
//    %new  = add i32 left, %b         // computes %a + %b.
//
// Each inserted instruction is tagged with metadata ("obf") to help prevent
// these dummy operations from being optimized away.
//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// This, for now, implements "Arithmetic code obfuscation" only.
struct InstructionObfuscationPass : public PassInfoMixin<InstructionObfuscationPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Skip declarations
    if (F.isDeclaration()) {
      return PreservedAnalyses::all();
    }

    // Create a dummy alloca in the entry block to prevent constant folding
    BasicBlock &EntryBB = F.getEntryBlock();
    LLVMContext &Ctx = F.getContext();
    Type *Int32Ty = Type::getInt32Ty(Ctx);
    
    // Map to store allocas for different types
    DenseMap<Type*, AllocaInst*> TypeToAlloca;
    
    // First, collect all add instructions from all blocks
    SmallVector<Instruction *, 32> AllAddInsts;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *BO = dyn_cast<BinaryOperator>(&I)) {
          if (BO->getOpcode() == Instruction::Add && BO->getType()->isIntegerTy())
            AllAddInsts.push_back(&I);
        }
      }
    }
    
    // If no add instructions, nothing to do
    if (AllAddInsts.empty()) {
      return PreservedAnalyses::all();
    }
    
    // Create all needed allocas upfront in the entry block
    // This avoids iterator invalidation issues
    IRBuilder<> EntryBuilder(&EntryBB, EntryBB.getFirstInsertionPt());
    for (Instruction *I : AllAddInsts) {
      Type *AddType = I->getType();
      if (TypeToAlloca.find(AddType) == TypeToAlloca.end()) {
        AllocaInst *NewAlloca = EntryBuilder.CreateAlloca(AddType, nullptr, "dummyForObfTyped");
        TypeToAlloca[AddType] = NewAlloca;
      }
    }
    
    // Process each add instruction.
    for (Instruction *I : AllAddInsts) {
        llvm::WithColor::note() << "Complicating: " << *I << '\n';

        IRBuilder<> Builder(I);
        
        // Get the type of the add instruction
        Type *AddType = I->getType();
        if (!AddType->isIntegerTy()) {
          continue; // Skip non-integer adds
        }

        // Create constants of the appropriate type
        Value *Zero = ConstantInt::get(AddType, 0);
        Value *FortyTwo = ConstantInt::get(AddType, 42);

        // Get the pre-created alloca for this type
        AllocaInst *AllocaToUse = TypeToAlloca[AddType];
        
        StoreInst *store0 = Builder.CreateStore(Zero, AllocaToUse);
        store0->setVolatile(true);

        // Load from alloca (volatile, so it won't be folded).
        LoadInst *dummyLoad =
            Builder.CreateLoad(AddType, AllocaToUse, "dummy.load");
        dummyLoad->setVolatile(true);

        // Now, build the dummy arithmetic sequence:
        // dummy = add (dummyLoad, 42)
        Instruction *dummy = cast<Instruction>(Builder.CreateAdd(
            dummyLoad, FortyTwo, "dummy"));
        dummy->setMetadata("obf", MDNode::get(Ctx, MDString::get(Ctx, "obf")));

        // temp = sub (dummy, 42)  ; This should yield the original
        // dummyLoad value (0)
        Instruction *temp = cast<Instruction>(
            Builder.CreateSub(dummy, FortyTwo, "temp"));
        temp->setMetadata("obf", MDNode::get(Ctx, MDString::get(Ctx, "obf")));

        // Now, replace:  %result = add i32 %a, %b
        // with:
        // left = add i32 (%a, temp)   ; (%a + 0)
        // newAdd = add i32 (left, %b)
        Value *a = I->getOperand(0);
        Instruction *left =
            cast<Instruction>(Builder.CreateAdd(a, temp, "left"));
        left->setMetadata("obf", MDNode::get(Ctx, MDString::get(Ctx, "obf")));

        Value *b = I->getOperand(1);
        Instruction *newAdd =
            cast<Instruction>(Builder.CreateAdd(left, b, "obf.add"));
        newAdd->setMetadata("obf", MDNode::get(Ctx, MDString::get(Ctx, "obf")));

        // Replace all uses of the original add with the newAdd and remove it.
        I->replaceAllUsesWith(newAdd);
        I->eraseFromParent();
    }
    return PreservedAnalyses::none();
  }
};

} // end anonymous namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(createModuleToFunctionPassAdaptor(InstructionObfuscationPass()));
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-instruction-obf", "0.0.1", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
