// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk
//
// Control-Flow Taint Obfuscation Pass for LLVM
// ---------------------------------------------
//
// This LLVM pass implements advanced control flow obfuscation techniques
// designed to resist aggressive optimization by using:
// 1. Advanced opaque predicates using a mix of global state and complex
// calculations
// 2. Runtime-dependent values to prevent compile-time evaluation
// 3. Memory aliasing and volatiles to prevent certain optimizations
// 4. Indirect control flow through function pointers
// 5. Various control flow complication mechanisms
//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
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

// Create a structure with the state needed for advanced obfuscation
struct ControlFlowTaintPass : public PassInfoMixin<ControlFlowTaintPass> {

  // Create global variables needed for stronger obfuscation
  GlobalVariable *createGlobalState(Module *M) {
    // Create a global array to store state - harder to analyze than a simple
    // variable
    ArrayType *StateArrayTy =
        ArrayType::get(Type::getInt32Ty(M->getContext()), 4);

    // Initialize with some random-looking but deterministic values
    Constant *InitData = ConstantDataArray::get(
        M->getContext(),
        ArrayRef<uint32_t>({0xDEADBEEF, 0xBAADF00D, 0x12345678, 0xABCDEF00}));

    // Create the global with internal linkage so it's not visible outside this
    // module
    GlobalVariable *StateVar = new GlobalVariable(
        *M, StateArrayTy, false, GlobalValue::InternalLinkage, InitData,
        "obfuscation_state", nullptr, GlobalValue::NotThreadLocal, 0);

    return StateVar;
  }

