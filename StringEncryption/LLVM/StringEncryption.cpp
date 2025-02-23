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
// and replacing the initializer with the encrypted data.
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

#ifndef STRING_CRYPTO_KEY
#define STRING_STRING_CRYPTO_KEY "default_key"
#endif

// A simple XOR-based encryption routine for strings.
static std::string encryptString(const std::string &str,
                                 const std::string &key) {
  std::string encrypted;
  encrypted.reserve(str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    encrypted.push_back(str[i] ^ key[i % key.size()]);
  }
  return encrypted;
}

namespace {

struct StringEncryptionPass : public PassInfoMixin<StringEncryptionPass> {
  std::string CryptoKey;
  StringEncryptionPass(std::string Key = STRING_CRYPTO_KEY) : CryptoKey(Key) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    for (GlobalVariable &GV : M.globals()) {
      // Process only globals with an initializer.
      if (!GV.hasInitializer())
        continue;

      // Only process globals that are constant arrays of i8.
      Type *GVType = GV.getValueType();
      if (auto *AT = dyn_cast<ArrayType>(GVType)) {
        if (AT->getElementType()->isIntegerTy(8)) {
          if (ConstantDataArray *CDA =
                  dyn_cast<ConstantDataArray>(GV.getInitializer())) {
            if (CDA->isString()) {
              // Get the original string.
              std::string origStr = CDA->getAsString().str();
              llvm::WithColor::note() << "Original string in " << GV.getName()
                                      << ": " << origStr << "\n";

              // Encrypt the string using the provided key.
              std::string encStr = encryptString(origStr, CryptoKey);
              llvm::WithColor::note() << "Encrypted string: " << encStr << "\n";

              // Determine the number of elements in the original array.
              // Typically, a string literal is stored as an array with a null
              // terminator.
              unsigned origNumElements = AT->getNumElements();
              // We assume that if origNumElements equals origStr.size() + 1,
              // then the original initializer included a null terminator.
              bool addNull = (origNumElements == origStr.size() + 1);

              // Create a new constant array with the encrypted string.
              Constant *newInit =
                  ConstantDataArray::getString(M.getContext(), encStr, addNull);

              // Check if the new initializer's type matches the global's type.
              if (newInit->getType() != GVType) {
                // If not, adjust the type via bitcasting.
                newInit = ConstantExpr::getBitCast(newInit, GVType);
              }

              GV.setInitializer(newInit);
              // Optionally, mark the global as non-constant if runtime
              // decryption is needed.
              GV.setConstant(false);
            }
          }
        }
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
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
