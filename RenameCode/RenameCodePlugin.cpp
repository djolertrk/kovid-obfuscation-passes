// Under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// author: djolertrk

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

#include <sstream>
#include <iomanip>
#include <string>

#define DEBUG_TYPE "kovid-rename-code"

using namespace llvm;

#ifndef CRYPTO_KEY
#define CRYPTO_KEY "default_key"
#endif

// We use a simple XOR cipher combined with a hex encoding step so that
// the resulting encrypted name consists only of valid (printable) characters.
// This is reversible: applying the same XOR with the same key after hex-decoding
// will yield the original function name.

/// Encrypt a string by XORing with the key and converting the result to hex.
static std::string encryptFunctionName(const std::string &name, const std::string &key) {
  std::string xored;
  xored.reserve(name.size());
  for (size_t i = 0; i < name.size(); ++i) {
    // XOR each character with the corresponding byte from the key (repeating as needed)
    xored.push_back(name[i] ^ key[i % key.size()]);
  }
  // Convert the binary result into a hex string.
  std::ostringstream oss;
  for (unsigned char c : xored) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  return oss.str();
}

/// Decrypt a hex-encoded string produced by encryptFunctionName.
static std::string decryptFunctionName(const std::string &hexStr, const std::string &key) {
  std::string xored;
  // First, convert hex string back into the binary (XORed) form.
  for (size_t i = 0; i < hexStr.size(); i += 2) {
    std::string byteStr = hexStr.substr(i, 2);
    char byte = static_cast<char>(std::stoi(byteStr, nullptr, 16));
    xored.push_back(byte);
  }
  // Then, reverse the XOR operation to recover the original name.
  std::string original;
  original.reserve(xored.size());
  for (size_t i = 0; i < xored.size(); ++i) {
    original.push_back(xored[i] ^ key[i % key.size()]);
  }
  return original;
}

static bool runCodeRename(Function &F, std::string &CryptoKey) {
  if (F.isDeclaration()) {
    llvm::dbgs() << "Skipping function declaration.\n";
    return false;
  }

  if (!F.hasLocalLinkage()) {
    llvm::dbgs() << "Skipping function with non local linkage.\n";
    return false;
  }

  // Get the original function name.
  std::string originalName = F.getName().str();
  llvm::dbgs() << "Original function name: " << originalName << "\n";

  // Encrypt the function name using the provided CryptoKey.
  std::string encryptedName = encryptFunctionName(originalName, CryptoKey);
  llvm::dbgs() << "Encrypted function name: " << encryptedName << "\n";

  // Demonstrate decryption.
  std::string decryptedName = decryptFunctionName(encryptedName, CryptoKey);
  llvm::dbgs() << "Decrypted function name: " << decryptedName << "\n";

  // Rename the function with the encrypted name.
  F.setName("_" + encryptedName);

  LLVM_DEBBUG(llvm::outs() << *F.getParent() << '\n');

  return true;
}

namespace {

struct RenameCode : PassInfoMixin<RenameCode> {
  std::string CryptoKey;
  RenameCode(std::string Key = CRYPTO_KEY) : CryptoKey(Key) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    llvm::dbgs() << "Running KoviD Rename Code Pass: " <<  F.getName() << '\n';
    llvm::dbgs() << "Using crypto key: " << CryptoKey << "\n";

    runCodeRename(F, CryptoKey);
    llvm::dbgs() << '\n';

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