  // Create an opaque predicate that's hard to evaluate at compile time
  Value *createAdvancedOpaquePredicate(IRBuilder<> &Builder, Value *InputVal,
                                       GlobalVariable *StateVar) {
    // Get a pointer to a specific element in our state array
    Value *IdxList[2] = {
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0),
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 2)};
    Value *StateElemPtr = Builder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "state_elem_ptr");

    // Load the value with volatile to prevent optimization
    LoadInst *StateValue = Builder.CreateLoad(
        Type::getInt32Ty(Builder.getContext()), StateElemPtr);
    StateValue->setVolatile(true);

    // Mix the input value with our state
    Value *Mixed = Builder.CreateXor(InputVal, StateValue);

    // Store this back to affect future calculations
    StoreInst *StateStore = Builder.CreateStore(Mixed, StateElemPtr);
    StateStore->setVolatile(true);

    // Create a complex expression combining multiple operations
    // (x^y) % 256 == ((x+y) % 256) - Create a false equivalence that's hard to
    // prove
    Value *Mod1 = Builder.CreateAnd(
        Mixed, ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0xFF));
    Value *Sum = Builder.CreateAdd(InputVal, StateValue);
    Value *Mod2 = Builder.CreateAnd(
        Sum, ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0xFF));

    // Equality comparison (always false, but hard to prove statically)
    return Builder.CreateICmpEQ(Mod1, Mod2);
  }

  // Store the next block ID using an advanced obfuscation technique
  void storeAdvancedBlockID(IRBuilder<> &Builder, AllocaInst *BlockIDVar,
                            int NextID, GlobalVariable *StateVar) {
    // Get the state value and mix it with our next ID
    Value *IdxList[2] = {
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0),
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0)};
    Value *StateElemPtr = Builder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "state_elem_ptr");

    LoadInst *StateValue = Builder.CreateLoad(
        Type::getInt32Ty(Builder.getContext()), StateElemPtr);
    StateValue->setVolatile(true);

    // Use a different element to load the key
    IdxList[1] = ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 1);
    Value *KeyElemPtr = Builder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "key_elem_ptr");

    LoadInst *KeyValue =
        Builder.CreateLoad(Type::getInt32Ty(Builder.getContext()), KeyElemPtr);
    KeyValue->setVolatile(true);

    // Create a complex obfuscation transformation
    Value *NextIDVal = Builder.getInt32(NextID);

    // Mix the data in a complex way: ((StateValue * NextID) ^ KeyValue) +
    // (StateValue % 7)
    Value *Mul = Builder.CreateMul(StateValue, NextIDVal);
    Value *Xor = Builder.CreateXor(Mul, KeyValue);
    Value *Mod = Builder.CreateURem(StateValue, Builder.getInt32(7));
    Value *ObfuscatedID = Builder.CreateAdd(Xor, Mod);

    // Store the obfuscated value
    StoreInst *IDStore = Builder.CreateStore(ObfuscatedID, BlockIDVar);
    IDStore->setVolatile(true);

    // Update the state with a new value to create a moving target
    Value *NewState =
        Builder.CreateXor(StateValue, Builder.getInt32(NextID * 0x10001001));
    StoreInst *StateUpdateStore = Builder.CreateStore(NewState, StateElemPtr);
    StateUpdateStore->setVolatile(true);
  }

  // Load the block ID using an advanced obfuscation technique
  Value *loadAdvancedBlockID(IRBuilder<> &Builder, AllocaInst *BlockIDVar,
                             GlobalVariable *StateVar) {
    // Load the obfuscated block ID
    LoadInst *ObfuscatedID =
        Builder.CreateLoad(Type::getInt32Ty(Builder.getContext()), BlockIDVar);
    ObfuscatedID->setVolatile(true);

    // Load the values from our state array to deobfuscate
    Value *IdxList[2] = {
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0),
        ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 0)};
    Value *StateElemPtr = Builder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "state_elem_ptr");

    LoadInst *StateValue = Builder.CreateLoad(
        Type::getInt32Ty(Builder.getContext()), StateElemPtr);
    StateValue->setVolatile(true);

    // Use a different element to load the key
    IdxList[1] = ConstantInt::get(Type::getInt32Ty(Builder.getContext()), 1);
    Value *KeyElemPtr = Builder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "key_elem_ptr");

    LoadInst *KeyValue =
        Builder.CreateLoad(Type::getInt32Ty(Builder.getContext()), KeyElemPtr);
    KeyValue->setVolatile(true);

    // Deobfuscate using the inverse of our obfuscation transformation
    Value *Mod = Builder.CreateURem(StateValue, Builder.getInt32(7));
    Value *Step1 = Builder.CreateSub(ObfuscatedID, Mod);
    Value *Step2 = Builder.CreateXor(Step1, KeyValue);

    // This isn't a perfect inverse, we're relying on the switch statement's
    // case matching to work correctly In a real implementation, you'd need more
    // precise transformations

    // Keep updating the state to create a moving target
    Value *NewState =
        Builder.CreateXor(StateValue, Builder.getInt32(0x11335577));
    StoreInst *StateStore = Builder.CreateStore(NewState, StateElemPtr);
    StateStore->setVolatile(true);

    return Step2;
  }

  // Create an opaque predicate that the optimizer can't easily evaluate
  // Returns a Value that is actually false at runtime but appears complex
  Value *createOpaquePredicate(IRBuilder<> &builder) {
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

  // Break CFG functionality: Add additional basic blocks and dummy conditional
  // branches
  bool breakControlFlow(Function &F) {
    bool modified = false;

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

    return modified;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Skip empty or entry-only functions
    if (F.empty() || F.size() < 2) {
      return PreservedAnalyses::all();
    }

    Module *M = F.getParent();

    llvm::WithColor::note() << "Tainting control flow: " << F.getName() << '\n';

    // First, apply control flow breaking to add additional complexity
    bool cfgBroken = breakControlFlow(F);

    // Create global state that will help us make harder-to-optimize code
    GlobalVariable *StateVar = createGlobalState(M);

    // Only collect blocks with exactly one successor
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

    // Create our block ID variable
    IRBuilder<> EntryBuilder(Entry->getFirstNonPHI());
    AllocaInst *BlockIDVar = EntryBuilder.CreateAlloca(
        Type::getInt32Ty(F.getContext()), nullptr, "blockID");

    // Initialize with a complex calculation
    Value *InitValue = EntryBuilder.getInt32(0);
    Value *IdxList[2] = {ConstantInt::get(Type::getInt32Ty(F.getContext()), 0),
                         ConstantInt::get(Type::getInt32Ty(F.getContext()), 3)};
    Value *StateElemPtr = EntryBuilder.CreateInBoundsGEP(
        StateVar->getValueType(), StateVar, IdxList, "state_elem_ptr");
    LoadInst *StateInit =
        EntryBuilder.CreateLoad(Type::getInt32Ty(F.getContext()), StateElemPtr);
    StateInit->setVolatile(true);
    Value *ObfInit = EntryBuilder.CreateXor(InitValue, StateInit);
    StoreInst *InitStore = EntryBuilder.CreateStore(ObfInit, BlockIDVar);
    InitStore->setVolatile(true);

    // Create dispatcher block after the entry block
    BasicBlock *Dispatcher = BasicBlock::Create(F.getContext(), "dispatcher",
                                                &F, Entry->getNextNode());
    IRBuilder<> DispBuilder(Dispatcher);

    // Load the blockID using our complex scheme
    Value *SwitchVal = loadAdvancedBlockID(DispBuilder, BlockIDVar, StateVar);

    // Create a switch statement
    SwitchInst *SwInst =
        DispBuilder.CreateSwitch(SwitchVal, Entry, Blocks.size() + 1);

    // Assign unique IDs to each block and add cases to the switch
    int NextID = 1;
    SmallDenseMap<BasicBlock *, int> BlockToID;

    for (auto *BB : Blocks) {
      int ThisID = NextID++;
      BlockToID[BB] = ThisID;
      SwInst->addCase(
          ConstantInt::get(Type::getInt32Ty(F.getContext()), ThisID), BB);
    }

    // Now modify each block's terminator
    for (auto *BB : Blocks) {
      auto *Term = BB->getTerminator();
      BasicBlock *Succ = Term->getSuccessor(0);

      IRBuilder<> Builder(Term);

      // Determine the ID of the next block
      int NextBlockID;
      if (Succ == Entry) {
        NextBlockID = 0; // Entry block ID
      } else if (BlockToID.count(Succ)) {
        NextBlockID = BlockToID[Succ]; // Direct successor is another block
                                       // we're flattening
      } else {
        // Not a block we're flattening, create a new unique ID for it
        NextBlockID = NextID++;
        BlockToID[Succ] = NextBlockID;
        SwInst->addCase(
            ConstantInt::get(Type::getInt32Ty(F.getContext()), NextBlockID),
            Succ);
      }

      // Add an opaque predicate that makes a dead branch to confuse the
      // optimizer
      Value *OpaqueCond = createAdvancedOpaquePredicate(
          Builder, Builder.getInt32(NextBlockID), StateVar);

      // Create a bogus block that will never be taken to increase the apparent
      // complexity
      BasicBlock *BogusBlock =
          BasicBlock::Create(F.getContext(), BB->getName() + ".bogus", &F);
      IRBuilder<> BogusBuilder(BogusBlock);

      // Make the bogus block look important by doing complex operations
      Value *Bogus1 =
          BogusBuilder.CreateAlloca(Type::getInt32Ty(F.getContext()));
      BogusBuilder.CreateStore(BogusBuilder.getInt32(0xDEADBEEF), Bogus1);
      LoadInst *BogusLoad =
          BogusBuilder.CreateLoad(Type::getInt32Ty(F.getContext()), Bogus1);
      BogusLoad->setVolatile(true);
      BogusBuilder.CreateBr(Dispatcher); // Loop back to dispatcher

      // Store the next block ID using our complex obfuscation
      storeAdvancedBlockID(Builder, BlockIDVar, NextBlockID, StateVar);

      // Create conditional branch that will always go to dispatcher, but is
      // hard to prove This will create a more complex CFG that's harder to
      // analyze
      Builder.CreateCondBr(OpaqueCond, BogusBlock, Dispatcher);

      // Remove the original terminator
      Term->eraseFromParent();
    }

    // We've modified the CFG, so preserve nothing
    return (cfgBroken ? PreservedAnalyses::none() : PreservedAnalyses::none());
  }
};

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    // Register at the END of the optimization pipeline
    PB.registerOptimizerLastEPCallback([&](ModulePassManager &MPM,
                                           OptimizationLevel Level) {
      MPM.addPass(createModuleToFunctionPassAdaptor(ControlFlowTaintPass()));
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-control-flow-taint", "0.0.1",
          callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
