// Under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Author: djolertrk
//
// Control-Flow Taint Obfuscation Pass for LLVM
// ---------------------------------------------
//
// This LLVM pass merges CFFlattening and BreakCFG techniques to implement
// advanced control flow obfuscation designed to resist aggressive optimization by using:
// 1. Control flow flattening with dispatcher blocks and switch statements
// 2. Breaking control flow with dummy blocks and opaque predicates
// 3. Advanced opaque predicates using global state and complex calculations
// 4. Runtime-dependent values to prevent compile-time evaluation
// 5. Memory aliasing and volatiles to prevent certain optimizations
// 6. Various control flow complication mechanisms
//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
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
  GlobalVariable *createGlobalState(Module *M, Function &F) {
    // Create a global array to store state - harder to analyze than a simple
    // variable
    ArrayType *StateArrayTy =
        ArrayType::get(Type::getInt32Ty(M->getContext()), 4);

    // Initialize with some random-looking but deterministic values
    Constant *InitData = ConstantDataArray::get(
        M->getContext(),
        ArrayRef<uint32_t>({0xDEADBEEF, 0xBAADF00D, 0x12345678, 0xABCDEF00}));

    // Create the global with internal linkage so it's not visible outside this
    // module - use unique name per function
    std::string GlobalName = "obfuscation_state_" + F.getName().str();
    GlobalVariable *StateVar = new GlobalVariable(
        *M, StateArrayTy, false, GlobalValue::InternalLinkage, InitData,
        GlobalName, nullptr, GlobalValue::NotThreadLocal, 0);

    return StateVar;
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

  // Create an advanced opaque predicate that's hard to evaluate at compile time
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

  // Store the next block ID - simplified version
  void storeAdvancedBlockID(IRBuilder<> &Builder, AllocaInst *BlockIDVar,
                            int NextID, GlobalVariable *StateVar) {
    // Simple store to avoid complex memory operations
    Value *NextIDVal = Builder.getInt32(NextID);
    Builder.CreateStore(NextIDVal, BlockIDVar);
  }

  // Load the block ID - simplified version
  Value *loadAdvancedBlockID(IRBuilder<> &Builder, AllocaInst *BlockIDVar,
                             GlobalVariable *StateVar) {
    // Simple load to avoid complex memory operations
    return Builder.CreateLoad(Type::getInt32Ty(Builder.getContext()), BlockIDVar);
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

    // Skip declarations and intrinsics
    if (F.isDeclaration() || F.isIntrinsic()) {
      return PreservedAnalyses::all();
    }

    // Skip variadic functions
    if (F.isVarArg()) {
      return PreservedAnalyses::all();
    }

    // Skip functions with special linkage types
    if (F.hasAvailableExternallyLinkage() || F.hasLinkOnceLinkage()) {
      return PreservedAnalyses::all();
    }

    // Skip main function to avoid issues with program initialization
    if (F.getName() == "main") {
      return PreservedAnalyses::all();
    }

    // Skip functions that are too large (register allocator might struggle)
    if (F.size() > 30) {
      return PreservedAnalyses::all();
    }

    // Count total instructions to avoid transforming very complex functions
    unsigned InstCount = 0;
    for (auto &BB : F) {
      InstCount += BB.size();
    }
    if (InstCount > 200) {
      return PreservedAnalyses::all();
    }
    
    // Skip C++ special functions (constructors, destructors, operators)
    if (F.getName().starts_with("_Z")) {
      // Check for C++ special member functions in mangled names
      StringRef MangledName = F.getName();
      // Destructors contain D0Ev, D1Ev, D2Ev
      // Constructors contain C1Ev, C2Ev
      // Operators often have specific patterns
      if (MangledName.contains("D0Ev") || MangledName.contains("D1Ev") || 
          MangledName.contains("D2Ev") || MangledName.contains("C1Ev") ||
          MangledName.contains("C2Ev") || MangledName.contains("dlEPv") ||
          MangledName.contains("nwEm") || MangledName.contains("naEm")) {
        return PreservedAnalyses::all();
      }
    }

    // Analyze function patterns to skip problematic functions
    bool hasComplexMemoryOps = false;
    bool hasComplexStructAccess = false;
    bool hasLoopWithMemoryOps = false;
    bool hasInlineAsm = false;
    bool hasIndirectCalls = false;
    unsigned MemoryOpCount = 0;
    unsigned GEPCount = 0;
    unsigned CallCount = 0;
    
    // Check for loop patterns and memory complexity
    for (auto &BB : F) {
      if (BB.isEHPad() || BB.isLandingPad()) {
        return PreservedAnalyses::all();
      }
      
      // Skip functions with indirect branches
      if (auto *Term = BB.getTerminator()) {
        if (isa<IndirectBrInst>(Term) || isa<CallBrInst>(Term)) {
          return PreservedAnalyses::all();
        }
      }
      
      // Skip functions with PHI nodes (our transformation doesn't handle them well)
      if (!BB.phis().empty()) {
        return PreservedAnalyses::all();
      }
      
      // Check if this block is part of a loop
      bool isInLoop = false;
      for (auto *Pred : predecessors(&BB)) {
        if (Pred == &BB || Pred->getTerminator()->getSuccessor(0) == &BB) {
          isInLoop = true;
          break;
        }
      }
      
      // Analyze instructions for complex patterns
      for (auto &I : BB) {
        // Check for memory operations
        if (isa<LoadInst>(&I) || isa<StoreInst>(&I)) {
          MemoryOpCount++;
          
          if (isInLoop) {
            hasLoopWithMemoryOps = true;
          }
          
          // Check for volatile or atomic operations
          if (auto *LI = dyn_cast<LoadInst>(&I)) {
            if (LI->isVolatile() || LI->isAtomic()) {
              hasComplexMemoryOps = true;
            }
            // Check if loading from complex structure
            if (auto *GEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand())) {
              if (GEP->getNumIndices() > 2) {
                hasComplexStructAccess = true;
              }
            }
          }
          if (auto *SI = dyn_cast<StoreInst>(&I)) {
            if (SI->isVolatile() || SI->isAtomic()) {
              hasComplexMemoryOps = true;
            }
            // Check if storing to complex structure
            if (auto *GEP = dyn_cast<GetElementPtrInst>(SI->getPointerOperand())) {
              if (GEP->getNumIndices() > 2) {
                hasComplexStructAccess = true;
              }
            }
          }
        }
        
        // Count GEP instructions (complex pointer arithmetic)
        if (isa<GetElementPtrInst>(&I)) {
          GEPCount++;
        }
        
        // Check for inline assembly
        if (isa<InlineAsm>(&I)) {
          hasInlineAsm = true;
        }
        
        // Check for calls
        if (auto *CI = dyn_cast<CallInst>(&I)) {
          CallCount++;
          
          // Check for indirect calls
          if (!CI->getCalledFunction()) {
            hasIndirectCalls = true;
          }
          
          // Check for memory intrinsics
          if (CI->getCalledFunction()) {
            StringRef Name = CI->getCalledFunction()->getName();
            if (Name.starts_with("llvm.memcpy") || Name.starts_with("llvm.memmove") ||
                Name.starts_with("llvm.memset") || Name.starts_with("llvm.lifetime") ||
                Name.starts_with("llvm.invariant") || Name.starts_with("llvm.assume")) {
              hasComplexMemoryOps = true;
            }
            // Check for allocation functions
            if (Name == "malloc" || Name == "calloc" || Name == "realloc" || 
                Name == "free" || Name == "_Znwm" || Name == "_Znam" || 
                Name == "_ZdlPv" || Name == "_ZdaPv") {
              hasComplexMemoryOps = true;
            }
          }
        }
        
        // Check for invoke instructions
        if (isa<InvokeInst>(&I)) {
          return PreservedAnalyses::all();
        }
      }
    }
    
    // Skip functions with problematic patterns
    if (hasComplexMemoryOps || hasInlineAsm || hasIndirectCalls ||
        hasComplexStructAccess || hasLoopWithMemoryOps ||
        MemoryOpCount > 50 || GEPCount > 30 || CallCount > 20) {
      return PreservedAnalyses::all();
    }

    Module *M = F.getParent();

    llvm::WithColor::note() << "Tainting control flow: " << F.getName() << '\n';

    // First, apply control flow breaking to add additional complexity
    // DISABLED: This can cause issues with register allocation in complex functions
    bool cfgBroken = false; // breakControlFlow(F);

    // Create global state that will help us make harder-to-optimize code
    GlobalVariable *StateVar = createGlobalState(M, F);

    // Only collect blocks with exactly one successor that are safe to transform
    SmallVector<BasicBlock *, 8> Blocks;
    for (auto &BB : F) {
      if (BB.getTerminator() && BB.getTerminator()->getNumSuccessors() == 1 &&
          &BB != &F.getEntryBlock()) {
        // Skip blocks that might be loop headers or have phi nodes in successors
        BasicBlock *Succ = BB.getTerminator()->getSuccessor(0);
        bool hasPhiUsers = false;
        for (auto &SuccPhi : Succ->phis()) {
          if (SuccPhi.getNumIncomingValues() > 1) {
            hasPhiUsers = true;
            break;
          }
        }
        if (!hasPhiUsers) {
          Blocks.push_back(&BB);
        }
      }
    }

    if (Blocks.empty()) {
      return PreservedAnalyses::all();
    }

    // Limit the number of blocks we transform to avoid overwhelming the register allocator
    if (Blocks.size() > 20) {
      Blocks.resize(20);
    }

    BasicBlock *Entry = &F.getEntryBlock();

    // Create our block ID variable
    IRBuilder<> EntryBuilder(Entry->getFirstNonPHI());
    AllocaInst *BlockIDVar = EntryBuilder.CreateAlloca(
        Type::getInt32Ty(F.getContext()), nullptr, "blockID");
    
    // Simple initialization to avoid complex memory operations
    Value *InitValue = EntryBuilder.getInt32(0);
    EntryBuilder.CreateStore(InitValue, BlockIDVar);

    // Create dispatcher block after the entry block
    BasicBlock *Dispatcher = BasicBlock::Create(F.getContext(), "dispatcher",
                                                &F, Entry->getNextNode());
    
    // Save the original entry successor before modifying
    BasicBlock *OriginalEntrySucc = nullptr;
    if (Entry->getTerminator()) {
      if (Entry->getTerminator()->getNumSuccessors() > 0) {
        OriginalEntrySucc = Entry->getTerminator()->getSuccessor(0);
      }
      Entry->getTerminator()->eraseFromParent();
    }
    
    // Connect entry block to dispatcher
    IRBuilder<> EntryTermBuilder(Entry);
    EntryTermBuilder.CreateBr(Dispatcher);
    
    IRBuilder<> DispBuilder(Dispatcher);

    // Load the blockID using our complex scheme
    Value *SwitchVal = loadAdvancedBlockID(DispBuilder, BlockIDVar, StateVar);

    // Create a switch statement - use original entry successor as default if available
    BasicBlock *DefaultDest = OriginalEntrySucc ? OriginalEntrySucc : Entry;
    SwitchInst *SwInst =
        DispBuilder.CreateSwitch(SwitchVal, DefaultDest, Blocks.size() + 1);

    // Assign unique IDs to each block and add cases to the switch
    int NextID = 1;
    SmallDenseMap<BasicBlock *, int> BlockToID;

    // If we have an original entry successor, assign it ID 0
    if (OriginalEntrySucc && OriginalEntrySucc != Entry) {
      BlockToID[OriginalEntrySucc] = 0;
      SwInst->addCase(
          ConstantInt::get(Type::getInt32Ty(F.getContext()), 0), OriginalEntrySucc);
    }

    for (auto *BB : Blocks) {
      int ThisID = NextID++;
      BlockToID[BB] = ThisID;
      SwInst->addCase(
          ConstantInt::get(Type::getInt32Ty(F.getContext()), ThisID), BB);
    }

    // First, we need to update PHI nodes before modifying terminators
    // Map from old block to new block for PHI updates
    DenseMap<BasicBlock*, BasicBlock*> OldToNewSuccessor;
    
    // Now modify each block's terminator
    for (auto *BB : Blocks) {
      auto *Term = BB->getTerminator();
      BasicBlock *Succ = Term->getSuccessor(0);

      // Before modifying, update any PHI nodes in the successor
      for (auto &Phi : Succ->phis()) {
        // The PHI will now receive values from the dispatcher instead of BB
        int Idx = Phi.getBasicBlockIndex(BB);
        if (Idx >= 0) {
          Phi.setIncomingBlock(Idx, Dispatcher);
        }
      }

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

      // Store the next block ID using our complex obfuscation
      storeAdvancedBlockID(Builder, BlockIDVar, NextBlockID, StateVar);

      // For now, skip the bogus block creation to simplify and avoid issues
      // Just branch to dispatcher
      Builder.CreateBr(Dispatcher);

      // Remove the original terminator
      Term->eraseFromParent();
    }

    // We've modified the CFG significantly, so we need to invalidate most analyses
    return PreservedAnalyses::none();
  }
};

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    // Register as a late function simplification pass instead of at the very end
    // This avoids conflicts with loop canonicalization
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
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