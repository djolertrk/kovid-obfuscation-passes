// Under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// author: djolertrk

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <string>

using namespace llvm;

namespace {
using namespace cl;
OptionCategory KovidDeobfuscatorCategory("Specific Options");
static opt<bool> Help("h", desc("Alias for -help"), Hidden,
                      cat(KovidDeobfuscatorCategory));

// --crypto-key accepts the decryption key.
// A positional argument accepts the encrypted function name.
static opt<std::string>
    CryptoKey("crypto-key", cl::desc("Specify the crypto key for decryption"),
              value_desc("key"), cl::ValueRequired,
              cat(KovidDeobfuscatorCategory));

static opt<std::string> EncryptedFunctionName(Positional,
                                              desc("<encrypted function name>"),
                                              init(""),
                                              cat(KovidDeobfuscatorCategory));
} // namespace

/// Decrypt a hex-encoded string produced by encryptFunctionName from the
/// RenameCode plugin.
static std::string decryptFunctionName(const std::string &hexStr,
                                       const std::string &key) {
  std::string xored;
  // First, convert the hex string back into the binary (XORed) form.
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

int main(int argc, char const *argv[]) {
  // Parse command-line options.
  HideUnrelatedOptions({&KovidDeobfuscatorCategory});
  cl::ParseCommandLineOptions(argc, argv, "=== kovid debfuscator ===\n");
  if (Help) {
    PrintHelpMessage(false, true);
    return 0;
  }

  // Validate that both the crypto key and encrypted function name are provided.
  if (CryptoKey.empty() || EncryptedFunctionName.empty()) {
    llvm::WithColor::error()
        << "both --crypto-key and an encrypted function name must be "
           "provided.\n";
    return 1;
  }

  // Perform decryption.
  std::string decryptedName =
      decryptFunctionName(EncryptedFunctionName, CryptoKey);
  outs() << "Decrypted function name: " << decryptedName << "\n";

  return 0;
}
