// Under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// author: djolertrk

//
// String Encryption Obfuscation Pass for LLVM
// ---------------------------------------------
//
// This LLVM pass implements a string encryption obfuscation technique.
// Its purpose is to hide plaintext string literals in the final binary.
// It does so by scanning all global variables for constant string literals,
// encrypting their contents using a simple XOR cipher with a provided key,
// and replacing them in the IR with encrypted data.
//
// In a production environment, a runtime decryption routine must be provided
// so that the original string values can be recovered when needed. This pass
// only performs the compile‑time encryption.
//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

#include <iomanip>
#include <sstream>
#include <string>

using namespace llvm;

#ifndef SE_LLVM_CRYPTO_KEY
#define SE_LLVM_CRYPTO_KEY "default_key"
#endif

// A simple XOR-based encryption routine for strings.
static std::string encryptString(const std::string &str,
                                 const std::string &key) {
  std::string encrypted;
  encrypted.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    encrypted.push_back(str[i] ^ key[i % key.size()]);
  }

  // Convert the binary result into a hex string.
  std::ostringstream oss;
  for (unsigned char c : encrypted) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }

  return oss.str();
}

namespace {

struct StringEncryptionPass : public PassInfoMixin<StringEncryptionPass> {
  std::string CryptoKey;
  StringEncryptionPass(std::string Key = SE_LLVM_CRYPTO_KEY) : CryptoKey(Key) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    // Collect globals to process in a separate container to avoid modifying
    // the iterator while in the loop.
    SmallVector<GlobalVariable *> GlobalsToProcess;

    for (GlobalVariable &GV : M.globals()) {
      // Process only globals with an initializer.
      if (!GV.hasInitializer())
        continue;

      // Only process globals that are constant arrays of i8.
      Type *GVType = GV.getValueType();
      if (auto *AT = dyn_cast<ArrayType>(GVType)) {
        if (AT->getElementType()->isIntegerTy(8)) {
          if (auto *CDA = dyn_cast<ConstantDataArray>(GV.getInitializer())) {
            if (CDA->isString()) {
              GlobalsToProcess.push_back(&GV);
            }
          }
        }
      }
    }

    for (GlobalVariable *GV : GlobalsToProcess) {
      auto *CDA = cast<ConstantDataArray>(GV->getInitializer());
      std::string origStr = CDA->getAsString().str();

      llvm::WithColor::note()
          << "Original string in " << GV->getName() << ": " << origStr << "\n";

      // Check if the original array had a null terminator.
      // For example, a [6 x i8] might store "Hello\0".
      auto *AT = cast<ArrayType>(GV->getValueType());
      unsigned origNumElements = AT->getNumElements();
      // If the array size is exactly origStr.size() + 1,
      // that implies there's a trailing '\0'.
      bool hadTerminator = (origNumElements == origStr.size() + 1);

      // If there was a null terminator, explicitly append it
      // before encrypting. That way, the '\0' itself is also XOR'd.
      if (hadTerminator) {
        // Make sure we only append if the last character isn't already '\0'.
        // getAsString() usually strips trailing null, but just to be safe:
        if (origStr.empty() || origStr.back() != '\0') {
          origStr.push_back('\0');
        }
      }

      // Encrypt everything (including the terminator if present).
      std::string encStr = encryptString(origStr, CryptoKey);
      llvm::WithColor::note() << "Using key: " << CryptoKey << '\n';
      llvm::WithColor::note() << "Encrypted string: " << encStr << "\n";

      Constant *NewInit = ConstantDataArray::getString(M.getContext(), encStr,
                                                       /*AddNull=*/true);

      // Now check if the type changed (e.g. length changed).
      Type *NewArrTy = NewInit->getType(); // something like [N x i8]
      Type *OldArrTy = GV->getValueType();

      if (NewArrTy != OldArrTy) {
        // Create a new global with the corrected type.
        auto *NewGV = new GlobalVariable(M, NewArrTy,
                                         /*isConstant=*/false, GV->getLinkage(),
                                         NewInit, GV->getName() + ".encrypted");

        if (GV->getAlignment())
          NewGV->setAlignment(GV->getAlign());
        NewGV->setVisibility(GV->getVisibility());
        NewGV->setDSOLocal(GV->isDSOLocal());

        // Replace uses with a bitcast if pointer types differ.
        if (NewGV->getType() != GV->getType()) {
          auto *BC = ConstantExpr::getPointerBitCastOrAddrSpaceCast(
              NewGV, GV->getType());
          GV->replaceAllUsesWith(BC);
        } else {
          GV->replaceAllUsesWith(NewGV);
        }
        GV->eraseFromParent();
      } else {
        // If lengths match, just replace the initializer in place.
        GV->setInitializer(NewInit);
        GV->setConstant(false);
      }
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
          MPM.addPass(StringEncryptionPass());
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "kovid-string-encryption", "0.0.1",
          callback};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
