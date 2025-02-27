/*
 * KoviD String Deobfuscation LLDB Plugin
 * --------------------------------------
 *
 * This LLDB plugin registers a new multiword command "deobfuscate" with a
 * subcommand "string" that decrypts an obfuscated global string. The plugin
 * assumes that global strings have been encrypted by the KoviD String
 * Encryption LLVM pass using a simple XOR cipher. At runtime, this plugin uses
 * the same crypto key (provided via the SE_LLVM_CRYPTO_KEY macro) to decrypt
 * the string.
 *
 * Usage in LLDB:
 *   (lldb) deobfuscate string <global_variable_name>
 *
 * License: Apache License v2.0 with LLVM Exceptions
 * Author: djolertrk
 */

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>

#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBFrame.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>
#include <lldb/API/SBValue.h>

#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"

// If SE_LLVM_CRYPTO_KEY is not provided by the parent CMake project, default to
// "default_key".
#ifndef SE_LLVM_CRYPTO_KEY
#define SE_LLVM_CRYPTO_KEY "default_key"
#endif

// A simple XOR decryption helper: given an encrypted string and a key,
// it reverses the XOR encryption.
static std::string decryptString(const std::string &hexStr,
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

// ----------------------------------------------------------------------
// A class implementing the LLDB command interface for "deobfuscate string".
class DeobfStringCommand : public lldb::SBCommandPluginInterface {
public:
  virtual bool DoExecute(lldb::SBDebugger debugger, char **command,
                         lldb::SBCommandReturnObject &result) override {
    if (!command || !command[0]) {
      result.Printf("Usage: deobfuscate string <global_variable_name>\n");
      result.SetStatus(lldb::eReturnStatusFailed);
      return false;
    }
    std::string varName(command[0]);

    lldb::SBTarget target = debugger.GetSelectedTarget();
    if (!target.IsValid()) {
      result.Printf("No valid target selected.\n");
      result.SetStatus(lldb::eReturnStatusFailed);
      return false;
    }
    // Look up the global variable by name.
    lldb::SBValue globalVar = target.FindFirstGlobalVariable(varName.c_str());
    if (!globalVar.IsValid()) {
      result.Printf("Global variable '%s' not found.\n", varName.c_str());
      result.SetStatus(lldb::eReturnStatusFailed);
      return false;
    }
    const char *encCStr = globalVar.GetSummary();
    if (!encCStr) {
      result.Printf("Failed to read global variable '%s' as a C-string.\n",
                    varName.c_str());
      result.SetStatus(lldb::eReturnStatusFailed);
      return false;
    }
    std::string encStr(encCStr);

    // Remove surrounding quotes if present.
    if (!encStr.empty() && encStr.front() == '"' && encStr.back() == '"') {
      encStr = encStr.substr(1, encStr.size() - 2);
    }

    llvm::WithColor::note() << "Value is " << encStr << '\n';

    std::string decStr = decryptString(encStr, SE_LLVM_CRYPTO_KEY);
    std::ostringstream oss;
    oss << "Decrypted string for global '" << varName << "': " << decStr
        << "\n";
    result.AppendMessage(oss.str().c_str());
    result.SetStatus(lldb::eReturnStatusSuccessFinishResult);
    return true;
  }
};

#define API __attribute__((used))
namespace lldb {

// Plugin entry point: registers the "deobfuscate" multiword command with the
// "string" subcommand.
API bool PluginInitialize(lldb::SBDebugger debugger) {
  // Get the LLDB command interpreter.
  lldb::SBCommandInterpreter interpreter = debugger.GetCommandInterpreter();

  // Register the multiword command "deobfuscate".
  lldb::SBCommand deobfCommand = interpreter.AddMultiwordCommand(
      "deobfuscate", "KoviD deobfuscation commands");
  if (!deobfCommand.IsValid()) {
    fprintf(stderr, "Failed to register 'deobfuscate' command\n");
    return false;
  }

  // Create an instance of our subcommand.
  DeobfStringCommand *stringCommand = new DeobfStringCommand();
  // Register the "string" subcommand using the overload that takes the command
  // name, a plugin interface pointer, a help string, and a syntax string
  // (nullptr).
  lldb::SBCommand stringCmd = deobfCommand.AddCommand(
      "string", stringCommand, "Decrypt an obfuscated global string", nullptr);
  if (!stringCmd.IsValid()) {
    fprintf(stderr, "Failed to register 'deobfuscate string' command\n");
    return false;
  }

  printf(
      "KoviD String Deobfuscation LLDB Plugin loaded. SE_LLVM_CRYPTO_KEY: %s\n",
      SE_LLVM_CRYPTO_KEY);
  return true;
}
} // namespace lldb
